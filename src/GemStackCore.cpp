#include <GemStackCore.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <array>

std::queue<std::string> commandQueue;
std::mutex queueMutex;
std::condition_variable queueCV;
bool running = true;
std::atomic<bool> isBusy{false};

// Global config instance
GemStackConfig g_config;

GemStackConfig getDefaultConfig() {
    return GemStackConfig();
}

bool loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Config file is optional, use defaults
        g_config = getDefaultConfig();
        return false;
    }

    std::cout << "[GemStack] Loading configuration from " << filename << std::endl;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmedLine = trim(line);

        // Skip empty lines and comments
        if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') {
            continue;
        }

        // Parse key=value pairs
        size_t equalsPos = trimmedLine.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmedLine.substr(0, equalsPos));
        std::string value = trim(trimmedLine.substr(equalsPos + 1));

        // Remove surrounding quotes from value if present
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        // Map configuration keys
        if (key == "autoCommitEnabled" || key == "auto_commit_enabled") {
            g_config.autoCommitEnabled = (value == "true" || value == "1" || value == "yes");
        } else if (key == "autoCommitMessagePrefix" || key == "auto_commit_message_prefix") {
            g_config.autoCommitMessagePrefix = value;
        } else if (key == "autoCommitIncludePrompt" || key == "auto_commit_include_prompt") {
            g_config.autoCommitIncludePrompt = (value == "true" || value == "1" || value == "yes");
        }
    }

    if (g_config.autoCommitEnabled) {
        std::cout << "[GemStack] Auto-commit enabled with prefix: \"" << g_config.autoCommitMessagePrefix << "\"" << std::endl;
    }

    return true;
}

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

