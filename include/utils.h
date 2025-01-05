#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Read the entire contents of a file into a null-terminated string.
 *
 * @param filename The path to the file to read.
 * @return A heap-allocated string containing the entire file contents, or
 *         NULL on error. Caller is responsible for free()ing the returned buffer.
 */
char* read_file(const char* filename);

/** 
 * A minimal package descriptor, same as in emberpm. 
 */
typedef struct {
    char name[256];      // e.g. "ember/net"
    char version[64];    // e.g. "0.1.0"
} EmberPackage;

/** 
 * A list of installed packages.
 */
typedef struct {
    EmberPackage* pkgs;  
    size_t count;
} EmberPackageList;

/**
 * @brief Reads the local Ember PM registry (packages.json) and returns 
 *        a list of installed packages.
 * 
 * The caller is responsible for free()ing the returned `pkgList.pkgs`.
 *
 * @return EmberPackageList
 */
EmberPackageList utils_read_installed_packages(void);

/**
 * @brief Checks whether a particular package (by name) is installed.
 * 
 * @param packageName The full package name, e.g. "ember/sdl"
 * @return true if found in the local registry, false otherwise
 */
bool utils_is_package_installed(const char* packageName);

#endif // UTILS_H
