#include "native/stdfns.h"
#include "native/library.h"
#include <math.h>

static void format_number(double num, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "%.6f", num);

    char* dot = strchr(buf, '.');
    if (dot) {
        char* end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            *end = '\0';
            end--;
        }
        if (end == dot) {
            *dot = '\0';
        }
    }
}

Value native_print(Value* args, size_t arg_count) {
    for (size_t i = 0; i < arg_count; i++) {
        switch (args[i].type) {
            case VALUE_STRING: { 
                printf("%s", args[i].as.str_val); 
                break;
            }
            case VALUE_NUMBER: {
                char buf[64];
                format_number(args[i].as.num_val, buf, sizeof(buf));
                printf("%s", buf);
                break;
            }
            case VALUE_BOOL: {
                printf("%s", args[i].as.bool_val ? "true" : "false"); 
                break;
            }
            case VALUE_UNDEFINED: {
                printf("undefined"); 
                break;
            }
            default: {
                printf("<unprintable [type=%d]>", args[i].type); 
                break;
            }
        }
        if (i + 1 < arg_count) printf(" ");
    }
    printf("\n");
    return (Value){ .type = VALUE_UNDEFINED };
}

Value native_sqrt(Value* args, size_t arg_count) {
    if (arg_count != 1) {
        return (Value){.type = VALUE_UNDEFINED };
    }

    Value arg_val = args[0];
    if (arg_val.type != VALUE_NUMBER) {
        return (Value){ .type = VALUE_UNDEFINED };
    }

    return (Value){ .type = VALUE_NUMBER, .as.num_val = sqrt(arg_val.as.num_val) };
}

static const NativeFnEntry std_entries[] = {
    NATIVE_FN("print", native_print),
};

static const NativeFnEntry math_entries[] = {
    NATIVE_FN("sqrt", native_sqrt),
};

void inject_native_libraries(Scope* scope) {
    register_native_library(scope, "std", std_entries, sizeof(std_entries) / sizeof(std_entries[0]));
    register_native_library(scope, "math", math_entries, sizeof(math_entries) / sizeof(math_entries[0]));
}