/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_parser.h"

#include <ctype.h>

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
static bool match(parser_t *self, toktype_t tag);
static void consume(parser_t *self, toktype_t tag, const char *msg);
static astref_t consume_ident(parser_t *self, const char *msg);
static bool is_line_or_block_done(parser_t *self);

/* Expression rules. */

static binary_op_type_t expr_paren_grouping(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_ident(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_string(parser_t *self, astref_t *node);
static binary_op_type_t expr_literal_scalar(parser_t *self, astref_t *node);
static binary_op_type_t expr_inc_prefix(parser_t *self, astref_t *node);
static binary_op_type_t expr_inc_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_dec_prefix(parser_t *self, astref_t *node);
static binary_op_type_t expr_dec_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_casting_infix(parser_t *self, astref_t *node);
static binary_op_type_t expr_unary_op(parser_t *self, astref_t *node);
static binary_op_type_t expr_binary_op(parser_t *self, astref_t *node);
static void expr_eval_precedence(parser_t *self, astref_t *node, precedence_t rule);

/* Core rules. */

static astref_t rule_expr(parser_t *self);
static astref_t rule_if(parser_t *self, bool in_loop);
static astref_t rule_while(parser_t *self, bool in_loop);
static astref_t rule_return(parser_t *self);
static astref_t rule_variable(parser_t *self, variable_scope_t vtype);
static astref_t rule_method(parser_t *self, bool is_static);
static astref_t rule_class(parser_t *self, bool static_class);

/* Top-level root statement rules. */

static astref_t parser_stmt_file(parser_t *self, bool *skip);
static astref_t parser_stmt_block_class(parser_t *self);
static astref_t parser_stmt_block_local(parser_t *self, bool in_loop);

/* Implementation */
#define isok(self) neo_likely((!(self)->panic && !(self)->error))

static NEO_COLDPROC void error(parser_t *self, const token_t *tok, const char *msg) {

}

static NEO_AINLINE void advance(parser_t *self) {
    neo_dassert(self);
    self->prev = self->curr;
    self->curr = lexer_scan_next(&self->lex);
    if (neo_unlikely(self->curr.type == TOK_ME_ERR)) {
        error(self, &self->curr, "Unexpected token");
    }
}

static NEO_AINLINE bool match(parser_t *self, toktype_t type) {
    neo_dassert(self);
    if (self->curr.type == type) {
        advance(self);
        return true;
    } else {
        return false;
    }
}

static NEO_AINLINE void consume(parser_t *self, toktype_t type, const char *msg) {
    neo_dassert(self);
    if (neo_unlikely(self->curr.type != type)) {
        error(self, &self->curr, msg);
    } else {
        advance(self);
    }
}

static NEO_AINLINE astref_t consume_identifier(parser_t *self, const char *msg) {
    neo_dassert(self);
    consume(self, TOK_LI_IDENT, msg);
    return astnode_new_ident(&self->pool, self->prev.lexeme);
}

/* bibibubupeepeeeppeeeepeepoopoooooooooooooooooooooo */
static NEO_AINLINE bool is_line_or_block_done(parser_t *self) {
    neo_dassert(self);
    return self->curr.type == TOK_KW_END || self->curr.type == TOK_PU_NEWLINE;
}

void parser_init(parser_t *self) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    lexer_init(&self->lex);
    astpool_init(&self->pool);
    self->prev.type = self->curr.type = TOK__COUNT;
}

void parser_free(parser_t *self) {
    neo_dassert(self);
    astpool_free(&self->pool);
    lexer_free(&self->lex);
    self->prev.type = self->curr.type = TOK_ME_EOF;
}

astref_t parser_parse(parser_t *self) {
    neo_dassert(self);
    return ASTREF_NULL;
}

astref_t parser_drain(parser_t *self) {
    neo_dassert(self);
    return ASTREF_NULL;
}

void parser_prepare(parser_t *self) {
    neo_dassert(self);
    self->error = self->panic = false;
    advance(self); /* Consume first token. */
}

bool parse_int(const char *str, size_t len, neo_int_t *o) {
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
    if (len >= 2 && neo_unlikely(*p == '0')) {
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
    char *buf = alloca(len+1);
    memcpy(buf, str, len);
    buf[len] = '\0';
    *o = strtod(buf, &buf);
    return true;
}
