cmake_minimum_required(VERSION 3.20)
project(GemStack)

set(CMAKE_CXX_STANDARD 20)

# Output executable to project root directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR})

# Include directories
include_directories(include)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Library for core logic (shared between main app and tests)
add_library(GemStackCore src/GemStackCore.cpp src/GitAutoCommit.cpp src/ProcessExecutor.cpp src/ConsoleUI.cpp src/CliManager.cpp)
target_include_directories(GemStackCore PUBLIC include)

# Main executable
add_executable(GemStack src/main.cpp)
target_link_libraries(GemStack PRIVATE GemStackCore)

# Test executable
enable_testing()

add_executable(GemStackTests tests/test_parsing.cpp tests/test_git_auto_commit.cpp tests/test_multiline.cpp tests/test_process_executor.cpp)
target_link_libraries(GemStackTests PRIVATE GemStackCore GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(GemStackTests)