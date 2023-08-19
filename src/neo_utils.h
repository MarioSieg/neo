/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_UTILS_H
#define NEO_UTILS_H

#include "neo_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Represents an error or warning emitted during static compilation from source to bytecode. */
typedef struct compile_error_t {
    uint32_t line;
    uint32_t col;
    const uint8_t *lexeme;
    const uint8_t *lexeme_line;
    const uint8_t *file;
    const char *msg; /* Error-specific message. */
} compile_error_t;

/* Create error from lexer token.  */
extern NEO_COLDPROC const compile_error_t *comerror_new(
    uint32_t line,
    uint32_t col,
    const uint8_t *lexeme,
    const uint8_t *lexeme_line,
    const uint8_t *file,
    const char *msg
);
extern NEO_COLDPROC const compile_error_t *comerror_from_token(const token_t *tok, const char *msg);
extern NEO_COLDPROC void comerror_free(const compile_error_t *self);

/* Collection of all errors from all phases emitted during a single source file compilation. */
typedef struct error_vector_t {
    const compile_error_t **p;
    uint32_t len;
    uint32_t cap;
} error_vector_t;

#define errvec_isempty(self) (!!((self).len))
extern NEO_EXPORT NEO_COLDPROC void errvec_init(error_vector_t *self);
extern NEO_EXPORT NEO_COLDPROC void errvec_push(error_vector_t *self, const compile_error_t* error);
extern NEO_EXPORT NEO_COLDPROC void errvec_free(error_vector_t *self);

#ifdef __cplusplus
}
#endif
#endif
