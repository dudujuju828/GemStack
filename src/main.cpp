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
#include <cstdio>
#include <array>

#include <GemStackCore.h>

// Platform-specific popen/pclose
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

// Check if output indicates model exhaustion/rate limit
bool isModelExhausted(const std::string& output) {
    // Common rate limit / quota exhaustion error patterns
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

// Execute command and capture output
std::pair<int, std::string> executeWithOutput(const std::string& command) {
    std::string output;
    std::array<char, 256> buffer;

    // Redirect stderr to stdout to capture all output
    std::string cmdWithRedirect = command + " 2>&1";

    FILE* pipe = popen(cmdWithRedirect.c_str(), "r");
    if (!pipe) {
        return {-1, "Failed to execute command"};
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string chunk = buffer.data();
        output += chunk;
        // Also print to console in real-time
        std::cout << chunk;
    }

    int result = pclose(pipe);
    return {result, output};
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

        // Execute the command with model fallback
        bool success = false;
        while (!success) {
            std::string model = getCurrentModel();
            std::cout << "[GemStack] Processing with model " << model << ": " << command << std::endl;

            // Construct the full command to run gemini-cli with YOLO mode and model
            std::string fullCommand = "node gemini-cli/scripts/start.js --yolo --model " + model + " " + command;

            auto [result, output] = executeWithOutput(fullCommand);

            if (result == 0 && !isModelExhausted(output)) {
                std::cout << "[GemStack] Command finished successfully." << std::endl;
                success = true;
            } else if (isModelExhausted(output)) {
                // Try to downgrade to next model
                if (!downgradeModel()) {
                    // No more models available
                    std::cerr << "[GemStack] Command failed: all models exhausted." << std::endl;
                    break;
                }
                // Will retry with the new model
                std::cout << "[GemStack] Retrying command with downgraded model..." << std::endl;
            } else {
                // Non-exhaustion failure
                std::cerr << "[GemStack] Command failed with code: " << result << std::endl;
                break;
            }
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