#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime.h"
#include "builtins.h"
#include "lexer.h"
#include "parser.h"

// Helper function to read a file's entire contents into a buffer
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

// Builtin print function
RuntimeValue builtin_print(Environment* env, RuntimeValue* args, int arg_count) {
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <script_file>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    char* script_content = read_file(filename);
    if (!script_content) {
        return 1;
    }

    // Initialize runtime environment
    Environment* env = runtime_create_environment();
    
    // Register built-in print function
    runtime_register_builtin(env, "print", builtin_print);

    // Run the script
    printf("=== Running EmberScript test: %s ===\n\n", filename);
    
    // Use the AST interpreter directly (not bytecode)
    Lexer lexer;
    lexer_init(&lexer, script_content);
    
    Parser* parser = parser_create(&lexer);
    ASTNode* root = parse_script(parser);
    
    if (root) {
        runtime_execute_block(env, root);
        free_ast(root);
    } else {
        fprintf(stderr, "Error: Failed to parse script.\n");
    }
    
    // Clean up
    free(parser);
    free(script_content);
    runtime_free_environment(env);
    
    printf("\n=== Test Complete ===\n");
    
    return 0;
} 