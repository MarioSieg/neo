/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <time.h>
#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"

/*
** This file is included into C++ from the unit tests, using raw #include <file.c>, to test all internals without exposing them.
** That's why there are some redundant casts from void* to T* e.g: ... = (uint8_t *)neo_memalloc(...),
** which are needed in C++ but not in C.
*/

/* Forward declarations. */
static bool perform_semantic_analysis(astpool_t *pool, astref_t root, error_vector_t *errors);

static NEO_COLDPROC const uint8_t *clone_span(srcspan_t span) { /* Create null-terminated heap copy of source span. */
    uint8_t *p = (uint8_t *)neo_memalloc(NULL, (1+span.len)*sizeof(*p)); /* +1 for \0. */
    if (span.p != NULL) {
        memcpy(p, span.p, span.len*sizeof(*p));
    }
    p[span.len] = '\0';
    return p;
}

NEO_COLDPROC const compile_error_t *comerror_from_token(error_type_t type, const token_t *tok, const uint8_t *msg) {
    neo_assert(tok != NULL, "Token is NULL");
    neo_assert(msg != NULL, "Message is NULL");
    compile_error_t *error = (compile_error_t *)neo_memalloc(NULL, sizeof(*error));
    error->type = type;
    error->line = tok->line;
    error->col = tok->col;
    error->lexeme = clone_span(tok->lexeme);
    error->lexeme_line = clone_span(tok->lexeme_line);
    error->file = neo_strdup2(tok->file, NULL);
    error->msg = neo_strdup2(msg, NULL);
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
    compile_error_t *error = (compile_error_t *)neo_memalloc(NULL, sizeof(*error));
    error->type = type;
    error->line = line;
    error->col = col;
    error->lexeme = neo_strdup2(lexeme, NULL);
    error->lexeme_line = neo_strdup2(lexeme_line, NULL);
    error->file = neo_strdup2(file, NULL);
    error->msg = neo_strdup2(msg, NULL);
    return error;
}

void comerror_print(const compile_error_t *self, FILE *f, bool colored) {
    neo_dassert(self != NULL && f != NULL, "Invalid arguments");
    char error_message [0x1000];
    bool src_hint = false; /* Print source hint? */
    const char *color = colored ? NEO_CCRED : NULL;
    const char *reset = colored ? NEO_CCRESET : NULL;
    switch (self->type) {
        case COMERR_OK: return;
        case COMERR_INTERNAL_COMPILER_ERROR:
            snprintf(error_message, sizeof(error_message), "Fatal internal compiler error: %s%s.%s", color, self->msg, reset);
            break;
        case COMERR_SYNTAX_ERROR:
            snprintf(error_message, sizeof(error_message), "Syntax error: %s%s.%s", color, self->msg, reset);
            src_hint = true;
            break;
        case COMERR_SYMBOL_REDEFINITION:
            snprintf(error_message, sizeof(error_message), "Identifier is already used in this scope: %s%s.%s", color, self->msg, reset);
            src_hint = true;
            break;
        case COMERR_INVALID_EXPRESSION:
            snprintf(error_message, sizeof(error_message), "Invalid expression: %s%s.%s", color, self->msg, reset);
            src_hint = true;
            break;
        case COMERR_TYPE_MISMATCH:
            snprintf(error_message, sizeof(error_message), "Type mismatch: %s%s.%s", color, self->msg, reset);
            src_hint = true;
            break;
        case COMERR__LEN: neo_unreachable();
    }
    fprintf(
        f,
        "%s:%" PRIu32 ":%" PRIu32 ": %s\n",
        self->file,
        self->line,
        self->col,
        error_message
    ); /* Print file, line and column. */
    /* Print source hint message. */
    if (src_hint) {
        fprintf(f, "%s%s%s\n", color, self->lexeme_line, reset);
        for (uint32_t i = 1; i < self->col; ++i) { /* Print spaces until column. */
            fputc(' ', f);
        }
        fputs(color, f);
        for (uint32_t i = 0; i < strlen((const char *)self->lexeme); ++i) { /* Print underline. */
            fputc('^', f);
        }
        fputs(reset, f);
        fputc('\n', f);
    }
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
    neo_dassert(self != NULL, "self is NULL");
    memset(self, 0, sizeof(*self));
}

