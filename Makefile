# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Iinclude -std=c11 -Wall -Wextra -g -O0

# Source, Build, and Object files
SRC = src
BUILD_DIR = build

# Source files
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))

# Target library
LIBRARY = $(BUILD_DIR)/libEmberScript.a

# Default build rule
all: $(LIBRARY)

# Rule to create the static library
$(LIBRARY): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $(OBJS)

# Rule to build object files from source files
$(BUILD_DIR)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
