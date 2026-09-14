#ifndef COMPILER_DEBUGGER_H
#define COMPILER_DEBUGGER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_ACTIONS 0

typedef struct Compiler Compiler;
typedef struct Program Program;
typedef struct Expression Expression;

void print_opcode(uint8_t opcode);
void debug_print(const char* text);
void debug_printerr(const char* text);
void debug_print_formatted(const char* format, ...);
void print_bytes(uint8_t* bytes, int count);
void print_compiled_program(Program* program);
const char* expr_type_to_str(Expression* expr);

#endif