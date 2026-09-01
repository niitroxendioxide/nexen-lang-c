#ifndef LANG_EVAL_H
#define LANG_EVAL_H

#include "parser/scope.h"
#include "parser/expr.h"

void display_value(const char* name, Value displayed);
Value apply_operator(char* op, Value left, Value right);
Value index_into(Value accessed_value, Value index_value);
int is_truthy(Value val);
Value evaluate(Expression* expr, Scope* scope);

#endif
