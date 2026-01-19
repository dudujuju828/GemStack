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

#include "GemStackCore.h"

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
        
        // Construct the full command to run gemini-cli with YOLO mode enabled
        std::string fullCommand = "node gemini-cli/scripts/start.js --yolo " + command;
        
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