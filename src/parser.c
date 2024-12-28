#include "parser.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void report_error(Parser* parser, char* message) {
    if (parser->error_callback) {
        ParserError error = {parser->lexer->line, parser->lexer->position, message};
        parser->error_callback(&error);
    } else {
        fprintf(stderr, "Parse error at line %d, column %d: %s\n",
                parser->lexer->line, parser->lexer->position, message);
    }
}

ASTNode* create_ast_node(ASTNodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Error: Memory allocation failed for AST node\n");
        return NULL;
    }
    memset(node, 0, sizeof(ASTNode));
    node->type = type;
    return node;
}

int get_operator_precedence(const char* op_symbol) {
    static const struct {
        const char* symbol;
        int precedence;
    } precedence_table[] = {
        {"||", 1},
        {"&&", 2},
        {"==", 3}, {"!=", 3},
        {"<", 4}, {"<=", 4}, {">", 4}, {">=", 4},
        {"+", 5}, {"-", 5},
        {"*", 6}, {"/", 6}, {"%", 6},
    };
    for (size_t i = 0; i < sizeof(precedence_table) / sizeof(precedence_table[0]); i++) {
        if (strcmp(precedence_table[i].symbol, op_symbol) == 0) {
            return precedence_table[i].precedence;
        }
    }
    return -1; // Unknown operator
}

Token peek_token(Parser* parser) {
    Lexer saved_lexer = *parser->lexer;
    Token next_token = lexer_next_token(&saved_lexer);
    return next_token;
}

Parser* parser_create(Lexer* lexer) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (!parser) {
        fprintf(stderr, "Error: Memory allocation failed for parser\n");
        return NULL;
    }
    parser->lexer = lexer;
    parser_advance(parser); // This sets the current_token
    parser->error_callback = NULL; // No error callback by default
    return parser;
}


void print_current_token(Parser* parser) {
    printf("Current token: type=%d, value=%s\n", parser->current_token.type, parser->current_token.value);
}

void parser_advance(Parser* parser) {
    if (!parser) {
        fprintf(stderr, "Error: Parser instance cannot be NULL\n");
        return;
    }

    // Advance to the next token from the lexer
    parser->current_token = lexer_next_token(parser->lexer);
}

void free_ast(ASTNode* node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_LITERAL:
            free(node->literal.value);
            break;

        case AST_BINARY_OP:
            free(node->binary_op.op_symbol);
            free_ast(node->binary_op.left);
            free_ast(node->binary_op.right);
            break;

        case AST_ASSIGNMENT:
            free(node->assignment.variable);
            free_ast(node->assignment.value);
            break;

        case AST_FUNCTION_CALL:
            free(node->function_call.function_name);
            for (int i = 0; i < node->function_call.argument_count; i++) {
                free_ast(node->function_call.arguments[i]);
            }
            free(node->function_call.arguments);
            break;

       case AST_IF_STATEMENT:
            free_ast(node->if_statement.condition);
            free_ast(node->if_statement.body);
            if (node->if_statement.else_body) {  // Add this check
                free_ast(node->if_statement.else_body);
            }
            break;

        case AST_WHILE_LOOP:
            free_ast(node->while_loop.condition);
            free_ast(node->while_loop.body);
            break;

        case AST_FOR_LOOP:
            free_ast(node->for_loop.initializer);
            free_ast(node->for_loop.condition);
            free_ast(node->for_loop.increment);
            free_ast(node->for_loop.body);
            break;

        case AST_LOGICAL_OP:
            free(node->logical_op.op_symbol);
            free_ast(node->logical_op.left);
            free_ast(node->logical_op.right);
            break;

        case AST_BLOCK:
            for (int i = 0; i < node->block.statement_count; i++) {
                free_ast(node->block.statements[i]);
            }
            free(node->block.statements);
            break;

        case AST_FUNCTION_DEF:
            free(node->function_def.function_name);
            for (int i = 0; i < node->function_def.parameter_count; i++) {
                free(node->function_def.parameters[i]);
            }
            free(node->function_def.parameters);
            free_ast(node->function_def.body);
            break;
        case AST_VARIABLE:
            free(node->variable.variable_name);
            break;

        case AST_VARIABLE_DECL:
            free(node->variable_decl.variable_name);
            if (node->variable_decl.initial_value) {
                free_ast(node->variable_decl.initial_value);
            }
            break;

        case AST_UNARY_OP:
            free(node->unary_op.op_symbol);
            free_ast(node->unary_op.operand);
            break;

        case AST_SWITCH_CASE:
            // Implement freeing logic for switch cases
            break;
        default:
            fprintf(stderr, "Error: Unknown AST node type\n");
            break;
    }

    free(node);
}

ASTNode* parse_script(Parser* parser) {
    // Allocate a block node to hold all top-level statements
    ASTNode* root = (ASTNode*)malloc(sizeof(ASTNode));
    if (!root) {
        fprintf(stderr, "Error: Memory allocation failed for script block\n");
        return NULL;
    }

    root->type = AST_BLOCK;
    root->block.statements = NULL;
    root->block.statement_count = 0;

    // Parse statements until the end of the script
    while (parser->current_token.type != TOKEN_EOF) {
        ASTNode* statement = parse_statement(parser);
        if (!statement) {
            fprintf(stderr, "Error: Failed to parse statement\n");
            free_ast(root);
            return NULL;
        }

        // Expand the block's statement array to accommodate the new statement
        root->block.statements = (ASTNode**)realloc(
            root->block.statements, 
            sizeof(ASTNode*) * (root->block.statement_count + 1)
        );
        if (!root->block.statements) {
            fprintf(stderr, "Error: Memory allocation failed for script statements\n");
            free_ast(statement);
            free_ast(root);
            return NULL;
        }

        // Add the statement to the block
        root->block.statements[root->block.statement_count] = statement;
        root->block.statement_count++;
    }

    return root;
}

