#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>  // For _mkdir on Windows
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

// -----------------------------------------------------------------------------
// Existing read_file implementation (unchanged).
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Helper: Return the user's local Ember PM directory (e.g. ~/.ember/pm/).
// This is basically the same logic used in emberpm_get_local_dir().
// -----------------------------------------------------------------------------
static const char* get_local_pm_dir(void) {
#ifdef _WIN32
    // On Windows, might use %APPDATA% or similar, but here's a sample:
    const char* home = getenv("USERPROFILE");
    if (!home) home = "C:\\Users\\Default";
    static char pathBuf[1024];
    snprintf(pathBuf, sizeof(pathBuf), "%s\\.ember\\pm", home);
    return pathBuf;
#else
    // On POSIX
    const char* home = getenv("HOME");
    if (!home) home = "/tmp"; // fallback
    static char pathBuf[1024];
    snprintf(pathBuf, sizeof(pathBuf), "%s/.ember/pm", home);
    return pathBuf;
#endif
}

// -----------------------------------------------------------------------------
// Implementation of utils_read_installed_packages()
// -----------------------------------------------------------------------------
EmberPackageList utils_read_installed_packages(void) {
    EmberPackageList result;
    result.count = 0;
    result.pkgs = NULL;

    // Build path: e.g. ~/.ember/pm/packages.json
    char regPath[1024];
    snprintf(regPath, sizeof(regPath), "%s/packages.json", get_local_pm_dir());

    // Read the file
    char* json = read_file(regPath);
    if (!json) {
        // If the file doesn't exist or couldn't be read, return empty list
        return result;
    }

    // Minimal naive parse:
    // 1) find "packages":[
    char* pkgsKey = strstr(json, "\"packages\"");
    if (!pkgsKey) {
        free(json);
        return result;  // no "packages" key found => empty
    }

    char* bracket = strchr(pkgsKey, '[');
    if (!bracket) {
        free(json);
        return result;
    }

    // 2) parse until ']'
    char* endArr = strchr(bracket, ']');
    if (!endArr) {
        free(json);
        return result;
    }

    size_t arrLen = (size_t)(endArr - bracket + 1);
    char* arrBuf = (char*)malloc(arrLen + 1);
    if (!arrBuf) {
        free(json);
        return result;
    }
    strncpy(arrBuf, bracket, arrLen);
    arrBuf[arrLen] = '\0';

    // Now parse each object like: {"name":"X","version":"Y"}
    const int MAX_PACKS = 100;
    EmberPackage* temp = (EmberPackage*)malloc(sizeof(EmberPackage) * MAX_PACKS);
    if (!temp) {
        free(arrBuf);
        free(json);
        return result;
    }

    size_t idx = 0;
    char* cursor = arrBuf;
    while (1) {
        char* objStart = strstr(cursor, "{\"name\"");
        if (!objStart || objStart >= endArr) {
            break;
        }
        char* objEnd = strchr(objStart, '}');
        if (!objEnd) {
            break;
        }

        // extract name
        char* nmKey = strstr(objStart, "\"name\"");
        if (!nmKey || nmKey > objEnd) break;
        char* nmVal = strstr(nmKey, ":\"");
        if (!nmVal || nmVal > objEnd) break;
        nmVal += 2; // skip :"
        char nameBuf[256];
        memset(nameBuf, 0, sizeof(nameBuf));
        int n = 0;
        while (*nmVal && *nmVal != '"' && nmVal < objEnd && n < 255) {
            nameBuf[n++] = *nmVal++;
        }
        nameBuf[n] = '\0';

        // extract version
        char versionBuf[64];
        memset(versionBuf, 0, sizeof(versionBuf));
        char* verKey = strstr(objStart, "\"version\"");
        if (verKey && verKey < objEnd) {
            char* verVal = strstr(verKey, ":\"");
            if (verVal && verVal < objEnd) {
                verVal += 2;
                int v = 0;
                while (*verVal && *verVal != '"' && verVal < objEnd && v < 63) {
                    versionBuf[v++] = *verVal++;
                }
                versionBuf[v] = '\0';
            }
        }

        // store in temp
        strncpy(temp[idx].name, nameBuf, 255);
        strncpy(temp[idx].version, versionBuf, 63);

        idx++;
        if (idx >= MAX_PACKS) {
            break;
        }

        cursor = objEnd + 1; // move past current object
    }

    if (idx > 0) {
        result.count = idx;
        result.pkgs = (EmberPackage*)malloc(sizeof(EmberPackage) * idx);
        memcpy(result.pkgs, temp, idx * sizeof(EmberPackage));
    }

    free(temp);
    free(arrBuf);
    free(json);
    return result;
}

// -----------------------------------------------------------------------------
// Implementation of utils_is_package_installed(packageName)
// -----------------------------------------------------------------------------
bool utils_is_package_installed(const char* packageName) {
    EmberPackageList pkgList = utils_read_installed_packages();
    if (pkgList.count == 0 || !pkgList.pkgs) {
        return false;
    }

    bool found = false;
    for (size_t i = 0; i < pkgList.count; i++) {
        if (strcmp(pkgList.pkgs[i].name, packageName) == 0) {
            found = true;
            break;
        }
    }

    free(pkgList.pkgs); // cleanup
    return found;
}
