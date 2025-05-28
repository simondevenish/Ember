// compiler.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "virtual_machine.h"
#include "parser.h"  // For ASTNodeType, ASTNode, etc.
#include "utils.h"

static void compile_node(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);

/* -------------------------------------------------------
   Symbol Table Implementation
   ------------------------------------------------------- */
SymbolTable* symbol_table_create() {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!table) return NULL;
    table->symbols = NULL;
    table->capacity = 0;
    table->count = 0;
    return table;
}

void symbol_table_free(SymbolTable* table) {
    if (!table) return;
    for (int i = 0; i < table->count; i++) {
        free(table->symbols[i].name);
    }
    free(table->symbols);
    free(table);
}

static void ensure_symtab_capacity(SymbolTable* table) {
    if (table->count >= table->capacity) {
        int new_capacity = (table->capacity < 8) ? 8 : table->capacity * 2;
        Symbol* new_symbols = realloc(table->symbols, new_capacity * sizeof(Symbol));
        if (!new_symbols) {
            fprintf(stderr, "Error: SymbolTable reallocation failed.\n");
            exit(EXIT_FAILURE);
        }
        table->symbols = new_symbols;
        table->capacity = new_capacity;
    }
}

int symbol_table_get_or_add(SymbolTable* table, const char* name, bool isFunction) {
    // See if the symbol already exists
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0) {
            // If we want to differentiate variable vs. function, we could check isFunction
            return table->symbols[i].index;
        }
    }
    // Otherwise, create a new symbol
    ensure_symtab_capacity(table);
    int index = table->count;
    table->symbols[index].name = strdup(name);
    table->symbols[index].index = index;      // For simplicity
    table->symbols[index].isFunction = isFunction;
    table->count++;
    return index;
}

/* -------------------------------------------------------
   Utility: Emit Single Byte or Byte + Operand
   ------------------------------------------------------- */
static void emit_byte(BytecodeChunk* chunk, uint8_t byte) {
    vm_chunk_write_byte(chunk, byte);
}
static void emit_two_bytes(BytecodeChunk* chunk, uint8_t b1, uint8_t b2) {
    emit_byte(chunk, b1);
    emit_byte(chunk, b2);
}

static int emit_jump(BytecodeChunk* chunk, uint8_t jumpOp) {
    // Emit the jump opcode plus two bytes for the jump offset (16-bit, big-endian)
    emit_byte(chunk, jumpOp);
    emit_byte(chunk, 0xFF); // placeholder high
    emit_byte(chunk, 0xFF); // placeholder low
    // Return the position of the offset
    return chunk->code_count - 2; 
}

static void patch_jump(BytecodeChunk* chunk, int offset) {
    // Calculate how far to jump from "offset" to the end of the chunk
    int jump_distance = chunk->code_count - offset - 2; 
    // Overwrite the placeholder bytes
    chunk->code[offset]   = (jump_distance >> 8) & 0xFF;
    chunk->code[offset+1] = jump_distance & 0xFF;
}

static int add_constant(BytecodeChunk* chunk, RuntimeValue val) {
    return vm_chunk_add_constant(chunk, val);
}
static void emit_constant(BytecodeChunk* chunk, RuntimeValue val) {
    int index = add_constant(chunk, val);
    emit_byte(chunk, OP_LOAD_CONST);
    emit_byte(chunk, (uint8_t)index);
}

/* -------------------------------------------------------
   Expression Compiler
   ------------------------------------------------------- */
