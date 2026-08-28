#include "file/content.h"

char* get_file_contents(const char* file_source, size_t* file_size) {
    FILE *file = fopen(file_source, "r");
    if (file == NULL) {
        return NULL;
    }


    fseek(file, 0, SEEK_END);
    
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL; 
    }

    fseek(file, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(content, sizeof(char), size, file);

    content[bytes_read] = '\0';

    if (file_size != NULL) {
        *file_size = bytes_read;
    }

    fclose(file);
    return content;
}

