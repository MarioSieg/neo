/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

/*
** About this file:
** Contains the lexer implementation.
*/

#ifndef NEO_LEXER_H
#define NEO_LEXER_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct source_t source_t;

#define tkdef(_, __)\
    /* Keywords */\
    _(TOK_KW_METHOD, "method")__\
    _(TOK_KW_LET, "let")__\
    _(TOK_KW_NEW, "new")__\
    _(TOK_KW_END, "end")__\
    _(TOK_KW_THEN, "then")__\
    _(TOK_KW_IF, "if")__\
    _(TOK_KW_ELSE, "else")__\
    _(TOK_KW_RETURN, "return")__\
    _(TOK_KW_CLASS, "class")__\
    _(TOK_KW_MODULE, "module")__\
    _(TOK_KW_BREAK, "break")__\
    _(TOK_KW_CONTINUE, "continue")__\
    _(TOK_KW_WHILE, "while")__\
    _(TOK_KW_STATIC, "static")__\
    _(TOK_KW_DO, "do")__\
    /* Literals */\
    _(TOK_LI_IDENT, "<ident>")__\
    _(TOK_LI_INT, "<int>")__\
    _(TOK_LI_FLOAT, "<float>")__\
    _(TOK_LI_STRING, "<string>")__  \
    _(TOK_LI_CHAR, "<char>")__\
    _(TOK_LI_TRUE, "true")__\
    _(TOK_LI_FALSE, "false")__\
    /* Punctuation */\
    _(TOK_PU_L_PAREN, "(")__\
    _(TOK_PU_R_PAREN, ")")__\
    _(TOK_PU_L_BRACKET, "[")__\
    _(TOK_PU_R_BRACKET, "]")__\
    _(TOK_PU_L_BRACE, "{")__\
    _(TOK_PU_R_BRACE, "}")__\
    _(TOK_PU_COMMA, ",")__\
    _(TOK_PU_ARROW, "->")__\
    _(TOK_PU_COLON, ":")__\
    _(TOK_PU_AT, "@")__\
    _(TOK_PU_NEWLINE, "\\n")__\
    /* Operators */\
    _(TOK_OP_DOT, ".")__\
    _(TOK_OP_ASSIGN, "=")__\
    _(TOK_OP_ADD, "+")__\
    _(TOK_OP_SUB, "-")__\
    _(TOK_OP_MUL, "*")__\
    _(TOK_OP_POW, "**")__\
    _(TOK_OP_ADD_NO_OV, "!+")__\
    _(TOK_OP_SUB_NO_OV, "!-")__\
    _(TOK_OP_MUL_NO_OV, "!*")__  \
    _(TOK_OP_POW_NO_OV, "!**")__\
    _(TOK_OP_DIV, "/")__\
    _(TOK_OP_MOD, "%")__\
    _(TOK_OP_ADD_ASSIGN, "+=")__\
    _(TOK_OP_SUB_ASSIGN, "-=")__\
    _(TOK_OP_MUL_ASSIGN, "*=")__\
    _(TOK_OP_POW_ASSIGN, "**=")__\
    _(TOK_OP_ADD_ASSIGN_NO_OV, "!+=")__\
    _(TOK_OP_SUB_ASSIGN_NO_OV, "!-=")__\
    _(TOK_OP_MUL_ASSIGN_NO_OV, "!*=")__\
    _(TOK_OP_POW_ASSIGN_NO_OV, "!**=")__\
    _(TOK_OP_DIV_ASSIGN, "/=")__\
    _(TOK_OP_MOD_ASSIGN, "%=")__\
    _(TOK_OP_INC, "++")__\
    _(TOK_OP_DEC, "--")__\
    _(TOK_OP_EQUAL, "==")__\
    _(TOK_OP_NOT_EQUAL, "!=")__\
    _(TOK_OP_LESS, "<")__\
    _(TOK_OP_LESS_EQUAL, "<=")__\
    _(TOK_OP_GREATER, ">")__\
    _(TOK_OP_GREATER_EQUAL, ">=")__\
    _(TOK_OP_BIT_AND, "&")__\
    _(TOK_OP_BIT_OR, "|")__\
    _(TOK_OP_BIT_XOR, "^")__\
    _(TOK_OP_BIT_AND_ASSIGN, "&=")__\
    _(TOK_OP_BIT_OR_ASSIGN, "|=")__\
    _(TOK_OP_BIT_XOR_ASSIGN, "^=")__\
    _(TOK_OP_BIT_ASHL, "<<")__\
    _(TOK_OP_BIT_ASHR, ">>")__  \
    _(TOK_OP_BIT_ROL, "<<<")__\
    _(TOK_OP_BIT_ROR, ">>>")__\
    _(TOK_OP_BIT_LSHR, ">>>>")__\
    _(TOK_OP_BIT_ASHL_ASSIGN, "<<=")__\
    _(TOK_OP_BIT_ASHR_ASSIGN, ">>=")__ \
    _(TOK_OP_BIT_ROL_ASSIGN, "<<<=")__\
    _(TOK_OP_BIT_ROR_ASSIGN, ">>>=")__\
    _(TOK_OP_BIT_LSHR_ASSIGN, ">>>>=")__\
    _(TOK_OP_BIT_COMPL, "~")__\
    _(TOK_OP_LOG_AND, "and")__\
    _(TOK_OP_LOG_OR, "or")__\
    _(TOK_OP_LOG_NOT, "not")__\
    /* Meta */\
    _(TOK_ME_ERR, "ERROR")__\
    _(TOK_ME_EOF, "EOF")

