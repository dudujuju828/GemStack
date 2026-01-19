#include <GemStackCore.h>
#include <fstream>
#include <iostream>
#include <algorithm>

std::queue<std::string> commandQueue;
std::mutex queueMutex;
std::condition_variable queueCV;
bool running = true;
std::atomic<bool> isBusy{false};

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