NEO_COLDPROC void errvec_push(error_vector_t *self, const compile_error_t *error) {
    neo_dassert(self != NULL, "self is NULL");
    if (!self->cap) {
        self->len = 0;
        self->cap = 1<<7;
        self->p = (const compile_error_t **)neo_memalloc(self->p, self->cap*sizeof(*self->p));
    } else if (self->len >= self->cap) {
        self->p = (const compile_error_t **)neo_memalloc(self->p, (self->cap<<=1)*sizeof(*self->p));
    }
    self->p[self->len++] = error;
}

NEO_COLDPROC void errvec_free(error_vector_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    for (uint32_t i = 0; i < self->len; ++i) { /* Free individual errors. */
        comerror_free(self->p[i]);
        self->p[i] = NULL;
    }
    neo_memalloc(self->p, 0); /* Free error list. */
}

void errvec_print(const error_vector_t *self, FILE *f, bool colored) {
    neo_dassert(self != NULL && f != NULL, "Invalid arguments");
    for (uint32_t i = 0; i < self->len; ++i) {
        comerror_print(self->p[i], f, colored);
    }
}

void errvec_clear(error_vector_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    for (uint32_t i = 0; i < self->len; ++i) { /* Free individual errors. */
        comerror_free(self->p[i]);
        self->p[i] = NULL;
    }
    if (self->p != NULL) {
        memset(self->p, 0, self->cap*sizeof(*self->p)); /* Reset error list. */
    }
    self->len = 0;
}

const source_t *source_from_file(const uint8_t *path, source_load_error_info_t *err_info) {
    neo_dassert(path != NULL, "path is NULL");
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
    uint8_t *buf = (uint8_t *)neo_memalloc(NULL, size+2); /* +1 for \n +1 for \0 */
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
    source_t *self = (source_t *)neo_memalloc(NULL, sizeof(*self));
    self->filename = neo_strdup2(path, NULL);
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
    source_t *self = (source_t *)neo_memalloc(NULL, sizeof(*self));
    self->filename = path;
    self->src = src;
    self->len = len;
    self->is_file = false;
    return self;
}

void source_free(const source_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    if (self->is_file) { /* Memory is owned when source was loaded from file, else referenced. */
        neo_memalloc((void *)self->filename, 0);
        neo_memalloc((void *)self->src, 0);
    }
    neo_memalloc((void *)self, 0);
}

void source_dump(const source_t *self, FILE *f) {
    neo_dassert(self != NULL && f != NULL, "Invalid arguments");
    fprintf(f, "Source: %s\n", self->filename);
    fprintf(f, "Length: %" PRIu32 "\n", (uint32_t)self->len);
    fprintf(f, "Content: %s\n", self->src);
    for (uint32_t i = 0; i < self->len; ++i) {
        fprintf(f, "\\x%02x", self->src[i]);
    }
    fputc('\n', f);
}

bool source_is_empty(const source_t *self) {
    neo_dassert(self != NULL, "self is NULL");
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
    neo_assert(self != NULL, "Compiler pointer is NULL");
    *self = (neo_compiler_t *)neo_memalloc(NULL, sizeof(**self));
    memset(*self, 0, sizeof(**self));
    neo_mempool_init(&(**self).pool, 8192);
    errvec_init(&(**self).errors);
    parser_init(&(**self).parser, &(**self).errors);
    (**self).flags = flags;
}

void compiler_free(neo_compiler_t **self) {
    neo_assert(self != NULL && *self != NULL, "Compiler pointer is NULL");
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
    neo_dassert(self->parser.lex.src == src->src, "Source mismatch");
}

