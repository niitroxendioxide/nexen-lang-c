#ifndef NEXEN_NEWCOMPILER_H
#define NEXEN_NEWCOMPILER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "parser/intermediate.h"

#define MAX_SYMBOL_COUNT 256
#define MAX_U16 1 << 16
#define LANG_SIGNATURE 0x6E786F21
#define LANG_MAJOR_VER 1
#define LANG_MINOR_VER 1
#define LANG_PATCH_VER 4
#define LANGUAGE_BEGIN 0xFFFF

typedef enum {
    N_CONST_BOOL = 0,
    N_CONST_NUMBER = 1,
    N_CONST_STRING = 2,
    N_CONST_FUNCTION = 3,
    N_CONST_ARRAY = 4,
    N_CONST_DICT = 5,
    N_CONST_CLASS = 6,
} ConstantType;

typedef struct {
    uint8_t type;
    union {
        double number;
        int8_t boolean;
        char* string;

        struct {
            uint8_t* bytes;
            uint8_t length;
        } function_body;
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
    OP_PUSH_U16 = 0x09,
    OP_PUSH_U8 = 0x0A,
    OP_PUSH_1 = 0x0B,
    OP_PUSH_0 = 0x0C,
    //OP_PUSH_SCOPE = 0x0D,  |
    //OP_POP_SCOPE = 0x0E,   | ==> unused/discarded in run-time
    // scopes or ifs
    OP_JUMP = 0x0F,
    OP_EQ = 0x10,
    OP_NOTEQ = 0x11,
    OP_LT = 0x12,
    OP_GT = 0x13,
    OP_LEQT = 0x14,
    OP_GEQT = 0x15,
    OP_JUMP_IF_TRUE = 0x16,
    OP_JUMP_IF_FALSE = 0x17,
    // functions
    OP_CALL_FN = 0x18,
    OP_RETURN = 0x19,
    OP_CALL_NATIVE = 0x1A,
    //arrays & dicts
    OP_PUSH_ARRAY = 0x1B,
    OP_PUSH_DICT = 0x1C,
    OP_DICT_SET = 0x1D,
    OP_NEW_STRUCT = 0x1E,
    OP_LOAD_FIELD = 0x1F,
} OpCode;

typedef struct {
    const char* name;
    int index;
    int unique_index;
    ConstantType type;
} Symbol;

#define DEF_NATIVE_FN(fn_name, idx) (Symbol){.index = idx, .name = fn_name, .unique_index = idx, .type = N_CONST_FUNCTION }

typedef struct SymbolTable {
    Symbol symbols[MAX_SYMBOL_COUNT];
    int count;
    struct SymbolTable* parent;
} SymbolTable;

typedef struct Function {
    uint8_t* bytes;
    const char* name;
    uint8_t reg_count;
    uint8_t arg_count;
    int length;
} Function;

typedef struct Program {
    struct Program* enclosing;

    // allocated
    Constant* constants;
    uint8_t* bytes;
    SymbolTable* symbol_table;
    Function** functions;


    // info
    int reserved_registers;
    int index_counter;
    int byte_limit;
    int func_limit;
    int func_count;
    uint32_t byte_counter;

    int constant_limit;
    uint32_t constant_counter;
} Program;

// should be an OpCode but i want to force it to strictly turn into a uint8_t
void emit_byte(Program* program, uint8_t byte);
uint32_t push_constant(Program* program, Constant constant);
int compile_program(const char* file_name, const char* file_output, int see_bytecode);
void print_program_bytecode(const char* file_name);

#endif