// compiler.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "virtual_machine.h"
#include "parser.h"  // For ASTNodeType, ASTNode, etc.
#include "utils.h"

// Forward declarations
static void compile_expression(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);
static void compile_statement(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);
static void compile_node(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);
static void compile_function_body(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);
static void compile_if_statement_with_return(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab);

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
    table->symbols[index].is_mutable = true;  // Default to mutable for backward compatibility
    table->count++;
    return index;
}

int symbol_table_get_or_add_variable(SymbolTable* table, const char* name, bool is_mutable) {
    // See if the symbol already exists
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0) {
            if (table->symbols[i].isFunction) {
                fprintf(stderr, "Error: '%s' is already defined as a function\n", name);
                return -1;
            }
            // Variable already exists - this could be an error in strict mode
            fprintf(stderr, "Error: Variable '%s' is already declared\n", name);
            return -1;
        }
    }
    // Otherwise, create a new variable symbol
    ensure_symtab_capacity(table);
    int index = table->count;
    table->symbols[index].name = strdup(name);
    table->symbols[index].index = index;
    table->symbols[index].isFunction = false;
    table->symbols[index].is_mutable = is_mutable;
    table->count++;
    return index;
}

bool symbol_table_is_variable_mutable(SymbolTable* table, const char* name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0) {
            if (table->symbols[i].isFunction) {
                return false; // Functions are not mutable variables
            }
            return table->symbols[i].is_mutable;
        }
    }
    return false; // Variable not found
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
   Special function body compiler that preserves last expression
   ------------------------------------------------------- */
static void compile_function_body(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab) {
    if (!node) return;
    
    if (node->type == AST_BLOCK) {
        // For function bodies, compile all statements except the last one normally
        // The last statement should be treated as a return value
        int statement_count = node->block.statement_count;
        
        for (int i = 0; i < statement_count; i++) {
            ASTNode* statement = node->block.statements[i];
            
            if (i == statement_count - 1) {
                // Last statement - treat as expression that should remain on stack
                switch (statement->type) {
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
                    case AST_RANGE:
                        // These are expressions - compile them without OP_POP
                        compile_expression(statement, chunk, symtab);
                        break;
                    case AST_IF_STATEMENT:
                        // Special handling for if statements as return values
                        compile_if_statement_with_return(statement, chunk, symtab);
                        break;
                    default:
                        // Regular statements - compile normally and push null as return value
                        compile_statement(statement, chunk, symtab);
                        // Push null as return value since this statement doesn't produce a value
                        RuntimeValue null_val;
                        null_val.type = RUNTIME_VALUE_NULL;
                        emit_constant(chunk, null_val);
                        break;
                }
            } else {
                // Not the last statement - compile normally
                compile_node(statement, chunk, symtab);
            }
        }
        
        if (statement_count == 0) {
            // Empty function body - return null
            RuntimeValue null_val;
            null_val.type = RUNTIME_VALUE_NULL;
            emit_constant(chunk, null_val);
        }
    } else {
        // Single statement function body
        switch (node->type) {
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
            case AST_RANGE:
                // Expression - compile without OP_POP
                compile_expression(node, chunk, symtab);
                break;
            case AST_IF_STATEMENT:
                // Special handling for if statements as return values
                compile_if_statement_with_return(node, chunk, symtab);
                break;
            default:
                // Statement - compile normally and push null
                compile_statement(node, chunk, symtab);
                RuntimeValue null_val;
                null_val.type = RUNTIME_VALUE_NULL;
                emit_constant(chunk, null_val);
                break;
        }
    }
}

/* -------------------------------------------------------
   Special if statement compiler that preserves result on stack for function returns
   ------------------------------------------------------- */
