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
        case AST_ARRAY_LITERAL:
            // Free each element in the array
            for (int i = 0; i < node->array_literal.element_count; i++) {
                free_ast(node->array_literal.elements[i]);
            }
            // Free the array of element pointers
            free(node->array_literal.elements);
            break;

        case AST_INDEX_ACCESS:
            // Free the array expression
            free_ast(node->index_access.array_expr);
            // Free the index expression
            free_ast(node->index_access.index_expr);
            break; 
        case AST_UNARY_OP:
            free(node->unary_op.op_symbol);
            free_ast(node->unary_op.operand);
            break;

        case AST_SWITCH_CASE:
            // Implement freeing logic for switch cases
            break;
        case AST_IMPORT:
            free(node->import_stmt.import_path);
            break;
        case AST_OBJECT_LITERAL:
            // Free keys
            for (int i = 0; i < node->object_literal.property_count; i++) {
                free(node->object_literal.keys[i]);
            }
            free(node->object_literal.keys);
            // Free values
            for (int i = 0; i < node->object_literal.property_count; i++) {
                free_ast(node->object_literal.values[i]);
            }
            free(node->object_literal.values);
            // Free mixins
            for (int i = 0; i < node->object_literal.mixin_count; i++) {
                free(node->object_literal.mixins[i]);
            }
            free(node->object_literal.mixins);
            break;
        case AST_PROPERTY_ACCESS:
            free(node->property_access.property);
            free_ast(node->property_access.object);
            break;
        case AST_METHOD_CALL:
            free(node->method_call.method);
            for (int i = 0; i < node->method_call.argument_count; i++) {
                free_ast(node->method_call.arguments[i]);
            }
            free(node->method_call.arguments);
            free_ast(node->method_call.object);
            break;
        case AST_PROPERTY_ASSIGNMENT:
            free(node->property_assignment.property);
            free_ast(node->property_assignment.object);
            free_ast(node->property_assignment.value);
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
    // Handle object literals
    else if (parser->current_token.type == TOKEN_PUNCTUATION &&
             strcmp(parser->current_token.value, "{") == 0) {
        factor_node = parse_object_literal(parser);
    }
    // Handle function expressions: fn(params) { ... }
    else if (parser->current_token.type == TOKEN_KEYWORD &&
             strcmp(parser->current_token.value, "fn") == 0) {
        factor_node = parse_function_expression(parser);
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
    }
    else {
        // If none of the above, return NULL (syntax error)
        report_error(parser, "Unexpected token");
        return NULL;
    }

    // Check for array index access (arr[idx])
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
        ASTNode* index_node = create_ast_node(AST_INDEX_ACCESS);
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

    // Check for property access or method call (obj.prop or obj.method())
    if (factor_node != NULL && 
        parser->current_token.type == TOKEN_PUNCTUATION &&
        strcmp(parser->current_token.value, ".") == 0) {
        factor_node = parse_property_or_method(parser, factor_node);
        
        // Handle chained property access or method calls (obj.prop1.prop2 or obj.method1().method2())
        while (factor_node != NULL && 
               parser->current_token.type == TOKEN_PUNCTUATION &&
               strcmp(parser->current_token.value, ".") == 0) {
            factor_node = parse_property_or_method(parser, factor_node);
        }
    }

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

             if (left->type != AST_VARIABLE && left->type != AST_PROPERTY_ACCESS) {
                 report_error(parser, "Left-hand side of '=' must be a variable or property access");
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
                left->variable.variable_name = NULL; 
            } 
            // If it's a property access, we use property_assignment 
            else if (left->type == AST_PROPERTY_ACCESS) {
                // Create a property assignment node instead
                free(assignment_node);  // Free the regular assignment node
                
                // Create a property assignment node
                ASTNode* property_assignment = create_ast_node(AST_PROPERTY_ASSIGNMENT);
                if (!property_assignment) {
                    report_error(parser, "Memory allocation failed for property assignment node");
                    free_ast(left);
                    free_ast(right);
                    return NULL;
                }
                
                // Move data from property_access to property_assignment
                property_assignment->property_assignment.object = left->property_access.object;
                property_assignment->property_assignment.property = left->property_access.property;
                property_assignment->property_assignment.value = right;
                
                // Don't free internal components as they are now owned by property_assignment
                left->property_access.object = NULL;  // Prevent double-free
                left->property_access.property = NULL;  // Prevent double-free
                
                free(left);  // Free only the property_access struct
                
                return property_assignment;
            }
            else {
                // This should not happen given the check above
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
    // Skip any leading newlines, indents, or dedents
    while (parser->current_token.type == TOKEN_NEWLINE || 
           parser->current_token.type == TOKEN_INDENT || 
           parser->current_token.type == TOKEN_DEDENT) {
        parser_advance(parser);
    }
    
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

    if (parser->current_token.type == TOKEN_KEYWORD && 
        strcmp(parser->current_token.value, "import") == 0) {
        return parse_import_statement(parser);
    }

    // Match a block
    if (parser->current_token.type == TOKEN_PUNCTUATION &&
        strcmp(parser->current_token.value, "{") == 0) {
        return parse_block(parser);
    }

    // Match a variable declaration (old syntax: var x = value)
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "var") == 0) {
        // Check if this is colon syntax: var name: value
        // We need to look ahead two tokens to see if there's a colon after the identifier
        // Save current state
        Lexer saved_lexer = *parser->lexer;
        Token saved_token = parser->current_token;
        
        // Advance past 'var'
        parser_advance(parser);
        
        // Check if next token is identifier
        if (parser->current_token.type == TOKEN_IDENTIFIER) {
            // Advance past identifier
            parser_advance(parser);
            
            // Check if next token is colon
            bool is_colon_syntax = (parser->current_token.type == TOKEN_PUNCTUATION && 
                                   strcmp(parser->current_token.value, ":") == 0);
            
            // Restore parser state
            *parser->lexer = saved_lexer;
            parser->current_token = saved_token;
            
            if (is_colon_syntax) {
                return parse_colon_variable_declaration(parser);
            }
        } else {
            // Restore parser state
            *parser->lexer = saved_lexer;
            parser->current_token = saved_token;
        }
        
        return parse_variable_declaration(parser, false);
    }

    // Match let variable declaration (new colon syntax: let name: value)
    if (parser->current_token.type == TOKEN_KEYWORD &&
        strcmp(parser->current_token.value, "let") == 0) {
        return parse_colon_variable_declaration(parser);
    }

    // Match an assignment, function definition, or implicit variable declaration
    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        // Peek ahead to check for assignment operator '=', colon ':', or function syntax
        Token next_token = peek_token(parser);
        if (next_token.type == TOKEN_OPERATOR && strcmp(next_token.value, "=") == 0) {
            return parse_assignment(parser);
        }
        // Check for function definition syntax: name: fn(params) { ... }
        else if (next_token.type == TOKEN_PUNCTUATION && strcmp(next_token.value, ":") == 0) {
            // We need to look ahead further to see if this is 'fn' (function) or a value (implicit var)
            // Save current state
            Lexer saved_lexer = *parser->lexer;
            Token saved_token = parser->current_token;
            
            // Advance past identifier and colon
            parser_advance(parser); // Skip identifier
            parser_advance(parser); // Skip colon
            
            bool is_function = (parser->current_token.type == TOKEN_KEYWORD && 
                               strcmp(parser->current_token.value, "fn") == 0);
            
            // Restore parser state
            *parser->lexer = saved_lexer;
            parser->current_token = saved_token;
            
            if (is_function) {
                return parse_function_definition(parser);
            } else {
                return parse_implicit_variable_declaration(parser);
            }
        }
    }

    // Parse an expression (this could be a property access, method call, or other expression)
    ASTNode* expression = parse_expression(parser, 0);
    if (expression) {
        // Check if this is a property assignment (expr followed by '=')
        if (expression->type == AST_PROPERTY_ACCESS && 
            parser->current_token.type == TOKEN_OPERATOR && 
            strcmp(parser->current_token.value, "=") == 0) {
            
            // Handle property assignment: obj.prop = value
            ASTNode* property_assignment = parse_property_assignment(parser, expression);
            
            // Optionally consume a semicolon after the assignment
            consume_optional_semicolon(parser);
            
            return property_assignment;
        }
        
        // It's a regular expression statement
        // Optionally consume a semicolon after the statement
        consume_optional_semicolon(parser);
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
        
        // Skip any newlines, indents, or dedents between statements
        while (parser->current_token.type == TOKEN_NEWLINE || 
               parser->current_token.type == TOKEN_INDENT || 
               parser->current_token.type == TOKEN_DEDENT) {
            parser_advance(parser);
        }
        
        // Check if we found the closing brace after skipping whitespace
        if (parser->current_token.type == TOKEN_PUNCTUATION &&
            strcmp(parser->current_token.value, "}") == 0) {
            break;
        }
        
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

ASTNode* parse_indented_block(Parser* parser) {
    // Expect an INDENT token to start the indented block
    if (parser->current_token.type != TOKEN_INDENT) {
        report_error(parser, "Expected indented block");
        return NULL;
    }
    
    // Consume the INDENT token
    parser_advance(parser);
    
    // Allocate memory for the block node
    ASTNode* block_node = create_ast_node(AST_BLOCK);
    if (!block_node) {
        report_error(parser, "Memory allocation failed for indented block node");
        return NULL;
    }

    block_node->block.statements = NULL;
    block_node->block.statement_count = 0;

    // Parse statements until we encounter a DEDENT token
    while (parser->current_token.type != TOKEN_DEDENT && 
           parser->current_token.type != TOKEN_EOF) {
        
        // Skip newlines between statements
        if (parser->current_token.type == TOKEN_NEWLINE) {
            parser_advance(parser);
            continue;
        }
        
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
            report_error(parser, "Memory allocation failed for indented block statements");
            free_ast(statement);
            free_ast(block_node);
            return NULL;
        }
        block_node->block.statements = temp;
        block_node->block.statements[block_node->block.statement_count++] = statement;
    }

    // After the loop, consume the DEDENT token
    if (parser->current_token.type == TOKEN_DEDENT) {
        parser_advance(parser);
    }

    return block_node;
}