ASTNode* parse_factor(Parser* parser) {
    ASTNode* factor_node = NULL;

    // Handle unary operators (e.g., -x, !x)
    if (parser->current_token.type == TOKEN_OPERATOR &&
        (strcmp(parser->current_token.value, "-") == 0 ||
         strcmp(parser->current_token.value, "!") == 0)) {
        // Save the operator
        char* operator = strdup(parser->current_token.value);
        if (!operator) {
            report_error(parser, "Memory allocation failed for operator");
            return NULL;
        }

        // Advance past the operator
        parser_advance(parser);

        // Parse the operand
        ASTNode* operand = parse_factor(parser);
        if (!operand) {
            report_error(parser, "Failed to parse operand for unary operation");
            free(operator);
            return NULL;
        }

        // Create a unary operation node
        ASTNode* unary_op = create_ast_node(AST_UNARY_OP);
        if (!unary_op) {
            report_error(parser, "Memory allocation failed for unary operation node");
            free(operator);
            free_ast(operand);
            return NULL;
        }

        unary_op->unary_op.op_symbol = operator;
        unary_op->unary_op.operand = operand;

        factor_node = unary_op;
    }
    // Handle literals (numbers, strings, booleans, null)
    else if (parser->current_token.type == TOKEN_NUMBER ||
        parser->current_token.type == TOKEN_STRING ||
        parser->current_token.type == TOKEN_BOOLEAN ||
        parser->current_token.type == TOKEN_NULL) {
        // Create a literal node
        ASTNode* literal = create_ast_node(AST_LITERAL);
        if (!literal) {
            report_error(parser, "Memory allocation failed for literal node");
            return NULL;
        }

        // Store the token type
        literal->literal.token_type = parser->current_token.type;

        literal->literal.value = strdup(parser->current_token.value);
        if (!literal->literal.value) {
            report_error(parser, "Memory allocation failed for literal value");
            free(literal);
            return NULL;
        }

        // Advance past the literal
        parser_advance(parser);
        factor_node = literal;
    }
    // Handle parentheses for sub-expressions
    else if (parser->current_token.type == TOKEN_PUNCTUATION &&
        strcmp(parser->current_token.value, "(") == 0) {
        // Advance past the opening parenthesis
        parser_advance(parser);

        // Parse the sub-expression
        ASTNode* expr = parse_expression(parser, 0);
        if (!expr) {
            report_error(parser, "Failed to parse sub-expression");
            return NULL;
        }

        // Expect a closing parenthesis
        if (parser->current_token.type != TOKEN_PUNCTUATION ||
            strcmp(parser->current_token.value, ")") != 0) {
            report_error(parser, "Expected closing parenthesis");
            free_ast(expr);
            return NULL;
        }

        // Advance past the closing parenthesis
        parser_advance(parser);
        factor_node = expr;
    }
    // Check for array literal: '['
    else if (parser->current_token.type == TOKEN_PUNCTUATION &&
        strcmp(parser->current_token.value, "[") == 0)
    {
        // Advance past '['
        parser_advance(parser);

        // Create the array literal node
        ASTNode* array_node = create_ast_node(AST_ARRAY_LITERAL);
        if (!array_node) {
            report_error(parser, "Failed to allocate AST_ARRAY_LITERAL node");
            return NULL;
        }

        // Prepare storage for elements
        array_node->array_literal.elements = NULL;
        array_node->array_literal.element_count = 0;

        // We might parse zero or more expressions, separated by commas, until we see ']'
        while (parser->current_token.type != TOKEN_PUNCTUATION ||
               strcmp(parser->current_token.value, "]") != 0)
        {
            // Parse an expression for each array element
            ASTNode* element = parse_expression(parser, 0);
            if (!element) {
                free_ast(array_node);
                return NULL;
            }

            // Grow the elements array by 1
            array_node->array_literal.element_count++;
            array_node->array_literal.elements = realloc(
                array_node->array_literal.elements,
                sizeof(ASTNode*) * array_node->array_literal.element_count
            );
            if (!array_node->array_literal.elements) {
                report_error(parser, "Memory allocation failed while parsing array elements");
                free_ast(element);
                free_ast(array_node);
                return NULL;
            }
            // Store the parsed element
            array_node->array_literal.elements[array_node->array_literal.element_count - 1] = element;

            // If the next token is a comma, consume it and continue
            if (parser->current_token.type == TOKEN_PUNCTUATION &&
                strcmp(parser->current_token.value, ",") == 0)
            {
                parser_advance(parser); // skip the comma
            }
            else {
                // Otherwise, break if we don't see a comma
                break;
            }
        }

        // Expect a closing bracket ']'
        if (parser->current_token.type != TOKEN_PUNCTUATION ||
            strcmp(parser->current_token.value, "]") != 0)
        {
            report_error(parser, "Expected ']' at the end of array literal");
            free_ast(array_node);
            return NULL;
        }

        // Consume the ']'
        parser_advance(parser);
        factor_node = array_node;
    }
    // Handle identifiers (variables and function calls)
    else if (parser->current_token.type == TOKEN_IDENTIFIER) {
        char* identifier = strdup(parser->current_token.value);
        if (!identifier) {
            report_error(parser, "Memory allocation failed for identifier");
            return NULL;
        }
        parser_advance(parser); // Advance past the identifier

        // Check if it's a function call
        if (parser->current_token.type == TOKEN_PUNCTUATION &&
            strcmp(parser->current_token.value, "(") == 0) {
            parser_advance(parser); // Skip '('

            // Parse arguments
            ASTNode** arguments = NULL;
            int argument_count = 0;

            if (parser->current_token.type != TOKEN_PUNCTUATION ||
                strcmp(parser->current_token.value, ")") != 0) {
                do {
                    ASTNode* arg = parse_expression(parser, 0);
                    if (!arg) {
                        report_error(parser, "Failed to parse function argument");
                        free(identifier);
                        // Free previously allocated arguments
                        for (int i = 0; i < argument_count; i++) {
                            free_ast(arguments[i]);
                        }
                        free(arguments);
                        return NULL;
                    }
                    ASTNode** temp = realloc(arguments, sizeof(ASTNode*) * (argument_count + 1));
                    if (!temp) {
                        report_error(parser, "Memory allocation failed for arguments");
                        free(identifier);
                        free_ast(arg);
                        for (int i = 0; i < argument_count; i++) {
                            free_ast(arguments[i]);
                        }
                        free(arguments);
                        return NULL;
                    }
                    arguments = temp;
                    arguments[argument_count++] = arg;

                    // Check for comma
                    if (parser->current_token.type == TOKEN_PUNCTUATION &&
                        strcmp(parser->current_token.value, ",") == 0) {
                        parser_advance(parser); // Skip ','
                    } else {
                        break; // No more arguments
                    }
                } while (1);

                // Expect a closing parenthesis ')'
                if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
                    report_error(parser, "Expected ')' after function arguments");
                    free(identifier);
                    for (int i = 0; i < argument_count; i++) {
                        free_ast(arguments[i]);
                    }
                    free(arguments);
                    return NULL;
                }
            } else {
                // No arguments; advance past ')'
                parser_advance(parser);
            }

            // Create function call node
            ASTNode* func_call = create_ast_node(AST_FUNCTION_CALL);
            if (!func_call) {
                report_error(parser, "Memory allocation failed for function call node");
                free(identifier);
                for (int i = 0; i < argument_count; i++) {
                    free_ast(arguments[i]);
                }
                free(arguments);
                return NULL;
            }
            func_call->function_call.function_name = identifier;
            func_call->function_call.arguments = arguments;
            func_call->function_call.argument_count = argument_count;

            factor_node = func_call;
        } else {
            // Variable reference
            ASTNode* var_node = create_ast_node(AST_VARIABLE);
            if (!var_node) {
                report_error(parser, "Memory allocation failed for variable node");
                free(identifier);
                return NULL;
            }
            var_node->variable.variable_name = identifier;
            factor_node = var_node;
        }
    } else {
        // If none of the above, return NULL (syntax error)
        report_error(parser, "Unexpected token");
        return NULL;
    }

     while (parser->current_token.type == TOKEN_PUNCTUATION &&
           strcmp(parser->current_token.value, "[") == 0)
    {
        // We have an index access, e.g. "myArray[ indexExpr ]"
        parser_advance(parser); // skip '['

        // parse the expression inside [ ... ]
        ASTNode* index_expr = parse_expression(parser, 0);
        if (!index_expr) {
            free_ast(factor_node);
            return NULL;
        }

        // Expect a closing bracket ']'
        if (parser->current_token.type != TOKEN_PUNCTUATION ||
            strcmp(parser->current_token.value, "]") != 0)
        {
            report_error(parser, "Expected ']' after array index expression");
            free_ast(factor_node);
            free_ast(index_expr);
            return NULL;
        }
        parser_advance(parser); // skip ']'

        // Build an AST_INDEX_ACCESS node
        ASTNode* index_node = create_ast_node(AST_INDEX_ACCESS);  // <-- you must define AST_INDEX_ACCESS
        if (!index_node) {
            report_error(parser, "Memory allocation failed for AST_INDEX_ACCESS");
            free_ast(factor_node);
            free_ast(index_expr);
            return NULL;
        }

        // store "myArray" in array_expr, "indexExpr" in index_expr
        index_node->index_access.array_expr = factor_node;
        index_node->index_access.index_expr = index_expr;

        // Now this index_node becomes the new 'factor_node',
        // in case there is another bracket: items[0][1]
        factor_node = index_node;
    }

    // Finally, return the constructed factor_node
    return factor_node;
}

