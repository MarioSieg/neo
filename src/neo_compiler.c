/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <time.h>
#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"
#include "neo_bc.h"

static NEO_COLDPROC const uint8_t *clone_span(srcspan_t span) { /* Create null-terminated heap copy of source span. */
    uint8_t *p = neo_memalloc(NULL, (1+span.len)*sizeof(*p)); /* +1 for \0. */
    memcpy(p, span.p, span.len*sizeof(*p));
    p[span.len] = '\0';
    return p;
}

NEO_COLDPROC const compile_error_t *comerror_from_token(error_type_t type, const token_t *tok, const uint8_t *msg) {
    neo_assert(tok && "Token is NULL");
    neo_assert(msg && "Message is NULL");
    compile_error_t *error = neo_memalloc(NULL, sizeof(*error));
    error->type = type;
    error->line = tok->line;
    error->col = tok->col;
    error->lexeme = clone_span(tok->lexeme);
    error->lexeme_line = clone_span(tok->lexeme_line);
    error->file = neo_strdup2(tok->file);
    error->msg = neo_strdup2(msg);
    return error;
}

NEO_COLDPROC const compile_error_t *comerror_new(
    error_type_t type,
    uint32_t line,
    uint32_t col,
    const uint8_t *lexeme,
    const uint8_t *lexeme_line,
    const uint8_t *file,
    const uint8_t *msg
) {
    msg = msg ? msg : (const uint8_t *)"Unknown error";
    lexeme = lexeme ? lexeme : (const uint8_t *)"?";
    lexeme_line = lexeme_line ? lexeme_line : (const uint8_t *)"?";
    file = file ? file : (const uint8_t *)"?";
    compile_error_t *error = neo_memalloc(NULL, sizeof(*error));
    error->type = type;
    error->line = line;
    error->col = col;
    error->lexeme = neo_strdup2(lexeme);
    error->lexeme_line = neo_strdup2(lexeme_line);
    error->file = neo_strdup2(file);
    error->msg = neo_strdup2(msg);
    return error;
}

