#ifndef NEXEN_INTERMEDIATE_H
#define NEXEN_INTERMEDIATE_H

#include "parser/expr.h"
#include "file/content.h"

typedef struct {
    Token* tokens;
    int token_count;
    Expression** expressions;
    size_t expression_count;
} ParsedProgram;

ParsedProgram* parse_file_expressions(const char* file);

#endif