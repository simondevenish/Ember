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
#include <unistd.h>  // For access(), etc. on POSIX
#include <dirent.h>
#endif

//
// ============= UTILITY & JSON HANDLING ==============
//

static void print_usage(void);
static const char* emberpm_get_local_dir(void);
static bool emberpm_ensure_local_dir(void);

// Forward declarations for actual commands
static int emberpm_cmd_install(const char* packageName);
static int emberpm_cmd_uninstall(const char* packageName);
static int emberpm_cmd_list(void);
static int emberpm_cmd_search(const char* term);

// Registry read/write
#define EMBERPM_REGISTRY "packages.json"

/** 
 * @brief A minimal package descriptor. 
 */
typedef struct {
    char name[256];      // e.g. "ember/net"
    char version[64];    // e.g. "0.1.0", or "git:..." or anything
} EmberPackage;

/** 
 * @brief A minimal list of installed packages. 
 */
typedef struct {
    EmberPackage* pkgs;
    size_t count;
} EmberPackageList;

/**
 * @brief Simple function to read an entire file into memory.
 *        Caller must free() the returned buffer.
 */
static char* emberpm_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/**
 * @brief Write a buffer to a file (overwrite).
 */
static bool emberpm_write_file(const char* path, const char* data) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(data);
    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return (wr == len);
}

/**
 * @brief Very simplistic JSON parsing to read a top-level structure like:
 *        {
 *          "packages":[
 *             {"name":"ember/net","version":"0.0.1"},
 *             ...
 *          ]
 *        }
 * 
 *        This returns an EmberPackageList.
 *        On error, returns an empty list (count=0, pkgs=NULL).
 */
static EmberPackageList emberpm_read_registry(void) {
    EmberPackageList result;
    result.count = 0;
    result.pkgs = NULL;

    // Build path: e.g. ~/.ember/pm/packages.json
    char regPath[1024];
    snprintf(regPath, sizeof(regPath), "%s/%s", emberpm_get_local_dir(), EMBERPM_REGISTRY);

    char* json = emberpm_read_file(regPath);
    if (!json) {
        // No registry or failed to read => we treat it as empty
        return result;
    }

    // Minimal naive parse:
    // We look for "packages":[
    char* pkgsKey = strstr(json, "\"packages\"");
    if (!pkgsKey) {
        free(json);
        return result;  // no "packages" key found
    }
    char* bracket = strchr(pkgsKey, '[');
    if (!bracket) {
        free(json);
        return result;
    }
    // Now bracket points to '[' for the array of packages
    // We'll parse each object until ']'.
    char* endArr = strchr(bracket, ']');
    if (!endArr) {
        free(json);
        return result;
    }

    // We'll copy out just that portion:
    size_t arrLen = (size_t)(endArr - bracket + 1);
    char* arrBuf = (char*)malloc(arrLen + 1);
    if (!arrBuf) {
        free(json);
        return result;
    }
    strncpy(arrBuf, bracket, arrLen);
    arrBuf[arrLen] = '\0';

    // Now parse each object like: {"name":"X","version":"Y"}
    // We'll do a super naive approach: find each {"name":..., "version":...}
    // Count occurrences
    char* cursor = arrBuf;
    const int MAX_PACKS = 100;  // arbitrary
    EmberPackage* temp = (EmberPackage*)malloc(sizeof(EmberPackage) * MAX_PACKS);
    if (!temp) {
        free(json);
        free(arrBuf);
        return result;
    }
    size_t idx = 0;

    while (1) {
        char* objStart = strstr(cursor, "{\"name\"");
        if (!objStart || objStart >= endArr) break;
        // find end of object
        char* objEnd = strchr(objStart, '}');
        if (!objEnd) break;

        // extract name
        // look for "name":" => skip 7 chars => read until next quote
        char* nmKey = strstr(objStart, "\"name\"");
        if (!nmKey || nmKey > objEnd) break;
        char* nmVal = strstr(nmKey, ":\"");
        if (!nmVal || nmVal > objEnd) break;
        nmVal += 2; // skip :"
        char nameBuf[256] = {0};
        int n = 0;
        while (*nmVal && *nmVal != '"' && nmVal < objEnd && n < 255) {
            nameBuf[n++] = *nmVal++;
        }
        nameBuf[n] = '\0';

        // extract version
        // look for "version":" => skip
        char versionBuf[64] = {0};
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
        if (idx >= MAX_PACKS) break;

        cursor = objEnd + 1; // move past current object
    }

    // finalize
    if (idx > 0) {
        result.count = idx;
        result.pkgs = (EmberPackage*)malloc(sizeof(EmberPackage) * idx);
        memcpy(result.pkgs, temp, sizeof(EmberPackage) * idx);
    }

    free(temp);
    free(arrBuf);
    free(json);
    return result;
}

