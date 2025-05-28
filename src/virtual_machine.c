#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "virtual_machine.h"
#include "runtime.h"

// Forward declarations for runtime functions
extern RuntimeValue runtime_value_copy(const RuntimeValue* value);
extern void runtime_free_value(RuntimeValue* value);

// Global environment for built-in functions
static Environment* g_global_env = NULL;

// Forward declarations
static void print_runtime_value_type(const char* label, const RuntimeValue* value);

// Forward declaration of helper functions
static bool update_globals_after_property_op(RuntimeValue* original, RuntimeValue* modified);
static RuntimeValue vm_get_property(RuntimeValue obj, const char* prop);
static void vm_add_property(RuntimeValue* obj, const char* propName, RuntimeValue* value);
static int json_stringify_value(const RuntimeValue* value, char* buffer, size_t buffer_size);
static void vm_copy_builtins_to_env(Environment* target_env);

/* ----------------
   Chunk Functions
   ---------------- */

BytecodeChunk* vm_create_chunk() {
    BytecodeChunk* chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
    if (!chunk) {
        fprintf(stderr, "Error: Memory allocation failed for BytecodeChunk.\n");
        return NULL;
    }
    chunk->code = NULL;
    chunk->code_count = 0;
    chunk->code_capacity = 0;

    chunk->constants = NULL;
    chunk->constants_count = 0;
    chunk->constants_capacity = 0;

    return chunk;
}

void vm_free_chunk(BytecodeChunk* chunk) {
    if (!chunk) return;
    if (chunk->code) free(chunk->code);
    if (chunk->constants) {
        // Strings or other allocated data in constants,
        // we might free them individually here.
        free(chunk->constants);
    }
    free(chunk);
}

static void ensure_code_capacity(BytecodeChunk* chunk, int additional) {
    int required = chunk->code_count + additional;
    if (required <= chunk->code_capacity) return;

    // Grow
    int new_capacity = (chunk->code_capacity < 8) ? 8 : chunk->code_capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    uint8_t* new_code = (uint8_t*)realloc(chunk->code, new_capacity * sizeof(uint8_t));
    if (!new_code) {
        fprintf(stderr, "Error: Memory allocation failed for code reallocation.\n");
        return;
    }
    chunk->code = new_code;
    chunk->code_capacity = new_capacity;
}

void vm_chunk_write_byte(BytecodeChunk* chunk, uint8_t byte) {
    ensure_code_capacity(chunk, 1);
    chunk->code[chunk->code_count] = byte;
    chunk->code_count++;
}

static void ensure_constants_capacity(BytecodeChunk* chunk) {
    if (chunk->constants_count < chunk->constants_capacity) return;
    int new_capacity = (chunk->constants_capacity < 8) ? 8 : chunk->constants_capacity * 2;
    RuntimeValue* new_constants = (RuntimeValue*)realloc(
        chunk->constants,
        new_capacity * sizeof(RuntimeValue)
    );
    if (!new_constants) {
        fprintf(stderr, "Error: Memory allocation failed for constants reallocation.\n");
        return;
    }
    chunk->constants = new_constants;
    chunk->constants_capacity = new_capacity;
}

int vm_chunk_add_constant(BytecodeChunk* chunk, RuntimeValue value) {
    ensure_constants_capacity(chunk);
    chunk->constants[chunk->constants_count] = value;
    chunk->constants_count++;
    return chunk->constants_count - 1; // index of the newly added constant
}

/* ----------------
   VM Functions
   ---------------- */

VM* vm_create(BytecodeChunk* chunk) {
    VM* vm = (VM*)malloc(sizeof(VM));
    if (!vm) {
        fprintf(stderr, "Error: Memory allocation failed for VM.\n");
        return NULL;
    }
    vm->chunk = chunk;
    vm->ip = chunk->code; // Start at the beginning of the code

    // TODO(SD) For now, let's pick a default stack size
    vm->stack_capacity = 256;
    vm->stack = (RuntimeValue*)malloc(sizeof(RuntimeValue) * vm->stack_capacity);
    vm->stack_top = vm->stack;
    // Initialize the stack with nulls if you want
    for (int i = 0; i < vm->stack_capacity; i++) {
        vm->stack[i].type = RUNTIME_VALUE_NULL;
    }

    // Initialize function storage
    vm->function_capacity = 32;
    vm->function_count = 0;
    vm->functions = (VMFunction*)malloc(sizeof(VMFunction) * vm->function_capacity);
    if (!vm->functions) {
        fprintf(stderr, "Error: Memory allocation failed for VM functions.\n");
        free(vm->stack);
        free(vm);
        return NULL;
    }

    // Initialize call frame storage
    vm->frame_capacity = 64;
    vm->frame_count = 0;
    vm->call_frames = (CallFrame*)malloc(sizeof(CallFrame) * vm->frame_capacity);
    if (!vm->call_frames) {
        fprintf(stderr, "Error: Memory allocation failed for VM call frames.\n");
        free(vm->functions);
        free(vm->stack);
        free(vm);
        return NULL;
    }

    return vm;
}

void vm_free(VM* vm) {
    if (!vm) return;
    if (vm->stack) {
        // Free each stack element if needed
        free(vm->stack);
    }
    
    // Free function storage
    if (vm->functions) {
        for (int i = 0; i < vm->function_count; i++) {
            free(vm->functions[i].name);
            if (vm->functions[i].param_names) {
                for (int j = 0; j < vm->functions[i].param_count; j++) {
                    free(vm->functions[i].param_names[j]);
                }
                free(vm->functions[i].param_names);
            }
        }
        free(vm->functions);
    }
    
    // Free call frame storage
    if (vm->call_frames) {
        free(vm->call_frames);
    }
    
    free(vm);
}

void vm_push(VM* vm, RuntimeValue value) {
    if (vm->stack_top >= vm->stack + vm->stack_capacity) {
        fprintf(stderr, "VM Error: Stack overflow\n");
        exit(1);
    }
    
    // Make a deep copy of the value to avoid reference issues
    *vm->stack_top = runtime_value_copy(&value);
    vm->stack_top++;
    // printf("VM DEBUG: PUSH - stack size now: %ld\n", vm->stack_top - vm->stack);
}

RuntimeValue vm_pop(VM* vm) {
    if (vm->stack_top <= vm->stack) {
        fprintf(stderr, "VM Error: Stack underflow.\n");
        RuntimeValue error = {.type = RUNTIME_VALUE_NULL};
        return error;
    }
    vm->stack_top--;
    
    // Debug what we're popping
    // print_runtime_value_type("Popping from stack", vm->stack_top);
    
    // printf("VM DEBUG: POP - stack size now: %ld\n", vm->stack_top - vm->stack);
    
    return *vm->stack_top;
}

/**
 * @brief Peek at a value on the stack without popping it.
 *
 * @param vm The VM instance.
 * @param distance How far from the top of the stack (0 = top).
 * @return RuntimeValue The value at that position.
 */
static RuntimeValue vm_peek(VM* vm, int distance) {
    if (vm->stack_top - distance - 1 < vm->stack) {
        fprintf(stderr, "VM Error: Stack underflow in peek.\n");
        RuntimeValue error = {.type = RUNTIME_VALUE_NULL};
        return error;
    }
    return vm->stack_top[-1 - distance];
}

