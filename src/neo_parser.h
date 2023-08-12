/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Parsing. LexicalTokens -> AST */

#ifndef NEO_PARSER_H
#define NEO_PARSER_H

#include "neo_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NEO_EXPORT bool parse_int(const char *str, size_t len, neo_int_t *o);
extern NEO_EXPORT bool parse_float(const char *str, size_t len, neo_float_t *o);

#ifdef __cplusplus
}
#endif
#endif
