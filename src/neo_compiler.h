/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_COMPILER_H
#define NEO_COMPILER_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct error_list_t {

} error_list_t;

/* Compiler option flags. */
typedef enum neo_compiler_flag_t {
    COM_FLAG_NONE = 0,
    COM_FLAG_DEBUG = 1<<0,        /* Print debug messages. */
    COM_FLAG_DUMP_AST = 1<<1,     /* Dump AST graphviz code to text file. */
    COM_FLAG_RENDER_AST = 1<<2,   /* Render AST graphviz code to image file. */
    COM_FLAG_SILENT = 1<<3,       /* Do not print any messages. */
    COM_FLAG_NO_STATUS = 1<<4,    /* Do not print status messages. */
    COM_FLAG_NO_AUTODUMP = 1<<5,  /* Do not dump error message on error. */
} neo_compiler_flag_t;

typedef void (*neo_compile_callback_hook_t)(const uint8_t *src, const uint8_t *filename, neo_compiler_flag_t flags, void *user);

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

#ifdef __cplusplus
}
#endif
#endif