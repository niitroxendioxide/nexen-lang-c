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
    VALUE_RANGE,
    VALUE_MODULE,
} ValueType;

typedef struct Scope Scope;
typedef struct Value Value;
typedef Value (*NativeFn)(Value* args, size_t arg_count);

typedef struct Value {
    ValueType type;
    int is_return;
    uint8_t is_exported;
    union {
        char* str_val;
        double num_val;
        int bool_val;

        struct {
            struct Value* items;
            size_t count;
            size_t capacity;
        } array_val;

        struct Scope* module_scope;

        struct {
            double end;
            double start;
            int included;
        } range;

        struct {
            struct Value* keys;
            struct Value* values;
            size_t count;
            size_t capacity;
        } dict_val;

        struct {
            struct Expression* def;
            struct Scope* closure;
            uint32_t bytecode_offset;
            uint8_t param_count;      
            uint16_t* param_name_indices; 
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
