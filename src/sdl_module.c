#include "sdl_module.h"
#include "runtime.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

//
// Global or static variables for SDL objects
//
static SDL_Window*    g_window   = NULL;
static SDL_Renderer*  g_renderer = NULL;
static bool           g_sdlInitialized = false;

//
// 1) sdl.init()
//    Initializes the SDL library
//    Returns: boolean (true if success, false if error)
//
RuntimeValue builtin_sdl_init(Environment* env, RuntimeValue* args, int arg_count)
{
    (void)env;       // not currently needed
    (void)arg_count; // no arguments expected
    
    RuntimeValue result;
    result.type = RUNTIME_VALUE_BOOLEAN;
    
    // If already initialized, return true
    if (g_sdlInitialized) {
        result.boolean_value = true;
        return result;
    }
    
    // Initialize SDL (you can also pass flags like SDL_INIT_VIDEO, etc.)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        result.boolean_value = false;
        return result;
    }

    // Mark as initialized
    g_sdlInitialized = true;
    result.boolean_value = true;
    return result;
}

//
// 2) sdl.createWindow(title, width, height)
//    Creates an SDL window and an optional renderer
//    Returns: boolean (true if success, false if error)
//
RuntimeValue builtin_sdl_create_window(Environment* env, RuntimeValue* args, int arg_count)
{
    (void)env;

    RuntimeValue result;
    result.type = RUNTIME_VALUE_BOOLEAN;
    result.boolean_value = false; // default to false

    if (arg_count < 3) {
        fprintf(stderr, "Error: sdl.createWindow requires 3 arguments: title, width, height.\n");
        return result;
    }

    // Check argument types
    if (args[0].type != RUNTIME_VALUE_STRING ||
        args[1].type != RUNTIME_VALUE_NUMBER ||
        args[2].type != RUNTIME_VALUE_NUMBER)
    {
        fprintf(stderr, "Error: sdl.createWindow(title, width, height) expects (string, number, number).\n");
        return result;
    }

    if (!g_sdlInitialized) {
        fprintf(stderr, "Error: SDL not initialized. Call sdl.init() first.\n");
        return result;
    }

    const char* title = args[0].string_value;
    int width  = (int)args[1].number_value;
    int height = (int)args[2].number_value;

    // If a window was previously created, destroy it first (optional)
    if (g_window) {
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        g_window   = NULL;
        g_renderer = NULL;
    }

    // Create the window
    g_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return result;
    }

    // Create a renderer as well
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return result; // still false
    }

    // If we reach here, success
    result.boolean_value = true;
    return result;
}

//
// 3) sdl.pollEvents()
//    Polls SDL events (i.e., user input, etc.)
//    Returns: null
//
RuntimeValue builtin_sdl_poll_events(Environment* env, RuntimeValue* args, int arg_count)
{
    (void)env;
    (void)args;
    (void)arg_count;

    if (!g_sdlInitialized) {
        fprintf(stderr, "Warning: SDL not initialized. pollEvents does nothing.\n");
        // Return null
        RuntimeValue r;
        r.type = RUNTIME_VALUE_NULL;
        return r;
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // For demonstration, handle a simple "quit" event
        if (e.type == SDL_QUIT) {
            // Could set a global variable or do something in the Ember environment
            // e.g. runtime_set_variable(...) to let scripts know the user tried to close
            fprintf(stdout, "[SDL] Quit event received.\n");
        }
        // Could also handle keydown, mouse events, etc.
    }

    // Return null
    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;
    return r;
}

//
// 4) sdl.quit()
//    Cleans up SDL
//    Returns: null
//
RuntimeValue builtin_sdl_quit(Environment* env, RuntimeValue* args, int arg_count)
{
    (void)env;
    (void)args;
    (void)arg_count;

    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }

    // If SDL was initialized, quit it
    if (g_sdlInitialized) {
        SDL_Quit();
        g_sdlInitialized = false;
    }

    // Return null
    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;
    return r;
}

//
// 5) sdl_register_builtins()
//    Registers all sdl.* built-ins in the environment
//
void sdl_register_builtins(Environment* env)
{
    // Register each function under "sdl.init", "sdl.createWindow", etc.
    runtime_register_builtin(env, "sdl_init",         builtin_sdl_init);
    runtime_register_builtin(env, "sdl_createWindow", builtin_sdl_create_window);
    runtime_register_builtin(env, "sdl_pollEvents",   builtin_sdl_poll_events);
    runtime_register_builtin(env, "sdl_quit",         builtin_sdl_quit);
}
