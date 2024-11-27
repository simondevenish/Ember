# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Iinclude -std=c11 -Wall -Wextra -g -O0

# Source, Build, and Object files
SRC = src
BUILD_DIR = build

# Source files
SRCS = $(wildcard $(SRC)/*.c) main.c
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))

# Target binary
TARGET = $(BUILD_DIR)/emberscript

# Default build rule
all: $(TARGET)

# Rule to create the target binary
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -o $@ $(OBJS)

# Rule to build object files from source files
$(BUILD_DIR)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build the main.o file
$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Run the interpreter with a script
run: $(TARGET)
	./$(TARGET) scripts/adventure_game.ember

.PHONY: all clean run
