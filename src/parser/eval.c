#include "parser/eval.h"
#include <math.h>

int are_both_numbers(Value left, Value right) {
    return left.type == right.type && left.type == VALUE_NUMBER;
}

Value apply_operator(char* op_code, Value left, Value right) {
    Value undefined_base = { .type = VALUE_UNDEFINED };
    double left_val = left.as.num_val;
    double right_val = right.as.num_val;

    if (strlen(op_code) >= 2) {
        if (strcmp(op_code, ">=") == 0) {
            if (!are_both_numbers(left, right)) {
                return undefined_base;
            }

            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val >= right_val) };
        } else if (strcmp(op_code, "<=") == 0) {
            if (!are_both_numbers(left, right)) {
                return undefined_base;
            }
            
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val <= right_val) };
        } else if (strcmp(op_code, "==") == 0) {
            if (left.type != right.type) {
                return (Value){.type = VALUE_BOOL, .as.bool_val = 0 };
            }

            switch (left.type) {
                case VALUE_ARRAY:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = 0 };
                case VALUE_DICT:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = 0 };
                case VALUE_NUMBER:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val == right_val) };
                case VALUE_STRING:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = (strcmp(left.as.str_val, right.as.str_val) == 0) };    
                case VALUE_BOOL:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = left.as.bool_val == right.as.bool_val };
                default: {
                    return undefined_base;
                }
            }
        } else if (strcmp(op_code, "!=") == 0 || strcmp(op_code, "~=") == 0) {
            if (left.type != right.type) {
                return (Value){.type = VALUE_BOOL, .as.bool_val = 1 };
            }

            switch (left.type) {
                case VALUE_ARRAY:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = 1 };
                case VALUE_DICT:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = 1 };
                case VALUE_NUMBER:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val != right_val) };
                case VALUE_STRING:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = (strcmp(left.as.str_val, right.as.str_val) == 1) };    
                case VALUE_BOOL:
                    return (Value){.type = VALUE_BOOL, .as.bool_val = left.as.bool_val != right.as.bool_val };    
                default: {
                    return undefined_base;
                }
            }
        } else if (strcmp(op_code, "..") == 0 && left.type == VALUE_STRING) {
            fprintf(stderr, "Must implement function for concatenation\n");
            return undefined_base;
        }

        return undefined_base;
    };

    if (left.type != right.type || left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
        return undefined_base;
    }

    switch ((unsigned char)op_code[0]) {
        case '+':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val + right_val };
        case '-':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val - right_val };
        case '*':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val * right_val };
        case '/':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val / right_val };
        case '^':
            return (Value){.type = VALUE_NUMBER, .as.num_val = pow(left_val, right_val) };
        case '>':
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val > right_val) };
        case '<':
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val < right_val) };
        default:
            fprintf(stderr, "Unsupported operation.\n");
            exit(1);
            break;
    }
}

Value index_into(Value accessed_value, Value index_value) {
    switch (accessed_value.type) {
        case VALUE_ARRAY: {
            if (index_value.type != VALUE_NUMBER) {
                fprintf(stderr, "Cannot index array without numbers\n");
                return (Value){.type = VALUE_UNDEFINED};
            }

            if (index_value.as.num_val >= accessed_value.as.array_val.count) {
                fprintf(stderr, "accessing value outside bounds\n");
                return (Value){.type = VALUE_UNDEFINED};
            }

            int c_index = (int) index_value.as.num_val;
            return accessed_value.as.array_val.items[c_index];
        }

        case VALUE_DICT: {
            for (int i = 0; i < accessed_value.as.dict_val.count; i++) {
                Value current_key = accessed_value.as.dict_val.keys[i];

                if ((index_value.type == VALUE_NUMBER && current_key.type == VALUE_NUMBER && current_key.as.num_val == index_value.as.num_val)
                    || (index_value.type == VALUE_STRING && current_key.type == VALUE_STRING && strcmp(current_key.as.str_val, index_value.as.str_val) == 0)
                ) {
                    return accessed_value.as.dict_val.values[i];
                }
            }

            return (Value){.type = VALUE_UNDEFINED};
        }

        default:
            return (Value){.type = VALUE_UNDEFINED};
    }
}