static void render_ast(neo_compiler_t *self, const source_t *src) {
#ifdef NEO_EXTENSION_AST_RENDERING
    size_t len = strlen((const char *)src->filename);
    if (!neo_utf8_is_ascii(src->filename, len)) {
        print_status_msg(self, NEO_CCRED, "Failed to render AST, filename is not ASCII.");
    } else {
        char *filename = (char *)alloca(len+sizeof("_ast.jpg"));
        memcpy(filename, src->filename, len);
        memcpy(filename+len, "_ast.jpg", sizeof("_ast.jpg")-1);
        filename[len+sizeof("_ast.jpg")-1] = '\0';
        ast_node_graphviz_render(&self->parser.pool, self->ast, filename);
    }
#else
    print_status_msg(self, NEO_CCRED, "Failed to render AST, Graphviz extension is not enabled.");
#endif
}

static NEO_NODISCARD bool compile_module(neo_compiler_t *self, const source_t *src) {
    neo_dassert(self != NULL && src != NULL, "Invalid arguments");
    compiler_reset_and_prepare(self, src); /* 1. Reset compiler state and prepare for new compilation. */
    self->ast = parser_drain(&self->parser); /* 2. Parse source code into AST. */
    neo_assert(!astref_isnull(self->ast), "Parser did not emit any AST");
    if (neo_unlikely(!perform_semantic_analysis(&self->parser.pool, self->ast, &self->errors))) { /* 3. Perform semantic analysis. */
        return false;
    }
    /* 4. Generate bytecode. */
    return self->errors.len == 0;
}

#define invoke_opt_hook(hook, ...) \
    if ((hook) != NULL) { \
        (*(hook))(__VA_ARGS__); \
    }

bool compiler_compile(neo_compiler_t *self, const source_t *src, void *usr) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    if (neo_unlikely(!src)) { return false; }
    if (source_is_empty(src)) { return true; } /* Empty source -> we're done here. */
    clock_t begin = clock();
    invoke_opt_hook(self->pre_compile_callback, src, self->flags, usr);
    bool success = compile_module(self, src);
    invoke_opt_hook(self->post_compile_callback, src, self->flags, usr);
    if (neo_unlikely(!success)) { /* Compilation failed. */
        print_status_msg(
            self,
            NEO_CCRED,
            "Compilation failed with %" PRIu32 " error%s.",
            self->errors.len,
            self->errors.len > 1 ? "s" : ""
        );
        if (!compiler_has_flags(self, COM_FLAG_NO_ERROR_DUMP)) {
            errvec_print(&self->errors, stdout, !compiler_has_flags(self, COM_FLAG_NO_COLOR));
        }
        invoke_opt_hook(self->on_error_callback, src, self->flags, usr);
        invoke_opt_hook(self->on_warning_callback, src, self->flags, usr);
        return false;
    }
    if (neo_unlikely(compiler_has_flags(self, COM_FLAG_RENDER_AST))) {
        render_ast(self, src);
    }
    double time_spent = (double)(clock()-begin)/CLOCKS_PER_SEC;
    if (!compiler_has_flags(self, COM_FLAG_NO_STATUS)) {
        print_status_msg(self, NULL, "Compiled '%s' in %.03fms\n", src->filename, time_spent*1000.0); /* TODO: UTF-8 aware printf. */
    }
    return true;
}

const error_vector_t *compiler_get_errors(const neo_compiler_t *self) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    return &self->errors;
}

astref_t compiler_get_ast_root(const neo_compiler_t *self, const astpool_t **pool) {
    neo_assert(self != NULL && pool != NULL, "Compiler pointer is NULL");
    *pool = &self->parser.pool;
    return self->ast;
}

neo_compiler_flag_t compiler_get_flags(const neo_compiler_t *self) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    return self->flags;
}

bool compiler_has_flags(const neo_compiler_t *self, neo_compiler_flag_t flags) {
    return (compiler_get_flags(self) & flags) != 0;
}

neo_compile_callback_hook_t *compiler_get_pre_compile_callback(const neo_compiler_t *self) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    return self->pre_compile_callback;
}

