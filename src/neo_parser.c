/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_parser.h"
#include "neo_compiler.h"

#include <ctype.h>

#define DEPTH_LIM 16384 /* Max parse depth per block. */
#define EXPR_OP_DONE BINOP__COUNT
#define check_depth_lim(x) neo_assert((x) < DEPTH_LIM && "Depth limit of scope reached (DEPTH_LIM)")

typedef enum precedence_t {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_TERNARY,
    PREC_OR,
    PREC_AND,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
} precedence_t;

typedef struct parse_rule_t {
    binary_op_type_t (*prefix)(parser_t *self, astref_t *node);
    binary_op_type_t (*infix)(parser_t *self, astref_t *node);
    precedence_t precedence;
} parse_rule_t;

/* Internal prototypes. */

/* Parser functionality core. */
static NEO_COLDPROC void error(parser_t *self, const token_t *tok, const char *msg);
static void advance(parser_t *self);
static bool consume_match(parser_t *self, toktype_t type);
static void consume_or_err(parser_t *self, toktype_t tag, const char *msg);
static astref_t consume_identifier(parser_t *self, const char *msg);
static bool is_line_or_block_done(parser_t *self);

/*
** The parser implementation grows down,
** atomic and smaller rules are at the top, complex and bigger rules at the bottom.
*/

/* Expression rules. */

static binary_op_type_t expr_paren_grouping(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_identifier(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_string(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_char(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_scalar(parser_t *self, astref_t *node);
static binary_op_type_t expr_inc_prefix(parser_t *self, astref_t *node);
static binary_op_type_t expr_inc_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_dec_prefix(parser_t *self, astref_t *node);
static binary_op_type_t expr_dec_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_casting_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_unary_op(parser_t *self, astref_t *node);
static binary_op_type_t expr_binary_op(parser_t *self, astref_t *node);
static binary_op_type_t expr_function_call(parser_t *self, astref_t *node);
static void expr_eval_precedence(parser_t *self, astref_t *node, precedence_t rule);

/* Core rules. */

static astref_t rule_expr(parser_t *self);
static astref_t rule_free_expr_statement(parser_t *self);
static astref_t rule_branch(parser_t *self, bool within_loop);
static astref_t rule_loop(parser_t *self, bool within_loop);
static astref_t rule_return(parser_t *self);
static astref_t rule_variable(parser_t *self, variable_scope_t var_scope);
static astref_t rule_method(parser_t *self, bool is_static);
static astref_t rule_class(parser_t *self, bool is_static);

/* Top-level root statement rules. */
static NEO_NODISCARD astref_t parser_root_stmt_local(parser_t *self, bool within_loop);
static NEO_NODISCARD astref_t parser_root_stmt_class(parser_t *self);
static NEO_NODISCARD astref_t parser_root_stmt_module(parser_t *self, bool *skip);
static NEO_NODISCARD astref_t parser_root_stmt_module_error_handling_wrapper(parser_t *self, bool *skip);
static NEO_NODISCARD astref_t parser_drain_whole_module(parser_t *self);

/* Parse rule table. */
static const parse_rule_t parse_rules_lut[] = {
    /* KW_* = Keyword tokens. */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_METHOD, "method" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_LET, "let" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_NEW, "new" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_END, "end" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_THEN, "then" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_IF, "if" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_ELSE, "else" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_RETURN, "return" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_CLASS, "class" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_MODULE, "module" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_BREAK, "break" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_CONTINUE, "continue" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_WHILE, "while" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_STATIC, "static" */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_DO, "do" */

    /* literals */
    {&expr_literal_identifier, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_scalar, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_scalar, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_string, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_char, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_scalar, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_scalar, &expr_casting_infix, PREC_PRIMARY},
    {&expr_literal_scalar, &expr_casting_infix, PREC_PRIMARY},

    /* SU_* = Structure tokens. */
    {&expr_paren_grouping, &expr_function_call, PREC_CALL},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE},

    /* OP_* = Operator tokens. */
    {NULL, NULL, PREC_NONE},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_FACTOR},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {&expr_inc_prefix, &expr_inc_infix, PREC_CALL},
    {&expr_dec_prefix, &expr_dec_infix, PREC_CALL},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_COMPARISON},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_TERM},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {NULL, &expr_binary_op, PREC_ASSIGNMENT},
    {&expr_unary_op, NULL, PREC_CALL},
    {NULL, &expr_binary_op, PREC_AND},
    {NULL, &expr_binary_op, PREC_OR},
    {&expr_unary_op, NULL, PREC_CALL},

    /* ME_* = Meta tokens. */
    {NULL, NULL, PREC_NONE},
    {NULL, NULL, PREC_NONE}
};

