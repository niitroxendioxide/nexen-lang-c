#include "newcomp/newcompiler.h"
#include <stdarg.h>
#include <string.h>

/* debugger */
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
        case OP_PUSH_I16:
            printf("> PUSH_I16 ");
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
        default:
            printf("> OP_UNKNOWN\n");
            break;
    }
}

void print_compiled_program(Program* program) {
    printf("\n\033[1;30mINSTRUCTION SET:\033[0m\n");
    for (int i = 0; i < program->byte_counter; i++) {
        uint8_t byte_up = program->bytes[i];
        print_opcode(byte_up);
        if (byte_up == OP_LOAD_CONST || byte_up == OP_LOAD_LOCAL 
            || byte_up == OP_STORE_LOCAL || byte_up == OP_PUSH_U8) {
            i++;
            printf("%d\n", program->bytes[i]);
        } else if (byte_up == OP_PUSH_I16 || byte_up == OP_JUMP || byte_up == OP_JUMP_IF_TRUE || byte_up == OP_JUMP_IF_FALSE) {
            uint16_t byte1 = program->bytes[++i];
            uint16_t byte2 = program->bytes[++i];
            uint16_t value = byte1 | (byte2 << 8);
            printf("%d\n", (int16_t)value);
        } else if (byte_up == OP_PUSH_NUM) {
            double value;
            memcpy(&value, &program->bytes[i + 1], sizeof(double));
            printf("%f\n", value);
            i += 8;
        }
    }
    printf("\n");
}

/* warnings */
void allocate_const_pool_if_full(Program* program) {
    if (program->constant_counter < program->constant_limit) {
        return;
    }

    if (program->constant_limit <= 0) {
        program->constant_limit = 10;
    }

    program->constant_limit *= 2;

    Constant* reallocated = realloc(program->constants, program->constant_limit * sizeof(Constant));
    if (reallocated == NULL) {
        fprintf(stderr, "Could not reallocate program constants memory\n");
        exit(1);
    }
    
    program->constants = reallocated;
}

void allocate_program_bytes_if_full(Program* program) {
    if (program->byte_counter < program->byte_limit) {
        return;
    }

    if (program->byte_limit <= 0) {
        program->byte_limit = 10;
    }

    program->byte_limit *= 2;

    uint8_t* reallocated_bytes = realloc(program->bytes, program->byte_limit * sizeof(*program->bytes));
    if (reallocated_bytes == NULL) {
        fprintf(stderr, "Could not reallocate program constants memory\n");
        exit(1);
    }
    
    program->bytes = reallocated_bytes;
}

/* symbol indexing */
int get_symbol_from_table(SymbolTable* table, const char* symbol) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, symbol) == 0) {
            return table->symbols[i].unique_index;
        }
    }

    if (table->parent != NULL) {
        return get_symbol_from_table(table->parent, symbol);
    }

    fprintf(stderr, "Undefined symbol: %s", symbol);
    exit(1);
}

int get_symbol_index(Program* program, const char* symbol) {
    return get_symbol_from_table(program->symbol_table, symbol);
} 

int push_symbol(Program* program, const char* symbol) {
    if (program->symbol_table->count >= MAX_SYMBOL_COUNT) {
        fprintf(stderr, "Program exceeded the maximum amount of symbols\n");
        exit(1);
    }

    for (int i = 0; i < program->symbol_table->count; i++) {
        if (strcmp(program->symbol_table->symbols[i].name, symbol) == 0) {
            fprintf(stderr, "Redefining existing symbol %s\n", symbol);
            exit(1);
        }
    }

    Symbol new_symbol = {
        .name = symbol,
        .index = program->symbol_table->count,
        .unique_index = program->index_counter,
    };

    program->symbol_table->symbols[program->symbol_table->count] = new_symbol;
    program->symbol_table->count++;

    return program->index_counter++;
}

void push_scope(Program* program) {
    SymbolTable* deeper_scope = malloc( sizeof(SymbolTable) );
    if (deeper_scope == NULL) {
        fprintf(stderr, "Could not allocate memory for the deeper scope.\n");
        exit(1);
    }

    deeper_scope->count = 0;
    deeper_scope->parent = program->symbol_table;
    program->symbol_table = deeper_scope;
}

