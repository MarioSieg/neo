/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_ast.h"

/* Implement AST node factory methods */

#define impl_ast_node_factory(name, ttype)\
  astref_t astnode_new_##name(astpool_t *pool, const node_##name##_t *node) {\
    neo_dassert(pool);\
    astnode_t *nn = NULL;\
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_##ttype);\
    nn->dat.n_##name = *node;\
    return ref;\
  }

#define impl_ast_node_hull_factory(name, ttype)\
  astref_t astnode_new_##name(astpool_t *pool) {\
    neo_dassert(pool);\
    return astpool_alloc(pool, NULL,  ASTNODE_##ttype);\
  }

#define impl_ast_node_literal_factory(name, ttype)\
  astref_t astnode_new_##name(astpool_t *pool, neo_##name##_t value) {\
    neo_dassert(pool);\
    astnode_t *nn = NULL;\
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_##ttype);\
    nn->dat.n_##name##_lit.value = value;\
    return ref;\
  }

impl_ast_node_factory(error, ERROR)
impl_ast_node_factory(group, GROUP)
impl_ast_node_factory(unary_op, UNARY_OP)
impl_ast_node_factory(binary_op, BINARY_OP)
impl_ast_node_factory(method, METHOD)
impl_ast_node_factory(block, BLOCK)
impl_ast_node_factory(variable, VARIABLE)
impl_ast_node_factory(return, RETURN)
impl_ast_node_hull_factory(break, BREAK)
impl_ast_node_hull_factory(continue, CONTINUE)
impl_ast_node_factory(branch, BRANCH)
impl_ast_node_factory(loop, LOOP)
impl_ast_node_factory(class, CLASS)
impl_ast_node_factory(module, MODULE)

impl_ast_node_literal_factory(int, INT_LIT)
impl_ast_node_literal_factory(float, FLOAT_LIT)
impl_ast_node_literal_factory(char, CHAR_LIT)
impl_ast_node_literal_factory(bool, BOOL_LIT)

astref_t astnode_new_string(astpool_t *pool, srcspan_t value) {
    neo_dassert(pool);
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_STRING_LIT);
    nn->dat.n_string_lit.span = value;
    nn->dat.n_string_lit.hash = srcspan_hash(value);
    return ref;
}

astref_t astnode_new_ident(astpool_t *pool, srcspan_t value) {
    astref_t ref = astnode_new_string(pool, value);
    astpool_resolve(pool, ref)->type = ASTNODE_IDENT_LIT;
    return ref;
}

astref_t astnode_new_block_with_nodes(astpool_t *pool, block_scope_t type, astref_t *nodes) {
    neo_dassert(pool);
    node_block_t block = {.blktype = type};
    while (astpool_isvalidref(pool, *nodes)) {
        node_block_push_child(pool, &block, *nodes++);
    }
    astnode_t *nn = NULL;
    astref_t ref = astpool_alloc(pool, &nn, ASTNODE_BLOCK);
    nn->dat.n_block = block;
    return ref;
}

#define _(_1, _2) [_1] = _2
static const char *const node_names[ASTNODE__COUNT] = { nodedef(_, NEO_SEP) };
#undef _

static const uint64_t NEO_UNUSED block_valid_masks[BLOCK__COUNT] = { /* This table contains masks of the allowed ASTNODE_* types for each block type inside a node_block_t. */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_CLASS), /* BLOCK_MODULE */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_METHOD)|astmask(ASTNODE_VARIABLE), /* BLOCK_CLASS */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE)|astmask(ASTNODE_BRANCH) /* BLOCK_LOCAL */
    |astmask(ASTNODE_LOOP)|astmask(ASTNODE_UNARY_OP)|astmask(ASTNODE_BINARY_OP)|astmask(ASTNODE_GROUP) /* BLOCK_LOCAL */
    |astmask(ASTNODE_RETURN)|astmask(ASTNODE_BREAK)|astmask(ASTNODE_CONTINUE), /* BLOCK_LOCAL */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE) /* BLOCK_PARAMLIST */
};

static const char *const block_names[BLOCK__COUNT] = {
    "module",
    "class",
    "local",
    "param-list"
};

