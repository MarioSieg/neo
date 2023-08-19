/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* High-level NEO compiler API for embedding/hosting etc. */

#ifndef NEO_COMPILER_H
#define NEO_COMPILER_H

#include "neo_ast.h"
#include "neo_core.h"
#include "neo_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compiler option flags. */
typedef enum neo_compiler_flag_t {
    COM_FLAG_NONE = 0,
    COM_FLAG_DEBUG = 1<<0,        /* Print debug messages. */
    COM_FLAG_DUMP_AST = 1<<1,     /* Dump AST graphviz code to text file. */
    COM_FLAG_RENDER_AST = 1<<2,   /* Render AST graphviz code to image file. */
} neo_compiler_flag_t;

typedef void (neo_compile_callback_hook_t)(const uint8_t *src, const uint8_t *filename, neo_compiler_flag_t flags, void *user);

/*
** Represents the Neo compiler. SOURCE CODE -> BYTE CODE.
** Turns text-based source code into binary-bytecode for the VM.
** Thread-local safe, so use one instance per thread when compiling code in parallel.
*/
typedef struct neo_compiler_t neo_compiler_t;
extern NEO_EXPORT void compiler_init(neo_compiler_t **self, neo_compiler_flag_t flags);
extern NEO_EXPORT void compiler_free(neo_compiler_t **self);

/**
** @brief Compiles the given source code into bytecode. The lifetime of @param src and @param filename must be longer than the compiler instance.
** @param self Compiler instance. Must be initialized with compiler_init().
** @param src Source code string.
** @param filename Filename of the source code.
** @return True if compilation was successful, false otherwise. See compiler_get_errors() for more information.
*/
extern NEO_EXPORT NEO_HOTPROC bool compiler_compile(neo_compiler_t *self, const uint8_t *src, const uint8_t *filename, void *user);

extern NEO_EXPORT const error_vector_t *compiler_get_errors(const neo_compiler_t *self);
extern NEO_EXPORT const astnode_t *compiler_get_ast_root(const neo_compiler_t *self);
extern NEO_EXPORT neo_compiler_flag_t compiler_get_flags(const neo_compiler_t *self);
extern NEO_EXPORT bool compiler_has_flags(const neo_compiler_t *self, neo_compiler_flag_t flags);
extern NEO_EXPORT neo_compile_callback_hook_t *compiler_get_pre_compile_callback(const neo_compiler_t *self);
extern NEO_EXPORT neo_compile_callback_hook_t *compiler_get_post_compile_callback(const neo_compiler_t *self);
extern NEO_EXPORT neo_compile_callback_hook_t *compiler_get_on_warning_callback(const neo_compiler_t *self);
extern NEO_EXPORT neo_compile_callback_hook_t *compiler_get_on_error_callback(const neo_compiler_t *self);
extern NEO_EXPORT void compiler_set_flags(neo_compiler_t *self, neo_compiler_flag_t new_flags);
extern NEO_EXPORT void compiler_add_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags);
extern NEO_EXPORT void compiler_remove_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags);
extern NEO_EXPORT void compiler_toggle_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags);
extern NEO_EXPORT void compiler_set_pre_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook);
extern NEO_EXPORT void compiler_set_post_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook);
extern NEO_EXPORT void compiler_set_on_warning_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook);
extern NEO_EXPORT void compiler_set_on_error_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook);

#ifdef __cplusplus
}
#endif
#endif