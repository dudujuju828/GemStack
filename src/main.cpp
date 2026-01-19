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
#include <vector>
#include <stdexcept>

#include <GemStackCore.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define popen _popen
#define pclose _pclose
#include <sys/ioctl.h>
#include <unistd.h>
#endif

// Animation control
std::atomic<bool> animationRunning{false};
std::thread animationThread;

#ifdef _WIN32
// Windows: Write directly to console buffer at specific position (non-blocking, independent of cout)
void writeStatusLine(const std::string& text, bool clear = false) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return;
    }

    // Calculate bottom row position within the visible window
    SHORT bottomRow = csbi.srWindow.Bottom;
    COORD statusPos = { 0, bottomRow };

    // Prepare the text to write (pad with spaces to clear the line)
    std::string paddedText = text;
    int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    if (static_cast<int>(paddedText.length()) < consoleWidth) {
        paddedText.append(consoleWidth - paddedText.length(), ' ');
    }

    // Write directly to console buffer - completely independent of cout
    DWORD written;
    WriteConsoleOutputCharacterA(hConsole, paddedText.c_str(),
                                  static_cast<DWORD>(paddedText.length()),
                                  statusPos, &written);

    // Set color attributes for the status line (cyan on black)
    if (!clear && !text.empty()) {
        std::vector<WORD> attrs(text.length(), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        WriteConsoleOutputAttribute(hConsole, attrs.data(),
                                    static_cast<DWORD>(attrs.size()),
                                    statusPos, &written);
    }
}

void statusAnimation() {
    const std::string baseText = "GemStack Generating";
    int dotCount = 0;

    while (animationRunning.load()) {
        std::string dots(dotCount + 1, '.');
        std::string padding(3 - dotCount, ' ');
        std::string statusText = baseText + " " + dots + padding;

        writeStatusLine(statusText);

        dotCount = (dotCount + 1) % 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    // Clear the status line when done
    writeStatusLine("", true);
}

#else
// Unix: Use ANSI codes but write to stderr to avoid interfering with stdout
void statusAnimation() {
    const std::string baseText = "GemStack Generating";
    int dotCount = 0;

    // Get terminal height
    int termHeight = 24;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        termHeight = w.ws_row;
    }

    while (animationRunning.load()) {
        std::string dots(dotCount + 1, '.');
        std::string padding(3 - dotCount, ' ');

        // Write to stderr using ANSI codes - independent of stdout buffering
        fprintf(stderr, "\033[s\033[%d;1H\033[K\033[36m%s %s%s\033[0m\033[u",
                termHeight, baseText.c_str(), dots.c_str(), padding.c_str());
        fflush(stderr);

        dotCount = (dotCount + 1) % 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    // Clear the status line when done
    fprintf(stderr, "\033[s\033[%d;1H\033[K\033[u", termHeight);
    fflush(stderr);
}
#endif

void startAnimation() {
    if (animationRunning.load()) return;  // Already running
    animationRunning.store(true);
    animationThread = std::thread(statusAnimation);
}

void stopAnimation() {
    animationRunning.store(false);
    if (animationThread.joinable()) {
        animationThread.join();
    }
}

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

#ifdef _WIN32
// Windows implementation using CreateProcess with proper console handling
std::pair<int, std::string> executeWithOutput(const std::string& command) {
    std::string output;

    // Create pipes for capturing stdout/stderr
    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;
    HANDLE hStdErrWrite = NULL;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pipe for stdout
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        return {-1, "Failed to create stdout pipe"};
    }

    // Ensure the read handle is not inherited
    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return {-1, "Failed to set handle information"};
    }

    // Duplicate stdout write handle for stderr
    if (!DuplicateHandle(GetCurrentProcess(), hStdOutWrite,
                         GetCurrentProcess(), &hStdErrWrite,
                         0, TRUE, DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return {-1, "Failed to duplicate handle for stderr"};
    }

    // Set up the process startup info
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    // Build command line - use cmd.exe /c to ensure proper shell environment
    std::string cmdLine = "cmd.exe /c " + command;

    // Create the child process
    // Using CREATE_NO_WINDOW to prevent console window popup
    // The process inherits our console for node-pty compatibility
    BOOL success = CreateProcessA(
        NULL,                          // Application name (use command line)
        const_cast<char*>(cmdLine.c_str()), // Command line
        NULL,                          // Process security attributes
        NULL,                          // Thread security attributes
        TRUE,                          // Inherit handles
        0,                             // Creation flags - inherit parent console
        NULL,                          // Environment (inherit from parent)
        NULL,                          // Current directory (inherit from parent)
        &si,                           // Startup info
        &pi                            // Process info
    );

    // Close write ends of pipes in parent - child has them now
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!success) {
        CloseHandle(hStdOutRead);
        return {-1, "Failed to create process: " + std::to_string(GetLastError())};
    }

    // Read output from pipe in a loop
    char buffer[256];
    DWORD bytesRead;

    while (true) {
        BOOL readSuccess = ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
        if (!readSuccess || bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        std::string chunk(buffer, bytesRead);
        output += chunk;
        // Print to console in real-time
        std::cout << chunk;
        std::cout.flush();
    }

    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Cleanup
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);

    return {static_cast<int>(exitCode), output};
}

#else
// Unix implementation using popen
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
#endif

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