void node_block_push_child(astpool_t *pool, node_block_t *block, astref_t node) {
    neo_dassert(pool && block);
    if (neo_unlikely(astref_isnull(node))) {
        return;
    } else if (!block->cap) { /* No nodes yet, so allocate. */
        block->cap=1<<6;
        block->nodes = neo_mempool_alloc(&pool->list_pool, block->cap*sizeof(*block->nodes));
    } else if (block->len >= block->cap) { /* Reallocate if necessary. */
        size_t oldlen = block->cap;
        block->cap<<=2;
        block->nodes = neo_mempool_realloc(&pool->list_pool, block->nodes, oldlen*sizeof(*block->nodes), block->cap*sizeof(*block->nodes)); /* Wasting a lot of memory here, because we can't individually free the previous block. :/ */
    }
    block->nodes[block->len++] = node;
#if NEO_DBG
    const astnode_t *pnode = astpool_resolve(pool, node);
    uint64_t mask = block_valid_masks[block->blktype];
    uint64_t node_mask = astmask(pnode->type);
    if (neo_unlikely((mask & node_mask) == 0)) {
        neo_error("Block node type '%s' is not allowed in '%s' block kind.", node_names[pnode->type], block_names[block->blktype]);
    }
    neo_assert((mask & node_mask) != 0 && "Block node type is not allowed in this block kind"); /* Check that the node type is allowed in this block type. For example, method declarations are not allowed in parameter list blocks.  */
#endif
}

#undef impl_ast_node_literal_factory
#undef impl_ast_node_hull_factory
#undef impl_ast_node_factory

static void astnode_visit_root_impl(astpool_t *pool, astref_t rootref, void (*visitor)(astpool_t *pool, astref_t node, void *user), void *user, size_t *c) {
    neo_dassert(pool && visitor && c);
    astnode_t *root = astpool_resolve(pool, rootref);
    if (neo_unlikely(!root)) { return; } /* Skip NULL nodes. */
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
        case ASTNODE_IDENT_LIT: {
            neo_dassert(!!(ASTNODE_LEAF_MASK&astmask(root->type)));
        } return; /* Visitor invocation is redundant. */
        case ASTNODE_GROUP: {
            const node_group_t *data = &root->dat.n_group;
            astnode_visit_root_impl(pool, data->child_expr, visitor, user, c);
        } break;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &root->dat.n_unary_op;
            astnode_visit_root_impl(pool, data->expr, visitor, user, c);
        } break;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &root->dat.n_binary_op;
            astnode_visit_root_impl(pool, data->left_expr, visitor, user, c);
            astnode_visit_root_impl(pool, data->right_expr, visitor, user, c);
        } break;
        case ASTNODE_METHOD: {
            const node_method_t *data = &root->dat.n_method;
            astnode_visit_root_impl(pool, data->ident, visitor, user, c);
            astnode_visit_root_impl(pool, data->params, visitor, user, c);
            astnode_visit_root_impl(pool, data->ret_type, visitor, user, c);
            astnode_visit_root_impl(pool, data->body, visitor, user, c);
        } break;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &root->dat.n_block;
            for (uint32_t i = 0; i < data->len; ++i) {
                astnode_visit_root_impl(pool, data->nodes[i], visitor, user, c);
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
        default: {
            neo_panic("invalid node type: %d", root->type);
        }
    }
    (*visitor)(pool, rootref, user);
}

size_t astnode_visit(astpool_t *pool, astref_t root, void (*visitor)(astpool_t *pool, astref_t node, void *user), void *user) {
    size_t c = 0;
    astnode_visit_root_impl(pool, root, visitor, user, &c);
    return c;
}

#define astverify(expr, msg) neo_assert((expr) && "AST verification failed: " msg)
#define isexpr(node) (ASTNODE_EXPR_MASK&(astmask((node)->type)))

static astnode_t *verify_resolve_node(astpool_t *pool, astref_t target) {
    neo_dassert(pool);
    astverify(astpool_isvalidref(pool, target), "AST reference is invalid");
    astnode_t *node = astpool_resolve(pool, target);
    astverify(node != NULL, "AST reference resolve returned NULL");
    return node;
}

