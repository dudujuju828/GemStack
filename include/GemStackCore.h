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

bool loadCommandsFromFile(const std::string& filename);
std::string getCurrentModel();
bool downgradeModel();
void resetModelToTop();

#endif // GEMSTACK_CORE_H