static void compile_if_statement_with_return(ASTNode* node, BytecodeChunk* chunk, SymbolTable* symtab) {
    if (!node || node->type != AST_IF_STATEMENT) {
        fprintf(stderr, "Compiler error: compile_if_statement_with_return called with non-if node\n");
        return;
    }

    // if (cond) { body } else { elseBody } - but preserve result values on stack
    // compile condition
    compile_expression(node->if_statement.condition, chunk, symtab);
    
    // Jump if false => else part
    int elseJump = emit_jump(chunk, OP_JUMP_IF_FALSE);
    
    // compile if body - check if it's a single expression or block
    if (node->if_statement.body->type == AST_BLOCK) {
        // For blocks, we need to ensure the last statement produces a value
        compile_function_body(node->if_statement.body, chunk, symtab);
    } else {
        // Single statement - compile as expression if possible
        compile_expression(node->if_statement.body, chunk, symtab);
    }
    
    // jump over else
    int endJump = emit_jump(chunk, OP_JUMP);
    
    // patch else jump
    patch_jump(chunk, elseJump);
    
    // compile else if it exists
    if (node->if_statement.else_body) {
        if (node->if_statement.else_body->type == AST_BLOCK) {
            compile_function_body(node->if_statement.else_body, chunk, symtab);
        } else {
            compile_expression(node->if_statement.else_body, chunk, symtab);
        }
    } else {
        // No else branch - push null as default value
        RuntimeValue null_val;
        null_val.type = RUNTIME_VALUE_NULL;
        emit_constant(chunk, null_val);
    }
    
    // patch end jump
    patch_jump(chunk, endJump);
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
            // Check if the variable is mutable before allowing assignment
            if (!symbol_table_is_variable_mutable(symtab, node->assignment.variable)) {
                fprintf(stderr, "Compile Error: Cannot assign to immutable variable '%s'\n", 
                        node->assignment.variable);
                return;
            }
            
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
            } else if (strcmp(op, "&&") == 0) {
                emit_byte(chunk, OP_AND);
            } else if (strcmp(op, "||") == 0) {
                emit_byte(chunk, OP_OR);
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
                // No duplication needed! OP_ARRAY_PUSH pops the array and value,
                // then pushes the updated array back, so the stack management is automatic
                
                // compile the element
                compile_expression(node->array_literal.elements[i], chunk, symtab);
                // OP_ARRAY_PUSH pops [array, element] and pushes [updated_array]
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

            // Handle properties with proper stack management
            // OP_SET_PROPERTY expects: [object, propName, value] and produces: [updatedObject]
            for (int i = 0; i < node->object_literal.property_count; i++) {
                // For all properties, we'll duplicate the object first, then add property
                // This ensures the object reference is always available for the next property
                emit_byte(chunk, OP_DUP);
                
                // Push property key (as string constant)
                RuntimeValue key_val;
                key_val.type = RUNTIME_VALUE_STRING;
                key_val.string_value = strdup(node->object_literal.keys[i]);
                emit_constant(chunk, key_val);
                
                // Evaluate the property value expression
                compile_expression(node->object_literal.values[i], chunk, symtab);
                
                // Set property on object: stack is [obj, obj, key, value] -> [obj, updatedObj]
                emit_byte(chunk, OP_SET_PROPERTY);
                
                // Now we have [originalObj, updatedObj] on stack
                // We need to remove the original and keep the updated one
                // Use OP_SWAP and OP_POP to clean up
                emit_byte(chunk, OP_SWAP);  // [updatedObj, originalObj]
                emit_byte(chunk, OP_POP);   // [updatedObj]
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
            // obj.method(args) => compile obj, duplicate it, get method property, compile args, then OP_CALL_METHOD
            
            // 1. Compile the object
            compile_expression(node->method_call.object, chunk, symtab);
            
            // 2. Duplicate the object (one copy for 'this', one for property access)
            emit_byte(chunk, OP_DUP);
            
            // 3. Get the method property from the object (consumes one copy of the object)
            RuntimeValue method_name_val;
            method_name_val.type = RUNTIME_VALUE_STRING;
            method_name_val.string_value = strdup(node->method_call.method);
            emit_constant(chunk, method_name_val);
            emit_byte(chunk, OP_GET_PROPERTY);
            
            // 4. Compile arguments
            for (int i = 0; i < node->method_call.argument_count; i++) {
                compile_expression(node->method_call.arguments[i], chunk, symtab);
            }
            
            // Stack is now: [object, method, arg1, arg2, ..., argN]
            // 5. Call method with the object as 'this'
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
        case AST_IF_STATEMENT: {
            // If statement as expression - use the special return-preserving compiler
            compile_if_statement_with_return(node, chunk, symtab);
            break;
        }
        case AST_RANGE: {
            // Range expression: start..end  
            // Create it exactly like a two-property object literal
            
            // Create a new object to represent the range
            emit_byte(chunk, OP_NEW_OBJECT);
            
            // Set the 'start' property
            emit_byte(chunk, OP_DUP);
            RuntimeValue start_prop;
            start_prop.type = RUNTIME_VALUE_STRING;
            start_prop.string_value = strdup("start");
            emit_constant(chunk, start_prop);
            compile_expression(node->range.start, chunk, symtab);
            emit_byte(chunk, OP_SET_PROPERTY);
            emit_byte(chunk, OP_SWAP);
            emit_byte(chunk, OP_POP);
            
            // Set the 'end' property
            emit_byte(chunk, OP_DUP);
            RuntimeValue end_prop;
            end_prop.type = RUNTIME_VALUE_STRING;
            end_prop.string_value = strdup("end");
            emit_constant(chunk, end_prop);
            compile_expression(node->range.end, chunk, symtab);
            emit_byte(chunk, OP_SET_PROPERTY);
            emit_byte(chunk, OP_SWAP);
            emit_byte(chunk, OP_POP);
            
            // The resulting range object is on the stack top
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
            // Handle both old syntax (var x = value) and new colon syntax (var x: value, let x: value, x: value)
            if (node->variable_decl.initial_value) {
                compile_expression(node->variable_decl.initial_value, chunk, symtab);
            } else {
                // No initializer => push null
                RuntimeValue cval;
                cval.type = RUNTIME_VALUE_NULL;
                emit_constant(chunk, cval);
            }
            
            // Use the new mutability-aware symbol table function
            int varIndex = symbol_table_get_or_add_variable(symtab, 
                                                           node->variable_decl.variable_name, 
                                                           node->variable_decl.is_mutable);
            if (varIndex == -1) {
                // Error already reported by symbol_table_get_or_add_variable
                return;
            }
            
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
        case AST_PROPERTY_ASSIGNMENT:
        case AST_RANGE: {
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
        case AST_NAKED_ITERATOR: {
            // Naked iterator: var: iterable (body)
            // Support different types:
            // - Range: var: start..end
            // - Array: var: array_var  
            // - Variable: var: collection_var
            
            ASTNode* iterable = node->naked_iterator.iterable;
            char* var_name = node->naked_iterator.variable_name;
            
            if (iterable->type == AST_RANGE) {
                // Range iteration: var = start; while (var <= end) { body; var++; }
                
                // Initialize the iterator variable to the start value
                compile_expression(iterable->range.start, chunk, symtab);
                int varIndex = symbol_table_get_or_add(symtab, var_name, false);
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
                
                // Loop start label
                int loopStart = chunk->code_count;
                
                // Loop condition: var <= end
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
                
                compile_expression(iterable->range.end, chunk, symtab);
                emit_byte(chunk, OP_LTE);
                
                int loopEndJump = emit_jump(chunk, OP_JUMP_IF_FALSE);
                
                // Compile loop body
                compile_node(node->naked_iterator.body, chunk, symtab);
                
                // Increment: var = var + 1
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
                
                RuntimeValue one;
                one.type = RUNTIME_VALUE_NUMBER;
                one.number_value = 1.0;
                emit_constant(chunk, one);
                
                emit_byte(chunk, OP_ADD);
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
                
                // Jump back to loop start
                emit_byte(chunk, OP_LOOP);
                int offset = chunk->code_count - loopStart + 2;
                emit_byte(chunk, (offset >> 8) & 0xFF);
                emit_byte(chunk, offset & 0xFF);
                
                patch_jump(chunk, loopEndJump);
                
            } else if (iterable->type == AST_VARIABLE || 
                       iterable->type == AST_ARRAY_LITERAL ||
                       iterable->type == AST_PROPERTY_ACCESS) {
                // Collection iteration: Need to handle both arrays and objects
                // Strategy: Get keys/indices and iterate over them
                
                // Create a temporary index variable
                char index_var_name[64];
                snprintf(index_var_name, sizeof(index_var_name), "__iter_index_%p", (void*)node);
                int indexVarIndex = symbol_table_get_or_add(symtab, index_var_name, false);
                
                // Create a temporary variable to store the keys/indices array
                char keys_var_name[64];
                snprintf(keys_var_name, sizeof(keys_var_name), "__iter_keys_%p", (void*)node);
                int keysVarIndex = symbol_table_get_or_add(symtab, keys_var_name, false);
                
                // Create a temporary variable to store the original collection (for array value access)
                char collection_var_name[64];
                snprintf(collection_var_name, sizeof(collection_var_name), "__iter_collection_%p", (void*)node);
                int collectionVarIndex = symbol_table_get_or_add(symtab, collection_var_name, false);
                
                // Load the collection and get its keys/indices
                compile_expression(iterable, chunk, symtab);
                emit_byte(chunk, OP_DUP); // Duplicate for storage
                
                // Store original collection
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((collectionVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(collectionVarIndex & 0xFF));
                
                // Get keys/indices
                emit_byte(chunk, OP_GET_KEYS);
                
                // Store the keys/indices array
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((keysVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(keysVarIndex & 0xFF));
                
                // Initialize index to 0
                RuntimeValue zero;
                zero.type = RUNTIME_VALUE_NUMBER;
                zero.number_value = 0.0;
                emit_constant(chunk, zero);
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((indexVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(indexVarIndex & 0xFF));
                
                // Loop start label
                int loopStart = chunk->code_count;
                
                // Loop condition: index < keys_array.length
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((indexVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(indexVarIndex & 0xFF));
                
                // Get length of keys array
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((keysVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(keysVarIndex & 0xFF));
                emit_byte(chunk, OP_GET_LENGTH);
                
                emit_byte(chunk, OP_LT);
                int loopEndJump = emit_jump(chunk, OP_JUMP_IF_FALSE);
                
                // Get the current key/index: key = keys[index]
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((keysVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(keysVarIndex & 0xFF));
                
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((indexVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(indexVarIndex & 0xFF));
                
                emit_byte(chunk, OP_GET_INDEX); // key = keys[index]
                
                // Now decide what to put in the iterator variable
                // We'll use a unified approach: always get collection[key] 
                // For arrays: key is numeric index, collection[key] gives us the value
                // For objects: key is string property name, collection[key] gives us the value
                // But the EmberScript semantics are:
                // - Arrays: iterate over values
                // - Objects: iterate over keys
                // So we need different behavior...
                
                // Let me implement this properly with runtime detection:
                // For now, let's assume arrays if AST_ARRAY_LITERAL or if we detect numeric keys
                
                // Duplicate the key to check its type and for potential use
                emit_byte(chunk, OP_DUP);
                
                // Load the original collection
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((collectionVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(collectionVarIndex & 0xFF));
                
                // Stack is now: [key, key, collection]
                // Swap to get [key, collection, key] for OP_GET_INDEX
                emit_byte(chunk, OP_SWAP); // [key, collection, key]
                
                // Get collection[key]
                emit_byte(chunk, OP_GET_INDEX); // [key, collection_value]
                
                // Now we have [original_key, collection_value]
                // For arrays: we want collection_value
                // For objects: we want original_key
                // 
                // The issue is we need to distinguish at compile time or runtime
                // Let's use this approach: if iterable is AST_ARRAY_LITERAL, use value, otherwise use key
                
                if (iterable->type == AST_ARRAY_LITERAL) {
                    // Arrays: use the collection_value, discard the key
                    emit_byte(chunk, OP_SWAP); // [collection_value, key]
                    emit_byte(chunk, OP_POP);  // [collection_value]
                } else {
                    // Variables and other types: assume objects by default
                    // Objects: use the key, discard the collection_value  
                    emit_byte(chunk, OP_POP);  // [key]
                }
                
                // Store in iterator variable (whatever is on top of stack now)
                int varIndex = symbol_table_get_or_add(symtab, var_name, false);
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((varIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(varIndex & 0xFF));
                
                // Compile loop body
                compile_node(node->naked_iterator.body, chunk, symtab);
                
                // Increment index: index = index + 1
                emit_byte(chunk, OP_LOAD_VAR);
                emit_byte(chunk, (uint8_t)((indexVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(indexVarIndex & 0xFF));
                
                RuntimeValue one;
                one.type = RUNTIME_VALUE_NUMBER;
                one.number_value = 1.0;
                emit_constant(chunk, one);
                
                emit_byte(chunk, OP_ADD);
                emit_byte(chunk, OP_STORE_VAR);
                emit_byte(chunk, (uint8_t)((indexVarIndex >> 8) & 0xFF));
                emit_byte(chunk, (uint8_t)(indexVarIndex & 0xFF));
                
                // Jump back to loop start
                emit_byte(chunk, OP_LOOP);
                int offset = chunk->code_count - loopStart + 2;
                emit_byte(chunk, (offset >> 8) & 0xFF);
                emit_byte(chunk, offset & 0xFF);
                
                patch_jump(chunk, loopEndJump);
                
            } else {
                fprintf(stderr, "Compiler error: Unsupported iterable type %d in naked iterator\n", iterable->type);
            }
            
            break;
        }
        case AST_BLOCK: {
            // compile each statement in the block
            for (int i = 0; i < node->block.statement_count; i++) {
                compile_node(node->block.statements[i], chunk, symtab);
            }
            break;
        }
        default:
            fprintf(stderr, "Compiler error: Unexpected node type %d in statement.\n", node->type);
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
        case AST_NAKED_ITERATOR:
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
        case AST_RANGE:
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
