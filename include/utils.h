#ifndef UTILS_H
#define UTILS_H

/**
 * @brief Read the entire contents of a file into a null-terminated string.
 *
 * @param filename The path to the file to read.
 * @return A heap-allocated string containing the entire file contents, or
 *         NULL on error. Caller is responsible for free()ing the returned buffer.
 */
char* read_file(const char* filename);

#endif // UTILS_H
