#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "lexer.h"
#include "parser.h"
#include "runtime.h"

/**
 * @brief Execute a script from source code.
 * 
 * @param source The source code of the script as a string.
 * @return int Status code (0 for success, non-zero for errors).
 */
int interpreter_execute_script(const char* source);

#endif // INTERPRETER_H
