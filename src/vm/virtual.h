#ifndef NEXEN_VM_H
#define NEXEN_VM_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "compiler/emit.h"
#include "parser/stack.h"
#include "parser/scope.h"
#include "native/stdfns.h"

typedef struct {
    uint32_t magic_constant;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t bytecode_size; 
} VMHeader;

typedef struct {
    ConstantType type;
    union {
        double number;
        char* string;
        int bool;
    } as;
} VMConstant;

typedef struct {
    uint32_t return_ip;
    Scope* scope_locals;
} CallFrame;

typedef struct {
    Value* stack;
    int stack_count;
    int stack_capacity;

    int call_frame_count;   // frames in use
    int call_frame_max;
    CallFrame* call_frames;

    Scope* globals;
} VM;

typedef struct {
    uint16_t body_offset;
    uint8_t param_count;
    uint16_t* param_name_indices;
} VMFunctionMeta;

void init_vm(VM* vm);
void free_vm(VM* vm);
void vm_push(VM* vm, Value value);
Value vm_pop(VM* vm);
void vm_push_call_frame(VM* vm, CallFrame frame);
CallFrame vm_pop_call_frame(VM* vm);
Scope* vm_current_scope(VM* vm);

void vm_execute(VM* vm, uint8_t* code, uint32_t code_size, VMConstant* constants, uint16_t constant_count,
                 VMFunctionMeta* functions, uint16_t function_count);

int vm_run_bytecode_from(const char* file_path, int file_debug, int show_elapsed_time);

#endif