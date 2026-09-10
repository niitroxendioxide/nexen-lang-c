#ifndef NEXEN_NEWCOMPILER_H
#define NEXEN_NEWCOMPILER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "parser/intermediate.h"

#define DEBUG_ACTIONS 0
#define MAX_SYMBOL_COUNT 100
#define MAX_I16 1 << 16
#define LANG_SIGNATURE 0x6E786F21
#define LANG_MAJOR_VER 1
#define LANG_MINOR_VER 0
#define LANGUAGE_BEGIN 0xFFFF

typedef enum {
    N_CONST_BOOL = 0,
    N_CONST_NUMBER = 1,
    N_CONST_STRING = 2,
} ConstantType;

typedef struct {
    uint8_t type; // ConstantType
    union {
        double number;
        int8_t boolean;
        char* string;
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
    // faster const loading
    OP_PUSH_NUM = 0x08,
    OP_PUSH_I16 = 0x09,
    OP_PUSH_U8 = 0x0A,
    OP_PUSH_1 = 0x0B,
    OP_PUSH_0 = 0x0C,
    // scopes or ifs
    OP_PUSH_SCOPE = 0x0D,
    OP_POP_SCOPE = 0x0E,
    OP_JUMP = 0x0F,
    OP_EQ = 0x10,
    OP_NOTEQ = 0x11,
    OP_LT = 0x12,
    OP_GT = 0x13,
    OP_LEQT = 0x14,
    OP_GEQT = 0x15,
    OP_JUMP_IF_TRUE = 0x16,
    OP_JUMP_IF_FALSE = 0x17,
} OpCode;

typedef struct {
    const char* name;
    int index;
    int unique_index;
} Symbol;

typedef struct SymbolTable {
    Symbol symbols[MAX_SYMBOL_COUNT];
    int count;
    struct SymbolTable* parent;
} SymbolTable;

typedef struct {
    // allocated
    Constant* constants;
    uint8_t* bytes;
    SymbolTable* symbol_table;

    // info
    int index_counter;
    int byte_limit;
    uint32_t byte_counter;

    int constant_limit;
    uint32_t constant_counter;
} Program;

// should be an OpCode but i want to force it to strictly turn into a uint8_t
void emit_byte(Program* program, uint8_t byte);
uint32_t push_constant(Program* program, Constant constant);
int compile_program(const char* file_name, const char* file_output);
void print_program_bytecode(const char* file_name);

#endif