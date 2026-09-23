#include "parser/eval.h"
#include "parser/intermediate.h"
#include <math.h>

#define nil (Value){.type = VALUE_UNDEFINED}
#define NumVal(x) (Value){.type = VALUE_NUMBER, .as.num_val = (x) }

int are_both_numbers(Value left, Value right) {
    return left.type == right.type && left.type == VALUE_NUMBER;
}

char* bool_str(uint8_t flag) {
    return flag == 1 ? "true":"false";
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

        case VALUE_MODULE: {
            Scope* mod_scope = accessed_value.as.module_scope;
            Binding* binding_to_be_found = lookup_in_scope(mod_scope, index_value.as.str_val);
            // printf("found binding: %s, is exported?: %s\n", binding_to_be_found->name, bool_str(binding_to_be_found->value.is_exported));
            if (binding_to_be_found != NULL && binding_to_be_found->value.is_exported == 1) {
                return binding_to_be_found->value;
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

Value evaluate(Expression* expr, Scope* scope) {
    switch (expr->type) {
        case EXPR_EXPORT: {
            Expression* exported_expression = expr->data.export;
            Value val = evaluate(exported_expression, scope);

            return val;
        }
        case EXPR_IMPORT: {
            const char* var_name = expr->data.define_body->data.assign.name->data.name;//evaluate(, scope);
            Value str_file = evaluate(expr->data.define_body->data.assign.value, scope);
            char file_name_stream[1024];
            snprintf(file_name_stream, strlen(str_file.as.str_val) + 4, "%s.nx", str_file.as.str_val);

            //printf("loading file: %s\n", file_name_stream);
            ParsedProgram* evaluate_program = parse_file_expressions(file_name_stream);
            if (evaluate_program == NULL) {
                fprintf(stderr, "Could not file into memory, memory not enough.\n");
                exit(1);
            }
            // Value* list = malloc(sizeof(Value) * evaluate_program->expression_count);

            Scope* mod_scope = create_scope(scope, NULL);
            if (mod_scope == NULL) {
                exit(1);
            };

            Value module = (Value){ .type = VALUE_MODULE, .as.module_scope = mod_scope };

            for (int i = 0; i < evaluate_program->expression_count; i++) {
                //display_expression(evaluate_program->expressions[i]);
                Value imported_val = evaluate(evaluate_program->expressions[i], mod_scope);
            }
            // printf("pushing to scope: %s\n", var_name);
            push_to_scope(scope, var_name, module);

            return module;
        }

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

        case EXPR_BOOL: {
            return (Value){
                .type = VALUE_BOOL,
                .as.bool_val = expr->data.bool_val,
            };
        }

        // (accessing scopes & varible stuff)
        case EXPR_NAME: {
            Binding* found = lookup_in_scope(scope, expr->data.name); 
            if (found == NULL) {
                fprintf(stderr, "Undefined variable: %s\n", expr->data.name);
                exit(1);
            }

            found->value.is_exported = expr->is_exporting;

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
            Value result;
            char* var_name;
            if (body->type == EXPR_ASSIGN) {
                result = evaluate(body->data.assign.value, scope);
                var_name = body->data.assign.name->data.name;
            } else if (body->type == EXPR_FUNCTION_DEF) {
                var_name = body->data.function_def.name;
                result = evaluate(body, scope);
            }

            //printf("var exported?! %s\n", bool_str(result.is_exported));

            result.is_exported = expr->is_exporting;
            push_to_scope(scope, var_name, result);

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
            Scope* sub_scope = create_scope(scope, NULL);

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

        case EXPR_WHILE_LOOP: {
            Expression* condition = expr->data.loop_while.condition;
            Expression* body = expr->data.loop_while.body;

            while (is_truthy(evaluate(condition, scope))) {
                evaluate(body, scope);
            }

            return nil;
        }

        case EXPR_FOR_LOOP: {
            Expression* variable = expr->data.loop_for.variable;
            Expression* looping_through = expr->data.loop_for.looping;
            Expression* body = expr->data.loop_for.body;

            const char* var_name = variable->data.define_body->data.assign.name->data.name;     
            evaluate(variable, scope);
            Value loop_thru = evaluate(looping_through, scope);

            if (loop_thru.type == VALUE_RANGE) {

                Value base_val = NumVal(loop_thru.as.range.start);

                int is_included = loop_thru.as.range.included;
                double st_val = loop_thru.as.range.start;
                double end_val = loop_thru.as.range.end;
                while ((is_included && st_val <= end_val) || (!is_included && st_val < end_val)) {
                    evaluate(body, scope);

                    st_val++;

                    Binding* var_binding = lookup_in_scope(scope, var_name);
                    // printf("rebinding: %s\n", var_name);
                    var_binding->value = NumVal(st_val);
                }
            } else if (loop_thru.type == VALUE_ARRAY) {
                push_to_scope(scope, var_name, nil);
                for (int i = 0; i < loop_thru.as.array_val.count; i++) {
                    Value item = loop_thru.as.array_val.items[i];
                    Binding* var_binding = lookup_in_scope(scope, var_name);
                    var_binding->value = item;

                    evaluate(body, scope);
                }
            }

            // 
            return nil;
        }

        case EXPR_RANGE: {
            Value start_num = evaluate(expr->data.range.start, scope);
            Value end_num = evaluate(expr->data.range.end, scope);

            return (Value) { 
                .type = VALUE_RANGE,
                .as.range.start = start_num.as.num_val,
                .as.range.end = end_num.as.num_val,
                .as.range.included = expr->data.range.included,
            };
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
            // printf("type of idx: %d, str_val: %s\n", (int) index_value.type, (char*) index_value.as.str_val);
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

            //push_to_scope(scope, expr->data.function_def.name, fn_val);
            return fn_val;
        }

        case EXPR_FN_CALL: {
            Expression* callee_expr = expr->data.call.callee;
            int is_method_call = callee_expr->type == EXPR_INDEX && callee_expr->data.index_expr.is_method_call;
            int is_mod_call = callee_expr->type == EXPR_INDEX && callee_expr->data.index_expr.is_mod_call;
            
            char* callee_name = "";

            Value self_value = { .type = VALUE_UNDEFINED };
            Value callee_val;

            if (is_method_call || is_mod_call) {
                self_value = evaluate(callee_expr->data.index_expr.target, scope);
                Value index_value = evaluate(callee_expr->data.index_expr.index, scope);
                //printf("index value type: %d, str_val: %s\n", (int) index_value.type, index_value.as.str_val);
                callee_name = index_value.as.str_val;
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

            if (callee_val.type == VALUE_UNDEFINED) {
                fprintf(stderr, "[Nexen-Runtime]: Cannot call \033[1;33m\"%s\"\033[0m because it is not defined, yet.\n", callee_name);
                exit(1);
            }

            if (callee_val.type == VALUE_NATIVE_FUNCTION) {
                Value result = callee_val.as.native_val(arg_values, total_arg_count);
                free(arg_values);
                return result;
            }

            if (callee_val.type != VALUE_FUNCTION) {
                fprintf(stderr, "Attempted to call a non-function value \033[1;33m\"%s\"\033[0m\n", callee_name);
                exit(1);
            }

            Expression* def = callee_val.as.func_val.def;
            if (total_arg_count != def->data.function_def.param_count) {
                fprintf(stderr, "Argument count mismatch calling %s\n", def->data.function_def.name);
                exit(1);
            }

            Scope* call_scope = create_scope(callee_val.as.func_val.closure, NULL);
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