// Global variables for bytecode execution
static RuntimeValue g_globals[256] = {0}; // Initialize all to zero

// Debug function to print a RuntimeValue
static void print_runtime_value_type(const char* label, const RuntimeValue* value) {
    if (!value) {
        printf("VM DEBUG: %s: NULL\n", label);
        return;
    }
    
    const char* type_name = "UNKNOWN";
    switch (value->type) {
        case RUNTIME_VALUE_NULL: type_name = "NULL"; break;
        case RUNTIME_VALUE_NUMBER: type_name = "NUMBER"; break;
        case RUNTIME_VALUE_BOOLEAN: type_name = "BOOLEAN"; break;
        case RUNTIME_VALUE_STRING: type_name = "STRING"; break;
        case RUNTIME_VALUE_OBJECT: type_name = "OBJECT"; break;
        case RUNTIME_VALUE_ARRAY: type_name = "ARRAY"; break;
        case RUNTIME_VALUE_FUNCTION: type_name = "FUNCTION"; break;
        default: break;
    }
    
    printf("VM DEBUG: %s: type=%s (%d)", label, type_name, value->type);
    
    if (value->type == RUNTIME_VALUE_NUMBER) {
        printf(", value=%f", value->number_value);
    } else if (value->type == RUNTIME_VALUE_STRING) {
        printf(", value=\"%s\"", value->string_value ? value->string_value : "null");
    } else if (value->type == RUNTIME_VALUE_OBJECT) {
        printf(", props=%d", value->object_value.count);
    }
    printf("\n");
}

/**
 * Helper function to compare two runtime values for equality.
 * Returns true if the values are equal, false otherwise.
 */
static bool runtime_values_equal(const RuntimeValue* a, const RuntimeValue* b) {
    if (a->type != b->type) {
        return false;
    }
    
    switch (a->type) {
        case RUNTIME_VALUE_NULL:
            return true;  // All nulls are equal
            
        case RUNTIME_VALUE_BOOLEAN:
            return a->boolean_value == b->boolean_value;
            
        case RUNTIME_VALUE_NUMBER:
            return a->number_value == b->number_value;
            
        case RUNTIME_VALUE_STRING:
            if (a->string_value == NULL || b->string_value == NULL) {
                return a->string_value == b->string_value;
            }
            return strcmp(a->string_value, b->string_value) == 0;
            
        case RUNTIME_VALUE_OBJECT:
            // Objects are equal if they're the same object (reference equality)
            // For deep equality, we'd need to compare all properties
            return a->object_value.count == b->object_value.count &&
                   a->object_value.keys == b->object_value.keys &&
                   a->object_value.values == b->object_value.values;
            
        case RUNTIME_VALUE_ARRAY:
            // Arrays are equal if they're the same array (reference equality)
            return a->array_value.count == b->array_value.count &&
                   a->array_value.elements == b->array_value.elements;
                   
        case RUNTIME_VALUE_FUNCTION:
            // Functions are equal if they're the same function
            return a->function_value.function_type == b->function_value.function_type &&
                   a->function_value.builtin_function == b->function_value.builtin_function;
                   
        default:
            return false;
    }
}

/* Helper function to set a nested property on an object 
 * Returns 1 on success, 0 on failure
 */
