#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime.h"
#include "interpreter.h"

// Helper function to read a file into memory
static char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: fseek() failed for file '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "Error: ftell() returned negative length for file '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)length, file);
    buffer[read_count] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.ember>\n", argv[0]);
        return 1;
    }

    char* source = read_file(argv[1]);
    if (!source) {
        return 1;
    }

    printf("Running script directly via interpreter: %s\n", argv[1]);
    int result = interpreter_execute_script(source);
    
    free(source);
    return result;
} 