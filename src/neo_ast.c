/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_ast.h"
#include "neo_lexer.h"
#include "neo_compiler.h"

void symtab_init(symtab_t *self, uint32_t cap) {
    neo_dassert(self != NULL, "self is NULL");
    memset(self, 0, sizeof(*self));
    cap = cap ? cap : 1<<6;
    self->cap = cap;
    self->buckets = neo_memalloc(NULL, self->cap*sizeof(*self->buckets));
    memset(self->buckets, 0, self->cap*sizeof(*self->buckets));
}

bool symtab_put(symtab_t *self, const node_ident_literal_t *key, const symrecord_t *val) {
    neo_dassert(self != NULL && key != NULL && key->hash != 0 && val != NULL, "Invalid arguments");
    /* Search existing bucket. */
    for (uint32_t i = 0; i < self->len; ++i) {
        symbuck_t *bucket = &self->buckets[i];
        if (bucket->key.hash == key->hash && bucket->key.span.len == key->span.len && memcmp(bucket->key.span.p, key->span.p, key->span.len) == 0) {
            bucket->val = *val;
            return false;
        }
    }
    if (self->len >= self->cap) { /* Reallocate if necessary. */
        self->buckets = neo_memalloc(self->buckets, (self->cap<<=1)*sizeof(*self->buckets));
    }
    self->buckets[self->len++] = (symbuck_t) {
        .key = *key,
        .val = *val
    };
    return true;
}

bool symtab_get(symtab_t *self, const node_ident_literal_t *key, const symrecord_t **val) {
    neo_dassert(self != NULL && key != NULL && key->hash != 0 && val != NULL, "Invalid arguments");
    for (uint32_t i = 0; i < self->len; ++i) {
        symbuck_t *bucket = &self->buckets[i];
        if (bucket->key.hash == key->hash && bucket->key.span.len == key->span.len && memcmp(bucket->key.span.p, key->span.p, key->span.len) == 0) {
            *val = &bucket->val;
            return true;
        }
    }
    return false;
}

uint32_t symtab_len(const symtab_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    return self->len;
}

void symtab_iter(const symtab_t *self, void (*callback)(const node_ident_literal_t *key, const symrecord_t *sym, void *usr), void *usr) {
    neo_dassert(self != NULL && callback != NULL, "Invalid arguments");
    for (uint32_t i = 0; i < self->len; ++i) {
        const symbuck_t *bucket = &self->buckets[i];
        (*callback)(&bucket->key, &bucket->val, usr);
    }
}

void symtab_free(symtab_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    neo_memalloc(self->buckets, 0);
    memset(self, 0, sizeof(*self));
}

static NEO_COLDPROC void symtab_print_visitor(const node_ident_literal_t *key, const symrecord_t *sym, void *usr) {
    neo_dassert(key != NULL && sym != NULL && usr != NULL, "Invalid arguments"); (void)sym;
    FILE *f = (FILE *)usr;
    fprintf(f, "\t%.*s\n", (int)key->span.len, key->span.p);
}

void symtab_print(const symtab_t *self, FILE *f, const char *name) {
    neo_dassert(self != NULL && f != NULL, "Invalid arguments");
    if (!self->len) { return; }
    fprintf(f, "Symbol table: %s\n", name ? name : "(unnamed)");
    symtab_iter(self, &symtab_print_visitor, f);
}

#define m(node) astmask(ASTNODE_##node)
static const uint64_t block_valid_masks[BLOCKSCOPE__COUNT] = { /* This table contains masks of the allowed ASTNODE_* types for each block type inside a node_block_t. */
    m(ERROR)|m(FUNCTION)|m(CLASS)|m(VARIABLE)|m(BRANCH)|m(LOOP)|m(UNARY_OP)|m(BINARY_OP)|m(GROUP), /* BLOCKSCOPE_MODULE */
    m(ERROR)|m(FUNCTION)|m(VARIABLE), /* BLOCKSCOPE_CLASS */
    m(ERROR)|m(VARIABLE)|m(BRANCH)|m(LOOP)|m(UNARY_OP)|m(BINARY_OP)|m(GROUP)|m(RETURN)|m(BREAK)|m(CONTINUE), /* BLOCKSCOPE_LOCAL */
    m(ERROR)|m(VARIABLE), /* BLOCKSCOPE_PARAMLIST */
    ASTNODE_EXPR_MASK /* BLOCKSCOPE_ARGLIST */
};
#undef m

#define _(_1, _2) [_1] = _2
const char *const astnode_names[ASTNODE__COUNT] = { nodedef(_, NEO_SEP) };
#undef _
static const char *const block_names[BLOCKSCOPE__COUNT] = {
    "(BLK) MODULE",
    "(BLK) CLASS",
    "(BLK) LOCAL",
    "(BLK) PARAMLIST",
    "(BLK) ARGLIST"
};

#define impl_ast_node_hull_factory(name, ttype)\
  astref_t astnode_new_##name(astpool_t *pool) {\
     neo_dassert(pool != NULL, "self is NULL");\
    return astpool_alloc(pool, NULL,  ASTNODE_##ttype);\
  }

