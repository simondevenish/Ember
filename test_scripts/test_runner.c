#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime.h"
#include "builtins.h"
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

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

void print_ast_detailed(ASTNode* node, int level) {
    if (!node) return;
    
    char indent[50] = {0};
    for (int i = 0; i < level; i++) {
        strcat(indent, "  ");
    }
    
    printf("%s", indent);
    
    switch (node->type) {
        case AST_BLOCK:
            printf("Block (statement count: %d):\n", node->block.statement_count);
            for (int i = 0; i < node->block.statement_count; i++) {
                print_ast_detailed(node->block.statements[i], level + 1);
            }
            break;
            
        case AST_VARIABLE:
            printf("Variable: '%s'\n", node->variable.variable_name);
            break;
            
        case AST_LITERAL:
            printf("Literal: '%s'\n", node->literal.value);
            break;
            
        case AST_BINARY_OP:
            printf("Binary Operation: '%s'\n", node->binary_op.op_symbol);
            print_ast_detailed(node->binary_op.left, level + 1);
            print_ast_detailed(node->binary_op.right, level + 1);
            break;
            
        case AST_FUNCTION_CALL:
            printf("Function Call: '%s' (args: %d)\n", node->function_call.function_name, node->function_call.argument_count);
            for (int i = 0; i < node->function_call.argument_count; i++) {
                print_ast_detailed(node->function_call.arguments[i], level + 1);
            }
            break;
            
        case AST_PROPERTY_ACCESS:
            printf("Property Access: '%s'\n", node->property_access.property);
            print_ast_detailed(node->property_access.object, level + 1);
            break;
            
        case AST_PROPERTY_ASSIGNMENT:
            printf("Property Assignment: '%s'\n", node->property_assignment.property);
            printf("%s  Object:\n", indent);
            print_ast_detailed(node->property_assignment.object, level + 2);
            printf("%s  Value:\n", indent);
            print_ast_detailed(node->property_assignment.value, level + 2);
            break;
            
        case AST_VARIABLE_DECL:
            printf("Variable Declaration: '%s'\n", node->variable_decl.variable_name);
            if (node->variable_decl.initial_value) {
                print_ast_detailed(node->variable_decl.initial_value, level + 1);
            }
            break;
            
        case AST_OBJECT_LITERAL:
            printf("Object Literal (property count: %d):\n", node->object_literal.property_count);
            for (int i = 0; i < node->object_literal.property_count; i++) {
                printf("%s  Property '%s':\n", indent, node->object_literal.keys[i]);
                print_ast_detailed(node->object_literal.values[i], level + 2);
            }
            break;
            
        default:
            printf("Unknown AST Node Type %d\n", node->type);
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script_file>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return 1;
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read the file contents
    char* source = (char*)malloc(file_size + 1);
    if (!source) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(source, 1, file_size, file);
    source[bytes_read] = '\0';
    fclose(file);

    printf("Executing script '%s'...\n", filename);

    // Create a lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // Create a parser
    Parser* parser = parser_create(&lexer);

    // Parse the script into an AST
    ASTNode* ast = parse_script(parser);
    if (!ast) {
        fprintf(stderr, "Error: Parsing failed.\n");
        free(source);
        return 1;
    }
    
    // Print the AST for debugging
    printf("\nAST Structure:\n");
    print_ast_detailed(ast, 0);
    printf("\n");

    // Create a runtime environment
    Environment* env = runtime_create_environment();

    // Register built-in functions
    runtime_register_builtin(env, "print", builtin_print);

    // Execute the AST directly (interpreter mode)
    runtime_execute_block(env, ast);

    // Clean up
    runtime_free_environment(env);
    free_ast(ast);
    free(source);

    printf("Script execution completed.\n");
    return 0;
} 