ASTNode* parse_expression(Parser* parser, int min_precedence) {
    // 1. Parse the initial left-hand side (factor)
    ASTNode* left = parse_factor(parser);
    if (!left) {
        fprintf(stderr, "Error: Failed to parse left-hand side of expression\n");
        return NULL;
    }

    // 2. Loop to handle multiple operators in sequence
    //    (e.g., left + right + right2, etc.)
    while (true) {
        // --- A) Check for assignment operator first (lowest precedence) ---
        // If the current token is '=' then we treat that as an assignment expression.
        // This effectively short-circuits all the usual precedence checks because
        // assignment is typically the lowest-precedence operator.
        if (parser->current_token.type == TOKEN_OPERATOR &&
            strcmp(parser->current_token.value, "=") == 0)
        {
            // Consume '='
            parser_advance(parser);

            // Parse the right-hand side of the assignment with the lowest precedence (0)
            ASTNode* right = parse_expression(parser, 0);
            if (!right) {
                fprintf(stderr, "Error: Failed to parse right-hand side of assignment\n");
                free_ast(left);
                return NULL;
            }

            // Build an AST_ASSIGNMENT node
            ASTNode* assignment_node = create_ast_node(AST_ASSIGNMENT);
            if (!assignment_node) {
                fprintf(stderr, "Error: Memory allocation failed for assignment node\n");
                free_ast(left);
                free_ast(right);
                return NULL;
            }

             if (left->type != AST_VARIABLE) {
                 report_error(parser, "Left-hand side of '=' must be a variable");
                 free_ast(left);
                 free_ast(right);
                 free_ast(assignment_node);
                 return NULL;
             }

            // Transfer or copy the variable name from 'left' if it's AST_VARIABLE
            if (left->type == AST_VARIABLE) {
                // Take ownership of the name pointer
                assignment_node->assignment.variable = left->variable.variable_name;
                // We do NOT free left->variable.variable_name here, because we just moved it
                // or we can do a strdup if we want a brand new copy, etc.
                left->variable.variable_name = NULL; 
            } else {
                // If you don't enforce variable left-sides, you might do something else:
                assignment_node->assignment.variable = strdup("<nonVariable>");
            }

            // Attach the right side
            assignment_node->assignment.value = right;

            // We no longer need 'left' as an AST node
            free(left);
            
            // The assignment node becomes our new "left"
            left = assignment_node;
        }
        // --- B) Otherwise, check for other operators (+, -, *, /, etc.) by precedence ---
        else if (parser->current_token.type == TOKEN_OPERATOR) {
            char* op = parser->current_token.value;
            int precedence = get_operator_precedence(op);

            // If the next operator's precedence is lower than the min_precedence we expect,
            // we break out of the loop and return what we have so far.
            if (precedence < min_precedence) {
                break;
            }

            // Otherwise, consume the operator
            char* operator = strdup(op);
            if (!operator) {
                fprintf(stderr, "Error: Memory allocation failed for operator\n");
                free_ast(left);
                return NULL;
            }
            parser_advance(parser);

            // Parse the right-hand side with precedence = (current precedence + 1)
            // so that we handle left-recursive expressions properly
            ASTNode* right = parse_expression(parser, precedence + 1);
            if (!right) {
                fprintf(stderr, "Error: Failed to parse right-hand side of expression\n");
                free(operator);
                free_ast(left);
                return NULL;
            }

            // Create a BinaryOp node
            ASTNode* binary_op = create_ast_node(AST_BINARY_OP);
            if (!binary_op) {
                fprintf(stderr, "Error: Memory allocation failed for binary operation node\n");
                free(operator);
                free_ast(left);
                free_ast(right);
                return NULL;
            }

            // Hook up left, operator, right
            binary_op->binary_op.left = left;
            binary_op->binary_op.right = right;
            binary_op->binary_op.op_symbol = operator;

            // That becomes our new left side
            left = binary_op;
        }
        // --- C) If it's not assignment or a recognized operator, we stop here ---
        else {
            break;
        }
    }

    return left;
}

