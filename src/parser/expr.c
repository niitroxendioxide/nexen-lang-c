#include "parser/expr.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parser/tokens.h"

BindingPower operator_binding_power(const char* op) {
    if (strcmp(op, "=") == 0)  return (BindingPower){0.1f, 0.2f};
    if (strcmp(op, "||") == 0) return (BindingPower){0.3f, 0.4f};
    if (strcmp(op, "&&") == 0) return (BindingPower){0.5f, 0.6f};
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return (BindingPower){0.7f, 0.8f};
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) return (BindingPower){0.9f, 1.0f};
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return (BindingPower){1.0f, 1.1f};
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return (BindingPower){2.0f, 2.1f};
    if (strcmp(op, ".") == 0 || strcmp(op, "[") == 0) return (BindingPower){4.0f, 4.1f};
    fprintf(stderr, "Invalid operator: %s\n", op);
    exit(1);
}


int expect_op(Token* tokens, int* pos, const char* op, size_t token_count) {
    if (*pos >= token_count) {
        return 0;
    }

    Token* current = &tokens[*pos];
    if (strcmp(current->data.op_val, op) == 0) {
        (*pos)++;
        return 1;
    }

    return 0;
}

int is_token_type(Token* tokens, int* pos, size_t token_count, TokenType token_type) {
    if (*pos >= token_count) {
        return 0;
    }

    Token* current = &tokens[*pos];
    if (current->type == token_type) {
        return 1;
    }

    return 0;
}

int is_name_token_eq(Token* tokens, int* pos, size_t token_count, const char* op) {
    if (*pos >= token_count) {
        return 0;
    }

    Token* current = &tokens[*pos];
    if (current->type == TOKEN_NAME && (strcmp(current->data.str_val, op) == 0)) {
        return 1;
    }

    return 0;
}

int is_number_token(const char* str_val) {
    if (str_val == NULL || str_val[0] == '\0') return 0;

    int i = 0;
    if (str_val[0] == '-') i = 1;
    if (str_val[i] == '\0') return 0;

    int seen_digit = 0;
    int seen_dot = 0;

    for (; str_val[i] != '\0'; i++) {
        if (isdigit((unsigned char)str_val[i])) {
            seen_digit = 1;
        } else if (str_val[i] == '.' && !seen_dot) {
            seen_dot = 1;
        } else {
            return 0;
        }
    }

    return seen_digit;
}

Expression* parse_name(Token* tokens, int* pos, size_t token_count) {
    if (*pos >= token_count) return NULL;

    Token* current = &tokens[*pos];
    if (current->type != TOKEN_NAME) return NULL;

    Expression* name_expression = malloc(sizeof(Expression));
    if (name_expression == NULL) return NULL;
    
    name_expression->type = EXPR_NAME;
    name_expression->data.name = current->data.str_val;
 
    (*pos)++;

    return name_expression;
}

