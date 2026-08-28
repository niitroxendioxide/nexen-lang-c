#ifndef LANG_SCOPE_H
#define LANG_SCOPE_H

#include "parser/stack.h"

typedef struct Scope {
    Stack bindings;
    struct Scope* parent;
} Scope;

Scope* create_scope(Scope* parent);
Binding* lookup_in_scope(Scope* scope, const char* name);
void push_to_scope(Scope* scope, const char* name, Value value);
void free_scope(Scope* scope);

#endif
