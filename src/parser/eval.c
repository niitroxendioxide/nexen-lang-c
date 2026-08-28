#include "parser/eval.h"

Value apply_operator(char* op_code, Value left, Value right) {
    if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
        return (Value){.type = VALUE_UNDEFINED };
    }
    
    double right_val = right.as.num_val;
    double left_val = left.as.num_val;

    if (strlen(op_code) >= 2) {
        if (strcmp(op_code, ">=") == 0) {
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val >= right_val) };
        } else if (strcmp(op_code, "<=") == 0) {
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val <= right_val) };
        } else if (strcmp(op_code, "==") == 0) {
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val == right_val) };
        } else if (strcmp(op_code, "!=") == 0 || strcmp(op_code, "~=") == 0) {
            return (Value){.type = VALUE_BOOL, .as.bool_val = (left_val != right_val) };
        }

        return (Value){.type = VALUE_UNDEFINED};
    };

    switch ((unsigned char)op_code[0]) {
        case '+':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val + right_val };
        case '-':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val - right_val };
        case '*':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val * right_val };
        case '/':
            return (Value){.type = VALUE_NUMBER, .as.num_val = left_val / right_val };
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
        case EXPR_NUMBER:
            return (Value){ .type = VALUE_NUMBER, .as.num_val = expr->data.value };

        case EXPR_NAME: {
            Binding* found = lookup_in_scope(scope, expr->data.name); 
            if (found == NULL) {
                fprintf(stderr, "Undefined variable: %s\n", expr->data.name);
                exit(1);
            }
            return found->value;
        }

        case EXPR_BINARY_OPERATOR: {
            Value left = evaluate(expr->data.operation.left, scope);
            Value right = evaluate(expr->data.operation.right, scope);
            if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
                fprintf(stderr, "Operations between non-numbers are not supported.\n");
                exit(1);
            }

            return apply_operator(expr->data.operation.op, left, right);
        }

        case EXPR_DEFINE: {
            Expression* body = expr->data.define_body; 
            Value result = evaluate(body->data.assign.value, scope);
            push_to_scope(scope, body->data.assign.name->data.name, result);

            //fprintf(stderr, "pushed to scope: %s %f", body->data.assign.name->data.name, result.as.num_val);
            return result; 
        }

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

            for (int i = 0; i < expr->data.block.count; i++) {
                Value evaluated = evaluate(expr->data.block.statements[i], scope);
                if (expr->data.block.statements[i]->type == EXPR_NAME) {
                    printf("> (%s): %f\n", expr->data.block.statements[i]->data.name, evaluated.as.num_val);
                };
            };

            free_scope(sub_scope);
            free(sub_scope);

            return (Value){.type = VALUE_UNDEFINED};
        }

        case EXPR_IF: {
            Value cond_result = evaluate(expr->data.conditional.condition, scope);

            //fprintf(stderr, "is truthy? %d", is_truthy(cond_result));
            if (is_truthy(cond_result) == 1) {
                return evaluate(expr->data.conditional.branch_then, scope);
            } else {
                return evaluate(expr->data.conditional.branch_else, scope); 
            }

            return (Value){.type = VALUE_UNDEFINED};
        }

        default:
            fprintf(stderr, "Cannot evaluate this expression type\n");
            exit(1);
    }
}