neo_compile_callback_hook_t *compiler_get_post_compile_callback(const neo_compiler_t *self) {
    neo_assert(self!= NULL, "Compiler pointer is NULL");
    return self->post_compile_callback;
}

neo_compile_callback_hook_t *compiler_get_on_warning_callback(const neo_compiler_t *self) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    return self->on_warning_callback;
}

neo_compile_callback_hook_t *compiler_get_on_error_callback(const neo_compiler_t *self) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    return self->on_error_callback;
}

void compiler_set_flags(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    self->flags = new_flags;
}

void compiler_add_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    self->flags = (neo_compiler_flag_t)(self->flags | new_flags);
}

void compiler_remove_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    self->flags = (neo_compiler_flag_t)(self->flags & ~new_flags);
}

void compiler_toggle_flag(neo_compiler_t *self, neo_compiler_flag_t new_flags) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    self->flags = (neo_compiler_flag_t)(self->flags ^ new_flags);
}

void compiler_set_pre_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    neo_assert(new_hook != NULL, "Callback hook is NULL");
    self->pre_compile_callback = new_hook;
}

void compiler_set_post_compile_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    neo_assert(new_hook != NULL, "Callback hook is NULL");
    self->post_compile_callback = new_hook;
}

void compiler_set_on_warning_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    neo_assert(new_hook != NULL, "Callback hook is NULL");
    self->on_warning_callback = new_hook;
}

void compiler_set_on_error_callback(neo_compiler_t *self, neo_compile_callback_hook_t *new_hook) {
    neo_assert(self != NULL, "Compiler pointer is NULL");
    neo_assert(new_hook != NULL, "Callback hook is NULL");
    self->on_error_callback = new_hook;
}

/*
** The semantic analysis needs to check for the following things:
**  Variable Declaration and Scope:
**      Ensure that variables are declared before they are used.
**      Check that variables are not redeclared within the same scope.
**      Implement a scoping mechanism and ensure that variables are accessible within their respective scopes.
**  Type Checking:
**      Verify that expressions and operands have compatible types. For example, you should not be able to add a string to an integer.
**      Enforce type compatibility rules for assignments, function parameters, and return values.
**      Detect type mismatches and report appropriate error messages.
**  Function and Method Calls:
**      Check that functions and methods are called with the correct number and types of arguments.
**      Verify that functions and methods return values of the expected types.
**  Control Structures:
**      Ensure that conditional statements (if, else) have boolean expressions as conditions.
**      Verify that loop control expressions (for, while) are boolean.
**      Check for proper nesting and scope handling within control structures.
**  Array and Indexing Operations:
**      Ensure that array or list indices are of integer type.
**      Check that array or list indices are within bounds.
**  Type Declarations:
**      Validate custom type declarations and ensure they adhere to language rules.
**      Check for cyclic type dependencies.
**  Function Signatures and Overloading:
**      Handle function and method overloading if your language supports it.
**      Check that functions with the same name have different parameter lists.
**  Error Handling:
**      Report meaningful error messages with location information for easier debugging.
**      Detect and handle exceptions or runtime errors if your language supports them.
**  Constant Values:
**      Ensure that constant values (e.g., literals) have valid types and values.
**  Type Inference:
**      Implement type inference algorithms to infer types when not explicitly provided by the programmer.
**  Built-in Functions and Libraries:
**      Ensure that built-in functions and libraries are available and correctly used.
**  Annotations and Attributes:
**      Support annotations or attributes to provide additional information for the compiler or runtime.
**
** ALREADY DONE:
** -> Check for redefinition of symbols.
** The parser already checks this because the symbol tables are built in the same step.
*/

static NEO_COLDPROC void node_block_dump_symbols(const node_block_t *self, FILE *f);

typedef enum typeid_t {
    TYPEID_INT,
    TYPEID_FLOAT,
    TYPEID_CHAR,
    TYPEID_BOOL,
    TYPEID_STRING,
    TYPEID_IDENT,
    TYPEID__LEN
} typeid_t;

