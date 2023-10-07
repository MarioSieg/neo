/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_parser.h"
#include "neo_compiler.h"

#include <ctype.h>

#define DEPTH_LIM 16384 /* Max parse depth per block. */
#define EXPR_OP_DONE BINOP__COUNT
#define check_depth_lim(x) neo_assert((x) < DEPTH_LIM, "Depth limit of scope reached: %d", DEPTH_LIM)

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
static astref_t rule_function(parser_t *self, bool is_static);
static astref_t rule_class(parser_t *self, bool is_static);

/* Top-level root statement rules. */
static NEO_NODISCARD astref_t parser_root_stmt(parser_t *self, bool within_loop);
static NEO_NODISCARD astref_t parser_root_stmt_class(parser_t *self);
static NEO_NODISCARD astref_t parser_root_stmt_module(parser_t *self, bool *skip);
static NEO_NODISCARD astref_t parser_root_stmt_module_error_handling_wrapper(parser_t *self, bool *skip);
static NEO_NODISCARD astref_t parser_drain_whole_module(parser_t *self);

/* Parse rule table. */
static const parse_rule_t parse_rules_lut[] = {
    /* KW_* = Keyword tokens. */
    {NULL, NULL, PREC_NONE}, /* TOK_KW_FUNCTION, "fn" */
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
#define is_status_ok(self) neo_likely((!(self)->panic && !(self)->error))

static NEO_COLDPROC void error(parser_t *self, const token_t *tok, const char *msg) {
    const compile_error_t *error = comerror_from_token(COMERR_SYNTAX_ERROR, tok, (const uint8_t *)msg);
    errvec_push(self->errors, error);
    self->error = self->panic = true;
    self->prev_error = msg;
}

static NEO_AINLINE void advance(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    self->prev = self->curr;
    self->curr = lexer_scan_next(&self->lex);
    if (neo_unlikely(self->curr.type == TOK_ME_ERR)) {
        error(self, &self->curr, "Unexpected token");
    }
}

static NEO_AINLINE bool consume_match(parser_t *self, toktype_t type) {
    neo_dassert(self != NULL, "self is NULL");
    if (self->curr.type == type) {
        advance(self);
        return true;
    } else {
        return false;
    }
}

static NEO_AINLINE void consume_or_err(parser_t *self, toktype_t type, const char *msg) {
    neo_dassert(self != NULL, "self is NULL");
    if (neo_unlikely(self->curr.type != type)) {
        error(self, &self->curr, msg);
    } else {
        advance(self);
    }
}

static NEO_AINLINE astref_t consume_identifier(parser_t *self, const char *msg) {
    neo_dassert(self != NULL, "self is NULL");
    consume_or_err(self, TOK_LI_IDENT, msg);
    return astnode_new_ident(&self->pool, self->prev.lexeme, &self->prev);
}

/* bibibubupeepeeeppeeeepeepoopoooooooooooooooooooooo */
static NEO_AINLINE bool is_line_or_block_done(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    return self->curr.type == TOK_KW_END || self->curr.type == TOK_PU_NEWLINE;
}

/* Expression rules. */

static binary_op_type_t expr_paren_grouping(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    if (neo_likely(self->prev.type == TOK_PU_L_PAREN)) {
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
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    neo_dassert(self->prev.type == TOK_LI_IDENT, "Invalid token, must be identifier");
    *node = astnode_new_ident(&self->pool, self->prev.lexeme, &self->prev);
    return EXPR_OP_DONE;
}

static bool escape_string(parser_t *self, uint8_t *p, size_t n) {
    neo_dassert(self != NULL && p != NULL, "Invalid arguments");
    size_t l = 0;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != '\\') { /* Nothing to escape. */
            p[l++] = p[i];
            continue;
        }
        switch (p[++i]) {
            case 'n': p[++l] = '\n'; continue;
            case 't': p[++l] = '\t'; continue;
            case '\\': p[++l] = '\\'; continue;
            case 'v': p[++l] = '\v'; continue;
            case 'r': p[++l] = '\r'; continue;
            default: return false;
        }
    }
    p[l] = '\0';
    return true;
}

