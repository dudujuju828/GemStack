#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <GemStackCore.h>

// ============================================================================
// Test Helpers
// ============================================================================

// Helper to reset the queue before each test
void ClearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<std::string> empty;
    std::swap(commandQueue, empty);
}

// Helper to create a temp file
void CreateTempFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
    file.close();
}

// ============================================================================
// File Parsing Tests
// ============================================================================

TEST(GemStackParsing, SingleLineCommand) {
    ClearQueue();
    std::string filename = "test_single.txt";
    CreateTempFile(filename, "GemStackSTART prompt \"Hello\" GemStackEND");

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 1);
        EXPECT_EQ(commandQueue.front(), "prompt \"Hello\"");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, MultiLineCommands) {
    ClearQueue();
    std::string filename = "test_multi.txt";
    std::string content = "GemStackSTART\n";
    content += "cmd1\n";
    content += "cmd2\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "cmd1");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "cmd2");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, IgnoreEmptyLines) {
    ClearQueue();
    std::string filename = "test_empty_lines.txt";
    std::string content = "GemStackSTART\n";
    content += "\n";
    content += "cmd1\n";
    content += "\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 1);
        EXPECT_EQ(commandQueue.front(), "cmd1");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, MixedInlineAndMulti) {
    ClearQueue();
    std::string filename = "test_mixed.txt";
    std::string content = "GemStackSTART cmd1\n";
    content += "cmd2\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "cmd1");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "cmd2");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, FileNotFound) {
    ClearQueue();
    bool loaded = loadCommandsFromFile("nonexistent_file_12345.txt");
    EXPECT_FALSE(loaded);
}

// ============================================================================
// String Utilities Tests
// ============================================================================

TEST(StringUtilities, TrimWhitespace) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\nhello\t\n"), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "   "); // All whitespace returns original
}

TEST(StringUtilities, TrimPreservesMiddleSpaces) {
    EXPECT_EQ(trim("  hello world  "), "hello world");
    EXPECT_EQ(trim("hello   world"), "hello   world");
}

// ============================================================================
// Path Utilities Tests
// ============================================================================

TEST(PathUtilities, NormalizePathBackslashes) {
    EXPECT_EQ(normalizePath("C:\\Users\\test\\folder"), "C:/Users/test/folder");
    EXPECT_EQ(normalizePath("path\\to\\file"), "path/to/file");
}

TEST(PathUtilities, NormalizePathForwardSlashes) {
    EXPECT_EQ(normalizePath("C:/Users/test/folder"), "C:/Users/test/folder");
    EXPECT_EQ(normalizePath("/home/user/folder"), "/home/user/folder");
}

TEST(PathUtilities, NormalizePathTrailingSlash) {
    EXPECT_EQ(normalizePath("path/to/folder/"), "path/to/folder");
    EXPECT_EQ(normalizePath("path\\to\\folder\\"), "path/to/folder");
}

TEST(PathUtilities, NormalizePathSingleSlash) {
    // Root path should keep the slash
    EXPECT_EQ(normalizePath("/"), "/");
}

TEST(PathUtilities, NormalizePathEmpty) {
    EXPECT_EQ(normalizePath(""), "");
}

TEST(PathUtilities, JoinPathBasic) {
    EXPECT_EQ(joinPath("/home/user", "folder"), "/home/user/folder");
    EXPECT_EQ(joinPath("C:/Users", "test"), "C:/Users/test");
}

TEST(PathUtilities, JoinPathWithTrailingSlash) {
    EXPECT_EQ(joinPath("/home/user/", "folder"), "/home/user/folder");
}

TEST(PathUtilities, JoinPathWithLeadingSlash) {
    EXPECT_EQ(joinPath("/home/user", "/folder"), "/home/user/folder");
}

TEST(PathUtilities, JoinPathEmptyBase) {
    EXPECT_EQ(joinPath("", "folder"), "folder");
}

TEST(PathUtilities, JoinPathEmptyRelative) {
    EXPECT_EQ(joinPath("/home/user", ""), "/home/user");
}

TEST(PathUtilities, JoinPathBothEmpty) {
    EXPECT_EQ(joinPath("", ""), "");
}

TEST(PathUtilities, JoinPathNormalizesBackslashes) {
    EXPECT_EQ(joinPath("C:\\Users", "test\\folder"), "C:/Users/test/folder");
}

// ============================================================================
// Security Utilities Tests
// ============================================================================

TEST(SecurityUtilities, EscapeForShellBasic) {
    EXPECT_EQ(escapeForShell("hello"), "hello");
    EXPECT_EQ(escapeForShell("hello world"), "hello world");
}

TEST(SecurityUtilities, EscapeForShellQuotes) {
    EXPECT_EQ(escapeForShell("hello \"world\""), "hello \\\"world\\\"");
}

TEST(SecurityUtilities, EscapeForShellBackslash) {
    EXPECT_EQ(escapeForShell("path\\to\\file"), "path\\\\to\\\\file");
}

