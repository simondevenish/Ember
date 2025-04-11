#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // if you need malloc, free, etc.
#include <string.h>   // if you need strcmp, etc.

#include <raylib_module.h>
#include <raylib.h>

#include "runtime.h"

//------------------------------------------------------
// builtin_raylib_init
//------------------------------------------------------
RuntimeValue builtin_raylib_init(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;

    RuntimeValue result;
    result.type = RUNTIME_VALUE_BOOLEAN; // Return true/false
    result.boolean_value = false;

    // Expect at least 3 arguments: width, height, title
    if (arg_count < 3) {
        fprintf(stderr, "Error: raylib_init requires (width, height, title)\n");
        return result;
    }

    // Extract values
    if (args[0].type != RUNTIME_VALUE_NUMBER ||
        args[1].type != RUNTIME_VALUE_NUMBER ||
        args[2].type != RUNTIME_VALUE_STRING) 
    {
        fprintf(stderr, "Error: raylib_init(width, height, title) expects numeric numeric string.\n");
        return result;
    }

    int width  = (int)args[0].number_value;
    int height = (int)args[1].number_value;
    const char* title = args[2].string_value;

    InitWindow(width, height, title);

    // Optionally set some FPS
    SetTargetFPS(60);

    // If we reach here, success
    result.boolean_value = true;
    return result;
}

//------------------------------------------------------
// builtin_raylib_close
//------------------------------------------------------
RuntimeValue builtin_raylib_close(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    (void)args;
    (void)arg_count;

    CloseWindow();
    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;
    return r;
}

//------------------------------------------------------
// builtin_raylib_window_should_close
//------------------------------------------------------
RuntimeValue builtin_raylib_window_should_close(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; (void)args; (void)arg_count;

    bool shouldClose = WindowShouldClose();

    RuntimeValue result;
    result.type = RUNTIME_VALUE_BOOLEAN;
    result.boolean_value = shouldClose;
    return result;
}

//------------------------------------------------------
// builtin_raylib_begin_drawing
//------------------------------------------------------
RuntimeValue builtin_raylib_begin_drawing(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; (void)args; (void)arg_count;

    BeginDrawing();

    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;
    return r;
}

//------------------------------------------------------
// builtin_raylib_end_drawing
//------------------------------------------------------
RuntimeValue builtin_raylib_end_drawing(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env; (void)args; (void)arg_count;

    EndDrawing();

    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;
    return r;
}

//------------------------------------------------------
// builtin_raylib_clear_background(r, g, b)
//------------------------------------------------------
RuntimeValue builtin_raylib_clear_background(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;

    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;

    if (arg_count < 3) {
        fprintf(stderr, "Error: raylib_clear_background(r,g,b) expects three numeric arguments.\n");
        return r;
    }
    if (args[0].type != RUNTIME_VALUE_NUMBER ||
        args[1].type != RUNTIME_VALUE_NUMBER ||
        args[2].type != RUNTIME_VALUE_NUMBER) {
        fprintf(stderr, "Error: raylib_clear_background(r,g,b) expects numeric.\n");
        return r;
    }

    int rr = (int)args[0].number_value;
    int gg = (int)args[1].number_value;
    int bb = (int)args[2].number_value;

    ClearBackground((Color){ rr, gg, bb, 255 });

    return r;
}

//------------------------------------------------------
// builtin_raylib_draw_text("Hello", x, y, fontSize)
//------------------------------------------------------
RuntimeValue builtin_raylib_draw_text(Environment* env, RuntimeValue* args, int arg_count) {
    (void)env;
    RuntimeValue r;
    r.type = RUNTIME_VALUE_NULL;

    if (arg_count < 4) {
        fprintf(stderr, "Error: raylib_draw_text requires (text, x, y, fontSize)\n");
        return r;
    }
    if (args[0].type != RUNTIME_VALUE_STRING ||
        args[1].type != RUNTIME_VALUE_NUMBER ||
        args[2].type != RUNTIME_VALUE_NUMBER ||
        args[3].type != RUNTIME_VALUE_NUMBER) 
    {
        fprintf(stderr, "Error: raylib_draw_text(text, x, y, size) expects string num num num.\n");
        return r;
    }

    const char* text = args[0].string_value;
    int x = (int)args[1].number_value;
    int y = (int)args[2].number_value;
    int fontSize = (int)args[3].number_value;

    DrawText(text, x, y, fontSize, RAYWHITE);

    return r;
}

//------------------------------------------------------
// Register All Raylib Builtins
//------------------------------------------------------
void raylib_register_builtins(Environment* env)
{
    // raylib_init, raylib_close, etc.
    runtime_register_builtin(env, "raylib_init", builtin_raylib_init);
    runtime_register_builtin(env, "raylib_close", builtin_raylib_close);
    runtime_register_builtin(env, "raylib_window_should_close", builtin_raylib_window_should_close);
    runtime_register_builtin(env, "raylib_begin_drawing", builtin_raylib_begin_drawing);
    runtime_register_builtin(env, "raylib_end_drawing", builtin_raylib_end_drawing);
    runtime_register_builtin(env, "raylib_clear_background", builtin_raylib_clear_background);
    runtime_register_builtin(env, "raylib_draw_text", builtin_raylib_draw_text);
}
