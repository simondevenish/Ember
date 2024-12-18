#include "runtime.h"

#include <stdio.h>      // For input/output functions (e.g., fprintf)
#include <stdlib.h>     // For memory allocation (e.g., malloc, free)
#include <string.h>     // For string manipulation (e.g., strcpy, strcmp)
#include <stdbool.h>    // For boolean data type
#ifdef _WIN32
 #include <windows.h>
 // Define pthread_t as a HANDLE on Windows
typedef HANDLE pthread_t;
#else
#include <pthread.h>    // Only include pthread.h if not on Windows
#endif
#include <ctype.h>
#include <math.h>

Environment* runtime_create_environment() {
    // Allocate memory for the environment
    Environment* env = (Environment*)malloc(sizeof(Environment));
    if (!env) {
        fprintf(stderr, "Error: Memory allocation failed for global environment.\n");
        return NULL;
    }

    // Initialize environment fields
    env->variable_name = NULL;
    env->value.type = RUNTIME_VALUE_NULL;
    env->value.string_value = NULL;
    env->next = NULL;
    env->parent = NULL;

    return env;
}

// Function to create a deep copy of a RuntimeValue
RuntimeValue runtime_value_copy(const RuntimeValue* value) {
    RuntimeValue copy = *value; // Shallow copy of the struct

    switch (value->type) {
        case RUNTIME_VALUE_STRING:
            if (value->string_value) {
                copy.string_value = strdup(value->string_value);
            }
            break;
        case RUNTIME_VALUE_FUNCTION:
            // For user-defined functions, we assume the function definition is shared
            // If you need to deep copy functions, implement it here
            break;
        default:
            // Other types (number, boolean, null) don't require special handling
            break;
    }

    return copy;
}