impl_ast_node_hull_factory(break, BREAK)
impl_ast_node_hull_factory(continue, CONTINUE)
impl_ast_node_hull_factory(self, SELF_LIT)

#undef impl_ast_node_hull_factory

#define astverify(expr, msg) neo_assert((expr), "AST verification failed: " msg)
static astnode_t *verify_resolve_node(astpool_t *pool, astref_t target) {
    neo_dassert(pool != NULL, "self is NULL");
    astverify(astpool_isvalidref(pool, target), "AST reference is invalid");
    astnode_t *node = astpool_resolve(pool, target);
    astverify(node != NULL, "AST reference resolve returned NULL");
    return node;
}
#define isexpr(node) (ASTNODE_EXPR_MASK&(astmask((node)->type)))
#define verify_resolve(ref) verify_resolve_node(pool, (ref))
#define verify_expr(node) astverify(isexpr((node)), "AST Node is not an expression")
#define verify_type(node, expected) astverify((node)->type == (expected), "AST Node is not of expected type: " #expected)
#define verify_block(node, expected)\
    verify_type((node), ASTNODE_BLOCK); \
    astverify((node)->dat.n_block.scope == (expected), "AST Node block type is not of expected block type: " #expected)

astref_t astnode_new_error(astpool_t *pool, const node_error_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_error_t *data = node;
    astverify(data->message, "Error message is NULL");

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_ERROR);
    nn->dat.n_error = *node;
    return ref;
}

astref_t astnode_new_group(astpool_t *pool, const node_group_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_group_t *data = node;
    const astnode_t *child_expr = verify_resolve(data->child_expr);
    verify_expr(child_expr);

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_GROUP);
    nn->dat.n_group = *node;
    return ref;
}

astref_t astnode_new_unary_op(astpool_t *pool, const node_unary_op_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_unary_op_t *data = node;
    astverify(data->opcode < UNOP__COUNT, "Unary op operator is invalid");
    const astnode_t *expr = verify_resolve(data->child_expr);
    verify_expr(expr);

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_UNARY_OP);
    nn->dat.n_unary_op = *node;
    return ref;
}

astref_t astnode_new_binary_op(astpool_t *pool, const node_binary_op_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_binary_op_t *data = node;
    astverify(data->opcode < BINOP__COUNT, "Binary op operator is invalid");
    const astnode_t *lhs = verify_resolve(data->left_expr);
    verify_expr(lhs);
    if (data->opcode == BINOP_CALL && !astref_isnull(data->right_expr)) { /* Call has a block of arguments. */
        const astnode_t *rhs = verify_resolve(data->right_expr);
        verify_type(rhs, ASTNODE_BLOCK);
        const node_block_t *block = &rhs->dat.n_block;
        astverify(block->scope == BLOCKSCOPE_ARGLIST, "Call block is not of type BLOCKSCOPE_ARGLIST");
        const astref_t *args = astpool_resolvelist(pool, block->nodes);
        for (uint32_t i = 0; i < block->len; ++i) { /* All arguments must be expressions. */
            const astnode_t *arg = verify_resolve(args[i]);
            verify_expr(arg);
        }
    } else if (!astref_isnull(data->right_expr)) {
        const astnode_t *rhs = verify_resolve(data->right_expr);
        verify_expr(rhs);
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BINARY_OP);
    nn->dat.n_binary_op = *node;
    return ref;
}

astref_t astnode_new_method(astpool_t *pool, const node_method_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_method_t *data = node;
    const astnode_t *ident = verify_resolve(data->ident);
    verify_type(ident, ASTNODE_IDENT_LIT);
    if (!astref_isnull(data->params)) { /* Optional. */
        const astnode_t *params = verify_resolve(data->params);
        verify_block(params, BLOCKSCOPE_PARAMLIST);
    }
    if (!astref_isnull(data->ret_type)) { /* Optional. */
        const astnode_t *ret_type = verify_resolve(data->ret_type);
        verify_type(ret_type, ASTNODE_IDENT_LIT);
    }
    if (!astref_isnull(data->body)) { /* Optional. */
        const astnode_t *body = verify_resolve(data->body);
        verify_block(body, BLOCKSCOPE_LOCAL);
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_FUNCTION);
    nn->dat.n_method = *node;
    return ref;
}

astref_t astnode_new_block(astpool_t *pool, const node_block_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    if (node->len == 0) {
        return ASTREF_NULL;
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BLOCK);
    nn->dat.n_block = *node;
    return ref;
}

astref_t astnode_new_variable(astpool_t *pool, const node_variable_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_variable_t *data = node;
    verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
    verify_type(verify_resolve(data->type), ASTNODE_IDENT_LIT);
    if (data->var_scope != VARSCOPE_PARAM) {
        verify_expr(verify_resolve(data->init_expr));
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_VARIABLE);
    nn->dat.n_variable = *node;
    return ref;
}

astref_t astnode_new_return(astpool_t *pool, const node_return_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_return_t *data = node;
    if (!astref_isnull(data->child_expr)) { /* Optional. */
        verify_expr(verify_resolve(data->child_expr));
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_RETURN);
    nn->dat.n_return = *node;
    return ref;
}

