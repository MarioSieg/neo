/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_LEX_H
#define NEO_LEX_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** Defines the list of lexer tokens.
** TODO: Update token ranges below when tokens are added/removed.
*/
#define INJECT_TOKEN_TYPES(_, __)\
	/* KW_* = Keyword tokens. */\
	_(TOK_KW_METHOD		        , "method")__\
	_(TOK_KW_LET			    , "let")__\
	_(TOK_KW_NEW			    , "new")__\
	_(TOK_KW_END			    , "end")__\
	_(TOK_KW_THEN			    , "then")__\
	_(TOK_KW_IF			        , "if")__\
	_(TOK_KW_ELSE			    , "else")__\
	_(TOK_KW_RETURN		        , "return")__\
    _(TOK_KW_CLASS              , "class")__ \
    _(TOK_KW_MODULE             , "module")__\
    _(TOK_KW_BREAK              , "break")__\
	_(TOK_KW_CONTINUE           , "continue")__\
    _(TOK_KW_WHILE              , "while")__\
    _(TOK_KW_STATIC             , "static")__\
    _(TOK_KW_DO                 , "do")__\
	_(TOK_KW_AS					, "as")__\
     \
	/* LI_* = Literal tokens. */\
	_(TOK_LI_IDENT			    , "<ident>")__\
	_(TOK_LI_INT			    , "<int>")__\
	_(TOK_LI_FLOAT			    , "<float>")__\
	_(TOK_LI_STRING		        , "<string>")__\
    _(TOK_LI_TRUE               , "true")__\
	_(TOK_LI_FALSE              , "false")__\
	\
	/* PU_* = Punctuation tokens. */\
	_(TOK_PU_L_PAREN		    , "(")__\
	_(TOK_PU_R_PAREN		    , ")")__\
	_(TOK_PU_L_BRACKET		    , "[")__\
	_(TOK_PU_R_BRACKET		    , "]")__\
	_(TOK_PU_L_BRACE		    , "{")__\
	_(TOK_PU_R_BRACE		    , "}")__\
	_(TOK_PU_COMMA			    , ",")__\
    _(TOK_PU_ARROW              , "->")__\
	\
	/* OP_* = Operator tokens. */\
    _(TOK_OP_DOT                , ".")__\
	_(TOK_OP_ASSIGN		  		, "=")__\
    _(TOK_OP_ADD			    , "+")__\
	_(TOK_OP_SUB			    , "-")__\
	_(TOK_OP_MUL			    , "*")__\
	_(TOK_OP_ADD_NO_OV			, "!+")/* add without overflow check (integer only) */__\
	_(TOK_OP_SUB_NO_OV			, "!-")/* subtract without overflow check (integer only) */__\
	_(TOK_OP_MUL_NO_OV			, "!*")/* multiply without overflow check (integer only) */__\
	_(TOK_OP_DIV			    , "/")__\
	_(TOK_OP_MOD			    , "%")__\
	_(TOK_OP_POW			    , "**")__\
	_(TOK_OP_ADD_ASSIGN	        , "+=")__\
	_(TOK_OP_SUB_ASSIGN	        , "-=")__\
	_(TOK_OP_MUL_ASSIGN	        , "*=")__\
	_(TOK_OP_ADD_NO_OV_ASSIGN	, "!+=")/* add assign without overflow check (integer only) */__\
	_(TOK_OP_SUB_NO_OV_ASSIGN	, "!-=")/* subtract assign without overflow check (integer only) */__\
	_(TOK_OP_MUL_NO_OV_ASSIGN	, "!*=")/* multiply assign without overflow check (integer only) */__\
	_(TOK_OP_DIV_ASSIGN	        , "/=")__\
	_(TOK_OP_MOD_ASSIGN	        , "%=")__\
	_(TOK_OP_POW_ASSIGN	        , "**=")__\
	_(TOK_OP_INC			    , "++")__\
	_(TOK_OP_DEC			    , "--")__\
	_(TOK_OP_EQUAL			    , "==")__\
	_(TOK_OP_NOT_EQUAL		    , "!=")__\
	_(TOK_OP_LESS			    , "<")__\
	_(TOK_OP_LESS_EQUAL	        , "<=")__\
	_(TOK_OP_GREATER		    , ">")__\
	_(TOK_OP_GREATER_EQUAL	    , ">=")__\
	\
	_(TOK_OP_BIT_AND		    , "&")/* bitwise and */__\
	_(TOK_OP_BIT_OR		        , "|")/* bitwise or */__\
	_(TOK_OP_BIT_XOR		    , "^")/* bitwise xor */__\
	_(TOK_OP_BIT_ASHL		    , "<<")/* bitwise arithmetic left shift */__\
	_(TOK_OP_BIT_ASHR		    , ">>")/* bitwise arithmetic right shift */__\
	_(TOK_OP_BIT_LSHR		    , ">>>")/* bitwise logical right shift */__\
	_(TOK_OP_BIT_AND_ASSIGN     , "&=")/* bitwise and assign */__\
	_(TOK_OP_BIT_OR_ASSIGN	    , "|=")/* bitwise or assign */__\
	_(TOK_OP_BIT_XOR_ASSIGN     , "^=")/* bitwise xor assign */__\
	_(TOK_OP_BIT_ASHL_ASSIGN    , "<<=")/* bitwise arithmetic left shift assign */__\
	_(TOK_OP_BIT_ASHR_ASSIGN    , ">>=")/* bitwise arithmetic right shift assign */__\
	_(TOK_OP_BIT_LSHR_ASSIGN    , ">>>=")/* bitwise logical right shift assign */__\
    _(TOK_OP_BIT_COMPL		    , "~")/* bitwise complement */__\
	\
    _(TOK_OP_LOG_AND            , "and")/* logical and */__\
    _(TOK_OP_LOG_OR             , "or")	/* logical or */__\
    _(TOK_OP_LOG_NOT			, "not")/* logical not */__\
	\
	/* ME_* = Meta tokens. */\
	_(TOK_ME_NL					, "\\n")__\
	_(TOK_ME_ERR			    , "ERROR")__\
	_(TOK_ME_EOF			    , "EOF")

