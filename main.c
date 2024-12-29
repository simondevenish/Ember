#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"

/**
 * @brief Helper function to read a file’s entire contents into a buffer.
 * 
 * @param filename The path to the file.
 * @return char*   Heap-allocated null-terminated string, or NULL on error.
 */
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script_file>\n", argv[0]);
        return 1;
    }

    // Read the script file
    char* script_content = read_file(argv[1]);
    if (!script_content) {
        // read_file already printed an error; just exit
        return 1;
    }

    // Use your new interpreter (which does lex->parse->compile->vm_run).
    int status = interpreter_execute_script(script_content);

    // Cleanup
    free(script_content);

    // Return the interpreter’s status
    return status;
}
