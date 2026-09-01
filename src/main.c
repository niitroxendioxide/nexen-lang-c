#include <stdio.h>
#include <string.h>
#include "parser/interpreter.h"
#include "compiler/compiler.h"
#include "vm/virtual.h"

typedef enum {
    INVALID_FILE,
    RAW_FILE,
    COMPILED_FILE,
} ProgramFileType;

ProgramFileType get_file_type(const char* file_name) {
    size_t length = strlen(file_name);
    
    if (length < 4) {
        return INVALID_FILE;
    }

    const char* last_four = file_name + (length - 4);
    if (strcmp(last_four, ".nxo") == 0) {
        return COMPILED_FILE;
    }

    const char* last_three = file_name + (length - 3);
    if (strcmp(last_three, ".nx") == 0) {
        return RAW_FILE;
    }

    return INVALID_FILE;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Correct usage: %s <file> [--build]", argv[0]);

        return 1;
    }

    const char* file_path = argv[1];
    ProgramFileType type = get_file_type(file_path);
    if (type == INVALID_FILE) {
        fprintf(stderr, "Make sure to pass in a valid .nx/.nxo file.\n");
        return 1;
    }

    int build_mode = 0;
    int tokenize_mode = 0;
    int debug_mode = 0;
    int show_elapsed_time = 0;

    const char*output = "main.nxo";

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--build") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Must specify an output file after %s.\n", argv[i]);
                return 1;
            }

            output = argv[++i];
            build_mode = 1;
        } else if (strcmp(argv[i], "--tokenize") == 0) {
            tokenize_mode = 1;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--run-time") == 0) {
            show_elapsed_time = 1;
        } else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            return 1;
        }
    }

    if (type == COMPILED_FILE) {
        return vm_run_bytecode_from(file_path, debug_mode, show_elapsed_time);
    }

    if (build_mode) {
        return compile_program(file_path, output, tokenize_mode);
    }

    return run_interpreter(file_path, tokenize_mode, show_elapsed_time);
}
