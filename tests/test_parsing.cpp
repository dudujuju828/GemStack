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

TEST(GemStackParsing, SinglePromptCommand) {
    ClearQueue();
    std::string filename = "test_single.txt";
    std::string content = "GemStackSTART\n";
    content += "prompt \"Hello\"\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 1);
        EXPECT_EQ(commandQueue.front(), "prompt \"Hello\"");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, MultiLinePrompts) {
    ClearQueue();
    std::string filename = "test_multi.txt";
    std::string content = "GemStackSTART\n";
    content += "prompt \"First task\"\n";
    content += "prompt \"Second task\"\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "prompt \"First task\"");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "prompt \"Second task\"");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, IgnoreEmptyLines) {
    ClearQueue();
    std::string filename = "test_empty_lines.txt";
    std::string content = "GemStackSTART\n";
    content += "\n";
    content += "prompt \"Hello\"\n";
    content += "\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 1);
        EXPECT_EQ(commandQueue.front(), "prompt \"Hello\"");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, NonPromptCommands) {
    ClearQueue();
    std::string filename = "test_non_prompt.txt";
    std::string content = "GemStackSTART\n";
    content += "--help\n";
    content += "--version\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "--help");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "--version");
    }
    std::remove(filename.c_str());
}

TEST(GemStackParsing, FileNotFound) {
    ClearQueue();
    bool loaded = loadCommandsFromFile("nonexistent_file_12345.txt");
    EXPECT_FALSE(loaded);
}

// ============================================================================
// Specify Directive Tests
// ============================================================================

TEST(SpecifyDirective, SingleSpecifyBeforePrompt) {
    ClearQueue();
    std::string filename = "test_specify_single.txt";
    std::string content = "GemStackSTART\n";
    content += "prompt \"First task\"\n";
    content += "specify \"Expected result X\"\n";
    content += "prompt \"Second task\"\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        // First prompt unchanged
        EXPECT_EQ(commandQueue.front(), "prompt \"First task\"");
        commandQueue.pop();
        // Second prompt should have checkpoint prepended
        std::string secondPrompt = commandQueue.front();
        EXPECT_TRUE(secondPrompt.find("CHECKPOINT") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("Expected result X") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("Second task") != std::string::npos);
    }
    std::remove(filename.c_str());
}

TEST(SpecifyDirective, MultipleSpecifiesBeforePrompt) {
    ClearQueue();
    std::string filename = "test_specify_multi.txt";
    std::string content = "GemStackSTART\n";
    content += "prompt \"First task\"\n";
    content += "specify \"Check A exists\"\n";
    content += "specify \"Check B is correct\"\n";
    content += "specify \"Check C has value\"\n";
    content += "prompt \"Second task\"\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        commandQueue.pop(); // Skip first prompt
        std::string secondPrompt = commandQueue.front();
        // All three specifications should be present
        EXPECT_TRUE(secondPrompt.find("Check A exists") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("Check B is correct") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("Check C has value") != std::string::npos);
        // Should be numbered
        EXPECT_TRUE(secondPrompt.find("1.") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("2.") != std::string::npos);
        EXPECT_TRUE(secondPrompt.find("3.") != std::string::npos);
    }
    std::remove(filename.c_str());
}

TEST(SpecifyDirective, SpecifyWithoutFollowingPrompt) {
    ClearQueue();
    std::string filename = "test_specify_orphan.txt";
    std::string content = "GemStackSTART\n";
    content += "prompt \"First task\"\n";
    content += "specify \"Orphaned spec\"\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    // Should still load successfully, just warn about orphan
    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        // Only the prompt should be queued, specify is orphaned
        ASSERT_EQ(commandQueue.size(), 1);
        EXPECT_EQ(commandQueue.front(), "prompt \"First task\"");
    }
    std::remove(filename.c_str());
}

// ============================================================================
// PromptBlock Tests
// ============================================================================

TEST(PromptBlock, BasicPromptBlock) {
    ClearQueue();
    std::string filename = "test_prompt_block.txt";
    std::string content = "GemStackSTART\n";
    content += "PromptBlockSTART\n";
    content += "prompt \"Task A\"\n";
    content += "prompt \"Task B\"\n";
    content += "PromptBlockEND\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "prompt \"Task A\"");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "prompt \"Task B\"");
    }
    std::remove(filename.c_str());
}

TEST(PromptBlock, MultiplePromptBlocks) {
    ClearQueue();
    std::string filename = "test_multi_blocks.txt";
    std::string content = "GemStackSTART\n";
    content += "PromptBlockSTART\n";
    content += "prompt \"Block 1 Task\"\n";
    content += "PromptBlockEND\n";
    content += "PromptBlockSTART\n";
    content += "prompt \"Block 2 Task\"\n";
    content += "PromptBlockEND\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        EXPECT_EQ(commandQueue.front(), "prompt \"Block 1 Task\"");
        commandQueue.pop();
        EXPECT_EQ(commandQueue.front(), "prompt \"Block 2 Task\"");
    }
    std::remove(filename.c_str());
}

TEST(PromptBlock, SpecifyResetsAtBlockBoundary) {
    ClearQueue();
    std::string filename = "test_block_reset.txt";
    std::string content = "GemStackSTART\n";
    content += "PromptBlockSTART\n";
    content += "prompt \"Block 1 Task\"\n";
    content += "specify \"Should not carry over\"\n";
    content += "PromptBlockEND\n";
    content += "PromptBlockSTART\n";
    content += "prompt \"Block 2 Task\"\n";
    content += "PromptBlockEND\n";
    content += "GemStackEND";
    CreateTempFile(filename, content);

    bool loaded = loadCommandsFromFile(filename);

    EXPECT_TRUE(loaded);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        ASSERT_EQ(commandQueue.size(), 2);
        commandQueue.pop(); // Skip first
        // Second prompt should NOT have the orphaned specification
        std::string secondPrompt = commandQueue.front();
        EXPECT_TRUE(secondPrompt.find("CHECKPOINT") == std::string::npos);
        EXPECT_TRUE(secondPrompt.find("Should not carry over") == std::string::npos);
    }
    std::remove(filename.c_str());
}

// ============================================================================
// Directive Parsing Helper Tests
// ============================================================================

TEST(DirectiveParsing, ExtractDirectiveContentBasic) {
    std::string line = "prompt \"Hello World\"";
    EXPECT_EQ(extractDirectiveContent(line, "prompt "), "Hello World");
}

TEST(DirectiveParsing, ExtractDirectiveContentSpecify) {
    std::string line = "specify \"Check that X exists\"";
    EXPECT_EQ(extractDirectiveContent(line, "specify "), "Check that X exists");
}

TEST(DirectiveParsing, ExtractDirectiveContentNoQuotes) {
    std::string line = "prompt Hello";
    EXPECT_EQ(extractDirectiveContent(line, "prompt "), "");
}

TEST(DirectiveParsing, ExtractDirectiveContentNotFound) {
    std::string line = "something else";
    EXPECT_EQ(extractDirectiveContent(line, "prompt "), "");
}

TEST(DirectiveParsing, StartsWithDirectiveTrue) {
    EXPECT_TRUE(startsWithDirective("prompt \"test\"", "prompt "));
    EXPECT_TRUE(startsWithDirective("specify \"check\"", "specify "));
}

TEST(DirectiveParsing, StartsWithDirectiveFalse) {
    EXPECT_FALSE(startsWithDirective("something prompt", "prompt "));
    EXPECT_FALSE(startsWithDirective("--help", "prompt "));
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