static void compile_expression(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab) {
    switch (node->type) {
        case AST_LITERAL: {
            RuntimeValue cval;
            memset(&cval, 0, sizeof(cval));
            switch (node->literal.token_type) {
                case TOKEN_NUMBER:
                    cval.type = RUNTIME_VALUE_NUMBER;
                    cval.number_value = atof(node->literal.value);
                    break;
                case TOKEN_STRING:
                    cval.type = RUNTIME_VALUE_STRING;
                    cval.string_value = strdup(node->literal.value);
                    break;
                case TOKEN_BOOLEAN:
                    cval.type = RUNTIME_VALUE_BOOLEAN;
                    cval.boolean_value = (strcmp(node->literal.value, "true") == 0);
                    break;
                case TOKEN_NULL:
                    cval.type = RUNTIME_VALUE_NULL;
                    break;
                default:
                    fprintf(stderr, "Compiler error: Unrecognized literal.\n");
                    cval.type = RUNTIME_VALUE_NULL;
            }
            emit_constant(chunk, cval);
            break;
        }
        case AST_VARIABLE: {
            // Load from variable
            int varIndex = symbol_table_get_or_add(symtab, node->variable.variable_name, false);
            emit_byte(chunk, OP_LOAD_VAR);
            // Emit 16-bit variable index (big-endian)
            emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
            emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
            break;
        }
        case AST_ASSIGNMENT: {
            // compile right-hand side
            compile_expression(node->assignment.value, chunk, symtab);
            // store into variable
            int varIndex = symbol_table_get_or_add(symtab, node->assignment.variable, false);
            emit_byte(chunk, OP_STORE_VAR);
            // Emit 16-bit variable index (big-endian)
            emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
            emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
            // The value remains on stack (if you want the assignment to produce a value).
            break;
        }
        case AST_BINARY_OP: {
            // compile left, then right
            compile_expression(node->binary_op.left, chunk, symtab);
            compile_expression(node->binary_op.right, chunk, symtab);
            // pick an opcode
            const char* op = node->binary_op.op_symbol;
            if (strcmp(op, "+") == 0) {
                emit_byte(chunk, OP_ADD);
            } else if (strcmp(op, "-") == 0) {
                emit_byte(chunk, OP_SUB);
            } else if (strcmp(op, "*") == 0) {
                emit_byte(chunk, OP_MUL);
            } else if (strcmp(op, "/") == 0) {
                emit_byte(chunk, OP_DIV);
            } else if (strcmp(op, "==") == 0) {
                emit_byte(chunk, OP_EQ);
            } else if (strcmp(op, "!=") == 0) {
                emit_byte(chunk, OP_NEQ);
            } else if (strcmp(op, "<") == 0) {
                emit_byte(chunk, OP_LT);
            } else if (strcmp(op, ">") == 0) {
                emit_byte(chunk, OP_GT);
            } else if (strcmp(op, "<=") == 0) {
                emit_byte(chunk, OP_LTE);
            } else if (strcmp(op, ">=") == 0) {
                emit_byte(chunk, OP_GTE);
            } else {
                fprintf(stderr, "Compiler error: Unsupported binary operator '%s'\n", op);
            }
            break;
        }
        case AST_FUNCTION_CALL: {
            // Check if this is a built-in function like print()
            if (strcmp(node->function_call.function_name, "print") == 0) {
                // Compile arguments for print
                for (int i = 0; i < node->function_call.argument_count; i++) {
                    compile_expression(node->function_call.arguments[i], chunk, symtab);
                }
                emit_byte(chunk, OP_PRINT);
                // OP_PRINT already pops the value, no need for additional OP_POP
            } else {
                // User-defined function call
                
                // First, check if the function exists in the symbol table
                int funcIndex = -1;
                for (int i = 0; i < symtab->count; i++) {
                    if (symtab->symbols[i].isFunction && 
                        strcmp(symtab->symbols[i].name, node->function_call.function_name) == 0) {
                        funcIndex = symtab->symbols[i].index;
                        break;
                    }
                }
                
                if (funcIndex == -1) {
                    fprintf(stderr, "Error: Undefined function '%s'\n", node->function_call.function_name);
                    return;
                }
                
                // Compile arguments in reverse order (so they're popped in correct order)
                for (int i = node->function_call.argument_count - 1; i >= 0; i--) {
                    compile_expression(node->function_call.arguments[i], chunk, symtab);
                }
                
                // Emit OP_CALL with function index and argument count
                emit_byte(chunk, OP_CALL);
                emit_byte(chunk, (uint8_t)funcIndex);
                emit_byte(chunk, (uint8_t)node->function_call.argument_count);
                
                printf("COMPILER DEBUG: Function call '%s' using funcIndex: %d, argCount: %d\n",
                       node->function_call.function_name, funcIndex, node->function_call.argument_count);
            }
            break;
        }
        case AST_ARRAY_LITERAL: {
            // For minimal array support: create a new array, push elements
            // OP_NEW_ARRAY, then OP_ARRAY_PUSH for each element
            int count = node->array_literal.element_count;
            emit_byte(chunk, OP_NEW_ARRAY);
            // Now stack has a new empty array
            for (int i = 0; i < count; i++) {
                // push the array ref again
                emit_byte(chunk, OP_DUP);
                // compile the element
                compile_expression(node->array_literal.elements[i], chunk, symtab);
                // OP_ARRAY_PUSH
                emit_byte(chunk, OP_ARRAY_PUSH);
            }
            // The resulting array is on the stack top
            break;
        }
        case AST_INDEX_ACCESS: {
            // compile array expr
            compile_expression(node->index_access.array_expr, chunk, symtab);
            // compile index
            compile_expression(node->index_access.index_expr, chunk, symtab);
            // OP_GET_INDEX
            emit_byte(chunk, OP_GET_INDEX);
            break;
        }
        case AST_UNARY_OP: {
            // e.g. !x
            compile_expression(node->unary_op.operand, chunk, symtab);
            if (strcmp(node->unary_op.op_symbol, "!") == 0) {
                emit_byte(chunk, OP_NOT);
            } else if (strcmp(node->unary_op.op_symbol, "-") == 0) {
                emit_byte(chunk, OP_NEG);
            } else {
                fprintf(stderr, "Compiler error: Unknown unary op '%s'\n", node->unary_op.op_symbol);
            }
            break;
        }
        case AST_OBJECT_LITERAL: {
            // Create a new object
            emit_byte(chunk, OP_NEW_OBJECT);

            // First, handle mixins if any
            if (node->object_literal.mixin_count > 0) {
                for (int i = 0; i < node->object_literal.mixin_count; i++) {
                    // Duplicate the object reference on the stack
                    emit_byte(chunk, OP_DUP);
                    
                    // Load the mixin object by name (it should be a variable)
                    int mixinIndex = symbol_table_get_or_add(symtab, node->object_literal.mixins[i], false);
                    emit_byte(chunk, OP_LOAD_VAR);
                    // Emit 16-bit variable index (big-endian)
                    emit_byte(chunk, (uint8_t)((mixinIndex >> 8) & 0xFF));
                    emit_byte(chunk, (uint8_t)(mixinIndex & 0xFF));
                    
                    // Copy all properties from mixin to the new object
                    // This will be handled by a new VM operation: OP_COPY_PROPERTIES
                    emit_byte(chunk, OP_COPY_PROPERTIES);
                }
            }

            // Then, add each regular property to the object
            for (int i = 0; i < node->object_literal.property_count; i++) {
                // Duplicate the object reference on the stack
                emit_byte(chunk, OP_DUP);
                
                // Push property key (as string constant)
                RuntimeValue key_val;
                key_val.type = RUNTIME_VALUE_STRING;
                key_val.string_value = strdup(node->object_literal.keys[i]);
                emit_constant(chunk, key_val);
                
                // Evaluate the property value expression
                compile_expression(node->object_literal.values[i], chunk, symtab);
                
                // Set property on object
                emit_byte(chunk, OP_SET_PROPERTY);
            }
            
            // The resulting object is on the stack top
            break;
        }
        case AST_PROPERTY_ACCESS: {
            // Compile the object expression
            compile_expression(node->property_access.object, chunk, symtab);
            
            // Push the property name (as string constant)
            RuntimeValue prop_val;
            prop_val.type = RUNTIME_VALUE_STRING;
            prop_val.string_value = strdup(node->property_access.property);
            emit_constant(chunk, prop_val);
            
            // Get property from object
            emit_byte(chunk, OP_GET_PROPERTY);
            break;
        }
        case AST_METHOD_CALL: {
            // obj.method(args) => compile obj, args, then OP_METHOD_CALL
            compile_expression(node->method_call.object, chunk, symtab);
            
            // Compile arguments in reverse order
            for (int i = node->method_call.argument_count - 1; i >= 0; i--) {
                compile_expression(node->method_call.arguments[i], chunk, symtab);
            }
            
            // Push method name as string constant
            RuntimeValue method_val;
            method_val.type = RUNTIME_VALUE_STRING;
            method_val.string_value = strdup(node->method_call.method);
            emit_constant(chunk, method_val);
            
            // Call method
            emit_byte(chunk, OP_CALL_METHOD);
            emit_byte(chunk, (uint8_t)node->method_call.argument_count);
            break;
        }
        case AST_PROPERTY_ASSIGNMENT: {
            // obj.prop = value
            // Compile: obj, "prop", value, OP_SET_PROPERTY
            // 1. Compile the object expression
            compile_expression(node->property_assignment.object, chunk, symtab);
            
            // 2. Push the property name (as string constant)
            RuntimeValue prop_val;
            prop_val.type = RUNTIME_VALUE_STRING;
            prop_val.string_value = strdup(node->property_assignment.property);
            emit_constant(chunk, prop_val);
            
            // 3. Compile the value to assign
            compile_expression(node->property_assignment.value, chunk, symtab);
            
            // 4. Use regular property setter for direct assignments
            emit_byte(chunk, OP_SET_PROPERTY);
            break;
        }
        case AST_FUNCTION_DEF: {
            // Function definition in expression context (e.g., in object literal)
            // Create a function object and push it onto the stack
            
            // For now, we'll create a simple function value
            // In a more complete implementation, we'd need to handle closures
            RuntimeValue func_val;
            func_val.type = RUNTIME_VALUE_FUNCTION;
            func_val.function_value.function_type = FUNCTION_TYPE_USER;
            
            // Create a user-defined function structure
            UserDefinedFunction* user_func = malloc(sizeof(UserDefinedFunction));
            if (!user_func) {
                fprintf(stderr, "Compiler error: Memory allocation failed for function\n");
                return;
            }
            
            user_func->name = strdup(node->function_def.function_name);
            user_func->parameter_count = node->function_def.parameter_count;
            user_func->parameters = malloc(sizeof(char*) * user_func->parameter_count);
            
            for (int i = 0; i < user_func->parameter_count; i++) {
                user_func->parameters[i] = strdup(node->function_def.parameters[i]);
            }
            
            user_func->body = node->function_def.body; // Share the AST node
            
            func_val.function_value.user_function = user_func;
            
            emit_constant(chunk, func_val);
            break;
        }
        default:
            // If we see a statement node in an expression context, that's likely a parse mismatch
            fprintf(stderr, "Compiler error: Unexpected node type %d in expression.\n", node->type);
            break;
    }
}

