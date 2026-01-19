#ifndef GEMSTACK_CORE_H
#define GEMSTACK_CORE_H

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern std::queue<std::string> commandQueue;
extern std::mutex queueMutex;
extern std::condition_variable queueCV;
extern bool running;
extern std::atomic<bool> isBusy;

bool loadCommandsFromFile(const std::string& filename);

#endif // GEMSTACK_CORE_H