Expression* parse_primary(Token* tokens, int* pos, size_t token_count) {
    if (*pos >= token_count) return NULL;
    Token* current = &tokens[*pos];

    switch (current->type) {
        case TOKEN_LPAREN: {
            (*pos)++;
            Expression* inner = parse_value(tokens, pos, token_count, 0);
            if (inner == NULL) return NULL;

            if (tokens[*pos].type != TOKEN_RPAREN) {
                fprintf(stderr, "Expected ')' to close expression.\n");
                exit(1);
            }
            (*pos)++;
            return inner;
        }

        case TOKEN_LBRACKET: {
            (*pos)++;
            size_t capacity = 8, count = 0;

            Expression** elements = malloc(sizeof(Expression*) * capacity);
            if (elements == NULL) {
                fprintf(stderr, "Error allocating initial elements array\n");
                exit(1);
            }

            while (tokens[*pos].type != TOKEN_RBRACKET) {
                if (count >= capacity) {
                    capacity *= 2;
                    Expression** temp = realloc(elements, sizeof(Expression*) * capacity);
                    if (temp == NULL) {
                        fprintf(stderr, "Error reallocating initial elements array\n");
                        exit(1);
                    };

                    elements = temp;
                }

                elements[count] = parse_value(tokens, pos, token_count, 0);
                count++;
            if (tokens[*pos].type == TOKEN_COMMA) {
                    (*pos)++;
                } else if (tokens[*pos].type != TOKEN_RBRACKET) {
                    fprintf(stderr, "Expected ',' or ']' in array literal\n");
                    exit(1);
                }
            }

            (*pos)++;

            Expression* array = malloc(sizeof(Expression));
            if (array == NULL) {
                fprintf(stderr, "couldn't allocate the array\n");
                exit(1);
            }

            array->type = EXPR_ARRAY;
            array->data.array.elements = elements;
            array->data.array.count = count;

            return array;
        }

        case TOKEN_STRING: {
            Expression* str_expr = malloc(sizeof(Expression));
            if (str_expr == NULL) {
                fprintf(stderr, "string malloc failed\n");
                exit(1);
            }

            str_expr->type = EXPR_STRING;
            str_expr->data.name = current->data.str_val;
            (*pos)++;

            return str_expr;
        }

        case TOKEN_SCOPE_BEGIN: {
            (*pos)++;

            size_t capacity = 8, count = 0;
            Expression** keys = malloc(sizeof(Expression*) * capacity);
            Expression** values = malloc(sizeof(Expression*) * capacity);
            if (keys == NULL || values == NULL) {
                fprintf(stderr, "Error allocating dict arrays\n");
                exit(1);
            }

            while (tokens[*pos].type != TOKEN_SCOPE_END) {
                if (count >= capacity) {
                    capacity *= 2;
                    keys = realloc(keys, sizeof(Expression*) * capacity);
                    values = realloc(values, sizeof(Expression*) * capacity);
                }

                Expression* key;

                if (tokens[*pos].type == TOKEN_LBRACKET) {
                    (*pos)++;
                    key = parse_value(tokens, pos, token_count, 0);
                    if (key == NULL) {
                        fprintf(stderr, "Expected expression inside '[ ]' key\n");
                        exit(1);
                    }
                    if (tokens[*pos].type != TOKEN_RBRACKET) {
                        fprintf(stderr, "Expected ']' to close computed key\n");
                        exit(1);
                    }
                    (*pos)++;
                } else {
                    key = parse_value(tokens, pos, token_count, 0);
                    if (key == NULL) {
                        fprintf(stderr, "Expected key in dict literal\n");
                        exit(1);
                    }

                    if (key->type == EXPR_NAME) {
                        key->type = EXPR_STRING;
                    }
                }

                if (!expect_op(tokens, pos, "=", token_count)) {
                    fprintf(stderr, "Expected '=' after dict key\n");
                    exit(1);
                }

                Expression* value = parse_value(tokens, pos, token_count, 0);
                if (value == NULL) {
                    fprintf(stderr, "Expected value after '=' in dict entry\n");
                    exit(1);
                }

                keys[count] = key;
                values[count] = value;
                count++;

                if (tokens[*pos].type == TOKEN_COMMA) {
                    (*pos)++;
                } else if (tokens[*pos].type != TOKEN_SCOPE_END) {
                    fprintf(stderr, "Expected ',' or '}' in dict literal\n");
                    exit(1);
                }
            }
            (*pos)++;

            Expression* dict_expr = malloc(sizeof(Expression));
            if (dict_expr == NULL) {
                fprintf(stderr, "couldn't allocate the dict\n");
                exit(1);
            }
            dict_expr->type = EXPR_DICT;
            dict_expr->data.dict.keys = keys;
            dict_expr->data.dict.values = values;
            dict_expr->data.dict.count = count;

            return dict_expr;
        }

        default: {
            if (current->type != TOKEN_NAME) return NULL;
            break;
        }
    }

    if (strcmp(current->data.str_val, "true") == 0 || strcmp(current->data.str_val, "false") == 0) {
        Expression* bool_exp = malloc(sizeof(Expression));
        if (bool_exp == NULL) return NULL;
        bool_exp->type = EXPR_BOOL;
        bool_exp->data.bool_val = strcmp(current->data.str_val, "false");

        (*pos)++;
        return bool_exp;
    };

    if (is_number_token(current->data.str_val)) {
        Expression* num_expr = malloc(sizeof(Expression));
        if (num_expr == NULL) return NULL;

        num_expr->type = EXPR_NUMBER;
        num_expr->data.value = strtod(current->data.str_val, NULL);

        (*pos)++;
        return num_expr;
    }

    return parse_name(tokens, pos, token_count);
}

