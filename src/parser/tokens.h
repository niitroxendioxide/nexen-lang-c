#ifndef TOKENS_H
#define TOKENS_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    TOKEN_NAME,
    TOKEN_OP,
    TOKEN_EOF,
    TOKEN_STATEMENT_END,
    TOKEN_SCOPE_BEGIN,
    TOKEN_SCOPE_END,
} TokenType;

typedef struct {
    TokenType type;
    union {
        char* str_val;
        char* op_val;
    } data;
} Token;

// utils
void free_tokens(Token* tokens, int token_count);
void display_token(Token token);


// working with the lang
Token* tokenize(const char* input, size_t input_len, int* token_count);



#endif