/**
 * @brief Write the given list of packages to the registry file in JSON format.
 */
static void emberpm_write_registry(const EmberPackageList* pkgList) {
    // We'll build a JSON string
    // {
    //   "packages":[
    //     {"name":"X","version":"Y"},
    //     ...
    //   ]
    // }

    // Very naive string building
    char* jsonBuf = (char*)malloc(10 * 1024);  // up to 10 KB
    if (!jsonBuf) return;
    strcpy(jsonBuf, "{\n  \"packages\":[\n");

    for (size_t i = 0; i < pkgList->count; i++) {
        char line[512];
        snprintf(line, sizeof(line),
                 "    {\"name\":\"%s\",\"version\":\"%s\"}",
                 pkgList->pkgs[i].name,
                 pkgList->pkgs[i].version[0] ? pkgList->pkgs[i].version : "0.0.0");

        strcat(jsonBuf, line);
        if (i + 1 < pkgList->count) strcat(jsonBuf, ",\n");
        else strcat(jsonBuf, "\n");
    }
    strcat(jsonBuf, "  ]\n}\n");

    // write to disk
    char regPath[1024];
    snprintf(regPath, sizeof(regPath), "%s/%s", emberpm_get_local_dir(), EMBERPM_REGISTRY);
    emberpm_write_file(regPath, jsonBuf);

    free(jsonBuf);
}

/**
 * @brief Find package index by name in the package list, or -1 if not found.
 */
