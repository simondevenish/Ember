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

# Source files for your core library
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))

# Static library name
LIBRARY = $(BUILD_DIR)/libEmber.a

# Main CLI (emberc) source and objects
# If main.c is in project root, you can leave it as "main.c"
# If it's somewhere else, adjust accordingly.
EMBERC_SRC = main.c
EMBERC_OBJ = $(BUILD_DIR)/main.o
EMBERC_BIN = $(BUILD_DIR)/emberc

# Test sources (C++ files)
TEST_SRCS = $(wildcard $(TESTS)/*.cpp)
TEST_OBJS = $(patsubst %.cpp, $(TEST_BUILD_DIR)/%.o, $(notdir $(TEST_SRCS)))

# GoogleTest sources
GTEST_SRCS = $(GTEST_DIR)/googletest/src/gtest-all.cc
GTEST_MAIN_SRCS = $(GTEST_DIR)/googletest/src/gtest_main.cc

GTEST_OBJS = $(GTEST_BUILD_DIR)/gtest-all.o
GTEST_MAIN_OBJS = $(GTEST_BUILD_DIR)/gtest_main.o

GTEST_LIB = $(GTEST_BUILD_DIR)/libgtest.a
GTEST_MAIN_LIB = $(GTEST_BUILD_DIR)/libgtest_main.a

# Include paths for GTest
GTEST_CXXFLAGS = -I$(GTEST_DIR)/googletest/include \
                 -I$(GTEST_DIR)/googletest \
                 -I$(GTEST_DIR)/googlemock/include \
                 -I$(GTEST_DIR)/googlemock

# Default build rule: build the library and the emberc tool
all: $(LIBRARY) $(EMBERC_BIN)

# -------------------------------------------------------
# 1) Build the static library from all .c files in src/
# -------------------------------------------------------
$(LIBRARY): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $(OBJS)

# Compile each .c in src/ to an object in build/
$(BUILD_DIR)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------------------------------
# 2) Build the emberc CLI
# -------------------------------------------------------
$(EMBERC_OBJ): $(EMBERC_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(EMBERC_BIN): $(EMBERC_OBJ) $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $(EMBERC_OBJ) $(LIBRARY) -lm -lpthread

# -------------------------------------------------------
# 3) GoogleTest (optional)
# -------------------------------------------------------
# Compile gtest-all.cc
$(GTEST_OBJS): $(GTEST_SRCS)
	@mkdir -p $(GTEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

$(GTEST_LIB): $(GTEST_OBJS)
	ar rcs $@ $^

# Compile gtest_main.cc
$(GTEST_MAIN_OBJS): $(GTEST_MAIN_SRCS)
	@mkdir -p $(GTEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

$(GTEST_MAIN_LIB): $(GTEST_MAIN_OBJS)
	ar rcs $@ $^

# -------------------------------------------------------
# 4) Unit tests
# -------------------------------------------------------
$(TEST_BUILD_DIR)/%.o: $(TESTS)/%.cpp
	@mkdir -p $(TEST_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $< -o $@

run_tests: $(LIBRARY) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$@ $(TEST_OBJS) \
        $(LIBRARY) $(GTEST_LIB) $(GTEST_MAIN_LIB) -lpthread

check: run_tests
	$(BUILD_DIR)/run_tests

# -------------------------------------------------------
# 5) Cleanup
# -------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean check run_tests
