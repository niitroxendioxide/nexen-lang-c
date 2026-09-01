#ifndef LANG_STACK_H
#define LANG_STACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_UNDEFINED,
    VALUE_BOOL,
    VALUE_ARRAY,
    VALUE_DICT,
    VALUE_FUNCTION,
    VALUE_NATIVE_FUNCTION,
} ValueType;

typedef struct Scope Scope;
typedef struct Value Value;
typedef Value (*NativeFn)(Value* args, size_t arg_count);

typedef struct Value {
    ValueType type;
    int is_return;
    union {
        char* str_val;
        double num_val;
        int bool_val;

        struct {
            struct Value* items;
            size_t count;
            size_t capacity;
        } array_val;

        struct {
            struct Value* keys;
            struct Value* values;
            size_t count;
            size_t capacity;
        } dict_val;

        struct {
            struct Expression* def;
            struct Scope* closure;
            uint32_t bytecode_offset; // prob code smell to reuse value in VM & Code but it helps reusing functions, things dont gotta be perfect
            uint8_t param_count;      // VM only: how many entries param_name_indices holds
            uint16_t* param_name_indices; // VM only: borrowed from the loaded function table, not owned by this Value
        } func_val;

        NativeFn native_val;
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