NEO_COLDPROC void comerror_free(const compile_error_t *self) {
    if (!self) { return; }
    neo_memalloc((void *)self->msg, 0);
    neo_memalloc((void *)self->file, 0);
    neo_memalloc((void *)self->lexeme_line, 0);
    neo_memalloc((void *)self->lexeme, 0);
    memset((void *)self, 0, sizeof(*self));
    neo_memalloc((void *)self, 0);
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

void errvec_print(const error_vector_t *self, FILE *f, bool colored) {
    neo_dassert(self && f);
    const char *color = colored ? NEO_CCRED : NULL;
    const char *reset = colored ? NEO_CCRESET : NULL;
    for (uint32_t i = 0; i < self->len; ++i) {
        const compile_error_t *e = self->p[i];
        fprintf(f, "%s:%"PRIu32":%"PRIu32": %s%s%s\n", e->file, e->line, e->col, color, e->msg, reset);
    }
}

void errvec_clear(error_vector_t *self) {
    neo_dassert(self);
    for (uint32_t i = 0; i < self->len; ++i) { /* Free individual errors. */
        comerror_free(self->p[i]);
        self->p[i] = NULL;
    }
    self->len = 0;
}

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
    if (neo_unlikely(result != NEO_UNIERR_OK)) {
        neo_memalloc(buf, 0);
        if (err_info) {
            err_info->error = SRCLOAD_INVALID_UTF8;
            err_info->invalid_utf8pos = pos;
            err_info->unicode_error = result;
        }
        return NULL;
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
    self->is_file = true;
    if (err_info) {
        err_info->error = SRCLOAD_OK;
    }
    return self;
}

const source_t *source_from_memory_ref(const uint8_t *path, const uint8_t *src, source_load_error_info_t *err_info) {
    if (neo_unlikely(!path || !src)) {
        return NULL;
    }
    size_t len = strlen((const char *)src);
    /* Verify that the src and path is valid UTF-8. */
    size_t pos = 0;
    neo_unicode_error_t result = neo_utf8_validate(src, len, &pos);
    if (neo_unlikely(result != NEO_UNIERR_OK)) {
        if (err_info) {
            err_info->error = SRCLOAD_INVALID_UTF8;
            err_info->invalid_utf8pos = pos;
            err_info->unicode_error = result;
        }
        return NULL;
    }
    result = neo_utf8_validate(path, strlen((const char *)src), &pos);
    if (neo_unlikely(result != NEO_UNIERR_OK)) {
        if (err_info) {
            err_info->error = SRCLOAD_INVALID_UTF8;
            err_info->invalid_utf8pos = pos;
            err_info->unicode_error = result;
        }
        return NULL;
    }
    source_t *self = neo_memalloc(NULL, sizeof(*self));
    self->filename = path;
    self->src = src;
    self->len = len;
    self->is_file = false;
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

void source_dump(const source_t *self, FILE *f) {
    neo_dassert(self != NULL && f != NULL);
    fprintf(f, "Source: %s\n", self->filename);
    fprintf(f, "Length: %"PRIu32"\n", (uint32_t)self->len);
    fprintf(f, "Content: %s\n", self->src);
    for (uint32_t i = 0; i < self->len; ++i) {
        fprintf(f, "\\x%02x", self->src[i]);
    }
    fputc('\n', f);
}

bool source_is_empty(const source_t *self) {
    neo_dassert(self);
    return self->len == 0 || *self->src == '\0' || *self->src == '\n';
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

static void print_status_msg(const neo_compiler_t *self, const char *color, const char *msg, ...) {
    if (compiler_has_flags(self, COM_FLAG_NO_STATUS)) { return; }
    va_list args;
    va_start(args, msg);
    if (compiler_has_flags(self, COM_FLAG_NO_COLOR) || !color) {
        vprintf(msg, args);
    } else {
        printf("%s", color);
        vprintf(msg, args);
        printf(NEO_CCRESET);
    }
    va_end(args);
    putchar('\n');
}

/* Reset compiler state and prepare for new compilation. */
static void compiler_reset_and_prepare(neo_compiler_t *self, const source_t *src) {
    errvec_clear(&self->errors);
    self->ast = ASTREF_NULL;
    parser_setup_source(&self->parser, src);
}

bool compiler_compile(neo_compiler_t *self, const source_t *src, void *user) {
    neo_assert(self && "Compiler pointer is NULL");
    if (neo_unlikely(!src)) { return false; }
    if (source_is_empty(src)) { return true; } /* Empty source -> we're done here. */
    clock_t begin = clock();
    if (self->pre_compile_callback) {
        (*self->pre_compile_callback)(src, self->flags, user);
    }
    compiler_reset_and_prepare(self, src);
    self->ast = parser_drain(&self->parser);
    neo_assert(!astref_isnull(self->ast) && "Parser did not emit any AST");
    if (self->post_compile_callback) {
        (*self->post_compile_callback)(src, self->flags, user);
    }
    if (compiler_has_flags(self, COM_FLAG_RENDER_AST)) {
#ifdef NEO_EXTENSION_AST_RENDERING
        size_t len = strlen((const char *)src->filename);
        if (!neo_utf8_is_ascii(src->filename, len)) {
            print_status_msg(self, NEO_CCRED, "Failed to render AST, filename is not ASCII.");
        } else {
            char *filename = alloca(len+sizeof("_ast.jpg"));
            memcpy(filename, src->filename, len);
            memcpy(filename+len, "_ast.jpg", sizeof("_ast.jpg")-1);
            filename[len+sizeof("_ast.jpg")-1] = '\0';
            ast_node_graphviz_render(&self->parser.pool, self->ast, filename);
        }
#else
        print_status_msg("Failed to render AST, Graphviz extension is not enabled.");
#endif
    }
    if (neo_unlikely(self->errors.len)) {
        print_status_msg(self, NEO_CCRED, "Compilation failed with %"PRIu32" error%s.", self->errors.len, self->errors.len > 1 ? "s" : "");
        errvec_print(&self->errors, stdout, !compiler_has_flags(self, COM_FLAG_NO_COLOR));
        return false;
    }
    double time_spent = (double)(clock()-begin)/CLOCKS_PER_SEC;
    if (!compiler_has_flags(self, COM_FLAG_NO_STATUS)) {
        print_status_msg(self, NULL, "Compiled '%s' in %.03fms\n", src->filename, time_spent*1000.0); /* TODO: UTF-8 aware printf. */
    }
    return true;
}

const error_vector_t *compiler_get_errors(const neo_compiler_t *self) {
    neo_assert(self && "Compiler pointer is NULL");
    return &self->errors;
}

astref_t compiler_get_ast_root(const neo_compiler_t *self, const astpool_t **pool) {
    neo_assert(self && pool && "Compiler pointer is NULL");
    *pool = &self->parser.pool;
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
