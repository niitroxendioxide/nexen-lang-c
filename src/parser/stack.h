#ifndef LANG_STACK_H
#define LANG_STACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_UNDEFINED,
    VALUE_BOOL,
    VALUE_ARRAY,
    VALUE_DICT
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        char* str_val;
        double num_val;
        int bool_val;

        struct {
            struct Value* items;
            size_t count;
            size_t capacity;
        } array_val;
    } as;
} Value; 

typedef struct {
    char* name;
    Value value; 
} Binding;

typedef struct {
    Binding* bindings;
    int count;
    int capacity;
} Stack;

void push_to_stack(
    Stack* stack, 
    const char* name, 
    Value value
);


void free_stack(Stack* stack);
Binding* lookup_binding(Stack* stack, const char* name);

#endif