void pop_scope(Program* program) {
    if (program->symbol_table->parent == NULL) {
        fprintf(stderr, "Compiling error, tried to pop-scope on parent scope.\n");
        exit(1);
    }

    SymbolTable* parent = program->symbol_table->parent;
    int added_symbols = (int) program->symbol_table->count;

    free(program->symbol_table);
    program->index_counter -= added_symbols;
    program->symbol_table = parent;
}

/* emitting */
void emit_byte(Program* program, uint8_t byte) {
    allocate_program_bytes_if_full(program);
    program->bytes[program->byte_counter] = byte;
    program->byte_counter++;
}

void override_byte(Program* program, uint8_t byte, int pointer) {
    if (pointer >= program->byte_limit) {
        return;
    }

    program->bytes[pointer] = byte;
}

void override_word(Program* program, uint16_t word, int pointer) {
    override_byte(program, word & 0xFF, pointer);
    override_byte(program, (word >> 8) & 0xFF, pointer + 1);
}

void emit_word(Program* program, uint16_t word) {
    emit_byte(program, word & 0xFF);
    emit_byte(program, (word >> 8) & 0xFF);
}

void emit_qword(Program* program, uint64_t qword) {
    emit_word(program, (qword) & 0xFFFF);
    emit_word(program, (qword >> 16) & 0xFFFF);
    emit_word(program, (qword >> 32) & 0xFFFF);
    emit_word(program, (qword >> 48) & 0xFFFF);
}

uint32_t push_constant(Program* program, Constant constant_val) {
    allocate_const_pool_if_full(program);
    program->constants[program->constant_counter] = constant_val;
    return program->constant_counter++;
}

/* initializing program & scopes */
Program* init_program() {
    Program* new_program = malloc(sizeof(Program));
    new_program->constant_counter = 0;
    new_program->byte_counter = 0;
    new_program->constant_limit = 10;
    new_program->byte_limit = 10;
    new_program->index_counter = 0;
    new_program->constants = malloc(new_program->constant_limit * sizeof(Constant));
    new_program->bytes = malloc(new_program->byte_limit * sizeof(uint8_t));
    new_program->symbol_table = malloc(sizeof(SymbolTable));

    return new_program;
}

/* evaluating and compiling */
OpCode opcode_for_op(const char* op) {
    if (strcmp(op, "+") == 0) { return OP_ADD; }
    if (strcmp(op, "-") == 0) { return OP_SUB; }
    if (strcmp(op, "/") == 0) { return OP_DIV; }
    if (strcmp(op, "*") == 0) { return OP_MUL; }
    if (strcmp(op, "==") == 0) { return OP_EQ; }
    if (strcmp(op, "!=") == 0) { return OP_NOTEQ; }
    if (strcmp(op, ">=") == 0) { return OP_GEQT; }
    if (strcmp(op, "<=") == 0) { return OP_LEQT; }
    if (strcmp(op, "<") == 0) { return OP_LT; }
    if (strcmp(op, ">") == 0) { return OP_GT; }

    return OP_VOID;
}