astref_t astnode_new_branch(astpool_t *pool, const node_branch_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_branch_t *data = node;
    verify_expr(verify_resolve(data->cond_expr));
    verify_block(verify_resolve(data->true_block), BLOCKSCOPE_LOCAL);
    if (!astref_isnull(data->false_block)) { /* Optional. */
        verify_block(verify_resolve(data->false_block), BLOCKSCOPE_LOCAL);
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BRANCH);
    nn->dat.n_branch = *node;
    return ref;
}

astref_t astnode_new_loop(astpool_t *pool, const node_loop_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_loop_t *data = node;
    verify_expr(verify_resolve(data->cond_expr));
    verify_block(verify_resolve(data->true_block), BLOCKSCOPE_LOCAL);

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_LOOP);
    nn->dat.n_loop = *node;
    return ref;
}

astref_t astnode_new_class(astpool_t *pool, const node_class_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_class_t *data = node;
    verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
    if (!astref_isnull(data->body)) { /* Optional. */
        verify_block(verify_resolve(data->body), BLOCKSCOPE_CLASS);
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_CLASS);
    nn->dat.n_class = *node;
    return ref;
}

astref_t astnode_new_module(astpool_t *pool, const node_module_t *node) {
    neo_dassert(pool != NULL && node != NULL, "Invalid arguments");

    /* Verify AST data. */
    const node_module_t *data = node;
    if (!astref_isnull(data->ident)) {
        verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
    }
    if (!astref_isnull(data->body)) { /* Optional. */
        verify_block(verify_resolve(data->body), BLOCKSCOPE_MODULE);
    }

    /* Create AST node. */
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_MODULE);
    nn->dat.n_module = *node;
    return ref;
}

astref_t astnode_new_int(astpool_t *pool, neo_int_t value, const token_t *tok) {
    neo_dassert(pool != NULL, "self is NULL");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_INT_LIT);
    nn->dat.n_int_lit.value = value;
    nn->dat.n_int_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_float(astpool_t *pool, neo_float_t value, const token_t *tok) {
    neo_dassert(pool != NULL, "self is NULL");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_FLOAT_LIT);
    nn->dat.n_float_lit.value = value;
    nn->dat.n_float_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_char(astpool_t *pool, neo_char_t value, const token_t *tok) {
    neo_dassert(pool != NULL, "self is NULL");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_CHAR_LIT);
    nn->dat.n_char_lit.value = value;
    nn->dat.n_char_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_bool(astpool_t *pool, neo_bool_t value, const token_t *tok) {
    neo_dassert(pool != NULL, "self is NULL");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BOOL_LIT);
    nn->dat.n_bool_lit.value = value;
    nn->dat.n_bool_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_string(astpool_t *pool, const uint8_t *src, const token_t *tok) {
    neo_dassert(pool != NULL && src != NULL, "Invalid arguments");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_STRING_LIT);
    /* String literals own the memory, because the literal must be escaped :) */
    size_t len;
    nn->dat.n_string_lit.str = neo_strdup2(src, &len); /* Clone. */
    nn->dat.n_string_lit.hash = neo_hash_fnv1a(nn->dat.n_string_lit.str, len); /* Strings are hashed. */
    nn->dat.n_string_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_ident(astpool_t *pool, srcspan_t value, const token_t *tok) {
    neo_dassert(pool != NULL, "self is NULL");
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_IDENT_LIT);
    nn->dat.n_ident_lit.span = value;
    nn->dat.n_ident_lit.hash = srcspan_hash(value); /* Identifiers are hashed. */
    nn->dat.n_ident_lit.tok = tok ? *tok : (token_t){};
    return ref;
}

astref_t astnode_new_block_with_nodes(astpool_t *pool, block_scope_t type, astref_t *nodes) {
    neo_dassert(pool != NULL && nodes != NULL, "Invalid arguments");
    node_block_t block = {
        .scope = type,
    };
    while (astpool_isvalidref(pool, *nodes)) {
        node_block_push_child(pool, &block, *nodes++);
    }
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BLOCK);
    nn->dat.n_block = block;
    return ref;
}

void node_block_push_child(astpool_t *pool, node_block_t *self, astref_t node) {
    neo_dassert(pool != NULL && self != NULL, "Invalid arguments");
    if (neo_unlikely(astref_isnull(node))) { /* Skip NULL nodes. */
        return;
    } else if (!self->cap) { /* No nodes yet, so allocate. */
        self->nodes = astpool_alloclist(pool, NULL, self->cap=1<<5);
    } else if (self->len >= self->cap) { /* Reallocate if necessary. */
        size_t oldlen = self->cap;
        const astref_t *old = astpool_resolvelist(pool, self->nodes);
        listref_t newref = astpool_alloclist(pool, NULL, self->cap<<=2);
        astref_t *new = astpool_resolvelist(pool, newref);
        memcpy(new, old, oldlen*sizeof(*new)); /* Copy old nodes. */
        self->nodes = newref;
    }
    astref_t *reflist = astpool_resolvelist(pool, self->nodes);
    reflist[self->len++] = node;

    /* Check if node is allowed in this block. */
    const astnode_t *pnode = astpool_resolve(pool, node);
    uint64_t mask = block_valid_masks[self->scope];
    uint64_t node_mask = astmask(pnode->type);
    if (neo_unlikely((mask & node_mask) == 0)) {
        neo_panic("Block node type '%s' is not allowed in '%s' self kind.", astnode_names[pnode->type], block_names[self->scope]);
    }
}

