#ifndef NEXEN_NEWCOMPILER_H
#define NEXEN_NEWCOMPILER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "parser/intermediate.h"

#define DEBUG_ACTIONS 1
#define MAX_SYMBOL_COUNT 100

typedef enum {
    N_CONST_BOOL = 0,
    N_CONST_NUMBER = 1,
    // N_CONST_STRING = 2,
} ConstantType;

typedef struct {
    uint8_t type; // ConstantType?
    union {
        double number;
        int8_t boolean;
    } as;
} Constant;

typedef enum {
    OP_VOID = 0x00,
    OP_LOAD_CONST = 0x01,
    OP_STORE_LOCAL = 0x02,
    OP_LOAD_LOCAL = 0x03,
    OP_ADD = 0x04,
    OP_SUB = 0x05,
    OP_MUL = 0x06,
    OP_DIV = 0x07,
} OpCode;

typedef struct {
    const char* name;
    uint16_t index;
} Symbol;

typedef struct {
    Symbol symbols[MAX_SYMBOL_COUNT];
    uint16_t count;
} SymbolTable;

typedef struct {
    // allocated
    Constant* constants;
    uint8_t* bytes;
    SymbolTable symbol_table;

    // info
    int byte_limit;
    uint32_t byte_counter;

    int constant_limit;
    uint32_t constant_counter;
} Program;

// should be an OpCode but i want to force it to strictly turn into a uint8_t
void emit_byte(Program* program, uint8_t byte);
uint32_t push_constant(Program* program, Constant constant);
int compile_program(const char* file_name);

#endif