ASTNode* parse_function_definition(Parser* parser) {
    // Parse the new function definition syntax: name: fn(params) { ... }
    
    // Expect a function name (identifier)
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        report_error(parser, "Expected function name");
        return NULL;
    }

    // Capture the function name
    char* function_name = strdup(parser->current_token.value);
    if (!function_name) {
        report_error(parser, "Memory allocation failed for function name");
        return NULL;
    }
    parser_advance(parser);

    // Expect a colon ':'
    if (!match_token(parser, TOKEN_PUNCTUATION, ":")) {
        report_error(parser, "Expected ':' after function name");
        free(function_name);
        return NULL;
    }

    // Expect 'fn' keyword
    if (!match_token(parser, TOKEN_KEYWORD, "fn")) {
        report_error(parser, "Expected 'fn' keyword after ':'");
        free(function_name);
        return NULL;
    }

    // Expect an opening parenthesis '('
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'fn'");
        free(function_name);
        return NULL;
    }

    // Parse parameters (same logic as original function definition)
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

    // Parse the function body - support both brace-based and indentation-based blocks
    ASTNode* body = NULL;
    
    // Skip any newlines, indents, or dedents before the body
    while (parser->current_token.type == TOKEN_NEWLINE || 
           parser->current_token.type == TOKEN_INDENT || 
           parser->current_token.type == TOKEN_DEDENT) {
        parser_advance(parser);
    }
    
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "{") == 0) {
        // Brace-based block
        body = parse_block(parser);
    } else if (parser->current_token.type == TOKEN_INDENT) {
        // Indentation-based block
        body = parse_indented_block(parser);
    } else {
        report_error(parser, "Expected '{' or indented block for function body");
        free(function_name);
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }
    
    if (!body) {
        report_error(parser, "Failed to parse function body");
        free(function_name);
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    // Create the function definition node (same as original)
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

ASTNode* parse_import_statement(Parser* parser)
{
    // 1) We expect "import" as the current token
    if (!match_token(parser, TOKEN_KEYWORD, "import")) {
        report_error(parser, "Expected 'import' keyword");
        return NULL;
    }

    // 2) Now we expect at least one identifier (e.g. "ember" or "my_file.ember"),
    //    but we also allow further punctuation segments (".", "/").
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        char msg[128];
        snprintf(msg, sizeof(msg),
            "Expected module or file identifier after 'import', got token type=%d val='%s'",
            parser->current_token.type,
            parser->current_token.value ? parser->current_token.value : "(null)");
        report_error(parser, msg);
        return NULL;
    }

    // Initialize our import_path with the first identifier
    char* import_path = strdup(parser->current_token.value);
    if (!import_path) {
        report_error(parser, "Memory allocation failed for import_path");
        return NULL;
    }
    parser_advance(parser); // consume the first identifier

    // 3) While we see punctuation of "." or "/", consume it and the next identifier
    //    This allows "ember/sdl" or "foo.bar.baz" style imports.
    while (parser->current_token.type == TOKEN_PUNCTUATION &&
           (strcmp(parser->current_token.value, ".") == 0 ||
            strcmp(parser->current_token.value, "/") == 0))
    {
        // Save the punctuation char ('.' or '/')
        char punct = parser->current_token.value[0]; 
        parser_advance(parser); // consume '.' or '/'

        // The next token must be another identifier
        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            report_error(parser, "Expected identifier after punctuation in import path");
            free(import_path);
            return NULL;
        }

        // Appendpunct + identifier to import_path
        size_t old_len = strlen(import_path);
        size_t extra_len = strlen(parser->current_token.value) + 2; // punct + identifier + '\0'
        char* new_path = (char*)malloc(old_len + extra_len);
        if (!new_path) {
            report_error(parser, "Memory allocation failed while appending to import path");
            free(import_path);
            return NULL;
        }

        // e.g., if punct='.', then "oldpath.identifier"
        //        if punct='/', then "oldpath/identifier"
        sprintf(new_path, "%s%c%s", import_path, punct, parser->current_token.value);

        free(import_path);
        import_path = new_path;

        parser_advance(parser); // consume the identifier
    }

    // 4) Optionally consume a semicolon to close the import statement
    consume_optional_semicolon(parser);

    // 5) Build the AST_IMPORT node
    ASTNode* node = create_ast_node(AST_IMPORT);
    if (!node) {
        report_error(parser, "Memory allocation failed for AST_IMPORT node");
        free(import_path);
        return NULL;
    }
    node->import_stmt.import_path = import_path;

    return node;
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

    // Skip any newlines before the body
    while (parser->current_token.type == TOKEN_NEWLINE) {
        parser_advance(parser);
    }

    // Parse the body of the if statement - support both brace-based and indentation-based blocks
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "{") == 0) {
        // Brace-based block
        body = parse_block(parser);
    } else if (parser->current_token.type == TOKEN_INDENT) {
        // Indentation-based block
        body = parse_indented_block(parser);
    } else {
        report_error(parser, "Expected '{' or indented block for if statement body");
        free_ast(condition);
        return NULL;
    }
    
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
        // Skip any newlines after 'else'
        while (parser->current_token.type == TOKEN_NEWLINE) {
            parser_advance(parser);
        }
        
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
            // Parse else block - support both brace-based and indentation-based blocks
            ASTNode* else_body = NULL;
            if (parser->current_token.type == TOKEN_PUNCTUATION && 
                strcmp(parser->current_token.value, "{") == 0) {
                // Brace-based block
                else_body = parse_block(parser);
            } else if (parser->current_token.type == TOKEN_INDENT) {
                // Indentation-based block
                else_body = parse_indented_block(parser);
            } else {
                report_error(parser, "Expected '{' or indented block for else clause");
                free_ast(if_node);
                return NULL;
            }
            
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

    // Skip any newlines before the body
    while (parser->current_token.type == TOKEN_NEWLINE) {
        parser_advance(parser);
    }

    // Parse the body of the while loop - support both brace-based and indentation-based blocks
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "{") == 0) {
        // Brace-based block
        body = parse_block(parser);
    } else if (parser->current_token.type == TOKEN_INDENT) {
        // Indentation-based block
        body = parse_indented_block(parser);
    } else {
        report_error(parser, "Expected '{' or indented block for while loop body");
        free_ast(condition);
        return NULL;
    }
    
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

    // Optionally consume a semicolon after the assignment
    consume_optional_semicolon(parser);

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
    
    // Set the new fields for backward compatibility with old syntax
    variable_decl_node->variable_decl.decl_type = VAR_DECL_VAR; // Old syntax defaults to var
    variable_decl_node->variable_decl.is_mutable = true;        // Old syntax is always mutable

    // If this is a STANDALONE declaration (not in for-header), optionally consume semicolon.
    // If it's inside a for-loop header, we'll rely on parse_for_loop() to handle the ';'.
    if (!inForHeader) {
        consume_optional_semicolon(parser);
    }

    return variable_decl_node;
}

