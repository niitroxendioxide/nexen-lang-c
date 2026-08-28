#include <stdio.h>
#include <string.h>
#include "parser/interpreter.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Correct usage: %s <file> [--build]", argv[0]);

        return 1;
    }

    const char* file_path = argv[1];
    int build_mode = 0;
    int tokenize_mode = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--build") == 0) {
            build_mode = 1;
        } else if (strcmp(argv[i], "--tokenize") == 0) {
            tokenize_mode = 1;
        } else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            return 1;
        }
    }

    if (build_mode) {
        fprintf(stderr, "Build mode not implemented yet.\n");
        return 0; 
    }

    return run_interpreter(file_path, tokenize_mode);
}
