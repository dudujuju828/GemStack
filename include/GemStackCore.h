#ifndef GEMSTACK_CORE_H
#define GEMSTACK_CORE_H

#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern std::queue<std::string> commandQueue;
extern std::mutex queueMutex;
extern std::condition_variable queueCV;
extern bool running;
extern std::atomic<bool> isBusy;

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

#endif // GEMSTACK_CORE_H