ASTNode* parse_statement(Parser* parser) {
    // Match an if statement
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "if") == 0) {
        return parse_if_statement(parser);
    }

    // Match a while loop
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "while") == 0) {
        return parse_while_loop(parser);
    }

    // Match a for loop
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "for") == 0) {
        return parse_for_loop(parser);
    }

    // Match a function definition
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "function") == 0) {
        return parse_function_definition(parser);
    }

    // Match a block
    if (parser->current_token.type == TOKEN_PUNCTUATION &&
        strcmp(parser->current_token.value, "{") == 0) {
        return parse_block(parser);
    }

    // Match a variable declaration
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "var") == 0) {
        return parse_variable_declaration(parser, false);
    }

    // Match an assignment
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        // Peek ahead to check for assignment operator '='
        Token next_token = peek_token(parser);
        if (next_token.type == TOKEN_OPERATOR && strcmp(next_token.value, "=") == 0) {
            return parse_assignment(parser);
        }
    }

    // Match a standalone expression (e.g., function call or binary operation)
    ASTNode* expression = parse_expression(parser, 0);
    if (expression) {
        // Expect a semicolon after the statement
        if (!match_token(parser, TOKEN_PUNCTUATION, ";")) {
            report_error(parser, "Expected ';' after statement");
            free_ast(expression);
            return NULL;
        }
        return expression;
    }

    report_error(parser, "Unexpected statement");
    parser_recover(parser);
    return NULL;
}

ASTNode* parse_block(Parser* parser) {
    // Ensure the block starts with '{'
    if (!match_token(parser, TOKEN_PUNCTUATION, "{")) {
        report_error(parser, "Expected '{' to start block");
        return NULL;
    }

    // Allocate memory for the block node
    ASTNode* block_node = create_ast_node(AST_BLOCK);
    if (!block_node) {
        report_error(parser, "Memory allocation failed for block node");
        return NULL;
    }

    block_node->block.statements = NULL;
    block_node->block.statement_count = 0;

    // Parse statements until we encounter '}'
    while (parser->current_token.type != TOKEN_PUNCTUATION ||
           strcmp(parser->current_token.value, "}") != 0) {
        ASTNode* statement = parse_statement(parser);
        if (!statement) {
            // Handle parsing error within the block
            free_ast(block_node);
            return NULL;
        }

        // Add the parsed statement to the block's statements array
        ASTNode** temp = realloc(block_node->block.statements,
                                 sizeof(ASTNode*) * (block_node->block.statement_count + 1));
        if (!temp) {
            report_error(parser, "Memory allocation failed for block statements");
            free_ast(statement);
            free_ast(block_node);
            return NULL;
        }
        block_node->block.statements = temp;
        block_node->block.statements[block_node->block.statement_count++] = statement;
    }

    // After the loop, consume the closing '}'
    if (!match_token(parser, TOKEN_PUNCTUATION, "}")) {
        report_error(parser, "Expected '}' to close block");
        free_ast(block_node);
        return NULL;
    }

    return block_node;
}

