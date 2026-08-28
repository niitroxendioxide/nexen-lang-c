#include "parser/tokens.h"

#include <ctype.h>
#include <string.h>

/*
int is_operator(char* input) {
    return (strcmp(input, "+") == 0)
    || (strcmp(input, "-")  == 0)
    || (strcmp(input, "*")  == 0)
    || (strcmp(input, "/")  == 0)
    || (strcmp(input, "==") == 0)
    || (strcmp(input, "!=") == 0)
    || (strcmp(input, ">=") == 0)
    || (strcmp(input, "<=") == 0)
    || (strcmp(input, ".")  == 0)
    || (strcmp(input, "&&") == 0)
    || (strcmp(input, "||") == 0)
    || (strcmp(input, ":")  == 0)
    || (strcmp(input, "[")  == 0)
    || (strcmp(input, "]")  == 0)
    || (strcmp(input, "=")  == 0)
}
*/

int is_operator(char input) {
    return input == '+' || input == '-' || input == '*' || input == '/' || input == '=' || input == '.' || input == '&' || input == '|' || input == '!' || input == '>' || input == '<';
}

int is_statement_end(char input) {
    return input == ';';
}

int is_scope_begin(char input) {
    return input == '{';
}

int is_scope_end(char input) {
    return input == '}';
}

int is_comma(char input) {
    return input == ',';
}

TokenType is_parenthesis(char input) {
    if (input == '(') {
        return TOKEN_LPAREN;
    } else if (input == ')') {
        return TOKEN_RPAREN;
    }

    return TOKEN_EOF;
}

TokenType is_bracket(char input) {
    if (input == '[') {
        return TOKEN_LBRACKET;
    } else if (input == ']') {
        return TOKEN_RBRACKET;
    }

    return TOKEN_EOF;
}

void free_tokens(Token* tokens, int token_count) {
    if (tokens == NULL) return;

    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_NAME) {
            free(tokens[i].data.str_val); 
        }
    }

    free(tokens);
}

void display_token(Token token) {
    switch (token.type) {
        case TOKEN_NAME:
            fprintf(stderr, "TOKEN_CHAR: %s\n", token.data.str_val);
            break;
        case TOKEN_OP:
            fprintf(stderr, "TOKEN_OP: %s\n", token.data.op_val);
            break;
        case TOKEN_EOF:
            fprintf(stderr, "TOKEN_EOF\n");
            break;
        case TOKEN_SCOPE_BEGIN:
            fprintf(stderr, "TOKEN_SCOPE_BEGIN\n");
            break;
        case TOKEN_SCOPE_END:
            fprintf(stderr, "TOKEN_SCOPE_END\n");
            break;
        case TOKEN_STATEMENT_END:
            fprintf(stderr, "TOKEN_STATEMENT_END (;)\n");
            break;
        case TOKEN_RPAREN:
            fprintf(stderr, "TOKEN_RPAREN )\n");
            break;
        case TOKEN_LPAREN:
            fprintf(stderr, "TOKEN_LPAREN (\n");
            break;
        case TOKEN_RBRACKET:
            fprintf(stderr, "TOKEN_RBRACKET ]\n");
            break;
        case TOKEN_LBRACKET:
            fprintf(stderr, "TOKEN_LBRACKET [\n");
            break;

        default:
            fprintf(stderr, "UNKNOWN_TOKEN\n");
            break;
    }
}

Token* tokenize(const char* input, size_t input_len, int* token_count) {
    int size_current = 10;
    int count = 0;

    Token* tokens = malloc(size_current * sizeof(Token));

    if (tokens == NULL) return NULL;

    size_t i = 0;
    while (i < input_len) {
        if (isspace((unsigned char)input[i])) {
            i++;
            continue;
        }

        if (count >= size_current) {
            size_current *= 2;
            Token* temp = realloc(tokens, size_current * sizeof(Token));
            if (temp == NULL) {
                free(tokens);
                return NULL;
            }

            tokens = temp;
        };


        TokenType parenthesis = is_parenthesis(input[i]);
        TokenType bracket = is_bracket(input[i]);
        
        if (parenthesis != TOKEN_EOF) {
            tokens[count].type = parenthesis;
            count++;
            i++;
            continue;
        } else if (bracket != TOKEN_EOF) {
            tokens[count].type = bracket;
            count++;
            i++;
            continue;
        } else if (is_comma(input[i])) { 
            tokens[count].type = TOKEN_COMMA;
            count++;
            i++;
            continue;
        } else if (is_statement_end(input[i])) {
            tokens[count].type = TOKEN_STATEMENT_END;
            count++;
            i++;
            continue;
        } else if (is_scope_begin(input[i])) {
            tokens[count].type = TOKEN_SCOPE_BEGIN;
            count++;
            i++;
            continue;
        } else if (is_scope_end(input[i])) {
            tokens[count].type = TOKEN_SCOPE_END;
            count++;
            i++;
            continue;
        } else if (is_operator(input[i])) {
            tokens[count].type = TOKEN_OP;
            size_t begin = i;
            while (i < input_len) {
                if ( !is_operator(input[i]) ) {
                    break;
                }

                i++;
            }

            size_t word_length = i - begin;
            size_t mem_length = word_length + 1;
            char* word_memory = malloc(mem_length);
            if (word_memory != NULL) {
                memcpy(word_memory, input + begin, word_length);
                word_memory[word_length] = '\0';
            }

            tokens[count].data.op_val = word_memory;

            count++;
            i++;
        } else {
            tokens[count].type = TOKEN_NAME;

            size_t begin = i;
            while (i < input_len) {
                if (
                    isspace((unsigned char)input[i]) 
                    || is_operator(input[i]) 
                    || is_statement_end(input[i])
                    || is_parenthesis(input[i]) != TOKEN_EOF
                    || is_bracket(input[i]) != TOKEN_EOF
                    || is_comma(input[i])
                ) {
                    break;
                }

                i++;
            }

            size_t word_length = i - begin;
            size_t mem_length = word_length + 1;
            char* word_memory = malloc(mem_length);
            if (word_memory != NULL) {
                memcpy(word_memory, input + begin, word_length);
                word_memory[word_length] = '\0';
            }

            tokens[count].data.str_val = word_memory;

            count++;
        }
    }

    if (count >= size_current) {
        Token* temp = realloc(tokens, (count + 1) * sizeof(Token));
        if (temp != NULL) tokens = temp;
    }

    if (tokens != NULL) {
        tokens[count].type = TOKEN_EOF;
        tokens[count].data.str_val = '\0';
        count++;
    }

    if (token_count != NULL) {
        *token_count = count;
    }

    return tokens;
}
