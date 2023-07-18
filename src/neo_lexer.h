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

typedef struct {
    const uint8_t *filename;
    const uint8_t *src;
    size_t len;
} source_t;
extern NEO_EXPORT bool source_load(source_t *self, const uint8_t *path);

typedef struct {
    size_t src_len;
    const uint8_t *src;
    const uint8_t *needle;
    const uint8_t *tok_start;
    const uint8_t *line_start;
    uint32_t cp_curr;
    uint32_t cp_next;
    uint32_t line;
    uint32_t col;
} lexer_t;

extern NEO_EXPORT void lexer_set_src(lexer_t *self, const uint8_t *src, size_t src_len);

#ifdef __cplusplus
}
#endif

#endif