/* -------------------------------------------------------
   Statement Compiler
   ------------------------------------------------------- */
static void compile_statement(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab) {
    switch (node->type) {
        case AST_VARIABLE_DECL: {
            // var X = <expr>;
            if (node->variable_decl.initial_value) {
                compile_expression(node->variable_decl.initial_value, chunk, symtab);
            } else {
                // No initializer => push null
                RuntimeValue cval;
                cval.type = RUNTIME_VALUE_NULL;
                emit_constant(chunk, cval);
            }
            int varIndex = symbol_table_get_or_add(symtab, node->variable_decl.variable_name, false);
            emit_byte(chunk, OP_STORE_VAR);
            // Emit 16-bit variable index (big-endian)
            emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
            emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
            break;
        }
        case AST_ASSIGNMENT:
        case AST_BINARY_OP:
        case AST_FUNCTION_CALL:
        case AST_ARRAY_LITERAL:
        case AST_INDEX_ACCESS:
        case AST_UNARY_OP:
        case AST_LITERAL:
        case AST_VARIABLE:
        case AST_OBJECT_LITERAL:
        case AST_PROPERTY_ACCESS:
        case AST_METHOD_CALL:
        case AST_PROPERTY_ASSIGNMENT: {
            // Expression statement
            compile_expression(node, chunk, symtab);
            // pop result (unless we want to keep it)
            emit_byte(chunk, OP_POP);
            break;
        }
        case AST_IF_STATEMENT: {
            // if (cond) { body } else { elseBody }
            // compile condition
            compile_expression(node->if_statement.condition, chunk, symtab);
            // Jump if false => else part
            int elseJump = emit_jump(chunk, OP_JUMP_IF_FALSE);
            // compile if body
            compile_node(node->if_statement.body, chunk, symtab);
            // jump over else
            int endJump = emit_jump(chunk, OP_JUMP);
            // patch else jump
            patch_jump(chunk, elseJump);
            // compile else if it exists
            if (node->if_statement.else_body) {
                compile_node(node->if_statement.else_body, chunk, symtab);
            }
            // patch end jump
            patch_jump(chunk, endJump);
            break;
        }
        case AST_WHILE_LOOP: {
            // while (cond) { body }
            // label loopStart
            int loopStart = chunk->code_count;
            // compile cond
            compile_expression(node->while_loop.condition, chunk, symtab);
            // jump if false => loopEnd
            int loopEndJump = emit_jump(chunk, OP_JUMP_IF_FALSE);
            // compile body
            compile_node(node->while_loop.body, chunk, symtab);
            // jump back to loopStart
            emit_byte(chunk, OP_LOOP);
            // We store a 2-byte offset for OP_LOOP
            // Distance = current - loopStart + 2 (the size of OP_LOOP itself)
            int offset = chunk->code_count - loopStart + 2;
            // High byte
            emit_byte(chunk, (offset >> 8) & 0xFF);
            // Low byte
            emit_byte(chunk, offset & 0xFF);

            // patch loopEnd
            patch_jump(chunk, loopEndJump);
            break;
        }
       case AST_IMPORT: {
            // The raw import path from the AST node (e.g. "my_script.ember" or "ember/net")
            const char* raw_path = node->import_stmt.import_path;

            // ------------------------------------------------------------------
            // 1) Determine if 'raw_path' ends with ".ember" -> local file import
            // ------------------------------------------------------------------
            bool isLocalFile = false;
            {
                const char* dotEmber = strstr(raw_path, ".ember");
                if (dotEmber != NULL) {
                    // If ".ember" appears at the end of raw_path
                    size_t full_len = strlen(raw_path);
                    size_t dot_pos  = (size_t)(dotEmber - raw_path); 
                    if (dot_pos + 6 == full_len) {
                        // The last 6 chars match ".ember"
                        isLocalFile = true;
                    }
                }
            }

            // ------------------------------------------------------------------
            // 2) Local ".ember" file import
            // ------------------------------------------------------------------
            if (isLocalFile) {
                // Attempt to read and parse the local file
                char* import_source = read_file(raw_path);
                if (!import_source) {
                    fprintf(stderr, 
                            "Compiler error: Could not open local import file '%s'\n", 
                            raw_path);
                    return;
                }

                // Lex & parse => build an AST
                Lexer import_lexer;
                lexer_init(&import_lexer, import_source);
                Parser* import_parser = parser_create(&import_lexer);
                ASTNode* import_root = parse_script(import_parser);

                if (!import_root) {
                    fprintf(stderr, 
                            "Compiler error: Parsing '%s' failed.\n", 
                            raw_path);
                    free(import_parser);
                    free(import_source);
                    return;
                }

                // Compile the newly parsed AST into *this* chunk & symtab
                bool ok = compile_ast(import_root, chunk, symtab);
                if (!ok) {
                    fprintf(stderr, 
                            "Compiler error: Sub-compile for '%s' failed.\n", 
                            raw_path);
                }

                // If the last byte is OP_EOF, remove it to avoid polluting main chunk
                if (chunk->code_count > 0 &&
                    chunk->code[chunk->code_count - 1] == OP_EOF)
                {
                    chunk->code_count--;
                }

                // Cleanup
                free_ast(import_root);
                free(import_parser);
                free(import_source);
            }
            else {
                // ------------------------------------------------------------------
                // 3) Module import (e.g., "ember/net", "raylib/2d", etc.)
                // ------------------------------------------------------------------
                // (A) Optionally check if installed using utils_is_package_installed
                // (B) If not installed -> compiler error or warning
                // (C) If installed or ignoring checks -> do nothing (no file compilation),
                //     since the module is presumably built-in or available at runtime.

                // Example logic:
                // If you skip the check, just log:
                // fprintf(stdout, "Importing module '%s' (no .ember extension)\n", raw_path);

                // But let's do a minimal installed check:
                if (!utils_is_package_installed(raw_path)) {
                    // Not installed => treat as error
                    fprintf(stderr, 
                            "Compiler error: Module '%s' is not installed (no local .ember found).\n",
                            raw_path);
                }
                else {
                    // Successfully found in local registry, so skip file compilation.
                    fprintf(stdout, 
                            "[Import] Found installed module '%s'. No local file to compile.\n",
                            raw_path);
                }
            }

            // No additional code emitted for the import statement itself
            break;
        }
        case AST_FOR_LOOP: {
            // for (init; cond; inc) { body }
            // compile init
            if (node->for_loop.initializer) {
                compile_node(node->for_loop.initializer, chunk, symtab);
            }
            // label loopStart
            int loopStart = chunk->code_count;
            // compile cond
            if (node->for_loop.condition) {
                compile_expression(node->for_loop.condition, chunk, symtab);
            } else {
                // If no condition, we treat it as 'true'
                RuntimeValue cval;
                cval.type = RUNTIME_VALUE_BOOLEAN;
                cval.boolean_value = true;
                emit_constant(chunk, cval);
            }
            // jump if false => loopEnd
            int loopEndJump = emit_jump(chunk, OP_JUMP_IF_FALSE);

            // compile body
            compile_node(node->for_loop.body, chunk, symtab);

            // compile inc
            if (node->for_loop.increment) {
                compile_expression(node->for_loop.increment, chunk, symtab);
                emit_byte(chunk, OP_POP); // discard inc result
            }
            // jump back to loopStart
            emit_byte(chunk, OP_LOOP);
            int offset = chunk->code_count - loopStart + 2;
            emit_byte(chunk, (offset >> 8) & 0xFF);
            emit_byte(chunk, offset & 0xFF);

            // patch loopEnd
            patch_jump(chunk, loopEndJump);
            break;
        }
        case AST_FUNCTION_DEF: {
            // Emit a jump to skip the function body during normal execution
            emit_byte(chunk, OP_JUMP);
            int jump_addr = chunk->code_count;
            emit_byte(chunk, 0); // Placeholder for jump distance high byte
            emit_byte(chunk, 0); // Placeholder for jump distance low byte

            // Record the function start IP in the constants table
            int function_start_ip = chunk->code_count;
            RuntimeValue func_ip_value;
            func_ip_value.type = RUNTIME_VALUE_NUMBER;
            func_ip_value.number_value = (double)function_start_ip;
            int func_const_index = add_constant(chunk, func_ip_value);
            
            // Store function metadata in symbol table
            int func_symbol_index = symbol_table_get_or_add(symtab, node->function_def.function_name, true);
            
            // Update the symbol table entry to store the constant index instead of symbol index
            for (int i = 0; i < symtab->count; i++) {
                if (symtab->symbols[i].isFunction && 
                    strcmp(symtab->symbols[i].name, node->function_def.function_name) == 0) {
                    symtab->symbols[i].index = func_const_index; // Store constant index, not symbol index
                    break;
                }
            }

            // Map parameters to local variable indices (starting from 256)
            for (int i = 0; i < node->function_def.parameter_count; i++) {
                const char* paramName = node->function_def.parameters[i];
                
                // Check if this parameter name already exists in the symbol table
                int existingIndex = -1;
                for (int j = 0; j < symtab->count; j++) {
                    if (strcmp(symtab->symbols[j].name, paramName) == 0) {
                        existingIndex = j;
                        break;
                    }
                }
                
                if (existingIndex >= 0) {
                    // Parameter name conflicts with existing symbol
                    // Override the existing symbol's index to point to local variable slot
                    symtab->symbols[existingIndex].index = 256 + i;
                } else {
                    // Add new parameter to symbol table with local index
                    ensure_symtab_capacity(symtab);
                    int paramIndex = symtab->count;
                    symtab->symbols[paramIndex].name = strdup(paramName);
                    symtab->symbols[paramIndex].index = 256 + i; // Local variable slot
                    symtab->symbols[paramIndex].isFunction = false;
                    symtab->count++;
                }
            }

            // Compile the function body
            compile_statement(node->function_def.body, chunk, symtab);

            // Emit return at the end of function
            emit_byte(chunk, OP_RETURN);

            // Patch the jump to skip the function body
            int jump_distance = chunk->code_count - jump_addr - 2; // -2 because we have 2 bytes for the offset
            chunk->code[jump_addr] = (uint8_t)((jump_distance >> 8) & 0xFF); // High byte
            chunk->code[jump_addr + 1] = (uint8_t)(jump_distance & 0xFF);    // Low byte

            break;
        }
        case AST_BLOCK: {
            // compile each statement
            for (int i = 0; i < node->block.statement_count; i++) {
                compile_node(node->block.statements[i], chunk, symtab);
            }
            break;
        }
        case AST_SWITCH_CASE: {
            // (Placeholder) We have not implemented codegen for switch statements yet.
            // For now, do nothing or produce a warning.
            // e.g.,
            fprintf(stderr, "Warning: Switch/case code generation not implemented.\n");
            break;
        }
        default:
            fprintf(stderr, "Compiler error: Unhandled statement node type %d\n", node->type);
            break;
    }
}