uint32_t compile_expr(Program* program, Expression* expr) {
    if (expr == NULL) return 0;

    switch (expr->type) {
        case EXPR_NUMBER: {
            double num_value = expr->data.value;
            if (num_value < 255) {
                emit_byte(program, OP_PUSH_U8);
                emit_byte(program, (uint8_t) num_value);
            } else if (abs(num_value) < MAX_I16) {
                emit_byte(program, OP_PUSH_I16);
                emit_word(program, (uint16_t) num_value);
            } else {
                emit_byte(program, OP_PUSH_NUM);
                emit_qword(program, (uint64_t) num_value);
            }

            break;
        }

        case EXPR_BLOCK: {
            emit_byte(program, OP_PUSH_SCOPE);
            push_scope(program);
            for (int i = 0; i < expr->data.block.count; i++) {
                Expression* cur_block_expr = expr->data.block.statements[i];
                compile_expr(program, cur_block_expr);
            }
            pop_scope(program);
            emit_byte(program, OP_POP_SCOPE);

            break;
        }

        case EXPR_IF: {
            Expression* compared = expr->data.conditional.condition;
            compile_expr(program, compared);

            emit_byte(program, OP_JUMP_IF_FALSE);
            int pre_then_branch_counter = program->byte_counter;
            emit_word(program, 0xFFFF);

            compile_expr(program, expr->data.conditional.branch_then);
            
            emit_byte(program, OP_JUMP);
            int post_then_branch_counter = program->byte_counter;
            emit_word(program, 0xFFFF);

            uint16_t relative_jump = (uint16_t) (post_then_branch_counter - pre_then_branch_counter);
            if (relative_jump > 0xFFFF) {
                fprintf(stderr, "Block too big\n");
                exit(1);
            }

            override_word(program, relative_jump, pre_then_branch_counter);
            compile_expr(program, expr->data.conditional.branch_else);
            int post_else_branch_counter = program->byte_counter;
            int finished_relative_jump = (post_else_branch_counter) - (post_then_branch_counter + 2);
            override_word(program, finished_relative_jump, post_then_branch_counter);

            break;
        }

        case EXPR_BINARY_OPERATOR: {
            Expression* left = expr->data.operation.left;
            Expression* right = expr->data.operation.right;

            compile_expr(program, left);
            compile_expr(program, right);
            
            OpCode operation = opcode_for_op(expr->data.operation.op);
            if (operation == OP_VOID) {
                fprintf(stderr, "Operation [%s] not implemented\n", expr->data.operation.op);
                exit(1);
            }
            
            emit_byte(program, operation);

            break;
        }

        case EXPR_STRING: {
            Constant new_constant = {
                .type = N_CONST_STRING,
                .as.string = expr->data.name,
            };

            uint32_t const_pointer = push_constant(program, new_constant);
            debug_print_formatted("Pushed String \033[1;34m[\"%s\"]\033[0m to the constant pool \033[1;35m[idx=%d]\033[0m", expr->data.name, const_pointer);

            emit_byte(program, OP_LOAD_CONST);
            emit_byte(program, (uint8_t) const_pointer);

            break;
        }

        case EXPR_BOOL: {
            if (expr->data.bool_val == 0) {
                emit_byte(program, OP_PUSH_0);
            } else {
                emit_byte(program, OP_PUSH_1);
            }

            break;
        }

        case EXPR_NAME: {
            uint16_t symbol_index = get_symbol_index(program, expr->data.name);
            emit_byte(program, OP_LOAD_LOCAL);
            emit_byte(program, (uint8_t) symbol_index);

            return symbol_index;
        }

        case EXPR_DEFINE: {
            Expression* def_body = expr->data.define_body;
            Expression* assign_name = def_body->data.assign.name;
            uint16_t symbol_index = push_symbol(program, assign_name->data.name);

            compile_expr(program, def_body);
            break;
        }

        case EXPR_ASSIGN: {
            Expression* assign_value = expr->data.assign.value;
            const char* assign_name = expr->data.assign.name->data.name;
            uint16_t stored_symbol_index = get_symbol_index(program, assign_name);
            compile_expr(program, assign_value);
            
            emit_byte(program, OP_STORE_LOCAL);
            emit_byte(program, (uint8_t) stored_symbol_index);

            break;
        }

        default: {
            debug_printerr("Unsupported expression skipped, type:");
            display_expression(expr);
            break;
        }
    }

    return 0;
}

