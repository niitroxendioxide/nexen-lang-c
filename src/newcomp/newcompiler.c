#include "newcomp/newcompiler.h"
#include "newcomp/debugger.h"
#include <string.h>

static SymbolTable global_functions = {
    .count = 0,
    .symbols = {},
    .parent = NULL,
};

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

void allocate_if_functions_full(Program* program) {
    if (program->func_count < program->func_limit) {
        return;
    }

    if (program->func_count >= program->func_limit) {
        program->func_limit = program->func_limit == 0 ? 10 : program->func_limit * 2;
    }

    Function** temp_reallocated = realloc(program->functions, sizeof(Function) * program->func_limit);
    if (temp_reallocated == NULL) {
        fprintf(stderr, "Function* realloc failed.\n");
        exit(1);
    }

    program->functions = temp_reallocated;
}

/* symbol indexing */
int get_symbol_from_table(SymbolTable* table, const char* symbol) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, symbol) == 0) {
            return table->symbols[i].unique_index;
        }
    }

    debug_print("calling onto parent now");

    if (table->parent != NULL) {
        return get_symbol_from_table(table->parent, symbol);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Symbol %s not defined", symbol);
    debug_printerr(buf);
    exit(1);
}

int get_function_index(Program* program, const char* func_name) {
    debug_print_formatted("Program has: %d functions", program->func_count);
    for (int i = 0; i < program->func_count; i++) {
        debug_print_formatted("> %s == %s?", program->functions[i]->name, func_name);
        if (strcmp(program->functions[i]->name, func_name) == 0) {
            return i;
        }
    }

    if (program->enclosing != NULL) {
        return get_function_index(program->enclosing, func_name);
    }

    return -1;
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

int get_total_active_registers(Program* program) {
    int total = 0;
    SymbolTable* current = program->symbol_table;
    while (current != NULL) {
        total += current->count;
        current = current->parent;
    }
    return total;
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

void emit_dword(Program* program, int dword) {
    emit_word(program, dword & 0xFFFF);
    emit_word(program, (dword >> 16) & 0xFFFF);
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
    new_program->enclosing = NULL;
    new_program->constants = malloc(new_program->constant_limit * sizeof(Constant));
    new_program->bytes = malloc(new_program->byte_limit * sizeof(uint8_t));
    new_program->symbol_table = malloc(sizeof(SymbolTable));
    new_program->symbol_table->count = 0;
    new_program->symbol_table->parent = NULL;
    new_program->func_count = 0;
    new_program->func_limit = 10;
    new_program->functions = malloc(sizeof(Function) * new_program->func_limit);
 
    if (new_program->constants == NULL || new_program->bytes == NULL || new_program->symbol_table == NULL || new_program->functions == NULL) {
        fprintf(stderr, "Could not allocate enough memory for the current compiled program\n");
        exit(1);
    }

    return new_program;
}

Program* enclose_program(Program* current, const char* fn_name) {
    allocate_if_functions_full(current);
    
    Function* new_func = malloc(sizeof(Function));
    if (new_func == NULL) {
        fprintf(stderr, "Couldn't store function, reason: malloc failed\n");
        exit(1);
    }

    new_func->name = fn_name;
    current->functions[current->func_count] = new_func;
    current->func_count++;

    // 
    Program* new_program = init_program();
    new_program->enclosing = current;

    return new_program;
}

Program* write_to_functions(Program* current, int argc, int fn_idx) {
    Program* parent = current->enclosing;
    if (parent == NULL) {
        fprintf(stderr, "Cannot write to NULL program (currently in topmost program).\n");
        exit(1);
    }

    
    Function* new_func = parent->functions[fn_idx];
    debug_print_formatted("wrote to function: %s. with len: %d", new_func->name, current->byte_counter);
    
    uint8_t reg_count = current->symbol_table->count;
    //new_func->name = func_name;

    for (int i = 0; i < current->constant_counter; i++) {
        Constant constant = current->constants[i];
        push_constant(parent, constant);
    }
    
    new_func->arg_count = argc;
    new_func->length = current->byte_counter;
    new_func->reg_count = reg_count;
    new_func->bytes = realloc(current->bytes, sizeof(uint8_t) * current->byte_counter);
    if (new_func->bytes == NULL) {
        exit(1);
    }

    // parent->functions[parent->func_count] = new_func;
    // parent->func_count++;
    
    return parent;
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

int get_str_constant(Program* program, const char* constant_value) {
    for (int i = 0; i < program->constant_counter; i++) {
        Constant* val = &program->constants[i];
        if (strcmp(val->as.string, constant_value) == 0) {
            return i;
        }
    }

    return -1;
}

ExprValueType compile_expr(Program* program, Expression* expr, int reg_used) {
    if (expr == NULL) return 0;

    switch (expr->type) {
        case EXPR_BLOCK: {
            //emit_byte(program, OP_PUSH_SCOPE);
            push_scope(program);
            for (int i = 0; i < expr->data.block.count; i++) {
                Expression* cur_block_expr = expr->data.block.statements[i];
                compile_expr(program, cur_block_expr, -1);
            }
            pop_scope(program);
            //emit_byte(program, OP_POP_SCOPE);

            break;
        }

        case EXPR_IF: {
            debug_print("Enclosing if block");
            int reg_base = get_total_active_registers(program);
            Expression* compared = expr->data.conditional.condition;
            compile_expr(program, compared, reg_base);

            emit_byte(program, OP_JUMP_IF_FALSE);
            emit_byte(program, reg_base);
            int pre_then_branch_counter = program->byte_counter;
            emit_word(program, 0xFFFF);

            compile_expr(program, expr->data.conditional.branch_then, -1);
            
            emit_byte(program, OP_JUMP);
            int post_then_branch_counter = program->byte_counter;
            emit_word(program, 0xFFFF);

            uint16_t relative_jump = (uint16_t) (post_then_branch_counter - pre_then_branch_counter);
            if (relative_jump > 0xFFFF) {
                fprintf(stderr, "Block too big\n");
                exit(1);
            }

            override_word(program, relative_jump, pre_then_branch_counter);
            compile_expr(program, expr->data.conditional.branch_else, -1);
            int post_else_branch_counter = program->byte_counter;
            int finished_relative_jump = (post_else_branch_counter) - (post_then_branch_counter + 2);
            override_word(program, finished_relative_jump, post_then_branch_counter);
            debug_print("Finished if, then & else branch");

            break;
        }

        case EXPR_FUNCTION_DEF: {
            const char* fn_name = strdup(expr->data.function_def.name);
            program = enclose_program(program, fn_name);
            int program_fn_idx = program->enclosing->func_count - 1;

            int param_count = expr->data.function_def.param_count;
            char** param_names = expr->data.function_def.param_names;
            for (int i = 0; i < param_count; i++) {
                const char* param_name = param_names[i];
                push_symbol(program, param_name);
            }

            Expression* f_body = expr->data.function_def.body;
            for (int i_e = 0; i_e < f_body->data.block.count; i_e++) {
                Expression* body_expr = f_body->data.block.statements[i_e];
                compile_expr(program, body_expr, -1);
            }

            program = write_to_functions(program, param_count, program_fn_idx);
    
            break;
        }

        case EXPR_RETURN: {
            Expression* returned = expr->data.return_value;
            int dest_reg = get_total_active_registers(program);

            compile_expr(program, returned, dest_reg);
            emit_byte(program, OP_RETURN);
            emit_byte(program, dest_reg);
            break;
        }

        case EXPR_FN_CALL: {
            Expression* calle = expr->data.call.callee;
            int is_method = calle->type == EXPR_INDEX && calle->data.index_expr.is_method_call;

            if (!is_method) {
                const char* call_name = calle->data.name;
                debug_print_formatted("Calling function: %s", call_name);

                int base_register = reg_used;
                if (base_register == -1) {
                    base_register = get_total_active_registers(program);
                }

                int is_global = -1;
                int func_index = get_function_index(program, call_name);
                debug_print_formatted("function index: %d", func_index);
                if (func_index == -1) {
                    is_global = get_symbol_from_table(&global_functions, call_name);
                }

                debug_print_formatted("compiling argument pushing. starting at reg: %d", reg_used);
                int argc = expr->data.call.argument_count;
                for (int i = 0; i < argc; i++) {
                    Expression* arg = expr->data.call.arguments[i];
                    compile_expr(program, arg, base_register + i);
                }

                if (is_global != -1) {
                    emit_byte(program, OP_CALL_NATIVE);
                    emit_byte(program, (uint8_t) base_register);
                    emit_byte(program, is_global);
                    emit_byte(program, argc);
                } else {
                    emit_byte(program, OP_CALL_FN);
                    emit_byte(program, (uint8_t) base_register);
                    emit_dword(program, func_index);
                }

                debug_print("function call written.");
            }
            break;
        };

        case EXPR_BINARY_OPERATOR: {
            Expression* left = expr->data.operation.left;
            Expression* right = expr->data.operation.right;

            compile_expr(program, left, reg_used + 1);
            compile_expr(program, right, reg_used + 2);
            
            OpCode operation = opcode_for_op(expr->data.operation.op);
            if (operation == OP_VOID) {
                fprintf(stderr, "Operation [%s] not implemented\n", expr->data.operation.op);
                exit(1);
            }
            
            emit_byte(program, operation);
            emit_byte(program, (uint8_t) reg_used);
            emit_byte(program, (uint8_t) reg_used + 1);
            emit_byte(program, (uint8_t) reg_used + 2);

            break;
        }

        case EXPR_NUMBER: {
            double num_value = expr->data.value;

            int is_int = (floor(num_value) == num_value);
            if (abs(num_value) < 255 && is_int) {
                emit_byte(program, OP_PUSH_U8);
                emit_byte(program, (uint8_t) reg_used);
                emit_byte(program, (uint8_t) num_value);
            } else if (abs(num_value) < MAX_U16 && is_int) {
                emit_byte(program, OP_PUSH_U16);
                emit_byte(program, (uint8_t) reg_used);
                emit_word(program, (uint16_t) num_value);
            } else {
                uint64_t raw_bits;
                memcpy(&raw_bits, &num_value, sizeof(double));

                emit_byte(program, OP_PUSH_NUM);
                emit_byte(program, (uint8_t) reg_used);
                emit_qword(program, raw_bits);
            }

            return EXPR_VAL_TYPE_NUMBER;
        }

        case EXPR_STRING: {
            int const_pointer = get_str_constant(program, expr->data.name);
            if (const_pointer == -1) {
                Constant new_constant = {
                    .type = N_CONST_STRING,
                    .as.string = expr->data.name,
                };

                const_pointer = push_constant(program, new_constant);
                debug_print_formatted("Pushed String \033[1;34m[\"%s\"]\033[0m to the constant pool \033[1;35m[idx=%d]\033[0m", expr->data.name, const_pointer);
            }

            emit_byte(program, OP_LOAD_CONST);
            emit_byte(program, (uint8_t) reg_used);
            emit_byte(program, (uint8_t) const_pointer);

            return EXPR_VAL_TYPE_STRING;
        }

        case EXPR_BOOL: {
            if (expr->data.bool_val == 0) {
                emit_byte(program, OP_PUSH_0);
            } else {
                emit_byte(program, OP_PUSH_1);
            }
            emit_byte(program, (uint8_t) reg_used);

            return EXPR_VAL_TYPE_BOOLEAN;
        }

        /*case EXPR_DICT: {
            int count = expr->data.dict.count;
            for (int i = 0; i<count; i++) {

            }
            break;
        }*/

        case EXPR_ARRAY: { 
            int count = expr->data.array.count;

            for (int i = 0; i < count; i++) {
                Expression* statement = expr->data.array.elements[i];
                if (i < count - 1) {
                    Expression* next = expr->data.array.elements[i + 1];
                    if (next->type != statement->type && statement->type != EXPR_NAME && next->type != EXPR_NAME) {
                        const char* type1 = expr_type_to_str(statement);
                        const char* type2 = expr_type_to_str(next);
                        const char str[] = "Compilation aborted, reason:\n\033[1;31m[Compile Error]:\033[0m Array types don't match.\n> [%d]: %s\n> [%d]: %s\n";
                        char buf[256];
                        snprintf(buf, sizeof(buf), str, i, type1, i+1, type2);

                        debug_printerr(buf);
                        exit(1);
                    }
                }

                compile_expr(program, statement, reg_used + i);
            }
            
            emit_byte(program, OP_PUSH_ARRAY);
            emit_byte(program, (uint8_t) reg_used);
            emit_dword(program, count);

            break;
        }

        case EXPR_NAME: {
            uint16_t symbol_index = get_symbol_index(program, expr->data.name);
            emit_byte(program, OP_LOAD_LOCAL);
            emit_byte(program, (uint8_t) reg_used);
            emit_byte(program, (uint8_t) symbol_index);

            break;
        }

        case EXPR_DEFINE: {
            Expression* def_body = expr->data.define_body;
            Expression* assign_name = def_body->data.assign.name;
            uint16_t symbol_index = push_symbol(program, assign_name->data.name);
            compile_expr(program, def_body, (int) symbol_index);

            break;
        }

        case EXPR_ASSIGN: {
            Expression* assign_value = expr->data.assign.value;
            const char* assign_name = expr->data.assign.name->data.name;
            uint16_t stored_symbol_index = get_symbol_index(program, assign_name);
            compile_expr(program, assign_value, (int) stored_symbol_index);
            
            //debug_print_formatted("Expression:");
            // display_expression(assign_value);
            /*if (assign_value->type != EXPR_NUMBER && assign_value->type != EXPR_BOOL && assign_value->type != EXPR_ARRAY ) {
                emit_byte(program, OP_STORE_LOCAL);
                emit_byte(program, (uint8_t) stored_symbol_index);
            }*/

            break;
        }

        default: {
            debug_printerr("Compilation aborted, reason:\n\033[1;31m[Compile Error]:\033[0m Unsupported expression:");
            display_expression(expr);
            fflush(stderr);
            exit(1);

            break;
        }
    }
}

int write_to_file(const char* output, Program* program) {
    if (program->byte_counter <= 0 && program->func_count <= 0) {
        fprintf(stderr, "Rejected file output, cannot write with empty program.\n");

        return 0;
    }
    
    uint32_t magic_constant = LANG_SIGNATURE;
    uint16_t version_major = LANG_MAJOR_VER;
    uint16_t version_minor = LANG_MINOR_VER;
    uint16_t version_patch = LANG_PATCH_VER;
    uint16_t language_begin = LANGUAGE_BEGIN;
    uint32_t program_size = (uint32_t) program->byte_counter;
    uint32_t constant_count = (uint32_t) program->constant_counter;
    uint32_t function_count = (uint32_t) program->func_count;
    uint8_t registers_used = (uint8_t) program->symbol_table->count;

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
    fwrite(&version_patch, sizeof(version_patch), 1, file);
    fwrite(&program_size, sizeof(program_size), 1, file);
    fwrite(&constant_count, sizeof(constant_count), 1, file);
    fwrite(&function_count, sizeof(function_count), 1, file);
    fwrite(&registers_used, sizeof(registers_used), 1, file);

    for (int i = 0; i < program->constant_counter; i++) {
        Constant constant_saved = program->constants[i];
        fwrite(&constant_saved.type, sizeof(uint8_t), 1, file);
        
        if (constant_saved.type == N_CONST_STRING) {
            uint32_t length = (uint32_t) strlen(constant_saved.as.string);
            fwrite(&length, sizeof(length), 1, file);
            fwrite(constant_saved.as.string, sizeof(char), length, file);            
        }
    }

    for (int i = 0; i < function_count; i++) {
        Function* func_saved = program->functions[i];
        uint8_t tag = N_CONST_FUNCTION;
        fwrite(&tag, sizeof(uint8_t), 1, file);
        fwrite(&func_saved->length, sizeof(int), 1, file);
        fwrite(&func_saved->arg_count, sizeof(uint8_t), 1, file);
        fwrite(&func_saved->reg_count, sizeof(uint8_t), 1, file);
        fwrite(func_saved->bytes, sizeof(uint8_t), func_saved->length, file);
    }

    fwrite(&language_begin, sizeof(language_begin), 1, file);
    fwrite(program->bytes, sizeof(uint8_t), program->byte_counter, file);
    fclose(file);
    printf("\033[1;33m[NexenC]\033[0m File output succesfully created at: \033[0;34m~\\%s\033[0m\n", output_file);

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

    uint16_t version_major = *(uint16_t*)&file_contents[curptr];  curptr += sizeof(uint16_t);
    uint16_t version_minor = *(uint16_t*)&file_contents[curptr];  curptr += sizeof(uint16_t);
    uint16_t version_patch = *(uint16_t*)&file_contents[curptr];  curptr += sizeof(uint16_t);

    uint32_t program_size   = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);
    uint32_t constant_count = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);
    uint32_t function_count = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);

    uint8_t registers_used = *(uint8_t*)&file_contents[curptr];   curptr += sizeof(uint8_t);

    Program* program = malloc(sizeof(Program));
    program->byte_counter = program_size;
    program->byte_limit = program_size;
    program->constant_counter = constant_count;
    program->constant_limit = constant_count;

    fprintf(stderr, "data: v%d.%d.%d, s: %dB, c_count: %d, fn_count: %d, reg_count: %d, at idx: %d\n", 
        version_major, version_minor, version_patch, program_size, constant_count, function_count, registers_used, curptr);

    program->bytes = malloc(program_size);
    if (constant_count > 0) {
        program->constants = malloc(sizeof(Constant) * constant_count);
    } else {
        program->constants = NULL;
    }

    if (function_count > 0) {
        program->functions = malloc(sizeof(Function) * function_count);
    } else {
        program->functions = NULL;
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

    for (uint32_t i = 0; i < function_count; i++) {
        uint8_t type = file_contents[curptr++];

        Function* new_fn = malloc(sizeof(Function));
        if (new_fn == NULL) {
            fprintf(stderr, "cannot allocate fn locally.\n");
            exit(1);
        }
        //program->functions[i].type = type;

        if (type == N_CONST_FUNCTION) {
            program->functions[i] = new_fn;
            uint32_t length = *(uint32_t*)&file_contents[curptr]; curptr += sizeof(uint32_t);
            uint8_t arg_c = *(uint8_t*)&file_contents[curptr]; curptr += sizeof(uint8_t);
            uint8_t reg_count = *(uint8_t*)&file_contents[curptr]; curptr += sizeof(uint8_t);

            program->functions[i]->bytes = malloc(length);
            memcpy(program->functions[i]->bytes, &file_contents[curptr], length);
            
            curptr += length;
        } else {
            fprintf(stderr, "Malformed program function\n.");
            exit(1);
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

void load_natives() {
    global_functions.symbols[0] = DEF_NATIVE_FN("print", 0);
    global_functions.symbols[1] = DEF_NATIVE_FN("len", 1);

    global_functions.count = 2;
}

int compile_program(const char* file_name, const char* output, int see_bytecode) {
    ParsedProgram* program_expressions = parse_file_expressions(file_name);
    if (program_expressions == NULL) {
        fprintf(stderr, "Could not parse program expressions, compilation terminated.\n");

        return 1;
    }

    load_natives();
    Program* program_result = init_program();
    for (int expr_idx = 0; expr_idx < program_expressions->expression_count; expr_idx++) {
        Expression* expr = program_expressions->expressions[expr_idx];
        compile_expr(program_result, expr, -1);
        free(expr);
    }

    free_tokens(program_expressions->tokens, program_expressions->token_count);
    free(program_expressions);

    if (see_bytecode == 1) {
        print_compiled_program(program_result);
    }

    int success = write_to_file(output, program_result);
    if (success == 0) {
        printf("Could not write to output %s\n", output);
        return 1;
    }


    return 0;
};