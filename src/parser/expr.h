#ifndef EXPR_H
#define EXPR_H

#include "parser/tokens.h"

typedef enum {
    EXPR_DEFINE,   
    EXPR_NAME,     
    EXPR_ASSIGN,       
    EXPR_NUMBER,
    EXPR_BOOL,
    EXPR_ARRAY,
    EXPR_BINARY_OPERATOR,
    EXPR_STATEMENT_END, 
    EXPR_BLOCK,
    EXPR_IF,
    EXPR_INDEX,
} ExprType;

typedef struct Expression {
    ExprType type;
    union {
        struct Expression* define_body;
        struct {
            struct Expression* name;
            struct Expression* value;
        } assign;

        struct {
            struct Expression** statements;
            size_t count;
        } block;

        struct {
            struct Expression** elements;
            size_t count;
        } array;

        struct {
            struct Expression* condition;
            struct Expression* branch_then;
            struct Expression* branch_else;
        } conditional;

        struct {
            struct Expression* target;
            struct Expression* index;
        } index_expr;

        struct {
            char* op;
            struct Expression* left;
            struct Expression* right;
        } operation;

        char* name;
        double value;
        int bool_val;
    } data;
} Expression;

typedef struct { float left; float right; } BindingPower;

BindingPower op_binding_power(const char* op);

void display_expression(Expression* expr);

Expression* parse_value(
    Token* tokens, 
    int* pos, 
    size_t token_count, 
    float weight
);

Expression* parse_block(
    Token* tokens, 
    int* pos, 
    size_t token_count
);

Expression** parse_statements(
    Token* tokens, 
    size_t token_count,
    int* current_token_pos,
    size_t* statement_count
);

#endif