static void astnode_visit_root_impl(astpool_t *pool, astref_t rootref, void (*visitor)(astpool_t *pool, astref_t node, void *user), void *user, size_t *c) {
    neo_dassert(pool != NULL && visitor != NULL && c != NULL, "Invalid arguments");
    if (astref_isnull(rootref)) { return; } /* Skip NULL nodes. */
    astnode_t *root = astpool_resolve(pool, rootref);
    ++*c; /* Increment counter. */
    switch (root->type) { /* Leafs have no children, so they are skipped. */
        case ASTNODE_ERROR:
        case ASTNODE_BREAK:
        case ASTNODE_CONTINUE:
        case ASTNODE_INT_LIT:
        case ASTNODE_FLOAT_LIT:
        case ASTNODE_CHAR_LIT:
        case ASTNODE_BOOL_LIT:
        case ASTNODE_STRING_LIT:
        case ASTNODE_IDENT_LIT:
        case ASTNODE_SELF_LIT: {
            neo_dassert(!!(ASTNODE_LEAF_MASK & astmask(root->type)), "AST node is not a leaf");
        } break; /* Visitor invocation is redundant. */
        case ASTNODE_GROUP: {
            const node_group_t *data = &root->dat.n_group;
            astnode_visit_root_impl(pool, data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &root->dat.n_unary_op;
            astnode_visit_root_impl(pool, data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &root->dat.n_binary_op;
            astnode_visit_root_impl(pool, data->left_expr, visitor, user, c);
            astnode_visit_root_impl(pool, data->right_expr, visitor, user, c);
        } break;
        case ASTNODE_FUNCTION: {
            const node_method_t *data = &root->dat.n_method;
            astnode_visit_root_impl(pool, data->ident, visitor, user, c);
            astnode_visit_root_impl(pool, data->params, visitor, user, c);
            astnode_visit_root_impl(pool, data->ret_type, visitor, user, c);
            astnode_visit_root_impl(pool, data->body, visitor, user, c);
        } break;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &root->dat.n_block;
            const astref_t *children = astpool_resolvelist(pool, data->nodes);
            for (uint32_t i = 0; i < data->len; ++i) {
                astnode_visit_root_impl(pool, children[i], visitor, user, c);
            }
        } break;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &root->dat.n_variable;
            astnode_visit_root_impl(pool, data->ident, visitor, user, c);
            astnode_visit_root_impl(pool, data->type, visitor, user, c);
            astnode_visit_root_impl(pool, data->init_expr, visitor, user, c);
        } break;
        case ASTNODE_RETURN: {
            const node_return_t *data = &root->dat.n_return;
            astnode_visit_root_impl(pool, data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &root->dat.n_branch;
            astnode_visit_root_impl(pool, data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(pool, data->true_block, visitor, user, c);
            astnode_visit_root_impl(pool, data->false_block, visitor, user, c);
        } break;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &root->dat.n_loop;
            astnode_visit_root_impl(pool, data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(pool, data->true_block, visitor, user, c);
        } break;
        case ASTNODE_CLASS: {
            const node_class_t *data = &root->dat.n_class;
            astnode_visit_root_impl(pool, data->ident, visitor, user, c);
            astnode_visit_root_impl(pool, data->body, visitor, user, c);
        } break;
        case ASTNODE_MODULE: {
            const node_module_t *data = &root->dat.n_module;
            astnode_visit_root_impl(pool, data->ident, visitor, user, c);
            astnode_visit_root_impl(pool, data->body, visitor, user, c);
        } break;
        case ASTNODE__COUNT: neo_unreachable();
    }
    (*visitor)(pool, rootref, user);
}

size_t astnode_visit(astpool_t *pool, astref_t root, void (*visitor)(astpool_t *pool, astref_t node, void *user), void *user) {
    size_t c = 0;
    astnode_visit_root_impl(pool, root, visitor, user, &c);
    return c;
}

/* Called by the pool to release individual node data. */
static void astnode_free(astnode_t *node) {
    if (!node || node->type != ASTNODE_STRING_LIT) { return; }
    uint8_t *str = node->dat.n_string_lit.str;
    if (!str) { return; }
    neo_memalloc(str, 0);
}

astref_t astpool_alloc(astpool_t *self, astnode_t **o, astnode_type_t type) { /* TODO: Use mempool alloc. */
    neo_dassert(self != NULL, "AST-pool is NULL");
    size_t plen = self->node_pool.len+sizeof(astnode_t);
    neo_assert(plen <= UINT32_MAX, "AST-pool out of nodes, max: %" PRIu32, UINT32_MAX);
    astnode_t *n = neo_mempool_alloc(&self->node_pool, sizeof(*n));
    n->type = type & 255;
    if (o) { *o = n; }
    plen /= sizeof(*n);
    astref_t ref = (astref_t)plen;
    return ref;
}

listref_t astpool_alloclist(astpool_t *self, astref_t **o, uint32_t len) { /* TODO: Use mempool alloc. */
    neo_dassert(self != NULL, "AST-pool is NULL");
    size_t plen = self->list_pool.len;
    neo_assert(plen <= UINT32_MAX, "AST-pool out of nodes, max: %" PRIx32, UINT32_MAX);
    astref_t *n = neo_mempool_alloc(&self->list_pool, len*sizeof(*n));
    if (o) { *o = n; }
    plen /= sizeof(*n);
    return (listref_t)plen;
}

void astpool_init(astpool_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    memset(self, 0, sizeof(*self));
    neo_mempool_init(&self->node_pool, sizeof(astnode_t)*(1<<10));
    neo_mempool_init(&self->list_pool, sizeof(astref_t)*(1<<10));
}

void astpool_free(astpool_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    for (size_t i = 0; i < self->node_pool.len; i += sizeof(astnode_t)) { /* Free all string literal data. */
        astnode_t *node = (astnode_t *)(&((uint8_t *)self->node_pool.top)[i]);
        astnode_free(node); /* Free individual node data, if any. */
    }
    neo_mempool_free(&self->list_pool);
    neo_mempool_free(&self->node_pool);
}

void astpool_reset(astpool_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    neo_mempool_reset(&self->list_pool);
    neo_mempool_reset(&self->node_pool);
}

const char *unary_op_lexeme(unary_op_type_t op) {
    switch (op) {
        case UNOP_PLUS: return tok_lexemes[TOK_OP_ADD];
        case UNOP_MINUS: return tok_lexemes[TOK_OP_SUB];
        case UNOP_LOG_NOT: return tok_lexemes[TOK_OP_LOG_NOT];
        case UNOP_BIT_COMPL: return tok_lexemes[TOK_OP_BIT_COMPL];
        case UNOP_INC: return tok_lexemes[TOK_OP_INC];
        case UNOP_DEC: return tok_lexemes[TOK_OP_DEC];
        default: return "";
    }
}

const char *binary_op_lexeme(binary_op_type_t op) {
    switch (op) {
        case BINOP_DOT: return tok_lexemes[TOK_OP_DOT];
        case BINOP_ASSIGN: return tok_lexemes[TOK_OP_ASSIGN];
        case BINOP_ADD: return tok_lexemes[TOK_OP_ADD];
        case BINOP_SUB: return tok_lexemes[TOK_OP_SUB];
        case BINOP_MUL: return tok_lexemes[TOK_OP_MUL];
        case BINOP_POW: return tok_lexemes[TOK_OP_POW];
        case BINOP_ADD_NO_OV: return tok_lexemes[TOK_OP_ADD_NO_OV];
        case BINOP_SUB_NO_OV: return tok_lexemes[TOK_OP_SUB_NO_OV];
        case BINOP_MUL_NO_OV: return tok_lexemes[TOK_OP_MUL_NO_OV];
        case BINOP_POW_NO_OV: return tok_lexemes[TOK_OP_POW_NO_OV];
        case BINOP_DIV: return tok_lexemes[TOK_OP_DIV];
        case BINOP_MOD: return tok_lexemes[TOK_OP_MOD];
        case BINOP_ADD_ASSIGN: return tok_lexemes[TOK_OP_ADD_ASSIGN];
        case BINOP_SUB_ASSIGN: return tok_lexemes[TOK_OP_SUB_ASSIGN];
        case BINOP_MUL_ASSIGN: return tok_lexemes[TOK_OP_MUL_ASSIGN];
        case BINOP_POW_ASSIGN: return tok_lexemes[TOK_OP_POW_ASSIGN];
        case BINOP_ADD_ASSIGN_NO_OV: return tok_lexemes[TOK_OP_ADD_ASSIGN_NO_OV];
        case BINOP_SUB_ASSIGN_NO_OV: return tok_lexemes[TOK_OP_SUB_ASSIGN_NO_OV];
        case BINOP_MUL_ASSIGN_NO_OV: return tok_lexemes[TOK_OP_MUL_ASSIGN_NO_OV];
        case BINOP_POW_ASSIGN_NO_OV: return tok_lexemes[TOK_OP_POW_ASSIGN_NO_OV];
        case BINOP_DIV_ASSIGN: return tok_lexemes[TOK_OP_DIV_ASSIGN];
        case BINOP_MOD_ASSIGN: return tok_lexemes[TOK_OP_MOD_ASSIGN];
        case BINOP_EQUAL: return tok_lexemes[TOK_OP_EQUAL];
        case BINOP_NOT_EQUAL: return tok_lexemes[TOK_OP_NOT_EQUAL];
        case BINOP_LESS: return tok_lexemes[TOK_OP_LESS];
        case BINOP_LESS_EQUAL: return tok_lexemes[TOK_OP_LESS_EQUAL];
        case BINOP_GREATER: return tok_lexemes[TOK_OP_GREATER];
        case BINOP_GREATER_EQUAL: return tok_lexemes[TOK_OP_GREATER_EQUAL];
        case BINOP_BIT_AND: return tok_lexemes[TOK_OP_BIT_AND];
        case BINOP_BIT_OR: return tok_lexemes[TOK_OP_BIT_OR];
        case BINOP_BIT_XOR: return tok_lexemes[TOK_OP_BIT_XOR];
        case BINOP_BIT_AND_ASSIGN: return tok_lexemes[TOK_OP_BIT_AND_ASSIGN];
        case BINOP_BIT_OR_ASSIGN: return tok_lexemes[TOK_OP_BIT_OR_ASSIGN];
        case BINOP_BIT_XOR_ASSIGN: return tok_lexemes[TOK_OP_BIT_XOR_ASSIGN];
        case BINOP_BIT_ASHL: return tok_lexemes[TOK_OP_BIT_ASHL];
        case BINOP_BIT_ASHR: return tok_lexemes[TOK_OP_BIT_ASHR];
        case BINOP_BIT_ROL: return tok_lexemes[TOK_OP_BIT_ROL];
        case BINOP_BIT_ROR: return tok_lexemes[TOK_OP_BIT_ROR];
        case BINOP_BIT_LSHR: return tok_lexemes[TOK_OP_BIT_LSHR];
        case BINOP_BIT_ASHL_ASSIGN: return tok_lexemes[TOK_OP_BIT_ASHL_ASSIGN];
        case BINOP_BIT_ASHR_ASSIGN: return tok_lexemes[TOK_OP_BIT_ASHR_ASSIGN];
        case BINOP_BIT_ROL_ASSIGN: return tok_lexemes[TOK_OP_BIT_ROL_ASSIGN];
        case BINOP_BIT_ROR_ASSIGN: return tok_lexemes[TOK_OP_BIT_ROR_ASSIGN];
        case BINOP_BIT_LSHR_ASSIGN: return tok_lexemes[TOK_OP_BIT_LSHR_ASSIGN];
        case BINOP_LOG_AND: return tok_lexemes[TOK_OP_LOG_AND];
        case BINOP_LOG_OR: return tok_lexemes[TOK_OP_LOG_OR];
        case BINOP_CALL: return "()";
        default: return "";
    }
}

/* GraphViz AST rendering for debugging and visualization. */
#ifdef NEO_EXTENSION_AST_RENDERING
#include <time.h>
#include <graphviz/gvc.h>

static Agnode_t *create_colored_node(Agraph_t *g, const astnode_t *target, const char *name, uint32_t c) {
    neo_dassert(g != NULL && target != NULL, "Invalid arguments");
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRIu32, c);
    Agnode_t *n = agnode(g, buf, 1);
    bool is_block = target->type == ASTNODE_BLOCK;
    char bname[128];
    if (is_block) {
        snprintf(bname, sizeof(bname), "%s depth=%" PRIu32, block_names[target->dat.n_block.scope], target->dat.n_block.scope_depth);
    }
    name = name && *name ? name : is_block ? bname : astnode_names[target->type];
    agsafeset(n, "label", (char *)name, "");
    agsafeset(n, "style", "filled", ""); /* make it filled */
    agsafeset(n, "color", "transparent", ""); /* hide outline */
    if (ASTNODE_LITERAL_MASK & astmask(target->type)) {
        agsafeset(n, "fillcolor", target->type == ASTNODE_IDENT_LIT ? "lightblue" : "peachpuff", "");
    } else {
        bool affect_cf = ASTNODE_CONTROL_FLOW & astmask(target->type) || (target->type == ASTNODE_BINARY_OP && target->dat.n_binary_op.opcode == BINOP_CALL); /* node affects control flow? */
        agsafeset(n, "fillcolor", affect_cf ? "coral1" : "aquamarine1", ""); /* fill color */
    }
    return n;
}

#if 0
static void create_symtab_node(Agraph_t *g, Agnode_t *gnode, const symtab_t *tab) {
    if (neo_unlikely(!tab || !tab->len)) { return; }
    char buf[(1<<5)+1];
    snprintf(buf, sizeof(buf), "%p", (const void*)tab);
    Agnode_t *n = agnode(g, buf, 1);
    char html[(1<<14)+1];
    int o = snprintf(html, sizeof(html), "<table>\n");
    for (uint32_t i = 0; i < tab->len; ++i) {
        o += snprintf(html+o, sizeof(html)-(size_t)o, "\t<tr><td>%.*s</td></tr>\n", tab->p[i].callee_ident.span.len, tab->symbols[i].callee_ident.span.needle);
    }
    snprintf(html+o, sizeof(html)-(size_t)o, "</table>");
    agsafeset(n, "label", agstrdup_html(g, html), "");
    agsafeset(n, "shape", "box", "");
    agsafeset(n, "style", "filled", "");
    agsafeset(n, "color", "transparent", "");
    agsafeset(n, "fillcolor", "cornsilk2", "");
    Agedge_t *e = agedge(g, gnode, n, NULL, 1);
    agsafeset(e, "label", (char*)tab->name, "");
}
#endif

static Agnode_t *graph_append(
    astnode_t *anode,
    Agraph_t *graph,
    Agnode_t *gnode,
    uint32_t *id,
    const char *name,
    const char *color,
    const char *edge
) {
    neo_dassert(anode != NULL && graph != NULL && gnode != NULL && id != NULL, "Invalid arguments");
    Agnode_t *node = create_colored_node(graph, anode, name, *id);
    Agedge_t *edge2 = agedge(graph, gnode, node, NULL, 1);
    if (edge) {
        agsafeset(edge2, "label", (char* )edge, "");
    }
    if (color) {
        agsafeset(node, "fillcolor", (char *)color, "");
    }
    return node;
}

static void graphviz_ast_visitor(
    astpool_t *pool,
    Agraph_t *graph,
    Agnode_t *anode,
    astref_t noderef,
    uint32_t *id,
    const char *edge
) {
    neo_dassert(pool != NULL && graph != NULL && anode != NULL && id != NULL, "Invalid arguments");
    if (astref_isnull(noderef)) { return; }
    astnode_t *node = astpool_resolve(pool, noderef);
    neo_dassert(node != NULL, "AST node is NULL");
    ++*id;
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
            graph_append(node, graph, anode, id, data->message ? data->message : "Unknown error", "red", edge);
        } return;
        case ASTNODE_BREAK: {
            graph_append(node, graph, anode, id, NULL, NULL, edge);
        } return;
        case ASTNODE_CONTINUE: {
            graph_append(node, graph, anode, id, NULL, NULL, edge);
        } return;
        case ASTNODE_INT_LIT: {
            const node_int_literal_t *data = &node->dat.n_int_lit;
            char buf[64];
            snprintf(buf, sizeof(buf), data->value > 0xffff ? "0x%" PRIx64 : "%" PRIi64, data->value);
            graph_append(node, graph, anode, id, buf, NULL, edge);
        } return;
        case ASTNODE_FLOAT_LIT: {
            const node_float_literal_t *data = &node->dat.n_float_lit;
            char buf[64];
            snprintf(buf, sizeof(buf), "%f", data->value);
            graph_append(node, graph, anode, id, buf, NULL, edge);
        } return;
        case ASTNODE_CHAR_LIT: {
            const node_char_literal_t *data = &node->dat.n_char_lit;
            char buf[64];
            snprintf(buf, sizeof(buf), data->value <= 0x7f ? "%c" : "%" PRIx32, data->value);
            graph_append(node, graph, anode, id, buf, NULL, edge);
        } return;
        case ASTNODE_BOOL_LIT: {
            const node_bool_literal_t *data = &node->dat.n_bool_lit;
            graph_append(node, graph, anode, id, data->value ? "true" : "false", NULL, edge);
        } return;
        case ASTNODE_STRING_LIT: {
            const node_string_literal_t *data = &node->dat.n_string_lit;
            char buf[8192];
            snprintf(buf, sizeof(buf), "\"%s\"", data->str);
            graph_append(node, graph, anode, id, buf, NULL, edge);
        } return;
        case ASTNODE_IDENT_LIT: {
            const node_ident_literal_t *data = &node->dat.n_ident_lit;
            char buf[8192];
            snprintf(buf, sizeof(buf), "%.*s", data->span.len, data->span.p);
            graph_append(node, graph, anode, id, buf, NULL, edge);
        } return;
        case ASTNODE_SELF_LIT: {
            graph_append(node, graph, anode, id, NULL, NULL, edge);
        } return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->child_expr, id, " child");
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
            Agnode_t *nn = graph_append(node, graph, anode, id, unary_op_lexeme(data->opcode), NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->child_expr, id, " child");
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
            Agnode_t *nn = graph_append(node, graph, anode, id, binary_op_lexeme(data->opcode), NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->left_expr, id, " left");
            graphviz_ast_visitor(pool, graph, nn, data->right_expr, id, " right");
        } return;
        case ASTNODE_FUNCTION: {
            const node_method_t *data = &node->dat.n_method;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->ident, id, " ident");
            graphviz_ast_visitor(pool, graph, nn, data->params, id, " params");
            graphviz_ast_visitor(pool, graph, nn, data->ret_type, id, " ret-type");
            graphviz_ast_visitor(pool, graph, nn, data->body, id, " body");
        } return;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &node->dat.n_block;
            const astref_t *children = astpool_resolvelist(pool, data->nodes);
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            char buf[64];
            for (uint32_t i = 0; i < data->len; ++i) {
                snprintf(buf, sizeof(buf), " child %" PRIu32, i+1);
                graphviz_ast_visitor(pool, graph, nn, children[i], id, buf);
            }
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->ident, id, " ident");
            graphviz_ast_visitor(pool, graph, nn, data->type, id, " type");
            graphviz_ast_visitor(pool, graph, nn, data->init_expr, id, " init-expr");
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->child_expr, id, " child");
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->cond_expr, id, " cond-expr");
            graphviz_ast_visitor(pool, graph, nn, data->true_block, id, " true-block");
            graphviz_ast_visitor(pool, graph, nn, data->false_block, id, " false-block");
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->cond_expr, id, " cond-expr");
            graphviz_ast_visitor(pool, graph, nn, data->true_block, id, " body");
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->ident, id, " ident");
            graphviz_ast_visitor(pool, graph, nn, data->body, id, " body");
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
            Agnode_t *nn = graph_append(node, graph, anode, id, NULL, NULL, edge);
            graphviz_ast_visitor(pool, graph, nn, data->ident, id, " ident");
            graphviz_ast_visitor(pool, graph, nn, data->body, id, " body");
        } return;
        case ASTNODE__COUNT: neo_unreachable();
    }
}