#define _(_1, _2) _1
typedef enum {
    tkdef(_, NEO_SEP),
    TOK__COUNT
} toktype_t;
#undef _
neo_static_assert(TOK__COUNT <= 255);
#define KWR_START TOK_KW_METHOD /* First keyword token */
#define KWR_END TOK_KW_DO /* Last keyword token */
#define KWR_LEN (KWR_END-KWR_START+1)
neo_static_assert(KWR_START>=0 && KWR_END<TOK__COUNT && KWR_LEN>0 && KWR_LEN<=255 && KWR_END-KWR_START>0);

typedef struct srcspan_t {
    const uint8_t *p;
    uint32_t len;
} srcspan_t;
#define srcspan_from(str) ((srcspan_t){.p=(const uint8_t *)(str),.len=sizeof(str)-1})
#define srcspan_eq(a, b) ((a).len == (b).len && memcmp((a).p, (b).p, (a).len) == 0)
#define srcspan_hash(span) (neo_hash_x17((span).p, (span).len))
extern const uint8_t *srcspan_clone(srcspan_t span); /* Create null-terminated heap copy of source span. */

typedef enum radix_t {
    RADIX_BIN = 2, /* Literal Prefix: 0b */
    RADIX_OCT = 8, /* Literal Prefix: 0o */
    RADIX_DEC = 10, /* Literal Prefix: none */
    RADIX_HEX = 16 /* Literal Prefix: 0x */
} radix_t;

/* Represents a token. */
typedef struct token_t {
    toktype_t type : 8;
    radix_t radix : 8; /* Only used if type == TOK_LI_INT */
    uint32_t line; /* Line number of the start of the token. 1-based. */
    uint32_t col; /* Column number of the start of the token. 1-based. */
    srcspan_t lexeme;
    srcspan_t lexeme_line;
    const uint8_t *file;
} token_t;
extern NEO_EXPORT NEO_COLDPROC void token_dump(const token_t *self);

/* Represents the lexer context for a single source file. */
typedef struct lexer_t {
    const source_t *src_data;
    const uint8_t *src;
    const uint8_t *needle;
    const uint8_t *tok_start;
    const uint8_t *line_start;
    const uint8_t *line_end;
    uint32_t cp_curr;
    uint32_t cp_next;
    uint32_t line;
    uint32_t col;
} lexer_t;

#define KW_MAPPING_CUSTOM_N 5 /* Number of custom keyword mappings. Currently, 5: true, false, and, or, not */
extern const toktype_t KW_MAPPINGS[KW_MAPPING_CUSTOM_N];

extern NEO_EXPORT void lexer_init(lexer_t *self);
extern NEO_EXPORT void lexer_setup_source(lexer_t *self, const source_t *src);
extern NEO_EXPORT NEO_NODISCARD token_t lexer_scan_next(lexer_t *self);
extern NEO_EXPORT size_t lexer_drain(lexer_t *self, token_t **tok);
extern NEO_EXPORT void lexer_free(lexer_t *self);

#ifdef __cplusplus
}
#endif

#endif
