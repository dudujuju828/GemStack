#ifndef GEMSTACK_CORE_H
#define GEMSTACK_CORE_H

#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <optional>

extern std::queue<std::string> commandQueue;
extern std::mutex queueMutex;
extern std::condition_variable queueCV;
extern bool running;
extern std::atomic<bool> isBusy;

// Configuration structure
struct GemStackConfig {
    // Auto-commit settings
    bool autoCommitEnabled = false;
    std::string autoCommitMessagePrefix = "[GemStack]";
    bool autoCommitIncludePrompt = true;  // Include prompt summary in commit message

    // Cooldown settings
    bool cooldownEnabled = false;
    int cooldownSeconds = 60;  // Default delay between prompts when cooldown is enabled
};

extern GemStackConfig g_config;

// Config file management
bool loadConfig(const std::string& filename = "GemStackConfig.txt");
GemStackConfig getDefaultConfig();

// Model fallback system - ordered from best to least-best
extern std::vector<std::string> modelFallbackList;
extern std::atomic<size_t> currentModelIndex;

// File parsing
bool loadCommandsFromFile(const std::string& filename);

// Model management
std::string getCurrentModel();
bool downgradeModel();
void resetModelToTop();

// Security utilities
std::string escapeForShell(const std::string& input);

// Rate limit detection
bool isModelExhausted(const std::string& output);

// String utilities
std::string trim(const std::string& str);

// Directive parsing utilities
std::string extractDirectiveContent(const std::string& line, const std::string& directive);
bool startsWithDirective(const std::string& trimmedLine, const std::string& directive);

// Path utilities
std::string normalizePath(const std::string& path);
std::string joinPath(const std::string& base, const std::string& relative);

// Output parsing utilities
std::string extractFirstMeaningfulLine(const std::string& output, size_t maxLength = 200);

// Session log management
const std::string SESSION_LOG_FILENAME = "GemStackSessionLog.txt";
std::string getSessionLogPath();
std::string readSessionLog();
void appendToSessionLog(const std::string& promptSummary, bool success, const std::string& notes = "");
void clearSessionLog();
std::string buildSessionContext();

// Cooldown management
// Injectable sleeper function type for testing (seconds -> void)
using SleeperFunction = std::function<void(int)>;

// Default sleeper implementation using std::this_thread::sleep_for
void defaultSleeper(int seconds);

// Set a custom sleeper function (for testing)
void setCooldownSleeper(SleeperFunction sleeper);

// Reset to default sleeper
void resetCooldownSleeper();

// Perform cooldown delay if enabled
// Returns true if cooldown was performed, false if skipped
bool performCooldown();

// Get the effective cooldown seconds (accounting for CLI overrides)
int getEffectiveCooldownSeconds();

// Check if cooldown is effectively enabled (accounting for CLI overrides)
bool isCooldownEnabled();

// Apply CLI overrides for cooldown settings
void applyCooldownCliOverrides(std::optional<bool> enabled, std::optional<int> seconds);

#endif // GEMSTACK_CORE_H
