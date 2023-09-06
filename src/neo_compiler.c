/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <time.h>
#include "neo_compiler.h"
#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_parser.h"
#include "neo_bc.h"

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
    self->is_file = true;
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

static void gen_code_unary_expr(bytecode_t *bc , unary_op_type_t op) {
    neo_dassert(bc);
    switch (op) {
        case UNOP_PLUS: bc_emit(bc, OPC_IADDO); return;
        case UNOP_MINUS: bc_emit(bc, OPC_ISUBO); return;
        case UNOP_LOG_NOT: bc_emit_ipush(bc, -1); bc_emit(bc, OPC_IXOR); return;
        case UNOP_BIT_COMPL: bc_emit_ipush(bc, -1); bc_emit(bc, OPC_IXOR); return;
        case UNOP_INC: bc_emit_ipush(bc, 1); bc_emit(bc, OPC_IADDO); return;
        case UNOP_DEC:  bc_emit_ipush(bc, 1); bc_emit(bc, OPC_ISUBO); return;
        default: neo_unreachable();
    }
}

static void gen_code_binary_expr(bytecode_t *bc , binary_op_type_t op) {
    neo_dassert(bc);
    switch (op) {
        case BINOP_DOT: return; /* TODO */
        case BINOP_ASSIGN: return; /* TODO */
        case BINOP_ADD: case BINOP_ADD_ASSIGN: bc_emit(bc, OPC_IADDO); return;
        case BINOP_SUB: case BINOP_SUB_ASSIGN: bc_emit(bc, OPC_ISUBO); return;
        case BINOP_MUL: case BINOP_MUL_ASSIGN: bc_emit(bc, OPC_IMULO); return;
        case BINOP_POW: case BINOP_POW_ASSIGN: bc_emit(bc, OPC_IPOWO); return;
        case BINOP_ADD_NO_OV: case BINOP_ADD_ASSIGN_NO_OV: bc_emit(bc, OPC_IADD); return;
        case BINOP_SUB_NO_OV: case BINOP_SUB_ASSIGN_NO_OV: bc_emit(bc, OPC_ISUB); return;
        case BINOP_MUL_NO_OV: case BINOP_MUL_ASSIGN_NO_OV: bc_emit(bc, OPC_IMUL); return;
        case BINOP_POW_NO_OV: case BINOP_POW_ASSIGN_NO_OV: bc_emit(bc, OPC_IPOW); return;
        case BINOP_DIV_ASSIGN: case BINOP_DIV: bc_emit(bc, OPC_IDIV); return;
        case BINOP_MOD_ASSIGN: case BINOP_MOD: bc_emit(bc, OPC_IMOD); return;
        case BINOP_EQUAL:  return; /* TODO */
        case BINOP_NOT_EQUAL: return; /* TODO */
        case BINOP_LESS: return; /* TODO */
        case BINOP_LESS_EQUAL: return; /* TODO */
        case BINOP_GREATER: return; /* TODO */
        case BINOP_GREATER_EQUAL: return; /* TODO */
        case BINOP_BIT_AND: case BINOP_BIT_AND_ASSIGN: bc_emit(bc, OPC_IAND); return;
        case BINOP_BIT_OR: case BINOP_BIT_OR_ASSIGN: bc_emit(bc, OPC_IOR); return;
        case BINOP_BIT_XOR: case BINOP_BIT_XOR_ASSIGN: bc_emit(bc, OPC_IXOR); return;
        case BINOP_BIT_ASHL: case BINOP_BIT_ASHL_ASSIGN: bc_emit(bc, OPC_ISAL); return;
        case BINOP_BIT_ASHR: case BINOP_BIT_ASHR_ASSIGN: bc_emit(bc, OPC_ISAR); return;
        case BINOP_BIT_ROL: case BINOP_BIT_ROL_ASSIGN: bc_emit(bc, OPC_IROL); return;
        case BINOP_BIT_ROR: case BINOP_BIT_ROR_ASSIGN: bc_emit(bc, OPC_IROR); return;
        case BINOP_BIT_LSHR: case BINOP_BIT_LSHR_ASSIGN: bc_emit(bc, OPC_ISLR); return;
        case BINOP_LOG_AND: return; /* TODO */
        case BINOP_LOG_OR: return; /* TODO */
        case BINOP_CALL: return; /* TODO */
        default: return;
    }
}

static void gen_code_visitor(const astpool_t *pool, astref_t root, void *usr) {
    bytecode_t *bc = (bytecode_t *)usr;
    astnode_t *node = astpool_resolve(pool, root);
    switch (node->type) {
        case ASTNODE_INT_LIT: bc_emit_ipush(bc, node->dat.n_int_lit.value); return;
        case ASTNODE_FLOAT_LIT: bc_emit_fpush(bc, node->dat.n_float_lit.value); return;
        case ASTNODE_CHAR_LIT: bc_emit_ipush(bc, node->dat.n_char_lit.value); return;
        case ASTNODE_BOOL_LIT: bc_emit_ipush(bc, node->dat.n_bool_lit.value); return;
        case ASTNODE_GROUP: return;
        case ASTNODE_UNARY_OP: gen_code_unary_expr(bc, node->dat.n_unary_op.opcode); return;
        case ASTNODE_BINARY_OP: gen_code_binary_expr(bc, node->dat.n_binary_op.opcode); return;
        default: return;
    }
}

static void gen_code(const astpool_t *pool, astref_t root) {
    bytecode_t bc;
    bc_init(&bc);
    astnode_visit(pool, root, &gen_code_visitor, &bc);
    bc_finalize(&bc);
    bc_disassemble(&bc, stdout, true);
    bc_free(&bc);
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
    neo_assert(!astref_isnull(self->ast) && "Parser did not emit any AST");
    if (self->post_compile_callback) {
        (*self->post_compile_callback)(src, self->flags, user);
    }
    gen_code(&self->parser.pool, self->ast);
    if (compiler_has_flags(self, COM_FLAG_RENDER_AST)) {
#ifdef NEO_EXTENSION_AST_RENDERING
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
        return false;
    }
    double time_spent = (double)(clock()-begin)/CLOCKS_PER_SEC;
    if (!compiler_has_flags(self, COM_FLAG_NO_STATUS)) {
        printf("Compiled %s in %.03fms\n", src->filename, time_spent*1000.0); /* TODO: UTF-8 aware printf. */
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
