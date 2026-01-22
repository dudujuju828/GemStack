#ifndef CLI_MANAGER_H
#define CLI_MANAGER_H

#include <string>

class CliManager {
public:
    // Initialize the CLI (extract if needed)
    static bool initialize();
    
    // Get the path to the CLI executable/script
    static std::string getGeminiCliPath();

private:
    static bool extractEmbeddedCli(const std::string& targetDir);
    static std::string getHomeDirectory();
    
    static std::string s_geminiCliPath;
};

#endif // CLI_MANAGER_H