typedef struct type_t {
    typeid_t tid : 8;
    union {
        node_int_literal_t as_int;
        node_float_literal_t as_float;
        node_char_literal_t as_char;
        node_bool_literal_t as_bool;
        node_string_literal_t as_string;
        node_ident_literal_t as_ident;
    };
} type_t;

static NEO_NODISCARD NEO_UNUSED bool deduce_typeof_expr(const astpool_t *pool, error_vector_t *errors, astref_t expr, typeid_t *out) {
    neo_dassert(pool != NULL && errors != NULL, "Invalid arguments");
    if (astref_isnull(expr)) { return false; }
    const astnode_t *node = astpool_resolve(pool, expr);
    switch (node->type) {
        case ASTNODE_INT_LIT: *out = TYPEID_INT; return true;
        case ASTNODE_FLOAT_LIT: *out = TYPEID_FLOAT; return true;
        case ASTNODE_CHAR_LIT: *out = TYPEID_CHAR; return true;
        case ASTNODE_BOOL_LIT: *out = TYPEID_BOOL; return true;
        case ASTNODE_STRING_LIT: *out = TYPEID_STRING; return true;
        case ASTNODE_IDENT_LIT:
        case ASTNODE_SELF_LIT: *out = TYPEID_IDENT; return true;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
            return deduce_typeof_expr(pool, errors, data->child_expr, out);
        }
        case ASTNODE_UNARY_OP: {
            const node_group_t *data = &node->dat.n_group;
            return deduce_typeof_expr(pool, errors, data->child_expr, out);
        }
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
            typeid_t left;
            typeid_t right;
            bool left_ok = deduce_typeof_expr(pool, errors, data->left_expr, &left);
            bool right_ok = deduce_typeof_expr(pool, errors, data->right_expr, &right);
            if (neo_unlikely(!left_ok || !right_ok)) { return false; } /* Types deduction error in one of them. */
            if (neo_likely(left == right)) { *out = left; return true; } /* Both binary operands are type equivalent. */
            errvec_push(errors, comerror_new(COMERR_TYPE_MISMATCH, 0, 0, NULL, NULL, NULL, NULL));
            return false;
        } break;
        default: {
            neo_assert((ASTNODE_EXPR_MASK & astmask(node->type)) == 0, "AST node is of type expression, but not handled");
            errvec_push(errors, comerror_new(COMERR_INVALID_EXPRESSION, 0, 0, NULL, NULL, NULL, NULL));
            return false;
        }
    }
}

#define MAX_BLOCK_DEPTH (1<<10)

/* Context for semantic analysis. */
typedef struct sema_context_t {
    error_vector_t *errors;
    const astpool_t *pool;
    astref_t *blocks; /* Tracked blocks, to free symtab from. */
    uint32_t len;
    uint32_t cap;
    uint32_t prev_depth; /* Previous scope depth. */
    uint32_t scope_stack_needle; /* Scope stack needle. */
} sema_context_t;

static void sema_ctx_init(sema_context_t *self, error_vector_t *errors, const astpool_t *pool) {
    neo_dassert(self != NULL && errors != NULL, "Invalid arguments");
    memset(self, 0, sizeof(*self));
    self->errors = errors;
    self->pool = pool;
    self->blocks = (astref_t *)neo_memalloc(NULL, (self->cap=1<<6)*sizeof(*self->blocks));
}

static void sema_ctx_push_block(sema_context_t *self, astref_t symtab) {
    neo_dassert(self != NULL, "self is NULL");
    if (neo_unlikely(astref_isnull(symtab))) { return; }
    const astnode_t *node = astpool_resolve(self->pool, symtab);
    neo_assert(node != NULL && node->type == ASTNODE_BLOCK, "AST node is not a block");
    if (self->len >= self->cap) {
        self->blocks = (astref_t *)neo_memalloc(self->blocks, (self->cap<<=1)*sizeof(*self->blocks));
    }
    self->blocks[self->len++] = symtab;
}

