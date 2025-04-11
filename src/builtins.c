#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "builtins.h"
#include "utils.h"
#include "runtime.h"
#include "sdl_module.h"
#include "raylib_module.h" // Our new module

/**
 * Register all built-in functions to the runtime environment.
 * @param env Pointer to the global runtime environment.
 */
void builtins_register(Environment* env) {
    // Register built-in functions here
    runtime_register_builtin(env, "print", builtin_print);

    // Register math-related functions
    runtime_register_builtin(env, "floor", builtin_floor);
    runtime_register_builtin(env, "ceil", builtin_ceil);
    runtime_register_builtin(env, "sqrt", builtin_sqrt);
    runtime_register_builtin(env, "pow", builtin_pow);
    runtime_register_builtin(env, "sin", builtin_sin);
    runtime_register_builtin(env, "cos", builtin_cos);
    runtime_register_builtin(env, "tan", builtin_tan);
    runtime_register_builtin(env, "log", builtin_log);
    runtime_register_builtin(env, "round", builtin_round);

    runtime_register_builtin(env, "concat", builtin_concat);
    runtime_register_builtin(env, "substring", builtin_substring);
    runtime_register_builtin(env, "to_upper", builtin_to_upper);
    runtime_register_builtin(env, "to_lower", builtin_to_lower);
    runtime_register_builtin(env, "index_of", builtin_index_of);
    runtime_register_builtin(env, "replace", builtin_replace);

    // --------------------------------------------
    // Conditionally register the SDL module if installed
    // --------------------------------------------
    if (is_sdl_installed()) {
        sdl_register_builtins(env);
    }

    if (utils_is_package_installed("raylib")) {
        raylib_register_builtins(env);
    }
}

bool is_sdl_installed(void)
{
    // ember/sdl -> embersdl quick hack to test functionality whilst '/' gets fixed
    return utils_is_package_installed("embersdl");
}

RuntimeValue builtin_floor(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'floor' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = floor(args[0].number_value) };
}

RuntimeValue builtin_ceil(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'ceil' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = ceil(args[0].number_value) };
}

RuntimeValue builtin_sqrt(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'sqrt' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = sqrt(args[0].number_value) };
}

RuntimeValue builtin_pow(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 2 || args[0].type != RUNTIME_VALUE_NUMBER || args[1].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'pow' requires two numeric arguments.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = pow(args[0].number_value, args[1].number_value) };
}

RuntimeValue builtin_sin(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'sin' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = sin(args[0].number_value) };
}

RuntimeValue builtin_cos(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'cos' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = cos(args[0].number_value) };
}

RuntimeValue builtin_tan(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'tan' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = tan(args[0].number_value) };
}

RuntimeValue builtin_log(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'log' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = log(args[0].number_value) };
}

RuntimeValue builtin_round(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'round' requires a single numeric argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }
    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = round(args[0].number_value) };
}

RuntimeValue builtin_concat(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 2 || args[0].type != RUNTIME_VALUE_STRING || args[1].type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "Error: 'concat' requires two string arguments.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* str1 = args[0].string_value;
    const char* str2 = args[1].string_value;
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    char* result_str = malloc(len1 + len2 + 1);
    if (!result_str) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    strcpy(result_str, str1);
    strcat(result_str, str2);

    RuntimeValue result = { .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    return result;
}

RuntimeValue builtin_substring(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 3 || args[0].type != RUNTIME_VALUE_STRING || args[1].type != RUNTIME_VALUE_NUMBER || args[2].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'substring' requires a string and two numeric arguments.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* str = args[0].string_value;
    int start = (int)args[1].number_value;
    int length = (int)args[2].number_value;

    if (start < 0 || length < 0 || start + length > (int)strlen(str)) {
        fprintf(stderr, "Error: Invalid range for 'substring'.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    char* result_str = malloc(length + 1);
    if (!result_str) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    strncpy(result_str, str + start, length);
    result_str[length] = '\0';

    RuntimeValue result = { .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    return result;
}

RuntimeValue builtin_to_upper(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "Error: 'to_upper' requires a single string argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* str = args[0].string_value;
    char* result_str = malloc(strlen(str) + 1);
    if (!result_str) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    for (size_t i = 0; str[i]; ++i) {
        result_str[i] = toupper(str[i]);
    }
    result_str[strlen(str)] = '\0';

    RuntimeValue result = { .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    return result;
}

RuntimeValue builtin_to_lower(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 1 || args[0].type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "Error: 'to_lower' requires a single string argument.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* str = args[0].string_value;
    char* result_str = malloc(strlen(str) + 1);
    if (!result_str) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    for (size_t i = 0; str[i]; ++i) {
        result_str[i] = tolower(str[i]);
    }
    result_str[strlen(str)] = '\0';

    RuntimeValue result = { .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    return result;
}

RuntimeValue builtin_index_of(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 2 || args[0].type != RUNTIME_VALUE_STRING || args[1].type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "Error: 'index_of' requires two string arguments.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* haystack = args[0].string_value;
    const char* needle = args[1].string_value;
    const char* found = strstr(haystack, needle);

    if (!found) {
        return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = -1 };
    }

    return (RuntimeValue){ .type = RUNTIME_VALUE_NUMBER, .number_value = (int)(found - haystack) };
}

RuntimeValue builtin_replace(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    if (arg_count != 3 || args[0].type != RUNTIME_VALUE_STRING || args[1].type != RUNTIME_VALUE_STRING || args[2].type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "Error: 'replace' requires three string arguments.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    const char* str = args[0].string_value;
    const char* search = args[1].string_value;
    const char* replace = args[2].string_value;

    const char* pos = strstr(str, search);
    if (!pos) {
        char* result_str = strdup(str);
        return (RuntimeValue){ .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    }

    size_t before_len = pos - str;
    size_t search_len = strlen(search);
    size_t replace_len = strlen(replace);
    size_t after_len = strlen(pos + search_len);

    char* result_str = malloc(before_len + replace_len + after_len + 1);
    if (!result_str) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
    }

    strncpy(result_str, str, before_len);
    strcpy(result_str + before_len, replace);
    strcpy(result_str + before_len + replace_len, pos + search_len);

    RuntimeValue result = { .type = RUNTIME_VALUE_STRING, .string_value = result_str };
    return result;
}