Environment* runtime_create_child_environment(Environment* parent) {
    // Allocate memory for the new child environment
    Environment* child_env = (Environment*)malloc(sizeof(Environment));
    if (!child_env) {
        fprintf(stderr, "Error: Memory allocation failed for child environment.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the child environment
    child_env->variable_name = NULL;
    child_env->value = (RuntimeValue){.type = RUNTIME_VALUE_NULL};
    child_env->next = NULL;
    child_env->parent = parent;

    return child_env;
}

void runtime_set_variable(Environment* env, const char* name, RuntimeValue value) {
    // Search for the variable in the current environment or parent environments
    Environment* current_env = env;
    while (current_env) {
        Environment* var = current_env->next;
        while (var) {
            if (var->variable_name && strcmp(var->variable_name, name) == 0) {
                // Variable exists; update its value
                runtime_free_value(&var->value);
                var->value = runtime_value_copy(&value);
                return;
            }
            var = var->next;
        }
        current_env = current_env->parent;
    }

    // Variable does not exist in the current environment; create it
    Environment* new_var = (Environment*)malloc(sizeof(Environment));
    if (!new_var) {
        fprintf(stderr, "Error: Memory allocation failed for new variable.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the new variable
    new_var->variable_name = strdup(name);
    if (!new_var->variable_name) {
        fprintf(stderr, "Error: Memory allocation failed for variable name.\n");
        exit(EXIT_FAILURE);
    }

    new_var->value = runtime_value_copy(&value);
    new_var->next = env->next;
    new_var->parent = NULL;

    // Add the new variable to the current environment's linked list
    env->next = new_var;
}

RuntimeValue* runtime_get_variable(Environment* env, const char* name) {
    Environment* current_env = env;

    while (current_env) {
        Environment* var = current_env->next;
        while (var) {
            if (var->variable_name && strcmp(var->variable_name, name) == 0) {
                return &var->value;
            }
            var = var->next;
        }
        current_env = current_env->parent;
    }

    return NULL;
}

RuntimeValue runtime_evaluate(Environment* env, ASTNode* node) {
    RuntimeValue result;
    result.type = RUNTIME_VALUE_NULL;

    if (!node) {
        fprintf(stderr, "Error: Attempted to evaluate a NULL AST node.\n");
        return result;
    }

    switch (node->type) {
        case AST_LITERAL: {
            switch (node->literal.token_type) {
                case TOKEN_NUMBER:
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = atof(node->literal.value);
                    break;
                case TOKEN_STRING:
                    result.type = RUNTIME_VALUE_STRING;
                    result.string_value = strdup(node->literal.value);
                    break;
                case TOKEN_BOOLEAN:
                    result.type = RUNTIME_VALUE_BOOLEAN;
                    result.boolean_value = (strcmp(node->literal.value, "true") == 0);
                    break;
                case TOKEN_NULL:
                    result.type = RUNTIME_VALUE_NULL;
                    break;
                default:
                    fprintf(stderr, "Error: Unknown literal type.\n");
                    break;
            }
            break;
        }
        case AST_ASSIGNMENT: {
            RuntimeValue value = runtime_evaluate(env, node->assignment.value);
            runtime_set_variable(env, node->assignment.variable, value);
            result = value;
            break;
        }
        case AST_VARIABLE_DECL: {
            RuntimeValue value = { .type = RUNTIME_VALUE_NULL };
            if (node->variable_decl.initial_value) {
                value = runtime_evaluate(env, node->variable_decl.initial_value);
            }
            runtime_set_variable(env, node->variable_decl.variable_name, value);
            result = value;
            break;
        }
        case AST_BLOCK:
            runtime_execute_block(env, node);
            result.type = RUNTIME_VALUE_NULL;
        break;
        case AST_BINARY_OP: {
            RuntimeValue left = runtime_evaluate(env, node->binary_op.left);
            RuntimeValue right = runtime_evaluate(env, node->binary_op.right);
            char* op = node->binary_op.op_symbol;

            if (strcmp(op, "+") == 0) {
                // Handle addition or string concatenation
                if (left.type == RUNTIME_VALUE_NUMBER && right.type == RUNTIME_VALUE_NUMBER) {
                    // Numeric addition
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = left.number_value + right.number_value;
                } else {
                    // String concatenation or mixed types
                    char* left_str = runtime_value_to_string(&left);
                    char* right_str = runtime_value_to_string(&right);
                    size_t total_length = strlen(left_str) + strlen(right_str) + 1;
                    char* concatenated = (char*)malloc(total_length);
                    if (!concatenated) {
                        fprintf(stderr, "Error: Memory allocation failed for string concatenation.\n");
                        free(left_str);
                        free(right_str);
                        result.type = RUNTIME_VALUE_NULL;
                        break;
                    }
                    strcpy(concatenated, left_str);
                    strcat(concatenated, right_str);

                    result.type = RUNTIME_VALUE_STRING;
                    result.string_value = concatenated;

                    free(left_str);
                    free(right_str);
                }
            } else if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
                // Numeric operations
                if (left.type == RUNTIME_VALUE_NUMBER && right.type == RUNTIME_VALUE_NUMBER) {
                    result.type = RUNTIME_VALUE_NUMBER;
                    if (strcmp(op, "-") == 0) {
                        result.number_value = left.number_value - right.number_value;
                    } else if (strcmp(op, "*") == 0) {
                        result.number_value = left.number_value * right.number_value;
                    } else if (strcmp(op, "/") == 0) {
                        if (right.number_value == 0) {
                            fprintf(stderr, "Error: Division by zero.\n");
                            result.type = RUNTIME_VALUE_NULL;
                        } else {
                            result.number_value = left.number_value / right.number_value;
                        }
                    } else if (strcmp(op, "%") == 0) {
                        result.number_value = fmod(left.number_value, right.number_value);
                    }
                } else {
                    fprintf(stderr, "Error: Operator '%s' requires numeric operands.\n", op);
                    result.type = RUNTIME_VALUE_NULL;
                }
            } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                // Equality comparison
                result.type = RUNTIME_VALUE_BOOLEAN;

                if (left.type == right.type) {
                    if (left.type == RUNTIME_VALUE_NUMBER) {
                        result.boolean_value = (left.number_value == right.number_value);
                    } else if (left.type == RUNTIME_VALUE_BOOLEAN) {
                        result.boolean_value = (left.boolean_value == right.boolean_value);
                    } else if (left.type == RUNTIME_VALUE_STRING) {
                        result.boolean_value = (strcmp(left.string_value, right.string_value) == 0);
                    } else if (left.type == RUNTIME_VALUE_NULL) {
                        result.boolean_value = true; // Both are null
                    } else {
                        result.boolean_value = false;
                    }
                } else {
                    // Different types
                    result.boolean_value = false;
                }

                if (strcmp(op, "!=") == 0) {
                    result.boolean_value = !result.boolean_value;
                }
            } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                    strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                // Comparison operations
                if (left.type == RUNTIME_VALUE_NUMBER && right.type == RUNTIME_VALUE_NUMBER) {
                    result.type = RUNTIME_VALUE_BOOLEAN;
                    if (strcmp(op, "<") == 0) {
                        result.boolean_value = left.number_value < right.number_value;
                    } else if (strcmp(op, ">") == 0) {
                        result.boolean_value = left.number_value > right.number_value;
                    } else if (strcmp(op, "<=") == 0) {
                        result.boolean_value = left.number_value <= right.number_value;
                    } else if (strcmp(op, ">=") == 0) {
                        result.boolean_value = left.number_value >= right.number_value;
                    }
                } else {
                    fprintf(stderr, "Error: Operator '%s' requires numeric operands.\n", op);
                    result.type = RUNTIME_VALUE_NULL;
                }
            } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                // Logical operations
                if (left.type == RUNTIME_VALUE_BOOLEAN && right.type == RUNTIME_VALUE_BOOLEAN) {
                    result.type = RUNTIME_VALUE_BOOLEAN;
                    if (strcmp(op, "&&") == 0) {
                        result.boolean_value = left.boolean_value && right.boolean_value;
                    } else if (strcmp(op, "||") == 0) {
                        result.boolean_value = left.boolean_value || right.boolean_value;
                    }
                } else {
                    fprintf(stderr, "Error: Operator '%s' requires boolean operands.\n", op);
                    result.type = RUNTIME_VALUE_NULL;
                }
            } else {
                fprintf(stderr, "Error: Unknown binary operator '%s'.\n", op);
                result.type = RUNTIME_VALUE_NULL;
            }
            break;
        }
        case AST_FUNCTION_DEF: {
            // Create a UserDefinedFunction structure
            UserDefinedFunction* user_function = (UserDefinedFunction*)malloc(sizeof(UserDefinedFunction));
            if (!user_function) {
                fprintf(stderr, "Error: Memory allocation failed for UserDefinedFunction.\n");
                exit(EXIT_FAILURE);
            }

            user_function->name = strdup(node->function_def.function_name);
            user_function->parameter_count = node->function_def.parameter_count;
            user_function->parameters = (char**)malloc(sizeof(char*) * user_function->parameter_count);
            for (int i = 0; i < user_function->parameter_count; i++) {
                user_function->parameters[i] = strdup(node->function_def.parameters[i]);
            }
            user_function->body = node->function_def.body;

            // Create a RuntimeValue to store the function
            RuntimeValue function_value;
            function_value.type = RUNTIME_VALUE_FUNCTION;
            function_value.function_value.function_type = FUNCTION_TYPE_USER;
            function_value.function_value.user_function = user_function;

            // Register the function in the environment
            runtime_set_variable(env, user_function->name, function_value);

            // The result is null
            result.type = RUNTIME_VALUE_NULL;
            break;
        }
        case AST_FUNCTION_CALL: {
            result = runtime_execute_function_call(env, node);
            break;
        }
        case AST_UNARY_OP: {
            RuntimeValue operand = runtime_evaluate(env, node->unary_op.operand);
            if (strcmp(node->unary_op.op_symbol, "!") == 0) {
                if (operand.type == RUNTIME_VALUE_BOOLEAN) {
                    result.type = RUNTIME_VALUE_BOOLEAN;
                    result.boolean_value = !operand.boolean_value;
                } else {
                    fprintf(stderr, "Error: '!' operator requires a boolean operand.\n");
                    result.type = RUNTIME_VALUE_NULL;
                }
            } else {
                fprintf(stderr, "Error: Unknown unary operator '%s'.\n", node->unary_op.op_symbol);
                result.type = RUNTIME_VALUE_NULL;
            }
            break;
        }
        case AST_VARIABLE: {
            RuntimeValue* value = runtime_get_variable(env, node->variable.variable_name);
            if (!value) {
                fprintf(stderr, "Error: Undefined variable '%s'.\n", node->variable.variable_name);
                result.type = RUNTIME_VALUE_NULL;
            } else {
                result = runtime_value_copy(value); // Return a copy to avoid sharing the same pointer
            }
            break;
        }
        case AST_IF_STATEMENT: {
            RuntimeValue condition = runtime_evaluate(env, node->if_statement.condition);
            if (condition.type == RUNTIME_VALUE_BOOLEAN && condition.boolean_value) {
                runtime_execute_block(env, node->if_statement.body);
            }
            result.type = RUNTIME_VALUE_NULL;
            break;
        }
        case AST_FOR_LOOP: {
            // Create a new scope for the loop
            Environment* loop_env = runtime_create_child_environment(env);

            // Execute initializer if it exists
            if (node->for_loop.initializer) {
                runtime_evaluate(loop_env, node->for_loop.initializer);
            }

            // Loop condition
            while (true) {
                // Evaluate condition if it exists
                if (node->for_loop.condition) {
                    RuntimeValue condition = runtime_evaluate(loop_env, node->for_loop.condition);
                    if (condition.type != RUNTIME_VALUE_BOOLEAN || !condition.boolean_value) {
                        break;
                    }
                }

                // Execute loop body
                runtime_execute_block(loop_env, node->for_loop.body);

                // Execute increment if it exists
                if (node->for_loop.increment) {
                    runtime_evaluate(loop_env, node->for_loop.increment);
                }
            }

            // Free the loop environment
            runtime_free_environment(loop_env);

            result.type = RUNTIME_VALUE_NULL;
            break;
        }
        case AST_WHILE_LOOP: {
            while (true) {
                RuntimeValue condition = runtime_evaluate(env, node->while_loop.condition);
                if (condition.type != RUNTIME_VALUE_BOOLEAN || !condition.boolean_value) {
                    break;
                }
                runtime_execute_block(env, node->while_loop.body);
            }
            result.type = RUNTIME_VALUE_NULL;
            break;
        }
        default:
            fprintf(stderr, "Error: Unhandled AST node type %d.\n", node->type);
            result.type = RUNTIME_VALUE_NULL;
            break;
    }

    return result;
}