static binary_op_type_t expr_literal_string(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    if (neo_likely(self->prev.type == TOK_LI_STRING)) {
        uint8_t *str;
        srcspan_stack_clone(self->prev.lexeme, str);
        escape_string(self, str, self->prev.lexeme.len);
        *node = astnode_new_string(&self->pool, str, &self->prev);
        return EXPR_OP_DONE;
    } else {
        error(self, &self->prev, "Invalid string literal");
        return EXPR_OP_DONE;
    }
}

static binary_op_type_t expr_literal_char(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    return EXPR_OP_DONE; /* TODO. */
}

/* Handles literals of type: int, float, true, false, self. */
static binary_op_type_t expr_literal_scalar(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    switch (self->prev.type) {
        case TOK_LI_INT: {
            srcspan_t lexeme = self->prev.lexeme;
            uint8_t *p;
            p = (uint8_t *)alloca((1+2+lexeme.len)*sizeof(*(p)));
            memcpy(p, lexeme.p, lexeme.len*sizeof(*(p)));
            p[lexeme.len] = 'l';
            p[lexeme.len+1] = 'l';
            p[lexeme.len+2] = '\0';
            record_t v = {0};
            neo_strscan_format_t fmt = neo_strscan_scan(p, lexeme.len+2, &v, NEO_STRSCAN_OPT_LL);
            if (neo_unlikely(fmt != NEO_STRSCAN_INT
                && fmt != NEO_STRSCAN_U32
                && fmt != NEO_STRSCAN_I64)) {
                error(self, &self->prev, "Invalid literal for type 'int'");
                return EXPR_OP_DONE;
            }
            *node = astnode_new_int(&self->pool, v.ri64, &self->prev);
        } break;
        case TOK_LI_FLOAT: {
            srcspan_t lexeme = self->prev.lexeme;
            record_t v = {0};
            neo_strscan_format_t fmt = neo_strscan_scan(lexeme.p, lexeme.len, &v, NEO_STRSCAN_OPT_TONUM);
            if (neo_unlikely(fmt != NEO_STRSCAN_NUM)) {
                error(self, &self->prev, "Invalid literal for type 'float'");
                return EXPR_OP_DONE;
            }
            *node = astnode_new_float(&self->pool, v.as_float, &self->prev);
        } break;
        case TOK_LI_TRUE: {
            *node = astnode_new_bool(&self->pool, NEO_TRUE, &self->prev);
        } break;
        case TOK_LI_FALSE: {
            *node = astnode_new_bool(&self->pool, NEO_FALSE, &self->prev);
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
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_inc_infix(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_dec_prefix(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_dec_infix(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    return EXPR_OP_DONE; /* TODO. */
}

static binary_op_type_t expr_casting_infix(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
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
            error(self, &self->prev, "Invalid infix expression, expected literal");
    }
    return EXPR_OP_DONE;
}

static binary_op_type_t expr_unary_op(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
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
            error(self, &self->prev, "Invalid unary operator, expected '+', '-', '~' or 'not'");
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
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
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
            error(self, &self->prev, "Invalid binary operator, expected '+', '-', '*', '/', '%', '**', '&', '|', '^', '<<', '>>', '>>>', 'and', 'or', '==', '!=', '<', '<=', '>', '>='");
            return EXPR_OP_DONE;
    }
}
#undef map_prec

static void push_scope(parser_t *self, node_block_t *block) {
    if (node_block_can_have_symtabs(block)) {
        block->scope_depth = ++self->scope_depth;
    }
}
static void pop_scope(parser_t *self, const node_block_t *block) {
    if (node_block_can_have_symtabs(block)) {
        neo_assert(self->scope_depth > 0, "Scope push/pop mismatch");
        --self->scope_depth;
    }
}

static binary_op_type_t expr_function_call(parser_t *self, astref_t *node) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    advance(self); /* Eat LPAREN. */
    if (neo_likely(self->prev.type == TOK_PU_L_PAREN)) {
        if (!consume_match(self, TOK_PU_R_PAREN)) { /* We have arguments. */
            node_block_t args_list = {
                .scope = BLOCKSCOPE_ARGLIST
            };
            push_scope(self, &args_list);
            do { /* Parse arguments. */
                astref_t arg = ASTREF_NULL;
                expr_eval_precedence(self, &arg, PREC_TERNARY);
                if (neo_unlikely(astref_isnull(arg))) {
                    error(self, &self->prev, "Invalid argument in function call, expected expression");
                    pop_scope(self, &args_list);
                    return EXPR_OP_DONE;
                }
                node_block_push_child(&self->pool, &args_list, arg);
            } while (consume_match(self, TOK_PU_COMMA));
            consume_or_err(self, TOK_PU_R_PAREN, "Expected ')' after function call arguments");
            pop_scope(self, &args_list);
            *node = astnode_new_block(&self->pool, &args_list);
        }
        return BINOP_CALL;
    } else {
        error(self, &self->prev, "Invalid token in expression, expected '('");
    }
    return EXPR_OP_DONE;
}

static void expr_eval_precedence(parser_t *self, astref_t *node, precedence_t rule) {
    neo_dassert(self != NULL && node != NULL, "Invalid arguments");
    advance(self); /* Every valid expression has a prefix. */
    binary_op_type_t (*prefix)(parser_t *self, astref_t *node) = parse_rules_lut[self->prev.type].prefix;
    if (neo_unlikely(!prefix)) {
        error(self, &self->prev, "Expected expression, got invalid token");
        *node = ASTREF_NULL;
        return;
    }
    bool is_assign = rule <= PREC_ASSIGNMENT;
    (*prefix)(self, node);
    while (rule <= parse_rules_lut[self->curr.type].precedence) {
        binary_op_type_t (*infix)(parser_t *self, astref_t *node) = parse_rules_lut[self->curr.type].infix;
        if (neo_unlikely(!infix)) {
            error(self, &self->curr, "Expected operator in expression, got invalid token");
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
        error(self, &self->prev, "Invalid assignment target, expected identifier");
    }
}

/* Core rules. */

static astref_t rule_expr(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t node = ASTREF_NULL;
    expr_eval_precedence(self, &node, PREC_ASSIGNMENT);
    return node;
}

static astref_t rule_free_expr_statement(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t node = rule_expr(self);
    if (neo_unlikely(astref_isnull(node))) {
        error(self, &self->prev, "Invalid expression, expected expression");
        return ASTREF_NULL;
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after expression. Rule:  <expression> <newline>");
    const astnode_t *expr = astpool_resolve(&self->pool, node);
    if (expr != NULL && expr->type == ASTNODE_IDENT_LIT) {
        error(self, &self->prev, "Free expression statement is not allowed to be an identifier. Rule:  <expression> <newline>");
        return ASTREF_NULL;
    }
    return node;
}

static astref_t rule_branch(parser_t *self, bool within_loop) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t condition = ASTREF_NULL;
    expr_eval_precedence(self, &condition, PREC_TERNARY);
    consume_or_err(self, TOK_KW_THEN, "Expected 'then' after if-condition. Rule:  if <condition> then <true-block>");
    astref_t true_block = parser_root_stmt(self, within_loop);
    return astnode_new_branch(&self->pool, &(node_branch_t) {
        .cond_expr = condition,
        .true_block = true_block,
        .false_block = ASTREF_NULL /* TODO: else-block. */
    });
}

static astref_t rule_loop(parser_t *self, bool within_loop) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t condition = ASTREF_NULL;
    expr_eval_precedence(self, &condition, PREC_TERNARY);
    consume_or_err(self, TOK_KW_DO, "Expected 'do' after while-loop condition. Rule:  while <condition> do <true-block>");
    astref_t true_block = parser_root_stmt(self, within_loop);
    return astnode_new_loop(&self->pool, &(node_loop_t) {
        .cond_expr = condition,
        .true_block = true_block,
    });
}

static astref_t rule_return(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t expr = ASTREF_NULL;
    if (!is_line_or_block_done(self)) { /* If the block is not done and no newline appears, we have an expression to return. */
        expr_eval_precedence(self, &expr, PREC_TERNARY);
    }
    return astnode_new_return(&self->pool, &(node_return_t) {
        .child_expr = expr
    });
}

static astref_t rule_variable(parser_t *self, variable_scope_t var_scope) {
    neo_dassert(self != NULL, "self is NULL");
    astref_t identifier = consume_identifier(self,
         var_scope == VARSCOPE_PARAM
         ? "Expected parameter name. Rule: let <name>: <type>"
         : "Expected variable name after 'let'. Rule:  let <name>: <type>");
    consume_or_err(self, TOK_PU_COLON, "Expected ':' after type. Rule:  let <name>: <type>");
    astref_t type = consume_identifier(self, "Expected type name after ':'. Rule:  let <name>: <type>");
    astref_t init_expr = ASTREF_NULL;
    if (var_scope != VARSCOPE_PARAM) { /* All non-parameters must be initialized. */
        if (neo_likely(consume_match(self, TOK_OP_ASSIGN))) {
            init_expr = rule_expr(self);
            if (neo_unlikely(astref_isnull(init_expr))) {
                error(self, &self->prev, "Expected expression after '='. Rule:  let <name>: <type> = <expression> <newline>");
                return ASTREF_NULL;
            }
        } else {
            error(self, &self->curr, "Variable must be initialized. Rule:  let <name>: <type> = <expression> <newline>");
            return ASTREF_NULL;
        }
        consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after variable definition. Rule:  let <name>: <type> = <expression> <newline>");
    }
    return astnode_new_variable(&self->pool, &(node_variable_t) {
        .var_scope = var_scope,
        .ident = identifier,
        .type = type,
        .init_expr = init_expr
    });
}

static astref_t rule_function(parser_t *self, bool is_static) {
    (void)is_static;
    neo_dassert(self != NULL, "self is NULL");
    astref_t identifier = consume_identifier(self, "Expected function name. Rule: function <name>(<parameters>) -> <return-type>");
    consume_or_err(self, TOK_PU_L_PAREN, "Expected '(' after function name. Rule: function <name>(<parameters>) -> <return-type>");
    astref_t parameters = ASTREF_NULL;
    if (!consume_match(self, TOK_PU_R_PAREN)) { /* We have parameters. */
        node_block_t params_block = {
            .scope = BLOCKSCOPE_PARAMLIST
        };
        push_scope(self, &params_block);
        uint32_t depth = 0;
        do { /* Eat all parameters. */
            check_depth_lim(depth);
            node_block_push_child(&self->pool, &params_block, rule_variable(self, VARSCOPE_PARAM));
            ++depth;
        } while (is_status_ok(self) && consume_match(self, TOK_PU_COMMA));
        consume_or_err(self, TOK_PU_R_PAREN, "Expected ')' after function parameter list. Rule: function <name>(<parameters>) -> <return-type>");
        pop_scope(self, &params_block);
        parameters = astnode_new_block(&self->pool, &params_block);
    }
    astref_t ret_type = ASTREF_NULL;
    if (consume_match(self, TOK_PU_ARROW)) { /* We have a return type. */
        ret_type = consume_identifier(self, "Expected type after '->'. Rule: function <name>(<parameters>) -> <return-type>");
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after function signature. Rule: function <name>(<parameters>) -> <return-type> <newline>");
    astref_t body = parser_root_stmt(self, false);
    return astnode_new_method(&self->pool, &(node_method_t) {
       .ident = identifier,
       .params = parameters,
       .ret_type = ret_type,
       .body = body
    }); /* TODO: is static etc.. */
}

static astref_t rule_class(parser_t *self, bool is_static) {
    (void)is_static;
    neo_dassert(self != NULL, "self is NULL");
    astref_t identifier = consume_identifier(self, "Expected class name. Rule: class <name> <newline>");
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after class name. Rule: class <name> <newline>");
    astref_t body = parser_root_stmt_class(self);
    return astnode_new_class(&self->pool, &(node_class_t) {
        .ident = identifier,
        .body = body
    }); /* TODO: is static etc.. */
}

/* Root rules. */

/*
** Parse root block statement. (Level 3+ statement.)
** Local statements are global statements, function bodies, if-bodies, while-bodies etc. but not class or module bodies.
*/
static NEO_HOTPROC astref_t parser_root_stmt(parser_t *self, bool within_loop) {
    neo_dassert(self != NULL, "self is NULL");
    node_block_t local_block = {
        .scope = BLOCKSCOPE_LOCAL
    };
    push_scope(self, &local_block);
    for (uint32_t depth = 0; is_status_ok(self) && !consume_match(self, TOK_KW_END); ++depth) {
        check_depth_lim(depth);
        if (consume_match(self, TOK_KW_LET)) {
            node_block_push_child(&self->pool, &local_block, rule_variable(self, VARSCOPE_LOCAL));
        } else if (consume_match(self, TOK_KW_IF)) {
            node_block_push_child(&self->pool, &local_block, rule_branch(self, within_loop));
        } else if (consume_match(self, TOK_KW_WHILE)) {
            node_block_push_child(&self->pool, &local_block, rule_loop(self, true));
        } else if (consume_match(self, TOK_KW_RETURN)) {
            node_block_push_child(&self->pool, &local_block, rule_return(self));
        } else if (consume_match(self, TOK_KW_BREAK)) {
            if (neo_likely(within_loop)) {
                node_block_push_child(&self->pool, &local_block, astnode_new_break(&self->pool));
            } else {
                pop_scope(self, &local_block);
                error(self, &self->prev, "'break' statement can only be used within loops");
                return ASTREF_NULL;
            }
        } else if (consume_match(self, TOK_KW_CONTINUE)) {
            if (neo_likely(within_loop)) {
                node_block_push_child(&self->pool, &local_block, astnode_new_continue(&self->pool));
            } else {
                pop_scope(self, &local_block);
                error(self, &self->prev, "'continue' statement can only be used within loops");
                return ASTREF_NULL;
            }
        } else if (consume_match(self, TOK_PU_NEWLINE)) {
            /* Ignored here. */
        } else {
            node_block_push_child(&self->pool, &local_block, rule_free_expr_statement(self));
        }
    }
    pop_scope(self, &local_block);
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after function end. Rule: end <newline>");
    return astnode_new_block(&self->pool, &local_block);
}

/*
** Parse class body statement. (Level 2 statement.)
** Class body statements are functions, class variables, constructors etc..
*/
static NEO_HOTPROC astref_t parser_root_stmt_class(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    node_block_t class_block = {
        .scope = BLOCKSCOPE_CLASS
    };
    push_scope(self, &class_block);
    for (uint32_t depth = 0; is_status_ok(self) && !consume_match(self, TOK_KW_END); ++depth) {
        check_depth_lim(depth);
        bool is_static = consume_match(self, TOK_KW_STATIC); /* Is the following function or variable static? */
        if (consume_match(self, TOK_KW_FUNCTION)) {
            node_block_push_child(&self->pool, &class_block, rule_function(self, is_static));
        } else if (consume_match(self, TOK_KW_LET)) {
            node_block_push_child(&self->pool, &class_block, rule_variable(self, is_static ? VARSCOPE_STATIC_FIELD : VARSCOPE_FIELD));
        } else if (consume_match(self, TOK_PU_NEWLINE)) {
            /* Ignored here. */
        } else {
            error(self, &self->curr, "Expected function or variable.");
            pop_scope(self, &class_block);
            return ASTREF_NULL;
        }
    }
    consume_or_err(self, TOK_PU_NEWLINE, "Expected new line after class end. Rule: end <newline>");
    pop_scope(self, &class_block);
    return astnode_new_block(&self->pool, &class_block);
}

/*
** Parse module body statement. (Level 1 statement.)
** Module level statements are local statements, classes, interfaces, enums etc..
*/
static NEO_HOTPROC astref_t parser_root_stmt_module(parser_t *self, bool *skip) {
    neo_dassert(self != NULL && skip != NULL, "Invalid arguments");
    *skip = false; /* Assume that token is not skipped. */
    bool is_static = consume_match(self, TOK_KW_STATIC); /* Is the following class static? */
    if (consume_match(self, TOK_KW_CLASS)) {
        return rule_class(self, is_static);
    } else if (consume_match(self, TOK_KW_FUNCTION)) {
        return rule_function(self, is_static);
    } else if (consume_match(self, TOK_KW_LET)) {
        return rule_variable(self, VARSCOPE_LOCAL);
    } else if (consume_match(self, TOK_KW_IF)) {
        return rule_branch(self, false);
    } else if (consume_match(self, TOK_KW_WHILE)) {
        return rule_loop(self, false);
    } else if (consume_match(self, TOK_KW_BREAK)) {
        error(self, &self->prev, "'break' statement can only be used within loops");
        return ASTREF_NULL;
    } else if (consume_match(self, TOK_KW_CONTINUE)) {
        error(self, &self->prev, "'continue' statement can only be used within loops");
        return ASTREF_NULL;
    } else if (consume_match(self, TOK_KW_RETURN)) {
        error(self, &self->prev, "'return' statement can only be used within functions");
        return ASTREF_NULL;
    } else if (consume_match(self, TOK_PU_NEWLINE)) {
        *skip = true; /* Don't add \n to AST. */
        return ASTREF_NULL;
    } else if (neo_unlikely(consume_match(self, TOK_ME_EOF))) {
        return ASTREF_NULL;
    } else {
        return rule_free_expr_statement(self);
    }
}

static astref_t parser_root_stmt_module_error_handling_wrapper(parser_t *self, bool *skip) {
    if (neo_unlikely(!self->lex.src_data->len)) { return ASTREF_NULL; }
    neo_assert(self->curr.type < TOK__COUNT, "Invalid token type");
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
    neo_dassert(self != NULL, "self is NULL");
    node_block_t module_block = {
        .scope = BLOCKSCOPE_MODULE
    };
    push_scope(self, &module_block);
    for (uint32_t depth = 0; is_status_ok(self); ++depth) {
        check_depth_lim(depth);
        bool skip = false;
        astref_t node = parser_root_stmt_module_error_handling_wrapper(self, &skip);
        if (skip) { continue; }
        if (neo_unlikely(astref_isnull(node))) { break; }
        neo_assert(astpool_isvalidref(&self->pool, node), "Invalid AST-Reference emitted");
        node_block_push_child(&self->pool, &module_block, node);
    }
    pop_scope(self, &module_block);
    return astnode_new_module(&self->pool, &(node_module_t) {
        .body = astnode_new_block(&self->pool, &module_block)
    });
}

/* Exported parser API. */

void parser_init(parser_t *self, error_vector_t *errors) {
    neo_dassert(self != NULL && errors != NULL, "Invalid arguments");
    memset(self, 0, sizeof(*self));
    lexer_init(&self->lex);
    astpool_init(&self->pool);
    self->prev.type = self->curr.type = TOK__COUNT;
    self->errors = errors;
}

void parser_free(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    astpool_free(&self->pool);
    lexer_free(&self->lex);
    self->prev.type = self->curr.type = TOK_ME_EOF;
}

astref_t parser_parse(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    bool skip;
    return parser_root_stmt_module_error_handling_wrapper(self, &skip);
}

astref_t parser_drain(parser_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    return parser_drain_whole_module(self);
}

void parser_setup_source(parser_t *self, const source_t *src) {
    neo_dassert(self != NULL && src != NULL, "Invalid arguments");
    astpool_reset(&self->pool); /* Reset AST pool. */
    lexer_setup_source(&self->lex, src);
    self->error = self->panic = false;
    advance(self); /* Consume first token. */
}
