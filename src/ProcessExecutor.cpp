#include <ProcessExecutor.h>
#include <iostream>
#include <array>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdio>
#include <memory>
#endif

#ifdef _WIN32
// Windows implementation using CreateProcess with proper console handling
std::pair<int, std::string> ProcessExecutor::execute(const std::string& command, const std::string& workingDir) {
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
std::pair<int, std::string> ProcessExecutor::execute(const std::string& command, const std::string& workingDir) {
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
