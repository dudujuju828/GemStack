#include <gtest/gtest.h>
#include <ProcessExecutor.h>

TEST(ProcessExecutorTest, EchoCommand) {
    // Basic test to verify command execution works
    auto result = ProcessExecutor::execute("echo Hello");
    
    // Exit code should be 0
    EXPECT_EQ(result.first, 0);
    
    // Output should contain "Hello"
    // Note: Output might vary by platform (newlines etc)
    EXPECT_TRUE(result.second.find("Hello") != std::string::npos);
}

TEST(ProcessExecutorTest, InvalidCommand) {
    // Basic test to verify failure
    auto result = ProcessExecutor::execute("nonexistent_command_12345");
    
    // Exit code should be non-zero
    EXPECT_NE(result.first, 0);
}
