#include <ConsoleUI.h>
#include <iostream>
#include <vector>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#endif

ConsoleUI::ConsoleUI() : animationRunning(false), totalTasks(0), currentTaskNum(0) {}

ConsoleUI::~ConsoleUI() {
    stopAnimation();
}

void ConsoleUI::setTotalTasks(int total) {
    totalTasks.store(total);
}

void ConsoleUI::incrementTaskProgress() {
    currentTaskNum.fetch_add(1);
}

void ConsoleUI::resetProgress() {
    totalTasks.store(0);
    currentTaskNum.store(0);
}

void ConsoleUI::startAnimation() {
    if (animationRunning.load()) return;
    animationRunning.store(true);
    animationThread = std::thread(&ConsoleUI::statusAnimation, this);
}

void ConsoleUI::stopAnimation() {
    animationRunning.store(false);
    if (animationThread.joinable()) {
        animationThread.join();
    }
}

#ifdef _WIN32
// Windows: Write directly to console buffer at specific position (non-blocking, independent of cout)
void ConsoleUI::writeStatusLine(const std::string& text, bool clear) {
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

void ConsoleUI::statusAnimation() {
    int dotCount = 0;

    while (animationRunning.load()) {
        // Build progress prefix if we have task info
        std::string progressPrefix;
        int current = currentTaskNum.load();
        int total = totalTasks.load();
        if (total > 0 && current > 0) {
            progressPrefix = "[" + std::to_string(current) + "/" + std::to_string(total) + "] ";
        }

        std::string dots(dotCount + 1, '.');
        std::string padding(3 - dotCount, ' ');
        std::string statusText = progressPrefix + "GemStack Generating " + dots + padding;

        writeStatusLine(statusText);

        dotCount = (dotCount + 1) % 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    // Clear the status line when done
    writeStatusLine("", true);
}

#else
// Unix: Use ANSI codes but write to stderr to avoid interfering with stdout
void ConsoleUI::statusAnimation() {
    int dotCount = 0;

    // Get terminal height
    int termHeight = 24;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        termHeight = w.ws_row;
    }

    while (animationRunning.load()) {
        // Build progress prefix if we have task info
        std::string progressPrefix;
        int current = currentTaskNum.load();
        int total = totalTasks.load();
        if (total > 0 && current > 0) {
            progressPrefix = "[" + std::to_string(current) + "/" + std::to_string(total) + "] ";
        }

        std::string dots(dotCount + 1, '.');
        std::string padding(3 - dotCount, ' ');
        std::string statusText = progressPrefix + "GemStack Generating";

        // Write to stderr using ANSI codes - independent of stdout buffering
        fprintf(stderr, "\033[s\033[%d;1H\033[K\033[36m%s %s%s\033[0m\033[u",
                termHeight, statusText.c_str(), dots.c_str(), padding.c_str());
        fflush(stderr);

        dotCount = (dotCount + 1) % 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    // Clear the status line when done
    fprintf(stderr, "\033[s\033[%d;1H\033[K\033[u", termHeight);
    fflush(stderr);
}
#endif