static void graph_submit(astpool_t *pool, Agraph_t *g, astref_t node) {
    neo_dassert(pool != NULL && g != NULL, "Invalid arguments");
    char statsbuf[512];
    int off = snprintf(
        statsbuf,
        sizeof(statsbuf),
        "Abstract Syntax Tree\nNodes: %zu\nPoolCap: %zu\nAllocated: %.02fKiB\nAllocs: %zu",
        pool->node_pool.len/sizeof(astnode_t),
        pool->node_pool.cap/sizeof(astnode_t),
        (double)(pool->node_pool.cap+pool->list_pool.cap)/1024.0,
        pool->node_pool.num_allocs+pool->list_pool.num_allocs
    );
    time_t timer = time(NULL);
    struct tm tm = {0};
#if NEO_COM_MSVC
    localtime_s(&tm, &timer);
#else
    tm = *localtime(&timer); /* TODO: replace with threadsafe localtime */
#endif
    strftime(statsbuf+off, sizeof(statsbuf)-1-(size_t)off, "\nDate: %d.%m.%Y %H:%M:%S", &tm);
    Agnode_t *root = agnode(g, statsbuf, 1);
    agsafeset(root, "style", "filled", "");
    agsafeset(root, "color", "transparent", "");
    agsafeset(root, "fillcolor", "azure", "");
    agsafeset(root, "shape", "box", "");
    Agnode_t *program = agnode(g, "NEO PROGRAM", 1);
    agsafeset(program, "style", "filled", "");
    agsafeset(program, "color", "transparent", "");
    agsafeset(program, "fillcolor", "orchid1", "");
    agsafeset(program, "shape", "box", "");
    uint32_t id = 0;
    graphviz_ast_visitor(pool, g, program, node, &id, NULL);
}

