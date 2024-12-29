#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "virtual_machine.h"

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

    return vm;
}

void vm_free(VM* vm) {
    if (!vm) return;
    if (vm->stack) {
        // Free each stack element if needed
        free(vm->stack);
    }
    free(vm);
}

void vm_push(VM* vm, RuntimeValue value) {
    // Check for overflow
    if (vm->stack_top - vm->stack >= vm->stack_capacity) {
        fprintf(stderr, "VM Error: Stack overflow.\n");
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

RuntimeValue vm_pop(VM* vm) {
    // Check for underflow
    if (vm->stack_top == vm->stack) {
        fprintf(stderr, "VM Error: Stack underflow.\n");
        RuntimeValue v; v.type = RUNTIME_VALUE_NULL;
        return v;
    }
    vm->stack_top--;
    return *vm->stack_top;
}

static RuntimeValue vm_peek(VM* vm, int distance) {
    // distance=0 => top
    return vm->stack_top[-1 - distance];
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "virtual_machine.h"
#include "runtime.h"

/**
 * For now, let's store variables in a fixed global array.
 * The compiler assigns each variable an index (0..255).
 */
static RuntimeValue g_globals[256]; // You can adapt as needed

int vm_run(VM* vm) {
    for (;;) {
        // Fetch the next instruction
        uint8_t instruction = *vm->ip++;

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
                // Pop and discard top of stack
                vm_pop(vm);
                break;
            }

            case OP_DUP: {
                // Duplicate the top stack value
                RuntimeValue topVal = vm_pop(vm);
                vm_push(vm, topVal);
                vm_push(vm, topVal);
                break;
            }

            case OP_SWAP: {
                // Swap top two stack items
                RuntimeValue a = vm_pop(vm);
                RuntimeValue b = vm_pop(vm);
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
                // The next byte is the variable index
                uint8_t varIndex = *vm->ip++;
                vm_push(vm, g_globals[varIndex]);
                break;
            }

            case OP_STORE_VAR: {
                // The next byte is the variable index
                uint8_t varIndex = *vm->ip++;
                // Pop top of stack and store in global array
                RuntimeValue value = vm_pop(vm);
                g_globals[varIndex] = value;
                // push it back for language’s assignment returning value
                // vm_push(vm, value);
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
                    // Non-boolean? Convert to boolean “truthiness” then invert
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
                // For a minimal pass, handle built-in calls or do placeholders
                // Byte 1: function index, Byte 2: argCount
                uint8_t funcIndex = *vm->ip++;
                uint8_t argCount  = *vm->ip++;
                
                // If we have user-defined functions with real call frames, we would implement them here.
                // For now, just do nothing or handle built-ins.

                // e.g. no-op placeholder
                (void)funcIndex;
                (void)argCount;
                break;
            }

            case OP_RETURN: {
                // Typically we would pop a return value, handle call frames, etc.
                // For now, let’s just return success from vm_run.
                return 0;
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
                break;
            }

            case OP_TO_STRING: {
                // If we want to convert the top value to a string in place
                // For now, just skip or handle as needed
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
