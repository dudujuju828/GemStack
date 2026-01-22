#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include <string>
#include <atomic>
#include <thread>

class ConsoleUI {
public:
    ConsoleUI();
    ~ConsoleUI();

    // Prevent copying
    ConsoleUI(const ConsoleUI&) = delete;
    ConsoleUI& operator=(const ConsoleUI&) = delete;

    // Task progress management
    void setTotalTasks(int total);
    void incrementTaskProgress();
    void resetProgress();

    // Animation control
    void startAnimation();
    void stopAnimation();

private:
    void statusAnimation(); // Worker function for thread
    void writeStatusLine(const std::string& text, bool clear = false);

    std::atomic<bool> animationRunning;
    std::thread animationThread;
    
    std::atomic<int> totalTasks;
    std::atomic<int> currentTaskNum;
};

#endif // CONSOLE_UI_H