ASTNode* parse_colon_variable_declaration(Parser* parser) {
    // Handle: var name: value, let name: value
    VariableDeclarationType decl_type;
    bool is_mutable;
    
    // Determine declaration type
    if (parser->current_token.type == TOKEN_KEYWORD) {
        if (strcmp(parser->current_token.value, "var") == 0) {
            decl_type = VAR_DECL_VAR;
            is_mutable = true;
        } else if (strcmp(parser->current_token.value, "let") == 0) {
            decl_type = VAR_DECL_LET;
            is_mutable = false;
        } else {
            fprintf(stderr, "Error: Expected 'var' or 'let' keyword\n");
            return NULL;
        }
        parser_advance(parser); // Skip 'var' or 'let'
    } else {
        fprintf(stderr, "Error: Expected variable declaration keyword\n");
        return NULL;
    }

    // Ensure the next token is an identifier (variable name)
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected identifier after variable declaration keyword\n");
        return NULL;
    }

    // Store the variable name
    char* variable_name = strdup(parser->current_token.value);
    if (!variable_name) {
        fprintf(stderr, "Error: Memory allocation failed for variable name\n");
        return NULL;
    }
    parser_advance(parser); // Skip the variable name

    // Expect a colon ':'
    if (parser->current_token.type != TOKEN_PUNCTUATION || 
        strcmp(parser->current_token.value, ":") != 0) {
        fprintf(stderr, "Error: Expected ':' after variable name in colon syntax\n");
        free(variable_name);
        return NULL;
    }
    parser_advance(parser); // Skip the ':'

    // Parse the initializer expression (required in colon syntax)
    ASTNode* initial_value = parse_expression(parser, 0);
    if (!initial_value) {
        fprintf(stderr, "Error: Failed to parse initializer for colon variable declaration\n");
        free(variable_name);
        return NULL;
    }

    // Create the variable declaration node
    ASTNode* variable_decl_node = create_ast_node(AST_VARIABLE_DECL);
    if (!variable_decl_node) {
        fprintf(stderr, "Error: Memory allocation failed for variable declaration node\n");
        free(variable_name);
        free_ast(initial_value);
        return NULL;
    }
    
    variable_decl_node->variable_decl.variable_name = variable_name;
    variable_decl_node->variable_decl.initial_value = initial_value;
    variable_decl_node->variable_decl.decl_type = decl_type;
    variable_decl_node->variable_decl.is_mutable = is_mutable;

    // Optionally consume a semicolon
    consume_optional_semicolon(parser);

    return variable_decl_node;
}

