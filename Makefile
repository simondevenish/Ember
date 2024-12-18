# Compiler
CC = gcc
CXX = g++

# Compiler flags for production code (C source)
CFLAGS = -Iinclude -std=c11 -Wall -Wextra -g -O0

# Compiler flags for test code (C++ source)
CXXFLAGS = -Iinclude -std=c++11 -Wall -Wextra -g -O0

# Directories
SRC = src
TESTS = tests
THIRDPARTY = thirdparty
BUILD_DIR = build
TEST_BUILD_DIR = $(BUILD_DIR)/tests
GTEST_DIR = $(THIRDPARTY)/googletest
GTEST_BUILD_DIR = $(BUILD_DIR)/thirdparty

# Source files
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))

# Test sources (CPP files)
TEST_SRCS = $(wildcard $(TESTS)/*.cpp)
TEST_OBJS = $(patsubst %.cpp, $(TEST_BUILD_DIR)/%.o, $(notdir $(TEST_SRCS)))

# Library
LIBRARY = $(BUILD_DIR)/libEmberScript.a

# GTest Sources
# gtest-all.cc compiles all of gtest's sources into one object.
# gtest_main.cc provides a main() function for running tests.
GTEST_SRCS = $(GTEST_DIR)/googletest/src/gtest-all.cc
GTEST_MAIN_SRCS = $(GTEST_DIR)/googletest/src/gtest_main.cc

GTEST_OBJS = $(GTEST_BUILD_DIR)/gtest-all.o
GTEST_MAIN_OBJS = $(GTEST_BUILD_DIR)/gtest_main.o

GTEST_LIB = $(GTEST_BUILD_DIR)/libgtest.a
GTEST_MAIN_LIB = $(GTEST_BUILD_DIR)/libgtest_main.a

# Include paths for GTest
GTEST_CXXFLAGS = -I$(GTEST_DIR)/googletest/include -I$(GTEST_DIR)/googletest -I$(GTEST_DIR)/googlemock/include -I$(GTEST_DIR)/googlemock

# Default build rule
all: $(LIBRARY)

# Build the static library for the main project
$(LIBRARY): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $(OBJS)

# Build object files from project C source files
$(BUILD_DIR)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build GoogleTest libraries
# Compile gtest-all.cc
$(GTEST_OBJS): $(GTEST_SRCS)
	@mkdir -p $(GTEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

# Create libgtest.a
$(GTEST_LIB): $(GTEST_OBJS)
	ar rcs $@ $^

# Compile gtest_main.cc
$(GTEST_MAIN_OBJS): $(GTEST_MAIN_SRCS)
	@mkdir -p $(GTEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

# Create libgtest_main.a
$(GTEST_MAIN_LIB): $(GTEST_MAIN_OBJS)
	ar rcs $@ $^

# Build object files for tests
$(TEST_BUILD_DIR)/%.o: $(TESTS)/%.cpp
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

# Test executable linked against our libEmberScript and GTest libs
run_tests: $(LIBRARY) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$@ $(TEST_OBJS) $(LIBRARY) $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread

# Run tests
check: run_tests
	$(BUILD_DIR)/run_tests

# Clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean check
