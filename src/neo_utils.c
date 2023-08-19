/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_utils.h"

static NEO_COLDPROC const uint8_t *clone_span(srcspan_t span) { /* Create null-terminated heap copy of source span. */
    uint8_t *p = neo_memalloc(NULL, (1+span.len)*sizeof(*p)); /* +1 for \0. */
    memcpy(p, span.p, span.len*sizeof(*p));
    p[span.len] = '\0';
    return p;
}

NEO_COLDPROC const compile_error_t *comerror_from_token(const token_t *tok, const char *msg) {
    neo_assert(tok && "Token is NULL");
    neo_assert(msg && "Message is NULL");
    compile_error_t *error = neo_memalloc(NULL, sizeof(*error));
    error->line = tok->line;
    error->col = tok->col;
    error->lexeme = clone_span(tok->lexeme);
    error->lexeme_line = clone_span(tok->lexeme_line);
    error->file = neo_strdup2(tok->file);
    error->msg = neo_strdup(msg);
    return error;
}

NEO_COLDPROC const compile_error_t *comerror_new(
    uint32_t line,
    uint32_t col,
    const uint8_t *lexeme,
    const uint8_t *lexeme_line,
    const uint8_t *file,
    const char *msg
) {
    msg = msg ? msg : "Unknown error";
    lexeme = lexeme ? lexeme : (const uint8_t *)"?";
    lexeme_line = lexeme_line ? lexeme_line : (const uint8_t *)"?";
    file = file ? file : (const uint8_t *)"?";
    compile_error_t *error = neo_memalloc(NULL, sizeof(*error));
    error->line = line;
    error->col = col;
    error->lexeme = neo_strdup2(lexeme);
    error->lexeme_line = neo_strdup2(lexeme_line);
    error->file = neo_strdup2(file);
    error->msg = neo_strdup(msg);
    return error;
}

NEO_COLDPROC void comerror_free(const compile_error_t *self) {
    if (!self) { return; }
    neo_memalloc((void *)self->msg, 0);
    neo_memalloc((void *)self->file, 0);
    neo_memalloc((void *)self->lexeme_line, 0);
    neo_memalloc((void *)self->lexeme, 0);
    memset((void *)self, 0, sizeof(*self));
}

NEO_COLDPROC void errvec_init(error_vector_t *self) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
}

NEO_COLDPROC void errvec_push(error_vector_t *self, const compile_error_t *error) {
    neo_dassert(self);
    if (!self->cap) {
        self->len = 0;
        self->cap = 1<<7;
        self->p = neo_memalloc(self->p, self->cap*sizeof(*self->p));
    } else if (self->len >= self->cap) {
        self->p = neo_memalloc(self->p, (self->cap<<=1)*sizeof(*self->p));
    }
    self->p[self->len++] = error;
}

NEO_COLDPROC void errvec_free(error_vector_t *self) {
    neo_dassert(self);
    for (uint32_t i = 0; i < self->len; ++i) { /* Free individual errors. */
        comerror_free(self->p[i]);
        self->p[i] = NULL;
    }
    neo_memalloc(self->p, 0); /* Free error list. */
}