// Execute a single prompt and return the result
std::pair<bool, std::string> executeSinglePrompt(const std::string& prompt) {
    bool success = false;
    std::string finalOutput;

    // Sanitize the prompt to prevent command injection
    std::string safePrompt = escapeForShell(prompt);

    while (!success) {
        std::string model = getCurrentModel();
        std::cout << "[GemStack] Processing with model " << model << ": " << safePrompt << std::endl;

        std::string fullCommand = "node gemini-cli/scripts/start.js --yolo --model " + model + " " + safePrompt;
        auto [result, output] = executeWithOutput(fullCommand);
        finalOutput = output;

        if (result == 0 && !isModelExhausted(output)) {
            std::cout << "[GemStack] Command finished successfully." << std::endl;
            success = true;
        } else if (isModelExhausted(output)) {
            if (!downgradeModel()) {
                std::cerr << "[GemStack] Command failed: all models exhausted." << std::endl;
                break;
            }
            std::cout << "[GemStack] Retrying command with downgraded model..." << std::endl;
        } else {
            std::cerr << "[GemStack] Command failed with code: " << result << std::endl;
            break;
        }
    }

    return {success, finalOutput};
}

// Reflective mode: AI continuously improves by asking itself what to do next
void runReflectiveMode(const std::string& initialPrompt, int maxIterations) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "[GemStack] REFLECTIVE MODE ACTIVATED" << std::endl;
    std::cout << "[GemStack] Max iterations: " << maxIterations << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::string currentPrompt = initialPrompt;

    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "[GemStack] Reflection Iteration " << iteration << "/" << maxIterations << std::endl;
        std::cout << "----------------------------------------\n" << std::endl;

        // Start animation for this iteration
        startAnimation();

        // Execute the current prompt
        auto [success, output] = executeSinglePrompt(currentPrompt);

        if (!success) {
            stopAnimation();
            std::cerr << "[GemStack] Reflective mode stopped due to execution failure." << std::endl;
            break;
        }

        // If not the last iteration, ask for the next prompt
        if (iteration < maxIterations) {
            std::cout << "\n[GemStack] Generating next reflection prompt..." << std::endl;

            // Ask the AI what to do next
            std::string reflectionQuery = "prompt \"Based on what was just accomplished, what is the single most impactful next step to improve or extend this work? Respond with ONLY the next task description, nothing else. Be specific and actionable.\"";

            auto [reflectSuccess, nextPrompt] = executeSinglePrompt(reflectionQuery);

            stopAnimation();

            if (!reflectSuccess) {
                std::cerr << "[GemStack] Failed to generate next reflection prompt." << std::endl;
                break;
            }

            // Clean up the response - trim whitespace
            size_t start = nextPrompt.find_first_not_of(" \t\n\r");
            size_t end = nextPrompt.find_last_not_of(" \t\n\r");
            if (start == std::string::npos || end == std::string::npos || start > end) {
                // Response was empty or all whitespace
                std::cerr << "[GemStack] AI returned empty response for next prompt." << std::endl;
                break;
            }
            nextPrompt = nextPrompt.substr(start, end - start + 1);

            // Validate the trimmed response isn't empty
            if (nextPrompt.empty()) {
                std::cerr << "[GemStack] AI returned empty response for next prompt." << std::endl;
                break;
            }

            // Format as a prompt command (escaping happens in executeSinglePrompt)
            currentPrompt = "prompt \"" + nextPrompt + "\"";

            std::cout << "\n[GemStack] Next prompt: " << currentPrompt << std::endl;
        } else {
            stopAnimation();
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "[GemStack] REFLECTIVE MODE COMPLETE" << std::endl;
    std::cout << "========================================\n" << std::endl;
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
        startAnimation();
        auto [success, output] = executeSinglePrompt(command);
        stopAnimation();

        isBusy = false;
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --reflect <prompt>     Run in reflective mode with the given initial prompt\n";
    std::cout << "  --iterations <n>       Set max iterations for reflective mode (default: 5)\n";
    std::cout << "  --help                 Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --reflect \"Build a simple calculator app\"\n";
    std::cout << "  " << programName << " --reflect \"Create a todo list\" --iterations 10\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Welcome to GemStack!" << std::endl;

    // Parse command line arguments
    bool reflectMode = false;
    std::string reflectPrompt;
    int iterations = 5;  // Default iterations

    const int MAX_ITERATIONS = 100;  // Safety cap

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--reflect") {
            reflectMode = true;
            if (i + 1 < argc) {
                reflectPrompt = argv[++i];
            } else {
                std::cerr << "Error: --reflect requires a prompt argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "--iterations") {
            if (i + 1 < argc) {
                try {
                    iterations = std::stoi(argv[++i]);
                    if (iterations < 1) {
                        std::cerr << "Error: iterations must be at least 1" << std::endl;
                        return 1;
                    }
                    if (iterations > MAX_ITERATIONS) {
                        std::cerr << "Error: iterations cannot exceed " << MAX_ITERATIONS << std::endl;
                        return 1;
                    }
                } catch (const std::out_of_range&) {
                    std::cerr << "Error: iterations value is out of range" << std::endl;
                    return 1;
                } catch (...) {
                    std::cerr << "Error: --iterations requires a numeric argument" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --iterations requires a numeric argument" << std::endl;
                return 1;
            }
        }
    }

    // Run in reflective mode if specified
    if (reflectMode) {
        // Validate prompt is not empty
        if (reflectPrompt.empty() || reflectPrompt.find_first_not_of(" \t\n\r") == std::string::npos) {
            std::cerr << "Error: --reflect requires a non-empty prompt" << std::endl;
            return 1;
        }

        // Format the initial prompt
        std::string initialPrompt = "prompt \"" + reflectPrompt + "\"";
        runReflectiveMode(initialPrompt, iterations);
        std::cout << "Goodbye!" << std::endl;
        return 0;
    }

    // Normal mode
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