ASTNode* parse_implicit_variable_declaration(Parser* parser) {
    // Handle: identifier: value (implicit var)
    
    // Ensure the current token is an identifier
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Error: Expected identifier for implicit variable declaration\n");
        return NULL;
    }

    // Store the variable name
    char* variable_name = strdup(parser->current_token.value);
    if (!variable_name) {
        fprintf(stderr, "Error: Memory allocation failed for variable name\n");
        return NULL;
    }
    parser_advance(parser); // Skip the variable name

    // Expect a colon ':' (this should already be verified by the caller)
    if (parser->current_token.type != TOKEN_PUNCTUATION || 
        strcmp(parser->current_token.value, ":") != 0) {
        fprintf(stderr, "Error: Expected ':' after variable name in implicit declaration\n");
        free(variable_name);
        return NULL;
    }
    parser_advance(parser); // Skip the ':'

    // Parse the initializer expression (required in colon syntax)
    ASTNode* initial_value = parse_expression(parser, 0);
    if (!initial_value) {
        fprintf(stderr, "Error: Failed to parse initializer for implicit variable declaration\n");
        free(variable_name);
        return NULL;
    }

    // Create the variable declaration node
    ASTNode* variable_decl_node = create_ast_node(AST_VARIABLE_DECL);
    if (!variable_decl_node) {
        fprintf(stderr, "Error: Memory allocation failed for variable declaration node\n");
        free(variable_name);
        free_ast(initial_value);
        return NULL;
    }
    
    variable_decl_node->variable_decl.variable_name = variable_name;
    variable_decl_node->variable_decl.initial_value = initial_value;
    variable_decl_node->variable_decl.decl_type = VAR_DECL_IMPLICIT;
    variable_decl_node->variable_decl.is_mutable = true; // Implicit defaults to mutable

    // Optionally consume a semicolon
    consume_optional_semicolon(parser);

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