Expression* parse_function_def(Token* tokens, int* pos, size_t token_count) {
    (*pos)++;

    if (tokens[*pos].type != TOKEN_NAME) {
        fprintf(stderr, "Expected function name after 'fn'\n");
        exit(1);
    }
    char* name = tokens[*pos].data.str_val;
    (*pos)++;

    if (tokens[*pos].type != TOKEN_LPAREN) {
        fprintf(stderr, "Expected '(' after function name\n");
        exit(1);
    }
    (*pos)++;

    char** param_names = malloc(sizeof(char*) * 8);
    size_t capacity = 8, param_count = 0;

    while (tokens[*pos].type != TOKEN_RPAREN) {
        if (param_count >= capacity) {
            capacity *= 2;
            param_names = realloc(param_names, sizeof(char*) * capacity);
        }
        if (tokens[*pos].type != TOKEN_NAME) {
            fprintf(stderr, "Expected parameter name\n");
            exit(1);
        }
        param_names[param_count++] = tokens[*pos].data.str_val;
        (*pos)++;

        if (tokens[*pos].type == TOKEN_COMMA) (*pos)++;
    }
    (*pos)++;

    Expression* body = parse_block(tokens, pos, token_count);
    if (body == NULL) {
        fprintf(stderr, "Couldn\'t parse the body of the function!\n");
        exit(1);
    }

    Expression* fn_def = malloc(sizeof(Expression));
    fn_def->type = EXPR_FUNCTION_DEF;
    fn_def->data.function_def.name = name;
    fn_def->data.function_def.param_names = param_names;
    fn_def->data.function_def.param_count = param_count;
    fn_def->data.function_def.body = body;

    return fn_def;
}

Expression* parse_postfix(Token* tokens, int* pos, size_t token_count) {
    Expression* expr = parse_primary(tokens, pos, token_count);
    if (expr == NULL) return NULL;

    while (*pos < token_count) {
        if (tokens[*pos].type == TOKEN_LBRACKET) {
            (*pos)++;

            Expression* index_expr = parse_value(tokens, pos, token_count, 0);
            if (index_expr == NULL) {
                fprintf(stderr, "Expected expression inside '[ ]'\n");
                exit(1);
            }

            if (tokens[*pos].type != TOKEN_RBRACKET) {
                fprintf(stderr, "Expected ']' to close index\n");
                exit(1);
            }
            (*pos)++;

            Expression* index_node = malloc(sizeof(Expression));
            index_node->type = EXPR_INDEX;
            index_node->data.index_expr.target = expr;
            index_node->data.index_expr.index = index_expr;

            expr = index_node;
        } else if (tokens[*pos].type == TOKEN_OP && strcmp(tokens[*pos].data.op_val, ".") == 0) {
            (*pos)++;

            if (tokens[*pos].type != TOKEN_NAME) {
                fprintf(stderr, "Expected a name after '.'\n");
                exit(1);
            }

            Expression* field_name = malloc(sizeof(Expression));
            field_name->type = EXPR_STRING;
            field_name->data.name = tokens[*pos].data.str_val;
            (*pos)++;

            Expression* index_node = malloc(sizeof(Expression));
            index_node->type = EXPR_INDEX;
            index_node->data.index_expr.target = expr;
            index_node->data.index_expr.index = field_name;
            expr = index_node;
        } else if (tokens[*pos].type == TOKEN_LPAREN) {
            (*pos)++; // consume '('

            Expression** args = malloc(sizeof(Expression*) * 8);
            size_t capacity = 8, arg_count = 0;

            while (tokens[*pos].type != TOKEN_RPAREN) {
                if (arg_count >= capacity) {
                    capacity *= 2;
                    args = realloc(args, sizeof(Expression*) * capacity);
                }
                args[arg_count++] = parse_value(tokens, pos, token_count, 0);
                if (tokens[*pos].type == TOKEN_COMMA) (*pos)++;
            }
            (*pos)++;

            Expression* call_node = malloc(sizeof(Expression));
            call_node->type = EXPR_FN_CALL;
            call_node->data.call.callee = expr;
            call_node->data.call.arguments = args;
            call_node->data.call.argument_count = arg_count;
            expr = call_node;
        } else {
            break;
        }
    }

    return expr;
}