ASTNode* parse_function_definition(Parser* parser) {
    // Ensure the function definition starts with the "function" keyword
    if (!match_token(parser, TOKEN_KEYWORD, "function")) {
        report_error(parser, "Expected 'function' keyword");
        return NULL;
    }

    // Expect a function name (identifier)
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        report_error(parser, "Expected function name after 'function'");
        return NULL;
    }

    // Capture the function name
    char* function_name = strdup(parser->current_token.value);
    if (!function_name) {
        report_error(parser, "Memory allocation failed for function name");
        return NULL;
    }
    parser_advance(parser);

    // Expect an opening parenthesis '('
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after function name");
        free(function_name);
        return NULL;
    }

    // Parse parameters
    char** parameters = NULL;
    int parameter_count = 0;

    // While the next token is not ')', parse parameters
    while (parser->current_token.type != TOKEN_PUNCTUATION ||
           strcmp(parser->current_token.value, ")") != 0) {

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            report_error(parser, "Expected parameter name");
            free(function_name);
            for (int i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }

        // Capture parameter name
        char* param_name = strdup(parser->current_token.value);
        if (!param_name) {
            report_error(parser, "Memory allocation failed for parameter name");
            free(function_name);
            for (int i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }

        // Add parameter name to the list
        char** temp = realloc(parameters, sizeof(char*) * (parameter_count + 1));
        if (!temp) {
            report_error(parser, "Memory allocation failed for parameters");
            free(param_name);
            free(function_name);
            for (int i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
        parameters = temp;
        parameters[parameter_count++] = param_name;

        parser_advance(parser);

        // If next token is ',', skip it and continue parsing parameters
        if (parser->current_token.type == TOKEN_PUNCTUATION &&
            strcmp(parser->current_token.value, ",") == 0) {
            parser_advance(parser);
        } else if (parser->current_token.type == TOKEN_PUNCTUATION &&
                   strcmp(parser->current_token.value, ")") == 0) {
            // End of parameter list
            break;
        } else {
            report_error(parser, "Expected ',' or ')' in parameter list");
            free(function_name);
            for (int i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
    }

    // Consume the closing parenthesis ')'
    if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
        report_error(parser, "Expected ')' after parameters");
        free(function_name);
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    // Parse the function body
    ASTNode* body = parse_block(parser);
    if (!body) {
        report_error(parser, "Failed to parse function body");
        free(function_name);
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    // Create the function definition AST node
    ASTNode* function_def_node = create_ast_node(AST_FUNCTION_DEF);
    if (!function_def_node) {
        report_error(parser, "Memory allocation failed for function definition node");
        free(function_name);
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        free_ast(body);
        return NULL;
    }

    function_def_node->function_def.function_name = function_name;
    function_def_node->function_def.parameters = parameters;
    function_def_node->function_def.parameter_count = parameter_count;
    function_def_node->function_def.body = body;

    return function_def_node;
}

ASTNode* parse_if_statement(Parser* parser) {
    // Ensure the statement starts with the "if" keyword
    if (!match_token(parser, TOKEN_KEYWORD, "if")) {
        fprintf(stderr, "Error: Expected 'if' keyword\n");
        return NULL;
    }

    // Expect an opening parenthesis '(' for the condition
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'if'");
        return NULL;
    }

    // Parse the condition
    ASTNode* condition = parse_expression(parser, 0);
    if (!condition) {
        fprintf(stderr, "Error: Failed to parse condition in 'if' statement\n");
        return NULL;
    }

    // Expect a closing parenthesis ')'
    if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
        report_error(parser, "Expected ')' after condition in 'if' statement");
        free_ast(condition);
        return NULL;
    }

    // Parse the body of the if statement
    ASTNode* body = parse_block(parser);
    if (!body) {
        fprintf(stderr, "Error: Failed to parse body of 'if' statement\n");
        free_ast(condition);
        return NULL;
    }

    // Create the if statement AST node
    ASTNode* if_node = create_ast_node(AST_IF_STATEMENT);
    if (!if_node) {
        fprintf(stderr, "Error: Memory allocation failed for 'if' statement node\n");
        free_ast(condition);
        free_ast(body);
        return NULL;
    }

    if_node->if_statement.condition = condition;
    if_node->if_statement.body = body;
    if_node->if_statement.else_body = NULL;

    // Check for 'else' clause
    if (match_token(parser, TOKEN_KEYWORD, "else")) {
        // Peek to see if the next token is 'if' without advancing
        if (parser->current_token.type == TOKEN_KEYWORD && strcmp(parser->current_token.value, "if") == 0) {
            // Handle 'else if' by recursively parsing as an 'if' statement
            ASTNode* else_if_node = parse_if_statement(parser);
            if (!else_if_node) {
                fprintf(stderr, "Error: Failed to parse 'else if' statement\n");
                free_ast(if_node);
                return NULL;
            }
            if_node->if_statement.else_body = else_if_node;
        } else {
            // Parse else block
            ASTNode* else_body = parse_block(parser);
            if (!else_body) {
                fprintf(stderr, "Error: Failed to parse 'else' body\n");
                free_ast(if_node);
                return NULL;
            }
            if_node->if_statement.else_body = else_body;
        }
    }

    return if_node;
}


ASTNode* parse_while_loop(Parser* parser) {
    // Ensure the statement starts with the "while" keyword
    if (!match_token(parser, TOKEN_KEYWORD, "while")) {
        fprintf(stderr, "Error: Expected 'while' keyword\n");
        return NULL;
    }

    // Expect an opening parenthesis '(' for the condition
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'while'");
        return NULL;
    }

    // Parse the condition
    ASTNode* condition = parse_expression(parser, 0);
    if (!condition) {
        fprintf(stderr, "Error: Failed to parse condition in 'while' loop\n");
        return NULL;
    }

    // Expect a closing parenthesis ')'
    if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
        report_error(parser, "Expected ')' after condition in 'while' loop");
        free_ast(condition);
        return NULL;
    }

    // Parse the body of the while loop
    ASTNode* body = parse_block(parser);
    if (!body) {
        fprintf(stderr, "Error: Failed to parse body of 'while' loop\n");
        free_ast(condition);
        return NULL;
    }

    // Create the while loop AST node
    ASTNode* while_node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!while_node) {
        fprintf(stderr, "Error: Memory allocation failed for 'while' loop node\n");
        free_ast(condition);
        free_ast(body);
        return NULL;
    }

    while_node->type = AST_WHILE_LOOP;
    while_node->while_loop.condition = condition;
    while_node->while_loop.body = body;

    return while_node;
}

