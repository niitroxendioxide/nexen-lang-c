#ifndef COMPILER_CMP_H
#define COMPILER_CMP_H

#include <stdint.h>
#include "compiler/emit.h"
#include "parser/expr.h"

typedef struct {
    ConstantType type;
    union {
        double number;
        char* string;
        int bool;
    } as;
} Constant;

// One entry per EXPR_FUNCTION_DEF compiled. body_offset points into the same
// Chunk.code the rest of the program lives in (the body is compiled inline,
// skipped over with an OP_JUMP at definition time) — this is metadata about
// that code, not a second code buffer. param_name_indices are constant-pool
// indices (owned by this struct, freed with the Chunk).
typedef struct {
    uint16_t body_offset;
    uint8_t param_count;
    uint16_t* param_name_indices;
} FunctionMeta;

typedef struct {
    uint8_t* code;
    int count;
    int capacity;
    Constant* constants;
    int constant_count;
    int constant_capacity;
    FunctionMeta* functions;
    int function_count;
    int function_capacity;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);

void emit_byte(Chunk* chunk, uint8_t byte);
void emit_u16(Chunk* chunk, uint16_t value);

// Forward-jump patching: emit_jump writes the jump opcode plus a 0xFFFF
// placeholder and returns the placeholder's offset; patch_jump backfills that
// placeholder with the current end of the chunk once the jump target is
// known (i.e. once whatever the jump was skipping has finished compiling).
int emit_jump(Chunk* chunk, uint8_t jump_opcode);
void patch_jump(Chunk* chunk, int jump_operand_offset);

int add_number_constant(Chunk* chunk, double value);
int add_string_constant(Chunk* chunk, const char* value);
int add_function(Chunk* chunk, uint16_t body_offset, uint8_t param_count, uint16_t* param_name_indices);

void compile(Expression* expr, Chunk* chunk);

int compile_program(const char* file_name, const char* file_output, int show_tokens);

#endif