static void block_free_symtabs(node_block_t *self) { /* Free all symbol tables in a block. */
    neo_dassert(self != NULL, "self is NULL");
    switch ((block_scope_t)self->scope) { /* Init all symbol tables. */
        case BLOCKSCOPE_MODULE:
            symtab_free(&self->symtabs.sc_module.class_table);
            symtab_free(&self->symtabs.sc_module.method_table);
            symtab_free(&self->symtabs.sc_module.variable_table);
            break;
        case BLOCKSCOPE_CLASS:
            symtab_free(&self->symtabs.sc_class.method_table);
            symtab_free(&self->symtabs.sc_class.variable_table);
            break;
        case BLOCKSCOPE_LOCAL:
            symtab_free(&self->symtabs.sc_local.variable_table);
            break;
        case BLOCKSCOPE_PARAMLIST:
            symtab_free(&self->symtabs.sc_params.variable_table);
            break;
        case BLOCKSCOPE_ARGLIST: break; /* No symbol table. */
        case BLOCKSCOPE__COUNT: neo_unreachable();
    }
}

static void sema_ctx_free(sema_context_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    for (uint32_t i = 0; i < self->len; ++i) {
        astnode_t *node = astpool_resolve(self->pool, self->blocks[i]);
        neo_assert(node != NULL && node->type == ASTNODE_BLOCK, "AST node is not a block");
        block_free_symtabs(&node->dat.n_block);
    }
    neo_memalloc(self->blocks, 0);
    memset(self, 0, sizeof(*self));
}

/* Checks if symbol is within symtab. If true, error is emitted and false is returned, else true is returned. */
static bool symtab_check(symtab_t *tab, const node_ident_literal_t *key, sema_context_t *ctx) {
    const symrecord_t *existing = NULL;
    if (neo_unlikely(symtab_get(tab, key, &existing))) { /* Key already exists. */
        neo_dassert(existing != NULL, "Existing symbol record is NULL");
        uint8_t *cloned;
        srcspan_stack_clone(key->span, cloned); /* Clone span into zero terminated stack-string. */
        /* Emit error, key string is cloned by comerror_from_token(). */
        errvec_push(ctx->errors, comerror_from_token(COMERR_SYMBOL_REDEFINITION, &existing->tok, cloned));
        return false; /* We're done. */
    }
    return true;
}

static bool symtab_check_block(node_block_t *block, const node_ident_literal_t *key, sema_context_t *ctx) {
    switch (block->scope) {
        case BLOCKSCOPE_MODULE:
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_module.class_table, key, ctx))) { return false; }
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_module.variable_table, key, ctx))) { return false; }
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_module.method_table, key, ctx))) { return false; }
            return true;
        case BLOCKSCOPE_CLASS:
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_class.variable_table, key, ctx))) { return false; }
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_class.method_table, key, ctx))) { return false; }
            return true;
        case BLOCKSCOPE_LOCAL:
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_local.variable_table, key, ctx))) { return false; }
            return true;
        case BLOCKSCOPE_PARAMLIST:
            if (neo_unlikely(!symtab_check(&block->symtabs.sc_params.variable_table, key, ctx))) { return false; }
            return true;
        case BLOCKSCOPE_ARGLIST:
        case BLOCKSCOPE__COUNT:
            return false;
            /* No symtabs. */
    }
    return false;
}

