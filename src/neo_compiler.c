/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"

struct neo_compiler_t {
    const uint8_t *filename; /* Filename. */
    const uint8_t *source_code; /* Source code string. */
    neo_mempool_t pool; /* Memory pool for allocations. */
    error_list_t errors; /* List of errors and warnings. */
    lexer_t lexer; /* Lexer state. */
    parser_t parser; /* Parser state. */
    astnode_t *ast; /* Root of the AST. */
    neo_compiler_flag_t flags; /* Compiler flags. */
    neo_compile_callback_hook_t pre_compile_callback; /* Called before compilation. */
    neo_compile_callback_hook_t post_compile_callback; /* Called after compilation. */
    neo_compile_callback_hook_t on_warning_callback; /* Called on warning. */
    neo_compile_callback_hook_t on_error_callback; /* Called on error. */
};

void compiler_init(neo_compiler_t **self, neo_compiler_flag_t flags) {
    neo_assert(self && "Compiler pointer is NULL.");
    *self = neo_memalloc(NULL, sizeof(**self));
    memset(*self, 0, sizeof(**self));
    neo_mempool_init(&(**self).pool, 8192);
    lexer_init(&(**self).lexer);
    (**self).flags = flags;
}

void compiler_free(neo_compiler_t **self) {
    neo_assert(self && *self && "Compiler pointer is NULL.");
    lexer_free(&(**self).lexer);
    neo_mempool_free(&(**self).pool);
    neo_memalloc((void *)(**self).filename, 0);
    neo_memalloc((void *)(**self).source_code, 0);
    neo_memalloc(*self, 0);
    *self = NULL;
}

bool compiler_compile(neo_compiler_t *self, const uint8_t *src, const uint8_t *filename, void *user) {
    neo_assert(self && "Compiler pointer is NULL.");
    if (neo_unlikely(!src || !filename)) { return false; }
    (*self->pre_compile_callback)(src, filename, self->flags, user);
    self->source_code = src;
    self->filename = filename;
    self->ast = NULL;
    source_t source = { .src = src, .filename = filename, .len = strlen((const char *)src) };
    lexer_set_src(&self->lexer, &source);
    token_t *tok = NULL;
    size_t len = lexer_drain(&self->lexer, &tok);
    for (size_t i = 0; i < len; ++i) {
        const token_t *t = tok+i;
        token_dump(t);
    }
    (*self->post_compile_callback)(src, filename, self->flags, user);
    return true;
}

