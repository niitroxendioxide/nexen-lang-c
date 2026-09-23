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
    //if (DEBUG_ACTIONS != 1) {
        //return;
    //}

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

const char* expr_type_to_str(Expression* expr) {
    switch (expr->type) {
        case EXPR_BOOL: return "bool";
        case EXPR_NUMBER: return "number";
        case EXPR_STRING: return "string";
        case EXPR_ARRAY: return "array";
        case EXPR_DICT: return "dict";
        default: break;
    }

    return "NaN";
}


const char* expr_val_to_str(int val_type) {
    switch (val_type) {
        case EXPR_VAL_TYPE_ARRAY: return "array";
        case EXPR_VAL_TYPE_BOOLEAN: return "bool";
        case EXPR_VAL_TYPE_NUMBER: return "number";
        case EXPR_VAL_TYPE_STRING: return "string";
        case EXPR_VAL_TYPE_DICT: return "dict";
        case EXPR_VAL_TYPE_NIL: return "nil";
        case EXPR_VAL_TYPE_RANGE: return "range";
        default: break;
    }

    return "NaN";
}


void print_opcode(uint8_t op) {
    switch (op) {
        case OP_VOID:
            printf("> VOID\n");
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
            printf("> PUSH_1 ");
            break;
        case OP_PUSH_0:
            printf("> PUSH_0 ");
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
        case OP_NEW_STRUCT:
            printf("> NEW_STRUCT ");
            break;
        case OP_LOAD_FIELD:
            printf("> LOAD_FIELD ");
            break;
        case OP_ADD:
            printf("> ADD ");
            break;
        case OP_SUB:
            printf("> SUB ");
            break;
        case OP_MUL:
            printf("> MUL ");
            break;
        case OP_DIV:
            printf("> DIV ");
            break;
        /*case OP_PUSH_SCOPE:
            printf("> OP_PUSH_SCOPE\n");
            break;
        case OP_POP_SCOPE:
            printf("> OP_POP_SCOPE\n");
            break;*/
        case OP_EQ:
            printf("> EQ ");
            break;
        case OP_NOTEQ:
            printf("> NEQ ");
            break;
        case OP_GT:
            printf("> GT ");
            break;
        case OP_LT:
            printf("> LT ");
            break;
        case OP_LEQT:
            printf("> LEQT ");
            break;
        case OP_GEQT:
            printf("> GEQT ");
            break;
        case OP_JUMP_IF_FALSE:
            printf("> JNEQ ");
            break;
        case OP_JUMP_IF_TRUE:
            printf("> JEQ ");
            break;
        case OP_JUMP:
            printf("> JUMP ");
            break;
        case OP_CALL_FN:
            printf("> CALL ");
            break;
        case OP_RETURN:
            printf("> RET ");
            break;
        case OP_CALL_NATIVE:
            printf("> SYSCALL ");
            break;
        case OP_PUSH_ARRAY:
            printf("> PUSH_ARR ");
            break;
        case OP_LOAD_INDEX:
            printf("> LOAD_INDEX ");
            break;
        default:
            printf("> UNKNOWN\n");
            break;
    }
}

void print_bytes(uint8_t* bytes, int total) {
    for (int i = 0; i < total; i++) {
        uint8_t byte_up = bytes[i];
        print_opcode(byte_up);
        if (byte_up == OP_STORE_LOCAL || byte_up == OP_PUSH_0 || byte_up == OP_PUSH_1 || byte_up == OP_RETURN) {
            const char* k = (bytes[i] == OP_LOAD_CONST) ? "K" : "R";
            printf("%s%d\n", k, bytes[++i]);

        } else if (byte_up == OP_PUSH_U8 || byte_up == OP_LOAD_CONST || byte_up == OP_LOAD_LOCAL) {
            uint8_t reg = bytes[++i];
            uint8_t val = bytes[++i];
            printf("R%d, %d\n", reg, val);

        } else if (byte_up == OP_ADD || byte_up == OP_DIV || byte_up == OP_MUL || byte_up == OP_SUB || byte_up == OP_LT || byte_up == OP_GT || byte_up == OP_LEQT || byte_up == OP_GEQT || byte_up == OP_EQ) {
            uint8_t reg_dest = bytes[++i];
            uint8_t val1 = bytes[++i];
            uint8_t val2 = bytes[++i];
            printf("R%d, R%d, R%d\n", reg_dest, val1, val2); 

        } else if (byte_up == OP_CALL_NATIVE) {
            uint8_t reg_dest = bytes[++i];
            uint8_t fn_val = bytes[++i];
            uint8_t argc = bytes[++i];
            printf("R%d, [%d], [%d]\n", reg_dest, fn_val, argc); 

        } else if (byte_up == OP_PUSH_ARRAY) {
            uint8_t reg = bytes[++i];
            int b1 = bytes[++i];
            int b2 = bytes[++i];
            int b3 = bytes[++i];
            int b4 = bytes[++i];
            int value = b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
            printf("R%d, %d\n", reg, value);

        } else if (byte_up == OP_PUSH_U16) {
            uint8_t reg = bytes[++i];
            uint16_t byte1 = bytes[++i];
            uint16_t byte2 = bytes[++i];
            uint16_t value = byte1 | (byte2 << 8);
            printf("R%d, %d\n", reg, (int16_t) value);

        } else if (byte_up == OP_NEW_STRUCT) {
            uint8_t reg = bytes[++i];
            uint8_t size = bytes[++i];
            printf("R%d, %d\n", reg, size);

        } else if (byte_up == OP_LOAD_FIELD || byte_up == OP_LOAD_INDEX) {
            uint8_t reg_dest = bytes[++i];
            uint8_t reg_ref = bytes[++i];
            uint8_t idx = bytes[++i];
            if (byte_up == OP_LOAD_FIELD) printf("R%d, R%d, [%d]\n", reg_dest, reg_ref, idx);
            else printf("R%d, R%d, R%d\n", reg_dest, reg_ref, idx);

        } else if (byte_up == OP_JUMP) {
            int32_t byte1 = bytes[++i];
            int32_t byte2 = bytes[++i];
            int32_t byte3 = bytes[++i];
            int32_t byte4 = bytes[++i];
            int32_t value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
            printf("%d\n", (int32_t) value);

        }  else if (byte_up == OP_JUMP_IF_TRUE || byte_up == OP_JUMP_IF_FALSE) {
            uint8_t reg_compared = bytes[++i];
            int32_t byte1 = bytes[++i];
            int32_t byte2 = bytes[++i];
            int32_t byte3 = bytes[++i];
            int32_t byte4 = bytes[++i];
            int32_t value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
            printf("R%d, %d\n", (uint8_t) reg_compared, (int32_t) value);

        } else if (byte_up == OP_PUSH_NUM) {
            uint8_t reg = bytes[++i];
            double value;
            memcpy(&value, &bytes[i + 1], sizeof(double));
            printf("%f\n", value);
            i += 8;

        } else if (byte_up == OP_CALL_FN) {
            uint8_t reg = bytes[++i];
            uint32_t byte1 = bytes[++i];
            uint32_t byte2 = bytes[++i];
            uint32_t byte3 = bytes[++i];
            uint32_t byte4 = bytes[++i];
            
            uint32_t value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
            printf("R%d, %u\n", reg, value);
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

    // \033[3;30m(Entry Point):\033[0m 
    printf("\033[1;30m__start:\033[0m\n");
    print_bytes(program->bytes, program->byte_counter);
    printf("\n");
}