ASTNode* parse_for_loop(Parser* parser) {
    // Ensure the statement starts with the "for" keyword
    if (!match_token(parser, TOKEN_KEYWORD, "for")) {
        fprintf(stderr, "Error: Expected 'for' keyword\n");
        return NULL;
    }

    // Expect an opening parenthesis '('
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'for'");
        return NULL;
    }

    //-------------------------------------------
    // 1) Parse the initializer (optional)
    //-------------------------------------------
    ASTNode* initializer = NULL;
    // If the current token isn't an immediate semicolon, we parse something:
    if (!(parser->current_token.type == TOKEN_PUNCTUATION &&
          strcmp(parser->current_token.value, ";") == 0))
    {
        // If it's 'var', 'let', or 'const' => parse a variable declaration in for-header mode
        if (parser->current_token.type == TOKEN_KEYWORD &&
            (strcmp(parser->current_token.value, "var") == 0 ||
             strcmp(parser->current_token.value, "let") == 0 ||
             strcmp(parser->current_token.value, "const") == 0))
        {
            // parse_variable_declaration(..., true) means "don't consume a trailing semicolon here"
            initializer = parse_variable_declaration(parser, true);
        }
        else {
            // Otherwise parse an expression initializer (like i = 0)
            initializer = parse_expression(parser, 0);
        }
    }

    // Now we consume the semicolon that ends the for-header "initializer" part
    if (!match_token(parser, TOKEN_PUNCTUATION, ";")) {
        report_error(parser, "Expected ';' after initializer in 'for' loop");
        free_ast(initializer);
        return NULL;
    }

    //-------------------------------------------
    // 2) Parse the condition (optional)
    //-------------------------------------------
    ASTNode* condition = NULL;
    // If the next token is not an immediate semicolon, parse an expression for the condition
    if (!(parser->current_token.type == TOKEN_PUNCTUATION &&
          strcmp(parser->current_token.value, ";") == 0))
    {
        condition = parse_expression(parser, 0);
        if (!condition) {
            fprintf(stderr, "Error: Failed to parse condition in 'for' loop\n");
            free_ast(initializer);
            return NULL;
        }
    }

    // Then consume the semicolon separating condition from increment
    if (!match_token(parser, TOKEN_PUNCTUATION, ";")) {
        report_error(parser, "Expected ';' after condition in 'for' loop");
        free_ast(initializer);
        free_ast(condition);
        return NULL;
    }

    //-------------------------------------------
    // 3) Parse the increment (optional)
    //-------------------------------------------
    ASTNode* increment = NULL;
    // If we don't see a closing parenthesis, parse an expression
    if (!(parser->current_token.type == TOKEN_PUNCTUATION &&
          strcmp(parser->current_token.value, ")") == 0))
    {
        increment = parse_expression(parser, 0);
        if (!increment) {
            fprintf(stderr, "Error: Failed to parse increment in 'for' loop\n");
            free_ast(initializer);
            free_ast(condition);
            return NULL;
        }
    }

    // Expect a closing parenthesis ')' after the increment
    if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
        report_error(parser, "Expected ')' after increment in 'for' loop");
        free_ast(initializer);
        free_ast(condition);
        free_ast(increment);
        return NULL;
    }

    //-------------------------------------------
    // 4) Parse the loop body
    //-------------------------------------------
    ASTNode* body = parse_block(parser);
    if (!body) {
        fprintf(stderr, "Error: Failed to parse body of 'for' loop\n");
        free_ast(initializer);
        free_ast(condition);
        free_ast(increment);
        return NULL;
    }

    // Create the for loop AST node
    ASTNode* for_node = create_ast_node(AST_FOR_LOOP);
    if (!for_node) {
        fprintf(stderr, "Error: Memory allocation failed for 'for' loop node\n");
        free_ast(initializer);
        free_ast(condition);
        free_ast(increment);
        free_ast(body);
        return NULL;
    }

    for_node->for_loop.initializer = initializer;
    for_node->for_loop.condition   = condition;
    for_node->for_loop.increment   = increment;
    for_node->for_loop.body        = body;

    return for_node;
}

ASTNode* parse_switch_case(Parser* parser) {
    // Ensure the current token is "switch"
    if (parser->current_token.type != TOKEN_KEYWORD || strcmp(parser->current_token.value, "switch") != 0) {
        fprintf(stderr, "Error: Expected 'switch' keyword\n");
        return NULL;
    }

    parser_advance(parser); // Advance past "switch"

    // Parse the condition in parentheses
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'switch'");
        return NULL;
    }

    parser_advance(parser); // Skip '('
    ASTNode* condition = parse_expression(parser, 0);
    if (!condition) {
        fprintf(stderr, "Error: Failed to parse switch condition\n");
        return NULL;
    }

    if (!match_token(parser, TOKEN_OPERATOR, ")")) {
        fprintf(stderr, "Error: Expected ')' after switch condition\n");
        free_ast(condition);
        return NULL;
    }

    parser_advance(parser); // Skip ')'

    // Parse the switch block
    if (!match_token(parser, TOKEN_PUNCTUATION, "{")) {
        report_error(parser, "Expected '{' after switch condition");
        free_ast(condition);
        return NULL;
    }

    parser_advance(parser); // Skip '{'

    // Initialize the switch_case node
    ASTNode* switch_node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!switch_node) {
        fprintf(stderr, "Error: Memory allocation failed for switch node\n");
        free_ast(condition);
        return NULL;
    }

    switch_node->type = AST_SWITCH_CASE;
    switch_node->switch_case.condition = condition;
    switch_node->switch_case.cases = NULL;
    switch_node->switch_case.default_case = NULL;
    switch_node->switch_case.case_count = 0;

    // Parse cases and default case
    while (!match_token(parser, TOKEN_OPERATOR, "}")) {
        if (match_token(parser, TOKEN_KEYWORD, "case")) {
            parser_advance(parser); // Skip "case"

            // Parse the case value
            ASTNode* case_value = parse_expression(parser, 0);
            if (!case_value) {
                fprintf(stderr, "Error: Failed to parse case value\n");
                free_ast(switch_node);
                return NULL;
            }

            if (!match_token(parser, TOKEN_PUNCTUATION, ":")) {
                report_error(parser, "Expected ':' after case value");
                free_ast(case_value);
                free_ast(switch_node);
                return NULL;
            }

            parser_advance(parser); // Skip ':'

            // Parse the case body
            ASTNode* case_body = parse_block(parser);
            if (!case_body) {
                fprintf(stderr, "Error: Failed to parse case body\n");
                free_ast(case_value);
                free_ast(switch_node);
                return NULL;
            }

            // Add the case to the cases array
            switch_node->switch_case.case_count++;
            switch_node->switch_case.cases = realloc(switch_node->switch_case.cases,
                sizeof(ASTNode*) * switch_node->switch_case.case_count);
            if (!switch_node->switch_case.cases) {
                fprintf(stderr, "Error: Memory allocation failed for switch cases\n");
                free_ast(case_value);
                free_ast(case_body);
                free_ast(switch_node);
                return NULL;
            }

            // Create a case node and add it
            ASTNode* case_node = (ASTNode*)malloc(sizeof(ASTNode));
            if (!case_node) {
                fprintf(stderr, "Error: Memory allocation failed for case node\n");
                free_ast(case_value);
                free_ast(case_body);
                free_ast(switch_node);
                return NULL;
            }

            case_node->type = AST_BLOCK; // Each case is treated as a block
            case_node->block.statements = malloc(2 * sizeof(ASTNode*));
            case_node->block.statements[0] = case_value;
            case_node->block.statements[1] = case_body;
            case_node->block.statement_count = 2;

            switch_node->switch_case.cases[switch_node->switch_case.case_count - 1] = case_node;
        } else if (match_token(parser, TOKEN_KEYWORD, "default")) {
            parser_advance(parser); // Skip "default"

            if (!match_token(parser, TOKEN_OPERATOR, ":")) {
                fprintf(stderr, "Error: Expected ':' after default\n");
                free_ast(switch_node);
                return NULL;
            }

            parser_advance(parser); // Skip ':'

            // Parse the default case body
            ASTNode* default_body = parse_block(parser);
            if (!default_body) {
                fprintf(stderr, "Error: Failed to parse default body\n");
                free_ast(switch_node);
                return NULL;
            }

            switch_node->switch_case.default_case = default_body;
        } else {
            fprintf(stderr, "Error: Unexpected token in switch statement\n");
            free_ast(switch_node);
            return NULL;
        }
    }

    parser_advance(parser); // Skip '}'
    return switch_node;
}


