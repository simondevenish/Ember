#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "runtime.h"

// Built-in function prototypes
RuntimeValue builtin_print_function(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_add_function(Environment* env, RuntimeValue* args, int arg_count);

// Helper function to read a file
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

    // Initialize the global environment
    Environment* global_env = runtime_create_environment();

    // Register built-in functions
    runtime_register_builtin(global_env, "print", builtin_print_function);
    runtime_register_builtin(global_env, "add", builtin_add_function);

    // Read the script file
    char* script_content = read_file(argv[1]);
    if (!script_content) {
        runtime_free_environment(global_env);
        return 1;
    }

    // Step 1: Lexing
    Lexer lexer;
    lexer_init(&lexer, script_content);

    // Step 2: Parsing
    Parser* parser = parser_create(&lexer);
    ASTNode* ast = parse_script(parser);

    if (!ast) {
        fprintf(stderr, "Error: Failed to parse script.\n");
        free(script_content);
        free(parser);
        runtime_free_environment(global_env);
        return 1;
    }

    // Step 3: Executing the script
    runtime_execute_block(global_env, ast);

    // Cleanup
    free_ast(ast);
    free(script_content);
    free(parser);
    runtime_free_environment(global_env);

    return 0;
}

// Built-in print function
RuntimeValue builtin_print_function(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    for (int i = 0; i < arg_count; i++) {
        char* str = runtime_value_to_string(&args[i]);
        printf("%s", str);
        free(str);
    }
    printf("\n");

    RuntimeValue result = { .type = RUNTIME_VALUE_NULL };
    return result;
}

// Example of another built-in function
RuntimeValue builtin_add_function(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; // Unused
    if (arg_count < 2 || args[0].type != RUNTIME_VALUE_NUMBER || args[1].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: 'add' requires two numeric arguments.\n");
        RuntimeValue result = { .type = RUNTIME_VALUE_NULL };
        return result;
    }

    RuntimeValue result;
    result.type = RUNTIME_VALUE_NUMBER;
    result.number_value = args[0].number_value + args[1].number_value;
    return result;
}