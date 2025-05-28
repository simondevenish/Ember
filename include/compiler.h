// compiler.h
#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"
#include "virtual_machine.h"

/**
 * @brief Simple structure to hold symbol info (variable or function).
 *        For now, we only handle top-level variables. You could extend
 *        it for function references, local scopes, etc.
 */
typedef struct {
    char* name;
    int index;       // For variables: index in the VM's "global" or local list
    bool isFunction; // If it's a function name
    bool is_mutable; // NEW: Track mutability (false for let variables)
} Symbol;

/**
 * @brief Table of all symbols encountered (variables, function names).
 *        This is a simplistic approach; more advanced compilers do
 *        multiple passes or build real scope structures.
 */
typedef struct {
    Symbol* symbols;
    int capacity;
    int count;
} SymbolTable;

/**
 * @brief Create an empty symbol table.
 */
SymbolTable* symbol_table_create();

/**
 * @brief Free a symbol table.
 */
void symbol_table_free(SymbolTable* table);

/**
 * @brief Find or insert a symbol (variable or function) in the table.
 *        Returns the index that will be used for load/store.
 */
int symbol_table_get_or_add(SymbolTable* table, const char* name, bool isFunction);

/**
 * @brief Find or insert a variable symbol with mutability tracking.
 *        Returns the index that will be used for load/store.
 */
int symbol_table_get_or_add_variable(SymbolTable* table, const char* name, bool is_mutable);

/**
 * @brief Check if a variable is mutable (for assignment validation).
 *        Returns true if mutable, false if immutable (let), or false if not found.
 */
bool symbol_table_is_variable_mutable(SymbolTable* table, const char* name);

/**
 * @brief Compile the given AST into bytecode (stored in `chunk`).
 *        Returns `true` on success, `false` on error.
 */
bool compile_ast(ASTNode* ast, BytecodeChunk* chunk, SymbolTable* symtab);

#endif // COMPILER_H