neo_static_assert(sizeof(parse_rules_lut)/sizeof(*parse_rules_lut) == TOK__COUNT && "Missing rules for tokens in rule table");

/* Implementation */
#define isok(self) neo_likely((!(self)->panic && !(self)->error))

static NEO_COLDPROC void error(parser_t *self, const token_t *tok, const char *msg) {
    const compile_error_t *error = comerror_from_token(COMERR_SYNTAX_ERROR, tok, (const uint8_t *)msg);
    errvec_push(self->errors, error);
    self->error = self->panic = true;
    self->prev_error = msg;
}

static NEO_AINLINE void advance(parser_t *self) {
    neo_dassert(self);
    self->prev = self->curr;
    self->curr = lexer_scan_next(&self->lex);
    if (neo_unlikely(self->curr.type == TOK_ME_ERR)) {
        error(self, &self->curr, "Unexpected token");
    }
}

static NEO_AINLINE bool consume_match(parser_t *self, toktype_t type) {
    neo_dassert(self);
    if (self->curr.type == type) {
        advance(self);
        return true;
    } else {
        return false;
    }
}

static NEO_AINLINE void consume_or_err(parser_t *self, toktype_t type, const char *msg) {
    neo_dassert(self);
    if (neo_unlikely(self->curr.type != type)) {
        error(self, &self->curr, msg);
    } else {
        advance(self);
    }
}

static NEO_AINLINE astref_t consume_identifier(parser_t *self, const char *msg) {
    neo_dassert(self);
    consume_or_err(self, TOK_LI_IDENT, msg);
    return astnode_new_ident(&self->pool, self->prev.lexeme);
}

/* bibibubupeepeeeppeeeepeepoopoooooooooooooooooooooo */
static NEO_AINLINE bool is_line_or_block_done(parser_t *self) {
    neo_dassert(self);
    return self->curr.type == TOK_KW_END || self->curr.type == TOK_PU_NEWLINE;
}

/* Expression rules. */

static binary_op_type_t expr_paren_grouping(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    if (self->prev.type == TOK_PU_L_PAREN) {
        expr_eval_precedence(self, node, PREC_TERNARY);
        consume_or_err(self, TOK_PU_R_PAREN, "Expected ')'");
        *node = astnode_new_group(&self->pool, &(node_group_t) {
            .child_expr = *node
        });
    } else {
        error(self, &self->prev, "Invalid token in expression");
    }
    return EXPR_OP_DONE;
}

static binary_op_type_t expr_literal_identifier(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    neo_dassert(self->prev.type == TOK_LI_IDENT);
    *node = astnode_new_ident(&self->pool, self->prev.lexeme);
    return EXPR_OP_DONE;
}

