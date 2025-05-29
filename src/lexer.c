#include "lexer.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->current_char = source[0];
    
    // Initialize indentation tracking
    lexer->indent_stack[0] = 0;  // Base indentation level
    lexer->indent_stack_size = 1;
    lexer->current_indent = 0;
    lexer->at_line_start = true;
    lexer->pending_dedents = false;
    lexer->dedent_count = 0;
}

void lexer_advance(Lexer* lexer) {
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1; // Reset column to 1 at the start of a new line
        lexer->at_line_start = true;  // Mark that we're at the start of a new line
    } else {
        lexer->column++;
        // Only set at_line_start to false if we encounter a non-whitespace character
        if (lexer->current_char != ' ' && lexer->current_char != '\t') {
            lexer->at_line_start = false;
        }
    }
    lexer->position++;
    lexer->current_char = lexer->source[lexer->position];
}

char lexer_peek(Lexer* lexer) {
    if (lexer->position + 1 < (int)strlen(lexer->source)) {
        return lexer->source[lexer->position + 1];
    }
    return '\0'; // Return null character if out of bounds
}

void lexer_skip_whitespace_and_comments(Lexer* lexer) {
    while (lexer->current_char != '\0') {
        if (lexer->current_char == ' ' || lexer->current_char == '\t' || lexer->current_char == '\r') {
            // Skip whitespace (but not newlines - they're significant for indentation)
            lexer_advance(lexer);
        } else if (lexer->current_char == '/' && lexer_peek(lexer) == '/') {
            // Skip single-line comments
            while (lexer->current_char != '\n' && lexer->current_char != '\0') {
                lexer_advance(lexer);
            }
        } else if (lexer->current_char == '/' && lexer_peek(lexer) == '*') {
            // Skip block comments
            lexer_advance(lexer); // Skip '/'
            lexer_advance(lexer); // Skip '*'
            while (!(lexer->current_char == '*' && lexer_peek(lexer) == '/') && lexer->current_char != '\0') {
                lexer_advance(lexer);
            }
            if (lexer->current_char != '\0') {
                lexer_advance(lexer); // Skip '*'
                lexer_advance(lexer); // Skip '/'
            }
        } else {
            break; // Stop if it's not whitespace or a comment
        }
    }
}

char* lexer_read_identifier(Lexer* lexer) {
    int start = lexer->position;

    // Continue while the character is alphanumeric or an underscore
    while (isalnum(lexer->current_char) || lexer->current_char == '_') {
        lexer_advance(lexer);
    }

    // Calculate the length of the identifier
    int length = lexer->position - start;

    // Allocate memory for the identifier
    char* identifier = (char*)malloc(length + 1);
    if (!identifier) {
        fprintf(stderr, "Error: Memory allocation failed for identifier\n");
        return NULL;
    }

    // Copy the identifier from the source and null-terminate it
    strncpy(identifier, &lexer->source[start], length);
    identifier[length] = '\0';

    return identifier;
}