static void inject_symtab_symbol( /* Injects a symbol into a symbol table. */
    symtab_t *target,
    const astpool_t *pool,
    astref_t noderef,
    astnode_type_t target_type,
    astref_t (*extractor)(const astnode_t *target),
    sema_context_t *ctx,
    uint32_t parent_depth
) {
    if (neo_unlikely(astref_isnull(noderef))) { return; }
    const astnode_t *node = astpool_resolve(pool, noderef);
    if (node->type != target_type) { return; } /* Skip non-target nodes. */
    const astnode_t *ident = astpool_resolve(pool, (*extractor)(node));
    neo_assert(ident != NULL && ident->type == ASTNODE_IDENT_LIT, "AST node is not an identifier");
    const node_ident_literal_t key = ident->dat.n_ident_lit;
    if (!symtab_is_init(target)) { /* Init symbol table if not already done. */
        symtab_init(target, 1<<4); /* Init symbol table. */
    }
    /* Now that we have the key, check if the identifier is already defined in any symbol tables. */
    if (neo_unlikely(!symtab_check(target, &key, ctx))) { return; }  /* 1. Check current symbtab. */
    for (uint32_t i = 0; i < ctx->len; ++i) {
        astnode_t *block = astpool_resolve(pool, ctx->blocks[i]);
        neo_dassert(block != NULL && block->type == ASTNODE_BLOCK, "Invalid block node");
        if (block->dat.n_block.scope_depth < parent_depth && /* Check if upper blocks contain the symbol. */
            neo_unlikely(!symtab_check_block(&block->dat.n_block, &key, ctx))) {
            return;
        }
    }
    /* Key does not exist yet, so register it. */
    const symrecord_t val = {
        .tok = key.tok,
        .node = noderef
    };
    symtab_put(target, &key, &val); /* Insert symbol. */
}

static astref_t sym_extract_class(const astnode_t *target) {
    return target->dat.n_class.ident;
}

static astref_t sym_extract_func(const astnode_t *target) {
    return target->dat.n_method.ident;
}

static astref_t sym_extract_variable(const astnode_t *target) {
    return target->dat.n_variable.ident;
}

static void populate_symbol_tables(
    node_block_t *self,
    astref_t selfref,
    astpool_t *pool,
    sema_context_t *ctx
) {
    if (self->len == 0) { return; }
    sema_ctx_push_block(ctx, selfref); /* Track block for later cleanup. */
    astref_t *children = astpool_resolvelist(pool, self->nodes);
    for (uint32_t i = 0; i < self->len; ++i) {
        astref_t target = children[i];
        if (astref_isnull(target)) { continue; } /* Skip NULL nodes. */
        /* Last but not least, inject symbols into the symbol table. */
        switch (self->scope) { /* Inject into all symbol tables. */
            case BLOCKSCOPE_MODULE:
                inject_symtab_symbol( /* Inject into module class table. */
                    &self->symtabs.sc_module.class_table,
                    pool,
                    target,
                    ASTNODE_CLASS,
                    &sym_extract_class,
                    ctx,
                    self->scope_depth
                );
                inject_symtab_symbol( /* Inject into class method table. */
                    &self->symtabs.sc_module.method_table,
                    pool,
                    target,
                    ASTNODE_FUNCTION,
                    &sym_extract_func,
                    ctx,
                    self->scope_depth
                );
                inject_symtab_symbol( /* Inject into class variable table. */
                    &self->symtabs.sc_module.variable_table,
                    pool,
                    target,
                    ASTNODE_VARIABLE,
                    &sym_extract_variable,
                    ctx,
                    self->scope_depth
                );
            break;
            case BLOCKSCOPE_CLASS:
                inject_symtab_symbol( /* Inject into class method table. */
                    &self->symtabs.sc_class.method_table,
                    pool,
                    target,
                    ASTNODE_FUNCTION,
                    &sym_extract_func,
                    ctx,
                    self->scope_depth
                );
                inject_symtab_symbol( /* Inject into class variable table. */
                    &self->symtabs.sc_class.variable_table,
                    pool,
                    target,
                    ASTNODE_VARIABLE,
                    &sym_extract_variable,
                    ctx,
                    self->scope_depth
                );
            break;
            case BLOCKSCOPE_LOCAL:
                inject_symtab_symbol( /* Inject into local variable table. */
                    &self->symtabs.sc_local.variable_table,
                    pool,
                    target,
                    ASTNODE_VARIABLE,
                    &sym_extract_variable,
                    ctx,
                    self->scope_depth
                );
            break;
            case BLOCKSCOPE_PARAMLIST:
                inject_symtab_symbol( /* Inject into parameter variable table. */
                    &self->symtabs.sc_params.variable_table,
                    pool,
                    target,
                    ASTNODE_VARIABLE,
                    &sym_extract_variable,
                    ctx,
                    self->scope_depth
                );
            break;
            case BLOCKSCOPE_ARGLIST: break; /* No symbols to inject. */
            case BLOCKSCOPE__COUNT: neo_unreachable();
        }
    }
}

