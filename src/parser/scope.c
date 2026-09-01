#include "parser/scope.h"

void display_value(const char* name, Value value) {
    switch (value.type) {
        case VALUE_STRING:
            printf("> (%s): %s\n", name, value.as.str_val);
            break;
        case VALUE_NUMBER:
            printf("> (%s): %f\n", name, value.as.num_val);
            break;
        case VALUE_BOOL: {
            const char *bool_state_text = (value.as.bool_val == 1) ? "true" : "false";
            printf("> (%s): %s\n", name, bool_state_text);
            break;
        }
        case VALUE_UNDEFINED:
            printf("> (%s): undefined\n", name);
            break;
        default:
            printf("> (%s): <value unreadable [type=%d, name=\"%s\"]>\n", name, value.type, name);
            break;
    }
}

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
