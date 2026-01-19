#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <GemStackCore.h>

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