Expression* parse_value(Token* tokens, int* pos, size_t token_count, float min_bp) {
    Expression* left = parse_postfix(tokens, pos, token_count);
    if (left == NULL) return NULL;

    while (*pos < token_count) {
        Token* current = &tokens[*pos];

        if (current->type != TOKEN_OP || strcmp(current->data.op_val, ",") == 0 || strcmp(current->data.op_val, "=") == 0) break;

        BindingPower bp = operator_binding_power(current->data.op_val);

        if (bp.left < min_bp) break; 
        (*pos)++; 

        Expression* right = parse_value(tokens, pos, token_count, bp.right);
        if (right == NULL) return NULL;

        Expression* op_expr = malloc(sizeof(Expression));
        if (op_expr == NULL) return NULL;
        op_expr->type = EXPR_BINARY_OPERATOR;
        op_expr->data.operation.op = current->data.op_val;
        op_expr->data.operation.left = left;
        op_expr->data.operation.right = right;

        left = op_expr;
    }


    return left;
}

Expression* parse_assignment(Token* tokens, int* pos, size_t token_count) {
    if (*pos >= token_count) {
        return NULL;
    }

    Expression* assigned_name = parse_name(tokens, pos, token_count);
    if (assigned_name == NULL) {
        return NULL;
    };

    int is_equal_op = expect_op(tokens, pos, "=", token_count);
    if (is_equal_op == 0) {
        return NULL;
    }

    Expression* assigned_value = parse_value(tokens, pos, token_count, 0);
    if (assigned_value == NULL) {
        return NULL;
    }

    Expression* new_expression = malloc(sizeof(Expression));
    
    new_expression->type = EXPR_ASSIGN;

    new_expression->data.assign.name = assigned_name;
    new_expression->data.assign.value = assigned_value;

    return new_expression;
}

Expression* parse_if(Token* tokens, int* pos, size_t token_count) {
    (*pos)++;

    Expression* condition_expression = parse_value(tokens, pos, token_count, 0);
    if (condition_expression == NULL) {
        fprintf(stderr, "Cannot have an if block without condition\n");
        exit(1);
    } 

    Expression* branch_then = parse_block(tokens, pos, token_count);
    Expression* branch_else = NULL;

    if (is_name_token_eq(tokens, pos, token_count, "else")) {
        (*pos)++;

        if (is_name_token_eq(tokens, pos, token_count, "if")) {
            branch_else = parse_if(tokens, pos, token_count);
        } else if (is_token_type(tokens, pos, token_count, TOKEN_SCOPE_BEGIN)) {
            branch_else = parse_block(tokens, pos, token_count);
        } else {
            fprintf(stderr, "Malformed else block, doesn\'t start with <TOKEN_SCOPE_BEGIN>\n");
            exit(1);
        }
    }

    Expression* if_expression = malloc(sizeof(Expression));
    if (if_expression == NULL) {
        fprintf(stderr, "Malloc for if_block failed");
        exit(1);
    }

    if_expression->type = EXPR_IF;
    if_expression->data.conditional.condition = condition_expression;
    if_expression->data.conditional.branch_then = branch_then;
    if_expression->data.conditional.branch_else = branch_else;

    return if_expression;
}

Expression* parse_token(Token* tokens, int* pos, size_t token_count) {
    if (*pos >= token_count) {
        return NULL;
    };

    Token* current = &tokens[*pos];

    if (strcmp(current->data.str_val, "let") == 0) {
        Expression* new_expression = malloc(sizeof(Expression));
        if (new_expression == NULL) return NULL;

        (*pos)++;

        Expression* define_body = parse_assignment(tokens, pos, token_count);
        if (define_body == NULL) {
            return NULL;
        }

        new_expression->type = EXPR_DEFINE;
        new_expression->data.define_body = define_body;

        return new_expression;
    } else if (strcmp(current->data.str_val, "if") == 0) {
        return parse_if(tokens, pos, token_count);
    } else if (*pos + 1 < token_count && tokens[*pos + 1].type == TOKEN_OP && strcmp(tokens[*pos + 1].data.op_val, "=") == 0) {
        Expression* assign_body = parse_assignment(tokens, pos, token_count);

        return assign_body;
    } else if (strcmp(current->data.str_val, "fn") == 0 || strcmp(current->data.str_val, "function") == 0) {
        return parse_function_def(tokens, pos, token_count);
    } else if (strcmp(current->data.str_val, "return") == 0) {
        (*pos)++;

        Expression* ret_expr = malloc(sizeof(Expression));
        ret_expr->type = EXPR_RETURN;

        if (tokens[*pos].type == TOKEN_STATEMENT_END || tokens[*pos].type == TOKEN_SCOPE_END) {
            ret_expr->data.return_value = NULL;
        } else {
            ret_expr->data.return_value = parse_value(tokens, pos, token_count, 0);
        }

        return ret_expr;
    }

    return parse_value(tokens, pos, token_count, 0);;
}

