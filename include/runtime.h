#ifndef RUNTIME_H
#define RUNTIME_H

#include "parser.h"

#include <stddef.h>

// Forward declarations
typedef struct Environment Environment;
typedef struct UserDefinedFunction UserDefinedFunction;
typedef struct RuntimeValue RuntimeValue;

// Runtime Value Types
typedef enum {
    RUNTIME_VALUE_NUMBER,
    RUNTIME_VALUE_STRING,
    RUNTIME_VALUE_BOOLEAN,
    RUNTIME_VALUE_NULL,
    RUNTIME_VALUE_ARRAY,
    RUNTIME_VALUE_OBJECT,
    RUNTIME_VALUE_FUNCTION  // Added to handle function types in runtime
} RuntimeValueType;

// User-Defined Functions
struct UserDefinedFunction {
    char* name;
    char** parameters;
    int parameter_count;
    ASTNode* body;
};

typedef enum {
    FUNCTION_TYPE_BUILTIN,
    FUNCTION_TYPE_USER
} FunctionType;

// Forward declaration of RuntimeValue
struct RuntimeValue;

// Built-in Function Pointer
typedef RuntimeValue (*BuiltinFunction)(Environment* env, RuntimeValue* args, int arg_count);

typedef struct {
    FunctionType function_type;
    union {
        BuiltinFunction builtin_function;
        UserDefinedFunction* user_function;
    };
} FunctionValue;

// Complete Definition of RuntimeValue
struct RuntimeValue {
    RuntimeValueType type;
    union {
        double number_value;
        char* string_value;
        bool boolean_value;
        struct {
            struct RuntimeValue* elements;
            int count;
        } array_value;
        struct {
            char** keys;
            struct RuntimeValue* values;
            int count;
        } object_value;
        FunctionValue function_value; // For functions
    };
};

// Environment (linked list for variables and scope management)
struct Environment {
    char* variable_name;
    RuntimeValue value;
    Environment* next;
    Environment* parent; // Parent environment for nested scopes
};

// Runtime Error
typedef struct {
    char* message;
    int line;
    int column;
} RuntimeError;

// Garbage Collector
typedef struct GarbageCollector {
    RuntimeValue* values;   // Array of tracked runtime values
    size_t value_count;     // Number of currently tracked values
    size_t value_capacity;  // Maximum capacity of the tracked values array
} GarbageCollector;

// Struct to pass data to the thread function
typedef struct {
    Environment* env;
    ASTNode* block;
} ThreadExecutionData;

// Runtime API
/**
 * @brief Create the global runtime environment.
 * 
 * @return Environment* Pointer to the global environment.
 */
Environment* runtime_create_environment();

/**
 * @brief Create a child environment for nested scopes.
 * 
 * @param parent Pointer to the parent environment.
 * @return Environment* Pointer to the child environment.
 */
Environment* runtime_create_child_environment(Environment* parent);

/**
 * @brief Reads and executes a .ember file into the specified environment.
 *
 * Loads the specified file (e.g., "items.ember"), parses it, and executes
 * all top-level statements in the same environment. This allows variables,
 * functions, and other definitions declared in that file to become available
 * to scripts that import it.
 *
 * @param env Pointer to the environment into which the file's code should be executed.
 * @param filename Path to the .ember script file to load and execute.
 * @return true on success, false if there was an error reading or parsing the file.
 */
bool runtime_execute_file_in_environment(Environment* env, const char* filename);

/**
 * @brief Add or update a variable in the current environment.
 * 
 * @param env Pointer to the environment.
 * @param name Name of the variable.
 * @param value Value to assign to the variable.
 */
void runtime_set_variable(Environment* env, const char* name, RuntimeValue value);

/**
 * @brief Retrieve the value of a variable from the environment.
 * 
 * @param env Pointer to the environment.
 * @param name Name of the variable to retrieve.
 * @return RuntimeValue* Pointer to the variable's value, or NULL if not found.
 */
RuntimeValue* runtime_get_variable(Environment* env, const char* name);

/**
 * @brief Evaluate an AST node and return its runtime value.
 * 
 * @param env Pointer to the environment.
 * @param node Pointer to the AST node to evaluate.
 * @return RuntimeValue The result of evaluating the AST node.
 */
RuntimeValue runtime_evaluate(Environment* env, ASTNode* node);

/**
 * @brief Execute a block of statements (AST_BLOCK) and return the value of the last expression.
 * 
 * @param env Pointer to the environment.
 * @param block Pointer to the block AST node.
 * @return RuntimeValue The value of the last expression in the block.
 */