TEST(SecurityUtilities, EscapeForShellDollar) {
    EXPECT_EQ(escapeForShell("$HOME"), "\\$HOME");
}

TEST(SecurityUtilities, EscapeForShellBacktick) {
    EXPECT_EQ(escapeForShell("`command`"), "\\`command\\`");
}

TEST(SecurityUtilities, EscapeForShellDangerousChars) {
    // Dangerous characters should be replaced with spaces
    EXPECT_EQ(escapeForShell("cmd1;cmd2"), "cmd1 cmd2");
    EXPECT_EQ(escapeForShell("cmd1&&cmd2"), "cmd1  cmd2");
    EXPECT_EQ(escapeForShell("cmd1|cmd2"), "cmd1 cmd2");
    EXPECT_EQ(escapeForShell("cmd1<file"), "cmd1 file");
    EXPECT_EQ(escapeForShell("cmd1>file"), "cmd1 file");
}

TEST(SecurityUtilities, EscapeForShellNewlines) {
    EXPECT_EQ(escapeForShell("line1\nline2"), "line1 line2");
    EXPECT_EQ(escapeForShell("line1\rline2"), "line1 line2");
}

// ============================================================================
// Model Management Tests
// ============================================================================

TEST(ModelManagement, GetCurrentModel) {
    resetModelToTop();
    std::string model = getCurrentModel();
    EXPECT_EQ(model, modelFallbackList[0]);
}

TEST(ModelManagement, DowngradeModel) {
    resetModelToTop();

    // First downgrade should succeed
    EXPECT_TRUE(downgradeModel());
    EXPECT_EQ(getCurrentModel(), modelFallbackList[1]);

    // Downgrade through all models
    while (downgradeModel()) {
        // Keep downgrading
    }

    // After all downgrades, should be at last model
    EXPECT_EQ(getCurrentModel(), modelFallbackList.back());

    // Further downgrade should fail
    EXPECT_FALSE(downgradeModel());

    // Reset for other tests
    resetModelToTop();
}

TEST(ModelManagement, ResetModelToTop) {
    resetModelToTop();
    downgradeModel();
    downgradeModel();

    resetModelToTop();
    EXPECT_EQ(getCurrentModel(), modelFallbackList[0]);
}

// ============================================================================
// Rate Limit Detection Tests
// ============================================================================

TEST(RateLimitDetection, DetectsRateLimit) {
    EXPECT_TRUE(isModelExhausted("Error: rate limit exceeded"));
    EXPECT_TRUE(isModelExhausted("RATE_LIMIT_EXCEEDED"));
    EXPECT_TRUE(isModelExhausted("quota exceeded for today"));
    EXPECT_TRUE(isModelExhausted("RESOURCE_EXHAUSTED"));
    EXPECT_TRUE(isModelExhausted("too many requests"));
    EXPECT_TRUE(isModelExhausted("Error code: 429"));
}

TEST(RateLimitDetection, NoFalsePositives) {
    EXPECT_FALSE(isModelExhausted("Command completed successfully"));
    EXPECT_FALSE(isModelExhausted("File created"));
    EXPECT_FALSE(isModelExhausted(""));
    EXPECT_FALSE(isModelExhausted("The rate of change is high"));
}

// ============================================================================
// Output Parsing Tests
// ============================================================================

TEST(OutputParsing, ExtractFirstMeaningfulLineBasic) {
    std::string output = "This is the first line\nSecond line\nThird line";
    EXPECT_EQ(extractFirstMeaningfulLine(output), "This is the first line");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineSkipsEmpty) {
    std::string output = "\n\nActual content\nMore content";
    EXPECT_EQ(extractFirstMeaningfulLine(output), "Actual content");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineSkipsGemStackStatus) {
    std::string output = "[GemStack] Processing...\n[GemStack] Status...\nActual result";
    EXPECT_EQ(extractFirstMeaningfulLine(output), "Actual result");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineSkipsFormatting) {
    std::string output = "========\n--------\nActual content";
    EXPECT_EQ(extractFirstMeaningfulLine(output), "Actual content");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineEmpty) {
    EXPECT_EQ(extractFirstMeaningfulLine(""), "Completed");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineAllSkipped) {
    std::string output = "[GemStack] Status\n========\n--------\n";
    EXPECT_EQ(extractFirstMeaningfulLine(output), "Completed");
}

TEST(OutputParsing, ExtractFirstMeaningfulLineTruncates) {
    std::string longLine(300, 'a');
    std::string result = extractFirstMeaningfulLine(longLine, 50);
    EXPECT_EQ(result.length(), 50);
    EXPECT_TRUE(result.find("...") != std::string::npos);
}

TEST(OutputParsing, ExtractFirstMeaningfulLineCustomMaxLength) {
    std::string output = "Short line";
    EXPECT_EQ(extractFirstMeaningfulLine(output, 100), "Short line");
}
