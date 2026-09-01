#include "parser/stack.h"
#include "parser/scope.h"

Value native_print(Value* args, size_t arg_count);
Value native_sqrt(Value* args, size_t arg_count);

void inject_native_libraries(Scope* scope);