Token lexer_next_token(Lexer* lexer) {
    // Handle indentation at the start of a line
    if (lexer->at_line_start) {
        Token indent_token = lexer_handle_indentation(lexer);
        if (indent_token.type != TOKEN_EOF) {
            return indent_token;
        }
    }

    lexer_skip_whitespace_and_comments(lexer);

    if (lexer->current_char == '\0') {
        return (Token){TOKEN_EOF, NULL, lexer->line, lexer->column};
    }

    // Handle newlines
    if (lexer->current_char == '\n') {
        lexer_advance(lexer);
        lexer->at_line_start = true;
        return (Token){TOKEN_NEWLINE, NULL, lexer->line, lexer->column};
    }

    // Identifiers and keywords
    if (isalpha(lexer->current_char) || lexer->current_char == '_') {
        char* identifier = lexer_read_identifier(lexer);

        if (strcmp(identifier, "true") == 0 || strcmp(identifier, "false") == 0) {
            return (Token){TOKEN_BOOLEAN, identifier, lexer->line, lexer->column};
        } else if (strcmp(identifier, "null") == 0) {
            return (Token){TOKEN_NULL, identifier, lexer->line, lexer->column};
        } else if (is_keyword(identifier)) {
            return (Token){TOKEN_KEYWORD, identifier, lexer->line, lexer->column};
        } else {
            return (Token){TOKEN_IDENTIFIER, identifier, lexer->line, lexer->column};
        }
    }

    // Numbers
    if (isdigit(lexer->current_char)) {
        int start = lexer->position;
        
        // Parse digits
        while (isdigit(lexer->current_char)) {
            lexer_advance(lexer);
        }
        
        // Check for decimal point, but be careful about range operator
        if (lexer->current_char == '.') {
            // Look ahead to see if this is a range operator (..)
            if (lexer_peek(lexer) == '.') {
                // This is a range operator, don't include the decimal point
                // Just return the integer part
                int length = lexer->position - start;
                char* number = (char*)malloc(length + 1);
                strncpy(number, &lexer->source[start], length);
                number[length] = '\0';
                return (Token){TOKEN_NUMBER, number, lexer->line, lexer->column};
            } else {
                // This is a decimal point, include it and continue parsing
                lexer_advance(lexer); // consume the '.'
                
                // Parse fractional part
                while (isdigit(lexer->current_char)) {
                    lexer_advance(lexer);
                }
            }
        }
        
        int length = lexer->position - start;
        char* number = (char*)malloc(length + 1);
        strncpy(number, &lexer->source[start], length);
        number[length] = '\0';
        return (Token){TOKEN_NUMBER, number, lexer->line, lexer->column};
    }

    // Strings
    if (lexer->current_char == '"') {
        lexer_advance(lexer); // Skip opening quote

        char* string = NULL;
        size_t buffer_size = 64;
        size_t string_index = 0;

        string = malloc(buffer_size);
        if (!string) {
            fprintf(stderr, "Error: Memory allocation failed for string literal\n");
            return (Token){TOKEN_EOF, NULL, lexer->line, lexer->column};
        }

        while (lexer->current_char != '"' && lexer->current_char != '\0') {
            if (lexer->current_char == '\\') {
                lexer_advance(lexer);
                switch (lexer->current_char) {
                    case 'n': string[string_index++] = '\n'; break;
                    case 't': string[string_index++] = '\t'; break;
                    case '\\': string[string_index++] = '\\'; break;
                    case '"': string[string_index++] = '"'; break;
                    default:
                        fprintf(stderr, "Error (Line %d, Position %d): Invalid escape sequence '\\%c'\n",
                                lexer->line, lexer->position, lexer->current_char);
                        free(string);
                        return (Token){TOKEN_ERROR, NULL, lexer->line, lexer->column};
                }
            } else {
                string[string_index++] = lexer->current_char;
            }

             if (string_index >= buffer_size - 1) {
                buffer_size *= 2;
                char* temp = realloc(string, buffer_size);
                if (!temp) {
                    fprintf(stderr, "Error: Memory allocation failed while reading string literal\n");
                    free(string);
                    return (Token){TOKEN_ERROR, NULL, lexer->line, lexer->column};
                }
                string = temp;
            }
            lexer_advance(lexer);
        }

        if (lexer->current_char == '\0') {
            fprintf(stderr, "Error: Unterminated string literal\n");
            free(string);
            return (Token){TOKEN_ERROR, NULL, lexer->line, lexer->column};
        }

        string[string_index] = '\0'; // Null-terminate the string
        lexer_advance(lexer); // Skip closing quote
        return (Token){TOKEN_STRING, string, lexer->line, lexer->column};
    }

    // Multi-character operators
    if (lexer->current_char == '=' || lexer->current_char == '!' ||
        lexer->current_char == '<' || lexer->current_char == '>' ||
        lexer->current_char == '&' || lexer->current_char == '|' ||
        lexer->current_char == '.') {
        char first_char = lexer->current_char;
        lexer_advance(lexer);

        if (lexer->current_char == '=') { // e.g., ==, !=, <=, >=
            lexer_advance(lexer);
            char* operator = (char*)malloc(3);
            operator[0] = first_char;
            operator[1] = '=';
            operator[2] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
        } else if (first_char == '&' && lexer->current_char == '&') { // &&
            lexer_advance(lexer);
            char* operator = (char*)malloc(3);
            operator[0] = '&';
            operator[1] = '&';
            operator[2] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
        } else if (first_char == '|' && lexer->current_char == '|') { // ||
            lexer_advance(lexer);
            char* operator = (char*)malloc(3);
            operator[0] = '|';
            operator[1] = '|';
            operator[2] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
        } else if (first_char == '.' && lexer->current_char == '.') { // .. (range operator)
            lexer_advance(lexer);
            char* operator = (char*)malloc(3);
            operator[0] = '.';
            operator[1] = '.';
            operator[2] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
        } else if (first_char == '<' && lexer->current_char == '-') { // <- (event binding operator)
            lexer_advance(lexer);
            char* operator = (char*)malloc(3);
            operator[0] = '<';
            operator[1] = '-';
            operator[2] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
        } else {
            // Single-character operator (e.g., =, <, >, !, .)
            char* operator = (char*)malloc(2);
            operator[0] = first_char;
            operator[1] = '\0';
            if (first_char == '.') {
                return (Token){TOKEN_PUNCTUATION, operator, lexer->line, lexer->column};
            } else {
                return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
            }
        }
    }

    // Single-character operators or unknown tokens
    char current_char = lexer->current_char;
    lexer_advance(lexer);

    // Handle supported single-character operators
    if (strchr("+-*/%", current_char)) {
        // Arithmetic operators
        char* operator = (char*)malloc(2);
        operator[0] = current_char;
        operator[1] = '\0';
        return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
    } else if (strchr("(){}[],;:", current_char)) {
        // Punctuation
        char* punctuation = (char*)malloc(2);
        punctuation[0] = current_char;
        punctuation[1] = '\0';
        return (Token){TOKEN_PUNCTUATION, punctuation, lexer->line, lexer->column};
    }

    // Unsupported token
    fprintf(stderr, "Error: Unexpected character '%c'\n", current_char);
    return (Token){TOKEN_ERROR, NULL, lexer->line, lexer->column};
}


