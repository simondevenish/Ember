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

#include "runtime.h"
#include "utils.h"

// Forward declarations
static RuntimeValue evaluate_property_access(Environment* env, ASTNode* node);
static bool set_nested_property(Environment* env, ASTNode* object_node, const char* property, RuntimeValue value);
static void set_property_at_path(RuntimeValue* obj, const char* prop, RuntimeValue value);
static bool update_nested_property(const char* var_name, const char* outer_prop, const char* inner_prop, RuntimeValue value, Environment* env);

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
    if (!value) {
        fprintf(stderr, "VM Error: runtime_value_copy called with NULL value\n");
        RuntimeValue null_result = {.type = RUNTIME_VALUE_NULL};
        return null_result;
    }
    
    RuntimeValue copy = *value; // Shallow copy of the struct

    switch (value->type) {
        case RUNTIME_VALUE_STRING:
            if (value->string_value) {
                copy.string_value = strdup(value->string_value);
            }
            break;
        case RUNTIME_VALUE_ARRAY:
            if (value->array_value.elements && value->array_value.count > 0) {
                copy.array_value.elements = malloc(sizeof(RuntimeValue) * value->array_value.count);
                if (copy.array_value.elements) {
                    for (int i = 0; i < value->array_value.count; i++) {
                        copy.array_value.elements[i] = runtime_value_copy(&value->array_value.elements[i]);
                    }
                }
            }
            break;
        case RUNTIME_VALUE_OBJECT:
            if (value->object_value.keys && value->object_value.values && value->object_value.count > 0) {
                // Validate the object structure first
                for (int i = 0; i < value->object_value.count; i++) {
                    if (!value->object_value.keys[i]) {
                        fprintf(stderr, "VM Error: Object key[%d] is NULL\n", i);
                        copy.object_value.count = 0;
                        copy.object_value.keys = NULL;
                        copy.object_value.values = NULL;
                        return copy;
                    }
                }
                
                // Allocate memory for keys and values
                copy.object_value.keys = malloc(sizeof(char*) * value->object_value.count);
                copy.object_value.values = malloc(sizeof(RuntimeValue) * value->object_value.count);
                
                if (copy.object_value.keys && copy.object_value.values) {
                    // Copy each key and value
                    for (int i = 0; i < value->object_value.count; i++) {
                        copy.object_value.keys[i] = strdup(value->object_value.keys[i]);
                        copy.object_value.values[i] = runtime_value_copy(&value->object_value.values[i]);
                    }
                } else {
                    // Handle allocation failure
                    fprintf(stderr, "VM Error: Failed to allocate memory for object copy\n");
                    if (copy.object_value.keys) {
                        free(copy.object_value.keys);
                        copy.object_value.keys = NULL;
                    }
                    if (copy.object_value.values) {
                        free(copy.object_value.values);
                        copy.object_value.values = NULL;
                    }
                    copy.object_value.count = 0;
                }
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
        case AST_IMPORT: {
            // node->import_stmt.import_path => e.g. "items.ember"
            bool ok = runtime_execute_file_in_environment(env, 
                                 node->import_stmt.import_path);
            if (!ok) {
                fprintf(stderr, "Error: Failed to import '%s'\n",
                        node->import_stmt.import_path);
            }
            // keep result=null
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
        case AST_ARRAY_LITERAL: {
            // We have an array literal: we want to create a RUNTIME_VALUE_ARRAY
            // with elements.
            int count = node->array_literal.element_count;

            // Allocate a new array to store the evaluated elements
            RuntimeValue* elementValues = malloc(sizeof(RuntimeValue) * count);
            if (!elementValues) {
                fprintf(stderr, "Error: Memory allocation failed for array literal\n");
                break;
            }

            // Evaluate each child expression
            for (int i = 0; i < count; i++) {
                ASTNode* elemNode = node->array_literal.elements[i];
                elementValues[i] = runtime_evaluate(env, elemNode);
                // You may or may not want to do further checks here
            }

            // Populate the result as an array
            result.type = RUNTIME_VALUE_ARRAY;
            result.array_value.elements = elementValues;
            result.array_value.count    = count;
            break;
        }
        case AST_INDEX_ACCESS: {
            // Evaluate the array object
            RuntimeValue arrayVal = runtime_evaluate(env, node->index_access.array_expr);

            // Evaluate the index expression
            RuntimeValue indexVal = runtime_evaluate(env, node->index_access.index_expr);

            // Check that arrayVal is actually an array
            if (arrayVal.type != RUNTIME_VALUE_ARRAY) {
                fprintf(stderr, "Error: Attempted indexing on non-array type.\n");
                result.type = RUNTIME_VALUE_NULL;
                break;
            }

            // Check that indexVal is a number
            if (indexVal.type != RUNTIME_VALUE_NUMBER) {
                fprintf(stderr, "Error: Array index must be numeric.\n");
                result.type = RUNTIME_VALUE_NULL;
                break;
            }

            // Convert the index to an integer
            int idx = (int)indexVal.number_value;
            if (idx < 0 || idx >= arrayVal.array_value.count) {
                fprintf(stderr, "Error: Array index %d out of bounds.\n", idx);
                result.type = RUNTIME_VALUE_NULL;
                break;
            }

            // The array's elements are stored in arrayVal.array_value.elements
            // so we retrieve the element at idx
            RuntimeValue element = arrayVal.array_value.elements[idx];

            // We typically *copy* the element if your language semantics treat them as distinct
            // For a shallow approach, you might do:
            result = runtime_value_copy(&element);

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
        case AST_OBJECT_LITERAL: {
            // Create an object with the specified properties
            result.type = RUNTIME_VALUE_OBJECT;
            int property_count = node->object_literal.property_count;
            
            // Allocate arrays for keys and values
            result.object_value.keys = (char**)malloc(sizeof(char*) * property_count);
            result.object_value.values = (RuntimeValue*)malloc(sizeof(RuntimeValue) * property_count);
            
            if (!result.object_value.keys || !result.object_value.values) {
                fprintf(stderr, "Error: Memory allocation failed for object literal.\n");
                free(result.object_value.keys);
                free(result.object_value.values);
                result.type = RUNTIME_VALUE_NULL;
                break;
            }
            
            // Initialize each property
            for (int i = 0; i < property_count; i++) {
                result.object_value.keys[i] = strdup(node->object_literal.keys[i]);
                if (!result.object_value.keys[i]) {
                    fprintf(stderr, "Error: Memory allocation failed for object key.\n");
                    // Clean up already allocated memory
                    for (int j = 0; j < i; j++) {
                        free(result.object_value.keys[j]);
                    }
                    free(result.object_value.keys);
                    free(result.object_value.values);
                    result.type = RUNTIME_VALUE_NULL;
                    break;
                }
                
                // Evaluate the value expression and store it
                result.object_value.values[i] = runtime_evaluate(env, node->object_literal.values[i]);
            }
            
            result.object_value.count = property_count;
            break;
        }
        
        case AST_PROPERTY_ACCESS: {
            printf("DEBUG: AST_PROPERTY_ACCESS - property: %s\n", node->property_access.property); fflush(stdout);
            if (node->property_access.object->type == AST_VARIABLE) {
                printf("DEBUG: Base object is variable: %s\n", 
                      node->property_access.object->variable.variable_name); fflush(stdout);
            } else if (node->property_access.object->type == AST_PROPERTY_ACCESS) {
                printf("DEBUG: Base object is nested property access: %s\n", 
                      node->property_access.object->property_access.property); fflush(stdout);
            }
            result = evaluate_property_access(env, node);
            printf("DEBUG: Property access result type: %d\n", result.type); fflush(stdout);
            if (result.type == RUNTIME_VALUE_STRING && result.string_value) {
                printf("DEBUG: Property value: %s\n", result.string_value); fflush(stdout);
            }
            break;
        }
        
        case AST_PROPERTY_ASSIGNMENT: {
            RuntimeValue value = runtime_evaluate(env, node->property_assignment.value);
            ASTNode* obj_node = node->property_assignment.object;
            
            printf("DEBUG: Property Assignment - setting '%s' to ", node->property_assignment.property); fflush(stdout);
            if (value.type == RUNTIME_VALUE_STRING) {
                printf("'%s'\n", value.string_value); fflush(stdout);
            } else {
                printf("value of type %d\n", value.type); fflush(stdout);
            }
            
            if (obj_node->type == AST_PROPERTY_ACCESS) {
                printf("DEBUG: Target is property access: '%s'\n", obj_node->property_access.property); fflush(stdout);
                
                // Let's handle the simplest case first: obj.x.y = z where obj is a variable
                if (obj_node->property_access.object->type == AST_VARIABLE) {
                    const char* var_name = obj_node->property_access.object->variable.variable_name;
                    const char* prop_name = obj_node->property_access.property;
                    const char* nested_prop = node->property_assignment.property;
                    
                    printf("DEBUG: Setting %s.%s.%s\n", var_name, prop_name, nested_prop); fflush(stdout);
                    
                    // Get the root object
                    RuntimeValue* root_obj_ptr = runtime_get_variable(env, var_name);
                    if (!root_obj_ptr || root_obj_ptr->type != RUNTIME_VALUE_OBJECT) {
                        fprintf(stderr, "Error: Cannot access property of non-object variable: %s\n", var_name);
                        result.type = RUNTIME_VALUE_NULL;
                        return result;
                    }
                    
                    // Make a copy of the root object so we can modify it
                    RuntimeValue root_obj = runtime_value_copy(root_obj_ptr);
                    
                    // Find the inner object (obj.x)
                    bool found_inner = false;
                    int inner_index = -1;
                    for (int i = 0; i < root_obj.object_value.count; i++) {
                        if (strcmp(root_obj.object_value.keys[i], prop_name) == 0) {
                            inner_index = i;
                            found_inner = true;
                            break;
                        }
                    }
                    
                    if (!found_inner) {
                        fprintf(stderr, "Error: Property not found: %s.%s\n", var_name, prop_name);
                        runtime_free_value(&root_obj);
                        result.type = RUNTIME_VALUE_NULL;
                        return result;
                    }
                    
                    // Check if the inner property is an object
                    if (root_obj.object_value.values[inner_index].type != RUNTIME_VALUE_OBJECT) {
                        fprintf(stderr, "Error: Property is not an object: %s.%s\n", var_name, prop_name);
                        runtime_free_value(&root_obj);
                        result.type = RUNTIME_VALUE_NULL;
                        return result;
                    }
                    
                    // Set the nested property on the inner object
                    RuntimeValue* inner_obj = &root_obj.object_value.values[inner_index];
                    bool found_nested = false;
                    for (int i = 0; i < inner_obj->object_value.count; i++) {
                        if (strcmp(inner_obj->object_value.keys[i], nested_prop) == 0) {
                            // Replace the existing value
                            runtime_free_value(&inner_obj->object_value.values[i]);
                            inner_obj->object_value.values[i] = runtime_value_copy(&value);
                            found_nested = true;
                            break;
                        }
                    }
                    
                    if (!found_nested) {
                        // Add the new property
                        int new_index = inner_obj->object_value.count;
                        inner_obj->object_value.count++;
                        inner_obj->object_value.keys = realloc(inner_obj->object_value.keys, 
                                                             inner_obj->object_value.count * sizeof(char*));
                        inner_obj->object_value.values = realloc(inner_obj->object_value.values, 
                                                               inner_obj->object_value.count * sizeof(RuntimeValue));
                        
                        inner_obj->object_value.keys[new_index] = strdup(nested_prop);
                        inner_obj->object_value.values[new_index] = runtime_value_copy(&value);
                    }
                    
                    // Update the root object in the environment
                    runtime_set_variable(env, var_name, root_obj);
                    printf("DEBUG: Updated nested property %s.%s.%s\n", var_name, prop_name, nested_prop); fflush(stdout);
                    
                    // Cleanup
                    runtime_free_value(&root_obj);
                    
                    return value;
                } else {
                    printf("DEBUG: Complex nested property assignment not fully implemented\n"); fflush(stdout);
                    // This is a more complex case (e.g. (expr).prop.inner = value)
                    // Implement this later if needed
                }
            }
            
            // Handle normal property assignment (obj.prop = value)
            RuntimeValue object = runtime_evaluate(env, obj_node);
            
            if (object.type != RUNTIME_VALUE_OBJECT) {
                fprintf(stderr, "Error: Cannot set property on non-object\n");
                result.type = RUNTIME_VALUE_NULL;
                return result;
            }
            
            // Find and update the property
            bool found = false;
            for (int i = 0; i < object.object_value.count; i++) {
                if (strcmp(object.object_value.keys[i], node->property_assignment.property) == 0) {
                    // Replace the value
                    runtime_free_value(&object.object_value.values[i]);
                    object.object_value.values[i] = runtime_value_copy(&value);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Add new property
                int new_index = object.object_value.count;
                object.object_value.count++;
                object.object_value.keys = realloc(object.object_value.keys,
                                                 object.object_value.count * sizeof(char*));
                object.object_value.values = realloc(object.object_value.values,
                                                   object.object_value.count * sizeof(RuntimeValue));
                
                object.object_value.keys[new_index] = strdup(node->property_assignment.property);
                object.object_value.values[new_index] = runtime_value_copy(&value);
            }
            
            // If this is a direct variable reference, update the variable
            if (obj_node->type == AST_VARIABLE) {
                const char* var_name = obj_node->variable.variable_name;
                runtime_set_variable(env, var_name, object);
            }
            
            // For property assignments, the result is the value being assigned
            return value;
        }
        case AST_METHOD_CALL: {
            // Evaluate the object expression
            RuntimeValue object = runtime_evaluate(env, node->method_call.object);
            
            // Ensure that it's an object
            if (object.type != RUNTIME_VALUE_OBJECT) {
                fprintf(stderr, "Error: Cannot call method '%s' on non-object value.\n", 
                         node->method_call.method);
                result.type = RUNTIME_VALUE_NULL;
                break;
            }
            
            // Look for the method in the object
            bool found = false;
            RuntimeValue method_value;
            
            for (int i = 0; i < object.object_value.count; i++) {
                if (strcmp(object.object_value.keys[i], node->method_call.method) == 0) {
                    // Found the method
                    method_value = object.object_value.values[i];
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                fprintf(stderr, "Error: Object has no method '%s'.\n", node->method_call.method);
                result.type = RUNTIME_VALUE_NULL;
                break;
            }
            
            // Ensure the property is actually a function
            if (method_value.type != RUNTIME_VALUE_FUNCTION) {
                fprintf(stderr, "Error: Property '%s' is not a function.\n", node->method_call.method);
                result.type = RUNTIME_VALUE_NULL;
                break;
            }
            
            // Execute the method in the context of the object
            if (method_value.function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                // Built-in function
                BuiltinFunction builtin_function = method_value.function_value.builtin_function;
                
                // Prepare arguments
                int arg_count = node->method_call.argument_count;
                RuntimeValue* args = (RuntimeValue*)malloc((arg_count + 1) * sizeof(RuntimeValue));
                if (!args) {
                    fprintf(stderr, "Error: Memory allocation failed for method arguments.\n");
                    result.type = RUNTIME_VALUE_NULL;
                    break;
                }
                
                // Add 'this' as the first argument (the object itself)
                args[0] = object;
                
                // Evaluate and add user arguments
                for (int i = 0; i < arg_count; i++) {
                    args[i + 1] = runtime_evaluate(env, node->method_call.arguments[i]);
                }
                
                // Execute the built-in function with the object as context
                result = builtin_function(env, args, arg_count + 1);
                
                // Free the allocated memory
                free(args);
                
            } else {
                // User-defined function
                UserDefinedFunction* user_function = method_value.function_value.user_function;
                
                // Create a child environment for the function
                Environment* child_env = runtime_create_child_environment(env);
                
                // Add 'this' as a special variable in the environment
                runtime_set_variable(child_env, "this", object);
                
                // Map parameters to argument values
                for (int i = 0; i < user_function->parameter_count; i++) {
                    const char* param_name = user_function->parameters[i];
                    RuntimeValue arg_value = (i < node->method_call.argument_count)
                        ? runtime_evaluate(env, node->method_call.arguments[i])
                        : (RuntimeValue){ .type = RUNTIME_VALUE_NULL };
                    runtime_set_variable(child_env, param_name, arg_value);
                }
                
                // Execute the function body
                runtime_execute_block(child_env, user_function->body);
                
                // For simplicity, methods return null (you can extend this to support return statements)
                result.type = RUNTIME_VALUE_NULL;
                
                // Free the child environment
                runtime_free_environment(child_env);
            }
            
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

bool runtime_execute_file_in_environment(Environment* env, const char* filename) {
    // 1) Read file
    char* script_content = read_file(filename);
    if (!script_content) {
        fprintf(stderr, "Error: Could not open import file '%s'\n", filename);
        return false;
    }

    // 2) Lex + parse => get AST
    Lexer lex;
    lexer_init(&lex, script_content);
    Parser* p = parser_create(&lex);
    ASTNode* root = parse_script(p);
    free(p); // free parser struct if needed

    if (!root) {
        fprintf(stderr, "Error: Parsing import file '%s' failed.\n", filename);
        free(script_content);
        return false;
    }

    // 3) Evaluate the top-level AST in the SAME environment
    //    which merges the variables and functions into the current environment.
    runtime_execute_block(env, root);
    // or if parse_script returns a block node, we can do:
    // runtime_execute_block(env, root);

    free_ast(root);
    free(script_content);
    return true;
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
        case RUNTIME_VALUE_ARRAY:
            if (value->array_value.elements) {
                // Free each element in the array
                for (int i = 0; i < value->array_value.count; i++) {
                    runtime_free_value(&value->array_value.elements[i]);
                }
                free(value->array_value.elements);
                value->array_value.elements = NULL;
            }
            value->array_value.count = 0;
            break;
        case RUNTIME_VALUE_OBJECT:
            if (value->object_value.keys && value->object_value.values) {
                // Free each key and value in the object
                for (int i = 0; i < value->object_value.count; i++) {
                    free(value->object_value.keys[i]);
                    runtime_free_value(&value->object_value.values[i]);
                }
                free(value->object_value.keys);
                free(value->object_value.values);
                value->object_value.keys = NULL;
                value->object_value.values = NULL;
            }
            value->object_value.count = 0;
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
            
        case RUNTIME_VALUE_ARRAY:
            printf("Array: [\n");
            for (int i = 0; i < value->array_value.count; i++) {
                printf("  [%d] ", i);
                print_runtime_value(&value->array_value.elements[i]);
            }
            printf("]\n");
            break;
            
        case RUNTIME_VALUE_OBJECT:
            printf("Object: {\n");
            for (int i = 0; i < value->object_value.count; i++) {
                printf("  %s: ", value->object_value.keys[i]);
                print_runtime_value(&value->object_value.values[i]);
            }
            printf("}\n");
            break;
            
        case RUNTIME_VALUE_FUNCTION:
            if (value->function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                printf("Built-in Function\n");
            } else {
                printf("User-defined Function: %s\n", 
                       value->function_value.user_function->name);
            }
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
        
        case RUNTIME_VALUE_ARRAY: {
            // Start with an empty JSON-style array
            char* array_str = strdup("[]");
            if (!array_str) return strdup("[Array]");
            
            // For each element in the array
            for (int i = 0; i < value->array_value.count; i++) {
                // Convert the element to a string
                char* elem_str = runtime_value_to_string(&value->array_value.elements[i]);
                if (!elem_str) continue;
                
                // Allocate memory for the new array string (old string + element + delimiter)
                size_t new_len = strlen(array_str) + strlen(elem_str) + 3; // +3 for ", " and potentially "]"
                char* new_array_str = (char*)malloc(new_len);
                if (!new_array_str) {
                    free(elem_str);
                    free(array_str);
                    return strdup("[Array]");
                }
                
                // If this is the first element, replace the opening bracket
                if (i == 0) {
                    sprintf(new_array_str, "[%s", elem_str);
                } else {
                    // Otherwise, append to the existing string with a delimiter
                    sprintf(new_array_str, "%.*s, %s", (int)strlen(array_str) - 1, array_str, elem_str);
                }
                
                // Close the array
                strcat(new_array_str, "]");
                
                // Free the old strings and update array_str
                free(elem_str);
                free(array_str);
                array_str = new_array_str;
            }
            
            result = array_str;
            break;
        }
        
        case RUNTIME_VALUE_OBJECT: {
            // Start with an empty JSON-style object
            char* obj_str = strdup("{}");
            if (!obj_str) return strdup("{Object}");
            
            // For each property in the object
            for (int i = 0; i < value->object_value.count; i++) {
                char* key = value->object_value.keys[i];
                char* value_str = runtime_value_to_string(&value->object_value.values[i]);
                if (!value_str) continue;
                
                // Allocate memory for the new object string
                size_t new_len = strlen(obj_str) + strlen(key) + strlen(value_str) + 5; // +5 for ": ", ", " and "}"
                char* new_obj_str = (char*)malloc(new_len);
                if (!new_obj_str) {
                    free(value_str);
                    free(obj_str);
                    return strdup("{Object}");
                }
                
                // If this is the first property, replace the opening brace
                if (i == 0) {
                    sprintf(new_obj_str, "{%s: %s", key, value_str);
                } else {
                    // Otherwise, append to the existing string with a delimiter
                    sprintf(new_obj_str, "%.*s, %s: %s", (int)strlen(obj_str) - 1, obj_str, key, value_str);
                }
                
                // Close the object
                strcat(new_obj_str, "}");
                
                // Free the old strings and update obj_str
                free(value_str);
                free(obj_str);
                obj_str = new_obj_str;
            }
            
            result = obj_str;
            break;
        }
        
        case RUNTIME_VALUE_FUNCTION: {
            // Return a simple string for functions
            if (value->function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                result = strdup("[Built-in Function]");
            } else {
                // Use the function name if available
                const char* name = value->function_value.user_function->name;
                if (name) {
                    size_t len = strlen(name) + 16; // Extra space for "[Function: ]"
                    result = (char*)malloc(len);
                    if (result) {
                        snprintf(result, len, "[Function: %s]", name);
                    } else {
                        result = strdup("[Function]");
                    }
                } else {
                    result = strdup("[Function]");
                }
            }
            break;
        }

        default: {
            // Handle unexpected types
            result = strdup("[Unknown Type]");
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

// -----------------------------
// Property Access Helper
// -----------------------------
static RuntimeValue evaluate_property_access(Environment* env, ASTNode* node) {
    if (node->type != AST_PROPERTY_ACCESS) {
        fprintf(stderr, "Error: evaluate_property_access called on non-property access node\n");
        RuntimeValue result;
        result.type = RUNTIME_VALUE_NULL;
        return result;
    }
    
    printf("DEBUG: Accessing property: %s\n", node->property_access.property); fflush(stdout);
    
    // Evaluate the object expression
    RuntimeValue object = runtime_evaluate(env, node->property_access.object);
    
    printf("DEBUG: Object type: %d\n", object.type); fflush(stdout);
    
    // Ensure that it's an object
    if (object.type != RUNTIME_VALUE_OBJECT) {
        fprintf(stderr, "Error: Cannot access property '%s' of non-object value.\n", 
                 node->property_access.property);
        runtime_free_value(&object);
        RuntimeValue result;
        result.type = RUNTIME_VALUE_NULL;
        return result;
    }
    
    printf("DEBUG: Object has %d properties\n", object.object_value.count); fflush(stdout);
    for (int i = 0; i < object.object_value.count; i++) {
        printf("DEBUG: Property %d: '%s'\n", i, object.object_value.keys[i]); fflush(stdout);
    }
    
    // Look for the property in the object
    bool found = false;
    RuntimeValue result;
    
    for (int i = 0; i < object.object_value.count; i++) {
        if (strcmp(object.object_value.keys[i], node->property_access.property) == 0) {
            // Found the property, return its value
            result = runtime_value_copy(&object.object_value.values[i]);
            printf("DEBUG: Found property '%s' with type %d\n", 
                  node->property_access.property, result.type); fflush(stdout);
            
            if (result.type == RUNTIME_VALUE_STRING && result.string_value != NULL) {
                printf("DEBUG: Property value is string: '%s'\n", result.string_value); fflush(stdout);
            }
            
            found = true;
            break;
        }
    }
    
    if (!found) {
        fprintf(stderr, "Error: Object has no property '%s'.\n", node->property_access.property);
        result.type = RUNTIME_VALUE_NULL;
    }
    
    runtime_free_value(&object);
    return result;
}

// -----------------------------
// Set Nested Property Helper
// -----------------------------
static bool set_nested_property(Environment* env, ASTNode* object_node, const char* property, RuntimeValue value) {
    printf("DEBUG: set_nested_property called with property '%s'\n", property);
    
    if (object_node->type == AST_VARIABLE) {
        // Simple case: variable.property
        char* var_name = object_node->variable.variable_name;
        printf("DEBUG: Simple case - Setting %s.%s\n", var_name, property);
        RuntimeValue* var_ptr = runtime_get_variable(env, var_name);
        
        if (!var_ptr || var_ptr->type != RUNTIME_VALUE_OBJECT) {
            printf("DEBUG: Variable '%s' not found or not an object\n", var_name);
            return false;
        }
        
        RuntimeValue var_copy = runtime_value_copy(var_ptr);
        
        // Find or add the property
        bool found = false;
        for (int i = 0; i < var_copy.object_value.count; i++) {
            if (strcmp(var_copy.object_value.keys[i], property) == 0) {
                printf("DEBUG: Found property '%s' in variable '%s'\n", property, var_name);
                runtime_free_value(&var_copy.object_value.values[i]);
                var_copy.object_value.values[i] = runtime_value_copy(&value);
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Add new property
            int new_count = var_copy.object_value.count + 1;
            char** new_keys = realloc(var_copy.object_value.keys, sizeof(char*) * new_count);
            RuntimeValue* new_values = realloc(var_copy.object_value.values, sizeof(RuntimeValue) * new_count);
            
            if (!new_keys || !new_values) {
                runtime_free_value(&var_copy);
                return false;
            }
            
            var_copy.object_value.keys = new_keys;
            var_copy.object_value.values = new_values;
            var_copy.object_value.keys[var_copy.object_value.count] = strdup(property);
            var_copy.object_value.values[var_copy.object_value.count] = runtime_value_copy(&value);
            var_copy.object_value.count = new_count;
        }
        
        // Update the variable
        runtime_set_variable(env, var_name, var_copy);
        runtime_free_value(&var_copy);
        
        return true;
    }
    else if (object_node->type == AST_PROPERTY_ACCESS) {
        // Nested case: obj.inner.property
        char* inner_prop = object_node->property_access.property;
        ASTNode* outer_obj = object_node->property_access.object;
        
        printf("DEBUG: Nested case - obj.%s.%s\n", inner_prop, property);
        
        if (outer_obj->type == AST_VARIABLE) {
            char* var_name = outer_obj->variable.variable_name;
            printf("DEBUG: Root object is variable '%s'\n", var_name);
        } else {
            printf("DEBUG: Root object is not a variable, type %d\n", outer_obj->type);
        }
        
        // Get the outer object
        RuntimeValue outer = runtime_evaluate(env, outer_obj);
        if (outer.type != RUNTIME_VALUE_OBJECT) {
            printf("DEBUG: Outer object is not an object, type %d\n", outer.type);
            runtime_free_value(&outer);
            return false;
        }
        
        // Find the inner object
        printf("DEBUG: Looking for inner property '%s' in outer object\n", inner_prop);
        printf("DEBUG: Outer object has %d properties\n", outer.object_value.count);
        for (int i = 0; i < outer.object_value.count; i++) {
            printf("DEBUG: Outer object property %d: '%s'\n", i, outer.object_value.keys[i]);
        }
        
        bool found_inner = false;
        int inner_idx = -1;
        for (int i = 0; i < outer.object_value.count; i++) {
            if (strcmp(outer.object_value.keys[i], inner_prop) == 0) {
                printf("DEBUG: Found inner property '%s'\n", inner_prop);
                if (outer.object_value.values[i].type != RUNTIME_VALUE_OBJECT) {
                    printf("DEBUG: Inner property '%s' is not an object, type %d\n", 
                          inner_prop, outer.object_value.values[i].type);
                    runtime_free_value(&outer);
                    return false;
                }
                found_inner = true;
                inner_idx = i;
                break;
            }
        }
        
        if (!found_inner) {
            runtime_free_value(&outer);
            return false;
        }
        
        // Update or add the property in the inner object
        RuntimeValue* inner_obj = &outer.object_value.values[inner_idx];
        printf("DEBUG: Inner object has %d properties\n", inner_obj->object_value.count);
        for (int i = 0; i < inner_obj->object_value.count; i++) {
            printf("DEBUG: Inner object property %d: '%s'\n", i, inner_obj->object_value.keys[i]);
        }
        
        // Find or add the property in the inner object
        bool found_prop = false;
        for (int i = 0; i < inner_obj->object_value.count; i++) {
            if (strcmp(inner_obj->object_value.keys[i], property) == 0) {
                // Update
                printf("DEBUG: Found property '%s' in inner object, updating\n", property);
                runtime_free_value(&inner_obj->object_value.values[i]);
                inner_obj->object_value.values[i] = runtime_value_copy(&value);
                if (value.type == RUNTIME_VALUE_STRING) {
                    printf("DEBUG: Updated to string value: '%s'\n", 
                              value.string_value);
                }
                found_prop = true;
                break;
            }
        }
        
        if (!found_prop) {
            // Add new
            int new_count = inner_obj->object_value.count + 1;
            char** new_keys = realloc(inner_obj->object_value.keys, sizeof(char*) * new_count);
            RuntimeValue* new_values = realloc(inner_obj->object_value.values, sizeof(RuntimeValue) * new_count);
            
            if (!new_keys || !new_values) {
                runtime_free_value(&outer);
                return false;
            }
            
            inner_obj->object_value.keys = new_keys;
            inner_obj->object_value.values = new_values;
            inner_obj->object_value.keys[inner_obj->object_value.count] = strdup(property);
            inner_obj->object_value.values[inner_obj->object_value.count] = runtime_value_copy(&value);
            inner_obj->object_value.count = new_count;
        }
        
        // Now update the original variable that contains the outer object
        if (outer_obj->type == AST_VARIABLE) {
            char* var_name = outer_obj->variable.variable_name;
            runtime_set_variable(env, var_name, outer);
        }
        else {
            // For deeply nested access, this won't work well
            // We would need a more complex approach
            runtime_free_value(&outer);
            return false;
        }
        
        runtime_free_value(&outer);
        return true;
    }
    
    return false;
}

// Simple direct function to set a nested property
static void set_property_at_path(RuntimeValue* obj, const char* prop, RuntimeValue value) {
    if (!obj || !prop) {
        fprintf(stderr, "Error: NULL object or property name in set_property_at_path\n");
        return;
    }

    if (obj->type != RUNTIME_VALUE_OBJECT) {
        fprintf(stderr, "Error: Cannot set property '%s' on non-object value\n", prop);
        return;
    }

    // Look for the property in the object
    bool found = false;
    
    // Check if the property already exists
    for (int i = 0; i < obj->object_value.count; i++) {
        if (strcmp(obj->object_value.keys[i], prop) == 0) {
            // Update existing property
            runtime_free_value(&obj->object_value.values[i]);  // Free existing value
            obj->object_value.values[i] = runtime_value_copy(&value);  // Set new value
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Property doesn't exist, add it
        int newCount = obj->object_value.count + 1;
        
        // Expand keys array
        char** newKeys = realloc(
            obj->object_value.keys,
            newCount * sizeof(char*)
        );
        if (!newKeys) {
            fprintf(stderr, "Error: Object property keys reallocation failed.\n");
            return;
        }
        
        // Expand values array
        RuntimeValue* newValues = realloc(
            obj->object_value.values,
            newCount * sizeof(RuntimeValue)
        );
        if (!newValues) {
            fprintf(stderr, "Error: Object property values reallocation failed.\n");
            free(newKeys);
            return;
        }
        
        // Add the new property
        newKeys[obj->object_value.count] = strdup(prop);
        newValues[obj->object_value.count] = runtime_value_copy(&value);
        
        // Update the object
        obj->object_value.keys = newKeys;
        obj->object_value.values = newValues;
        obj->object_value.count = newCount;
    }
}

/**
 * Helper function to update a nested property (obj.x.y) format
 * 
 * @param var_name The name of the root variable (e.g., "obj")
 * @param outer_prop The name of the outer property (e.g., "x")
 * @param inner_prop The name of the inner property (e.g., "y")
 * @param value The value to assign to the inner property
 * @param env The current environment
 * @return true if successful, false if there was an error
 */
static bool update_nested_property(const char* var_name, const char* outer_prop, const char* inner_prop, RuntimeValue value, Environment* env) {
    printf("DEBUG: update_nested_property: %s.%s.%s\n", var_name, outer_prop, inner_prop); fflush(stdout);
    
    // Get the variable from the environment
    RuntimeValue* var_ptr = runtime_get_variable(env, var_name);
    if (!var_ptr || var_ptr->type != RUNTIME_VALUE_OBJECT) {
        fprintf(stderr, "Error: Variable '%s' not found or not an object\n", var_name); fflush(stderr);
        return false;
    }
    
    printf("DEBUG: Found variable '%s' in environment\n", var_name); fflush(stdout);
    
    // Make a copy of the variable
    RuntimeValue var_copy = runtime_value_copy(var_ptr);
    
    printf("DEBUG: Variable has %d properties\n", var_copy.object_value.count); fflush(stdout);
    for (int i = 0; i < var_copy.object_value.count; i++) {
        printf("DEBUG: Property %d: '%s'\n", i, var_copy.object_value.keys[i]); fflush(stdout);
    }
    
    // Find the outer property (e.g., "x" in obj.x.y)
    bool found_outer = false;
    for (int i = 0; i < var_copy.object_value.count; i++) {
        if (strcmp(var_copy.object_value.keys[i], outer_prop) == 0) {
            found_outer = true;
            printf("DEBUG: Found outer property '%s' at index %d\n", outer_prop, i); fflush(stdout);
            
            // Make sure it's an object
            if (var_copy.object_value.values[i].type != RUNTIME_VALUE_OBJECT) {
                fprintf(stderr, "Error: Property '%s' is not an object\n", outer_prop); fflush(stderr);
                runtime_free_value(&var_copy);
                return false;
            }
            
            printf("DEBUG: Outer property '%s' is an object with %d properties\n", 
                  outer_prop, var_copy.object_value.values[i].object_value.count); fflush(stdout);
            
            for (int j = 0; j < var_copy.object_value.values[i].object_value.count; j++) {
                printf("DEBUG: Inner property %d: '%s'\n", j, 
                      var_copy.object_value.values[i].object_value.keys[j]); fflush(stdout);
            }
            
            // Update the inner property in the inner object
            printf("DEBUG: Setting inner property '%s'\n", inner_prop); fflush(stdout);
            set_property_at_path(&(var_copy.object_value.values[i]), inner_prop, value);
            
            // Debug the inner object after update
            printf("DEBUG: Inner object after update has %d properties\n", 
                  var_copy.object_value.values[i].object_value.count); fflush(stdout);
                  
            for (int j = 0; j < var_copy.object_value.values[i].object_value.count; j++) {
                printf("DEBUG: After update - Inner property %d: '%s'\n", j, 
                      var_copy.object_value.values[i].object_value.keys[j]); fflush(stdout);
                
                if (strcmp(var_copy.object_value.values[i].object_value.keys[j], inner_prop) == 0) {
                    printf("DEBUG: Found updated property '%s'\n", inner_prop); fflush(stdout);
                    
                    if (var_copy.object_value.values[i].object_value.values[j].type == RUNTIME_VALUE_STRING) {
                        printf("DEBUG: Updated value is string: '%s'\n", 
                              var_copy.object_value.values[i].object_value.values[j].string_value); fflush(stdout);
                    }
                }
            }
            
            break;
        }
    }
    
    if (!found_outer) {
        fprintf(stderr, "Error: Property '%s' not found in object '%s'\n", outer_prop, var_name); fflush(stderr);
        runtime_free_value(&var_copy);
        return false;
    }
    
    // Update the variable in the environment
    runtime_set_variable(env, var_name, var_copy);
    printf("DEBUG: Updated variable '%s' in environment with new nested property value\n", var_name); fflush(stdout);
    
    // Clean up
    runtime_free_value(&var_copy);
    
    return true;
}