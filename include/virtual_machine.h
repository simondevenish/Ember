#ifndef VIRTUAL_MACHINE_H
#define VIRTUAL_MACHINE_H

#include <stdint.h>

#include "runtime.h"
#include "parser.h"

/**
 * @brief The bytecode instruction set for EmberScript.
 */
typedef enum {
    // Misc and flow control
    OP_NOOP = 0,         // No-operation
    OP_EOF,              // End of bytecode (optional sentinel)
    OP_POP,              // Pop top of stack
    OP_DUP,              // Duplicate top of stack
    OP_SWAP,             // Swap top two stack items

    // Constants and variables
    OP_LOAD_CONST,       // Load a constant value onto the stack
    OP_LOAD_VAR,         // Load a variable from environment (by index or name)
    OP_STORE_VAR,        // Store top-of-stack value into variable (by index or name)
    OP_LOAD_GLOBAL,      // If we differentiate local vs. global in the futrue
    OP_STORE_GLOBAL,     
    OP_LOAD_UPVALUE,     // For closures / upvalues placeholder for future
    OP_STORE_UPVALUE,

    // Arithmetic and bitwise
    OP_ADD,              // a + b
    OP_SUB,              // a - b
    OP_MUL,              // a * b
    OP_DIV,              // a / b
    OP_MOD,              // a % b
    OP_NEG,              // -a (unary negation)

    // Logical operators
    OP_NOT,              // !a
    OP_AND,              // a && b (short-circuit if needed)
    OP_OR,               // a || b (short-circuit if needed)
    OP_EQ,               // a == b
    OP_NEQ,              // a != b
    OP_LT,               // a < b
    OP_GT,               // a > b
    OP_LTE,              // a <= b
    OP_GTE,              // a >= b

    // Jumps and branches
    OP_JUMP,             // Unconditional jump
    OP_JUMP_IF_FALSE,    // Jump if top of stack is false
    OP_JUMP_IF_TRUE,     // Jump if top of stack is true (optional)
    OP_LOOP,             // Jump backward (used for loops)

    // Function calls and returns
    OP_CALL,             // Call a function (may include arg count)
    OP_RETURN,           // Return from function (pop stack frame, etc.)

    // Objects, arrays, indexing
    OP_NEW_ARRAY,        // Create a new array
    OP_ARRAY_PUSH,       // Push an element into the array
    OP_GET_INDEX,        // a[b] (array or object index)
    OP_SET_INDEX,        // a[b] = c
    OP_NEW_OBJECT,       // Create a new object/map/dict
    OP_SET_PROPERTY,     // object.prop = value
    OP_GET_PROPERTY,     // push object.prop

    // Type conversions, printing, etc. (examples)
    OP_PRINT,            // Debug print top of stack
    OP_TO_STRING,        // Convert top of stack to string

    // Coroutines or concurrency placeholders for future
    OP_YIELD,
    OP_RESUME,

    // Additional placeholders
    OP_THROW,            // Throw an error
    OP_TRY_CATCH         // Possibly a try-catch block in the future??
} OpCode;

/**
 * @brief A structure representing a chunk of bytecode.
 *
 * Often, we store:
 * - A dynamic array `code` of `uint8_t` instructions
 * - A dynamic array `constants` for literal values
 */
typedef struct {
    uint8_t* code;        ///< The actual bytecode instructions
    int code_count;       ///< Number of instructions
    int code_capacity;    ///< Allocated capacity for `code`
    
    RuntimeValue* constants; ///< A table/array of constants used by this chunk
    int constants_count;     ///< Number of constants
    int constants_capacity;  ///< Allocated capacity for constants
} BytecodeChunk;

/**
 * @brief A structure representing the VM state.
 *
 * For a stack-based VM:
 * - `stack` array for push/pop
 * - `stack_top` to track the current top
 * - The current instruction pointer (IP)
 * - Possibly a pointer to Environment for variables, or a specialized “CallFrame” array
 */
typedef struct {
    BytecodeChunk* chunk; ///< The chunk of bytecode we're executing
    uint8_t* ip;          ///< Instruction pointer into `chunk->code`
    
    RuntimeValue* stack;  ///< The VM's operand stack
    RuntimeValue* stack_top; ///< Points to the next free slot
    int stack_capacity;   ///< Size of `stack`
    
    // Potentially a call stack for function calls, environments, etc.
    // Environment* global_env; // Bridging to runtime environment
    // ...
} VM;

/**
 * @brief Create a new, empty BytecodeChunk.
 *
 * @return BytecodeChunk*
 */
BytecodeChunk* vm_create_chunk();

/**
 * @brief Free the memory allocated by a BytecodeChunk.
 *
 * @param chunk The chunk to free.
 */
void vm_free_chunk(BytecodeChunk* chunk);

/**
 * @brief Add a byte (instruction opcode or operand) to the chunk.
 *
 * @param chunk The BytecodeChunk to modify.
 * @param byte The byte to add.
 */
void vm_chunk_write_byte(BytecodeChunk* chunk, uint8_t byte);

/**
 * @brief Add a constant (e.g., a number or string) to the chunk's constants table.
 *
 * @param chunk The BytecodeChunk
 * @param value The constant value (RuntimeValue)
 * @return int The index of the constant in the table.
 */
int vm_chunk_add_constant(BytecodeChunk* chunk, RuntimeValue value);

/**
 * @brief Initialize a new VM with a given chunk.
 *
 * @param chunk The chunk of bytecode to run.
 * @return VM* Pointer to the newly allocated VM.
 */
VM* vm_create(BytecodeChunk* chunk);

/**
 * @brief Free a VM and its resources.
 *
 * @param vm The VM to free.
 */
void vm_free(VM* vm);

/**
 * @brief Run the bytecode in the given VM until completion or error.
 *
 * @param vm The VM instance.
 * @return int 0 on success, non-zero on error.
 */
int vm_run(VM* vm);

/**
 * @brief Push a value onto the VM stack.
 *
 * @param vm The VM instance.
 * @param value The value to push.
 */
void vm_push(VM* vm, RuntimeValue value);

/**
 * @brief Pop a value from the VM stack.
 *
 * @param vm The VM instance.
 * @return RuntimeValue The popped value.
 */
RuntimeValue vm_pop(VM* vm);

#endif // VIRTUAL_MACHINE_H
