/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Parsing. LexicalTokens -> AST */

#ifndef NEO_PARSER_H
#define NEO_PARSER_H

#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Represents the parser context for a single source file. */
typedef struct parser_t {
    lexer_t lex; /* Lexer for the source file. */
    astpool_t pool; /* Memory pool for allocations. */
    token_t curr; /* Current token. */
    token_t prev; /* Previous token. */
    error_vector_t *errors; /* List of errors. */
    bool panic; /* Panic mode. */
    bool error; /* Error mode. */
} parser_t;

extern NEO_EXPORT void parser_init(parser_t *self, error_vector_t *errors);
extern NEO_EXPORT void parser_free(parser_t *self);
extern NEO_EXPORT NEO_NODISCARD astref_t parser_parse(parser_t *self);
extern NEO_EXPORT astref_t parser_drain(parser_t *self);
extern NEO_EXPORT void parser_setup_source(parser_t *self, const source_t *src);

extern NEO_EXPORT bool parse_int(const char *str, size_t len, neo_int_t *o);
extern NEO_EXPORT bool parse_float(const char *str, size_t len, neo_float_t *o);

#ifdef __cplusplus
}
#endif
#endif
