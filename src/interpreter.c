#include "interpreter.h"

#include "lexer.h"
#include "parser.h"
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

int interpreter_execute_script(const char* source) {
    // Step 1: Lexing
    Lexer lexer;
    lexer_init(&lexer, source);

    // Step 2: Parsing
    Parser* parser = parser_create(&lexer);
    ASTNode* root = parse_script(parser);

    if (!root) {
        fprintf(stderr, "Error: Parsing failed.\n");
        return 1;
    }

    // Step 3: Runtime Evaluation
    Environment* env = runtime_create_environment();
    
    // Optionally, register built-in functions here, if required
    // runtime_register_builtin(env, "print", builtin_print_function);

    RuntimeValue result = runtime_evaluate(env, root);

    // Debug output (optional)
    print_runtime_value(&result);

    // Cleanup
    free_ast(root);
    runtime_free_environment(env);
    free(parser);

    return 0;
}