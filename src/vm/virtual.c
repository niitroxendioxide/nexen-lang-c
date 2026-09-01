#include "vm/virtual.h"
#include "compiler/emit.h"
#include "parser/eval.h"
#include <time.h>

static int operand_size(uint8_t opcode) {
    switch (opcode) {
        case OP_PUSH_CONST:
        case OP_DEFINE_NAME:
        case OP_LOAD_NAME:
        case OP_JUMP:
        case OP_MAKE_FUNCTION:
        case OP_STORE_NAME:
            return 2;
        case OP_CALL:
            return 1;
        default:
            return 0;
    }
}

static int refers_to_constant(uint8_t opcode) {
    return opcode == OP_PUSH_CONST || opcode == OP_DEFINE_NAME
        || opcode == OP_LOAD_NAME || opcode == OP_STORE_NAME;
}

void init_vm(VM* vm) {
    vm->stack = NULL;
    vm->stack_count = 0;
    vm->stack_capacity = 0;
    vm->globals = create_scope(NULL);
    vm->call_frame_count = 0;
    vm->call_frame_max = 64;
    vm->call_frames = malloc(sizeof(CallFrame) * vm->call_frame_max);

    inject_native_libraries(vm->globals);
}

void free_vm(VM* vm) {
    free(vm->stack);
    free(vm->call_frames);
    free_scope(vm->globals);
    vm->stack_capacity = 300;
    vm->stack = malloc(vm->stack_capacity * sizeof(Value));
    vm->stack_count = 0;
    vm->call_frames = NULL;
    vm->call_frame_count = 0;
    vm->globals = NULL;
}

void vm_push(VM* vm, Value value) {
    if (vm->stack_count + 1 > vm->stack_capacity) {
        vm->stack_capacity = vm->stack_capacity < 8 ? 8 : vm->stack_capacity * 2;
        vm->stack = realloc(vm->stack, vm->stack_capacity * sizeof(Value));
    }
    vm->stack[vm->stack_count++] = value;
}

Value vm_pop(VM* vm) {
    if (vm->stack_count == 0) {
        fprintf(stderr, "vm_pop: stack underflow\n");
        exit(1);
    }
    return vm->stack[--vm->stack_count];
}


void vm_push_call_frame(VM* vm, CallFrame frame) {
    if (vm->call_frame_count >= vm->call_frame_max) {
        fprintf(stderr, "vm_push_call_frame: call stack overflow (recursion limit reached)\n");
        exit(1);
    }
    vm->call_frames[vm->call_frame_count++] = frame;
}

CallFrame vm_pop_call_frame(VM* vm) {
    if (vm->call_frame_count == 0) {
        fprintf(stderr, "vm_pop_call_frame: call frame underflow\n");
        exit(1);
    }
    return vm->call_frames[--vm->call_frame_count];
}