// Helper function to optionally consume a semicolon (for backward compatibility)
void consume_optional_semicolon(Parser* parser) {
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, ";") == 0) {
        parser_advance(parser);
    }
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

        case AST_OBJECT_LITERAL:
            printf("Object Literal:\n");
            for (int i = 0; i < node->object_literal.property_count; i++) {
                printf("  %s: ", node->object_literal.keys[i]);
                print_ast(node->object_literal.values[i], depth + 1);
            }
            break;

        case AST_PROPERTY_ACCESS:
            printf("Property Access: %s\n", node->property_access.property);
            print_ast(node->property_access.object, depth + 1);
            break;

        case AST_METHOD_CALL:
            printf("Method Call: %s\n", node->method_call.method);
            print_ast(node->method_call.object, depth + 1);
            printf("  Arguments:\n");
            for (int i = 0; i < node->method_call.argument_count; i++) {
                print_ast(node->method_call.arguments[i], depth + 1);
            }
            break;

        case AST_PROPERTY_ASSIGNMENT:
            printf("Property Assignment: %s\n", node->property_assignment.property);
            print_ast(node->property_assignment.object, depth + 1);
            printf("  Value:\n");
            print_ast(node->property_assignment.value, depth + 1);
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

ASTNode* parse_object_literal(Parser* parser) {
    // Expect opening '{'
    if (!match_token(parser, TOKEN_PUNCTUATION, "{")) {
        report_error(parser, "Expected '{' to start object literal");
        return NULL;
    }

    char** keys = NULL;
    ASTNode** values = NULL;
    int property_count = 0;
    char** mixins = NULL;
    int mixin_count = 0;

    // Handle the empty object case
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "}") == 0) {
        parser_advance(parser); // Skip '}'
        
        // Create empty object literal node
        ASTNode* object_node = create_ast_node(AST_OBJECT_LITERAL);
        if (!object_node) {
            report_error(parser, "Memory allocation failed for object literal node");
            return NULL;
        }
        
        object_node->object_literal.keys = NULL;
        object_node->object_literal.values = NULL;
        object_node->object_literal.property_count = 0;
        
        return object_node;
    }

    // Skip any newlines, indents, or dedents before checking for mixins
    while (parser->current_token.type == TOKEN_NEWLINE || 
           parser->current_token.type == TOKEN_INDENT || 
           parser->current_token.type == TOKEN_DEDENT) {
        parser_advance(parser);
    }

    // Check for mixin syntax: :[MixinName1, MixinName2]
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, ":") == 0) {
        
        // Look ahead to see if this is a mixin declaration
        Token next_token = peek_token(parser);
        
        if (next_token.type == TOKEN_PUNCTUATION && 
            strcmp(next_token.value, "[") == 0) {
            
            // This is a mixin declaration
            parser_advance(parser); // Skip ':'
            parser_advance(parser); // Skip '['
            
            // Parse mixin names
            do {
                if (parser->current_token.type != TOKEN_IDENTIFIER) {
                    report_error(parser, "Expected mixin name (identifier)");
                    // Clean up
                    for (int i = 0; i < mixin_count; i++) {
                        free(mixins[i]);
                    }
                    free(mixins);
                    return NULL;
                }
                
                // Store the mixin name
                char* mixin_name = strdup(parser->current_token.value);
                if (!mixin_name) {
                    report_error(parser, "Memory allocation failed for mixin name");
                    // Clean up
                    for (int i = 0; i < mixin_count; i++) {
                        free(mixins[i]);
                    }
                    free(mixins);
                    return NULL;
                }
                parser_advance(parser);
                
                // Expand the mixins array
                char** new_mixins = realloc(mixins, sizeof(char*) * (mixin_count + 1));
                if (!new_mixins) {
                    report_error(parser, "Memory allocation failed for mixins");
                    free(mixin_name);
                    for (int i = 0; i < mixin_count; i++) {
                        free(mixins[i]);
                    }
                    free(mixins);
                    return NULL;
                }
                mixins = new_mixins;
                mixins[mixin_count] = mixin_name;
                mixin_count++;
                
                // Look for ',' or ']'
                if (parser->current_token.type == TOKEN_PUNCTUATION && 
                    strcmp(parser->current_token.value, ",") == 0) {
                    parser_advance(parser); // Skip ','
                } else if (parser->current_token.type == TOKEN_PUNCTUATION && 
                           strcmp(parser->current_token.value, "]") == 0) {
                    parser_advance(parser); // Skip ']'
                    break;
                } else {
                    report_error(parser, "Expected ',' or ']' in mixin list");
                    // Clean up
                    for (int i = 0; i < mixin_count; i++) {
                        free(mixins[i]);
                    }
                    free(mixins);
                    return NULL;
                }
            } while (1);
            
            // Skip any newlines, indents, or dedents before checking for end of object
            while (parser->current_token.type == TOKEN_NEWLINE || 
                   parser->current_token.type == TOKEN_INDENT || 
                   parser->current_token.type == TOKEN_DEDENT) {
                parser_advance(parser);
            }
            
            // After parsing mixins, check if there are more properties
            if (parser->current_token.type == TOKEN_PUNCTUATION && 
                strcmp(parser->current_token.value, "}") == 0) {
                // No more properties, just mixins
                parser_advance(parser); // Skip '}'
                
                // Create object literal node with just mixins
                ASTNode* object_node = create_ast_node(AST_OBJECT_LITERAL);
                if (!object_node) {
                    report_error(parser, "Memory allocation failed for object literal node");
                    // Clean up
                    for (int i = 0; i < mixin_count; i++) {
                        free(mixins[i]);
                    }
                    free(mixins);
                    return NULL;
                }
                
                object_node->object_literal.keys = NULL;
                object_node->object_literal.values = NULL;
                object_node->object_literal.property_count = 0;
                object_node->object_literal.mixins = mixins;
                object_node->object_literal.mixin_count = mixin_count;
                
                return object_node;
            }
            
            // There are more properties after mixins - require a comma
            if (parser->current_token.type == TOKEN_PUNCTUATION && 
                strcmp(parser->current_token.value, ",") == 0) {
                parser_advance(parser); // Skip ','
                
                // Skip any newlines after the comma
                while (parser->current_token.type == TOKEN_NEWLINE) {
                    parser_advance(parser);
                }
            } else {
                report_error(parser, "Expected ',' after mixin declaration when properties follow");
                // Clean up
                for (int i = 0; i < mixin_count; i++) {
                    free(mixins[i]);
                }
                free(mixins);
                return NULL;
            }
        }
    }

    // Parse properties
    do {
        // Skip any newlines, indents, or dedents before parsing the key
        while (parser->current_token.type == TOKEN_NEWLINE || 
               parser->current_token.type == TOKEN_INDENT || 
               parser->current_token.type == TOKEN_DEDENT) {
            parser_advance(parser);
        }
        
        // Check if we've reached the end of the object
        if (parser->current_token.type == TOKEN_PUNCTUATION && 
            strcmp(parser->current_token.value, "}") == 0) {
            break;
        }
        
        // Parse the key, which must be an identifier
        if (parser->current_token.type != TOKEN_IDENTIFIER && 
            parser->current_token.type != TOKEN_STRING) {
            report_error(parser, "Expected identifier or string as object key");
            // Clean up
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            // Also clean up mixins
            for (int i = 0; i < mixin_count; i++) {
                free(mixins[i]);
            }
            free(mixins);
            return NULL;
        }

        // Store the key
        char* key = strdup(parser->current_token.value);
        if (!key) {
            report_error(parser, "Memory allocation failed for object key");
            // Clean up
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }
        parser_advance(parser);

        // Expect ':'
        if (!match_token(parser, TOKEN_PUNCTUATION, ":")) {
            report_error(parser, "Expected ':' after object key");
            // Clean up
            free(key);
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }

        // Parse the value
        ASTNode* value = parse_expression(parser, 0);
        if (!value) {
            report_error(parser, "Failed to parse object property value");
            // Clean up
            free(key);
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }

        // Expand the arrays of keys and values
        char** new_keys = realloc(keys, sizeof(char*) * (property_count + 1));
        if (!new_keys) {
            report_error(parser, "Memory allocation failed for object keys");
            // Clean up
            free(key);
            free_ast(value);
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }
        keys = new_keys;

        ASTNode** new_values = realloc(values, sizeof(ASTNode*) * (property_count + 1));
        if (!new_values) {
            report_error(parser, "Memory allocation failed for object values");
            // Clean up
            free(key);
            free_ast(value);
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }
        values = new_values;

        // Store the key and value
        keys[property_count] = key;
        values[property_count] = value;
        property_count++;

        // Skip any newlines, indents, or dedents after the property value
        while (parser->current_token.type == TOKEN_NEWLINE || 
               parser->current_token.type == TOKEN_INDENT || 
               parser->current_token.type == TOKEN_DEDENT) {
            parser_advance(parser);
        }

        // Look for ',' or '}'
        if (parser->current_token.type == TOKEN_PUNCTUATION && 
            strcmp(parser->current_token.value, ",") == 0) {
            parser_advance(parser); // Skip ','
            
            // Skip any newlines after the comma
            while (parser->current_token.type == TOKEN_NEWLINE) {
                parser_advance(parser);
            }
            
            // Handle trailing comma, if the next token is '}'
            if (parser->current_token.type == TOKEN_PUNCTUATION && 
                strcmp(parser->current_token.value, "}") == 0) {
                break;
            }
        } else if (parser->current_token.type == TOKEN_PUNCTUATION && 
                   strcmp(parser->current_token.value, "}") == 0) {
            // End of object literal
            break;
        } else {
            report_error(parser, "Expected ',' or '}' after object property");
            // Clean up
            for (int i = 0; i < property_count; i++) {
                free(keys[i]);
                free_ast(values[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }
    } while (1);

    // Consume the closing '}'
    if (!match_token(parser, TOKEN_PUNCTUATION, "}")) {
        report_error(parser, "Expected '}' to close object literal");
        // Clean up
        for (int i = 0; i < property_count; i++) {
            free(keys[i]);
            free_ast(values[i]);
        }
        free(keys);
        free(values);
        return NULL;
    }

    // Create the object literal node
    ASTNode* object_node = create_ast_node(AST_OBJECT_LITERAL);
    if (!object_node) {
        report_error(parser, "Memory allocation failed for object literal node");
        // Clean up
        for (int i = 0; i < property_count; i++) {
            free(keys[i]);
            free_ast(values[i]);
        }
        free(keys);
        free(values);
        return NULL;
    }

    object_node->object_literal.keys = keys;
    object_node->object_literal.values = values;
    object_node->object_literal.property_count = property_count;
    object_node->object_literal.mixins = mixins;
    object_node->object_literal.mixin_count = mixin_count;

    return object_node;
}

ASTNode* parse_property_or_method(Parser* parser, ASTNode* object) {
    // Expect '.'
    if (!match_token(parser, TOKEN_PUNCTUATION, ".")) {
        report_error(parser, "Expected '.' for property access");
        free_ast(object);
        return NULL;
    }

    // Expect property/method name (identifier)
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        report_error(parser, "Expected identifier after '.'");
        free_ast(object);
        return NULL;
    }

    // Capture property/method name
    char* name = strdup(parser->current_token.value);
    if (!name) {
        report_error(parser, "Memory allocation failed for property/method name");
        free_ast(object);
        return NULL;
    }
    parser_advance(parser);

    // Check if this is a property access or a method call
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "(") == 0) {
        // This is a method call
        parser_advance(parser); // Skip '('

        // Parse arguments
        ASTNode** arguments = NULL;
        int argument_count = 0;

        // Handle the case of no arguments
        if (parser->current_token.type == TOKEN_PUNCTUATION && 
            strcmp(parser->current_token.value, ")") == 0) {
            parser_advance(parser); // Skip ')'
        } else {
            // Parse arguments
            do {
                ASTNode* arg = parse_expression(parser, 0);
                if (!arg) {
                    report_error(parser, "Failed to parse method argument");
                    free(name);
                    free_ast(object);
                    // Clean up previously parsed arguments
                    for (int i = 0; i < argument_count; i++) {
                        free_ast(arguments[i]);
                    }
                    free(arguments);
                    return NULL;
                }

                // Add to arguments array
                ASTNode** new_args = realloc(arguments, sizeof(ASTNode*) * (argument_count + 1));
                if (!new_args) {
                    report_error(parser, "Memory allocation failed for method arguments");
                    free(name);
                    free_ast(object);
                    free_ast(arg);
                    // Clean up previously parsed arguments
                    for (int i = 0; i < argument_count; i++) {
                        free_ast(arguments[i]);
                    }
                    free(arguments);
                    return NULL;
                }
                arguments = new_args;
                arguments[argument_count++] = arg;

                // Look for ',' or ')'
                if (parser->current_token.type == TOKEN_PUNCTUATION && 
                    strcmp(parser->current_token.value, ",") == 0) {
                    parser_advance(parser); // Skip ','
                } else {
                    break;
                }
            } while (1);

            // Expect closing ')'
            if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
                report_error(parser, "Expected ')' after method arguments");
                free(name);
                free_ast(object);
                // Clean up arguments
                for (int i = 0; i < argument_count; i++) {
                    free_ast(arguments[i]);
                }
                free(arguments);
                return NULL;
            }
        }

        // Create method call node
        ASTNode* method_call = create_ast_node(AST_METHOD_CALL);
        if (!method_call) {
            report_error(parser, "Memory allocation failed for method call node");
            free(name);
            free_ast(object);
            // Clean up arguments
            for (int i = 0; i < argument_count; i++) {
                free_ast(arguments[i]);
            }
            free(arguments);
            return NULL;
        }

        method_call->method_call.object = object;
        method_call->method_call.method = name;
        method_call->method_call.arguments = arguments;
        method_call->method_call.argument_count = argument_count;

        return method_call;
    } else {
        // This is a property access
        ASTNode* property_access = create_ast_node(AST_PROPERTY_ACCESS);
        if (!property_access) {
            report_error(parser, "Memory allocation failed for property access node");
            free(name);
            free_ast(object);
            return NULL;
        }

        property_access->property_access.object = object;
        property_access->property_access.property = name;

        return property_access;
    }
}

