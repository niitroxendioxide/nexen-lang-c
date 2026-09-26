#include "newcomp/newcompiler.h"
#include "newcomp/debugger.h"
#include <string.h>

static SymbolTable native_symbols = {
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
    for (int i = table->count - 1; i >= 0; --i) {
        // printf("at %d\n", i);
        if (strcmp(table->symbols[i].name, symbol) == 0) {
            return table->symbols[i].unique_index;
        }
    }

    debug_print("calling onto parent now");

    if (table->parent != NULL) {
        return get_symbol_from_table(table->parent, symbol);
    }

    return -1;
}

int get_function_index(Program* program, const char* func_name) {
    //debug_print_formatted("Program has: %d functions", program->func_count);
    for (int i = 0; i < program->func_count; i++) {
        // debug_print_formatted("> %s == %s?", program->functions[i]->name, func_name);
        if (strcmp(program->functions[i]->name, func_name) == 0) {
            return i;
        }
    }

    if (program->enclosing != NULL) {
        return get_function_index(program->enclosing, func_name);
    }

    return -1;
}

Symbol* find_symbol(SymbolTable* table, const char* symbol) {
    for (int i = table->count - 1; i >= 0; --i) {
        // printf("at %d\n", i);
        if (strcmp(table->symbols[i].name, symbol) == 0) {
            return &table->symbols[i];
        }
    }

    if (table->parent != NULL) {
        return find_symbol(table->parent, symbol);
    }

    if (table != &native_symbols) {
        return find_symbol(&native_symbols, symbol);
    }

    return NULL;
}

Symbol get_symbol(SymbolTable* table, const char* symbol) {
    Symbol* found = find_symbol(table, symbol);
    if (found == NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Symbol %s not defined", symbol);
        debug_printerr(buf);
        exit(1);
    }

    return *found;
}

Symbol get_symbol_or_glob(Program* program, const char* symbol) {
    Symbol* found = find_symbol(program->symbol_table, symbol);
    if (found == NULL) {
        Program* cur = program;
        while (cur->enclosing != NULL) cur = cur->enclosing;

        found = find_symbol(cur->globals, symbol);

        if (found == NULL) {
            char buf[128];
            snprintf(buf, sizeof(buf), "GLOB Symbol %s not defined", symbol);
            debug_printerr(buf);
            exit(1);
        }
    }

    return *found;
}

int get_symbol_index(Program* program, const char* symbol) {
    int res = get_symbol_from_table(program->symbol_table, symbol);
    if (res == -1) {
        res = get_symbol_from_table(program->globals, symbol);
        if (res == -1) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Symbol %s not defined", symbol);
            debug_printerr(buf);
            exit(1);
        }
    }

    return res;
}

int get_total_active_registers(Program* program) {
    int total = 0;
    SymbolTable* current = program->symbol_table;
    while (current != NULL) {
        total += current->count;
        current = current->parent;
    }
    return total + program->reserved_registers; 
}

Symbol* push_symbol_entry(Program* program, const char* symbol, int global) {
    SymbolTable* table = program->symbol_table;
    if (global == 1) {
        table = program->globals;
    }

    if (table->count >= MAX_SYMBOL_COUNT) {
        fprintf(stderr, "Program exceeded the maximum amount of symbols\n");
        exit(1);
    }

    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, symbol) == 0) {
            fprintf(stderr, "Redefining existing symbol %s\n", symbol);
            exit(1);
        }
    }

    
    Symbol new_symbol = {
        .name = symbol,
        .index = table->count,
        .unique_index = program->index_counter,
        .global = (uint8_t) global,
        .value = Comp_NilVal,
    };
    
    int slot = table->count;
    table->symbols[slot] = new_symbol;
    table->count++;
    program->index_counter++;
    
    return &table->symbols[slot];
}

int push_symbol(Program* program, const char* symbol) {
    return push_symbol_entry(program, symbol, 0)->unique_index;
}