// Which scope OP_DEFINE_NAME/LOAD_NAME/STORE_NAME should actually look
// names up in: the innermost active call's locals if we're inside a call,
// globals otherwise. This is the one place that "knows" about call frames
// for those three opcodes — they don't need to care about the call stack
// themselves.
Scope* vm_current_scope(VM* vm) {
    if (vm->call_frame_count > 0) {
        return vm->call_frames[vm->call_frame_count - 1].scope_locals;
    }
    return vm->globals;
}

 
void vm_execute(VM* vm, uint8_t* code, uint32_t code_size, VMConstant* constants, uint16_t constant_count,
                 VMFunctionMeta* functions, uint16_t function_count) {
    uint32_t ip = 0;

    while (ip < code_size) {
        uint8_t opcode = code[ip++];
        int op_size = operand_size(opcode);
        uint16_t operand = 0;

        if (op_size == 2) {
            operand = (uint16_t)(code[ip] | (code[ip + 1] << 8));
            ip += 2;
        } else if (op_size == 1) {
            operand = code[ip];
            ip += 1;
        }

        switch (opcode) {
            case OP_PUSH_CONST: {
                Value const_pushed = { .type = VALUE_UNDEFINED };
                switch (constants[operand].type) {
                    case CONST_NUMBER: {
                        const_pushed.type = VALUE_NUMBER;
                        const_pushed.as.num_val = constants[operand].as.number;
                        break;
                    }

                    case CONST_STRING: {
                        const_pushed.type = VALUE_STRING;
                        const_pushed.as.str_val = constants[operand].as.string;
                        break;
                    }

                    case CONST_BOOL: {
                        const_pushed.type = VALUE_BOOL;
                        const_pushed.as.bool_val = constants[operand].as.bool;
                        break;
                    }

                    default: break;
                }

                vm_push(vm, const_pushed);
                break;
            }
            case OP_POP: {
                vm_pop(vm);
                break;
            }
            case OP_DEFINE_NAME: {
                Value defined_value = vm_pop(vm);

                push_to_scope(vm_current_scope(vm), constants[operand].as.string, defined_value);
                break;
            }
            case OP_LOAD_NAME: {
                // Falls back to globals automatically: lookup_in_scope walks
                // Scope->parent, and a call frame's locals scope has globals
                // as its parent (see OP_CALL below) — so a function can still
                // read `print` or any other global without special-casing it.
                Binding* binding = lookup_in_scope(vm_current_scope(vm), constants[operand].as.string);
                if (binding == NULL) {
                    fprintf(stderr, "Undefined value: %s\n", constants[operand].as.string);
                    exit(1);
                }

                vm_push(vm, binding->value);
                break;
            }
            case OP_STORE_NAME: {
                Binding* binding = lookup_in_scope(vm_current_scope(vm), constants[operand].as.string);
                if (binding == NULL) {
                    fprintf(stderr, "Cannot reassign undefined value: %s\n", constants[operand].as.string);
                    exit(1);
                }

                Value stack_top_val = vm_pop(vm);
                push_to_scope(vm_current_scope(vm), constants[operand].as.string, stack_top_val);

                break;
            }
            case OP_BINARY_ADD:
            case OP_BINARY_SUB:
            case OP_BINARY_MUL:
            case OP_BINARY_DIV: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);

                char* operator = "null";
                switch (opcode) {
                    case OP_BINARY_ADD: operator = "+"; break;
                    case OP_BINARY_SUB: operator = "-"; break;
                    case OP_BINARY_MUL: operator = "*"; break;
                    case OP_BINARY_DIV: operator = "/"; break;
                }

                if (strcmp(operator, "null") == 0) {
                    fprintf(stderr, "Unsupported operation.\n");
                    exit(1);
                }

                Value new_val = apply_operator(operator, left, right);
                vm_push(vm, new_val);
                break;
            }
            case OP_JUMP: {
                ip = operand; 
                break;
            }
            case OP_PUSH_UNDEFINED: {
                vm_push(vm, (Value){ .type = VALUE_UNDEFINED });
                break;
            }
            case OP_MAKE_FUNCTION: {
                if (operand >= function_count) {
                    fprintf(stderr, "vm_execute: invalid function table index %u\n", operand);
                    exit(1);
                }

                VMFunctionMeta* meta = &functions[operand];
                Value fn_val = { .type = VALUE_FUNCTION };
                fn_val.as.func_val.def = NULL; 
                fn_val.as.func_val.closure = vm->globals;
                fn_val.as.func_val.bytecode_offset = meta->body_offset;
                fn_val.as.func_val.param_count = meta->param_count;
                fn_val.as.func_val.param_name_indices = meta->param_name_indices;

                vm_push(vm, fn_val);
                break;
            }
            case OP_CALL: {
                uint8_t arg_count = (uint8_t)operand;

                if (vm->stack_count < arg_count + 1) {
                    fprintf(stderr, "vm_execute: stack underflow on call\n");
                    exit(1);
                }

                Value* args = &vm->stack[vm->stack_count - arg_count];
                Value callee = vm->stack[vm->stack_count - arg_count - 1];

                if (callee.type == VALUE_NATIVE_FUNCTION) {
                    Value result = callee.as.native_val(args, arg_count);
                    vm->stack_count -= (arg_count + 1);
                    vm_push(vm, result);
                } else if (callee.type == VALUE_FUNCTION) {
                    if (arg_count != callee.as.func_val.param_count) {
                        fprintf(stderr, "vm_execute: argument count mismatch calling function (expected %u, got %u)\n",
                                callee.as.func_val.param_count, arg_count);
                        exit(1);
                    }

                    Scope* call_scope = create_scope(callee.as.func_val.closure);
                    for (uint8_t i = 0; i < arg_count; i++) {
                        uint16_t name_idx = callee.as.func_val.param_name_indices[i];
                        push_to_scope(call_scope, constants[name_idx].as.string, args[i]);
                    }

                    vm->stack_count -= (arg_count + 1);

                    CallFrame entered_callframe = { .return_ip = ip, .scope_locals = call_scope };
                    vm_push_call_frame(vm, entered_callframe);

                    ip = callee.as.func_val.bytecode_offset;
                } else {
                    fprintf(stderr, "vm_execute: attempted to call a non-function value\n");
                    exit(1);
                }
                break;
            }
            case OP_RETURN: {
                Value return_value = vm_pop(vm);

                if (vm->call_frame_count == 0) {
                    vm_push(vm, return_value);
                    return;
                }

                CallFrame frame = vm_pop_call_frame(vm);
                free_scope(frame.scope_locals);
                ip = frame.return_ip;
                vm_push(vm, return_value);
                break;
            }
            default:
                fprintf(stderr, "vm_execute: unimplemented opcode 0x%02X\n", opcode);
                break;
        }

        (void)operand;
    }

    (void)constants;
    (void)constant_count;
}