ASTNode* parse_property_assignment(Parser* parser, ASTNode* property_access) {
    // Ensure we have a property access node
    if (!property_access || property_access->type != AST_PROPERTY_ACCESS) {
        report_error(parser, "Expected property access for assignment");
        if (property_access) free_ast(property_access);
        return NULL;
    }

    // Expect assignment operator '='
    if (!match_token(parser, TOKEN_OPERATOR, "=")) {
        report_error(parser, "Expected '=' for property assignment");
        free_ast(property_access);
        return NULL;
    }

    // Parse the value expression
    ASTNode* value = parse_expression(parser, 0);
    if (!value) {
        report_error(parser, "Failed to parse property assignment value");
        free_ast(property_access);
        return NULL;
    }

    // Create a property assignment node
    ASTNode* property_assignment = create_ast_node(AST_PROPERTY_ASSIGNMENT);
    if (!property_assignment) {
        report_error(parser, "Memory allocation failed for property assignment node");
        free_ast(property_access);
        free_ast(value);
        return NULL;
    }

    // Move data from property_access to property_assignment
    property_assignment->property_assignment.object = property_access->property_access.object;
    property_assignment->property_assignment.property = property_access->property_access.property;
    property_assignment->property_assignment.value = value;
    
    // Don't free internal components as they are now owned by property_assignment
    free(property_access); // Only free the struct itself, not its contents
    
    return property_assignment;
}