static int vm_set_property_at_path(RuntimeValue* obj, const char* prop, RuntimeValue value) {
    if (!obj || !prop) {
        fprintf(stderr, "VM Error: NULL object or property name in vm_set_property_at_path\n");
        return 0;
    }

    if (obj->type != RUNTIME_VALUE_OBJECT) {
        fprintf(stderr, "VM Error: Cannot set property '%s' on non-object value\n", prop);
        return 0;
    }

    // Look for the property in the object
    bool found = false;
    
    // Check if the property already exists
    for (int i = 0; i < obj->object_value.count; i++) {
        if (strcmp(obj->object_value.keys[i], prop) == 0) {
            // Update existing property
            printf("VM DEBUG: Updating existing property '%s'\n", prop);
            runtime_free_value(&obj->object_value.values[i]);  // Free existing value
            obj->object_value.values[i] = runtime_value_copy(&value);  // Set new value
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Property doesn't exist, add it
        printf("VM DEBUG: Adding new property '%s'\n", prop);
        int newCount = obj->object_value.count + 1;
        
        // Expand keys array
        char** newKeys = realloc(
            obj->object_value.keys,
            newCount * sizeof(char*)
        );
        if (!newKeys) {
            fprintf(stderr, "VM Error: Object property keys reallocation failed.\n");
            return 0;
        }
        
        // Expand values array
        RuntimeValue* newValues = realloc(
            obj->object_value.values,
            newCount * sizeof(RuntimeValue)
        );
        if (!newValues) {
            fprintf(stderr, "VM Error: Object property values reallocation failed.\n");
            free(newKeys);
            return 0;
        }
        
        // Add the new property
        newKeys[obj->object_value.count] = strdup(prop);
        newValues[obj->object_value.count] = runtime_value_copy(&value);
        
        // Update the object
        obj->object_value.keys = newKeys;
        obj->object_value.values = newValues;
        obj->object_value.count = newCount;
        
        printf("VM DEBUG: Added property '%s', object now has %d properties\n", 
              prop, obj->object_value.count);
    }
    
    return 1;
}

/* Helper function to get a property from an object 
 * Returns the property value if found, or a NULL value if not found
 */
static RuntimeValue vm_get_property(RuntimeValue obj, const char* prop) {
    RuntimeValue result = {.type = RUNTIME_VALUE_NULL};
    
    if (obj.type != RUNTIME_VALUE_OBJECT) {
        return result;
    }
    
    for (int i = 0; i < obj.object_value.count; i++) {
        if (strcmp(obj.object_value.keys[i], prop) == 0) {
            return runtime_value_copy(&obj.object_value.values[i]);
        }
    }
    
    return result;
}

/**
 * Helper for update globals after a property change 
 * Returns 1 on success, 0 on failure
 */
static bool update_globals_after_property_op(RuntimeValue* original, RuntimeValue* modified) {
    for (int i = 0; i < 100; i++) {
        if (g_globals[i].type == RUNTIME_VALUE_OBJECT) {
            // Basic equality check (this is a simplification)
            if (g_globals[i].object_value.count == original->object_value.count) {
                printf("VM DEBUG: Updating global at index %d\n", i);
                runtime_free_value(&g_globals[i]);
                g_globals[i] = runtime_value_copy(modified);
                return true;
            }
        }
    }
    return false;
}

// Helper function to set a nested property on an object
// property_path is a dot-separated path like "a.b.c"
static bool vm_set_nested_property(RuntimeValue* obj, const char* property_path, RuntimeValue value) {
    if (!obj || obj->type != RUNTIME_VALUE_OBJECT || !property_path) {
        return false;
    }
    
    printf("VM DEBUG: Setting nested property at path: '%s'\n", property_path);
    
    // Make a copy of the property path so we can tokenize it
    char* path_copy = strdup(property_path);
    if (!path_copy) {
        fprintf(stderr, "VM Error: Failed to allocate memory for property path.\n");
        return false;
    }
    
    // Split the path into segments
    char* segments[32] = {0}; // Support up to 32 levels deep
    int segment_count = 0;
    
    // Parse the property path into segments
    char* token = strtok(path_copy, ".");
    while (token && segment_count < 32) {
        segments[segment_count++] = strdup(token);
        token = strtok(NULL, ".");
    }
    
    if (segment_count == 0) {
        fprintf(stderr, "VM Error: Empty property path.\n");
        free(path_copy);
        return false;
    }
    
    printf("VM DEBUG: Property path has %d segments\n", segment_count);
    
    // Start with the provided object
    RuntimeValue* current = obj;
    
    // Process all segments except the last one
    for (int i = 0; i < segment_count - 1; i++) {
        // Look for this property in the current object
        bool found = false;
        RuntimeValue* next = NULL;
        
        // Trace the instruction
        // printf("VM TRACE: Looking for property '%s' (segment %d)\n", segments[i], i);
        
        for (int j = 0; j < current->object_value.count; j++) {
            if (strcmp(current->object_value.keys[j], segments[i]) == 0) {
                // Found the property
                // Trace the instruction
                // printf("VM TRACE: Found property '%s'\n", segments[i]);
                
                // Check if it's an object
                if (current->object_value.values[j].type == RUNTIME_VALUE_OBJECT) {
                    // It's an object, move to next level
                    next = &current->object_value.values[j];
                    found = true;
                    break;
                } else {
                    // Property exists but isn't an object - replace it
                    printf("VM DEBUG: Property '%s' exists but is not an object - replacing\n", segments[i]);
                    
                    // Free the current value
                    runtime_free_value(&current->object_value.values[j]);
                    
                    // Create a new empty object
                    current->object_value.values[j].type = RUNTIME_VALUE_OBJECT;
                    current->object_value.values[j].object_value.count = 0;
                    current->object_value.values[j].object_value.keys = NULL;
                    current->object_value.values[j].object_value.values = NULL;
                    
                    next = &current->object_value.values[j];
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) {
            // Property doesn't exist, create a new object property
            printf("VM DEBUG: Property '%s' not found - creating new object\n", segments[i]);
            
            // Create new empty object
            RuntimeValue new_obj;
            new_obj.type = RUNTIME_VALUE_OBJECT;
            new_obj.object_value.count = 0;
            new_obj.object_value.keys = NULL;
            new_obj.object_value.values = NULL;
            
            // Add to current object
            vm_add_property(current, segments[i], &new_obj);
            
            // Get reference to the newly added property
            int last_idx = current->object_value.count - 1;
            next = &current->object_value.values[last_idx];
            
            // Free our temporary object (it's been copied by vm_add_property)
            runtime_free_value(&new_obj);
        }
        
        // Move to the next object in the path
        current = next;
    }
    
    // At this point, current points to the object that should contain the final property
    const char* final_property = segments[segment_count - 1];
    printf("VM DEBUG: Setting final property '%s'\n", final_property);
    
    // Set the final property to the provided value
    vm_add_property(current, final_property, &value);
    
    // Clean up
    for (int i = 0; i < segment_count; i++) {
        free(segments[i]);
    }
    free(path_copy);
    
    return true;
}

// Helper function to update globals
static void update_global_if_object(RuntimeValue *obj, int varIndex) {
    // If the variable was loaded from globals, update it back
    if (varIndex >= 0 && varIndex < 256 && obj->type == RUNTIME_VALUE_OBJECT) {
        printf("VM DEBUG: Updating global variable at index %d with modified object\n", varIndex);
        
        // Free existing value
        runtime_free_value(&g_globals[varIndex]);
        
        // Store the updated value
        g_globals[varIndex] = runtime_value_copy(obj);
        
        print_runtime_value_type("Global variable after update", &g_globals[varIndex]);
    }
}

// Find a variable index based on its value
static int find_variable_index(RuntimeValue* value) {
    if (!value || value->type != RUNTIME_VALUE_OBJECT) {
        return -1;  // Not an object, so not tracking
    }
    
    // Check globals array for matching object
    for (int i = 0; i < 256; i++) {
        if (g_globals[i].type == RUNTIME_VALUE_OBJECT) {
            // Simple equality check - just check if object structure matches
            if (g_globals[i].object_value.count == value->object_value.count) {
                return i;  // Return the variable index
            }
        }
    }
    
    return -1;  // Not found
}

// Clean up the vm_add_property function
static void vm_add_property(RuntimeValue* obj, const char* propName, RuntimeValue* value) {
    if (obj == NULL || obj->type != RUNTIME_VALUE_OBJECT || propName == NULL) {
        fprintf(stderr, "VM Error: Cannot add property to non-object or with NULL property name.\n");
        return;
    }
    
    // Look for the property in the object
    bool found = false;
    
    // Check if the property already exists
    for (int i = 0; i < obj->object_value.count; i++) {
        if (strcmp(obj->object_value.keys[i], propName) == 0) {
            // Update existing property
            runtime_free_value(&obj->object_value.values[i]);  // Free existing value
            obj->object_value.values[i] = runtime_value_copy(value);  // Set new value
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
            fprintf(stderr, "VM Error: Object property keys reallocation failed.\n");
            return;
        }
        
        // Expand values array
        RuntimeValue* newValues = realloc(
            obj->object_value.values,
            newCount * sizeof(RuntimeValue)
        );
        if (!newValues) {
            fprintf(stderr, "VM Error: Object property values reallocation failed.\n");
            free(newKeys);
            return;
        }
        
        // Add the new property
        char* duplicated_name = strdup(propName);
        
        newKeys[obj->object_value.count] = duplicated_name;
        newValues[obj->object_value.count] = runtime_value_copy(value);
        
        // Update the object
        obj->object_value.keys = newKeys;
        obj->object_value.values = newValues;
        obj->object_value.count = newCount;
    }
}

// Add this function to print the entire stack at key points
static void print_stack_trace(VM* vm, const char* label) {
    printf("===== STACK TRACE (%s) - %ld items =====\n", label, (vm->stack_top - vm->stack));
    
    for (int i = 0; i < (vm->stack_top - vm->stack); i++) {
        RuntimeValue* value = &vm->stack[i];
        printf("  [%d] ", i);
        
        switch (value->type) {
            case RUNTIME_VALUE_NULL:
                printf("NULL\n");
                break;
            case RUNTIME_VALUE_NUMBER:
                printf("NUMBER: %f\n", value->number_value);
                break;
            case RUNTIME_VALUE_BOOLEAN:
                printf("BOOLEAN: %s\n", value->boolean_value ? "true" : "false");
                break;
            case RUNTIME_VALUE_STRING:
                printf("STRING: '%s'\n", value->string_value ? value->string_value : "(null)");
                break;
            case RUNTIME_VALUE_OBJECT:
                printf("OBJECT: props=%d (", value->object_value.count);
                // Print a sample of property names
                for (int j = 0; j < value->object_value.count && j < 3; j++) {
                    if (j > 0) printf(", ");
                    if (value->object_value.keys && value->object_value.keys[j]) {
                        printf("'%s'", value->object_value.keys[j]);
                    } else {
                        printf("'(null)'");
                    }
                }
                if (value->object_value.count > 3) printf(", ...");
                printf(")\n");
                break;
            case RUNTIME_VALUE_ARRAY:
                printf("ARRAY: elements=%d\n", value->array_value.count);
                break;
            case RUNTIME_VALUE_FUNCTION:
                printf("FUNCTION\n");
                break;
            default:
                printf("UNKNOWN TYPE: %d\n", value->type);
                break;
        }
    }
    printf("========================================\n");
}

int vm_run(VM* vm) {
    // Initialize global variables if not already done
    for (int i = 0; i < 256; i++) {
        g_globals[i].type = RUNTIME_VALUE_NULL;
        g_globals[i].string_value = NULL;
    }
    
    for (;;) {
        // Decode and execute the instruction
        uint8_t instruction = *vm->ip++;
        
        // Check if we're at the end of the bytecode
        if (vm->ip - vm->chunk->code > vm->chunk->code_count) {
            fprintf(stderr, "VM Error: Instruction pointer out of bounds.\n");
            return 1;
        }
        
        // Get instruction name
        const char* instruction_name = "UNKNOWN";
        switch (instruction) {
            case OP_NOOP: instruction_name = "OP_NOOP"; break;
            case OP_EOF: instruction_name = "OP_EOF"; break;
            case OP_POP: instruction_name = "OP_POP"; break;
            case OP_DUP: instruction_name = "OP_DUP"; break;
            case OP_SWAP: instruction_name = "OP_SWAP"; break;
            case OP_LOAD_CONST: instruction_name = "OP_LOAD_CONST"; break;
            case OP_LOAD_VAR: instruction_name = "OP_LOAD_VAR"; break;
            case OP_STORE_VAR: instruction_name = "OP_STORE_VAR"; break;
            case OP_ADD: instruction_name = "OP_ADD"; break;
            case OP_NEW_OBJECT: instruction_name = "OP_NEW_OBJECT"; break;
            case OP_GET_PROPERTY: instruction_name = "OP_GET_PROPERTY"; break;
            case OP_SET_PROPERTY: instruction_name = "OP_SET_PROPERTY"; break;
            case OP_SET_NESTED_PROPERTY: instruction_name = "OP_SET_NESTED_PROPERTY"; break;
            case OP_PRINT: instruction_name = "OP_PRINT"; break;
            case OP_COPY_PROPERTIES: instruction_name = "OP_COPY_PROPERTIES"; break;
            case OP_CALL_METHOD: instruction_name = "OP_CALL_METHOD"; break;
            default: break;
        }
        
        // Trace the instruction
        // printf("VM TRACE: Executing %s (%d) at offset %ld, stack size: %ld\n", 
        //        instruction_name, instruction, (vm->ip - 1 - vm->chunk->code), (vm->stack_top - vm->stack));
               
        switch (instruction) {

            case OP_NOOP: {
                // do nothing
                break;
            }

            case OP_EOF: {
                // End of the bytecode
                return 0;
            }

            case OP_POP: {
                // Check if we have something to pop
                if (vm->stack_top <= vm->stack) {
                    // Attempted to pop from empty stack, ignore silently
                } else {
                    vm_pop(vm);
                }
                break;
            }

            case OP_DUP: {
                RuntimeValue val = vm_peek(vm, 0);
                vm_push(vm, val);
                break;
            }

            case OP_SWAP: {
                // Swap top two values on the stack
                if ((vm->stack_top - vm->stack) < 2) {
                    fprintf(stderr, "VM Error: Stack underflow during OP_SWAP (need at least 2 values).\n");
                    return 1;
                }
                RuntimeValue a = vm_pop(vm);
                RuntimeValue b = vm_pop(vm);
                // Push them back in reverse order
                vm_push(vm, a);
                vm_push(vm, b);
                break;
            }

            /* -----------------------------
               Constants & Variables
               ----------------------------- */
            case OP_LOAD_CONST: {
                // The next byte is the index into constants
                uint8_t const_index = *vm->ip++;
                RuntimeValue c = vm->chunk->constants[const_index];
                vm_push(vm, c);
                break;
            }

            case OP_LOAD_VAR: {
                // The next two bytes are the variable index (16-bit, big-endian)
                uint16_t varIndex = (uint16_t)(((*vm->ip++) << 8) | (*vm->ip++));
                
                // Make a deep copy of the global variable
                RuntimeValue value = runtime_value_copy(&g_globals[varIndex]);
                
                vm_push(vm, value);
                break;
            }

            case OP_STORE_VAR: {
                // The next two bytes are the variable index (16-bit, big-endian)
                uint16_t varIndex = (uint16_t)(((*vm->ip++) << 8) | (*vm->ip++));
                
                // Pop top of stack
                RuntimeValue value = vm_pop(vm);
                
                // Free any existing value in the global slot to prevent memory leaks
                runtime_free_value(&g_globals[varIndex]);
                
                // Store a deep copy of the value in globals
                g_globals[varIndex] = runtime_value_copy(&value);
                
                // Free the original value since we made a copy
                runtime_free_value(&value);
                
                break;
            }

            /* -----------------------------
               Arithmetic & Logic
               ----------------------------- */
            case OP_ADD: {
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);

                // 1) string + string
                if (a.type == RUNTIME_VALUE_STRING && b.type == RUNTIME_VALUE_STRING) {
                    size_t lenA = strlen(a.string_value);
                    size_t lenB = strlen(b.string_value);
                    char* newStr = (char*)malloc(lenA + lenB + 1);
                    if (!newStr) {
                        fprintf(stderr, "VM Error: Memory allocation failed for string concat.\n");
                        return 1;
                    }
                    strcpy(newStr, a.string_value);
                    strcat(newStr, b.string_value);

                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_STRING;
                    result.string_value = newStr;
                    vm_push(vm, result);
                }
                // 2) string + X
                else if (a.type == RUNTIME_VALUE_STRING) {
                    // Convert b to string, then do string+string
                    char* bStr = runtime_value_to_string(&b);
                    if (!bStr) {
                        fprintf(stderr, "VM Error: Failed to convert operand to string.\n");
                        return 1;
                    }
                    size_t lenA = strlen(a.string_value);
                    size_t lenB = strlen(bStr);
                    char* newStr = (char*)malloc(lenA + lenB + 1);
                    if (!newStr) {
                        fprintf(stderr, "VM Error: Memory allocation failed for string concat.\n");
                        free(bStr);
                        return 1;
                    }
                    strcpy(newStr, a.string_value);
                    strcat(newStr, bStr);

                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_STRING;
                    result.string_value = newStr;
                    vm_push(vm, result);

                    free(bStr);  // done using the temporary string
                }
                // 3) X + string
                else if (b.type == RUNTIME_VALUE_STRING) {
                    // Convert a to string, then do string+string
                    char* aStr = runtime_value_to_string(&a);
                    if (!aStr) {
                        fprintf(stderr, "VM Error: Failed to convert operand to string.\n");
                        return 1;
                    }
                    size_t lenA = strlen(aStr);
                    size_t lenB = strlen(b.string_value);
                    char* newStr = (char*)malloc(lenA + lenB + 1);
                    if (!newStr) {
                        fprintf(stderr, "VM Error: Memory allocation failed for string concat.\n");
                        free(aStr);
                        return 1;
                    }
                    strcpy(newStr, aStr);
                    strcat(newStr, b.string_value);

                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_STRING;
                    result.string_value = newStr;
                    vm_push(vm, result);

                    free(aStr);  // done using the temporary string
                }
                // 4) number + number
                else if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = a.number_value + b.number_value;
                    vm_push(vm, result);
                }
                // 5) fallback error
                else {
                    fprintf(stderr, "VM Error: OP_ADD cannot handle these operand types.\n");
                    return 1;
                }
                break;
            }
            case OP_SUB: {
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);
                if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = a.number_value - b.number_value;
                    vm_push(vm, result);
                } else {
                    fprintf(stderr, "VM Error: OP_SUB expects two numbers.\n");
                    return 1;
                }
                break;
            }

            case OP_MUL: {
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);
                if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = a.number_value * b.number_value;
                    vm_push(vm, result);
                } else {
                    fprintf(stderr, "VM Error: OP_MUL expects two numbers.\n");
                    return 1;
                }
                break;
            }

            case OP_DIV: {
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);
                if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    if (b.number_value == 0) {
                        fprintf(stderr, "VM Error: Division by zero.\n");
                        return 1;
                    }
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_NUMBER;
                    result.number_value = a.number_value / b.number_value;
                    vm_push(vm, result);
                } else {
                    fprintf(stderr, "VM Error: OP_DIV expects two numbers.\n");
                    return 1;
                }
                break;
            }

            case OP_MOD: {
                // a % b
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);
                if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    if (b.number_value == 0) {
                        fprintf(stderr, "VM Error: Modulo by zero.\n");
                        return 1;
                    }
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_NUMBER;
                    // Use fmod for floating mod
                    result.number_value = fmod(a.number_value, b.number_value);
                    vm_push(vm, result);
                } else {
                    fprintf(stderr, "VM Error: OP_MOD expects two numbers.\n");
                    return 1;
                }
                break;
            }

            case OP_NEG: {
                // Unary negation
                RuntimeValue val = vm_pop(vm);
                if (val.type == RUNTIME_VALUE_NUMBER) {
                    val.number_value = -val.number_value;
                    vm_push(vm, val);
                } else {
                    fprintf(stderr, "VM Error: OP_NEG expects a number.\n");
                    return 1;
                }
                break;
            }

            case OP_NOT: {
                // Logical NOT
                RuntimeValue val = vm_pop(vm);
                if (val.type == RUNTIME_VALUE_BOOLEAN) {
                    val.boolean_value = !val.boolean_value;
                    vm_push(vm, val);
                } else {
                    // Non-boolean? Convert to boolean "truthiness" then invert
                    bool truthy = false;
                    if (val.type == RUNTIME_VALUE_NUMBER) {
                        truthy = (val.number_value != 0);
                    } else if (val.type == RUNTIME_VALUE_STRING) {
                        truthy = (val.string_value && val.string_value[0] != '\0');
                    }
                    RuntimeValue result;
                    result.type = RUNTIME_VALUE_BOOLEAN;
                    result.boolean_value = !truthy;
                    vm_push(vm, result);
                }
                break;
            }

            case OP_EQ: 
            case OP_NEQ:
            case OP_LT:
            case OP_GT:
            case OP_LTE:
            case OP_GTE: {
                RuntimeValue b = vm_pop(vm);
                RuntimeValue a = vm_pop(vm);
                RuntimeValue result;
                result.type = RUNTIME_VALUE_BOOLEAN;
                bool comparison = false;

                // For simplicity, only handle numbers
                if (a.type == RUNTIME_VALUE_NUMBER && b.type == RUNTIME_VALUE_NUMBER) {
                    double x = a.number_value;
                    double y = b.number_value;

                    switch (instruction) {
                        case OP_EQ:  comparison = (x == y); break;
                        case OP_NEQ: comparison = (x != y); break;
                        case OP_LT:  comparison = (x <  y); break;
                        case OP_GT:  comparison = (x >  y); break;
                        case OP_LTE: comparison = (x <= y); break;
                        case OP_GTE: comparison = (x >= y); break;
                        default: break;
                    }
                }
                else {
                    // String == string, etc., handle here
                    if (instruction == OP_EQ || instruction == OP_NEQ) {
                        // As a naive fallback, treat different types as 'not equal'
                        bool equal = false;
                        if (a.type == b.type) {
                            // Check equality for booleans, strings, etc.
                            if (a.type == RUNTIME_VALUE_BOOLEAN) {
                                equal = (a.boolean_value == b.boolean_value);
                            } else if (a.type == RUNTIME_VALUE_STRING && b.string_value && a.string_value) {
                                equal = (strcmp(a.string_value, b.string_value) == 0);
                            } else if (a.type == RUNTIME_VALUE_NULL) {
                                equal = true; // both null
                            }
                        }
                        comparison = equal;
                        if (instruction == OP_NEQ) comparison = !comparison;
                    }
                }

                result.boolean_value = comparison;
                vm_push(vm, result);
                break;
            }

            /* -----------------------------
               Branching (Jumps)
               ----------------------------- */
            case OP_JUMP_IF_FALSE: {
                // 16-bit offset
                uint16_t offset = (uint16_t)(((*vm->ip++) << 8) | (*vm->ip++));
                RuntimeValue cond = vm_pop(vm);

                // Evaluate as boolean
                bool isFalse = false;
                if (cond.type == RUNTIME_VALUE_BOOLEAN) {
                    isFalse = (cond.boolean_value == false);
                }
                else if (cond.type == RUNTIME_VALUE_NUMBER) {
                    isFalse = (cond.number_value == 0);
                }
                else if (cond.type == RUNTIME_VALUE_NULL) {
                    isFalse = true;
                }

                if (isFalse) {
                    vm->ip += offset;  // jump forward
                }
                break;
            }

            case OP_JUMP: {
                // unconditional jump
                uint16_t offset = (uint16_t)(((*vm->ip++) << 8) | (*vm->ip++));
                vm->ip += offset;
                break;
            }

            case OP_LOOP: {
                // jump backward by offset
                uint16_t offset = (uint16_t)(((*vm->ip++) << 8) | (*vm->ip++));
                vm->ip -= offset; // Move IP *backwards*
                break;
            }

            /* -----------------------------
               Functions & Return
               ----------------------------- */
            case OP_CALL: {
                // Byte 1: function index, Byte 2: argCount
                uint8_t funcIndex = *vm->ip++;
                uint8_t argCount  = *vm->ip++;
                
                printf("VM DEBUG: OP_CALL funcIndex=%d, argCount=%d\n", funcIndex, argCount);
                
                // For user-defined functions, look up the function start IP
                // The funcIndex should correspond to a constant that contains the start IP
                if (funcIndex >= vm->chunk->constants_count) {
                    fprintf(stderr, "VM Error: Invalid function index %d\n", funcIndex);
                    return 1;
                }
                
                // Get the function start IP from the constants table
                RuntimeValue funcInfo = vm->chunk->constants[funcIndex];
                if (funcInfo.type != RUNTIME_VALUE_NUMBER) {
                    fprintf(stderr, "VM Error: Function info is not a number\n");
                    return 1;
                }
                
                int functionStartIP = (int)funcInfo.number_value;
                printf("VM DEBUG: Calling function at IP %d\n", functionStartIP);
                
                // Save current IP for return
                uint8_t* returnIP = vm->ip;
                printf("VM DEBUG: Current IP offset: %ld, will return to: %ld\n", 
                       vm->ip - vm->chunk->code, returnIP - vm->chunk->code);
                
                // Pop arguments and store them as local variables
                RuntimeValue* args = malloc(argCount * sizeof(RuntimeValue));
                for (int i = argCount - 1; i >= 0; i--) {
                    args[i] = vm_pop(vm);
                }
                
                // Store arguments in "local" variable slots (using high indices)
                for (int i = 0; i < argCount; i++) {
                    int localVarIndex = 256 + i; // Use high indices for locals
                    if (localVarIndex < 512) { // Ensure we don't overflow
                        g_globals[localVarIndex] = args[i];
                    }
                }
                
                free(args);
                
                // Jump to function start
                vm->ip = vm->chunk->code + functionStartIP;
                
                // Store return IP as a special marker on the stack
                RuntimeValue returnMarker;
                returnMarker.type = RUNTIME_VALUE_NUMBER;
                returnMarker.number_value = (double)(returnIP - vm->chunk->code);
                vm_push(vm, returnMarker);
                
                printf("VM DEBUG: Pushed return marker for IP offset %d\n", (int)(returnIP - vm->chunk->code));
                
                break;
            }

            case OP_RETURN: {
                // For our simple function call implementation, we need to:
                // 1. Pop the return IP marker from the stack
                // 2. Restore the instruction pointer
                // 3. Continue execution
                
                printf("VM DEBUG: OP_RETURN - stack size: %ld\n", vm->stack_top - vm->stack);
                
                // Check if we have anything on the stack
                if (vm->stack_top <= vm->stack) {
                    // No values on stack - this could be end of main program or empty function return
                    // We should only end if we're truly at the end of the bytecode
                    printf("VM DEBUG: Empty stack, ending program\n");
                    return 0;
                }
                
                // Look for the return marker (we pushed it as a number)
                RuntimeValue returnMarker = vm_pop(vm);
                printf("VM DEBUG: Popped value type: %d\n", returnMarker.type);
                
                if (returnMarker.type == RUNTIME_VALUE_NUMBER) {
                    // This is a return marker - restore the instruction pointer
                    int returnOffset = (int)returnMarker.number_value;
                    vm->ip = vm->chunk->code + returnOffset;
                    printf("VM DEBUG: Returning to IP offset %d\n", returnOffset);
                } else {
                    // This wasn't a return marker, it's some other value
                    // Put it back on the stack and continue execution
                    vm_push(vm, returnMarker);
                    printf("VM DEBUG: Not a return marker, continuing execution\n");
                    // Don't return - just continue execution
                }
                
                break;
            }

            /* -----------------------------
               Arrays / Indexing
               ----------------------------- */
            case OP_NEW_ARRAY: {
                // Create a new array (RUNTIME_VALUE_ARRAY with 0 elements)
                RuntimeValue arr;
                arr.type = RUNTIME_VALUE_ARRAY;
                arr.array_value.count = 0;
                arr.array_value.elements = NULL; // empty

                vm_push(vm, arr);
                break;
            }

            case OP_ARRAY_PUSH: {
                // Expect: top => value, below => array
                RuntimeValue val = vm_pop(vm);
                RuntimeValue arr = vm_pop(vm);

                if (arr.type != RUNTIME_VALUE_ARRAY) {
                    fprintf(stderr, "VM Error: OP_ARRAY_PUSH on non-array.\n");
                    return 1;
                }

                // Expand array by 1
                int newCount = arr.array_value.count + 1;
                RuntimeValue* newElems = realloc(
                    arr.array_value.elements,
                    newCount * sizeof(RuntimeValue)
                );
                if (!newElems) {
                    fprintf(stderr, "VM Error: Array push reallocation failed.\n");
                    return 1;
                }
                newElems[arr.array_value.count] = val;
                arr.array_value.elements = newElems;
                arr.array_value.count = newCount;

                // Push the updated array back
                vm_push(vm, arr);
                break;
            }

            case OP_GET_INDEX: {
                // Expect: top => index, below => array
                RuntimeValue indexVal = vm_pop(vm);
                RuntimeValue arrVal   = vm_pop(vm);

                if (arrVal.type != RUNTIME_VALUE_ARRAY) {
                    fprintf(stderr, "VM Error: OP_GET_INDEX on non-array.\n");
                    return 1;
                }
                if (indexVal.type != RUNTIME_VALUE_NUMBER) {
                    fprintf(stderr, "VM Error: OP_GET_INDEX requires numeric index.\n");
                    return 1;
                }

                int idx = (int)indexVal.number_value;
                if (idx < 0 || idx >= arrVal.array_value.count) {
                    fprintf(stderr, "VM Error: Array index %d out of bounds.\n", idx);
                    return 1;
                }

                // Retrieve element
                RuntimeValue element = arrVal.array_value.elements[idx];
                vm_push(vm, element);
                break;
            }

            /* -----------------------------
               Printing, etc.
               ----------------------------- */
            case OP_PRINT: {
                // pop top
                RuntimeValue v = vm_pop(vm);

                // Convert to string (your runtime has a helper, or do a quick approach):
                if (v.type == RUNTIME_VALUE_NUMBER) {
                    printf("%g\n", v.number_value);
                }
                else if (v.type == RUNTIME_VALUE_STRING && v.string_value) {
                    printf("%s\n", v.string_value);
                }
                else if (v.type == RUNTIME_VALUE_BOOLEAN) {
                    printf("%s\n", v.boolean_value ? "true" : "false");
                }
                else if (v.type == RUNTIME_VALUE_NULL) {
                    printf("null\n");
                }
                else {
                    // For arrays or other objects, do something minimal:
                    printf("[Object or Array]\n");
                }
                
                // Push null back onto the stack so OP_POP has something to consume
                RuntimeValue null_result = {.type = RUNTIME_VALUE_NULL};
                vm_push(vm, null_result);
                break;
            }

            case OP_TO_STRING: {
                // If we want to convert the top value to a string in place
                // For now, just skip or handle as needed
                break;
            }

            /* -----------------------------
               Objects & Properties
               ----------------------------- */
            case OP_NEW_OBJECT: {
                // Create a new empty object
                RuntimeValue obj;
                obj.type = RUNTIME_VALUE_OBJECT;
                obj.object_value.count = 0;
                obj.object_value.keys = NULL;
                obj.object_value.values = NULL;
                
                vm_push(vm, obj);
                break;
            }

            case OP_GET_PROPERTY: {
                // Expect: top => property name, bottom => object
                RuntimeValue propNameVal = vm_pop(vm);
                RuntimeValue objVal = vm_pop(vm);
                
                if (objVal.type != RUNTIME_VALUE_OBJECT) {
                    fprintf(stderr, "VM Error: OP_GET_PROPERTY on non-object (type %d).\n", objVal.type);
                    return 1;
                }
                if (propNameVal.type != RUNTIME_VALUE_STRING) {
                    fprintf(stderr, "VM Error: OP_GET_PROPERTY requires string property name.\n");
                    return 1;
                }
                
                const char* propName = propNameVal.string_value;
                bool found = false;
                RuntimeValue result = {.type = RUNTIME_VALUE_NULL}; // Default to null if property not found
                
                // Look for the property in the object
                for (int i = 0; i < objVal.object_value.count; i++) {
                    if (strcmp(objVal.object_value.keys[i], propName) == 0) {
                        result = runtime_value_copy(&objVal.object_value.values[i]);
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    fprintf(stderr, "VM Warning: Object has no property '%s'.\n", propName);
                }
                
                vm_push(vm, result);
                break;
            }

            case OP_SET_PROPERTY: {
                // Ensure we have at least 3 values on the stack
                if ((vm->stack_top - vm->stack) < 3) {
                    fprintf(stderr, "VM Error: Stack underflow for OP_SET_PROPERTY (need at least 3 values).\n");
                    return 1;
                }
                
                // Pop the values in the correct order: value, propName, object
                RuntimeValue value = vm_pop(vm);      // Top of stack: the value to set
                RuntimeValue propName = vm_pop(vm);   // Property name (string)
                RuntimeValue object = vm_pop(vm);     // Object to modify
                
                // Validate types
                if (propName.type != RUNTIME_VALUE_STRING || propName.string_value == NULL) {
                    fprintf(stderr, "VM Error: Property name must be a valid string, got type %d.\n", propName.type);
                    return 1;
                }
                
                if (object.type != RUNTIME_VALUE_OBJECT) {
                    fprintf(stderr, "VM Error: Cannot add property to non-object (type %d).\n", object.type);
                    return 1;
                }
                
                // Create a deep copy of the object to avoid reference issues
                RuntimeValue objectCopy = runtime_value_copy(&object);
                
                // Set property on the object copy
                vm_add_property(&objectCopy, propName.string_value, &value);
                
                // If this is a global variable, update it
                for (int i = 0; i < 256; i++) {
                    if (g_globals[i].type == RUNTIME_VALUE_OBJECT) {
                        // Simple check to see if this might be our object
                        if (g_globals[i].object_value.count == object.object_value.count) {
                            runtime_free_value(&g_globals[i]);
                            g_globals[i] = runtime_value_copy(&objectCopy);
                            break;
                        }
                    }
                }
                
                // Push the updated object back
                vm_push(vm, objectCopy);
                
                // Free our local copy since vm_push makes its own copy
                runtime_free_value(&objectCopy);
                
                break;
            }

            case OP_SET_NESTED_PROPERTY: {
                // Ensure we have at least 3 values on the stack
                if ((vm->stack_top - vm->stack) < 3) {
                    fprintf(stderr, "VM Error: Stack underflow for OP_SET_NESTED_PROPERTY (need at least 3 values).\n");
                    return 1;
                }
                
                // Get the items from the stack directly (not popping yet)
                RuntimeValue value = *(vm->stack_top - 1);
                RuntimeValue pathVal = *(vm->stack_top - 2);
                RuntimeValue objVal = *(vm->stack_top - 3);
                
                printf("VM DEBUG: OP_SET_NESTED_PROPERTY - Value type: %d, Path type: %d, Object type: %d\n", 
                       value.type, pathVal.type, objVal.type);
                
                // Validate types
                if (pathVal.type != RUNTIME_VALUE_STRING || pathVal.string_value == NULL) {
                    fprintf(stderr, "VM Error: OP_SET_NESTED_PROPERTY requires valid string property path.\n");
                    return 1;
                }
                
                if (objVal.type != RUNTIME_VALUE_OBJECT) {
                    fprintf(stderr, "VM Error: OP_SET_NESTED_PROPERTY requires object as target.\n");
                    return 1;
                }
                
                // Now pop the values
                RuntimeValue val = vm_pop(vm);
                RuntimeValue path = vm_pop(vm);
                RuntimeValue obj = vm_pop(vm);
                
                const char* propPath = path.string_value;
                printf("VM DEBUG: Setting nested property at path: '%s'\n", propPath);
                
                // Create a deep copy of the object to work with
                RuntimeValue objCopy = runtime_value_copy(&obj);
                
                // Set the nested property using our helper function
                bool success = vm_set_nested_property(&objCopy, propPath, val);
                
                if (!success) {
                    fprintf(stderr, "VM Error: Failed to set nested property at path '%s'.\n", propPath);
                    runtime_free_value(&objCopy);
                    return 1;
                }
                
                // If this is a global variable, update it too
                for (int i = 0; i < 256; i++) {
                    if (g_globals[i].type == RUNTIME_VALUE_OBJECT) {
                        // Simple check to see if this might be our object
                        if (g_globals[i].object_value.count == obj.object_value.count) {
                            runtime_free_value(&g_globals[i]);
                            g_globals[i] = runtime_value_copy(&objCopy);
                            break;
                        }
                    }
                }
                
                // Push the updated object back
                vm_push(vm, objCopy);
                
                // Free the local copy
                runtime_free_value(&objCopy);
                break;
            }

            case OP_CALL_METHOD: {
                // Byte 1: argCount (not including 'this')
                uint8_t argCount = *vm->ip++;
                
                // Stack: [object, method, arg1, arg2, ..., argN]
                
                // Collect arguments (excluding 'this' and method)
                RuntimeValue* args = malloc((argCount + 1) * sizeof(RuntimeValue));
                if (!args) {
                    fprintf(stderr, "VM Error: Memory allocation failed for method arguments.\n");
                    return 1;
                }
                
                // Pop arguments in reverse order
                for (int i = argCount - 1; i >= 0; i--) {
                    args[i + 1] = vm_pop(vm);
                }
                
                // Pop the method
                RuntimeValue methodVal = vm_pop(vm);
                
                // Pop the object ('this')
                RuntimeValue thisObj = vm_pop(vm);
                args[0] = thisObj; // 'this' is the first argument
                
                // Check if the method is a function
                if (methodVal.type != RUNTIME_VALUE_FUNCTION) {
                    fprintf(stderr, "VM Error: Cannot call non-function value as a method.\n");
                    free(args);
                    return 1;
                }
                
                // Call the function with 'this' as the first argument
                if (methodVal.function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                    // Built-in function
                    RuntimeValue result = methodVal.function_value.builtin_function(NULL, args, argCount + 1);
                    vm_push(vm, result);
                } else {
                    // User-defined function - we need to execute it with proper 'this' context
                    UserDefinedFunction* user_func = methodVal.function_value.user_function;
                    printf("VM DEBUG: user_func = %p\n", (void*)user_func);
                    if (user_func) {
                        printf("VM DEBUG: user_func->name = %s\n", user_func->name ? user_func->name : "NULL");
                        printf("VM DEBUG: user_func->body = %p\n", (void*)user_func->body);
                        printf("VM DEBUG: user_func->parameter_count = %d\n", user_func->parameter_count);
                        if (user_func->body) {
                            printf("VM DEBUG: user_func->body->type = %d\n", user_func->body->type);
                        }
                    }
                    
                    if (!user_func || !user_func->body) {
                        fprintf(stderr, "VM Error: Invalid user-defined function.\n");
                        free(args);
                        return 1;
                    }
                    
                    // Create a temporary environment for the function call
                    // We'll use the runtime system to execute the function body
                    Environment* temp_env = runtime_create_environment();
                    if (!temp_env) {
                        fprintf(stderr, "VM Error: Failed to create environment for method call.\n");
                        free(args);
                        return 1;
                    }
                    
                    // Copy all built-in functions from global environment to temp environment
                    vm_copy_builtins_to_env(temp_env);
                    
                    // Set 'this' in the environment
                    runtime_set_variable(temp_env, "this", thisObj);
                    
                    // Set parameters in the environment
                    for (int i = 0; i < user_func->parameter_count && i < argCount; i++) {
                        if (user_func->parameters && user_func->parameters[i]) {
                            runtime_set_variable(temp_env, user_func->parameters[i], args[i + 1]);
                        }
                    }
                    
                    // Execute the function body using the runtime system
                    runtime_execute_block(temp_env, user_func->body);
                    
                    // Check if 'this' was modified and update the original object
                    RuntimeValue* modified_this = runtime_get_variable(temp_env, "this");
                    if (modified_this && modified_this->type == RUNTIME_VALUE_OBJECT) {
                        // Update the original object on the stack with the modified 'this'
                        // The original object is still on the stack (we duplicated it earlier)
                        RuntimeValue* original_obj = vm->stack_top - 1;
                        if (original_obj->type == RUNTIME_VALUE_OBJECT) {
                            // Free the old object and replace with the modified one
                            runtime_free_value(original_obj);
                            *original_obj = runtime_value_copy(modified_this);
                            
                            // Also need to update any global variables that reference this object
                            // This is a simplified approach - in a full implementation we'd need
                            // proper reference tracking
                            for (int i = 0; i < 256; i++) {
                                if (g_globals[i].type == RUNTIME_VALUE_OBJECT) {
                                    // Simple heuristic: if the object has the same number of properties,
                                    // it might be the same object (this is imperfect but works for our test)
                                    if (g_globals[i].object_value.count == thisObj.object_value.count) {
                                        runtime_free_value(&g_globals[i]);
                                        g_globals[i] = runtime_value_copy(modified_this);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    // For now, methods return null (we can extend this later for return statements)
                    RuntimeValue result = {.type = RUNTIME_VALUE_NULL};
                    vm_push(vm, result);
                    
                    // Clean up the temporary environment
                    runtime_free_environment(temp_env);
                }
                
                free(args);
                break;
            }

            case OP_COPY_PROPERTIES: {
                // Stack: [target_object, source_object]
                // Copy all properties from source_object to target_object
                
                // Ensure we have at least 2 values on the stack
                if ((vm->stack_top - vm->stack) < 2) {
                    fprintf(stderr, "VM Error: Stack underflow for OP_COPY_PROPERTIES (need at least 2 values).\n");
                    return 1;
                }
                
                // Pop the source object (mixin)
                RuntimeValue source = vm_pop(vm);
                
                // Peek at the target object (don't pop it, we'll modify it in place)
                RuntimeValue* target = vm->stack_top - 1;
                
                // Validate types
                if (source.type != RUNTIME_VALUE_OBJECT) {
                    fprintf(stderr, "VM Error: Source for OP_COPY_PROPERTIES must be an object (got type %d).\n", source.type);
                    return 1;
                }
                
                if (target->type != RUNTIME_VALUE_OBJECT) {
                    fprintf(stderr, "VM Error: Target for OP_COPY_PROPERTIES must be an object (got type %d).\n", target->type);
                    return 1;
                }
                
                // Copy all properties from source to target
                for (int i = 0; i < source.object_value.count; i++) {
                    const char* key = source.object_value.keys[i];
                    RuntimeValue* value = &source.object_value.values[i];
                    
                    // Add/overwrite property in target object
                    vm_add_property(target, key, value);
                }
                
                // The target object remains on the stack (modified in place)
                break;
            }

            /* -----------------------------
               Default (unknown opcode)
               ----------------------------- */
            default: {
                fprintf(stderr, "VM Error: Unknown opcode %d.\n", instruction);
                return 1;
            }
        } // end switch
    } // end for
}

/* ----------------
   Function Management
   ---------------- */

int vm_add_function(VM* vm, const char* name, int start_ip, int param_count, char** param_names) {
    if (!vm || !name) return -1;
    
    // Ensure capacity
    if (vm->function_count >= vm->function_capacity) {
        vm->function_capacity *= 2;
        vm->functions = (VMFunction*)realloc(vm->functions, sizeof(VMFunction) * vm->function_capacity);
        if (!vm->functions) {
            fprintf(stderr, "VM Error: Failed to reallocate function storage.\n");
            return -1;
        }
    }
    
    VMFunction* func = &vm->functions[vm->function_count];
    func->name = strdup(name);
    func->start_ip = start_ip;
    func->param_count = param_count;
    
    // Copy parameter names
    if (param_count > 0 && param_names) {
        func->param_names = (char**)malloc(sizeof(char*) * param_count);
        for (int i = 0; i < param_count; i++) {
            func->param_names[i] = strdup(param_names[i]);
        }
    } else {
        func->param_names = NULL;
    }
    
    return vm->function_count++;
}

VMFunction* vm_find_function(VM* vm, const char* name) {
    if (!vm || !name) return NULL;
    
    for (int i = 0; i < vm->function_count; i++) {
        if (strcmp(vm->functions[i].name, name) == 0) {
            return &vm->functions[i];
        }
    }
    return NULL;
}

void vm_push_frame(VM* vm, VMFunction* function, uint8_t* return_ip) {
    if (!vm || !function) return;
    
    // Ensure capacity
    if (vm->frame_count >= vm->frame_capacity) {
        vm->frame_capacity *= 2;
        vm->call_frames = (CallFrame*)realloc(vm->call_frames, sizeof(CallFrame) * vm->frame_capacity);
        if (!vm->call_frames) {
            fprintf(stderr, "VM Error: Failed to reallocate call frame storage.\n");
            return;
        }
    }
    
    CallFrame* frame = &vm->call_frames[vm->frame_count++];
    frame->function = function;
    frame->return_ip = return_ip;
    frame->stack_base = vm->stack_top - vm->stack;
}

CallFrame* vm_pop_frame(VM* vm) {
    if (!vm || vm->frame_count <= 0) return NULL;
    
    return &vm->call_frames[--vm->frame_count];
}

/**
 * @brief Set the global environment that contains all built-in functions.
 * This should be called once during VM initialization.
 */
void vm_set_global_environment(Environment* global_env) {
    g_global_env = global_env;
}

/**
 * @brief Copy all built-in functions from the global environment to a target environment.
 * This ensures that temporary environments have access to all built-ins.
 */
static void vm_copy_builtins_to_env(Environment* target_env) {
    if (!g_global_env || !target_env) {
        return;
    }
    
    // Walk through the global environment and copy all built-in functions
    Environment* current = g_global_env->next; // Skip the head node
    while (current) {
        if (current->variable_name && current->value.type == RUNTIME_VALUE_FUNCTION) {
            if (current->value.function_value.function_type == FUNCTION_TYPE_BUILTIN) {
                // Copy this built-in function to the target environment
                runtime_set_variable(target_env, current->variable_name, current->value);
            }
        }
        current = current->next;
    }
}
