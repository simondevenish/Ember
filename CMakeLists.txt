cmake_minimum_required(VERSION 3.10)
project(EmberScript C CXX)

# Set the C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Include directory
include_directories(include)

# Compiler warnings and other flags
# These replicate -Wall -Wextra -g -O0 for both C and C++
add_compile_options(-Wall -Wextra -g -O0)

# Gather all source files from src/
file(GLOB EMBERSCRIPT_SOURCES src/*.c)

# Create static library for EmberScript
add_library(EmberScript STATIC ${EMBERSCRIPT_SOURCES})

# Enable testing
enable_testing()

add_subdirectory(thirdparty/googletest)

# Gather all test files from tests/
file(GLOB TEST_SOURCES tests/*.cpp)

# Create the test executable
add_executable(run_tests ${TEST_SOURCES})

# Link test executable with EmberScript library and GTest libraries
target_link_libraries(run_tests PRIVATE EmberScript gtest gtest_main pthread)

# Add the test to ctest
add_test(NAME EmberScriptTests COMMAND run_tests)

# Optionally, you can add a custom target `check` to run tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS run_tests)