int vm_run_bytecode_from(const char* file_path, int file_debug, int show_elapsed_time) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    VMHeader header;
    if (fread(&header, sizeof(VMHeader), 1, file) != 1) {
        printf("Error: Could not read file header.\n");
        fclose(file);
        return 1;
    }

    if (header.magic_constant != LANG_SIGNATURE) {
        printf("Cannot read non .nxo files (file signature missing).\n");
        fclose(file);
        return 1;
    }

    if (file_debug == 1) {
        printf("---- [Reading .nxo]:\n");
        printf("> Signature: 0x%X\n", header.magic_constant);
        printf("> Version: %d.%d\n", header.version_major, header.version_minor);
        printf("> Size: %u bytes\n", header.bytecode_size);
    }

    uint16_t constant_count;
    if (fread(&constant_count, sizeof(constant_count), 1, file) != 1) {
        printf("Error: Could not read constant pool count.\n");
        fclose(file);
        return 1;
    }

    VMConstant* constants = malloc(sizeof(VMConstant) * constant_count);
    if (constants == NULL && constant_count > 0) {
        printf("Error: Out of memory reading constant pool.\n");
        fclose(file);
        return 1;
    }

    for (uint16_t i = 0; i < constant_count; i++) {
        uint8_t tag;
        if (fread(&tag, sizeof(tag), 1, file) != 1) {
            printf("Error: Unexpected end of file reading constant %u.\n", i);
            free(constants);
            fclose(file);
            return 1;
        }
        constants[i].type = (ConstantType)tag;

        if (constants[i].type == CONST_NUMBER) {
            if (fread(&constants[i].as.number, sizeof(double), 1, file) != 1) {
                printf("Error: Unexpected end of file reading constant %u.\n", i);
                free(constants);
                fclose(file);
                return 1;
            }
        } else {
            uint32_t length;
            if (fread(&length, sizeof(length), 1, file) != 1) {
                printf("Error: Unexpected end of file reading constant %u.\n", i);
                free(constants);
                fclose(file);
                return 1;
            }
            char* str = malloc(length + 1);
            if (fread(str, 1, length, file) != length) {
                printf("Error: Unexpected end of file reading constant %u.\n", i);
                free(str);
                free(constants);
                fclose(file);
                return 1;
            }
            str[length] = '\0';
            constants[i].as.string = str;
        }
    }

    if (file_debug == 1) {
        printf("> Constants: %u\n", constant_count);
        for (uint16_t i = 0; i < constant_count; i++) {
            if (constants[i].type == CONST_NUMBER) {
                printf("  [%u] number %f\n", i, constants[i].as.number);
            } else {
                printf("  [%u] string \"%s\"\n", i, constants[i].as.string);
            }
        }
    }

    uint16_t function_count;
    if (fread(&function_count, sizeof(function_count), 1, file) != 1) {
        printf("Error: Could not read function table count.\n");
        free(constants);
        fclose(file);
        return 1;
    }

    VMFunctionMeta* functions = malloc(sizeof(VMFunctionMeta) * (function_count > 0 ? function_count : 1));
    if (functions == NULL) {
        printf("Error: Out of memory reading function table.\n");
        free(constants);
        fclose(file);
        return 1;
    }

    for (uint16_t i = 0; i < function_count; i++) {
        if (fread(&functions[i].body_offset, sizeof(functions[i].body_offset), 1, file) != 1
            || fread(&functions[i].param_count, sizeof(functions[i].param_count), 1, file) != 1) {
            printf("Error: Unexpected end of file reading function %u.\n", i);
            
            for (uint16_t j = 0; j < i; j++) {
                free(functions[j].param_name_indices);
            }

            free(functions);
            free(constants);
            fclose(file);
            return 1;
        }

        functions[i].param_name_indices = malloc(sizeof(uint16_t) * (functions[i].param_count > 0 ? functions[i].param_count : 1));
        if (fread(functions[i].param_name_indices, sizeof(uint16_t), functions[i].param_count, file) != functions[i].param_count) {
            printf("Error: Unexpected end of file reading function %u.\n", i);
            free(functions[i].param_name_indices);

            for (uint16_t j = 0; j < i; j++) {
                free(functions[j].param_name_indices);
            }

            free(functions);
            free(constants);
            fclose(file);
            return 1;
        }
    }

    if (file_debug == 1) {
        printf("> Functions: %u\n", function_count);
        for (uint16_t i = 0; i < function_count; i++) {
            printf("  [%u] body_offset=%u param_count=%u\n", i, functions[i].body_offset, functions[i].param_count);
        }
        printf("---- [Operations]:\n");
    }

    uint8_t* code = malloc(header.bytecode_size);
    if (code == NULL) {
        printf("Error: Out of memory reading bytecode.\n");
        for (uint16_t i = 0; i < function_count; i++) free(functions[i].param_name_indices);
        free(functions);
        free(constants);
        fclose(file);
        return 1;
    }

    if (fread(code, 1, header.bytecode_size, file) != header.bytecode_size) {
        printf("Error: Unexpected end of file while reading bytecode.\n");
        free(code);
        for (uint16_t i = 0; i < function_count; i++) {
            free(functions[i].param_name_indices);
        }
        free(functions);
        free(constants);
        fclose(file);
        return 1;
    }


    // running the actual vm code
    struct timespec start, end;

    timespec_get(&start, TIME_UTC);

    VM vm;
    init_vm(&vm);
    vm_execute(&vm, code, header.bytecode_size, constants, constant_count, functions, function_count);
    free_vm(&vm);

    if (show_elapsed_time == 1) {
        timespec_get(&end, TIME_UTC);
                        
        int64_t diff_us = ((int64_t)end.tv_sec - start.tv_sec) * 1000000 + 
                        ((int64_t)end.tv_nsec - start.tv_nsec) / 1000;

        printf("\n\033[1;32m[Program-Time]\033[0m Program elapsed time: \033[1;32m%ld us\033[0m\033[0m\n", diff_us);
    }

    free(code);
    for (uint16_t i = 0; i < constant_count; i++) {
        if (constants[i].type == CONST_STRING) free(constants[i].as.string);
    }
    free(constants);
    for (uint16_t i = 0; i < function_count; i++) {
        free(functions[i].param_name_indices);
    }
    free(functions);
    fclose(file);
    return 0;
}