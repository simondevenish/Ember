#ifndef SDL_MODULE_H
#define SDL_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

/**
 * @brief Built-in function for initializing SDL (sdl.init).
 * 
 * Usage from Ember script:
 *    sdl.init();
 * 
 * Returns a boolean indicating success or failure.
 */
RuntimeValue builtin_sdl_init(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Built-in function for creating an SDL window (sdl.createWindow).
 * 
 * Usage from Ember script:
 *    sdl.createWindow("My Title", 800, 600);
 * 
 * Expects 3 arguments:
 *  1) String: window title
 *  2) Number: width
 *  3) Number: height
 * 
 * Returns a boolean (true if the window was created, false otherwise).
 */
RuntimeValue builtin_sdl_create_window(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Built-in function for polling SDL events (sdl.pollEvents).
 * 
 * Usage from Ember script:
 *    sdl.pollEvents();
 * 
 * Loops through SDL events and can be used to detect user input or quit events.
 * Returns null (no direct return value).
 */
RuntimeValue builtin_sdl_poll_events(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Built-in function for quitting SDL (sdl.quit).
 * 
 * Usage from Ember script:
 *    sdl.quit();
 * 
 * Cleans up SDL subsystems. Returns null.
 */
RuntimeValue builtin_sdl_quit(Environment* env, RuntimeValue* args, int arg_count);

/**
 * @brief Registers all SDL-related built-in functions into the given environment.
 *
 * Call this after creating the global Environment and before running scripts
 * that use `sdl.init`, `sdl.createWindow`, etc.
 *
 * @param env Pointer to the global Environment.
 */
void sdl_register_builtins(Environment* env);

#ifdef __cplusplus
}
#endif

#endif // SDL_MODULE_H