#define _(_1, _2) _1
typedef enum {
	INJECT_TOKEN_TYPES(_, NEO_ENUM_SEP),
	TOK_COUNT_,
	/* These token ranges must be updated when tokens are added/removed. */
	TOK_RNG_KW_START = TOK_KW_METHOD,
	TOK_RNG_KW_END = TOK_KW_AS,

	TOK_RNG_LI_START = TOK_LI_IDENT,
	TOK_RNG_LI_END = TOK_LI_FALSE,

	TOK_RNG_PU_START = TOK_PU_L_PAREN,
	TOK_RNG_PU_END = TOK_PU_ARROW,

	TOK_RNG_OP_START = TOK_OP_DOT,
	TOK_RNG_OP_END = TOK_OP_LOG_NOT,

	TOK_RNG_ME_START = TOK_ME_NL,
	TOK_RNG_ME_END = TOK_ME_EOF,
} token_type_t;
neo_static_assert(TOK_COUNT_ <= 255);
neo_static_assert(TOK_RNG_ME_END <= 255);
#undef _

typedef enum {
	LI_RADIX_DEC = 10, /* Literal prefix: None. */
	LI_RADIX_HEX = 16, /* Literal prefix: 0x. */
	LI_RADIX_BIN = 2, /* Literal prefix: 0b. */
	LI_RADIX_OCT = 8, /* Literal prefix: 0c. */
} literal_radix_t;

typedef struct {
	const uint8_t *p;
	uint32_t len;
} lex_span_t;

typedef struct {
	token_type_t type : 8;
	literal_radix_t radix : 8;
	lex_span_t value;
	uint32_t line_no;
	lex_span_t line;
	uint32_t col_no;
} token_t;

#ifdef __cplusplus
}
#endif

#endif
