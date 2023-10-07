/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Parsing. LexicalTokens -> AST */

#ifndef NEO_PARSER_H
#define NEO_PARSER_H

#include "neo_ast.h"
#include "neo_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct error_vector_t;

/* Represents the parser context for a single source file. */
typedef struct parser_t {
    lexer_t lex; /* Lexer for the source file. */
    astpool_t pool; /* Memory pool for allocations. */
    token_t curr; /* Current token. */
    token_t prev; /* Previous token. */
    struct error_vector_t *errors; /* List of errors. */
    bool panic; /* Panic mode. */
    bool error; /* Error mode. */
    const char *prev_error; /* Previous error message. */
    uint32_t scope_depth; /* Scope depth. */
} parser_t;

extern NEO_EXPORT void parser_init(parser_t *self, struct error_vector_t *errors);
extern NEO_EXPORT void parser_free(parser_t *self);
extern NEO_EXPORT NEO_NODISCARD astref_t parser_parse(parser_t *self);
extern NEO_EXPORT astref_t parser_drain(parser_t *self);
extern NEO_EXPORT void parser_setup_source(parser_t *self, const struct source_t *src);

#ifdef __cplusplus
}
#endif
#endif
