/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Parsing. LexicalTokens -> AST */

#ifndef NEO_PARSER_H
#define NEO_PARSER_H

#include "neo_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEPTH_LIM 16384u /* Max parse depth per block. */

/* Represents the parser context for a single source file. */
typedef struct parser_t {
    neo_mempool_t *pool; /* Memory pool for allocations. */
    token_t curr;
    token_t prev;
    bool panic;
    bool error;
} parser_t;

extern NEO_EXPORT bool parse_int(const char *str, size_t len, neo_int_t *o);
extern NEO_EXPORT bool parse_float(const char *str, size_t len, neo_float_t *o);

#ifdef __cplusplus
}
#endif
#endif
