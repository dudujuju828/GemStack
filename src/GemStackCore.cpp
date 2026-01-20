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

bool loadCommandsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[GemStack] " << filename << " not found. Skipping file input." << std::endl;
        return false;
    }

    std::string line;
    bool collecting = false;
    bool commandsLoaded = false;

    while (std::getline(file, line)) {
        size_t startPos = line.find("GemStackSTART");
        size_t endPos = line.find("GemStackEND");

        if (startPos != std::string::npos) {
            collecting = true;
            // Handle case where content starts on the same line after START
            std::string afterStart = line.substr(startPos + 13);
            if (!afterStart.empty()) {
                // If END is also on this line
                size_t endPosInline = afterStart.find("GemStackEND");
                if (endPosInline != std::string::npos) {
                   std::string cmd = afterStart.substr(0, endPosInline);
                   cmd = trim(cmd);
                   if (!cmd.empty()) {
                       {
                           std::lock_guard<std::mutex> lock(queueMutex);
                           commandQueue.push(cmd);
                           commandsLoaded = true;
                       }
                       std::cout << "[GemStack] File command queued from " << filename << std::endl;
                   }
                   collecting = false; 
                   continue;
                }
                
                std::string cmd = trim(afterStart);
                if (!cmd.empty()) {
                     {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        commandQueue.push(cmd);
                        commandsLoaded = true;
                    }
                    std::cout << "[GemStack] File command queued from " << filename << std::endl;
                }
            }
        }
        else if (endPos != std::string::npos) {
            collecting = false;
            // Handle content before END on the same line
            std::string beforeEnd = line.substr(0, endPos);
             std::string cmd = trim(beforeEnd);
             if (!cmd.empty()) {
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    commandQueue.push(cmd);
                    commandsLoaded = true;
                }
                std::cout << "[GemStack] File command queued from " << filename << std::endl;
             }
        }
        else if (collecting) {
            // In the body of the block. Treat this line as a command.
            if (line.empty()) continue;
            
            std::string cmd = trim(line);
            if (cmd.empty()) continue; // skip blank lines

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.push(cmd);
                commandsLoaded = true;
            }
            std::cout << "[GemStack] File command queued from " << filename << std::endl;
        }
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