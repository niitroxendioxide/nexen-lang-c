#include "native/library.h"

Value build_native_library(const NativeFnEntry* entries, size_t count) {
    Value* keys = malloc(sizeof(Value) * count);
    Value* values = malloc(sizeof(Value) * count);
    if (keys == NULL || values == NULL) {
        fprintf(stderr, "malloc failed building native library\n");
        exit(1);
    }

    for (size_t i = 0; i < count; i++) {
        keys[i] = (Value){ .type = VALUE_STRING, .as.str_val = (char*)entries[i].name };
        values[i] = (Value){ .type = VALUE_NATIVE_FUNCTION, .as.native_val = entries[i].fn };
    }

    return (Value){
        .type = VALUE_DICT,
        .as.dict_val = {
            .keys = keys,
            .values = values,
            .count = count,
            .capacity = count,
        },
    };
}

void register_native_library(Scope* scope, const char* lib_name, const NativeFnEntry* entries, size_t count) {
    Value lib_val = build_native_library(entries, count);
    push_to_scope(scope, lib_name, lib_val);
}