ASTNode* parse_assignment(Parser* parser) {
    // Ensure the current token is an identifier
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected an identifier for assignment\n");
        return NULL;
    }

    // Store the variable name
    char* variable_name = strdup(parser->current_token.value);
    if (!variable_name) {
        fprintf(stderr, "Error: Memory allocation failed for variable name\n");
        return NULL;
    }

    // Advance to the next token
    parser_advance(parser);

    // Ensure the next token is the '=' operator
    if (parser->current_token.type != TOKEN_OPERATOR || strcmp(parser->current_token.value, "=") != 0) {
        report_error(parser, "Expected '=' in assignment statement");
        free(variable_name);
        return NULL;
    }

    // Advance to the next token
    parser_advance(parser);

    // Parse the value (right-hand side of the assignment)
    ASTNode* value_node = parse_expression(parser, 0);
    if (!value_node) {
        fprintf(stderr, "Error: Failed to parse right-hand side of assignment\n");
        free(variable_name);
        return NULL;
    }

    // Create the assignment node
    ASTNode* assignment_node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!assignment_node) {
        fprintf(stderr, "Error: Memory allocation failed for assignment node\n");
        free(variable_name);
        free_ast(value_node);
        return NULL;
    }

    // Expect a semicolon ';' after the assignment
    if (!match_token(parser, TOKEN_PUNCTUATION, ";")) {
        report_error(parser, "Expected ';' after assignment");
        free(variable_name);
        free_ast(value_node);
        free(assignment_node);
        return NULL;
    }

    assignment_node->type = AST_ASSIGNMENT;
    assignment_node->assignment.variable = variable_name;
    assignment_node->assignment.value = value_node;

    return assignment_node;
}

ASTNode* parse_variable_declaration(Parser* parser, bool inForHeader) {
    // Ensure the current token is a keyword for variable declaration (e.g., "var", "let", "const")
    if (parser->current_token.type != TOKEN_KEYWORD ||
        (strcmp(parser->current_token.value, "var") != 0 &&
         strcmp(parser->current_token.value, "let") != 0 &&
         strcmp(parser->current_token.value, "const") != 0))
    {
        fprintf(stderr, "Error: Expected a variable declaration keyword (e.g., var, let, const)\n");
        return NULL;
    }

    // Advance past the declaration keyword
    parser_advance(parser); // skip 'var', 'let', or 'const'

    // Ensure the next token is an identifier (variable name)
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected an identifier for variable declaration\n");
        return NULL;
    }

    // Store the variable name
    char* variable_name = strdup(parser->current_token.value);
    if (!variable_name) {
        fprintf(stderr, "Error: Memory allocation failed for variable name\n");
        return NULL;
    }
    parser_advance(parser); // Skip the variable name

    // Check for an optional initializer (e.g., "var x = 5")
    ASTNode* initial_value = NULL;
    if (parser->current_token.type == TOKEN_OPERATOR &&
        strcmp(parser->current_token.value, "=") == 0)
    {
        // Advance past the '=' operator
        parser_advance(parser);

        // Parse the initializer expression
        initial_value = parse_expression(parser, 0);
        if (!initial_value) {
            fprintf(stderr, "Error: Failed to parse initializer for variable declaration\n");
            free(variable_name);
            return NULL;
        }
    }

    // Create the variable declaration node
    ASTNode* variable_decl_node = create_ast_node(AST_VARIABLE_DECL);
    if (!variable_decl_node) {
        fprintf(stderr, "Error: Memory allocation failed for variable declaration node\n");
        free(variable_name);
        if (initial_value) free_ast(initial_value);
        return NULL;
    }
    variable_decl_node->variable_decl.variable_name = variable_name;
    variable_decl_node->variable_decl.initial_value = initial_value;

    // If this is a STANDALONE declaration (not in for-header), consume the semicolon now.
    // If it's inside a for-loop header, we'll rely on parse_for_loop() to handle the ';'.
    if (!inForHeader) {
        if (!match_token(parser, TOKEN_PUNCTUATION, ";")) {
            report_error(parser, "Expected ';' after variable declaration");
            free(variable_name);
            if (initial_value) free_ast(initial_value);
            free(variable_decl_node);
            return NULL;
        }
    }

    return variable_decl_node;
}

