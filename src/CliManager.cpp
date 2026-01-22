#include <CliManager.h>
#include <EmbeddedCli.h>
#include <GemStackCore.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

std::string CliManager::s_geminiCliPath;

std::string CliManager::getGeminiCliPath() {
    return s_geminiCliPath;
}

std::string CliManager::getHomeDirectory() {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (home) return std::string(home);
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) return std::string(drive) + std::string(path);
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    return ".";
#endif
}

bool CliManager::extractEmbeddedCli(const std::string& targetDir) {
    // Check if main file exists (simple check)
    if (fs::exists(targetDir + "/gemini.js")) {
        return true;
    }

    std::cout << "[GemStack] Extracting embedded Gemini CLI to " << targetDir << "..." << std::endl;
    
    try {
        fs::create_directories(targetDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }

    std::string tempZipPath = targetDir + "/cli.zip";
    std::ofstream zipFile(tempZipPath, std::ios::binary);
    if (!zipFile) {
        std::cerr << "Failed to create temp zip file: " << tempZipPath << std::endl;
        return false;
    }
    zipFile.write(reinterpret_cast<const char*>(EMBEDDED_CLI_DATA), EMBEDDED_CLI_SIZE);
    zipFile.close();
    
    std::string command = "tar -xf \"" + tempZipPath + "\" -C \"" + targetDir + "\"";
    int result = system(command.c_str());
    
    try {
        fs::remove(tempZipPath);
    } catch (...) {}
    
    if (result != 0) {
        std::cerr << "Failed to extract CLI using tar." << std::endl;
        #ifdef _WIN32
        std::cerr << "Trying PowerShell Expand-Archive..." << std::endl;
        std::ofstream zipFile2(tempZipPath, std::ios::binary);
        zipFile2.write(reinterpret_cast<const char*>(EMBEDDED_CLI_DATA), EMBEDDED_CLI_SIZE);
        zipFile2.close();
        
        std::string psCommand = "powershell -Command \"Expand-Archive -Path '" + tempZipPath + "' -DestinationPath '" + targetDir + "' -Force\"";
        result = system(psCommand.c_str());
        try { fs::remove(tempZipPath); } catch (...) {}
        
        if (result != 0) {
             std::cerr << "Failed to extract CLI using PowerShell." << std::endl;
             return false;
        }
        #else
        return false;
        #endif
    }
    
    return true;
}

bool CliManager::initialize() {
    std::string home = getHomeDirectory();
    std::string cliDir = joinPath(home, ".gemstack/gemini-cli");
    
    if (!extractEmbeddedCli(cliDir)) {
        return false;
    }
    
    s_geminiCliPath = joinPath(cliDir, "gemini.js");
    return true;
}