void ast_node_graphviz_dump(astpool_t *pool, astref_t root, FILE *f) {
    neo_dassert(pool != NULL && f != NULL, "Invalid arguments");
    neo_info("dumping AST to graphviz representation...%s", "");
    time_t timer = time(NULL);
    struct tm tm = {0};
#if NEO_COM_MSVC
    localtime_s(&tm, &timer);
#else
    tm = *localtime(&timer); /* TODO: replace with threadsafe localtime */
#endif
    char buf[1<<6];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &tm);
    fprintf(f, "// Neo AST graphviz representation code - optimized for DOT engine\n");
    fprintf(f, "// Autogenerated - do NOT edit! Generated on: %s\n", buf);
    fprintf(f, "// Each root is of type ASTNode* in the C code, each edge is a pointer to a child root\n");
    GVC_t *gvc = gvContext();
    Agraph_t *g = agopen("AST", Agdirected, NULL);
    graph_submit(pool, g, root);
    gvLayout(gvc, g, "dot");
    gvRender(gvc, g, "dot", f);
    gvFreeLayout(gvc, g);
    agclose(g);
    gvFreeContext(gvc);
}

void ast_node_graphviz_render(astpool_t *pool, astref_t root, const char *filename) {
    neo_dassert(pool != NULL && filename != NULL, "Invalid arguments");
    neo_info("rendering AST to image: '%s'...", filename);
    GVC_t *gvc = gvContext();
    Agraph_t *g = agopen("AST", Agdirected, NULL);
    graph_submit(pool, g, root);
    gvLayout(gvc, g, "dot");
    gvRenderFilename(gvc, g, "jpg", filename);
    gvFreeLayout(gvc, g);
    agclose(g);
    gvFreeContext(gvc);
}