int is_truthy(Value val) {
    if (val.type == VALUE_BOOL) {
        return val.as.bool_val == 1;
    } else if (val.type == VALUE_NUMBER) {
        return val.as.num_val > 0;
    } else if (val.type == VALUE_UNDEFINED) {
        return 0;
    }

    return 1;
}

void display_value(const char* name, Value value) {
    switch (value.type) {
        case VALUE_STRING:
            printf("> (%s): %s\n", name, value.as.str_val);
            break;
        case VALUE_NUMBER:
            printf("> (%s): %f\n", name, value.as.num_val);
            break;
        case VALUE_BOOL: {
            const char *bool_state_text = (value.as.bool_val == 1) ? "true" : "false";
            printf("> (%s): %s\n", name, bool_state_text);
            break;
        }
        case VALUE_UNDEFINED:
            printf("> (%s): undefined\n", name);
            break;
        default:
            printf("> (%s): <value unreadable [type=%d, name=\"%s\"]>\n", name, value.type, name);
            break;
    }
}

Value evaluate(Expression* expr, Scope* scope) {
    switch (expr->type) {
        // core (like number & string)
        case EXPR_NUMBER: {
            return (Value){ 
                .type = VALUE_NUMBER, 
                .as.num_val = expr->data.value 
            };
        }

        case EXPR_STRING: {
            return (Value){
                .type = VALUE_STRING,
                .as.str_val = expr->data.name,
            };
        }

        // (accessing scopes & varible stuff)
        case EXPR_NAME: {
            Binding* found = lookup_in_scope(scope, expr->data.name); 
            if (found == NULL) {
                fprintf(stderr, "Undefined variable: %s\n", expr->data.name);
                exit(1);
            }
            return found->value;
        }

        // ==, !=, <=, >=, ~=, >, <, *, +, -, /
        case EXPR_BINARY_OPERATOR: {
            Value left = evaluate(expr->data.operation.left, scope);
            Value right = evaluate(expr->data.operation.right, scope);

            return apply_operator(expr->data.operation.op, left, right);
        }

        // the let keyword
        case EXPR_DEFINE: {
            Expression* body = expr->data.define_body; 
            Value result = evaluate(body->data.assign.value, scope);
            push_to_scope(scope, body->data.assign.name->data.name, result);

            return result; 
        }

        // overriding already existing variables using a = new_value
        case EXPR_ASSIGN: {
            Expression* reassigned_value = expr->data.assign.value;
            Binding* found = lookup_in_scope(scope, expr->data.assign.name->data.name); 
        
            if (found == NULL) {
                fprintf(stderr, "Cannot reassign %s because it was never defined.\n", expr->data.name);
                exit(1);
            }

            Value new_value = evaluate(reassigned_value, scope);

            found->value = new_value;
            return new_value;
        }

        case EXPR_BLOCK: {
            Scope* sub_scope = create_scope(scope);

            Value last = (Value){ .type = VALUE_UNDEFINED };
            for (int i = 0; i < expr->data.block.count; i++) {
                last = evaluate(expr->data.block.statements[i], sub_scope);
                if (last.is_return) {
                    free_scope(sub_scope);
                    return last;
                }
            }

            free_scope(sub_scope);

            return last;
        }

        case EXPR_ARRAY: {
            Value* array_list = malloc(sizeof(Value) * expr->data.array.count);
            if (array_list == NULL) {
                fprintf(stderr, "malloc failed for array list\n");
                exit(1);
            }

            for (int i = 0; i < expr->data.array.count; i++) {
                Value value_of_element = evaluate(expr->data.array.elements[i], scope); 
                array_list[i] = value_of_element;
            }

            return (Value){.type = VALUE_ARRAY, .as.array_val = {
                .items = array_list,
                .count = expr->data.array.count,
                .capacity = expr->data.array.count
                }, 
            };
        }

        case EXPR_DICT: {
            size_t count = expr->data.dict.count;

            Value* keys = malloc(sizeof(Value) * count);
            Value* values = malloc(sizeof(Value) * count);
            if (keys == NULL || values == NULL) {
                fprintf(stderr, "malloc failed building dict value\n");
                exit(1);
            }

            for (size_t i = 0; i < count; i++) {
                Value key_val = evaluate(expr->data.dict.keys[i], scope);

                if (key_val.type != VALUE_STRING && key_val.type != VALUE_NUMBER) {
                    fprintf(stderr, "Dict keys must be strings or numbers\n");
                    exit(1);
                }

                keys[i] = key_val;
                values[i] = evaluate(expr->data.dict.values[i], scope);
            }

            Value dict_val;
            dict_val.type = VALUE_DICT;
            dict_val.as.dict_val.keys = keys;
            dict_val.as.dict_val.values = values;
            dict_val.as.dict_val.count = count;
            dict_val.as.dict_val.capacity = count;

            return dict_val;
        }

        case EXPR_INDEX: {
            Value index_value = evaluate(expr->data.index_expr.index, scope);
            Value accessed_value = evaluate(expr->data.index_expr.target, scope);

            return index_into(accessed_value, index_value);
        }

        case EXPR_IF: {
            Value cond_result = evaluate(expr->data.conditional.condition, scope);

            if (is_truthy(cond_result) == 1) {
                return evaluate(expr->data.conditional.branch_then, scope);
            } else {
                return evaluate(expr->data.conditional.branch_else, scope); 
            }

            return (Value){.type = VALUE_UNDEFINED};
        }

        case EXPR_FUNCTION_DEF: {
            Value fn_val;
            fn_val.type = VALUE_FUNCTION;
            fn_val.as.func_val.def = expr;
            fn_val.as.func_val.closure = scope;

            push_to_scope(scope, expr->data.function_def.name, fn_val);
            return fn_val;
        }

        case EXPR_FN_CALL: {
            Expression* callee_expr = expr->data.call.callee;
            int is_method_call = callee_expr->type == EXPR_INDEX && callee_expr->data.index_expr.is_method_call;

            Value self_value = { .type = VALUE_UNDEFINED };
            Value callee_val;

            if (is_method_call) {
                self_value = evaluate(callee_expr->data.index_expr.target, scope);
                Value index_value = evaluate(callee_expr->data.index_expr.index, scope);
                callee_val = index_into(self_value, index_value);
            } else {
                callee_val = evaluate(callee_expr, scope);
            }

            size_t explicit_arg_count = expr->data.call.argument_count;
            size_t total_arg_count = explicit_arg_count + (is_method_call ? 1 : 0);

            Value* arg_values = malloc(sizeof(Value) * total_arg_count);
            size_t arg_offset = 0;
            if (is_method_call) {
                arg_values[0] = self_value;
                arg_offset = 1;
            }
            for (size_t i = 0; i < explicit_arg_count; i++) {
                arg_values[arg_offset + i] = evaluate(expr->data.call.arguments[i], scope);
            }

            if (callee_val.type == VALUE_NATIVE_FUNCTION) {
                Value result = callee_val.as.native_val(arg_values, total_arg_count);
                free(arg_values);
                return result;
            }

            if (callee_val.type != VALUE_FUNCTION) {
                fprintf(stderr, "Attempted to call a non-function value\n");
                exit(1);
            }

            Expression* def = callee_val.as.func_val.def;
            if (total_arg_count != def->data.function_def.param_count) {
                fprintf(stderr, "Argument count mismatch calling %s\n", def->data.function_def.name);
                exit(1);
            }

            Scope* call_scope = create_scope(callee_val.as.func_val.closure);
            for (size_t i = 0; i < total_arg_count; i++) {
                push_to_scope(call_scope, def->data.function_def.param_names[i], arg_values[i]);
            }

            free(arg_values);
            Value result = evaluate(def->data.function_def.body, call_scope);
            result.is_return = 0;

            free_scope(call_scope);
            return result;
        }

        case EXPR_RETURN: {
            Value result = (Value){ .type = VALUE_UNDEFINED };
            if (expr->data.return_value != NULL) {
                result = evaluate(expr->data.return_value, scope);
                if (result.is_return) return result;
            }
            result.is_return = 1;
            return result;
        }

        default:
            fprintf(stderr, "Cannot evaluate this expression type (%d)\n", expr->type);
            exit(1);
    }
}
