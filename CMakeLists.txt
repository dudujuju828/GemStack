cmake_minimum_required(VERSION 3.20)
project(GemStack)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Library for core logic (shared between main app and tests)
add_library(GemStackCore GemStackCore.cpp)

# Main executable
add_executable(GemStack main.cpp)
target_link_libraries(GemStack PRIVATE GemStackCore)

# Test executable
enable_testing()

add_executable(GemStackTests tests/test_parsing.cpp)
target_link_libraries(GemStackTests PRIVATE GemStackCore GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(GemStackTests)
