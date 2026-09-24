#include "parser/intermediate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* import_into_glob(const char* main_file, const char* imported_file) {
    if (!main_file || !imported_file) return NULL;

    const char* main_slash = strrchr(main_file, '/');
    const char* main_backslash = strrchr(main_file, '\\');
    const char* main_sep = (main_slash > main_backslash) ? main_slash : main_backslash;

    size_t dir_len = 0;
    if (main_sep != NULL) {
        dir_len = (main_sep - main_file) + 1;
    }

    const char* file_ptr = imported_file;
    if (strncmp(file_ptr, "./", 2) == 0 || strncmp(file_ptr, ".\\", 2) == 0) {
        file_ptr += 2;
    }

    size_t file_len = strlen(file_ptr);
    size_t ext_len = strlen(".nx");
    size_t total_len = dir_len + file_len + ext_len + 1;
    
    char* temp = (char*)malloc(total_len);
    if (!temp) return NULL;

    temp[0] = '\0';
    if (dir_len > 0) {
        strncpy(temp, main_file, dir_len);
        temp[dir_len] = '\0';
    }
    strcat(temp, file_ptr);

    size_t temp_len = strlen(temp);
    if (temp_len < 3 || strcmp(temp + temp_len - 3, ".nx") != 0) {
        strcat(temp, ".nx");
    }

    if (dir_len > 1) {
        const char* folder_end = main_sep;
        const char* prev_slash = NULL;
        
        for (const char* p = folder_end - 1; p >= main_file; p--) {
            if (*p == '/' || *p == '\\') {
                prev_slash = p;
                break;
            }
        }
        
        const char* folder_start = (prev_slash != NULL) ? (prev_slash + 1) : main_file;
        size_t folder_name_len = folder_end - folder_start;

        if (folder_name_len > 0) {
            char* dup_pattern1 = (char*)malloc(folder_name_len + 2);
            char* dup_pattern2 = (char*)malloc(folder_name_len + 2);
            
            if (dup_pattern1 && dup_pattern2) {
                strncpy(dup_pattern1, folder_start, folder_name_len);
                dup_pattern1[folder_name_len] = '/';
                dup_pattern1[folder_name_len + 1] = '\0';

                strncpy(dup_pattern2, folder_start, folder_name_len);
                dup_pattern2[folder_name_len] = '\\';
                dup_pattern2[folder_name_len + 1] = '\0';

                char* match = strstr(temp + dir_len, dup_pattern1);
                if (!match) match = strstr(temp + dir_len, dup_pattern2);

                if (match) {
                    memmove(match, match + folder_name_len + 1, strlen(match + folder_name_len + 1) + 1);
                }
            }
            free(dup_pattern1);
            free(dup_pattern2);
        }
    }

    return temp;
}

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