int write_to_file(const char* output, Program* program) {
    if (program->byte_counter <= 0) {
        fprintf(stderr, "Rejected file output, cannot write with empty program.\n");

        return 0;
    }
    
    uint32_t magic_constant = LANG_SIGNATURE;
    uint16_t version_major = LANG_MAJOR_VER;
    uint16_t version_minor = LANG_MINOR_VER;
    uint16_t language_begin = LANGUAGE_BEGIN;
    uint32_t program_size = (uint32_t) program->byte_counter;
    uint32_t constant_count = (uint32_t) program->constant_counter;

    size_t filename_len = strlen(output) + 4 + 1;
    char* output_file = malloc(filename_len);
    if (output_file == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    snprintf(output_file, filename_len, "%s.nxo", output);

    FILE *file = fopen(output_file, "wb");
    if (file == NULL) {
        perror("Failed to create bytecode file\n");
        return 0;
    }

    fwrite(&magic_constant, sizeof(magic_constant), 1, file);
    fwrite(&version_major, sizeof(version_major), 1, file);
    fwrite(&version_minor, sizeof(version_minor), 1, file);
    fwrite(&program_size, sizeof(program_size), 1, file);
    fwrite(&constant_count, sizeof(constant_count), 1, file);

    for (int i = 0; i < program->constant_counter; i++) {
        Constant constant_saved = program->constants[i];
        fwrite(&constant_saved.type, sizeof(uint8_t), 1, file);
        
        if (constant_saved.type == N_CONST_STRING) {
            uint32_t length = (uint32_t) strlen(constant_saved.as.string);
            fwrite(&length, sizeof(length), 1, file);
            fwrite(constant_saved.as.string, sizeof(char), length, file);            
        }
    }

    fwrite(&language_begin, sizeof(language_begin), 1, file);
    fwrite(program->bytes, sizeof(uint8_t), program->byte_counter, file);
    fclose(file);
    printf("\033[1;33m[Nexen]\033[0m File output succesfully created at: \033[0;34m~\\%s\033[0m\n", output_file);

    return 1;
}

Program* load_program_from_binary(const char* file_contents, size_t file_size) {
    int curptr = 0;

    uint32_t magic_constant = *(uint32_t*)&file_contents[curptr];
    curptr += sizeof(uint32_t);
    
    if (magic_constant != LANG_SIGNATURE) {
        fprintf(stderr, "Invalid file signature.\n");
        return NULL;
    }

    uint16_t version_major = *(uint16_t*)&file_contents[curptr]; curptr += sizeof(uint16_t);
    uint16_t version_minor = *(uint16_t*)&file_contents[curptr]; curptr += sizeof(uint16_t);

    uint32_t program_size = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);
    uint32_t constant_count = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);

    Program* program = malloc(sizeof(Program));
    program->byte_counter = program_size;
    program->byte_limit = program_size;
    program->constant_counter = constant_count;
    program->constant_limit = constant_count;

    program->bytes = malloc(program_size);
    if (constant_count > 0) {
        program->constants = malloc(sizeof(Constant) * constant_count);
    } else {
        program->constants = NULL;
    }

    for (uint32_t i = 0; i < constant_count; i++) {
        uint8_t type = file_contents[curptr++];
        program->constants[i].type = type;

        if (type == N_CONST_STRING) {
            uint32_t length = *(uint32_t*)&file_contents[curptr];
            curptr += sizeof(uint32_t);

            program->constants[i].as.string = malloc(length + 1);
            memcpy(program->constants[i].as.string, &file_contents[curptr], length);
            program->constants[i].as.string[length] = '\0';
            
            curptr += length;
        }
    }

    uint16_t language_begin = *(uint16_t*)&file_contents[curptr];
    curptr += sizeof(uint16_t);

    if (language_begin != LANGUAGE_BEGIN) {
        fprintf(stderr, "Sync error: Program begin marker 0xFFFF not found.\n");
        return NULL;
    }

    memcpy(program->bytes, &file_contents[curptr], program_size);

    return program;
}

void print_program_bytecode(const char* compiled_input) {
    size_t file_size;
    const char* file_contents = get_file_contents(compiled_input, &file_size);
    Program* decomp_program = load_program_from_binary(file_contents, file_size);
    print_compiled_program(decomp_program);
}

int compile_program(const char* file_name, const char* output) {
    ParsedProgram* program_expressions = parse_file_expressions(file_name);
    if (program_expressions == NULL) {
        fprintf(stderr, "Could not parse program expressions, compilation terminated.\n");

        return 1;
    }

    Program* program_result = init_program();
    for (int expr_idx = 0; expr_idx < program_expressions->expression_count; expr_idx++) {
        Expression* expr = program_expressions->expressions[expr_idx];
        compile_expr(program_result, expr);
        free(expr);
    }

    free_tokens(program_expressions->tokens, program_expressions->token_count);
    free(program_expressions);
    int success = write_to_file(output, program_result);
    if (success == 0) {
        printf("Could not write to output %s\n", output);
        return 1;
    }

    print_compiled_program(program_result);

    return 0;
};