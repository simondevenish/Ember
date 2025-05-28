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
}

void lexer_advance(Lexer* lexer) {
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1; // Reset column to 1 at the start of a new line
    } else {
        lexer->column++;
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
        if (lexer->current_char == ' ' || lexer->current_char == '\t' || lexer->current_char == '\n' || lexer->current_char == '\r') {
            // Skip whitespace
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
    lexer_skip_whitespace_and_comments(lexer);

    // End of input
    if (lexer->current_char == '\0') {
        return (Token){TOKEN_EOF, NULL, lexer->line, lexer->column};
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
        while (isdigit(lexer->current_char) || lexer->current_char == '.') {
            lexer_advance(lexer);
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
        lexer->current_char == '&' || lexer->current_char == '|') {
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
        } else {
            // Single-character operator (e.g., =, <, >, !)
            char* operator = (char*)malloc(2);
            operator[0] = first_char;
            operator[1] = '\0';
            return (Token){TOKEN_OPERATOR, operator, lexer->line, lexer->column};
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
    } else if (strchr("(){}[],;.:", current_char)) {
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
        "var", "const", "let", "true", "false", "null", "import", "fn"
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
