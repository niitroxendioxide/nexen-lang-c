#include <stdio.h>
#include <string.h>
#include "parser/interpreter.h"
#include "parser/tokens.h"
#include "file/content.h"
#include "parser/expr.h"
#include "parser/scope.h"
#include "parser/eval.h"
#include "native/stdfns.h"
#include <time.h>

int run_interpreter(const char* file_name, int show_tokens, int show_elapsed_time) {
    struct timespec t_start, t_end;
    size_t file_size = 0;
    int token_count = 0;
    size_t statement_count = 0;

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


    int current_token_pointer = 0;
    Expression** expressions = parse_statements(my_tokens, token_count, &current_token_pointer, &statement_count);
    Scope* program_scope = create_scope(NULL);

    inject_native_libraries(program_scope);

    timespec_get(&t_start, TIME_UTC);
    for (size_t i = 0; i < statement_count; i++) {
        Value result = evaluate(expressions[i], program_scope);
        if (expressions[i]->type == EXPR_NAME) {
            display_value(expressions[i]->data.name, result);
        };

        (void)result;
    }

    if (show_elapsed_time == 1) {
        timespec_get(&t_end, TIME_UTC);
                        
        int64_t diff_us = ((int64_t)t_end.tv_sec - t_start.tv_sec) * 1000000 + 
                        ((int64_t)t_end.tv_nsec - t_start.tv_nsec) / 1000;

        printf("\n\033[1;32m[Program-Time]\033[0m Program elapsed time: \033[1;32m%ld us\033[0m\033[0m\n", diff_us);
    }

    free_scope(program_scope);
    free_tokens(my_tokens, token_count);
    // free_expressions();
    free(expressions);

    return 0;
}
