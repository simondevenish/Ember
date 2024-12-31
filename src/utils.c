#include "utils.h"

#include <stdio.h>   // For FILE, fopen, fread, fclose, etc.
#include <stdlib.h>  // For malloc, free
#include <string.h>  // For strlen, etc. if needed

char* read_file(const char* filename)
{
    FILE* file = fopen(filename, "rb"); // "rb" = read binary
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    // Seek to end to find out how big the file is
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: fseek() failed for '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "Error: ftell() returned negative length for '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    // Rewind to start
    rewind(file);

    // Allocate buffer (+1 for the terminating null)
    char* buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed for reading '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    // Read all bytes
    size_t read_count = fread(buffer, 1, (size_t)length, file);
    buffer[read_count] = '\0'; // Null-terminate

    fclose(file);
    return buffer;
}
