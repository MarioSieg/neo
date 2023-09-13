/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* High-level NEO compiler API for embedding/hosting etc. */

#ifndef NEO_COMPILER_H
#define NEO_COMPILER_H

#include "neo_ast.h"
#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compilation error codes. */
typedef enum error_type_t {
    COMERR_OK = 0, /* No error. */
    COMERR_INTERNAL_COMPILER_ERROR, /* Internal compiler error. */
    COMERR_SYNTAX_ERROR, /* Syntax error. */
    COMERR_SYMBOL_REDEFINITION, /* Symbol redefinition. */
    COMERR__LEN
} error_type_t;

/* Represents an error or warning emitted during static compilation from source to bytecode. */
typedef struct compile_error_t {
    error_type_t type;
    uint32_t line;
    uint32_t col;
    const uint8_t *lexeme;
    const uint8_t *lexeme_line;
    const uint8_t *file;
    const uint8_t *msg; /* Error-specific message. */
} compile_error_t;

/* Create error from lexer token.  */
extern NEO_COLDPROC const compile_error_t *comerror_new(
    error_type_t type,
    uint32_t line,
    uint32_t col,
    const uint8_t *lexeme,
    const uint8_t *lexeme_line,
    const uint8_t *file,
    const uint8_t *msg
);
extern NEO_COLDPROC const compile_error_t *comerror_from_token(error_type_t type, const token_t *tok, const uint8_t *msg);
extern NEO_COLDPROC void comerror_print(const compile_error_t *self, FILE *f, bool colored);
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
extern NEO_EXPORT NEO_COLDPROC void errvec_print(const error_vector_t *self, FILE *f, bool colored);
extern NEO_EXPORT NEO_COLDPROC void errvec_clear(error_vector_t *self);
extern NEO_EXPORT NEO_COLDPROC void errvec_free(error_vector_t *self);

typedef struct source_t {
    const uint8_t *filename; /* Encoded in UTF-8 and terminated. */
    const uint8_t *src; /* Encoded in UTF-8 and terminated. */
    bool is_file; /* True if source is loaded from file, false if from memory. */
    size_t len; /* In bytes. */
} source_t;

typedef enum source_load_error_t {
    SRCLOAD_OK = 0,
    SRCLOAD_INVALID_UTF8,
    SRCLOAD_FILE_NOT_FOUND,
    SRCLOAD_FILE_READ_ERROR
} source_load_error_t;

typedef struct source_load_error_info_t {
    source_load_error_t error;
    size_t invalid_utf8pos; /* Only filled if error == SRCLOAD_INVALID_UTF8. */
    neo_unicode_error_t unicode_error; /* Only filled if error == SRCLOAD_INVALID_UTF8. */
    size_t bytes_read; /* Only filled if error == SRCLOAD_FILE_READ_ERROR. */
} source_load_error_info_t;

/**
 * Load source code from UTF-8 file and validate source.
 * @param path The file path, which is cloned into self.
 * @return New instance or NULL on failure.
 */
extern NEO_EXPORT NEO_NODISCARD const source_t *source_from_file(const uint8_t *path, source_load_error_info_t *err_info);

/**
 * Load source code from memory.
 * @param path The file path, which is just referenced and not freed by self.
 * @param src The source code, which is just referenced and not freed by self.
 * @return
 */
extern NEO_EXPORT NEO_NODISCARD const source_t *source_from_memory_ref(const uint8_t *path, const uint8_t *src, source_load_error_info_t *err_info);
extern NEO_EXPORT bool source_is_empty(const source_t *self);
extern NEO_EXPORT void source_free(const source_t *self);
extern NEO_EXPORT NEO_COLDPROC void source_dump(const source_t *self, FILE *f);

/* Compiler option flags. */
typedef enum neo_compiler_flag_t {
    COM_FLAG_NONE = 0,
    COM_FLAG_DEBUG = 1 << 0,          /* Print debug messages. */
    COM_FLAG_DUMP_AST = 1 << 1,       /* Dump AST graphviz code to text file. */
    COM_FLAG_RENDER_AST = 1 << 2,     /* Render AST graphviz code to image file. */
    COM_FLAG_NO_STATUS = 1 << 3,      /* Don't print status messages. */
    COM_FLAG_NO_COLOR = 1 << 4,       /* Don't print colored messages. */
    COM_FLAG_NO_ERROR_DUMP = 1 << 5,  /* Don't print error dump. */
} neo_compiler_flag_t;

typedef void (neo_compile_callback_hook_t)(const source_t *src, neo_compiler_flag_t flags, void *user);

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
extern NEO_EXPORT NEO_HOTPROC bool compiler_compile(neo_compiler_t *self, const source_t *src, void *usr);

extern NEO_EXPORT const error_vector_t *compiler_get_errors(const neo_compiler_t *self);
extern NEO_EXPORT astref_t compiler_get_ast_root(const neo_compiler_t *self, const astpool_t **pool);
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