static void semantic_visitor(astpool_t *pool, astref_t ref, void *usr) {
    sema_context_t *ctx = (sema_context_t *)usr;
    astnode_t *node = astpool_resolve(pool, ref);
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
            (void)data;
        } return;
        case ASTNODE_BREAK: {

        } return;
        case ASTNODE_CONTINUE: {

        } return;
        case ASTNODE_INT_LIT: {
            const node_int_literal_t *data = &node->dat.n_int_lit;
            (void)data;
        } return;
        case ASTNODE_FLOAT_LIT: {
            const node_float_literal_t *data = &node->dat.n_float_lit;
            (void)data;
        } return;
        case ASTNODE_CHAR_LIT: {
            const node_char_literal_t *data = &node->dat.n_char_lit;
            (void)data;
        } return;
        case ASTNODE_BOOL_LIT: {
            const node_bool_literal_t *data = &node->dat.n_bool_lit;
            (void)data;
        } return;
        case ASTNODE_STRING_LIT: {
            const node_string_literal_t *data = &node->dat.n_string_lit;
            (void)data;
        } return;
        case ASTNODE_IDENT_LIT: {
            const node_ident_literal_t *data = &node->dat.n_ident_lit;
            (void)data;
        } return;
        case ASTNODE_SELF_LIT: {

        } return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
            (void)data;
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
            (void)data;
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
            (void)data;
        } return;
        case ASTNODE_FUNCTION: {
            const node_method_t *data = &node->dat.n_method;
            (void)data;
        } return;
        case ASTNODE_BLOCK: {
            node_block_t *data = &node->dat.n_block;
            if (node_block_can_have_symtabs(data)) {
                populate_symbol_tables(data, ref, pool, ctx);
            }
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
            (void)data;
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
            (void)data;
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
            (void)data;
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
            (void)data;
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
            (void)data;
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
            (void)data;
        } return;
        case ASTNODE__COUNT: neo_unreachable();
    }
}

static bool perform_semantic_analysis(astpool_t *pool, astref_t root, error_vector_t *errors) {
    neo_dassert(!astref_isnull(root) && pool != NULL && errors != NULL, "Invalid arguments");
    uint32_t error_count = errors->len;
    sema_context_t ctx;
    sema_ctx_init(&ctx, errors, pool);
    astnode_visit(pool, root, &semantic_visitor, &ctx);
    sema_ctx_free(&ctx);
    return errors->len == error_count;
}

static NEO_COLDPROC NEO_UNUSED void node_block_dump_symbols(const node_block_t *self, FILE *f) {
    neo_dassert(self != NULL && f != NULL, "Invalid arguments");
    switch ((block_scope_t)self->scope) { /* Free all symbol tables. */
        case BLOCKSCOPE_MODULE:
            symtab_print(&self->symtabs.sc_module.class_table, f, "Classes");
            symtab_print(&self->symtabs.sc_module.method_table, f, "Methods");
            symtab_print(&self->symtabs.sc_module.variable_table, f, "Variables");
            break;
        case BLOCKSCOPE_CLASS:
            symtab_print(&self->symtabs.sc_class.method_table, f, "Methods");
            symtab_print(&self->symtabs.sc_class.variable_table, f, "Variables");
            break;
        case BLOCKSCOPE_LOCAL:
            symtab_print(&self->symtabs.sc_local.variable_table, f, "Variables");
            break;
        case BLOCKSCOPE_PARAMLIST:
            symtab_print(&self->symtabs.sc_params.variable_table, f, "Variables");
            break;
        case BLOCKSCOPE_ARGLIST: break; /* No symbol table. */
        case BLOCKSCOPE__COUNT: neo_unreachable();
    }
}