#define verify_resolve(ref) verify_resolve_node(pool, (ref))
#define verify_expr(node) astverify(isexpr((node)), "AST Node is not an expression")
#define verify_type(node, expected) astverify((node)->type == (expected), "AST Node is not of expected type: " #expected)
#define verify_block(node, expected)\
    verify_type((node), ASTNODE_BLOCK); \
    astverify((node)->dat.n_block.blktype == (expected), "AST Node block type is not of expected block type: " #expected)

static void ast_validator(astpool_t *pool, astref_t noderef, void *user) {
    (void)user;
    neo_dassert(pool);
    astnode_t *node = astpool_resolve(pool, noderef);
    switch (node->type) {
        case ASTNODE_ERROR: {
            const node_error_t *data = &node->dat.n_error;
            astverify(data->message, "Error message is NULL");
            astverify(data->token.lexeme.p && data->token.lexeme.len, "Error token lexeme is NULL");
        } return;
        case ASTNODE_BREAK: /* No assoc data. */
        case ASTNODE_CONTINUE: /* No assoc data. */
        case ASTNODE_INT_LIT: /* No validation needed. */
        case ASTNODE_FLOAT_LIT: /* No validation needed. */
        case ASTNODE_CHAR_LIT: /* No validation needed. */
        case ASTNODE_BOOL_LIT: /* No validation needed. */
        case ASTNODE_STRING_LIT: /* No validation needed. */
        case ASTNODE_IDENT_LIT: /* No validation needed. */
            return;
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
            const astnode_t *child_expr = verify_resolve(data->child_expr);
            verify_expr(child_expr);
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
            astverify(data->opcode < UNOP__COUNT, "Unary op operator is invalid");
            const astnode_t *expr = verify_resolve(data->expr);
            verify_expr(expr);
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
            astverify(data->opcode < BINOP__COUNT, "Binary op operator is invalid");
            const astnode_t *lhs = verify_resolve(data->left_expr);
            verify_expr(lhs);
            const astnode_t *rhs = verify_resolve(data->right_expr);
            verify_expr(rhs);
        } return;
        case ASTNODE_METHOD: {
            const node_method_t *data = &node->dat.n_method;
            const astnode_t *ident = verify_resolve(data->ident);
            verify_type(ident, ASTNODE_IDENT_LIT);
            if (!astref_isnull(data->params)) { /* Optional. */
                const astnode_t *params = verify_resolve(data->params);
                verify_block(params, BLOCK_PARAMLIST);
            }
            if (!astref_isnull(data->ret_type)) { /* Optional. */
                const astnode_t *ret_type = verify_resolve(data->ret_type);
                verify_type(ret_type, ASTNODE_IDENT_LIT);
            }
            if (!astref_isnull(data->body)) { /* Optional. */
                const astnode_t *body = verify_resolve(data->body);
                verify_block(body, BLOCK_LOCAL);
            }
        } return;
        case ASTNODE_BLOCK: {
            const node_block_t *data = &node->dat.n_block;
            astverify(data->nodes != NULL, "Block nodes array is NULL");
            astverify(data->len > 0, "Block nodes array is empty");
            for (uint32_t i = 0; i < data->len; ++i) {
                astref_t child = data->nodes[i];
                const astnode_t *child_node = verify_resolve(child);
                uint64_t mask = block_valid_masks[data->blktype];
                uint64_t node_mask = astmask(child_node->type);
#if NEO_DBG
                if (neo_unlikely((mask & node_mask) == 0)) {
                    neo_error("Block node type '%s' is not allowed in '%s' block kind.", node_names[child_node->type], block_names[data->blktype]);
                }
#endif
                astverify((mask & node_mask) != 0, "Block node type is not allowed in this block kind"); /* Check that the node type is allowed in this block type. For example, method declarations are not allowed in parameter list blocks.  */
            }
            switch (data->blktype) {
                case BLOCK_MODULE: {
                    const symtab_t *class_table = data->symtabs.sc_module.class_table;
                    astverify(class_table != NULL, "Module class table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_CLASS: {
                    const symtab_t *var_table = data->symtabs.sc_class.var_table;
                    (void)var_table;
                    //astverify(var_table != NULL, "Class variable table is NULL");
                    const symtab_t *method_table = data->symtabs.sc_class.method_table;
                    (void)method_table;
                    //astverify(method_table != NULL, "Class method table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_LOCAL: {
                    const symtab_t *var_table = data->symtabs.sc_local.var_table;
                    (void)var_table;
                    //astverify(var_table != NULL, "Local variable table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                case BLOCK_PARAMLIST: {
                    const symtab_t *var_table = data->symtabs.sc_params.var_table;
                    (void)var_table;
                    //astverify(var_table != NULL, "Parameter list variable table is NULL");
                    /* TODO: Validate symtab itself. */
                } break;
                default: neo_panic("Invalid block type: %d", data->blktype);
            }
        } return;
        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &node->dat.n_variable;
            verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
            verify_type(verify_resolve(data->type), ASTNODE_IDENT_LIT);
            verify_expr(verify_resolve(data->init_expr));
        } return;
        case ASTNODE_RETURN: {
            const node_return_t *data = &node->dat.n_return;
            if (!astref_isnull(data->child_expr)) { /* Optional. */
                verify_expr(verify_resolve(data->child_expr));
            }
        } return;
        case ASTNODE_BRANCH: {
            const node_branch_t *data = &node->dat.n_branch;
            verify_expr(verify_resolve(data->cond_expr));
            verify_block(verify_resolve(data->true_block), BLOCK_LOCAL);
            if (!astref_isnull(data->false_block)) { /* Optional. */
                verify_block(verify_resolve(data->false_block), BLOCK_LOCAL);
            }
        } return;
        case ASTNODE_LOOP: {
            const node_loop_t *data = &node->dat.n_loop;
            verify_expr(verify_resolve(data->cond_expr));
            verify_block(verify_resolve(data->true_block), BLOCK_LOCAL);
        } return;
        case ASTNODE_CLASS: {
            const node_class_t *data = &node->dat.n_class;
            verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
            if (!astref_isnull(data->body)) { /* Optional. */
                verify_block(verify_resolve(data->body), BLOCK_CLASS);
            }
        } return;
        case ASTNODE_MODULE: {
            const node_module_t *data = &node->dat.n_module;
            verify_type(verify_resolve(data->ident), ASTNODE_IDENT_LIT);
            if (!astref_isnull(data->body)) { /* Optional. */
                verify_block(verify_resolve(data->body), BLOCK_MODULE);
            }
        } return;
        default: {
            neo_panic("invalid node type: %d", node->type);
        }
    }
}

