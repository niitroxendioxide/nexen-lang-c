#include "parser/stack.h"

void push_to_stack(Stack* stack, const char* name, Value value) {
    if (stack->capacity <= 0) {
        stack->capacity = 10;
        Binding* temp_allocated = malloc(sizeof(Binding) * stack->capacity);
        if (temp_allocated == NULL) {
            fprintf(stderr, "Stack pushed into with size 0, malloc failed\n.");
            exit(1);
            return;
        }

        stack->bindings = temp_allocated;
    }

    if (stack->count + 1 >= stack->capacity) {
        stack->capacity *= 2;

        Binding* temp = realloc(stack->bindings, sizeof(Binding) * stack->capacity);
        if (temp == NULL) {
            fprintf(stderr, "Failed to reallocate memory for the stack.\n");
            exit(1);
            return; 
        };

        stack->bindings = temp;
    };

    stack->bindings[stack->count].name = strdup(name);    
    stack->bindings[stack->count].value = value;
    stack->count++;
}

void free_stack(Stack* stack) {
    if (stack == NULL || stack->bindings == NULL) return;
    for (int i = 0; i < stack->count; i++) {
        free(stack->bindings[i].name);
    }
    free(stack->bindings);
    stack->bindings = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

Binding* lookup_binding(Stack* stack, const char* name) {
    for (int i = stack->count - 1; i >= 0; i--) {
        if (strcmp(stack->bindings[i].name, name) == 0) {
            return &stack->bindings[i];
        }
    }

    return NULL;
}

