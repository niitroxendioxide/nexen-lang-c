#ifndef LANG_EVAL_H
#define LANG_EVAL_H

#include "parser/scope.h"
#include "parser/expr.h"

Value apply_operator(char* op, Value left, Value right);
int is_truthy(Value val);
Value evaluate(Expression* expr, Scope* scope);

#endif