static binary_op_type_t expr_literal_string(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_literal_char(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

/* Handles literals of type: int, float, true, false, self. */
static binary_op_type_t expr_literal_scalar(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    switch (self->prev.type) {
        case TOK_LI_INT: {
            srcspan_t lexeme = self->prev.lexeme;
            neo_int_t x = 0;
            if (neo_unlikely(!parse_int((const char *)lexeme.p, lexeme.len, self->prev.radix, &x))) {
                error(self, &self->prev, "Invalid int literal");
                return EXPR_OP_DONE;
            }
            *node = astnode_new_int(&self->pool, x);
        } break;
        case TOK_LI_FLOAT: {
            srcspan_t lexeme = self->prev.lexeme;
            neo_float_t x = 0;
            if (neo_unlikely(!parse_float((const char *)lexeme.p, lexeme.len, &x))) {
                error(self, &self->prev, "Invalid float literal");
                return EXPR_OP_DONE;
            }
            *node = astnode_new_float(&self->pool, x);
        } break;
        case TOK_LI_TRUE: {
            *node = astnode_new_bool(&self->pool, NEO_TRUE);
        } break;
        case TOK_LI_FALSE: {
            *node = astnode_new_bool(&self->pool, NEO_FALSE);
        } break;
        case TOK_LI_SELF: {
            *node = astnode_new_self(&self->pool);
        } break;
        default: {
            error(self, &self->prev, "Literal type not yet implemented");
        }
    }
    return EXPR_OP_DONE;
}

static binary_op_type_t expr_inc_prefix(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_inc_infix(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_dec_prefix(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_dec_infix(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_casting_infix(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    advance(self);
    switch (self->prev.type) {
        case TOK_LI_INT:
        case TOK_LI_FLOAT:
        case TOK_LI_TRUE:
        case TOK_LI_FALSE:
        case TOK_LI_SELF: expr_literal_scalar(self, node); break;
        case TOK_LI_CHAR: expr_literal_char(self, node); break;
        case TOK_LI_STRING: expr_literal_string(self, node); break;
        case TOK_LI_IDENT: expr_literal_identifier(self, node); break;
        default:
            error(self, &self->prev, "Invalid infix expression");
    }
    return EXPR_OP_DONE;
}

static binary_op_type_t expr_unary_op(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    unary_op_type_t opcode = UNOP__COUNT;
    switch (self->prev.type) {
        case TOK_OP_ADD: opcode = UNOP_PLUS; break;
        case TOK_OP_SUB: opcode = UNOP_MINUS; break;
        case TOK_OP_BIT_COMPL: opcode = UNOP_BIT_COMPL; break;
        case TOK_OP_LOG_NOT: opcode = UNOP_LOG_NOT; break;
        case TOK_OP_INC: {
            error(self, &self->prev, "Unary increment is not yet implemented");
        } break;
        case TOK_OP_DEC: {
            error(self, &self->prev, "Unary decrement is not yet implemented");
        } break;
        default:
            error(self, &self->prev, "Invalid unary operator");
            return EXPR_OP_DONE;
    }
    astref_t expr = ASTREF_NULL;
    expr_eval_precedence(self, &expr, PREC_TERM);
    *node = astnode_new_unary_op(&self->pool, &(node_unary_op_t) {
        .opcode = opcode,
        .child_expr = expr
    });
    return EXPR_OP_DONE;
}

#define map_prec(type, prec)  case TOK_OP_##type: expr_eval_precedence(self, node, (precedence_t)(1+(PREC_##prec))); return BINOP_##type
static binary_op_type_t expr_binary_op(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    advance(self);
    switch (self->prev.type) {
        map_prec(ADD, TERM);
        map_prec(SUB, TERM);
        map_prec(MUL, FACTOR);
        map_prec(POW, FACTOR);
        map_prec(ADD_NO_OV, TERM);
        map_prec(SUB_NO_OV, TERM);
        map_prec(MUL_NO_OV, FACTOR);
        map_prec(POW_NO_OV, FACTOR);
        map_prec(DIV, FACTOR);
        map_prec(MOD, FACTOR);
        map_prec(BIT_AND, TERM);
        map_prec(BIT_OR, TERM);
        map_prec(BIT_XOR, TERM);
        map_prec(BIT_ASHL, TERM);
        map_prec(BIT_ASHR, TERM);
        map_prec(BIT_ROL, TERM);
        map_prec(BIT_ROR, TERM);
        map_prec(BIT_LSHR, TERM);
        map_prec(LOG_AND, AND);
        map_prec(LOG_OR, OR);

        map_prec(ASSIGN, ASSIGNMENT);
        map_prec(ADD_ASSIGN, ASSIGNMENT);
        map_prec(SUB_ASSIGN, ASSIGNMENT);
        map_prec(MUL_ASSIGN, ASSIGNMENT);
        map_prec(POW_ASSIGN, ASSIGNMENT);
        map_prec(ADD_ASSIGN_NO_OV, ASSIGNMENT);
        map_prec(SUB_ASSIGN_NO_OV, ASSIGNMENT);
        map_prec(MUL_ASSIGN_NO_OV, ASSIGNMENT);
        map_prec(POW_ASSIGN_NO_OV, ASSIGNMENT);
        map_prec(DIV_ASSIGN, ASSIGNMENT);
        map_prec(MOD_ASSIGN, ASSIGNMENT);
        map_prec(BIT_AND_ASSIGN, ASSIGNMENT);
        map_prec(BIT_OR_ASSIGN, ASSIGNMENT);
        map_prec(BIT_XOR_ASSIGN, ASSIGNMENT);
        map_prec(BIT_ASHL_ASSIGN, ASSIGNMENT);
        map_prec(BIT_ASHR_ASSIGN, ASSIGNMENT);
        map_prec(BIT_ROL_ASSIGN, ASSIGNMENT);
        map_prec(BIT_ROR_ASSIGN, ASSIGNMENT);
        map_prec(BIT_LSHR_ASSIGN, ASSIGNMENT);

        map_prec(EQUAL, COMPARISON);
        map_prec(NOT_EQUAL, COMPARISON);
        map_prec(LESS, COMPARISON);
        map_prec(LESS_EQUAL, COMPARISON);
        map_prec(GREATER, COMPARISON);
        map_prec(GREATER_EQUAL, COMPARISON);

        default:
            error(self, &self->prev, "Invalid binary operator");
            return EXPR_OP_DONE;
    }
}
#undef map_prec

static binary_op_type_t expr_function_call(parser_t *self, astref_t *node) {
    neo_dassert(self && node);
    advance(self); /* Eat LPAREN. */
    if (neo_likely(self->prev.type == TOK_PU_L_PAREN)) {
        if (!consume_match(self, TOK_PU_R_PAREN)) { /* We have arguments. */
            node_block_t arguments;
            node_block_init(&arguments, BLOCKSCOPE_ARGLIST);
            do { /* Parse arguments. */
                astref_t arg = ASTREF_NULL;
                expr_eval_precedence(self, &arg, PREC_TERNARY);
                if (neo_unlikely(astref_isnull(arg))) {
                    error(self, &self->prev, "Invalid argument in function call");
                    node_block_free(&arguments); /* Free block because it is not added to the AST pool for automatic memory management. */
                    return EXPR_OP_DONE;
                }
                node_block_push_child(&self->pool, &arguments, arg);
            } while (consume_match(self, TOK_PU_COMMA));
            consume_or_err(self, TOK_PU_R_PAREN, "Expected ')'");
            if (arguments.len) { /*If block is not empty, create block node. If block is empty, create ASTREF_NULL */
                *node = astnode_new_block(&self->pool, &arguments);
            } else {
                *node = ASTREF_NULL;
                node_block_free(&arguments);
            }
        }
        return BINOP_CALL;
    } else {
        error(self, &self->prev, "Invalid token in expression");
    }
    return EXPR_OP_DONE;
}

static void expr_eval_precedence(parser_t *self, astref_t *node, precedence_t rule) {
    neo_dassert(self && node);
    advance(self); /* Every valid expression has a prefix. */
    binary_op_type_t (*prefix)(parser_t *self, astref_t *node) = parse_rules_lut[self->prev.type].prefix;
    if (neo_unlikely(!prefix)) {
        error(self, &self->prev, "Expected expression");
        *node = ASTREF_NULL;
        return;
    }
    bool is_assign = rule <= PREC_ASSIGNMENT;
    (*prefix)(self, node);
    while (rule <= parse_rules_lut[self->curr.type].precedence) {
        binary_op_type_t (*infix)(parser_t *self, astref_t *node) = parse_rules_lut[self->curr.type].infix;
        if (neo_unlikely(!infix)) {
            error(self, &self->curr, "Expected operator in expression");
            *node = ASTREF_NULL;
            return;
        }
        astref_t right = ASTREF_NULL;
        binary_op_type_t opcode = (*infix)(self, &right);
        if (opcode == EXPR_OP_DONE) {
            *node = ASTREF_NULL;
            return;
        }
        *node = astnode_new_binary_op(&self->pool, &(node_binary_op_t) {
           .opcode = opcode,
           .left_expr = *node,
           .right_expr = right
        });
    }
    if (neo_unlikely(is_assign && consume_match(self, TOK_OP_ASSIGN))) {
        error(self, &self->prev, "Invalid assignment target");
    }
}

/* Core rules. */

static astref_t rule_expr(parser_t *self) {
    neo_dassert(self);
    astref_t node = ASTREF_NULL;
    expr_eval_precedence(self, &node, PREC_ASSIGNMENT);
    return node;
}

static astref_t rule_free_expr_statement(parser_t *self) {
    neo_dassert(self);
    astref_t node = rule_expr(self);
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after freestanding expression");
    return node;
}

static astref_t rule_branch(parser_t *self, bool within_loop) {
    neo_dassert(self);
    astref_t condition = ASTREF_NULL;
    expr_eval_precedence(self, &condition, PREC_TERNARY);
    consume_or_err(self, TOK_KW_THEN, "Expected 'then' after if-statement condition");
    astref_t true_block = parser_root_stmt_local(self, within_loop);
    return astnode_new_branch(&self->pool, &(node_branch_t) {
        .cond_expr = condition,
        .true_block = true_block,
        .false_block = ASTREF_NULL /* TODO: else-block. */
    });
}

static astref_t rule_loop(parser_t *self, bool within_loop) {
    neo_dassert(self);
    astref_t condition = ASTREF_NULL;
    expr_eval_precedence(self, &condition, PREC_TERNARY);
    consume_or_err(self, TOK_KW_DO, "Expected 'do' after while-loop condition");
    astref_t true_block = parser_root_stmt_local(self, within_loop);
    return astnode_new_loop(&self->pool, &(node_loop_t) {
        .cond_expr = condition,
        .true_block = true_block,
    });
}

static astref_t rule_return(parser_t *self) {
    neo_dassert(self);
    astref_t expr = ASTREF_NULL;
    if (!is_line_or_block_done(self)) { /* If the block is not done and no newline appears, we have an expression to return. */
        expr_eval_precedence(self, &expr, PREC_TERNARY);
    }
    return astnode_new_return(&self->pool, &(node_return_t) {
        .child_expr = expr
    });
}

static astref_t rule_variable(parser_t *self, variable_scope_t var_scope) {
    neo_dassert(self);
    astref_t identifier = consume_identifier(self,
         var_scope == VARSCOPE_PARAM
         ? "Expected parameter identifier"
         : "Expected variable identifier after 'let'");
    consume_or_err(self, TOK_PU_COLON, "Expected ':' after identifier");
    astref_t type = consume_identifier(self, "Expected type identifier");
    astref_t init_expr = ASTREF_NULL;
    if (var_scope != VARSCOPE_PARAM) { /* All non-parameters must be initialized. */
        if (neo_likely(consume_match(self, TOK_OP_ASSIGN))) {
            init_expr = rule_expr(self);
        } else {
            error(self, &self->curr, "Variable must be initialized");
            return ASTREF_NULL;
        }
        consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after variable definition");
    }
    return astnode_new_variable(&self->pool, &(node_variable_t) {
        .var_scope = var_scope,
        .ident = identifier,
        .type = type,
        .init_expr = init_expr
    });
}

static astref_t rule_method(parser_t *self, bool is_static) {
    (void)is_static;
    neo_dassert(self);
    astref_t identifier = consume_identifier(self, "Expected method identifier");
    consume_or_err(self, TOK_PU_L_PAREN, "Expected '(' after method identifier");
    astref_t parameters = ASTREF_NULL;
    if (!consume_match(self, TOK_PU_R_PAREN)) { /* We have parameters. */
        node_block_t param_list;
        node_block_init(&param_list, BLOCKSCOPE_PARAMLIST);
        int depth = 0;
        do { /* Eat all parameters. */
            check_depth_lim(depth);
            node_block_push_child(&self->pool, &param_list, rule_variable(self, VARSCOPE_PARAM));
            ++depth;
        } while (isok(self) && consume_match(self, TOK_PU_COMMA));
        consume_or_err(self, TOK_PU_R_PAREN, "Expected ')' after method parameter list");
        if (param_list.len) { /*If block is not empty, create block node. If block is empty, create ASTREF_NULL */
            parameters = astnode_new_block(&self->pool, &param_list);
        } else {
            parameters = ASTREF_NULL;
            node_block_free(&param_list);
        }
    }
    astref_t ret_type = ASTREF_NULL;
    if (consume_match(self, TOK_PU_ARROW)) { /* We have a return type. */
        ret_type = consume_identifier(self, "Expected type identifier after method arrow '->'");
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after method signature");
    astref_t body = parser_root_stmt_local(self, false);
    return astnode_new_method(&self->pool, &(node_method_t) {
       .ident = identifier,
       .params = parameters,
       .ret_type = ret_type,
       .body = body
    }); /* TODO: is static etc.. */
}

static astref_t rule_class(parser_t *self, bool is_static) {
    (void)is_static;
    neo_dassert(self);
    astref_t identifier = consume_identifier(self, "Expected class identifier");
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after class identifier");
    astref_t body = parser_root_stmt_class(self);
    return astnode_new_class(&self->pool, &(node_class_t) {
        .ident = identifier,
        .body = body
    }); /* TODO: is static etc.. */
}

/* Root rules. */

/*
** Parse local block statement. (Level 3+ statement.)
** Local statements are method bodies, if-bodies, while-bodies etc. but not class or module bodies.
*/
static NEO_HOTPROC astref_t parser_root_stmt_local(parser_t *self, bool within_loop) {
    neo_dassert(self);
    node_block_t block;
    node_block_init(&block, BLOCKSCOPE_LOCAL);
    for (int depth = 0; isok(self) && !consume_match(self, TOK_KW_END); ++depth) {
        check_depth_lim(depth);
        if (consume_match(self, TOK_KW_LET)) {
            node_block_push_child(&self->pool, &block, rule_variable(self, VARSCOPE_LOCAL));
        } else if (consume_match(self, TOK_KW_IF)) {
            node_block_push_child(&self->pool, &block, rule_branch(self, within_loop));
        } else if (consume_match(self, TOK_KW_WHILE)) {
            node_block_push_child(&self->pool, &block, rule_loop(self, true));
        } else if (consume_match(self, TOK_KW_RETURN)) {
            node_block_push_child(&self->pool, &block, rule_return(self));
        } else if (consume_match(self, TOK_KW_BREAK)) {
            if (neo_likely(within_loop)) {
                node_block_push_child(&self->pool, &block, astnode_new_break(&self->pool));
            } else {
                error(self, &self->prev, "'break'-statement can only be used within loops");
                return ASTREF_NULL;
            }
        } else if (consume_match(self, TOK_KW_CONTINUE)) {
            if (neo_likely(within_loop)) {
                node_block_push_child(&self->pool, &block, astnode_new_continue(&self->pool));
            } else {
                error(self, &self->prev, "'continue'-statement can only be used within loops");
                return ASTREF_NULL;
            }
        } else if (consume_match(self, TOK_PU_NEWLINE)) {
            /* Ignored here. */
        } else {
            node_block_push_child(&self->pool, &block, rule_free_expr_statement(self));
        }
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after method end");
    if (block.len) { /*If block is not empty, create block node. If block is empty, create ASTREF_NULL */
        return astnode_new_block(&self->pool, &block);
    } else {
        node_block_free(&block);
        return ASTREF_NULL;
    }
}

/*
** Parse class body statement. (Level 2 statement.)
** Class body statements are methods, class variables, constructors etc..
*/
static NEO_HOTPROC astref_t parser_root_stmt_class(parser_t *self) {
    neo_dassert(self);
    node_block_t block;
    node_block_init(&block, BLOCKSCOPE_CLASS);
    for (int depth = 0; isok(self) && !consume_match(self, TOK_KW_END); ++depth) {
        check_depth_lim(depth);
        bool is_static = consume_match(self, TOK_KW_STATIC); /* Is the following method or variable static? */
        if (consume_match(self, TOK_KW_METHOD)) {
            node_block_push_child(&self->pool, &block, rule_method(self, is_static));
        } else if (consume_match(self, TOK_KW_LET)) {
            node_block_push_child(&self->pool, &block, rule_variable(self, is_static ? VARSCOPE_STATIC_FIELD : VARSCOPE_FIELD));
        } else if (consume_match(self, TOK_PU_NEWLINE)) {
            /* Ignored here. */
        } else {
            error(self, &self->curr, "Expected method or variable definition within class");
            return ASTREF_NULL;
        }
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after class end");
    if (block.len) { /*If block is not empty, create block node. If block is empty, create ASTREF_NULL */
        return astnode_new_block(&self->pool, &block);
    } else {
        node_block_free(&block);
        return ASTREF_NULL;
    }
}

/*
** Parse module body statement. (Level 3 statement.)
** Module level statements are classes, interfaces, enums etc..
*/
static NEO_HOTPROC astref_t parser_root_stmt_module(parser_t *self, bool *skip) {
    neo_dassert(self && skip);
    *skip = false; /* Assume that token is not skipped. */
    bool is_static = consume_match(self, TOK_KW_STATIC); /* Is the following class static? */
    if (consume_match(self, TOK_KW_CLASS)) {
        return rule_class(self, is_static);
    } else if (consume_match(self, TOK_PU_NEWLINE)) {
        *skip = true; /* Don't add \n to AST. */
        return ASTREF_NULL;
    } else if (neo_unlikely(consume_match(self, TOK_ME_EOF))) {
        return ASTREF_NULL;
    } else {
        error(self, &self->curr, "No class found to execute. Did you forget to add a class to your source file?");
        return ASTREF_NULL;
    }
}

static astref_t parser_root_stmt_module_error_handling_wrapper(parser_t *self, bool *skip) {
    if (neo_unlikely(!self->lex.src_data->len)) { return ASTREF_NULL; }
    neo_assert(self->curr.type < TOK__COUNT && "Invalid token type");
    astref_t root = parser_root_stmt_module(self, skip);
    if (neo_unlikely(self->panic)) {
        return astnode_new_error(&self->pool, &(node_error_t) {
            .message = self->prev_error ? self->prev_error : "Unknown Error",
            .token = self->curr
        });
    } else {
        return root;
    }
}

static NEO_HOTPROC astref_t parser_drain_whole_module(parser_t *self) {
    neo_dassert(self);
    node_block_t block;
    node_block_init(&block, BLOCKSCOPE_MODULE);
    for (int depth = 0; isok(self); ++depth) {
        check_depth_lim(depth);
        bool skip = false;
        astref_t node = parser_root_stmt_module_error_handling_wrapper(self, &skip);
        if (skip) { continue; }
        if (neo_unlikely(astref_isnull(node))) { break; }
        neo_assert(astpool_isvalidref(&self->pool, node) && "Invalid AST-Reference emitted");
        node_block_push_child(&self->pool, &block, node);
    }
    astref_t body = ASTREF_NULL;
    if (block.len) { /*If block is not empty, create block node. If block is empty, create ASTREF_NULL */
        body = astnode_new_block(&self->pool, &block);
    } else {
        body = ASTREF_NULL;
        node_block_free(&block);
    }
    return astnode_new_module(&self->pool, &(node_module_t) {
        .body = body
    });
}

/* Exported parser API. */

void parser_init(parser_t *self, error_vector_t *errors) {
    neo_dassert(self && errors);
    memset(self, 0, sizeof(*self));
    lexer_init(&self->lex);
    astpool_init(&self->pool);
    self->prev.type = self->curr.type = TOK__COUNT;
    self->errors = errors;
}

void parser_free(parser_t *self) {
    neo_dassert(self);
    astpool_free(&self->pool);
    lexer_free(&self->lex);
    self->prev.type = self->curr.type = TOK_ME_EOF;
}

astref_t parser_parse(parser_t *self) {
    neo_dassert(self);
    bool skip;
    return parser_root_stmt_module_error_handling_wrapper(self, &skip);
}

astref_t parser_drain(parser_t *self) {
    neo_dassert(self);
    return parser_drain_whole_module(self);
}

void parser_setup_source(parser_t *self, const source_t *src) {
    neo_dassert(self && src);
    astpool_reset(&self->pool); /* Reset AST pool. */
    lexer_setup_source(&self->lex, src);
    self->error = self->panic = false;
    advance(self); /* Consume first token. */
}

bool parse_int(const char *str, size_t len, radix_t radix_hint, neo_int_t *o) {
    neo_int_t r = 0;
    neo_int_t sign = 1;
    int radix = 10;
    const char *p = str;
    const char *pe = p+len;
    if (neo_unlikely(!len || !*p)) { *o = 0; return false; }
    if (neo_unlikely(p[len-1]=='_')) { *o = 0; return false; } /* trailing _ not allow */
    while (neo_unlikely(isspace(*p))) { ++p; }
    if (*p=='+' || *p=='-') { sign = *p++ == '-' ? -1 : 1; }
    if (neo_unlikely(p == str+len)) { *o = 0; return false; } /* invalid number */
    if (neo_unlikely(*p=='_')) { *o = 0; return false; } /* _ prefix isn't allowed */
    if (radix_hint != RADIX_UNKNOWN) {
        radix = (int)radix_hint;
    } else if (len >= 2 && neo_unlikely(*p == '0')) {
        if ((tolower(p[1])) == 'x') { /* hex */
            radix = 16;
            p += 2;
        } else if (tolower(p[1]) == 'b') { /* bin */
            radix = 2;
            p += 2;
        } else if (tolower(p[1]) == 'c') { /* oct */
            radix = 8;
            p += 2;
        }
        if (neo_unlikely(p == str+len)) { *o = 0; return false; } /* invalid number */
    }
    switch (radix) {
        default:
        case 10: { /* dec */
            for (; neo_likely(isdigit(*p)) || neo_unlikely(*p=='_'); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = *p - '0';
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)/10 : r > (NEO_INT_MAX-digit)/10) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = r*10+digit*sign;
            }
        } break;
        case 16: { /* hex */
            for (; neo_likely(p < pe) && (neo_likely(isxdigit(*p)) || neo_unlikely(*p=='_')); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = (*p&15) + (*p >= 'A' ? 9 : 0);
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)>>4 : r > (NEO_INT_MAX-digit)>>4) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = (r<<4)+digit*sign;
            }
        } break;
        case 2: { /* bin */
            unsigned bits = 0;
            neo_uint_t v = 0;
            for (; neo_likely(p < pe) && *p; ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                v <<= 1;
                v ^= (*p=='1')&1;
                ++bits;
            }
            if (neo_unlikely(!bits)) { *o = 0; return false; } /* invalid bitcount */
            else if (neo_unlikely(bits > 64)) { *o = NEO_INT_MAX;  return false; } /* invalid bitcount */
            else if (neo_unlikely(v > (neo_uint_t)(sign == -1 ? NEO_INT_MIN : NEO_INT_MAX))) {
                *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                return false;
            }
            r = (neo_int_t)v;
            r *= sign;
        } break;
        case 8: { /* oct */
            for (; neo_likely(p < pe) && (neo_likely(*p >= '0' && *p <= '7') || neo_unlikely(*p=='_')); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = *p - '0';
                if (neo_unlikely(digit >= 8)) { /* invalid octal digit */
                    *o = 0;
                    return false;
                }
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)>>3 : r > (NEO_INT_MAX-digit)>>3) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = (r<<3)+digit*sign;
            }
        }
    }
    if (neo_unlikely(p != str+len)) { *o = 0; return false; } /* invalid number */
    *o = r;
    return true;
}

bool parse_float(const char *str, size_t len, neo_float_t *o) {
    char *buf = (char *)alloca(len+1);
    memcpy(buf, str, len);
    buf[len] = '\0';
    *o = strtod(buf, &buf);
    return true;
}
