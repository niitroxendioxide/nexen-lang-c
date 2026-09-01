#include "parser/scope.h"


Scope* create_scope(Scope* parent) {
    Scope* scope = malloc(sizeof(Scope));
    if (scope == NULL) {
        fprintf(stderr, "malloc failed for scope\n");
        exit(1);
    }

    scope->bindings.bindings = NULL; // matches how you zero-initialized program_stack in main
    scope->bindings.count = 0;
    scope->bindings.capacity = 0;
    scope->parent = parent;

    return scope;
}

Binding* lookup_in_scope(Scope* scope, const char* name) {
    Binding* value_in_stack = lookup_binding(&scope->bindings, name);
    if (value_in_stack != NULL) {
        return value_in_stack;
    }

    if (scope->parent != NULL) {
        return lookup_in_scope(scope->parent, name);
    }
    return NULL;
}

void push_to_scope(Scope* scope, const char* name, Value value) {
    push_to_stack(&scope->bindings, name, value);
}

void free_scope(Scope* scope) {
    free_stack(&scope->bindings);
    free(scope);
}
