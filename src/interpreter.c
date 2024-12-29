#include "interpreter.h"

#include "lexer.h"
#include "parser.h"
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

int interpreter_execute_script(const char* source) {
    if (!source) {
        fprintf(stderr, "Error: Source code is NULL.\n");
        return 1;
    }

    /* -----------------------------
       1) Lexing
       ----------------------------- */
    Lexer lexer;
    lexer_init(&lexer, source);

    /* -----------------------------
       2) Parsing
       ----------------------------- */
    Parser* parser = parser_create(&lexer);
    ASTNode* root = parse_script(parser);
    if (!root) {
        fprintf(stderr, "Error: Parsing failed.\n");
        // Clean up parser
        free(parser);
        return 1;
    }

    /* -----------------------------
       3) Create a Bytecode Chunk
       ----------------------------- */
    BytecodeChunk* chunk = vm_create_chunk();
    if (!chunk) {
        fprintf(stderr, "Error: Failed to create bytecode chunk.\n");
        free_ast(root);
        free(parser);
        return 1;
    }

    /* -----------------------------
       4) Create a Symbol Table
       ----------------------------- */
    SymbolTable* symtab = symbol_table_create();
    if (!symtab) {
        fprintf(stderr, "Error: Failed to create symbol table.\n");
        vm_free_chunk(chunk);
        free_ast(root);
        free(parser);
        return 1;
    }

    /* -----------------------------
       5) Compile AST -> Bytecode
       ----------------------------- */
    if (!compile_ast(root, chunk, symtab)) {
        fprintf(stderr, "Error: Compilation failed.\n");
        symbol_table_free(symtab);
        vm_free_chunk(chunk);
        free_ast(root);
        free(parser);
        return 1;
    }

    /* -----------------------------
       6) Create a VM and run it
       ----------------------------- */
    VM* vm = vm_create(chunk);
    if (!vm) {
        fprintf(stderr, "Error: Failed to create VM.\n");
        symbol_table_free(symtab);
        vm_free_chunk(chunk);
        free_ast(root);
        free(parser);
        return 1;
    }

    int vm_result = vm_run(vm);  // 0 on success, non-zero on error

    /* -----------------------------
       7) Cleanup
       ----------------------------- */
    vm_free(vm);
    symbol_table_free(symtab);
    vm_free_chunk(chunk);
    free_ast(root);
    free(parser);

    return vm_result;
}