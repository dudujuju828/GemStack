#include <iostream>
#include <fstream>
#include <sstream>
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
#include <filesystem>

#include <GemStackCore.h>
#include <GitAutoCommit.h>

// Global auto-commit handler
GitAutoCommit g_autoCommit;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define popen _popen
#define pclose _pclose
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

#include "EmbeddedCli.h"

// Global path to the CLI entry point
std::string g_geminiCliPath;

std::string getHomeDirectory() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (home) return std::string(home);
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) return std::string(drive) + std::string(path);
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    return ".";
#endif
}

bool extractEmbeddedCli(const std::string& targetDir) {
    // Check if main file exists (simple check)
    if (fs::exists(targetDir + "/gemini.js")) {
        return true;
    }

    std::cout << "[GemStack] Extracting embedded Gemini CLI to " << targetDir << "..." << std::endl;
    
    try {
        fs::create_directories(targetDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }

    std::string tempZipPath = targetDir + "/cli.zip";
    std::ofstream zipFile(tempZipPath, std::ios::binary);
    if (!zipFile) {
        std::cerr << "Failed to create temp zip file: " << tempZipPath << std::endl;
        return false;
    }
    zipFile.write(reinterpret_cast<const char*>(EMBEDDED_CLI_DATA), EMBEDDED_CLI_SIZE);
    zipFile.close();
    
    std::string command = "tar -xf \"" + tempZipPath + "\" -C \"" + targetDir + "\"";
    int result = system(command.c_str());
    
    try {
        fs::remove(tempZipPath);
    } catch (...) {}
    
    if (result != 0) {
        std::cerr << "Failed to extract CLI using tar." << std::endl;
        #ifdef _WIN32
        std::cerr << "Trying PowerShell Expand-Archive..." << std::endl;
        std::ofstream zipFile2(tempZipPath, std::ios::binary);
        zipFile2.write(reinterpret_cast<const char*>(EMBEDDED_CLI_DATA), EMBEDDED_CLI_SIZE);
        zipFile2.close();
        
        std::string psCommand = "powershell -Command \"Expand-Archive -Path '" + tempZipPath + "' -DestinationPath '" + targetDir + "' -Force\"";
        result = system(psCommand.c_str());
        try { fs::remove(tempZipPath); } catch (...) {}
        
        if (result != 0) {
             std::cerr << "Failed to extract CLI using PowerShell." << std::endl;
             return false;
        }
        #else
        return false;
        #endif
    }
    
    return true;
}

bool initializeCli() {
    std::string home = getHomeDirectory();
    std::string cliDir = joinPath(home, ".gemstack/gemini-cli");
    
    if (!extractEmbeddedCli(cliDir)) {
        return false;
    }
    
    g_geminiCliPath = joinPath(cliDir, "gemini.js");
    return true;
}
// Reflection mode prompt log
struct ReflectionLogEntry {
    int iteration;
    std::string prompt;
    std::string summary;
    bool success;
};

std::vector<ReflectionLogEntry> reflectionLog;
const std::string REFLECTION_LOG_FILENAME = "GemStackReflectionLog.txt";

// Get the full path for the reflection log (in current folder)
std::string getReflectionLogPath() {
    return REFLECTION_LOG_FILENAME;
}

// Write reflection log to file for persistence and debugging
void writeReflectionLogToFile(const std::string& initialGoal) {
    std::string logPath = getReflectionLogPath();
    std::ofstream logFile(logPath);
    if (!logFile.is_open()) {
        std::cerr << "[GemStack] Warning: Could not write reflection log file." << std::endl;
        return;
    }

    logFile << "================================================================================\n";
    logFile << "GEMSTACK REFLECTION LOG\n";
    logFile << "================================================================================\n\n";
    logFile << "INITIAL GOAL: " << initialGoal << "\n\n";
    logFile << "--------------------------------------------------------------------------------\n\n";

    for (const auto& entry : reflectionLog) {
        logFile << "ITERATION " << entry.iteration << " [" << (entry.success ? "SUCCESS" : "FAILED") << "]\n";
        logFile << "PROMPT: " << entry.prompt << "\n";
        if (!entry.summary.empty()) {
            logFile << "SUMMARY: " << entry.summary << "\n";
        }
        logFile << "\n--------------------------------------------------------------------------------\n\n";
    }

    logFile.close();
}

// Build context string from reflection log for injection into prompts
std::string buildReflectionContext(const std::string& initialGoal) {
    if (reflectionLog.empty()) {
        return "";
    }

    std::string context = "CONTEXT FROM PREVIOUS ITERATIONS:\n";
    context += "Initial Goal: " + initialGoal + "\n\n";
    context += "Work completed so far:\n";

    for (const auto& entry : reflectionLog) {
        context += "- Iteration " + std::to_string(entry.iteration) + ": " + entry.prompt;
        if (!entry.summary.empty()) {
            context += " -> " + entry.summary;
        }
        context += "\n";
    }

    context += "\nBuild upon this previous work. Do not repeat completed tasks.\n\n";
    return context;
}

// Wrapper for extracting output summary (uses core function)
inline std::string extractOutputSummary(const std::string& output, size_t maxLength = 200) {
    return extractFirstMeaningfulLine(output, maxLength);
}

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

#ifdef _WIN32
// Windows implementation using CreateProcess with proper console handling
std::pair<int, std::string> executeWithOutput(const std::string& command, const std::string& workingDir = "") {
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

    // Determine working directory (NULL means inherit from parent)
    const char* lpCurrentDirectory = workingDir.empty() ? NULL : workingDir.c_str();

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
        lpCurrentDirectory,            // Current directory for the child process
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
std::pair<int, std::string> executeWithOutput(const std::string& command, const std::string& workingDir = "") {
    std::string output;
    std::array<char, 256> buffer;

    // Build command with optional directory change
    std::string fullCommand;
    if (!workingDir.empty()) {
        fullCommand = "cd \"" + workingDir + "\" && " + command + " 2>&1";
    } else {
        fullCommand = command + " 2>&1";
    }

    FILE* pipe = popen(fullCommand.c_str(), "r");
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

// Extract a short summary from a prompt for commit messages
static std::string extractPromptSummary(const std::string& prompt) {
    std::string summary = prompt;

    // Remove "prompt " wrapper if present
    if (summary.find("prompt \"") == 0) {
        summary = summary.substr(8);
        if (!summary.empty() && summary.back() == '"') {
            summary.pop_back();
        }
    }

    // Remove CHECKPOINT prefix if present (from specify directives)
    size_t checkpointPos = summary.find("CHECKPOINT");
    if (checkpointPos == 0) {
        size_t taskPos = summary.find("proceed with the following task:");
        if (taskPos != std::string::npos) {
            summary = summary.substr(taskPos + 33); // Length of "proceed with the following task:"
            summary = trim(summary);
        }
    }

    // Truncate to reasonable length for commit message
    if (summary.length() > 80) {
        summary = summary.substr(0, 77) + "...";
    }

    return summary;
}

// Execute a single prompt and return the result
std::pair<bool, std::string> executeSinglePrompt(const std::string& prompt) {
    bool success = false;
    std::string finalOutput;

    // Sanitize the prompt to prevent command injection
    std::string safePrompt = escapeForShell(prompt);

    while (!success) {
        std::string model = getCurrentModel();
        std::cout << "[GemStack] Processing with model " << model << std::endl;

        std::string fullCommand = "node \"" + g_geminiCliPath + "\" --yolo --model " + model + " " + safePrompt;

        // Execute in current directory
        auto [result, output] = executeWithOutput(fullCommand, ".");
        finalOutput = output;

        if (result == 0 && !isModelExhausted(output)) {
            std::cout << "[GemStack] Command finished successfully." << std::endl;
            success = true;

            // Perform auto-commit if enabled (uses GitAutoCommit module)
            std::string promptSummary = extractPromptSummary(prompt);
            g_autoCommit.maybeCommit(promptSummary);
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
    std::cout << "[GemStack] Log file: " << getReflectionLogPath() << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Clear previous reflection log
    reflectionLog.clear();

    // Extract the actual goal from the initial prompt (remove "prompt " wrapper if present)
    std::string initialGoal = initialPrompt;
    if (initialGoal.find("prompt \"") == 0) {
        initialGoal = initialGoal.substr(8); // Remove 'prompt "'
        if (!initialGoal.empty() && initialGoal.back() == '"') {
            initialGoal.pop_back(); // Remove trailing quote
        }
    }

    std::string currentPrompt = initialPrompt;

    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "[GemStack] Reflection Iteration " << iteration << "/" << maxIterations << std::endl;
        std::cout << "----------------------------------------\n" << std::endl;

        // Build context from previous iterations
        std::string context = buildReflectionContext(initialGoal);

        // For iterations after the first, inject context into the prompt
        std::string promptWithContext = currentPrompt;
        if (iteration > 1 && !context.empty()) {
            // Extract prompt content and inject context
            if (currentPrompt.find("prompt \"") == 0) {
                std::string promptContent = currentPrompt.substr(8);
                if (!promptContent.empty() && promptContent.back() == '"') {
                    promptContent.pop_back();
                }
                promptWithContext = "prompt \"" + context + "CURRENT TASK: " + promptContent + "\"";
            }
        }

        // Start animation for this iteration
        startAnimation();

        // Execute the current prompt (with context if applicable)
        auto [success, output] = executeSinglePrompt(promptWithContext);

        // Extract summary from output for the log
        std::string summary = extractOutputSummary(output);

        // Log this iteration
        ReflectionLogEntry entry;
        entry.iteration = iteration;
        // Store the original prompt (without context) for cleaner logs
        if (currentPrompt.find("prompt \"") == 0) {
            entry.prompt = currentPrompt.substr(8);
            if (!entry.prompt.empty() && entry.prompt.back() == '"') {
                entry.prompt.pop_back();
            }
        } else {
            entry.prompt = currentPrompt;
        }
        entry.summary = summary;
        entry.success = success;
        reflectionLog.push_back(entry);

        // Write log to file after each iteration
        writeReflectionLogToFile(initialGoal);

        if (!success) {
            stopAnimation();
            std::cerr << "[GemStack] Reflective mode stopped due to execution failure." << std::endl;
            break;
        }

        // If not the last iteration, ask for the next prompt
        if (iteration < maxIterations) {
            std::cout << "\n[GemStack] Generating next reflection prompt..." << std::endl;

            // Build a comprehensive reflection query that includes the full history
            std::string historyContext = "You are in iteration " + std::to_string(iteration) + " of " + std::to_string(maxIterations) + " in a reflective development session.\n\n";
            historyContext += "ORIGINAL GOAL: " + initialGoal + "\n\n";
            historyContext += "COMPLETED WORK:\n";
            for (const auto& logEntry : reflectionLog) {
                historyContext += "- Iteration " + std::to_string(logEntry.iteration) + ": " + logEntry.prompt;
                if (!logEntry.summary.empty()) {
                    historyContext += " (Result: " + logEntry.summary + ")";
                }
                historyContext += "\n";
            }
            historyContext += "\nBased on the original goal and the work completed so far, what is the single most impactful next step to improve or extend this work? ";
            historyContext += "Do NOT repeat any tasks already completed. Focus on what's missing or could be improved. ";
            historyContext += "Respond with ONLY the next task description, nothing else. Be specific and actionable.";

            std::string reflectionQuery = "prompt \"" + historyContext + "\"";

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
    std::cout << "[GemStack] See " << getReflectionLogPath() << " for full session log" << std::endl;
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
    std::cout << "  --reflect <prompt>             Run in reflective mode with the given initial prompt\n";
    std::cout << "  --iterations <n>               Set max iterations for reflective mode (default: 5)\n";
    std::cout << "  --config <path>                Load configuration from specified file\n";
    std::cout << "  --auto-commit                  Force enable auto-commit for this run\n";
    std::cout << "  --no-auto-commit               Force disable auto-commit for this run\n";
    std::cout << "  --commit-prefix <text>         Override commit message prefix\n";
    std::cout << "  --commit-include-prompt <bool> Include prompt summary in commits (true/false)\n";
    std::cout << "  --help                         Show this help message\n\n";
    std::cout << "Auto-commit precedence: CLI flags > config file > defaults\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --reflect \"Build a simple calculator app\"\n";
    std::cout << "  " << programName << " --reflect \"Create a todo list\" --iterations 10\n";
    std::cout << "  " << programName << " --auto-commit --commit-prefix \"[AI]\"\n";
    std::cout << "  " << programName << " --config ./my-config.txt\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Welcome to GemStack!" << std::endl;

    if (!initializeCli()) {
        std::cerr << "Failed to initialize embedded CLI. Exiting." << std::endl;
        return 1;
    }

    // Parse command line arguments (first pass to get config path)
    std::string configPath = "GemStackConfig.txt";  // Default
    bool reflectMode = false;
    std::string reflectPrompt;
    int iterations = 5;  // Default iterations

    // CLI overrides for auto-commit
    std::optional<bool> cliAutoCommitEnabled;
    std::optional<std::string> cliCommitPrefix;
    std::optional<bool> cliCommitIncludePrompt;

    const int MAX_ITERATIONS = 100;  // Safety cap

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                configPath = argv[++i];
            } else {
                std::cerr << "Error: --config requires a path argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
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
        } else if (arg == "--auto-commit") {
            cliAutoCommitEnabled = true;
        } else if (arg == "--no-auto-commit") {
            cliAutoCommitEnabled = false;
        } else if (arg == "--commit-prefix") {
            if (i + 1 < argc) {
                cliCommitPrefix = argv[++i];
            } else {
                std::cerr << "Error: --commit-prefix requires a text argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "--commit-include-prompt") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                if (val == "true" || val == "1" || val == "yes") {
                    cliCommitIncludePrompt = true;
                } else if (val == "false" || val == "0" || val == "no") {
                    cliCommitIncludePrompt = false;
                } else {
                    std::cerr << "Error: --commit-include-prompt requires true or false" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --commit-include-prompt requires true or false" << std::endl;
                return 1;
            }
        }
    }

    // Load configuration (optional - uses defaults if file doesn't exist)
    loadConfig(configPath);

    // Initialize GitAutoCommit with config file settings
    GitAutoCommitConfig autoCommitConfig;
    autoCommitConfig.enabled = g_config.autoCommitEnabled;
    autoCommitConfig.messagePrefix = g_config.autoCommitMessagePrefix;
    autoCommitConfig.includePrompt = g_config.autoCommitIncludePrompt;
    g_autoCommit.setConfig(autoCommitConfig);

    // Apply CLI overrides (these take precedence over config file)
    g_autoCommit.applyCliOverrides(cliAutoCommitEnabled, cliCommitPrefix, cliCommitIncludePrompt);

    // Log effective auto-commit state
    if (g_autoCommit.isEnabled()) {
        std::cout << "[GemStack] Auto-commit is enabled" << std::endl;
    }

    std::cout << std::endl;

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