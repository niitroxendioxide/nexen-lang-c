#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "compiler/compiler.h"
#include "file/content.h"
#include "parser/tokens.h"

void init_chunk(Chunk* chunk) {
    chunk->code = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->constants = NULL;
    chunk->constant_count = 0;
    chunk->constant_capacity = 0;
    chunk->functions = NULL;
    chunk->function_count = 0;
    chunk->function_capacity = 0;
}

void emit_byte(Chunk* chunk, uint8_t byte) {
    if (chunk->count + 1 > chunk->capacity) {
        chunk->capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = realloc(chunk->code, chunk->capacity * sizeof(uint8_t));
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;
}

void emit_u16(Chunk* chunk, uint16_t value) {
    emit_byte(chunk, (uint8_t)(value & 0xFF));
    emit_byte(chunk, (uint8_t)((value >> 8) & 0xFF));
}

int emit_jump(Chunk* chunk, uint8_t jump_opcode) {
    emit_byte(chunk, jump_opcode);
    emit_u16(chunk, 0xFFFF); // patch_jump() backfills this
    return chunk->count - 2;
}

void patch_jump(Chunk* chunk, int jump_operand_offset) {
    uint16_t target = (uint16_t)chunk->count;
    chunk->code[jump_operand_offset] = (uint8_t)(target & 0xFF);
    chunk->code[jump_operand_offset + 1] = (uint8_t)((target >> 8) & 0xFF);
}

static int add_constant(Chunk* chunk, Constant constant) {
    if (chunk->constant_count + 1 > chunk->constant_capacity) {
        chunk->constant_capacity = chunk->constant_capacity < 8 ? 8 : chunk->constant_capacity * 2;
        chunk->constants = realloc(chunk->constants, chunk->constant_capacity * sizeof(Constant));
    }
    chunk->constants[chunk->constant_count] = constant;
    return chunk->constant_count++;
}

int add_number_constant(Chunk* chunk, double value) {
    return add_constant(chunk, (Constant){ .type = CONST_NUMBER, .as.number = value });
}

int add_string_constant(Chunk* chunk, const char* value) {
    return add_constant(chunk, (Constant){ .type = CONST_STRING, .as.string = strdup(value) });
}

int add_bool_constant(Chunk* chunk, int value) {
    return add_constant(chunk, (Constant){ .type = CONST_BOOL, .as.bool = value });
}

int add_function(Chunk* chunk, uint16_t body_offset, uint8_t param_count, uint16_t* param_name_indices) {
    if (chunk->function_count + 1 > chunk->function_capacity) {
        chunk->function_capacity = chunk->function_capacity < 4 ? 4 : chunk->function_capacity * 2;
        chunk->functions = realloc(chunk->functions, chunk->function_capacity * sizeof(FunctionMeta));
    }
    chunk->functions[chunk->function_count] = (FunctionMeta){
        .body_offset = body_offset,
        .param_count = param_count,
        .param_name_indices = param_name_indices,
    };
    return chunk->function_count++;
}

void compile(Expression* expr, Chunk* chunk) {
    if (expr == NULL) return;

    switch (expr->type) {
        case EXPR_NUMBER: {
            int index = add_number_constant(chunk, expr->data.value);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_STRING: {
            int index = add_string_constant(chunk, expr->data.name);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_BOOL: {
            int index = add_bool_constant(chunk, expr->data.bool_val);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_NAME: {
            int index = add_string_constant(chunk, expr->data.name);
            emit_byte(chunk, OP_LOAD_NAME);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_DEFINE: {
            Expression* body = expr->data.define_body;
            compile(body->data.assign.value, chunk);

            int index = add_string_constant(chunk, body->data.assign.name->data.name);
            emit_byte(chunk, OP_DEFINE_NAME);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_ASSIGN: {
            compile(expr->data.assign.value, chunk);

            int index = add_string_constant(chunk, expr->data.assign.name->data.name);
            emit_byte(chunk, OP_STORE_NAME);
            emit_u16(chunk, (uint16_t)index);
            break;
        }

        case EXPR_BINARY_OPERATOR: {
            compile(expr->data.operation.left, chunk);
            compile(expr->data.operation.right, chunk);

            if (strcmp(expr->data.operation.op, "+") == 0) emit_byte(chunk, OP_BINARY_ADD);
            else if (strcmp(expr->data.operation.op, "-") == 0) emit_byte(chunk, OP_BINARY_SUB);
            else if (strcmp(expr->data.operation.op, "*") == 0) emit_byte(chunk, OP_BINARY_MUL);
            else if (strcmp(expr->data.operation.op, "/") == 0) emit_byte(chunk, OP_BINARY_DIV);
            break;
        }

        // `obj:method(...)` calls compile the callee through EXPR_INDEX, which
        // compile() doesn't emit yet, so they'd silently produce broken
        // bytecode (args + OP_CALL with no callee underneath). Reject them
        // explicitly for now instead of emitting garbage.
        case EXPR_FN_CALL: {
            Expression* callee_expr = expr->data.call.callee;
            int is_method_call = callee_expr->type == EXPR_INDEX && callee_expr->data.index_expr.is_method_call;

            if (is_method_call) {
                fprintf(stderr, "Could not emit OPCODE (method calls not supported yet)\n");
                break;
            }

            compile(callee_expr, chunk);
            for (size_t i = 0; i < expr->data.call.argument_count; i++) {
                compile(expr->data.call.arguments[i], chunk);
            }

            emit_byte(chunk, OP_CALL);
            emit_byte(chunk, (uint8_t)expr->data.call.argument_count);
            break;
        }

        // No separate lexical scope per block right now — only function
        // calls get their own scope. A block is just "run these statements,
        // discard every value except the last" (that last value is the
        // block's own value, e.g. what a function body returns on fallthrough).
        case EXPR_BLOCK: {
            size_t count = expr->data.block.count;
            if (count == 0) {
                emit_byte(chunk, OP_PUSH_UNDEFINED);
                break;
            }

            for (size_t i = 0; i < count; i++) {
                compile(expr->data.block.statements[i], chunk);
                if (i + 1 < count) {
                    emit_byte(chunk, OP_POP);
                }
            }
            break;
        }

        case EXPR_RETURN: {
            if (expr->data.return_value != NULL) {
                compile(expr->data.return_value, chunk);
            } else {
                emit_byte(chunk, OP_PUSH_UNDEFINED);
            }
            emit_byte(chunk, OP_RETURN);
            break;
        }

        // Definitions compile inline: jump over the body so plain execution
        // doesn't fall into it, compile the body where it sits, then patch
        // the jump to land right after it. OP_MAKE_FUNCTION (emitted after
        // the patch) is what actually runs at definition time — it builds
        // the function value from the FunctionMeta this just registered.
        case EXPR_FUNCTION_DEF: {
            int skip_jump = emit_jump(chunk, OP_JUMP);

            uint16_t body_offset = (uint16_t)chunk->count;
            compile(expr->data.function_def.body, chunk);
            emit_byte(chunk, OP_RETURN); // fallthrough guard if the body doesn't explicitly `return`

            patch_jump(chunk, skip_jump);

            size_t param_count = expr->data.function_def.param_count;
            uint16_t* param_name_indices = malloc(sizeof(uint16_t) * (param_count > 0 ? param_count : 1));
            for (size_t i = 0; i < param_count; i++) {
                param_name_indices[i] = (uint16_t)add_string_constant(chunk, expr->data.function_def.param_names[i]);
            }

            int function_idx = add_function(chunk, body_offset, (uint8_t)param_count, param_name_indices);

            emit_byte(chunk, OP_MAKE_FUNCTION);
            emit_u16(chunk, (uint16_t)function_idx);

            int name_idx = add_string_constant(chunk, expr->data.function_def.name);
            emit_byte(chunk, OP_DEFINE_NAME);
            emit_u16(chunk, (uint16_t)name_idx);
            break;
        }
        default: {
            fprintf(stderr, "Could not emit OPCODE [%d]\n", expr->type);
            break;
        }
    }
}

void free_chunk(Chunk* chunk) {
    for (int i = 0; i < chunk->constant_count; i++) {
        if (chunk->constants[i].type == CONST_STRING) {
            free(chunk->constants[i].as.string);
        }
    }
    for (int i = 0; i < chunk->function_count; i++) {
        free(chunk->functions[i].param_name_indices);
    }
    free(chunk->code);
    free(chunk->constants);
    free(chunk->functions);
    init_chunk(chunk);
}

int write_to_output(const char* file_output, Chunk* program_chunk) {
    uint32_t magic_constant = LANG_SIGNATURE;
    uint16_t version_major = 1;
    uint16_t version_minor = 0;

    FILE *file = fopen(file_output, "wb");
    if (file == NULL) {
        perror("Failed to create bytecode file\n");
        return 0;
    }

    fwrite(&magic_constant, sizeof(magic_constant), 1, file);
    fwrite(&version_major, sizeof(version_major), 1, file);
    fwrite(&version_minor, sizeof(version_minor), 1, file);

    // Header ends here (matches VMHeader read as one 12-byte struct on the VM
    // side), so bytecode_size has to come right after version_minor, before
    // the constant pool section below.
    uint32_t bytecode_size = (uint32_t)program_chunk->count;
    fwrite(&bytecode_size, sizeof(uint32_t), 1, file);

    // Constant pool section: without this, every OP_PUSH_CONST/DEFINE_NAME/
    // LOAD_NAME/STORE_NAME operand in the code below is an index into a table
    // that only ever existed in this process's memory.
    uint16_t constant_count = (uint16_t)program_chunk->constant_count;
    fwrite(&constant_count, sizeof(constant_count), 1, file);

    for (int i = 0; i < program_chunk->constant_count; i++) {
        Constant* constant = &program_chunk->constants[i];
        uint8_t tag = (uint8_t)constant->type;
        fwrite(&tag, sizeof(tag), 1, file);

        if (constant->type == CONST_NUMBER) {
            fwrite(&constant->as.number, sizeof(double), 1, file);
        } else {
            uint32_t length = (uint32_t)strlen(constant->as.string);
            fwrite(&length, sizeof(length), 1, file);
            fwrite(constant->as.string, sizeof(char), length, file);
        }
    }

    // Function table section: OP_MAKE_FUNCTION's operand indexes into this,
    // not the constant pool. Placed after constants, before code, mirroring
    // the constant pool's self-contained "count then entries" shape so the
    // reader can walk it without needing offsets computed elsewhere.
    // Format: u16 function_count, then per entry:
    //   u16 body_offset, u8 param_count, param_count * u16 param_name_const_idx
    uint16_t function_count = (uint16_t)program_chunk->function_count;
    fwrite(&function_count, sizeof(function_count), 1, file);

    for (int i = 0; i < program_chunk->function_count; i++) {
        FunctionMeta* fn = &program_chunk->functions[i];
        fwrite(&fn->body_offset, sizeof(fn->body_offset), 1, file);
        fwrite(&fn->param_count, sizeof(fn->param_count), 1, file);
        fwrite(fn->param_name_indices, sizeof(uint16_t), fn->param_count, file);
    }

    fwrite(program_chunk->code, sizeof(uint8_t), bytecode_size, file);
    fclose(file);
    printf("File output succesfully created at: %s\n", file_output);

    return 1;
}

int compile_program(const char* file_name, const char* p_output_file, int show_tokens) {
    int token_count, current_token_pointer = 0;
    size_t file_size, statement_count = 0;

    const char* file_contents = get_file_contents(file_name, &file_size);

    if (file_contents == NULL) {
        fprintf(stderr, "Error when opening file contents!\n");
        return 1;
    }

    Token* my_tokens = tokenize(file_contents, file_size, &token_count);
    if (show_tokens == 1) {
        for (int i = 0; i < token_count; i++) {
            display_token(my_tokens[i]);
        }
    }

    Expression** expressions = parse_statements(my_tokens, token_count, &current_token_pointer, &statement_count);

    Chunk* program_chunk = malloc(sizeof(Chunk));
    if (program_chunk == NULL) {
        fprintf(stderr, "Couldn't allocate memory for program chunk\n");
        return 1;
    }
    init_chunk(program_chunk);

    for (int expr_c = 0; expr_c < statement_count; expr_c++) {
        compile(expressions[expr_c], program_chunk);
    }

    size_t filename_len = strlen(p_output_file) + 4 + 1;
    char* output_file = malloc(filename_len);
    if (output_file == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    snprintf(output_file, filename_len, "%s.nxo", p_output_file);

    int res = write_to_output(output_file, program_chunk);
    if (res == 0) {
        printf("Failed to write to .nxo output file\n");
        return 1;
    }

    return 0;
}