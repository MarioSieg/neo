/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"

struct neo_compiler_t {
    const uint8_t *filename; /* Filename. */
    const uint8_t *source_code; /* Source code string. */
    neo_mempool_t pool; /* Memory pool for allocations. */
    error_vector_t errors; /* List of errors and warnings. */
    parser_t parser; /* Parser state. */
    astnode_t *ast; /* Root of the AST. */
    neo_compiler_flag_t flags; /* Compiler flags. */
    neo_compile_callback_hook_t *pre_compile_callback; /* Called before compilation. */
    neo_compile_callback_hook_t *post_compile_callback; /* Called after compilation. */
    neo_compile_callback_hook_t *on_warning_callback; /* Called on warning. */
    neo_compile_callback_hook_t *on_error_callback; /* Called on error. */
};

void compiler_init(neo_compiler_t **self, neo_compiler_flag_t flags) {
    neo_assert(self && "Compiler pointer is NULL");
    *self = neo_memalloc(NULL, sizeof(**self));
    memset(*self, 0, sizeof(**self));
    neo_mempool_init(&(**self).pool, 8192);
    errvec_init(&(**self).errors);
    parser_init(&(**self).parser, &(**self).errors);
    (**self).flags = flags;
}

void compiler_free(neo_compiler_t **self) {
    neo_assert(self && *self && "Compiler pointer is NULL");
    parser_free(&(**self).parser);
    errvec_free(&(**self).errors);
    neo_mempool_free(&(**self).pool);
    neo_memalloc((void *)(**self).filename, 0);
    neo_memalloc((void *)(**self).source_code, 0);
    neo_memalloc(*self, 0);
    *self = NULL;
}

bool compiler_compile(neo_compiler_t *self, const uint8_t *src, const uint8_t *filename, void *user) {
    neo_assert(self && "Compiler pointer is NULL");
    if (neo_unlikely(!src || !filename)) { return false; }
    (*self->pre_compile_callback)(src, filename, self->flags, user);
    self->source_code = src;
    self->filename = filename;
    self->ast = NULL;
    parser_prepare(&self->parser);
    /* TODO: compile */
    (*self->post_compile_callback)(src, filename, self->flags, user);
    return true;
}

const error_vector_t *compiler_get_errors(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return &self->errors;
}

const astnode_t *compiler_get_ast_root(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->ast;
}

neo_compiler_flag_t compiler_get_flags(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->flags;
}

bool compiler_has_flags(const neo_compiler_t *self, neo_compiler_flag_t flags) {
    return (compiler_get_flags(self) & flags) != 0;
}

neo_compile_callback_hook_t *compiler_get_pre_compile_callback(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->pre_compile_callback;
}

neo_compile_callback_hook_t *compiler_get_post_compile_callback(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->post_compile_callback;
}

neo_compile_callback_hook_t *compiler_get_on_warning_callback(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->on_warning_callback;
}

neo_compile_callback_hook_t *compiler_get_on_error_callback(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return self->on_error_callback;
}

void compiler_set_flags(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self && "Compiler pointer is NULL");
    self->flags = new_flags;
}

void compiler_add_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self && "Compiler pointer is NULL");
    self->flags |= new_flags;
}

void compiler_remove_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self && "Compiler pointer is NULL");
    self->flags &= ~new_flags;
}

void compiler_toggle_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self && "Compiler pointer is NULL");
    self->flags ^= new_flags;
}

void compiler_set_pre_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self && "Compiler pointer is NULL");
    neo_assert(new_hook && "Callback hook is NULL");
    self->pre_compile_callback = new_hook;
}

void compiler_set_post_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self && "Compiler pointer is NULL");
    neo_assert(new_hook && "Callback hook is NULL");
    self->post_compile_callback = new_hook;
}

void compiler_set_on_warning_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self && "Compiler pointer is NULL");
    neo_assert(new_hook && "Callback hook is NULL");
    self->on_warning_callback = new_hook;
}

void compiler_set_on_error_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self && "Compiler pointer is NULL");
    neo_assert(new_hook && "Callback hook is NULL");
    self->on_error_callback = new_hook;
}
