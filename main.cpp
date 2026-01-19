#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <atomic>
#include <chrono>

std::queue<std::string> commandQueue;
std::mutex queueMutex;
std::condition_variable queueCV;
bool running = true;
std::atomic<bool> isBusy{false};

bool loadCommandsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[GemStack] " << filename << " not found. Skipping file input." << std::endl;
        return false;
    }

    std::string line;
    std::string currentCommand;
    bool collecting = false;
    bool commandsLoaded = false;

    while (std::getline(file, line)) {
        size_t startPos = line.find("GemStackSTART");
        size_t endPos = line.find("GemStackEND");

        if (startPos != std::string::npos) {
            collecting = true;
            currentCommand = ""; 
            
            if (endPos != std::string::npos && endPos > startPos) {
                // Single line case
                currentCommand = line.substr(startPos + 13, endPos - (startPos + 13));
                
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    commandQueue.push(currentCommand);
                    commandsLoaded = true;
                }
                queueCV.notify_one();
                std::cout << "[GemStack] File command queued from " << filename << std::endl;
                collecting = false;
                continue; 
            }
            
            std::string afterStart = line.substr(startPos + 13);
            if (!afterStart.empty()) {
                currentCommand += afterStart;
            }
        }
        else if (collecting && endPos != std::string::npos) {
            std::string beforeEnd = line.substr(0, endPos);
            if (!beforeEnd.empty()) {
                if (!currentCommand.empty()) currentCommand += " ";
                currentCommand += beforeEnd;
            }
            
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.push(currentCommand);
                commandsLoaded = true;
            }
            queueCV.notify_one();
            std::cout << "[GemStack] File command queued from " << filename << std::endl;
            collecting = false;
        }
        else if (collecting) {
            if (!currentCommand.empty()) currentCommand += " ";
            currentCommand += line;
        }
    }
    return commandsLoaded;
}

void worker() {
    while (true) {
        std::string command;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [] { return !commandQueue.empty() || !running; });

            if (commandQueue.empty() && !running) {
                break;
            }

            command = commandQueue.front();
            commandQueue.pop();
            isBusy = true;
        }

        // Execute the command
        std::cout << "[GemStack] Processing: " << command << std::endl;
        
        // Construct the full command to run gemini-cli
        // Assuming we are running from the root where gemini-cli folder exists
        std::string fullCommand = "node gemini-cli/scripts/start.js " + command;
        
        int result = std::system(fullCommand.c_str());
        
        if (result != 0) {
            std::cerr << "[GemStack] Command failed with code: " << result << std::endl;
        } else {
            std::cout << "[GemStack] Command finished successfully." << std::endl;
        }
        isBusy = false;
    }
}

int main() {
    std::cout << "Welcome to GemStack!" << std::endl;
    std::cout << "Queue commands for Gemini. Type 'exit' to quit." << std::endl;

    // Load commands from file
    bool fileCommandsLoaded = loadCommandsFromFile("GemStackQueue.txt");

    std::thread workerThread(worker);

    if (fileCommandsLoaded) {
        std::cout << "[GemStack] Processing file commands in batch mode..." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            bool empty;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                empty = commandQueue.empty();
            }
            if (empty && !isBusy) {
                break;
            }
        }
    } else {
        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) {
                break; // EOF
            }

            if (line == "exit" || line == "quit") {
                break;
            }

            if (line.empty()) {
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.push(line);
            }
            queueCV.notify_one();
            std::cout << "[GemStack] Command queued." << std::endl;
        }
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        running = false;
    }
    queueCV.notify_all();

    if (workerThread.joinable()) {
        workerThread.join();
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}