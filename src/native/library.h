#ifndef LANG_NATIVE_LIBRARY_H
#define LANG_NATIVE_LIBRARY_H

#include "parser/scope.h"
#include "parser/stack.h"

typedef struct {
    const char* name;
    NativeFn fn;
} NativeFnEntry;

#define NATIVE_FN(fn_name, fn_ptr) { .name = (fn_name), .fn = (fn_ptr) }

Value build_native_library(const NativeFnEntry* entries, size_t count);
void register_native_library(Scope* scope, const char* lib_name, const NativeFnEntry* entries, size_t count);

#endif