static int emberpm_find_package_index(const EmberPackageList* pkgList, const char* name) {
    for (size_t i = 0; i < pkgList->count; i++) {
        if (strcmp(pkgList->pkgs[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

//
// ============= MAIN PM COMMANDS ==============
//

static int emberpm_cmd_install(const char* packageName) {
    if (!emberpm_ensure_local_dir()) {
        fprintf(stderr, "Error: Could not create local Ember PM directory.\n");
        return 1;
    }

    // Load current registry
    EmberPackageList reg = emberpm_read_registry();

    // Check if already installed
    int existing = emberpm_find_package_index(&reg, packageName);
    if (existing >= 0) {
        printf("Package '%s' is already installed. (version: %s)\n",
               reg.pkgs[existing].name,
               reg.pkgs[existing].version);
        free(reg.pkgs);
        return 0;
    }

    printf("Installing package '%s'...\n", packageName);

    // Download or copy placeholder
    // e.g., create a subdir under .ember/pm/<packageName> ?

    // Expand the list by 1
    EmberPackageList newReg;
    newReg.count = reg.count + 1;
    newReg.pkgs = (EmberPackage*)malloc(sizeof(EmberPackage) * newReg.count);

    // copy old
    for (size_t i = 0; i < reg.count; i++) {
        newReg.pkgs[i] = reg.pkgs[i];
    }

    // add new
    strncpy(newReg.pkgs[reg.count].name, packageName, 255);
    strncpy(newReg.pkgs[reg.count].version, "0.1.0", 63); // placeholder

    // free old reg
    free(reg.pkgs);

    // Write
    emberpm_write_registry(&newReg);
    free(newReg.pkgs);

    printf("Package '%s' installed successfully!\n", packageName);
    return 0;
}

static int emberpm_cmd_uninstall(const char* packageName) {
    if (!emberpm_ensure_local_dir()) {
        fprintf(stderr, "Error: Could not access local Ember PM directory.\n");
        return 1;
    }
    // load registry
    EmberPackageList reg = emberpm_read_registry();
    int idx = emberpm_find_package_index(&reg, packageName);
    if (idx < 0) {
        printf("Package '%s' is not installed.\n", packageName);
        free(reg.pkgs);
        return 0;
    }

    printf("Uninstalling package '%s'...\n", packageName);

    // remove from array
    EmberPackageList newReg;
    newReg.count = reg.count - 1;
    newReg.pkgs = NULL;
    if (newReg.count > 0) {
        newReg.pkgs = (EmberPackage*)malloc(sizeof(EmberPackage) * newReg.count);
        size_t j = 0;
        for (size_t i = 0; i < reg.count; i++) {
            if ((int)i == idx) continue; // skip
            newReg.pkgs[j++] = reg.pkgs[i];
        }
    }
    free(reg.pkgs);

    // remove local files? e.g. .ember/pm/ember/net
    // We'll do a minimal approach
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", emberpm_get_local_dir(), packageName);
    // TODO: remove directory if it exists

    // Write out new
    emberpm_write_registry(&newReg);
    free(newReg.pkgs);

    printf("Package '%s' uninstalled.\n", packageName);
    return 0;
}

static int emberpm_cmd_list(void) {
    if (!emberpm_ensure_local_dir()) {
        fprintf(stderr, "Error: Could not access local Ember PM directory.\n");
        return 1;
    }

    EmberPackageList reg = emberpm_read_registry();
    printf("Installed packages:\n");
    if (reg.count == 0) {
        printf("  (none)\n");
        free(reg.pkgs);
        return 0;
    }
    for (size_t i = 0; i < reg.count; i++) {
        printf("  %s (version: %s)\n", reg.pkgs[i].name, reg.pkgs[i].version);
    }
    free(reg.pkgs);
    return 0;
}

static int emberpm_cmd_search(const char* term) {
    if (!emberpm_ensure_local_dir()) {
        fprintf(stderr, "Error: Could not access local Ember PM directory.\n");
        return 1;
    }
    EmberPackageList reg = emberpm_read_registry();
    printf("Searching for packages matching '%s' in local registry...\n", term);

    bool foundAny = false;
    for (size_t i = 0; i < reg.count; i++) {
        if (strstr(reg.pkgs[i].name, term)) {
            printf("  %s (version: %s)\n", reg.pkgs[i].name, reg.pkgs[i].version);
            foundAny = true;
        }
    }
    if (!foundAny) {
        printf("No matches found in local registry.\n");
    }

    // For a real “remote” search, you'd do an HTTP request to a package registry.
    free(reg.pkgs);
    return 0;
}

//
// ============= MAIN ENTRY POINT ==============
//

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* command = argv[1];

    if (strcmp(command, "help") == 0) {
        print_usage();
        return 0;
    }
    else if (strcmp(command, "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'install' requires a package name.\n");
            return 1;
        }
        const char* packageName = argv[2];
        return emberpm_cmd_install(packageName);
    }
    else if (strcmp(command, "uninstall") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'uninstall' requires a package name.\n");
            return 1;
        }
        const char* packageName = argv[2];
        return emberpm_cmd_uninstall(packageName);
    }
    else if (strcmp(command, "list") == 0) {
        return emberpm_cmd_list();
    }
    else if (strcmp(command, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'search' requires a term.\n");
            return 1;
        }
        const char* term = argv[2];
        return emberpm_cmd_search(term);
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n\n", command);
        print_usage();
        return 1;
    }

    return 0;
}

//
// ============= IMPLEMENTATION HELPERS ==============
//

static void print_usage(void) {
    printf(
        "Usage: emberpm <command> [arguments]\n"
        "\n"
        "Commands:\n"
        "  install   <package>    Install a package from a registry or local path.\n"
        "  uninstall <package>    Remove a previously installed package.\n"
        "  list                  List installed packages.\n"
        "  search    <term>       Search for packages matching <term> in local registry.\n"
        "  help                  Show this help.\n"
        "\n"
        "Examples:\n"
        "  emberpm install ember/net\n"
        "  emberpm uninstall ember/net\n"
        "  emberpm list\n"
        "  emberpm search net\n"
        "\n"
    );
}

/**
 * @brief Return the path to the user's local Ember PM directory.
 *        For example: ~/.ember/pm/ on POSIX systems, 
 *        or %USERPROFILE%\\.ember\\pm on Windows.
 */
static const char* emberpm_get_local_dir(void) {
#ifdef _WIN32
    // On Windows, you might use %APPDATA% or something like that
    const char* home = getenv("USERPROFILE");
    if (!home) home = "C:\\Users\\Default";
    static char pathBuf[1024];
    snprintf(pathBuf, sizeof(pathBuf), "%s\\.ember\\pm", home);
    return pathBuf;
#else
    // On Linux/macOS, use $HOME
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";  // fallback
    static char pathBuf[1024];
    snprintf(pathBuf, sizeof(pathBuf), "%s/.ember/pm", home);
    return pathBuf;
#endif
}

/**
 * @brief Ensure that the local Ember PM directory exists. Create if needed.
 */
static bool emberpm_ensure_local_dir(void) {
    const char* dir = emberpm_get_local_dir();

#ifdef _WIN32
    // On Windows, mkdir creates one level only
    if (mkdir(dir) != 0) {
        // If fails but directory already exists => ignore
        // Otherwise handle error
        if (errno != EEXIST) {
            // Some error other than "already exists"
            // Could check `_access(dir, 0)` or similar
        }
    }
#else
    // On POSIX
    if (mkdir(dir, 0755) != 0) {
        if (errno != EEXIST) {
            // Some error other than "already exists"
        }
    }
#endif
    return true;
}
