#include <stdio.h>
#include <string.h>
#include "parser/interpreter.h"
#include "parser/tokens.h"
#include "file/content.h"
#include "parser/expr.h"
#include "parser/scope.h"
#include "parser/eval.h"
#include "native/stdfns.h"

int run_interpreter(const char* file_name, int show_tokens) {
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

    for (size_t i = 0; i < statement_count; i++) {
        Value result = evaluate(expressions[i], program_scope);
        if (expressions[i]->type == EXPR_NAME) {
            display_value(expressions[i]->data.name, result);
        };

        (void)result;
    }

    free_scope(program_scope);
    free_tokens(my_tokens, token_count);
    // free_expressions();
    free(expressions);

    return 0;
}