RuntimeValue runtime_execute_block_with_return(Environment* env, ASTNode* block);

/**
 * @brief Execute a block of statements (AST_BLOCK).
 * 
 * @param env Pointer to the environment.
 * @param block Pointer to the block AST node.
 */
void runtime_execute_block(Environment* env, ASTNode* block);

/**
 * @brief Execute a function call (AST_FUNCTION_CALL).
 * 
 * @param env Pointer to the environment.
 * @param function_call Pointer to the function call AST node.
 * @return RuntimeValue The result of the function call.
 */
RuntimeValue runtime_execute_function_call(Environment* env, ASTNode* function_call);

/**
 * @brief Register a built-in function in the environment.
 * 
 * @param env Pointer to the environment.
 * @param name Name of the function.
 * @param function Function pointer to the built-in function.
 */
void runtime_register_builtin(Environment* env, const char* name, BuiltinFunction function);

/**
 * @brief Register a user-defined function in the environment.
 * 
 * @param env Pointer to the environment.
 * @param function Pointer to the user-defined function to register.
 */
void runtime_register_function(Environment* env, UserDefinedFunction* function);

/**
 * @brief Retrieve a user-defined function from the environment.
 * 
 * @param env Pointer to the environment.
 * @param name Name of the function to retrieve.
 * @return UserDefinedFunction* Pointer to the user-defined function, or NULL if not found.
 */
UserDefinedFunction* runtime_get_function(Environment* env, const char* name);

/**
 * @brief Handle a runtime error and terminate execution.
 * 
 * @param error Pointer to the RuntimeError structure.
 */
void runtime_error(RuntimeError* error);

/**
 * @brief Generate and report a runtime error with details.
 * 
 * @param env Pointer to the environment.
 * @param message The error message.
 * @param node Pointer to the AST node where the error occurred.
 */
void runtime_report_error(Environment* env, const char* message, const ASTNode* node);

/**
 * @brief Free the memory used by the runtime environment.
 * 
 * @param env Pointer to the environment.
 */
void runtime_free_environment(Environment* env);

/**
 * @brief Free memory allocated for a runtime value.
 * 
 * @param value Pointer to the RuntimeValue to free.
 */
void runtime_free_value(RuntimeValue* value);

/**
 * @brief Print a runtime value for debugging purposes.
 * 
 * @param value Pointer to the runtime value to print.
 */
void print_runtime_value(const RuntimeValue* value);

/**
 * @brief Print all variables in the environment for debugging purposes.
 * 
 * @param env Pointer to the environment.
 */
void runtime_print_environment(const Environment* env);

/**
 * @brief Convert a runtime value to a string.
 * 
 * @param value Pointer to the runtime value.
 * @return char* String representation of the runtime value.
 */
char* runtime_value_to_string(const RuntimeValue* value);

/**
 * @brief Execute a block of code in a separate thread.
 * 
 * @param env Pointer to the environment.
 * @param block Pointer to the block AST node.
 */
void runtime_execute_in_thread(Environment* env, ASTNode* block);

/**
 * @brief Initialize the garbage collector.
 * 
 * @return GarbageCollector* Pointer to the new garbage collector.
 */
GarbageCollector* runtime_gc_init();

/**
 * @brief Track a runtime value for garbage collection.
 * 
 * @param gc Pointer to the garbage collector.
 * @param value The runtime value to track.
 */
void runtime_gc_track(GarbageCollector* gc, RuntimeValue value);

/**
 * @brief Perform garbage collection to free unused memory.
 * 
 * @param gc Pointer to the garbage collector.
 */
void runtime_gc_collect(GarbageCollector* gc);

/**
 * @brief Free the memory used by the garbage collector.
 * 
 * @param gc Pointer to the garbage collector.
 */
void runtime_gc_free(GarbageCollector* gc);


// Event System
typedef struct RuntimeEvent {
    char* event_name;
    RuntimeValue* data;
} RuntimeEvent;

/**
 * @brief Trigger an event in the runtime environment.
 * 
 * This function handles custom runtime events, invoking any registered event handlers
 * or performing necessary actions based on the event name and associated data.
 * 
 * @param env Pointer to the environment where the event occurs.
 * @param event Pointer to the RuntimeEvent structure containing event details.
 */
void runtime_trigger_event(Environment* env, RuntimeEvent* event);

#endif // RUNTIME_H
