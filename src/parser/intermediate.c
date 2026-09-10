#include "parser/intermediate.h"

ParsedProgram* parse_file_expressions(const char* file_name) {
    size_t file_size = 0;
    ParsedProgram* new_program = malloc(sizeof(ParsedProgram));
    if (new_program == NULL) {
        fprintf(stderr, "Couldn't allocate enough memory for the ParsedProgram instance\n");
        exit(1);
    }

    const char* file_contents = get_file_contents(file_name, &file_size);

    if (file_contents == NULL) {
        fprintf(stderr, "Error when opening file contents!\n");
        return NULL;
    }

    new_program->tokens = tokenize(file_contents, file_size, &new_program->token_count);
    int current_token_pointer = 0;
    new_program->expressions = parse_statements(new_program->tokens, new_program->token_count, &current_token_pointer, &new_program->expression_count);

    return new_program;
};