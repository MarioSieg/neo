/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* LEXER (Lexical Analyzer) also known as Tokenizer. Source Code -> Lexical Tokens */

#ifndef NEO_LEXER_H
#define NEO_LEXER_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct source_t;

extern NEO_EXPORT uint32_t utf8_seqlen(uint32_t x); /* Returns the length in bytes of the UTF-8 sequence starting with x. */
extern NEO_EXPORT uint32_t utf8_decode(const uint8_t **p); /* Decode UTF-8 sequence into UTF-32 code-point and increment p. Assumes valid UTF-8. */

/* Token types. */
#define tkdef(_, __)\
    /* Keywords */\
    _(TOK_KW_FUNCTION, "func")__\
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
    _(TOK_LI_FALSE, "false")__ \
    _(TOK_LI_SELF, "self")__\
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
#define KWR_START TOK_KW_FUNCTION /* First keyword token */
#define KWR_END TOK_KW_DO /* Last keyword token */
#define KWR_LEN (KWR_END-KWR_START+1)
neo_static_assert(KWR_START>=0 && KWR_END<TOK__COUNT && KWR_LEN>0 && KWR_LEN<=255 && KWR_END-KWR_START>0);
extern NEO_EXPORT const char *tok_lexemes[TOK__COUNT];

/* Represents a span (also known as slice in the Rust world) of UTF-8 source code. */
typedef struct srcspan_t {
    const uint8_t *p; /* Start pointer. */
    uint32_t len; /* Length in bytes. So end would be: const uint8_t *end = p + len.  */
} srcspan_t;
#define srcspan_from(str) ((srcspan_t){.p=(const uint8_t *)(str),.len=sizeof(str)-1}) /* Create source span from string literal. */
#define srcspan_eq(a, b) ((a).len == (b).len && ((a).p == (b).p || memcmp((a).p, (b).p, (a).len) == 0)) /* Compare two source spans. */
#define srcspan_hash(span) (neo_hash_fnv1a((span).p, (span).len)) /* Hash source span. */
#define srcspan_stack_clone(span, var) /* Create null-terminated stack copy of source span using alloca. */\
    (var) = (uint8_t *)alloca((1+span.len)*sizeof(*(var))); /* +1 for \0. */\
    memcpy((var), span.p, span.len*sizeof(*(var)));\
    (var)[span.len] = '\0'
extern NEO_EXPORT const uint8_t *srcspan_heap_clone(srcspan_t span); /* Create null-terminated heap copy of source span. Don't forget to free the memory :) */

typedef enum radix_t {
    RADIX_BIN = 2, /* Literal Prefix: 0b */
    RADIX_OCT = 8, /* Literal Prefix: 0o */
    RADIX_DEC = 10,/* Literal Prefix: none */
    RADIX_HEX = 16,/* Literal Prefix: 0x */
    RADIX_UNKNOWN = 0
} radix_t;

/* Represents a token. */
typedef struct token_t {
    toktype_t type : 8; /* Token type. */
    radix_t radix : 8; /* Only used if type == TOK_LI_INT */
    uint32_t line; /* Line number of the start of the token. 1-based. */
    uint32_t col; /* Column number of the start of the token. 1-based. */
    srcspan_t lexeme; /* Source span of the token. */
    srcspan_t lexeme_line; /* Source span of the whole line containing the token. */
    const uint8_t *file; /* File name of the source file containing the token. */
} token_t;
extern NEO_EXPORT NEO_COLDPROC void token_dump(const token_t *self); /* Dump token to stdout. */

/*
** Represents the lexer context for a single source file.
** The lexer decodes the source file into UTF-32 code-points and scans for tokens.
** The UTF-8 to UTF-32 decoding is done lazily on the fly and each sequence is only decoded once, which is fast.
** Tokens are also not collected into a vector, but instead the lexer returns them one by one.
*/
typedef struct lexer_t {
    const struct source_t *src_data; /* Source file data. */
    const uint8_t *src; /* Source file data pointer. */
    const uint8_t *needle; /* Current position in source file data. */
    const uint8_t *tok_start; /* Start of current token. */
    const uint8_t *line_start; /* Start of current line. */
    const uint8_t *line_end; /* End of current line. */
    uint32_t cp_curr; /* Current decoded UTF-32 code-point. */
    uint32_t cp_next; /* Next decoded UTF-32 code-point. */
    uint32_t line; /* Current line number. 1-based. */
    uint32_t col; /* Current column number. 1-based. */
} lexer_t;

#define KW_MAPPING_CUSTOM_N 6 /* Number of custom keyword mappings. Currently, 5: true, false, and, or, not, self. */
extern const toktype_t KW_MAPPINGS[KW_MAPPING_CUSTOM_N];

extern NEO_EXPORT void lexer_init(lexer_t *self); /* Initialize lexer. */
extern NEO_EXPORT void lexer_setup_source(lexer_t *self, const struct source_t *src); /* Setup internal lexer state for source file processing. */
extern NEO_EXPORT NEO_NODISCARD token_t lexer_scan_next(lexer_t *self); /* Scan next token. */
extern NEO_EXPORT size_t lexer_drain(lexer_t *self, token_t **tok); /* Drain all tokens into a vector. */
extern NEO_EXPORT void lexer_free(lexer_t *self); /* Free lexer. */

#ifdef __cplusplus
}
#endif

#endif