#endif

#if 0 /* Copy and paste this skeleton to quickly create a new AST visitor. */
static void my_ast_visitor(astpool_t *pool, astref_t noderef, void *user) {
    astnode_t *node = astpool_resolve(pool, noderef); (void)user;
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
        } return;
        case ASTNODE_BREAK: {

        } return;
        case ASTNODE_CONTINUE: {

        } return;
        case ASTNODE_INT_LIT: {
            const node_int_literal_t *data = &node->dat.n_int_lit;
        } return;
        case ASTNODE_FLOAT_LIT: {
            const node_float_literal_t *data = &node->dat.n_float_lit;
        } return;
        case ASTNODE_CHAR_LIT: {
            const node_char_literal_t *data = &node->dat.n_char_lit;
        } return;
        case ASTNODE_BOOL_LIT: {
            const node_bool_literal_t *data = &node->dat.n_bool_lit;
        } return;
        case ASTNODE_STRING_LIT: {
            const node_string_literal_t *data = &node->dat.n_string_lit;
        } return;
        case ASTNODE_IDENT_LIT: {
            const node_ident_literal_t *data = &node->dat.n_ident_lit;
        } return;
        case ASTNODE_SELF_LIT: {

        } return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
        } return;
        case ASTNODE_FUNCTION: {
            const node_method_t *data = &node->dat.n_method;
        } return;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &node->dat.n_block;
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
        } return;
        case ASTNODE__COUNT: neo_unreachable();
    }
}
#endif
