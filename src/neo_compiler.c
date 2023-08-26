/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <time.h>
#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"

const source_t *source_from_file(const uint8_t *path, source_load_error_info_t *err_info) {
    neo_dassert(path);
    FILE *f = NULL;
    if (neo_unlikely(!neo_fopen(&f, path, NEO_FMODE_R|NEO_FMODE_BIN))) {
        if (err_info) {
            err_info->error = SRCLOAD_FILE_NOT_FOUND;
        }
        return NULL;
    }
    /* Check for BOM and skip it if present */
    uint8_t bom[3];
    size_t bom_len = fread(bom, sizeof(*bom),  sizeof(bom), f);
    bool has_bom = false;
    if (bom_len == sizeof(bom) && memcmp(bom, "\xef\xbb\xbf", sizeof(bom)) == 0) {
        has_bom = true; /* BOM found */
    }
    else { rewind(f); } /* No BOM, rewind to start of file */
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    rewind(f);
    if (has_bom) {
        fseek(f, sizeof(bom), SEEK_SET); /* BOM detected, skip it */
        size -= sizeof(bom);
    }
    uint8_t *buf = neo_memalloc(NULL, size+2); /* +1 for \n +1 for \0 */
    size_t bytes_read = fread(buf, sizeof(*buf), size, f);
    if (neo_unlikely(bytes_read != size)) { /* Read file into buffer */
        neo_memalloc(buf, 0);
        fclose(f);
        if (err_info) {
            err_info->error = SRCLOAD_FILE_READ_ERROR;
            err_info->bytes_read = bytes_read;
        }
        return NULL; /* Failed to read all bytes or read error. */
    }
    fclose(f); /* Close file */
    /* Verify that the file is valid UTF-8. */
    size_t pos = 0;
    neo_unicode_error_t result = neo_utf8_validate(buf, size, &pos);
    if (result != NEO_UNIERR_OK) {
        neo_memalloc(buf, 0);
        if (err_info) {
            err_info->error = SRCLOAD_INVALID_UTF8;
            err_info->invalid_utf8pos = pos;
            err_info->unicode_error = result;
        }
        return false;
    }
#if NEO_OS_WINDOWS
    /* We read the file as binary file, so we need to replace \r\n by ourselves, fuck you Windows! */
    for (size_t i = 0; i < size; ++i) {
        if (buf[i] == '\r' && buf[i+1] == '\n') {
            buf[i] = '\n';
            memmove(buf+i+1, buf+i+2, size-i-2); /* Fold data downwards. */
            --size, --i;
        }
    }
#endif
    buf[size] = '\n'; /* Append final newline */
    buf[size+1] = '\0'; /* Append terminator */
    source_t *self = neo_memalloc(NULL, sizeof(*self));
    self->filename = neo_strdup2(path);
    self->src = buf;
    self->len = size+1; /* +1 for final newline. */
    if (err_info) {
        err_info->error = SRCLOAD_OK;
    }
    return self;
}

const source_t *source_from_memory(const uint8_t *path, const uint8_t *src) {
    if (neo_unlikely(!path || !src)) {
        return NULL;
    }
    source_t *self = neo_memalloc(NULL, sizeof(*self));
    self->filename = path;
    self->src = src;
    self->len = strlen((const char *)src);
    return self;
}

void source_free(const source_t *self) {
    neo_dassert(self);
    if (self->is_file) { /* Memory is owned when source was loaded from file, else referenced. */
        neo_memalloc((void *)self->filename, 0);
        neo_memalloc((void *)self->src, 0);
    }
    neo_memalloc((void *)self, 0);
}

struct neo_compiler_t {
    neo_mempool_t pool; /* Memory pool for allocations. */
    error_vector_t errors; /* List of errors and warnings. */
    parser_t parser; /* Parser state. */
    astref_t ast; /* Root of the AST. */
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
    neo_memalloc(*self, 0);
    *self = NULL;
}

bool compiler_compile(neo_compiler_t *self, const source_t *src, void *user) {
    neo_assert(self && "Compiler pointer is NULL");
    if (neo_unlikely(!src)) { return false; }
    clock_t begin = clock();
    if (self->pre_compile_callback) {
        (*self->pre_compile_callback)(src, self->flags, user);
    }
    errvec_clear(&self->errors);
    self->ast = ASTREF_NULL;
    parser_setup_source(&self->parser, src);
    self->ast = parser_drain(&self->parser);
   // astnode_validate(&self->parser.pool, self->ast);
    neo_assert(!astref_isnull(self->ast) && "Parser did not emit any AST");
    if (self->post_compile_callback) {
        (*self->post_compile_callback)(src, self->flags, user);
    }
    if (compiler_has_flags(self, COM_FLAG_RENDER_AST)) {
#ifdef NEO_EXTENSION_HAS_GRAPHVIZ
        size_t len = strlen((const char *)src->filename);
        if (!neo_utf8_is_ascii(src->filename, len)) {
            neo_error("Failed to render AST, filename is not ASCII.");
        } else {
            char *filename = alloca(len+sizeof("_ast.jpg"));
            memcpy(filename, src->filename, len);
            memcpy(filename+len, "_ast.jpg", sizeof("_ast.jpg")-1);
            filename[len+sizeof("_ast.jpg")-1] = '\0';
            ast_node_graphviz_render(&self->parser.pool, self->ast, filename);
        }
#else
        neo_error("Failed to render AST, Graphviz extension is not enabled.");
#endif
    }
    if (neo_unlikely(self->errors.len)) {
        neo_error("Compilation failed with %"PRIu32" errors.", self->errors.len);
        errvec_print(&self->errors, stderr);
    }
    double time_spent = (double)(clock()-begin)/CLOCKS_PER_SEC;
    if (!compiler_has_flags(self, COM_FLAG_NO_STATUS)) {
        printf("Compiled %s in %.3fms\n", src->filename, time_spent*1000.0); /* TODO: UTF-8 aware printf. */
    }
    return true;
}

const error_vector_t *compiler_get_errors(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return &self->errors;
}

astref_t compiler_get_ast_root(const neo_compiler_t *self) {
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
