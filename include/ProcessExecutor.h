#ifndef PROCESS_EXECUTOR_H
#define PROCESS_EXECUTOR_H

#include <string>
#include <utility>

class ProcessExecutor {
public:
    // Execute a shell command and return {exit_code, output}
    static std::pair<int, std::string> execute(const std::string& command, const std::string& workingDir = "");
};

#endif // PROCESS_EXECUTOR_H
