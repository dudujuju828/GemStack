#include <GemStackCore.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

std::queue<std::string> commandQueue;
std::mutex queueMutex;
std::condition_variable queueCV;
bool running = true;
std::atomic<bool> isBusy{false};

// Model fallback list - ordered from best to least-best
std::vector<std::string> modelFallbackList = {
    "gemini-3-pro-preview",
    "gemini-3-flash-preview",
    "gemini-2.5-pro",
    "gemini-2.0-flash",
    "gemini-1.5-pro",
    "gemini-1.5-flash"
};
std::atomic<size_t> currentModelIndex{0};

std::string getCurrentModel() {
    size_t idx = currentModelIndex.load();
    if (idx < modelFallbackList.size()) {
        return modelFallbackList[idx];
    }
    return modelFallbackList.back();
}

bool downgradeModel() {
    size_t idx = currentModelIndex.load();
    if (idx + 1 < modelFallbackList.size()) {
        currentModelIndex.store(idx + 1);
        std::cout << "[GemStack] Model exhausted. Downgrading to: " << getCurrentModel() << std::endl;
        return true;
    }
    std::cerr << "[GemStack] All models exhausted. No fallback available." << std::endl;
    return false;
}

void resetModelToTop() {
    currentModelIndex.store(0);
}

// Escape a string for safe shell usage
std::string escapeForShell(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size() * 2);

    for (char c : input) {
        switch (c) {
            // Escape shell metacharacters
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '$':  escaped += "\\$"; break;
            case '`':  escaped += "\\`"; break;
            case '!':  escaped += "\\!"; break;
            // Block dangerous characters entirely
            case ';':
            case '&':
            case '|':
            case '<':
            case '>':
            case '\n':
            case '\r':
                escaped += ' ';  // Replace with space
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

// Check if output indicates model exhaustion/rate limit
bool isModelExhausted(const std::string& output) {
    const std::vector<std::string> exhaustionPatterns = {
        "rate limit",
        "Rate limit",
        "RATE_LIMIT",
        "quota exceeded",
        "Quota exceeded",
        "QUOTA_EXCEEDED",
        "resource exhausted",
        "Resource exhausted",
        "RESOURCE_EXHAUSTED",
        "too many requests",
        "Too many requests",
        "429",
        "limit reached",
        "exhausted"
    };

    for (const auto& pattern : exhaustionPatterns) {
        if (output.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Helper to trim whitespace from both ends
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Extract content from a directive like: specify "content here" or prompt "content here"
// Returns the content without quotes, or empty string if not a valid directive
std::string extractDirectiveContent(const std::string& line, const std::string& directive) {
    size_t pos = line.find(directive);
    if (pos == std::string::npos) {
        return "";
    }

    // Find the opening quote after the directive
    size_t quoteStart = line.find('"', pos + directive.length());
    if (quoteStart == std::string::npos) {
        return "";
    }

    // Find the closing quote
    size_t quoteEnd = line.rfind('"');
    if (quoteEnd == std::string::npos || quoteEnd <= quoteStart) {
        return "";
    }

    return line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

// Check if line starts with a directive (after trimming)
bool startsWithDirective(const std::string& trimmedLine, const std::string& directive) {
    return trimmedLine.find(directive) == 0;
}

bool loadCommandsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[GemStack] " << filename << " not found. Skipping file input." << std::endl;
        return false;
    }

    std::string line;
    bool inGemStackBlock = false;
    bool inPromptBlock = false;
    bool commandsLoaded = false;
    int promptBlockCount = 0;

    // Accumulated specify statements to prepend to the next prompt
    std::vector<std::string> pendingSpecifications;

    while (std::getline(file, line)) {
        std::string trimmedLine = trim(line);

        // Check for GemStack delimiters
        if (trimmedLine.find("GemStackSTART") != std::string::npos) {
            inGemStackBlock = true;
            continue;
        }
        if (trimmedLine.find("GemStackEND") != std::string::npos) {
            inGemStackBlock = false;
            continue;
        }

        // Only process content inside GemStack block
        if (!inGemStackBlock) {
            continue;
        }

        // Check for PromptBlock delimiters
        if (trimmedLine.find("PromptBlockSTART") != std::string::npos) {
            inPromptBlock = true;
            promptBlockCount++;
            pendingSpecifications.clear(); // Clear specs at block start
            std::cout << "[GemStack] Entering PromptBlock " << promptBlockCount << std::endl;
            continue;
        }
        if (trimmedLine.find("PromptBlockEND") != std::string::npos) {
            inPromptBlock = false;
            // Warn if there are unused specifications at block end
            if (!pendingSpecifications.empty()) {
                std::cout << "[GemStack] Warning: " << pendingSpecifications.size()
                          << " specify statement(s) at end of block with no following prompt" << std::endl;
                pendingSpecifications.clear();
            }
            std::cout << "[GemStack] Exiting PromptBlock " << promptBlockCount << std::endl;
            continue;
        }

        // Skip empty lines
        if (trimmedLine.empty()) {
            continue;
        }

        // Handle 'specify' directive - accumulate for the next prompt
        if (startsWithDirective(trimmedLine, "specify ")) {
            std::string specContent = extractDirectiveContent(trimmedLine, "specify ");
            if (!specContent.empty()) {
                pendingSpecifications.push_back(specContent);
                std::cout << "[GemStack] Specification queued: \""
                          << (specContent.length() > 50 ? specContent.substr(0, 50) + "..." : specContent)
                          << "\"" << std::endl;
            }
            continue;
        }

        // Handle 'prompt' directive - may have specifications prepended
        if (startsWithDirective(trimmedLine, "prompt ")) {
            std::string promptContent = extractDirectiveContent(trimmedLine, "prompt ");

            if (!promptContent.empty()) {
                std::string finalCommand;

                // If there are pending specifications, prepend them as a verification checkpoint
                if (!pendingSpecifications.empty()) {
                    std::string verificationBlock = "CHECKPOINT - Before proceeding, verify the following expectations are met. If any are NOT correct, fix them first and explain what was missing:\n";
                    for (size_t i = 0; i < pendingSpecifications.size(); i++) {
                        verificationBlock += "  " + std::to_string(i + 1) + ". " + pendingSpecifications[i] + "\n";
                    }
                    verificationBlock += "\nAfter verification is complete, proceed with the following task:\n" + promptContent;

                    finalCommand = "prompt \"" + verificationBlock + "\"";

                    std::cout << "[GemStack] Prompt with " << pendingSpecifications.size()
                              << " verification checkpoint(s) queued" << std::endl;
                    pendingSpecifications.clear();
                } else {
                    finalCommand = trimmedLine; // Use original prompt line
                    std::cout << "[GemStack] Prompt queued from " << filename << std::endl;
                }

                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    commandQueue.push(finalCommand);
                    commandsLoaded = true;
                }
            }
            continue;
        }

        // For any other command (not prompt or specify), queue it directly
        // This handles things like --help, --version, etc.
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.push(trimmedLine);
            commandsLoaded = true;
        }
        std::cout << "[GemStack] Command queued from " << filename << std::endl;
    }

    // Final warning for unused specifications outside any block
    if (!pendingSpecifications.empty()) {
        std::cout << "[GemStack] Warning: " << pendingSpecifications.size()
                  << " specify statement(s) at end of file with no following prompt" << std::endl;
    }

    return commandsLoaded;
}

// Normalize path separators (convert backslashes to forward slashes)
std::string normalizePath(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Remove trailing slash if present (unless it's the root)
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }

    return normalized;
}

// Join two path components
std::string joinPath(const std::string& base, const std::string& relative) {
    if (base.empty()) {
        return normalizePath(relative);
    }
    if (relative.empty()) {
        return normalizePath(base);
    }

    std::string normalizedBase = normalizePath(base);
    std::string normalizedRelative = normalizePath(relative);

    // Remove leading slash from relative if present
    if (!normalizedRelative.empty() && normalizedRelative[0] == '/') {
        normalizedRelative = normalizedRelative.substr(1);
    }

    // Ensure base doesn't end with slash
    if (!normalizedBase.empty() && normalizedBase.back() == '/') {
        return normalizedBase + normalizedRelative;
    }

    return normalizedBase + "/" + normalizedRelative;
}

// Extract the first meaningful line from output (skipping status messages)
std::string extractFirstMeaningfulLine(const std::string& output, size_t maxLength) {
    if (output.empty()) {
        return "Completed";
    }

    std::istringstream stream(output);
    std::string line;
    std::string result;

    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        // Skip common status/formatting lines
        if (line.find("[GemStack]") != std::string::npos) continue;
        if (line.find("========") != std::string::npos) continue;
        if (line.find("--------") != std::string::npos) continue;
        if (line.find("Checking build") != std::string::npos) continue;

        // Found a meaningful line
        result = trim(line);
        break;
    }

    if (result.empty()) {
        return "Completed";
    }

    // Truncate if too long
    if (result.length() > maxLength) {
        result = result.substr(0, maxLength - 3) + "...";
    }

    return result;
}