// Extract content from a directive like: specify "content" or prompt {{ content }}
std::string extractDirectiveContent(const std::string& line, const std::string& directive) {
    size_t pos = line.find(directive);
    if (pos == std::string::npos) {
        return "";
    }

    // Check for braces {{ ... }}
    size_t braceStart = line.find("{{", pos + directive.length());
    if (braceStart != std::string::npos) {
        size_t braceEnd = line.find("}}", braceStart + 2);
        if (braceEnd != std::string::npos) {
             return line.substr(braceStart + 2, braceEnd - braceStart - 2);
        }
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

    // Current block's goal (high-level description of final product)
    std::string currentBlockGoal;

    // Multi-line parsing state
    bool inMultiLine = false;
    std::string multiLineBuffer;
    std::string currentMultiLineDirective; // "prompt ", "specify ", "goal "

    while (std::getline(file, line)) {
        std::string trimmedLine = trim(line);

        // Handle multi-line content accumulation
        if (inMultiLine) {
            size_t closePos = line.find("}}");
            if (closePos != std::string::npos) {
                // Found closer - append content before }}
                multiLineBuffer += line.substr(0, closePos);
                inMultiLine = false;
                
                std::string content = multiLineBuffer;
                std::string directive = currentMultiLineDirective;
                
                // Process the completed content
                if (directive == "goal ") {
                    if (!content.empty()) {
                        if (!currentBlockGoal.empty()) {
                            std::cout << "[GemStack] Warning: Multiple goals in block " << promptBlockCount
                                      << ". Overwriting previous goal." << std::endl;
                        }
                        currentBlockGoal = content;
                        std::cout << "[GemStack] Goal set (multi-line)" << std::endl;
                    }
                } else if (directive == "specify ") {
                    if (!content.empty()) {
                        pendingSpecifications.push_back(content);
                        std::cout << "[GemStack] Specification queued (multi-line)" << std::endl;
                    }
                } else if (directive == "prompt ") {
                    if (!content.empty()) {
                        std::string finalCommand;
                        std::string augmentedPrompt;

                        bool hasGoal = !currentBlockGoal.empty();
                        bool hasSpecs = !pendingSpecifications.empty();

                        if (hasGoal || hasSpecs) {
                            if (hasGoal) {
                                augmentedPrompt += "GOAL - The ultimate objective you are working towards:\n";
                                augmentedPrompt += "  " + currentBlockGoal + "\n\n";
                            }
                            if (hasSpecs) {
                                augmentedPrompt += "CHECKPOINT - Before proceeding, verify the following expectations are met. If any are NOT correct, fix them first and explain what was missing:\n";
                                for (size_t i = 0; i < pendingSpecifications.size(); i++) {
                                    augmentedPrompt += "  " + std::to_string(i + 1) + ". " + pendingSpecifications[i] + "\n";
                                }
                                augmentedPrompt += "\n";
                                pendingSpecifications.clear();
                            }
                            if (hasGoal && !hasSpecs) {
                                augmentedPrompt += "CURRENT TASK:\n" + content;
                            } else {
                                augmentedPrompt += "After verification is complete, proceed with the following task:\n" + content;
                            }
                            
                            finalCommand = "prompt \"" + augmentedPrompt + "\"";
                            
                             if (hasGoal && hasSpecs) {
                                std::cout << "[GemStack] Prompt with goal and " << pendingSpecifications.size()
                                          << " checkpoint(s) queued" << std::endl;
                            } else if (hasGoal) {
                                std::cout << "[GemStack] Prompt with goal queued" << std::endl;
                            } else {
                                std::cout << "[GemStack] Prompt with " << pendingSpecifications.size()
                                          << " verification checkpoint(s) queued" << std::endl;
                            }
                        } else {
                            finalCommand = "prompt \"" + content + "\"";
                            std::cout << "[GemStack] Prompt queued (multi-line)" << std::endl;
                        }

                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            commandQueue.push(finalCommand);
                            commandsLoaded = true;
                        }
                    }
                }
                
                continue;
            } else {
                multiLineBuffer += line + "\n";
                continue;
            }
        }

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
            currentBlockGoal.clear();      // Clear goal at block start
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
            currentBlockGoal.clear(); // Clear goal when exiting block
            std::cout << "[GemStack] Exiting PromptBlock " << promptBlockCount << std::endl;
            continue;
        }

        // Skip empty lines
        if (trimmedLine.empty()) {
            continue;
        }

        // Check for start of multi-line directive {{ 
        std::string potentialDirective;
        if (startsWithDirective(trimmedLine, "prompt ")) potentialDirective = "prompt ";
        else if (startsWithDirective(trimmedLine, "goal ")) potentialDirective = "goal ";
        else if (startsWithDirective(trimmedLine, "specify ")) potentialDirective = "specify ";
        
        if (!potentialDirective.empty()) {
            size_t braceStart = trimmedLine.find("{{");
            // Ensure brace is after the directive
            if (braceStart != std::string::npos) {
                 size_t braceEnd = trimmedLine.find("}}", braceStart);
                 if (braceEnd == std::string::npos) {
                     // Start of multi-line block
                     inMultiLine = true;
                     currentMultiLineDirective = potentialDirective;
                     multiLineBuffer = trimmedLine.substr(braceStart + 2) + "\n";
                     continue;
                 }
                 // If braceEnd is found, it's a single line {{...}} handled by extractDirectiveContent
            }
        }

        // Handle 'goal' directive - sets high-level objective for the block
        if (startsWithDirective(trimmedLine, "goal ")) {
            std::string goalContent = extractDirectiveContent(trimmedLine, "goal ");
            if (!goalContent.empty()) {
                if (!currentBlockGoal.empty()) {
                    std::cout << "[GemStack] Warning: Multiple goals in block " << promptBlockCount
                              << ". Overwriting previous goal." << std::endl;
                }
                currentBlockGoal = goalContent;
                std::cout << "[GemStack] Goal set: \"" 
                          << (goalContent.length() > 60 ? goalContent.substr(0, 60) + "..." : goalContent)
                          << "\"" << std::endl;
            }
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

        // Handle 'prompt' directive - may have goal and specifications prepended
        if (startsWithDirective(trimmedLine, "prompt ")) {
            std::string promptContent = extractDirectiveContent(trimmedLine, "prompt ");

            if (!promptContent.empty()) {
                std::string finalCommand;
                std::string augmentedPrompt;

                // Build the augmented prompt with goal and/or specifications
                bool hasGoal = !currentBlockGoal.empty();
                bool hasSpecs = !pendingSpecifications.empty();

                if (hasGoal || hasSpecs) {
                    // Add goal context if present
                    if (hasGoal) {
                        augmentedPrompt += "GOAL - The ultimate objective you are working towards:\n";
                        augmentedPrompt += "  " + currentBlockGoal + "\n\n";
                    }

                    // Add verification checkpoints if present
                    if (hasSpecs) {
                        augmentedPrompt += "CHECKPOINT - Before proceeding, verify the following expectations are met. If any are NOT correct, fix them first and explain what was missing:\n";
                        for (size_t i = 0; i < pendingSpecifications.size(); i++) {
                            augmentedPrompt += "  " + std::to_string(i + 1) + ". " + pendingSpecifications[i] + "\n";
                        }
                        augmentedPrompt += "\n";
                        pendingSpecifications.clear();
                    }

                    // Add the actual task
                    if (hasGoal && !hasSpecs) {
                        augmentedPrompt += "CURRENT TASK:\n" + promptContent;
                    } else {
                        augmentedPrompt += "After verification is complete, proceed with the following task:\n" + promptContent;
                    }

                    finalCommand = "prompt \"" + augmentedPrompt + "\"";

                    if (hasGoal && hasSpecs) {
                        std::cout << "[GemStack] Prompt with goal and " << pendingSpecifications.size()
                                  << " checkpoint(s) queued" << std::endl;
                    } else if (hasGoal) {
                        std::cout << "[GemStack] Prompt with goal queued" << std::endl;
                    } else {
                        std::cout << "[GemStack] Prompt with " << pendingSpecifications.size()
                                  << " verification checkpoint(s) queued" << std::endl;
                    }
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

        // For any other command (not prompt, specify, or goal), queue it directly
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
