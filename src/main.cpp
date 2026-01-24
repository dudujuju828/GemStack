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
#include <optional>

#include <GemStackCore.h>
#include <GitAutoCommit.h>
#include <ProcessExecutor.h>
#include <ConsoleUI.h>
#include <CliManager.h>

// Global auto-commit handler
GitAutoCommit g_autoCommit;

namespace fs = std::filesystem;

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
std::pair<bool, std::string> executeSinglePrompt(const std::string& prompt, bool injectSessionContext = true) {
    bool success = false;
    std::string finalOutput;
    std::string promptSummary = extractPromptSummary(prompt);

    // Check if this is a "prompt" command that we can pass via file to avoid shell injection
    bool isPromptCommand = (prompt.find("prompt \"") == 0);
    std::string tempInputFile = "GemStackInput.tmp";
    bool useFile = false;
    std::string fullCommand;
    std::string cliPath = CliManager::getGeminiCliPath();
    std::string model; // Will be set in the loop

    if (isPromptCommand) {
        useFile = true;
        std::string contentToWrite;

        // Extract raw content from prompt command
        std::string promptContent = prompt.substr(8);
        if (!promptContent.empty() && promptContent.back() == '"') {
            promptContent.pop_back();
        }

        // Build full content with session context
        if (injectSessionContext) {
            std::string sessionContext = buildSessionContext();
            contentToWrite = sessionContext + promptContent;
        } else {
            contentToWrite = promptContent;
        }

        // Write to temp file
        std::ofstream outFile(tempInputFile, std::ios::trunc); // Overwrite if exists
        if (outFile.is_open()) {
            outFile << contentToWrite;
            outFile.close();
        } else {
            std::cerr << "[GemStack] Error: Could not create temp input file. Falling back to unsafe method." << std::endl;
            useFile = false;
        }
    }

    while (!success) {
        model = getCurrentModel();
        std::cout << "[GemStack] Processing with model " << model << std::endl;

        if (useFile) {
            // Use redirection from temp file
            // Note: "prompt" subcommand is required
            fullCommand = "node \"" + cliPath + "\" --yolo --model " + model + " prompt < " + tempInputFile;
        } else {
            // Legacy/Fallback method (unsafe for flags in content, but necessary for non-prompt commands like --version)
            
            // Build the prompt with session context if requested (and feasible)
            std::string promptWithContext = prompt;
            if (injectSessionContext && !isPromptCommand) { 
                // Only try to inject if it looks like a prompt but failed isPromptCommand check?
                // Actually, if it's not "prompt \"...\"", we probably can't inject context easily 
                // without breaking the command structure (e.g. it might be "--help").
                // So we skip injection for non-standard prompts.
            }

            // Sanitize
            std::string safePrompt = escapeForShell(promptWithContext);
            fullCommand = "node \"" + cliPath + "\" --yolo --model " + model + " " + safePrompt;
        }

        // Execute in current directory
        auto [result, output] = ProcessExecutor::execute(fullCommand, ".");
        finalOutput = output;

        if (result == 0 && !isModelExhausted(output)) {
            std::cout << "[GemStack] Command finished successfully." << std::endl;
            success = true;

            // Append to session log
            appendToSessionLog(promptSummary, true);

            // Perform auto-commit if enabled (uses GitAutoCommit module)
            g_autoCommit.maybeCommit(promptSummary);
        } else if (isModelExhausted(output)) {
            if (!downgradeModel()) {
                std::cerr << "[GemStack] Command failed: all models exhausted." << std::endl;
                // Log failure to session log
                appendToSessionLog(promptSummary, false, "All models exhausted");
                break;
            }
            std::cout << "[GemStack] Retrying command with downgraded model..." << std::endl;
        } else {
            std::cerr << "[GemStack] Command failed with code: " << result << std::endl;
            // Log failure to session log
            appendToSessionLog(promptSummary, false, "Exit code: " + std::to_string(result));
            break;
        }
    }

    // Cleanup temp file
    if (useFile) {
        std::error_code ec;
        fs::remove(tempInputFile, ec);
        if (ec) {
            // Non-critical error, just log debug if needed
        }
    }

    return {success, finalOutput};
}

// Reflective mode: AI continuously improves by asking itself what to do next
void runReflectiveMode(const std::string& initialPrompt, int maxIterations, ConsoleUI& ui) {
    std::cout << "\n========================================\n";
    std::cout << "[GemStack] REFLECTIVE MODE ACTIVATED\n";
    std::cout << "[GemStack] Max iterations: " << maxIterations << "\n";
    std::cout << "[GemStack] Log file: " << getReflectionLogPath() << "\n";
    std::cout << "========================================\n\n";

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
        std::cout << "\n----------------------------------------\n";
        std::cout << "[GemStack] Reflection Iteration " << iteration << "/" << maxIterations << "\n";
        std::cout << "----------------------------------------\n\n";

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
        ui.startAnimation();

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
            ui.stopAnimation();
            std::cerr << "[GemStack] Reflective mode stopped due to execution failure." << std::endl;
            break;
        }

        // Perform cooldown if enabled and not the last iteration
        if (iteration < maxIterations) {
            performCooldown();
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

            // Don't inject session context for the reflection meta-query
            auto [reflectSuccess, nextPrompt] = executeSinglePrompt(reflectionQuery, false);

            ui.stopAnimation();

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
            ui.stopAnimation();
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "[GemStack] REFLECTIVE MODE COMPLETE\n";
    std::cout << "[GemStack] See " << getReflectionLogPath() << " for full session log\n";
    std::cout << "========================================\n\n";
}