int push_global(Program* program, const char* symbol) {
    return push_symbol_entry(program, symbol, 1)->unique_index;
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

void override_dword(Program* program, int word, int pointer) {
    override_word(program, word & 0xFFFF, pointer);
    override_word(program, (word >> 16) & 0xFFFF, pointer + 2);
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

void free_registers(Program* program, int amount) {
    int res_registers = program->reserved_registers;
    
    if (res_registers < amount) {
        program->reserved_registers = 0;
        program->index_counter -= res_registers;
    } else {
        program->reserved_registers -= amount;
        program->index_counter -= amount;
    }
}

void reserve_registers(Program* program, int amount) {
    program->reserved_registers += amount;
    program->index_counter += amount;
}


/* initializing program & scopes */
Program* init_program(const char* source_file) {
    Program* new_program = malloc(sizeof(Program));
    new_program->constant_counter = 0;
    new_program->byte_counter = 0;
    new_program->constant_limit = 10;
    new_program->byte_limit = 10;
    new_program->module_count = 0;
    new_program->modules_capacity = 10;
    new_program->index_counter = 0;
    new_program->enclosing = NULL;
    new_program->is_main = 1;
    new_program->reserved_registers = 0;
    new_program->constants = malloc(new_program->constant_limit * sizeof(Constant));
    new_program->bytes = malloc(new_program->byte_limit * sizeof(uint8_t));
    new_program->symbol_table = malloc(sizeof(SymbolTable));
    new_program->symbol_table->count = 0;
    new_program->symbol_table->parent = NULL;
    new_program->globals = malloc(sizeof(SymbolTable));
    new_program->globals->count = 0;
    new_program->globals->parent = NULL;
    new_program->func_count = 0;
    new_program->file_source = strdup(source_file);
    new_program->func_limit = 10;
    
    new_program->modules = malloc(sizeof(CompiledModule) * new_program->modules_capacity);
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
    Program* new_program = init_program(current->file_source);
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
    // debug_print_formatted("wrote to function: %s. with len: %d", new_func->name, current->byte_counter);
    
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
        if (val->type == N_CONST_STRING && strcmp(val->as.string, constant_value) == 0) {
            return i;
        }
    }

    return -1;
}

SymbolValue compile_expr(Program* program, Expression* expr, int reg_used) {
    if (expr == NULL) return Comp_NilVal;

    switch (expr->type) {
        case EXPR_EXPORT: {
            if (program->enclosing != NULL) {
                err_print_format("Compile time error.\n\033[1;31m> [Not Allowed]\033[0m: Cannot export from non-global scope");
                exit(1);
            }
            expr->data.export->is_exporting = 1;

            compile_expr(program, expr->data.export, reg_used);

            //debug_print_formatted("Exported of type: %d\n", (int) val.type);
            // int exported_register = get_total_active_registers(program);

            break;
        }
        case EXPR_IMPORT: {
            const char* var_name = expr->data.define_body->data.assign.name->data.name;
            Expression* val = expr->data.define_body->data.assign.value;

            if (val->type != EXPR_STRING) {
                err_print_format("Compile time error raised\n\033[1;31m> [Not Allowed]\033[0m: Dynamic module loading not allowed.");
                exit(1);
            }

            int mod_loaded_reg = reg_used > 0 ? reg_used : get_total_active_registers(program);
            const char* file_name_src = import_into_glob(program->file_source, val->data.name);
            ParsedProgram* parsed_program = parse_file_expressions(file_name_src);

            int is_broken = 0;
            for (int i = 0; i < program->module_count; i++) {
                CompiledModule* existing_module = program->modules[i];
                //debug_print_formatted("Already existing module %s is at index: %d", existing_module->module_path, i);
                if (strcmp(existing_module->module_path, file_name_src) == 0) {
                    emit_byte(program, OP_LOAD_MOD);
                    emit_byte(program, mod_loaded_reg);
                    emit_dword(program, i);
                    is_broken = 1;
                    break;
                }
            }
            if (is_broken) break;

            CompiledModule* comp_module = malloc(sizeof(CompiledModule));
            if (comp_module == NULL) {
                err_print_format("No more memory.");
                exit(1);
            };

            if (program->module_count + 1 >= program->modules_capacity) {
                program->modules_capacity *= 2;
                program->modules = realloc(program->modules, sizeof(CompiledModule) * program->modules_capacity);
                if (program->modules == NULL) {
                    err_print_format("No more memory.");
                    exit(1);
                }
            }

            comp_module->module_path = file_name_src;
            comp_module->state = MOD_COMPILING;
            
            Program* new_module = init_program(file_name_src);
            new_module->is_main = 0;

            //debug_print_formatted("Compiling module: %s as %s", file_name_src, var_name);
            for (int i = 0; i < parsed_program->expression_count; i++) {
                Expression* expr = parsed_program->expressions[i];
                compile_expr(new_module, expr, -1);
                free(expr);
            }

            // print_compiled_program(new_module);

            // debug_print_formatted("Program has %d symbols", new_module->globals->count);
            SymbolTable* exports = &comp_module->exports;
            for (int i = 0; i < new_module->globals->count; i++) {
                Symbol symbol = new_module->globals->symbols[i];
                // printf("symbol exported?: %s\n", symbol.value.is_exported == 1 ? "yes" : "no");
                if (symbol.value.is_exported) {
                    int idx_correct = exports->count;
                    // symbol.index = idx_correct;
                    symbol.unique_index = idx_correct;

                    exports->symbols[idx_correct] = symbol;
                    exports->count++;
                }
            }

            for (int i = 0; i < exports->count; i++) {
                Symbol exported_symbol = exports->symbols[i];
                /*debug_print_formatted(
                    "exported symbol \033[1;32m\"%s\"\033[0m from module \033[1;35m\"%s\"\033[0m", 
                    exported_symbol.name, 
                    file_name_src
                );*/
            }

            for (int i = 0; i < new_module->constant_counter; i++) {
                Constant val = new_module->constants[i];
                push_constant(program, val);
            }

            free(new_module->constants); // should work fine, i think

            for (int i = 0; i < new_module->module_count; i++) {
                int mod_idx = program->module_count++;
                program->modules[mod_idx] = new_module->modules[i];
            }


            int mod_idx = program->module_count++;
            comp_module->module_program = new_module;
            program->modules[mod_idx] = comp_module;

            Symbol* module_symbol = push_symbol_entry(program, var_name, 0);
            module_symbol->value = (SymbolValue){ .type = EXPR_VAL_TYPE_MODULE, .value.module.symbols = exports };

            if (DEBUG_ACTIONS) {
                debug_print_formatted("Final program has %d module(s). Module [%s] code:", program->module_count, file_name_src);
                print_compiled_program(new_module);
            }

            emit_byte(program, OP_LOAD_MOD);
            emit_byte(program, mod_loaded_reg);
            emit_dword(program, mod_idx);

            break;
        }

        case EXPR_BLOCK: {
            //emit_byte(program, OP_PUSH_SCOPE);
            push_scope(program);
            for (int i = 0; i < expr->data.block.count; i++) {
                Expression* cur_block_expr = expr->data.block.statements[i];
                compile_expr(program, cur_block_expr, get_total_active_registers(program));
            }
            pop_scope(program);
            //emit_byte(program, OP_POP_SCOPE);

            return Comp_NilVal;
        }

        case EXPR_IF: {
            debug_print("Enclosing if block");
            int reg_base = get_total_active_registers(program);
            Expression* compared = expr->data.conditional.condition;
            compile_expr(program, compared, reg_base);

            emit_byte(program, OP_JUMP_IF_FALSE);
            emit_byte(program, reg_base);
            int jiffalse_counter = program->byte_counter;
            emit_dword(program, 0xFFFFFFFF);
            int pre_then_branch = program->byte_counter;

            compile_expr(program, expr->data.conditional.branch_then, -1);
            
            int skip_jump_ctr = -1;
            if (expr->data.conditional.branch_else != NULL) {
                emit_byte(program, OP_JUMP);
                skip_jump_ctr = program->byte_counter;
                emit_dword(program, 0xFFFFFFFF);
            }

            int post_then_branch = program->byte_counter;

            int relative_jump = (int) (post_then_branch - pre_then_branch);
            if (relative_jump > 0xFFFF) {
                fprintf(stderr, "Block too big\n");
                exit(1);
            }

            override_dword(program, relative_jump, jiffalse_counter);

            if (expr->data.conditional.branch_else != NULL) {
                debug_print("Finished compiling else branch");
                compile_expr(program, expr->data.conditional.branch_else, -1);
                int post_else_branch_counter = program->byte_counter;
                int finished_relative_jump = post_else_branch_counter - post_then_branch;
                
                override_dword(program, finished_relative_jump, skip_jump_ctr);
            }

            debug_print("Finished compiling if");

            return Comp_NilVal;
        }

        case EXPR_FUNCTION_DEF: {
            const char* fn_name = strdup(expr->data.function_def.name);
            // debug_print_formatted("function written! %s", fn_name);
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

            return (SymbolValue){.type = EXPR_VAL_TYPE_FUNCTION, .value.str_val = fn_name };
        }

        case EXPR_WHILE_LOOP: {
            Expression* body = expr->data.loop_while.body;
            Expression* condition = expr->data.loop_while.condition;

            int free_reg = get_total_active_registers(program);

            int loop_start = (int) program->byte_counter;
            compile_expr(program, condition, free_reg);

            emit_byte(program, OP_JUMP_IF_FALSE);
            emit_byte(program, (uint8_t) free_reg);
            int jump_dword_ptr = (int) program->byte_counter;
            emit_dword(program, 0x0);

            int jif_instruction = (int) program->byte_counter;
            compile_expr(program, body, -1);

            int jump_instruction = (int) program->byte_counter;
            emit_byte(program, OP_JUMP);
            emit_dword(program, loop_start - (jump_instruction + 5));

            override_dword(program, (int) program->byte_counter - jif_instruction, jump_dword_ptr);

            return Comp_NilVal;
        }

        case EXPR_FOR_LOOP: {
            Expression* variable = expr->data.loop_for.variable;
            Expression* body = expr->data.loop_for.body;
            Expression* looping = expr->data.loop_for.looping;

            const char* loop_idx_symbol = variable->data.define_body->data.assign.name->data.name;
            
            push_scope(program);
            if (looping->type == EXPR_NAME) {
                Symbol found = get_symbol_or_glob(program, looping->data.name);
                if (found.value.type != EXPR_VAL_TYPE_ARRAY) {
                    debug_printerr("Cannot loop through non-array value");
                    exit(1);
                }

                Expression* start = malloc(sizeof(Expression));
                start->type = EXPR_NUMBER;
                start->data.value = 0;

                Expression* end = malloc(sizeof(Expression));
                end->type = EXPR_NUMBER;
                end->data.value = found.value.value.array_val.count;

                Expression* len_array = malloc(sizeof(Expression));
                len_array->type = EXPR_RANGE;
                len_array->data.range.start = start;
                len_array->data.range.end = end;
                len_array->data.range.included = 0;

                int symbol_reg = get_total_active_registers(program);
                // display_expression(variable);
                compile_expr(program, variable, symbol_reg);

                //display_expression(len_array);

                int var_reg = get_total_active_registers(program);
                // printf("putting variable at: %d\n", var_reg);
                emit_byte(program, OP_PUSH_U8);
                emit_byte(program, var_reg);
                emit_byte(program, 0);

                reserve_registers(program, 1);

                int loop_reg = get_total_active_registers(program);
                compile_expr(program, len_array, loop_reg);

                int jump_reg = loop_reg + 1;
                // int symbol_reg = get_symbol_index(program, loop_idx_symbol);

                reserve_registers(program, 2);

                /* pasted */
                int condition_ptr = program->byte_counter;
                emit_byte(program, OP_LOAD_FIELD);
                emit_byte(program, jump_reg);
                emit_byte(program, loop_reg);
                emit_byte(program, (uint8_t) 1); 

                emit_byte(program, OP_LT);
                emit_byte(program, jump_reg);
                emit_byte(program, var_reg);
                emit_byte(program, jump_reg);

                emit_byte(program, OP_JUMP_IF_FALSE);
                emit_byte(program, jump_reg);

                int jump_dest_ptr = program->byte_counter;
                emit_dword(program, 0x0);
                int block_start = program->byte_counter;

                emit_byte(program, OP_LOAD_INDEX);
                emit_byte(program, symbol_reg);
                emit_byte(program, (uint8_t) found.unique_index);
                emit_byte(program, var_reg);

                compile_expr(program, body, jump_reg + 1);

                emit_byte(program, OP_PUSH_U8);
                int free_reg = get_total_active_registers(program);
                emit_byte(program, free_reg);
                emit_byte(program, (uint8_t) 1);

                emit_byte(program, OP_ADD);
                emit_byte(program, var_reg);
                emit_byte(program, var_reg);
                emit_byte(program, (uint8_t) free_reg);

                int body_ended_ptr = program->byte_counter;
                int body_size = (body_ended_ptr - block_start) + 5;
                override_dword(program, body_size, jump_dest_ptr);

                int diff = (condition_ptr - (body_ended_ptr + 5));
                emit_byte(program, OP_JUMP);
                emit_dword(program, diff);
                free_registers(program, 3);

                /**/
            } else if (looping->type == EXPR_RANGE) {
                compile_expr(program, variable, -1);

                int loop_reg = get_total_active_registers(program);
                

                compile_expr(program, looping, loop_reg);
                int jump_reg = loop_reg + 1;
                int symbol_reg = get_symbol_index(program, loop_idx_symbol);

                reserve_registers(program, 2);
                int condition_ptr = program->byte_counter;
                emit_byte(program, OP_LOAD_FIELD);
                emit_byte(program, jump_reg);
                emit_byte(program, loop_reg);
                emit_byte(program, (uint8_t) 1); 

                emit_byte(program, OP_LT);
                emit_byte(program, jump_reg);
                emit_byte(program, symbol_reg);
                emit_byte(program, jump_reg);

                emit_byte(program, OP_JUMP_IF_FALSE);
                emit_byte(program, jump_reg);

                int jump_dest_ptr = program->byte_counter;
                emit_dword(program, 0x0);
                int block_start = program->byte_counter;

                compile_expr(program, body, jump_reg + 1);

                emit_byte(program, OP_PUSH_U8);
                int free_reg = get_total_active_registers(program);
                emit_byte(program, free_reg);
                emit_byte(program, (uint8_t) 1);

                emit_byte(program, OP_ADD);
                emit_byte(program, symbol_reg);
                emit_byte(program, symbol_reg);
                emit_byte(program, (uint8_t) free_reg);

                int body_ended_ptr = program->byte_counter;
                int body_size = (body_ended_ptr - block_start) + 5;
                override_dword(program, body_size, jump_dest_ptr);

                int diff = (condition_ptr - (body_ended_ptr + 5));
                emit_byte(program, OP_JUMP);
                emit_dword(program, diff);
                free_registers(program, 2);
            }

            pop_scope(program);

            return Comp_NilVal;
        }

        case EXPR_RETURN: {
            Expression* returned = expr->data.return_value;
            int dest_reg = get_total_active_registers(program);

            compile_expr(program, returned, dest_reg);
            emit_byte(program, OP_RETURN);
            emit_byte(program, dest_reg);

            return Comp_NilVal;
        }

        case EXPR_FN_CALL: {
            Expression* calle = expr->data.call.callee;
            int is_method = calle->type == EXPR_INDEX && calle->data.index_expr.is_method_call;
            int is_mod = calle->type == EXPR_INDEX && calle->data.index_expr.is_mod_call;

            if (!is_method && !is_mod) {
                const char* call_name = calle->data.name;
                // debug_print_formatted("Calling function: %s", call_name);

                int base_register = reg_used;
                if (base_register == -1) {
                    base_register = get_total_active_registers(program);
                }

                int is_global = -1;
                int func_index = get_function_index(program, call_name);
                //debug_print_formatted("function index: %d", func_index);
                if (func_index == -1) {
                    Symbol* global_symbol = find_symbol(&native_symbols, call_name);
                    if (global_symbol != NULL && global_symbol->global == 1 && global_symbol->value.type == EXPR_VAL_TYPE_FUNCTION) {
                        is_global = global_symbol->unique_index;
                    } else {
                        const char str[] = "Compilation aborted, reason:\n\033[1;31m[Compile Error]\033[0m: Cannot call %s, symbol undefined.\n";
                        char buf[256];
                        snprintf(buf, sizeof(buf), str, call_name);
                        debug_printerr(buf);
                        exit(1);
                    }
                }

                //debug_print_formatted("compiling argument pushing. starting at reg: %d", reg_used);
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

                //debug_print("function call written.");
            } else if (is_mod) {
                //debug_print_formatted("Compiling mod fn call!");

                int calling_reg = get_total_active_registers(program);
                SymbolValue symbol_val = compile_expr(program, calle, calling_reg);
                if (symbol_val.type != EXPR_VAL_TYPE_FUNCTION) {
                    err_print_format(
                        "Attempted to call %s::%s as function.", 
                        calle->data.index_expr.target->data.name,
                        calle->data.index_expr.index->data.name
                    );
                    exit(1);
                }

                int argc = expr->data.call.argument_count;
                for (int i = 0; i < argc; i++) {
                    Expression* arg = expr->data.call.arguments[i];
                    compile_expr(program, arg, calling_reg + i + 1);
                }

                emit_byte(program, OP_CALL_REG);
                emit_byte(program, (uint8_t) reg_used);
                emit_byte(program, (uint8_t) calling_reg);
            }

            return Comp_NilVal;
        };

        case EXPR_BINARY_OPERATOR: {
            Expression* left = expr->data.operation.left;
            Expression* right = expr->data.operation.right;

            int next_free = get_total_active_registers(program);
            int l_reg = next_free;
            int r_reg = next_free + 1;
            uint8_t l_changed = 0;
            SymbolValue l_value;

            if (left->type == EXPR_NAME) {
                Symbol l_symbol = get_symbol_or_glob(program, left->data.name);
                if (l_symbol.global) {
                    compile_expr(program, left, l_reg);
                } else {
                    l_changed = 1;
                    l_reg = l_symbol.unique_index;
                    l_value = l_symbol.value;
                }
            } else {
                l_value = compile_expr(program, left, l_reg);
            }

            if (right->type == EXPR_NAME) {
                Symbol r_symbol = get_symbol_or_glob(program, right->data.name);
                if (r_symbol.global) {
                    compile_expr(program, right, r_reg);
                } else {
                    r_reg = r_symbol.unique_index;
                }
            } else {
                if (l_changed == 1) r_reg = next_free;
                compile_expr(program, right, r_reg);
            }

            OpCode operation = opcode_for_op(expr->data.operation.op);
            if (operation == OP_VOID) {
                fprintf(stderr, "Operation [%s] not implemented\n", expr->data.operation.op);
                exit(1);
            }

            // printf("Using register: %d\n", reg_used);
            
            emit_byte(program, operation);
            emit_byte(program, (uint8_t) reg_used);
            emit_byte(program, (uint8_t) l_reg);
            emit_byte(program, (uint8_t) r_reg);

            switch (operation) {
                case OP_EQ:
                case OP_NOTEQ:
                case OP_LT:
                case OP_GT:
                case OP_LEQT:
                case OP_GEQT:
                    return Comp_BoolVal(0);

                default:
                    return l_value;
            }
        }

        case EXPR_NUMBER: {
            double num_value = expr->data.value;
            // debug_print_formatted("Expression number: %f", num_value);

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

            return Comp_NumVal(num_value);
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

            return Comp_StrVal(expr->data.name);
        }

        case EXPR_BOOL: {
            if (expr->data.bool_val == 0) {
                emit_byte(program, OP_PUSH_0);
            } else {
                emit_byte(program, OP_PUSH_1);
            }
            emit_byte(program, (uint8_t) reg_used);

            return Comp_BoolVal(expr->data.bool_val);
        }

        /*case EXPR_DICT: {
            int count = expr->data.dict.count;
            for (int i = 0; i<count; i++) {

            }
            break;
        }*/

        case EXPR_RANGE: {
            int free_reg = get_total_active_registers(program);

            // setting up pre-values
            compile_expr(program, expr->data.range.start, free_reg);
            compile_expr(program, expr->data.range.end, free_reg + 1);

            if (expr->data.range.included == 1) {
                emit_byte(program, OP_PUSH_1);
            } else {
                emit_byte(program, OP_PUSH_0);
            }
            emit_byte(program, (uint8_t) free_reg + 2);

            // creating struct
            emit_byte(program, OP_NEW_STRUCT);
            emit_byte(program, (uint8_t) reg_used);
            emit_byte(program, (uint8_t) 3);

            return Comp_NilVal;
        }

        case EXPR_ARRAY: { 
            int count = expr->data.array.count;

            Expression** elements = expr->data.array.elements;
            ExprValueType current_type = EXPR_VAL_TYPE_NIL;

            int is_str_array = 0;
            if (elements[0]->type == EXPR_STRING) is_str_array = 1;
            else if (elements[0]->type == EXPR_NAME) {
                Symbol* defined_symbol = find_symbol(program->symbol_table, elements[0]->data.name);
                if (defined_symbol->value.type == EXPR_VAL_TYPE_STRING) is_str_array = 1;
            }

            if (count > 2 || is_str_array) {
                Constant** const_elements_array = malloc(sizeof(Constant) * count);
                Constant array_val = {
                    .as.array.elements = const_elements_array, 
                    .as.array.count = count,
                    .type = N_CONST_ARRAY,
                };

                if (const_elements_array == NULL) {
                    debug_printerr("Could not allocate array constant, malloc failed");
                    exit(1);
                };

                for (int i = 0; i < count; i++) {
                    switch (elements[i]->type) {
                        case EXPR_NAME: {
                            Symbol val = get_symbol(program->symbol_table, elements[i]->data.name);
                            //const_elements_array[i] = Register_Const(val.unique_index);
                            Constant* const_val = malloc(sizeof(Constant));
                            const_val->type = N_CONST_RUNTIME_REG;
                            const_val->as.const_ref = val.unique_index;
                            const_elements_array[i] = const_val;
                            break;
                        }
                        case EXPR_NUMBER: {
                            Constant* const_val = malloc(sizeof(Constant));
                            const_val->type = N_CONST_NUMBER;
                            const_val->as.number = elements[i]->data.value;
                            const_elements_array[i] = const_val; //Num_Const(elements[i]->data.value); //(Constant) {.type = N_CONST_NUMBER, .as.number =  };
                            break;
                        }
                        case EXPR_BOOL: {
                            Constant* const_val = malloc(sizeof(Constant));
                            const_val->type = N_CONST_BOOL;
                            const_val->as.boolean = elements[i]->data.bool_val;
                            const_elements_array[i] = const_val;
                            //const_elements_array[i] = Bool_Const(elements[i]->data.bool_val);
                            break;
                        }
                        case EXPR_STRING: {
                            int str_idx = get_str_constant(program, elements[i]->data.name);
                            if (str_idx == -1) {
                                Constant new_const = Str_Const(elements[i]->data.name);
                                str_idx = push_constant(program, new_const);
                            } 

                            Constant* const_val = malloc(sizeof(Constant));
                            const_val->type = N_CONST_STR_REF;
                            const_val->as.const_ref = str_idx;
                            const_elements_array[i] = const_val;//StrRef_Const(str_idx);
                            
                            break;
                        }
                    }
                }

                int arr_idx = push_constant(program, array_val);

                emit_byte(program, OP_LOAD_CONST);
                emit_byte(program, (uint8_t) reg_used);
                emit_byte(program, (uint8_t) arr_idx);

            } else {
                for (int i = 0; i < count; i++) {
                    Expression* statement = expr->data.array.elements[i];
                    SymbolValue new_type = compile_expr(program, statement, reg_used + i);
                    
                    if (current_type != EXPR_VAL_TYPE_NIL && (new_type.type != current_type)) {
                        const char* type1 = expr_val_to_str((int) new_type.type);
                        const char* type2 = expr_val_to_str((int) current_type);
                        const char str[] = "Compilation aborted, reason:\n\033[1;31m[Compile Error]\033[0m: Array types don't match.\n> [%d]: %s\n> [%d]: %s\n";
                        char buf[256];
                        snprintf(buf, sizeof(buf), str, i-1, type2, i, type1);
                        debug_printerr(buf);
                        exit(1);
                    }

                    current_type = new_type.type;
                }
                
                emit_byte(program, OP_PUSH_ARRAY);
                emit_byte(program, (uint8_t) reg_used);
                emit_dword(program, count);
            }

            return Comp_ArrayVal(current_type, count);
        }

        case EXPR_INDEX: {
            Expression* target = expr->data.index_expr.target;
            Expression* index = expr->data.index_expr.index;
            int is_mod_call = expr->data.index_expr.is_mod_call;

            if (is_mod_call) {
                const char* mod_name = target->data.name;
                const char* indexed_name = index->data.name;
                
                Symbol mod_symbol = get_symbol(program->symbol_table, mod_name);
                Symbol index = get_symbol(mod_symbol.value.value.module.symbols, indexed_name);
                // printf("accessing index: %d of module %s\n", (int) index.index, mod_name);

                emit_byte(program, OP_LOAD_FIELD);
                emit_byte(program, reg_used);
                emit_byte(program, (uint8_t) mod_symbol.unique_index);
                emit_byte(program, index.index);

                debug_print_formatted("Loading %s::%s", mod_name, indexed_name);
                //exit(1);

                return index.value;
            }

            Symbol target_symbol = get_symbol(program->symbol_table, target->data.name);
            if (target_symbol.value.type == EXPR_VAL_TYPE_ARRAY) {
                if (index->type == EXPR_NAME) {
                    int free_reg = get_total_active_registers(program);
                    compile_expr(program, index, free_reg);
                    
                    emit_byte(program, OP_LOAD_INDEX);
                    emit_byte(program, reg_used);
                    emit_byte(program, (uint8_t) target_symbol.unique_index);
                    emit_byte(program, (uint8_t) free_reg);
                } else {
                    int indexed_value = index->data.value;
                    emit_byte(program, OP_LOAD_FIELD);
                    emit_byte(program, reg_used);
                    emit_byte(program, (uint8_t) target_symbol.unique_index);
                    emit_byte(program, (uint8_t) indexed_value);
                }

                
                return (SymbolValue) { .type = target_symbol.value.value.array_val.arr_type, .value.is_nil = 0 };
            } else if (target_symbol.value.type == EXPR_VAL_TYPE_DICT) {
                return Comp_NilVal;
            }

            const char str[] = "Compilation aborted, reason:\n\033[1;31m[Compile Error]\033[0m: Cannot index into [%s], it is of type %s.\n";
            char buf[256];
            snprintf(buf, sizeof(buf), str, target->data.name, expr_val_to_str((int) target_symbol.value.type));
            debug_printerr(buf);
            exit(1);
        }

        case EXPR_NAME: {
            const char* var_name = expr->data.name;
            Symbol symbol = get_symbol_or_glob(program, var_name);
            /*if (symbol_ref == NULL)
            {
                symbol_ref = find_symbol(program->globals, var_name);
                if (symbol_ref == NULL) {
                    /*err_print_format("Compilation aborted.\033[1;31m> [Compile Error]\033[0m: Cannot find \"%s\"", var_name);
                    exit(1);
                }
            }*/
            

            if (symbol.global) {
                emit_byte(program, OP_LOAD_GLOB);
                emit_byte(program, (uint8_t) reg_used);
                emit_byte(program, (uint8_t) symbol.unique_index);
            } else {
                emit_byte(program, OP_LOAD_LOCAL);
                emit_byte(program, (uint8_t) reg_used);
                emit_byte(program, (uint8_t) symbol.unique_index);
            }

            // debug_print_formatted("name [%s] referenced, type %d\n", var_name, (int) symbol.value.type);

            return symbol.value;
        }

        case EXPR_DEFINE: {
            // display_expression(expr);
            Expression* def_body = expr->data.define_body;
            int is_global = program->enclosing == NULL && program->is_main == 0;
            debug_print_formatted("is global? %s", is_global == 1 ? "yes" : "no");
            if (def_body->type == EXPR_ASSIGN) {
                Expression* assign_name = def_body->data.assign.name;
                Symbol* entry = push_symbol_entry(program, assign_name->data.name, is_global);
                SymbolValue defined_value = compile_expr(program, def_body, entry->unique_index);
                defined_value.is_exported = expr->is_exporting;
                entry->value = defined_value;


                return defined_value;//
            } else if (def_body->type == EXPR_FUNCTION_DEF) {
                const char* fn_name = def_body->data.function_def.name;
                //debug_print_formatted("writing fn: %s", fn_name);
                Symbol* entry = push_symbol_entry(program, fn_name, is_global);
                //debug_print_formatted("symbol idx: %d, symbol name: %s", entry->unique_index, entry->name);

                
                SymbolValue defined_value = compile_expr(program, def_body, entry->unique_index);
                defined_value.is_exported = expr->is_exporting;
                // printf("func defined type: %d\n", (int) defined_value.type);

                int fnidx = 0;
                for (int i = 0; i < program->func_count; i++) {
                    if (i > 255 && is_global) {
                        err_print_format("Cannot export more than 255 expressions (functions included) per module.");
                        exit(1);
                    }

                    Function* fnval = program->functions[i];
                    if (strcmp(fnval->name, fn_name) == 0) {
                        fnidx = i;
                    }
                }

                entry->index = fnidx;
                entry->value = defined_value;
                return defined_value;
            }

            break;
        }

        case EXPR_ASSIGN: {
            Expression* assign_value = expr->data.assign.value;
            const char* assign_name = expr->data.assign.name->data.name;
            // uint16_t stored_symbol_index = get_symbol_index(program, assign_name);
            Symbol* symbolfound = find_symbol(program->symbol_table, assign_name);
            if (symbolfound == NULL) {
                symbolfound = find_symbol(program->globals, assign_name);
                if (symbolfound == NULL) {
                    debug_print_formatted("Symbol [%s] undefined.", assign_name);
                    exit(1);
                }
            }

            SymbolValue assigned_value = compile_expr(program, assign_value, symbolfound->unique_index);
            
            //debug_print_formatted("Expression:");
            // display_expression(assign_value);
            /*if (assign_value->type != EXPR_NUMBER && assign_value->type != EXPR_BOOL && assign_value->type != EXPR_ARRAY ) {
                emit_byte(program, OP_STORE_LOCAL);
                emit_byte(program, (uint8_t) stored_symbol_index);
            }*/

            return assigned_value;//break;
        }

        default: {
            debug_printerr("Compilation aborted, reason:\n\033[1;31m[Compile Error]:\033[0m Unsupported expression:");
            display_expression(expr);
            fflush(stderr);
            exit(1);

            break;
        }
    }

    return Comp_NilVal;
}

int write_constant_to_file(Constant constant_saved, FILE* file) {
    fwrite(&constant_saved.type, sizeof(uint8_t), 1, file);
        
    if (constant_saved.type == N_CONST_STRING) {
        uint32_t length = (uint32_t) strlen(constant_saved.as.string);
        fwrite(&length, sizeof(length), 1, file);
        fwrite(constant_saved.as.string, sizeof(char), length, file);  
        // debug_print("Written STR to constants!\n");    

    } else if (constant_saved.type == N_CONST_BOOL) {
        uint8_t value = (uint8_t) constant_saved.as.boolean;
        fwrite(&value, sizeof(value), 1, file);
        // debug_print("Written BOOL to constants!\n");
    
    } else if (constant_saved.type == N_CONST_NUMBER) {
        double value = (double) constant_saved.as.number;
        fwrite(&value, sizeof(value), 1, file);
        //debug_print("Written NUM to constants!\n");

    } else if (constant_saved.type == N_CONST_RUNTIME_REG || constant_saved.type == N_CONST_STR_REF) {
        uint8_t value = (uint8_t) constant_saved.as.const_ref;
        fwrite(&value, sizeof(value), 1, file);
        //debug_print("Written REF to constants!\n");

    } else if (constant_saved.type == N_CONST_ARRAY) {
        uint32_t length = (uint32_t) constant_saved.as.array.count;
        fwrite(&length, sizeof(length), 1, file);

        for (int i = 0; i < constant_saved.as.array.count; i++) {
            Constant* element = (Constant*) constant_saved.as.array.elements[i];
            write_constant_to_file(*element, file);
        }

        //debug_print("Written ARRAY to constants!\n"); 
    }
}

int write_function_to_file(Function* fn_saved, FILE* file) {
    uint8_t tag = N_CONST_FUNCTION;
    fwrite(&tag, sizeof(uint8_t), 1, file);
    fwrite(&fn_saved->length, sizeof(int), 1, file);
    fwrite(&fn_saved->arg_count, sizeof(uint8_t), 1, file);
    fwrite(&fn_saved->reg_count, sizeof(uint8_t), 1, file);
    fwrite(fn_saved->bytes, sizeof(uint8_t), fn_saved->length, file);
}

/*
    notes for when i make the vm:


    * use a 'module-context' variable for when you load module parts, that way when you do
    "load_glob r0, 1" it grabs the module-context and looks for the register 1. that way
    we can keep exporting external and values inaccessible from outer scopes.
*/

int write_module_to_file(CompiledModule* module, FILE* file) {
    uint8_t mod_type = (uint8_t) N_CONST_MODULE;
    fwrite(&mod_type, sizeof(uint8_t), 1, file);

    Program* mod_prog = module->module_program;

    /* writing like a program */
    uint8_t exported = (uint8_t) module->exports.count;
    uint32_t module_size = (uint32_t) mod_prog->byte_counter;
    uint32_t func_count = (uint32_t) mod_prog->func_count;

    fwrite(&exported, sizeof(exported), 1, file);
    fwrite(&module_size, sizeof(module_size), 1, file);
    fwrite(&func_count, sizeof(func_count), 1, file);
    debug_print_formatted("Writing module with %d functions and %d exports.", func_count, exported);

    for (uint8_t field_idx = 0; field_idx < exported; field_idx++) {//module->exports.symbols->unique_index;
        Symbol symbol = module->exports.symbols[(int) field_idx];
        uint8_t reg_referenced = symbol.index;
        uint8_t symbol_type = (uint8_t) symbol.value.type;
        fwrite(&field_idx, sizeof(field_idx), 1, file);
        fwrite(&symbol_type, sizeof(uint8_t), 1, file);
        fwrite(&reg_referenced, sizeof(reg_referenced), 1, file);
        debug_print_formatted("Export id %d is actually referencing Reg%d, and is of type (uint8) %d", field_idx, reg_referenced, symbol_type);
    }

    for (int i = 0; i < func_count; i++) {
        write_function_to_file(mod_prog->functions[i], file);
    }

    fwrite(mod_prog->bytes, sizeof(uint8_t), mod_prog->byte_counter, file);
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
    uint32_t modules_compiled = (uint32_t) program->module_count;

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
    fwrite(&modules_compiled, sizeof(modules_compiled), 1, file);

    for (int i = 0; i < program->constant_counter; i++) {
        Constant constant_saved = program->constants[i];
        write_constant_to_file(constant_saved, file);
    }

    for (int i = 0; i < function_count; i++) {
        Function* func_saved = program->functions[i];
        write_function_to_file(func_saved, file);
    }

    for (int i = 0; i < modules_compiled; i++) {
        CompiledModule* mod_saved = program->modules[i];
        write_module_to_file(mod_saved, file);
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

    if (!(version_major == 0 && version_minor == 0 && version_patch == 12000)) {
        fprintf(stderr, "Cannot open this binary as it is of an unsupported version.");
        exit(1);
        return NULL;
    }

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
    native_symbols.symbols[0] = Def_Native_Function("print", 0);
    // global_symbols.symbols[1] = DEF_NATIVE_FN("len", 1);

    native_symbols.count = 1;
}

int compile_program(const char* file_name, const char* output, int see_bytecode) {
    ParsedProgram* program_expressions = parse_file_expressions(file_name);
    if (program_expressions == NULL) {
        fprintf(stderr, "Could not parse program expressions, compilation terminated.\n");

        return 1;
    }

    load_natives();
    Program* program_result = init_program(file_name);
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