/* -------------------------------------------------------
   Recursively compile an AST node
   ------------------------------------------------------- */
static void compile_node(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab) {
    if (!node) return;

    switch (node->type) {
        // Anything that can appear as a top-level statement
        case AST_VARIABLE_DECL:
        case AST_ASSIGNMENT:
        case AST_FUNCTION_CALL:
        case AST_IF_STATEMENT:
        case AST_WHILE_LOOP:
        case AST_FOR_LOOP:
        case AST_FUNCTION_DEF:
        case AST_BLOCK:
        case AST_BINARY_OP:
        case AST_ARRAY_LITERAL:
        case AST_INDEX_ACCESS:
        case AST_UNARY_OP:
        case AST_LITERAL:
        case AST_VARIABLE:
        case AST_IMPORT:
        case AST_SWITCH_CASE:
        case AST_OBJECT_LITERAL:
        case AST_PROPERTY_ACCESS:
        case AST_METHOD_CALL:
        case AST_PROPERTY_ASSIGNMENT:
            compile_statement(node, chunk, symtab);
            break;

        default:
            fprintf(stderr, "Compiler error: compile_node unrecognized AST type %d.\n", node->type);
            break;
    }
}

/* -------------------------------------------------------
   Main Entry Point
   ------------------------------------------------------- */
bool compile_ast(ASTNode* ast, BytecodeChunk* chunk, SymbolTable* symtab) {
    if (!ast || !chunk || !symtab) {
        fprintf(stderr, "Error: compile_ast called with invalid arguments.\n");
        return false;
    }

    compile_node(ast, chunk, symtab);

    // Finally, emit an OP_EOF or OP_RETURN to cleanly end
    emit_byte(chunk, OP_EOF);
    return true;
}
