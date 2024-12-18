#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

// AST Node Types
typedef enum {
    AST_LITERAL,
    AST_VARIABLE,        // Variable reference
    AST_BINARY_OP,
    AST_UNARY_OP,        // Unary operation (e.g., -x, !flag)
    AST_ASSIGNMENT,
    AST_VARIABLE_DECL,   // Variable declaration (e.g., var x = 5)
    AST_FUNCTION_CALL,
    AST_IF_STATEMENT,
    AST_WHILE_LOOP,
    AST_FOR_LOOP,
    AST_SWITCH_CASE,     // Switch/case statement
    AST_LOGICAL_OP,
    AST_BLOCK,
    AST_FUNCTION_DEF
} ASTNodeType;

// AST Node Structure
typedef struct ASTNode {
    ASTNodeType type;
    int line;   // Line number where this node appears
    int column; // Column number where this node appears
    union {
        struct { ScriptTokenType token_type; char* value; } literal; // Literal values (e.g., numbers, strings)
        struct { struct ASTNode* operand; char* op_symbol; } unary_op;  // Unary operation (e.g., -x, !x)
        struct { struct ASTNode* left; struct ASTNode* right; char* op_symbol; } binary_op; // Binary operation (e.g., x + y)
        struct { char* variable; struct ASTNode* value; } assignment; // Assignment (e.g., x = y)
        struct { char* variable_name; struct ASTNode* initial_value; } variable_decl; // Variable declaration (e.g., var x = 5)
        struct { char* function_name; struct ASTNode** arguments; int argument_count; } function_call; // Function call
        struct { struct ASTNode* condition; struct ASTNode* body; struct ASTNode* else_body; } if_statement; // If statement
        struct { struct ASTNode* condition; struct ASTNode* body; } while_loop; // While loop
        struct { struct ASTNode* initializer; struct ASTNode* condition; struct ASTNode* increment; struct ASTNode* body; } for_loop; // For loop
        struct { struct ASTNode* case_value; struct ASTNode* case_body; } single_case; // Single case in switch statement
        struct { struct ASTNode* condition; struct ASTNode** cases; struct ASTNode* default_case; int case_count; } switch_case; // Switch statement
        struct { struct ASTNode** statements; int statement_count; } block; // Block of statements
        struct { char* function_name; char** parameters; int parameter_count; struct ASTNode* body; } function_def; // Function definition
        struct { struct ASTNode* left; struct ASTNode* right; char* op_symbol; } logical_op; // Logical operation (e.g., &&, ||)
        struct { char* variable_name; } variable; // For AST_VARIABLE
    };
} ASTNode;


// Error Handling
typedef struct {
    int line;
    int column;
    char* message;
} ParserError;

// Error callback function pointer
typedef void (*ParserErrorCallback)(const ParserError* error);

// Parser State
typedef struct {
    Lexer* lexer;
    Token current_token;
    ParserErrorCallback error_callback; // Error reporting callback
} Parser;

// Parser API
/**
 * @brief Create a new parser instance and initialize it with a lexer.
 * 
 * @param lexer The lexer instance to use.
 * @return Parser* A new parser instance.
 */
Parser* parser_create(Lexer* lexer);

/**
 * @brief Advance the parser to the next token in the input.
 * 
 * @param parser The parser instance.
 */
void parser_advance(Parser* parser);

/**
 * @brief Free the memory allocated for an Abstract Syntax Tree (AST).
 * 
 * @param node The root of the AST to free.
 */
void free_ast(ASTNode* node);

/**
 * @brief Parse a factor, which could be a literal, unary operation, or a parenthesized sub-expression.
 *
 * A factor represents the lowest level of an arithmetic or logical expression,
 * such as numbers, strings, variables, or unary operations.
 *
 * @param parser The parser instance.
 * @return ASTNode* Pointer to the parsed factor node, or NULL if parsing fails.
 */
ASTNode* parse_factor(Parser* parser);

/**
 * @brief Parse the entire script and construct an AST.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The root node of the constructed AST.
 */
ASTNode* parse_script(Parser* parser);

/**
 * @brief Parse an expression (e.g., binary operations, literals).
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed expression node.
 */
ASTNode* parse_expression(Parser* parser, int min_precedence);

/**
 * @brief Parse a single statement (e.g., assignment, if statement).
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed statement node.
 */
ASTNode* parse_statement(Parser* parser);

/**
 * @brief Parse a block of statements enclosed in braces.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed block node.
 */
ASTNode* parse_block(Parser* parser);

/**
 * @brief Parse a function definition.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed function definition node.
 */
ASTNode* parse_function_definition(Parser* parser);

/**
 * @brief Parse an if statement with its condition and body.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed if statement node.
 */
ASTNode* parse_if_statement(Parser* parser);

/**
 * @brief Parse a while loop with its condition and body.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed while loop node.
 */
ASTNode* parse_while_loop(Parser* parser);

/**
 * @brief Parse a for loop, including its initializer, condition, increment, and body.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed for loop node.
 */
ASTNode* parse_for_loop(Parser* parser);

/**
 * @brief Parse a switch/case construct, including cases and a default case.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed switch/case node.
 */
ASTNode* parse_switch_case(Parser* parser);

/**
 * @brief Parse an assignment statement.
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed assignment node.
 */
ASTNode* parse_assignment(Parser* parser);

/**
 * @brief Parse a variable declaration (e.g., int x = 5;).
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed variable declaration node.
 */
ASTNode* parse_variable_declaration(Parser* parser);

/**
 * @brief Parse an anonymous block (e.g., a block of statements not tied to any specific construct).
 * 
 * @param parser The parser instance.
 * @return ASTNode* The parsed anonymous block node.
 */
ASTNode* parse_anonymous_block(Parser* parser);

/**
 * @brief Attempt to recover from a parsing error and continue parsing.
 * 
 * @param parser The parser instance.
 */
void parser_recover(Parser* parser);

/**
 * @brief Check if the current token matches the expected type and value.
 * 
 * @param parser The parser instance.
 * @param type The expected token type.
 * @param value The expected token value (can be NULL).
 * @return true If the current token matches.
 * @return false Otherwise.
 */
bool match_token(Parser* parser, ScriptTokenType type, const char* value);

/**
 * @brief Generate a parser error with a custom message.
 * 
 * @param parser The parser instance.
 * @param message The error message to report.
 * @return ParserError* A new error object containing the details.
 */
ParserError* parser_error(Parser* parser, const char* message);

/**
 * @brief Print an Abstract Syntax Tree (AST) in a human-readable format for debugging.
 * 
 * @param node The root node of the AST.
 * @param depth The current depth (used for indentation).
 */
void print_ast(const ASTNode* node, int depth);

/**
 * @brief Set a custom error callback for the parser.
 * 
 * @param parser The parser instance.
 * @param callback The error callback function to set.
 */
void parser_set_error_callback(Parser* parser, ParserErrorCallback callback);


#endif // PARSER_H