ASTNode* parse_anonymous_block(Parser* parser) {
    if (!match_token(parser, TOKEN_OPERATOR, "{")) {
        fprintf(stderr, "Error: Expected '{' to start anonymous block.\n");
        return NULL;
    }

    // Create a block node
    ASTNode* block_node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!block_node) {
        fprintf(stderr, "Error: Memory allocation failed for anonymous block.\n");
        return NULL;
    }

    block_node->type = AST_BLOCK;
    block_node->block.statements = NULL;
    block_node->block.statement_count = 0;

    // Parse statements inside the block
    while (!match_token(parser, TOKEN_OPERATOR, "}")) {
        ASTNode* statement = parse_statement(parser);
        if (!statement) {
            fprintf(stderr, "Error: Failed to parse statement inside anonymous block.\n");
            free_ast(block_node);
            return NULL;
        }

        // Grow the statements array
        block_node->block.statement_count++;
        block_node->block.statements = (ASTNode**)realloc(
            block_node->block.statements,
            block_node->block.statement_count * sizeof(ASTNode*)
        );

        if (!block_node->block.statements) {
            fprintf(stderr, "Error: Memory allocation failed for block statements.\n");
            free_ast(block_node);
            return NULL;
        }

        block_node->block.statements[block_node->block.statement_count - 1] = statement;
    }

    // Advance past the closing '}'
    parser_advance(parser);

    return block_node;
}

void parser_recover(Parser* parser) {
    // Advance tokens until we find a statement boundary or EOF
    while (parser->current_token.type != TOKEN_EOF) {
        // Check for a token that indicates the end of a statement or block
        if (parser->current_token.type == TOKEN_PUNCTUATION &&
            (strcmp(parser->current_token.value, ";") == 0 ||
            strcmp(parser->current_token.value, "}") == 0)) {
            parser_advance(parser); // Advance past the recovery point
            return;
        }

        // Otherwise, keep advancing
        parser_advance(parser);
    }
}

bool match_token(Parser* parser, ScriptTokenType type, const char* value) {
    // Check if the current token matches the expected type
    if (parser->current_token.type != type) {
        return false;
    }

    // If a specific value is provided, check if it matches the current token value
    if (value != NULL && strcmp(parser->current_token.value, value) != 0) {
        return false;
    }

    // Token matches; advance to the next token
    parser_advance(parser);
    return true;
}

ParserError* parser_error(Parser* parser, const char* message) {
    // Allocate memory for the error object
    ParserError* error = (ParserError*)malloc(sizeof(ParserError));
    if (!error) {
        fprintf(stderr, "Error: Memory allocation failed for ParserError.\n");
        return NULL;
    }

    // Set the error properties
    error->line = parser->lexer->line;
    error->column = parser->lexer->position;
    error->message = strdup(message); // Duplicate the error message for safe storage

    // Print the error to standard error for immediate feedback
    fprintf(stderr, "Parser Error at line %d, column %d: %s\n", error->line, error->column, error->message);

    return error;
}

void print_ast(const ASTNode* node, int depth) {
    if (!node) {
        return;
    }

    // Print indentation based on depth
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print the node type
    switch (node->type) {
        case AST_LITERAL:
            printf("Literal: %s\n", node->literal.value);
            break;

        case AST_BINARY_OP:
            printf("Binary Operation: %s\n", node->binary_op.op_symbol);
            print_ast(node->binary_op.left, depth + 1);
            print_ast(node->binary_op.right, depth + 1);
            break;

        case AST_ASSIGNMENT:
            printf("Assignment: %s\n", node->assignment.variable);
            print_ast(node->assignment.value, depth + 1);
            break;

        case AST_FUNCTION_CALL:
            printf("Function Call: %s\n", node->function_call.function_name);
            for (int i = 0; i < node->function_call.argument_count; i++) {
                print_ast(node->function_call.arguments[i], depth + 1);
            }
            break;

       case AST_IF_STATEMENT:
            printf("If Statement:\n");
            printf("  Condition:\n");
            print_ast(node->if_statement.condition, depth + 1);
            printf("  Body:\n");
            print_ast(node->if_statement.body, depth + 1);
            if (node->if_statement.else_body) {
                printf("  Else Body:\n");
                print_ast(node->if_statement.else_body, depth + 1);
            }
            break;

        case AST_WHILE_LOOP:
            printf("While Loop:\n");
            printf("  Condition:\n");
            print_ast(node->while_loop.condition, depth + 1);
            printf("  Body:\n");
            print_ast(node->while_loop.body, depth + 1);
            break;

        case AST_FOR_LOOP:
            printf("For Loop:\n");
            printf("  Initializer:\n");
            print_ast(node->for_loop.initializer, depth + 1);
            printf("  Condition:\n");
            print_ast(node->for_loop.condition, depth + 1);
            printf("  Increment:\n");
            print_ast(node->for_loop.increment, depth + 1);
            printf("  Body:\n");
            print_ast(node->for_loop.body, depth + 1);
            break;

        case AST_LOGICAL_OP:
            printf("Logical Operation: %s\n", node->logical_op.op_symbol);
            print_ast(node->logical_op.left, depth + 1);
            print_ast(node->logical_op.right, depth + 1);
            break;

        case AST_BLOCK:
            printf("Block:\n");
            for (int i = 0; i < node->block.statement_count; i++) {
                print_ast(node->block.statements[i], depth + 1);
            }
            break;

        case AST_FUNCTION_DEF:
            printf("Function Definition: %s\n", node->function_def.function_name);
            printf("  Parameters:\n");
            for (int i = 0; i < node->function_def.parameter_count; i++) {
                for (int j = 0; j < depth + 2; j++) {
                    printf("  ");
                }
                printf("%s\n", node->function_def.parameters[i]);
            }
            printf("  Body:\n");
            print_ast(node->function_def.body, depth + 1);
            break;

        case AST_SWITCH_CASE:
            printf("Switch Statement:\n");
            printf("  Condition:\n");
            print_ast(node->switch_case.condition, depth + 1);
            printf("  Cases:\n");
            for (int i = 0; i < node->switch_case.case_count; i++) {
                print_ast(node->switch_case.cases[i], depth + 1);
            }
            if (node->switch_case.default_case) {
                printf("  Default Case:\n");
                print_ast(node->switch_case.default_case, depth + 1);
            }
            break;

        default:
            printf("Unknown AST Node Type\n");
            break;
    }
}

void parser_set_error_callback(Parser* parser, ParserErrorCallback callback) {
    if (!parser) {
        fprintf(stderr, "Error: Attempted to set error callback on a NULL parser.\n");
        return;
    }

    parser->error_callback = callback;
}