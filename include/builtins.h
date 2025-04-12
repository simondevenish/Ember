#ifndef BUILTINS_H
#define BUILTINS_H

#include "runtime.h"

/**
 * @brief Register all built-in functions to the runtime environment.
 *
 * @param env Pointer to the global runtime environment.
 */
void builtins_register(Environment* env);

/**
 * @brief Print values to the console.
 *
 * @param env The runtime environment.
 * @param args The arguments passed to the function.
 * @param arg_count The number of arguments.
 * @return RuntimeValue The result of the function (null in this case).
 */
RuntimeValue builtin_print(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Return the absolute value of a number.
 *
 * @param env The runtime environment.
 * @param args The arguments passed to the function.
 * @param arg_count The number of arguments.
 * @return RuntimeValue The absolute value.
 */
RuntimeValue builtin_abs(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Return the length of a string or array.
 *
 * @param env The runtime environment.
 * @param args The arguments passed to the function.
 * @param arg_count The number of arguments.
 * @return RuntimeValue The length of the string or array.
 */
RuntimeValue builtin_len(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Generate a random number between 0 and 1.
 *
 * @param env The runtime environment.
 * @param args The arguments passed to the function.
 * @param arg_count The number of arguments.
 * @return RuntimeValue A random number.
 */
RuntimeValue builtin_rand(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Return the type of a runtime value as a string.
 *
 * @param env The runtime environment.
 * @param args The arguments passed to the function.
 * @param arg_count The number of arguments.
 * @return RuntimeValue The type of the value.
 */
RuntimeValue builtin_type(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Math and Numeric Functions
 */
RuntimeValue builtin_floor(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_ceil(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_sqrt(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_pow(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_sin(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_cos(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_tan(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_log(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_round(Environment* env, RuntimeValue* args, int arg_count);

/**
 * String Functions
 */
RuntimeValue builtin_concat(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_substring(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_to_upper(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_to_lower(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_index_of(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_replace(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Array Functions
 */
RuntimeValue builtin_push(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_pop(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_shift(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_unshift(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_slice(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_join(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Type Conversion
 */
RuntimeValue builtin_to_string(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_to_number(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_to_boolean(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Utility Functions
 */
RuntimeValue builtin_typeof(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_eval(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_assert(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_clone(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Date and Time
 */
RuntimeValue builtin_now(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_date(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_time(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Control Flow
 */
RuntimeValue builtin_exit(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_throw(Environment* env, RuntimeValue* args, int arg_count);

/**
 * I/O
 */
RuntimeValue builtin_read_file(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_write_file(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Debugging
 */
RuntimeValue builtin_log(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_debug(Environment* env, RuntimeValue* args, int arg_count);

/**
 * Randomness and Probability
 */
RuntimeValue builtin_rand_int(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_rand_choice(Environment* env, RuntimeValue* args, int arg_count);

#endif // BUILTINS_H
