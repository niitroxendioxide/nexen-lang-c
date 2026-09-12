#include "newcomp/debugger.h"
#include "newcomp/newcompiler.h"
#include <stdarg.h>

void debug_print(const char* text) {
    if (DEBUG_ACTIONS != 1) {
        return;
    }

    fprintf(stderr, "\033[1;32m[Compiler]\033[0m: %s\n", text);
}

void debug_printerr(const char* text) {
    if (DEBUG_ACTIONS != 1) {
        return;
    }

    fprintf(stderr, "\033[1;31m[WARNING]\033[0m: %s\n", text);
}

void debug_print_formatted(const char* format, ...) {
    char buffer[512];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    debug_print(buffer); 
}

void print_opcode(uint8_t op) {
    switch (op) {
        case OP_VOID:
            printf("> NO_OP\n");
            break;
        case OP_PUSH_NUM:
            printf("> PUSH_NUM ");
            break;
        case OP_PUSH_U16:
            printf("> PUSH_U16 ");
            break;
        case OP_PUSH_U8:
            printf("> PUSH_U8 ");
            break;
        case OP_PUSH_1:
            printf("> PUSH_1\n");
            break;
        case OP_PUSH_0:
            printf("> PUSH_0\n");
            break;
        case OP_LOAD_CONST:
            printf("> LOAD_CONST ");
            break;
        case OP_STORE_LOCAL:
            printf("> STORE_LOCAL ");
            break;
        case OP_LOAD_LOCAL:
            printf("> LOAD_LOCAL ");
            break;
        case OP_ADD:
            printf("> OP_ADD\n");
            break;
        case OP_SUB:
            printf("> OP_SUB\n");
            break;
        case OP_MUL:
            printf("> OP_MUL\n");
            break;
        case OP_DIV:
            printf("> OP_DIV\n");
            break;
        case OP_PUSH_SCOPE:
            printf("> OP_PUSH_SCOPE\n");
            break;
        case OP_POP_SCOPE:
            printf("> OP_POP_SCOPE\n");
            break;
        case OP_EQ:
            printf("> OP_EQUAL\n");
            break;
        case OP_NOTEQ:
            printf("> OP_NOT_EQUAL\n");
            break;
        case OP_GT:
            printf("> OP_GREATER_THAN\n");
            break;
        case OP_LT:
            printf("> OP_LESS_THAN\n");
            break;
        case OP_LEQT:
            printf("> OP_LESS_EQUAL_THAN\n");
            break;
        case OP_GEQT:
            printf("> OP_GREATER_EQUAL_THAN\n");
            break;
        case OP_JUMP_IF_FALSE:
            printf("> OP_JUMP_IF_FALSE ");
            break;
        case OP_JUMP_IF_TRUE:
            printf("> OP_JUMP_IF_TRUE ");
            break;
        case OP_JUMP:
            printf("> OP_JUMP ");
            break;
        case OP_CALL_FN:
            printf("> OP_CALL ");
            break;
        case OP_RETURN:
            printf("> OP_RETURN\n");
            break;
        default:
            printf("> OP_UNKNOWN\n");
            break;
    }
}

void print_bytes(uint8_t* bytes, int total) {
    for (int i = 0; i < total; i++) {
        uint8_t byte_up = bytes[i];
        print_opcode(byte_up);
        if (byte_up == OP_LOAD_CONST || byte_up == OP_LOAD_LOCAL 
            || byte_up == OP_STORE_LOCAL || byte_up == OP_PUSH_U8) {
            i++;
            printf("%d\n", bytes[i]);
        } else if (byte_up == OP_PUSH_U16 || byte_up == OP_JUMP || byte_up == OP_JUMP_IF_TRUE || byte_up == OP_JUMP_IF_FALSE) {
            uint16_t byte1 = bytes[++i];
            uint16_t byte2 = bytes[++i];
            uint16_t value = byte1 | (byte2 << 8);
            printf("%d\n", (int16_t)value);
        } else if (byte_up == OP_PUSH_NUM) {
            double value;
            memcpy(&value, &bytes[i + 1], sizeof(double));
            printf("%f\n", value);
            i += 8;
        } else if (byte_up == OP_CALL_FN) {
            uint32_t byte1 = bytes[i + 1];
            uint32_t byte2 = bytes[i + 2];
            uint32_t byte3 = bytes[i + 3];
            uint32_t byte4 = bytes[i + 4];
            
            uint32_t value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
            printf("%u\n", value);
    
            i += 4;
        }
    }
}

void print_compiled_program(Program* program) {
    printf("\n\033[1;30mPROGRAM:\033[0m\n");

    for (int i = 0; i < program->func_count; i++) {
        Function* fn = program->functions[i];
        printf("__%s:\033[0m\n", fn->name);
        print_bytes(fn->bytes, fn->length);
        printf("\n");
    }

    printf("\033[3;30m(Entry Point):\033[0m \n\033[1;30m__start:\033[0m\n");
    print_bytes(program->bytes, program->byte_counter);
    printf("\n");
}
