#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

// Token types for the scripting language
typedef enum {
    TOKEN_IDENTIFIER,  // Names (e.g., variable or function names)
    TOKEN_NUMBER,      // Numbers (e.g., 42, 3.14)
    TOKEN_STRING,      // Strings (e.g., "Hello, world!")
    TOKEN_OPERATOR,    // Operators (e.g., +, -, *, /)
    TOKEN_KEYWORD,     // Keywords (e.g., if, while, return)
    TOKEN_PUNCTUATION, // Punctuation (e.g., (, ), {, }, ;)
    TOKEN_BOOLEAN,     // Boolean values (e.g., true, false)
    TOKEN_NULL,        // Null value (e.g., null or nil)
    TOKEN_INDENT,      // Indentation increase
    TOKEN_DEDENT,      // Indentation decrease
    TOKEN_NEWLINE,     // Significant newline (for indentation tracking)
    TOKEN_EOF,         // End of file/input
    TOKEN_ERROR        // Error token type
} ScriptTokenType;

// Token structure
typedef struct {
    ScriptTokenType type;  // Type of the token
    char* value;     // Value of the token (e.g., "42", "+")
    int line;        // Line number of the token
    int column;      // Column number of the token
} Token;

// Indentation stack for tracking nested indentation levels
#define MAX_INDENT_LEVELS 64

// Lexer structure
typedef struct {
    const char* source;  // Script source code
    int position;        // Current position in the source
    int line;            // Current line in the source
    int column;          // Current column in the source
    char current_char;   // Current character being processed
    
    // Indentation tracking
    int indent_stack[MAX_INDENT_LEVELS];  // Stack of indentation levels
    int indent_stack_size;                // Current size of indent stack
    int current_indent;                   // Current line's indentation level
    bool at_line_start;                   // True if we're at the start of a line
    bool pending_dedents;                 // True if we have pending DEDENT tokens
    int dedent_count;                     // Number of pending DEDENT tokens
} Lexer;

/**
 * @brief Initializes the lexer with the source script.
 * 
 * @param lexer The lexer instance.
 * @param source The source script as a string.
 */
void lexer_init(Lexer* lexer, const char* source);

/**
 * @brief Advances the lexer to the next character in the source.
 * 
 * @param lexer The lexer instance.
 */
void lexer_advance(Lexer* lexer);

/**
 * @brief Skips whitespace characters in the source.
 * 
 * @param lexer The lexer instance.
 */
void lexer_skip_whitespace(Lexer* lexer);

/**
 * @brief Reads an identifier or keyword from the source.
 * 
 * @param lexer The lexer instance.
 * @return char* The identifier or keyword as a string.
 */
char* lexer_read_identifier(Lexer* lexer);

void lexer_skip_whitespace_and_comments(Lexer* lexer);

char lexer_peek(Lexer* lexer);

/**
 * @brief Retrieves the next token from the source.
 * 
 * @param lexer The lexer instance.
 * @return Token The next token in the source.
 */
Token lexer_next_token(Lexer* lexer);

void add_token(Lexer* lexer, ScriptTokenType type, const char* value);
/**
 * @brief Checks if a string is a keyword.
 * 
 * @param identifier The string to check.
 * @return true If the string is a keyword.
 * @return false Otherwise.
 */
bool is_keyword(const char* identifier);

/**
 * @brief Frees memory allocated for a token.
 * 
 * @param token The token to free.
 */
void free_token(Token* token);

/**
 * @brief Prints a token for debugging purposes.
 * 
 * @param token The token to print.
 */
void print_token(const Token* token);

/**
 * @brief Handles indentation at the beginning of a line.
 * 
 * @param lexer The lexer instance.
 * @return Token The appropriate INDENT, DEDENT, or next token.
 */
Token lexer_handle_indentation(Lexer* lexer);

/**
 * @brief Calculates the indentation level of the current line.
 * 
 * @param lexer The lexer instance.
 * @return int The indentation level (number of spaces/tabs).
 */
int lexer_calculate_indentation(Lexer* lexer);

/**
 * @brief Pushes an indentation level onto the stack.
 * 
 * @param lexer The lexer instance.
 * @param level The indentation level to push.
 */
void lexer_push_indent(Lexer* lexer, int level);

/**
 * @brief Pops an indentation level from the stack.
 * 
 * @param lexer The lexer instance.
 * @return int The popped indentation level.
 */
int lexer_pop_indent(Lexer* lexer);

#endif // LEXER_H