void runtime_execute_block(Environment* env, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) {
        fprintf(stderr, "Error: Invalid block node provided for execution.\n");
        return;
    }

    for (int i = 0; i < block->block.statement_count; i++) {
        ASTNode* statement = block->block.statements[i];
        runtime_evaluate(env, statement);
    }
}

RuntimeValue runtime_execute_function_call(Environment* env, ASTNode* function_call) {
    const char* function_name = function_call->function_call.function_name;

   // Retrieve the function from the environment
    RuntimeValue* function_value = runtime_get_variable(env, function_name);
    if (function_value && function_value->type == RUNTIME_VALUE_FUNCTION) {
        if (function_value->function_value.function_type == FUNCTION_TYPE_BUILTIN) {
            // Built-in function
            BuiltinFunction builtin_function = function_value->function_value.builtin_function;
            
            int arg_count = function_call->function_call.argument_count;
            RuntimeValue* args = (RuntimeValue*)malloc(arg_count * sizeof(RuntimeValue));
            if (!args) {
                fprintf(stderr, "Error: Memory allocation failed for function arguments.\n");
                RuntimeValue result = { .type = RUNTIME_VALUE_NULL };
                return result;
            }

            // Evaluate arguments
            for (int i = 0; i < arg_count; i++) {
                args[i] = runtime_evaluate(env, function_call->function_call.arguments[i]);
            }

            // Execute the built-in function
            RuntimeValue result = builtin_function(env, args, arg_count);

            // Free the allocated memory
            free(args);

            return result;

        } else if (function_value->function_value.function_type == FUNCTION_TYPE_USER) {
            // User-defined function
            UserDefinedFunction* user_function = function_value->function_value.user_function;

            // Create a child environment for the function
            Environment* child_env = runtime_create_child_environment(env);

            // Map parameters to argument values
            for (int i = 0; i < user_function->parameter_count; i++) {
                const char* param_name = user_function->parameters[i];
                RuntimeValue arg_value = (i < function_call->function_call.argument_count)
                    ? runtime_evaluate(env, function_call->function_call.arguments[i])
                    : (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
                runtime_set_variable(child_env, param_name, arg_value);
            }

            // Execute the function body
            runtime_execute_block(child_env, user_function->body);

            // For simplicity, functions return null (you can extend this to support return statements)
            RuntimeValue result = { .type = RUNTIME_VALUE_NULL };

            // Free the child environment
            runtime_free_environment(child_env);

            return result;
        }
    } else {
        // Function not found
        fprintf(stderr, "Error: Undefined function '%s'.\n", function_name);
        RuntimeValue result = { .type = RUNTIME_VALUE_NULL };
        return result;
    }

    RuntimeValue result;
    result.type = RUNTIME_VALUE_NULL;
    return result;
}


void runtime_register_builtin(Environment* env, const char* name, BuiltinFunction function) {
    if (!env || !name || !function) {
        fprintf(stderr, "Error: Invalid arguments for registering a built-in function.\n");
        return;
    }

    // Create a runtime value to store the function pointer
    RuntimeValue function_value;
    function_value.type = RUNTIME_VALUE_FUNCTION;
    function_value.function_value.function_type = FUNCTION_TYPE_BUILTIN;
    function_value.function_value.builtin_function = function;

    // Add the function to the environment
    runtime_set_variable(env, name, function_value);
}

void runtime_register_function(Environment* env, UserDefinedFunction* function) {
    if (!env || !function || !function->name) {
        fprintf(stderr, "Error: Invalid arguments for registering a user-defined function.\n");
        return;
    }

    // Create a runtime value to store the user-defined function
    RuntimeValue function_value;
    function_value.type = RUNTIME_VALUE_STRING; // Using string storage to hold the pointer
    function_value.string_value = (char*)function;

    // Add the function to the environment
    runtime_set_variable(env, function->name, function_value);
}

UserDefinedFunction* runtime_get_function(Environment* env, const char* name) {
    if (!env || !name) {
        fprintf(stderr, "Error: Invalid arguments for retrieving a user-defined function.\n");
        return NULL;
    }

    // Search for the function in the environment
    RuntimeValue* value = runtime_get_variable(env, name);
    if (value && value->type == RUNTIME_VALUE_STRING) {
        // Assume the string value stores the pointer to the user-defined function
        return (UserDefinedFunction*)value->string_value;
    }

    // Function not found
    return NULL;
}

void runtime_error(RuntimeError* error) {
    if (!error) {
        fprintf(stderr, "Error: A runtime error occurred, but no details were provided.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Runtime Error: %s (Line: %d, Column: %d)\n", 
            error->message ? error->message : "Unknown error", 
            error->line, 
            error->column);
    
    // Terminate execution
    exit(EXIT_FAILURE);
}

void runtime_report_error(Environment* env, const char* message, const ASTNode* node) {
    (void)env; // Suppress unused parameter warning
    if (!message || !node) {
        fprintf(stderr, "Error: runtime_report_error called with invalid arguments.\n");
        exit(EXIT_FAILURE);
    }

    // Create and populate the runtime error structure
    RuntimeError error;
    error.message = strdup(message); // Duplicate the message for safety
    error.line = node->line;         // Assume the ASTNode has line information
    error.column = node->column;     // Assume the ASTNode has column information

    // Print the error details
    fprintf(stderr, "Runtime Error: %s (Line: %d, Column: %d)\n", 
            error.message, 
            error.line, 
            error.column);

    // Free the duplicated message
    free(error.message);

    // Terminate execution
    exit(EXIT_FAILURE);
}

void runtime_free_environment(Environment* env) {
    while (env) {
        Environment* next = env->next;

        // Free the variable name
        if (env->variable_name) {
            free(env->variable_name);
        }

        // Free the value
        runtime_free_value(&env->value);

        // Free the environment itself
        free(env);

        env = next;
    }
}

void runtime_free_value(RuntimeValue* value) {
    if (!value) {
        return;
    }

    switch (value->type) {
        case RUNTIME_VALUE_STRING:
            if (value->string_value) {
                free(value->string_value);
                value->string_value = NULL;
            }
            break;
        case RUNTIME_VALUE_FUNCTION:
            if (value->function_value.function_type == FUNCTION_TYPE_USER) {
                UserDefinedFunction* user_function = value->function_value.user_function;
                if (user_function) {
                    free(user_function->name);
                    for (int i = 0; i < user_function->parameter_count; i++) {
                        free(user_function->parameters[i]);
                    }
                    free(user_function->parameters);
                    // Do not free user_function->body here; it's part of the AST and will be freed separately
                    free(user_function);
                }
            }
            // No action needed for built-in functions
            break;
        default:
            // No action needed for other types
            break;
    }

    // Reset the value
    value->type = RUNTIME_VALUE_NULL;
}

void print_runtime_value(const RuntimeValue* value) {
    if (!value) {
        printf("RuntimeValue: NULL\n");
        return;
    }

    printf("RuntimeValue: ");
    switch (value->type) {
        case RUNTIME_VALUE_NUMBER:
            printf("Number: %f\n", value->number_value);
            break;

        case RUNTIME_VALUE_STRING:
            if (value->string_value) {
                printf("String: \"%s\"\n", value->string_value);
            } else {
                printf("String: NULL\n");
            }
            break;

        case RUNTIME_VALUE_BOOLEAN:
            printf("Boolean: %s\n", value->boolean_value ? "true" : "false");
            break;

        case RUNTIME_VALUE_NULL:
            printf("Null\n");
            break;

        default:
            printf("Unknown type\n");
            break;
    }
}

void runtime_print_environment(const Environment* env) {
    if (!env) {
        printf("Environment is empty.\n");
        return;
    }

    printf("Environment Variables:\n");
    const Environment* current = env;
    while (current) {
        printf("Variable: %s = ", current->variable_name);
        print_runtime_value(&current->value);
        current = current->next;
    }
}

char* runtime_value_to_string(const RuntimeValue* value) {
    if (!value) {
        return strdup("null");
    }

    char* result = NULL;

    switch (value->type) {
        case RUNTIME_VALUE_NUMBER: {
            // Allocate enough space for a double value in string form
            result = (char*)malloc(32);
            if (result) {
                snprintf(result, 32, "%.2f", value->number_value);
            }
            break;
        }

        case RUNTIME_VALUE_STRING: {
            // Do not add extra quotes
            result = strdup(value->string_value);
            break;
        }

        case RUNTIME_VALUE_BOOLEAN: {
            // Convert boolean to "true" or "false"
            result = strdup(value->boolean_value ? "true" : "false");
            break;
        }

        case RUNTIME_VALUE_NULL: {
            // Return "null" for null values
            result = strdup("null");
            break;
        }

        default: {
            // Handle unexpected types
            result = strdup("unknown");
            break;
        }
    }

    return result;
}

// Thread function to execute the block
static void* thread_execute_block(void* arg) {
    ThreadExecutionData* data = (ThreadExecutionData*)arg;

    if (!data || !data->env || !data->block) {
        fprintf(stderr, "Error: Invalid data passed to thread_execute_block.\n");
        free(data);
        return NULL;
    }

    // Execute the block
    runtime_execute_block(data->env, data->block);

    // Free the allocated thread data
    free(data);

    return NULL;
}

void runtime_execute_in_thread(Environment* env, ASTNode* block) {
    if (!env || !block) {
        fprintf(stderr, "Error: Cannot execute in thread with NULL environment or block.\n");
        return;
    }

    // Allocate memory for thread data
    ThreadExecutionData* data = (ThreadExecutionData*)malloc(sizeof(ThreadExecutionData));
    if (!data) {
        fprintf(stderr, "Error: Failed to allocate memory for thread data.\n");
        return;
    }

    // Initialize the thread data
    data->env = env;
    data->block = block;

    // Create a new thread
    pthread_t thread;
    int result = pthread_create(&thread, NULL, thread_execute_block, data);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to create thread (error code %d).\n", result);
        free(data);
        return;
    }

    // Detach the thread so it cleans up automatically
    pthread_detach(thread);
}

GarbageCollector* runtime_gc_init() {
    // Allocate memory for the GarbageCollector
    GarbageCollector* gc = (GarbageCollector*)malloc(sizeof(GarbageCollector));
    if (!gc) {
        fprintf(stderr, "Error: Failed to allocate memory for garbage collector.\n");
        return NULL;
    }

    // Initialize the fields
    gc->values = NULL;        // No tracked values initially
    gc->value_capacity = 0;   // No capacity initially
    gc->value_count = 0;      // No values are being tracked

    return gc;
}

void runtime_gc_track(GarbageCollector* gc, RuntimeValue value) {
    if (!gc) {
        fprintf(stderr, "Error: Garbage collector is NULL.\n");
        return;
    }

    // Resize the tracked values array if necessary
    if (gc->value_count >= gc->value_capacity) {
        size_t new_capacity = gc->value_capacity == 0 ? 16 : gc->value_capacity * 2;
        RuntimeValue* new_values = realloc(gc->values, new_capacity * sizeof(RuntimeValue));
        if (!new_values) {
            fprintf(stderr, "Error: Failed to allocate memory for garbage collector tracking.\n");
            return;
        }
        gc->values = new_values;
        gc->value_capacity = new_capacity;
    }

    // Add the value to the tracked values array
    gc->values[gc->value_count] = value;
    gc->value_count++;
}

void runtime_gc_collect(GarbageCollector* gc) {
    if (!gc) {
        fprintf(stderr, "Error: Garbage collector is NULL.\n");
        return;
    }

    for (size_t i = 0; i < gc->value_count; i++) {
        RuntimeValue* value = &gc->values[i];

        // Free memory for string values
        if (value->type == RUNTIME_VALUE_STRING && value->string_value) {
            free(value->string_value);
            value->string_value = NULL;
        }

        // Mark the value slot as nullified
        value->type = RUNTIME_VALUE_NULL;
    }

    // Reset the value count
    gc->value_count = 0;
}

void runtime_gc_free(GarbageCollector* gc) {
    if (gc) {
        // Free the array of tracked values if it exists
        if (gc->values) {
            free(gc->values);
        }

        // Free the GarbageCollector structure itself
        free(gc);
    }
}

void runtime_trigger_event(Environment* env, RuntimeEvent* event) {
    if (!env || !event) {
        fprintf(stderr, "Error: Environment or event is NULL in runtime_trigger_event.\n");
        return;
    }

    // Navigate through the environment to find event handlers
    Environment* current_env = env;

    while (current_env) {
        // Assuming event handlers are registered as variables in the environment
        RuntimeValue* handler_value = runtime_get_variable(current_env, event->event_name);
        if (handler_value && handler_value->type == RUNTIME_VALUE_FUNCTION) {
            FunctionValue function_value = handler_value->function_value;

            if (function_value.function_type == FUNCTION_TYPE_USER) {
                UserDefinedFunction* handler_function = function_value.user_function;

                if (handler_function) {
                    // Create a new environment for the function call
                    Environment* function_env = runtime_create_child_environment(current_env);

                    // Bind event data as function arguments (assumes single data value)
                    if (handler_function->parameter_count == 1 && event->data) {
                        runtime_set_variable(function_env, handler_function->parameters[0], *event->data);
                    }

                    // Execute the handler function body
                    runtime_execute_block(function_env, handler_function->body);

                    // Free the child environment after the function call
                    runtime_free_environment(function_env);

                    return; // Handler executed successfully
                }
            } else if (function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                // Handle built-in functions if necessary
                BuiltinFunction builtin_function = function_value.builtin_function;

                // Prepare arguments (assuming event data is the only argument)
                RuntimeValue args[1];
                if (event->data) {
                    args[0] = *(event->data);
                } else {
                    args[0].type = RUNTIME_VALUE_NULL;
                }

                // Call the built-in function
                builtin_function(current_env, args, 1);

                return; // Handler executed successfully
            }
        }

        // Move up to the parent environment if the handler was not found in the current scope
        current_env = current_env->parent;
    }

    // If we reached here, no handler was found for the event
    fprintf(stderr, "Warning: No handler found for event '%s'.\n", event->event_name);
}