Expression* parse_block(Token* tokens, int* pos, size_t token_count) {
    if (tokens[*pos].type != TOKEN_SCOPE_BEGIN) {
        fprintf(stderr, "Cannot begin scope without a scope begin token.\n");
        exit(1);
    }

    (*pos)++;
    
    size_t block_statement_count = 0;
    Expression** block_statements = parse_statements(tokens, token_count, pos, &block_statement_count);
    if (block_statements == NULL) {
        fprintf(stderr, "Error when parsing block\n");
        exit(1);
    }

    Token* last_token = &tokens[*pos];
    if (last_token->type != TOKEN_SCOPE_END) {
        fprintf(stderr, "Scope does not have a set ending.\n");
        exit(1);
    }

    (*pos)++;

    Expression* new_block = malloc(sizeof(Expression));
    if (new_block == NULL) {
        fprintf(stderr, "Block scope malloc failed.\n");
        exit(1);
    }

    new_block->type = EXPR_BLOCK;
    new_block->data.block.statements = block_statements;
    new_block->data.block.count = block_statement_count;

    return new_block;
}

Expression** parse_statements(Token* tokens, size_t token_count, int* current_token_pos, size_t* statement_count) {
    int current_expression = 0;
    Expression** expression_list = malloc(token_count * sizeof(Expression));    

    while (*current_token_pos < token_count) {

        if (tokens[*current_token_pos].type == TOKEN_NAME) {
            Expression* new_expression = parse_token(tokens, current_token_pos, token_count);
            if (new_expression == NULL) {
                fprintf(stderr, "Expression could not be created.\n");

                exit(1);
            }

            if (new_expression->type != EXPR_IF && new_expression->type != EXPR_FUNCTION_DEF) {
                if (tokens[*current_token_pos].type == TOKEN_STATEMENT_END) {
                   (*current_token_pos)++;
                } else {
                    fprintf(stderr, "Exiting on lack of token statement end?\n[Warning]: make sure to add support if the statement should continue without ;\n");
                    exit(1);
                }
            } 

            expression_list[current_expression] = new_expression;
            current_expression++;

        } else if (tokens[*current_token_pos].type == TOKEN_SCOPE_BEGIN) {
            Expression* new_block = parse_block(tokens, current_token_pos, token_count);

            expression_list[current_expression] = new_block;
            current_expression++;
        } else if (tokens[*current_token_pos].type == TOKEN_EOF || tokens[*current_token_pos].type == TOKEN_SCOPE_END) {
            break;
        } else if (tokens[*current_token_pos].type == TOKEN_STATEMENT_END) {
            (*current_token_pos)++;
            continue;
        } else {
            fprintf(stderr, "Invalid expression. \n");
            display_token(tokens[*current_token_pos]);
            exit(1);
        }
    }

    if (statement_count != NULL) {
        *statement_count = current_expression;
    }

    return expression_list;
}

void display_expression(Expression* expr) {
    switch (expr->type) {
        case EXPR_DEFINE: {
            Expression* body = expr->data.define_body;
            Expression* value = body->data.assign.value;

            fprintf(stderr, "Expression (Define): %s = ", body->data.assign.name->data.name);
            if (value->type == EXPR_NAME) {
                fprintf(stderr, "%s\n", value->data.name);
            } else if (value->type == EXPR_NUMBER) {
                fprintf(stderr, "%f\n", value->data.value);
            } else {
                fprintf(stderr, "<complex expression>\n"); 
            }
            break;
        }
        case EXPR_NAME:
            fprintf(stderr, "Expression (Name): %s\n", expr->data.name);
            
            break;
        default:
            fprintf(stderr, "Unsupported expression\n");
            break;
    }
}
