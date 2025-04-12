#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "compiler.h"

// Stub for builtin_print
RuntimeValue builtin_print(Environment* env, RuntimeValue* args, int arg_count) {
    printf("PRINT: ");
    for (int i = 0; i < arg_count; i++) {
        char* str = runtime_value_to_string(&args[i]);
        printf("%s", str);
        free(str);
    }
    printf("\n");
    
    RuntimeValue result = { .type = RUNTIME_VALUE_NULL };
    return result;
}

// Stub for raylib_register_builtins
void raylib_register_builtins(Environment* env) {
    // No-op for test
    (void)env;
}

// Stub for symbol_table functions
typedef struct SymbolTable {
    int dummy;
} SymbolTable;

SymbolTable* symbol_table_create() {
    return calloc(1, sizeof(SymbolTable));
}

void symbol_table_free(SymbolTable* symtab) {
    free(symtab);
}

// Placeholder for compile_ast
bool compile_ast(ASTNode* root, BytecodeChunk* chunk, SymbolTable* symtab) {
    // Skip bytecode compilation for our test
    (void)root;
    (void)chunk;
    (void)symtab;
    return true;
} 