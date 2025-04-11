#ifndef RAYLIB_MODULE_H
#define RAYLIB_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

/**
 * @brief Initialize raylib. 
 *        E.g. open a window, set up a graphics context, etc.
 *        In normal raylib, you'd call InitWindow, etc.
 *
 * Usage from Ember script:
 *    raylib_init(width, height, "My Window Title");
 */
RuntimeValue builtin_raylib_init(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Close the window, end the Raylib context
 * 
 * Usage:
 *    raylib_close();
 */
RuntimeValue builtin_raylib_close(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief A simple function to check if the window should close (e.g., user pressed X).
 * 
 * Usage:
 *    raylib_window_should_close();
 * Returns boolean true if window should close.
 */
RuntimeValue builtin_raylib_window_should_close(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief A main loop step for Raylib. 
 *        E.g. BeginDrawing() / EndDrawing() for a simple 2D scenario.
 * 
 * Usage:
 *    raylib_begin_drawing();
 *    // do drawing calls...
 *    raylib_end_drawing();
 */
RuntimeValue builtin_raylib_begin_drawing(Environment* env, RuntimeValue* args, int arg_count);
RuntimeValue builtin_raylib_end_drawing(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Clear the background with some color
 * 
 * Usage:
 *    raylib_clear_background(r, g, b);
 */
RuntimeValue builtin_raylib_clear_background(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Some more advanced calls, e.g. drawing text, shapes, etc.
 *        We'll keep them basic for demonstration.
 */
RuntimeValue builtin_raylib_draw_text(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Register all Raylib built-in functions to the runtime environment.
 * 
 * Call this in your `builtins_register(...)` if `raylib` is installed, 
 * or conditionally if user has `import raylib;`.
 */
void raylib_register_builtins(Environment* env);

#ifdef __cplusplus
}
#endif

#endif // RAYLIB_MODULE_H