// Function to check if an identifier is a keyword
bool is_keyword(const char* identifier) {
    static const char* keywords[] = {
        "if", "else", "while", "for", "return", "break", "continue",
        "var", "const", "let", "true", "false", "null", "import", "fn", "fire"
    };

    static const int keyword_count = sizeof(keywords) / sizeof(keywords[0]);

    for (int i = 0; i < keyword_count; i++) {
        if (strcmp(identifier, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

void print_token(const Token* token) {
    if (token->type == TOKEN_EOF) {
        printf("Token: EOF\n");
    } else if (token->type == TOKEN_ERROR) {
        printf("Token: ERROR\n");
    } else if (token->type == TOKEN_INDENT) {
        printf("Token: INDENT\n");
    } else if (token->type == TOKEN_DEDENT) {
        printf("Token: DEDENT\n");
    } else if (token->type == TOKEN_NEWLINE) {
        printf("Token: NEWLINE\n");
    } else {
        printf("Token: Type=%d, Value=%s\n", token->type, token->value);
    }
}

void free_token(Token* token) {
    if (token->value) {
        free(token->value);
        token->value = NULL;
    }
}

int lexer_calculate_indentation(Lexer* lexer) {
    int indent_level = 0;
    int saved_position = lexer->position;
    int saved_column = lexer->column;
    char saved_char = lexer->current_char;
    
    // Count spaces and tabs at the beginning of the line
    while (lexer->current_char == ' ' || lexer->current_char == '\t') {
        if (lexer->current_char == ' ') {
            indent_level++;
        } else if (lexer->current_char == '\t') {
            indent_level += 4; // Treat tab as 4 spaces
        }
        lexer_advance(lexer);
    }
    
    // If we hit a newline or comment, this is an empty/comment line - ignore for indentation
    if (lexer->current_char == '\n' || lexer->current_char == '\0' || 
        (lexer->current_char == '/' && lexer_peek(lexer) == '/')) {
        // Restore position and return -1 to indicate this line should be ignored
        lexer->position = saved_position;
        lexer->column = saved_column;
        lexer->current_char = saved_char;
        return -1;
    }
    
    return indent_level;
}

void lexer_push_indent(Lexer* lexer, int level) {
    if (lexer->indent_stack_size < MAX_INDENT_LEVELS) {
        lexer->indent_stack[lexer->indent_stack_size] = level;
        lexer->indent_stack_size++;
    } else {
        fprintf(stderr, "Error: Maximum indentation depth exceeded\n");
    }
}

int lexer_pop_indent(Lexer* lexer) {
    if (lexer->indent_stack_size > 1) {
        lexer->indent_stack_size--;
        return lexer->indent_stack[lexer->indent_stack_size];
    }
    return 0; // Base level
}

Token lexer_handle_indentation(Lexer* lexer) {
    // Calculate indentation level for this line
    int indent_level = lexer_calculate_indentation(lexer);
    
    // If this is an empty line or comment line, skip indentation processing
    if (indent_level == -1) {
        lexer->at_line_start = false;
        return lexer_next_token(lexer); // Continue with normal tokenization
    }
    
    int current_level = lexer->indent_stack[lexer->indent_stack_size - 1];
    
    if (indent_level > current_level) {
        // Increased indentation - emit INDENT
        lexer_push_indent(lexer, indent_level);
        lexer->at_line_start = false;
        return (Token){TOKEN_INDENT, NULL, lexer->line, lexer->column};
    } else if (indent_level < current_level) {
        // Decreased indentation - emit DEDENT(s)
        lexer->dedent_count = 0;
        
        // Count how many levels we need to dedent
        while (lexer->indent_stack_size > 1 && 
               lexer->indent_stack[lexer->indent_stack_size - 1] > indent_level) {
            lexer_pop_indent(lexer);
            lexer->dedent_count++;
        }
        
        // Check if we landed on a valid indentation level
        if (lexer->indent_stack[lexer->indent_stack_size - 1] != indent_level) {
            fprintf(stderr, "Error: Invalid indentation level at line %d\n", lexer->line);
            return (Token){TOKEN_ERROR, NULL, lexer->line, lexer->column};
        }
        
        if (lexer->dedent_count > 0) {
            lexer->pending_dedents = true;
            lexer->dedent_count--; // We'll return the first DEDENT now
            lexer->at_line_start = false;
            return (Token){TOKEN_DEDENT, NULL, lexer->line, lexer->column};
        }
    }
    
    // Same indentation level - continue with normal tokenization
    lexer->at_line_start = false;
    return lexer_next_token(lexer);
}
