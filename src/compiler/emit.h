#ifndef COMPILER_EMIT_H
#define COMPILER_EMIT_H

#define LANG_SIGNATURE 0x6E786F21

typedef enum {
    CONST_NUMBER = 0,
    CONST_STRING = 1,
    CONST_BOOL = 2,
} ConstantType;

typedef enum {
    OP_PUSH_CONST = 0x01,  // u16 const_idx : push constants[idx] (number or string) onto the stack
    OP_POP        = 0x02,  // pop and discard the top of the stack

    OP_DEFINE_NAME = 0x03, // u16 name_idx : pop value, bind name in the current scope   (let x = ...)
    OP_LOAD_NAME   = 0x04, // u16 name_idx : push the value currently bound to name
    OP_STORE_NAME  = 0x05, // u16 name_idx : pop value, reassign an existing binding     (x = ...)

    OP_BINARY_ADD = 0x06,
    OP_BINARY_SUB = 0x07,
    OP_BINARY_MUL = 0x08,
    OP_BINARY_DIV = 0x09,

    OP_CALL = 0x0A, // u8 arg_count : (callee, a1, ..., aN) -> (result)

    OP_PUSH_UNDEFINED = 0x0B, // () -> (undefined)
    OP_JUMP           = 0x0C, // u16 target_offset : unconditional jump, absolute offset into code
    OP_RETURN         = 0x0D, // (value) -> ()  unwind the current call frame, yielding value

    // u16 function_idx : () -> (function)
    // function_idx indexes the function table (see Chunk.functions / the
    // function-table section written after the constant pool), not the
    // constant pool — that's where body_offset/param_count/param names live.
    OP_MAKE_FUNCTION = 0x0E,
} OpCode;

#endif