void astnode_validate(astpool_t *pool, astref_t root) {
    neo_dassert(pool);
    astnode_visit(pool, root, &ast_validator, NULL);
}

#if 0 /* Copy and paste this skeleton to quickly create a new AST visitor. */
static void my_ast_validator(astnode_t *node, void *user) {
    (void)user;
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
        case ASTNODE_GROUP: {
            const node_group_t *data = &node->dat.n_group;
        } return;
        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &node->dat.n_unary_op;
        } return;
        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &node->dat.n_binary_op;
        } return;
        case ASTNODE_METHOD: {
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
        default: {
            neo_panic("invalid node type: %d", node->type);
        }
    }
}
#endif

astref_t astpool_alloc(astpool_t *self, astnode_t **o, astnode_type_t type) {
    neo_dassert(self);
    size_t plen = self->node_pool.len+sizeof(astnode_t);
    neo_assert(plen <= UINT32_MAX && "AST-pool out of nodes, max: UINT32_MAX");
    astnode_t *n = neo_mempool_alloc(&self->node_pool, sizeof(astnode_t));
    n->type = type & 255;
    if (o) { *o = n; }
    plen /= sizeof(astnode_t);
    return (astref_t)plen;
}

void astpool_init(astpool_t *self) {
    neo_dassert(self);
    neo_mempool_init(&self->node_pool, sizeof(astnode_t) * 8192);
    neo_mempool_init(&self->list_pool, sizeof(astref_t) * 8192);
}

void astpool_free(astpool_t *self) {
    neo_dassert(self);
    neo_mempool_free(&self->list_pool);
    neo_mempool_free(&self->node_pool);
}