ASTNode* parse_function_expression(Parser* parser) {
    // Parse function expressions: fn(params) { ... }
    // This is similar to parse_function_definition but without a name
    
    // Expect 'fn' keyword (already checked in parse_factor)
    if (!match_token(parser, TOKEN_KEYWORD, "fn")) {
        report_error(parser, "Expected 'fn' keyword");
        return NULL;
    }

    // Expect an opening parenthesis '('
    if (!match_token(parser, TOKEN_PUNCTUATION, "(")) {
        report_error(parser, "Expected '(' after 'fn'");
        return NULL;
    }

    // Parse parameters (same logic as function definition)
    char** parameters = NULL;
    int parameter_count = 0;

    // While the next token is not ')', parse parameters
    while (parser->current_token.type != TOKEN_PUNCTUATION ||
           strcmp(parser->current_token.value, ")") != 0) {

        if (parser->current_token.type != TOKEN_IDENTIFIER) {
            report_error(parser, "Expected parameter name");
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
            for (int i = 0; i < parameter_count; i++) {
                free(parameters[i]);
            }
            free(parameters);
            return NULL;
        }
    }

    // Consume the closing parenthesis ')'
    if (!match_token(parser, TOKEN_PUNCTUATION, ")")) {
        report_error(parser, "Expected ')' after parameter list");
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    // Parse the function body - support both brace-based and indentation-based blocks
    ASTNode* body = NULL;
    
    // Skip any newlines, indents, or dedents before the body
    while (parser->current_token.type == TOKEN_NEWLINE || 
           parser->current_token.type == TOKEN_INDENT || 
           parser->current_token.type == TOKEN_DEDENT) {
        parser_advance(parser);
    }
    
    if (parser->current_token.type == TOKEN_PUNCTUATION && 
        strcmp(parser->current_token.value, "{") == 0) {
        // Brace-based block
        body = parse_block(parser);
    } else if (parser->current_token.type == TOKEN_INDENT) {
        // Indentation-based block
        body = parse_indented_block(parser);
    } else {
        report_error(parser, "Expected '{' or indented block for function body");
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }
    
    if (!body) {
        report_error(parser, "Failed to parse function body");
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        return NULL;
    }

    // Create the function definition node
    ASTNode* function_node = create_ast_node(AST_FUNCTION_DEF);
    if (!function_node) {
        report_error(parser, "Memory allocation failed for function expression node");
        for (int i = 0; i < parameter_count; i++) {
            free(parameters[i]);
        }
        free(parameters);
        free_ast(body);
        return NULL;
    }

    // For function expressions, we use a generated name
    function_node->function_def.function_name = strdup("<anonymous>");
    function_node->function_def.parameters = parameters;
    function_node->function_def.parameter_count = parameter_count;
    function_node->function_def.body = body;

    return function_node;
}