void worker(ConsoleUI& ui) {
    while (true) {
        std::string command;
        bool moreCommandsPending = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [] { return !commandQueue.empty() || !running; });

            if (commandQueue.empty() && !running) {
                break;
            }

            command = commandQueue.front();
            commandQueue.pop();
            moreCommandsPending = !commandQueue.empty();
            isBusy = true;
        }

        // Increment task counter
        ui.incrementTaskProgress();

        // Execute the command with model fallback
        ui.startAnimation();
        auto [success, output] = executeSinglePrompt(command);
        ui.stopAnimation();

        // Perform cooldown if enabled and more commands are pending
        if (moreCommandsPending) {
            performCooldown();
        }

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
    std::cout << "  --cooldown                     Enable cooldown delay between prompts\n";
    std::cout << "  --no-cooldown                  Disable cooldown delay between prompts\n";
    std::cout << "  --cooldown-seconds <n>         Set cooldown delay duration (default: 60)\n";
    std::cout << "  --help                         Show this help message\n\n";
    std::cout << "Precedence: CLI flags > config file > defaults\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --reflect \"Build a simple calculator app\"\n";
    std::cout << "  " << programName << " --reflect \"Create a todo list\" --iterations 10\n";
    std::cout << "  " << programName << " --auto-commit --commit-prefix \"[AI]\"\n";
    std::cout << "  " << programName << " --cooldown --cooldown-seconds 30\n";
    std::cout << "  " << programName << " --config ./my-config.txt\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Welcome to GemStack!" << std::endl;

    // Parse command line arguments (first pass to get config path)
    std::string configPath = "GemStackConfig.txt";  // Default
    bool reflectMode = false;
    std::string reflectPrompt;
    int iterations = 5;  // Default iterations

    // CLI overrides for auto-commit
    std::optional<bool> cliAutoCommitEnabled;
    std::optional<std::string> cliCommitPrefix;
    std::optional<bool> cliCommitIncludePrompt;

    // CLI overrides for cooldown
    std::optional<bool> cliCooldownEnabled;
    std::optional<int> cliCooldownSeconds;

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
        } else if (arg == "--cooldown") {
            cliCooldownEnabled = true;
        } else if (arg == "--no-cooldown") {
            cliCooldownEnabled = false;
        } else if (arg == "--cooldown-seconds") {
            if (i + 1 < argc) {
                try {
                    int seconds = std::stoi(argv[++i]);
                    // Fall back to 60 if non-positive
                    cliCooldownSeconds = (seconds > 0) ? seconds : 60;
                } catch (...) {
                    std::cerr << "Error: --cooldown-seconds requires a numeric argument" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --cooldown-seconds requires a numeric argument" << std::endl;
                return 1;
            }
        }
    }

    // Initialize CLI manager
    if (!CliManager::initialize()) {
        std::cerr << "Failed to initialize embedded CLI. Exiting." << std::endl;
        return 1;
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

    // Apply cooldown CLI overrides
    applyCooldownCliOverrides(cliCooldownEnabled, cliCooldownSeconds);

    // Log effective auto-commit state
    if (g_autoCommit.isEnabled()) {
        std::cout << "[GemStack] Auto-commit is enabled" << std::endl;
    }

    // Log effective cooldown state
    if (isCooldownEnabled()) {
        std::cout << "[GemStack] Cooldown is enabled: " << getEffectiveCooldownSeconds() << " seconds between prompts" << std::endl;
    }

    std::cout << std::endl;

    // Instantiate ConsoleUI
    ConsoleUI ui;

    // Run in reflective mode if specified
    if (reflectMode) {
        // Validate prompt is not empty
        if (reflectPrompt.empty() || reflectPrompt.find_first_not_of(" \t\n\r") == std::string::npos) {
            std::cerr << "Error: --reflect requires a non-empty prompt" << std::endl;
            return 1;
        }

        // Format the initial prompt
        std::string initialPrompt = "prompt \"" + reflectPrompt + "\"";
        runReflectiveMode(initialPrompt, iterations, ui);
        std::cout << "Goodbye!" << std::endl;
        return 0;
    }

    // Normal mode
    std::cout << "Queue commands for Gemini. Type 'exit' to quit." << std::endl;

    // Load commands from file
    bool fileCommandsLoaded = loadCommandsFromFile("GemStackQueue.txt");

    // Set total task count for progress display
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        // Update ConsoleUI with total tasks
        ui.setTotalTasks(static_cast<int>(commandQueue.size()));
    }
    // Note: ConsoleUI initialized with current=0 by default

    // Start worker thread, passing UI instance
    std::thread workerThread([&ui]() { worker(ui); });

    if (fileCommandsLoaded) {
        // Accessing commandQueue via mutex to check size is redundant as ui has it, 
        // but ui.totalTasks is atomic.
        // We can just print a message.
        std::cout << "[GemStack] Processing tasks in batch mode..." << std::endl;
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
                // Update total tasks count in UI for correct progress display
                // Need to get current total and add 1
                // But ConsoleUI only has setTotalTasks.
                // We should probably expose addTasks or something.
                // For now, re-set it.
                // Wait, commandQueue.size() includes the new one.
                // Total tasks tracked by UI should ideally increase.
                // But totalTasks in ConsoleUI is just for display "x/Total".
                // If we are in interactive mode, Total keeps growing?
                // The original code:
                /*
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    commandQueue.push(line);
                }
                */
                // It DID NOT update totalTasks in interactive loop in original code!
                // So original code showed [x/InitTotal] ?
                // Original: `totalTasks` was atomic global. Set once before worker start.
                // Never updated in interactive loop.
                // So interactive commands showed [1/0], [2/0]?
                // I will replicate existing behavior or improve it. 
                // Since I cannot change ConsoleUI API in this file easily without rewriting header, 
                // I'll stick to existing behavior (no update).
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
