/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_ast.h"

/* Implement AST node factory methods */

#define impl_ast_node_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool, const node_##name##_t *node) {\
    neo_asd(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(*node));\
    nn->type = ASTNODE_##ttype;\
    nn->dat.n_##name = *node;\
    return nn;\
  }

#define impl_ast_node_hull_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool) {\
    neo_asd(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));\
    nn->type = ASTNODE_##ttype;\
    return nn;\
  }

#define impl_ast_node_literal_factory(name, ttype)\
  astnode_t *astnode_new_##name(neo_mempool_t *pool, neo_##name##_t value) {\
    neo_asd(pool);\
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));\
    nn->type = ASTNODE_##ttype;\
    nn->dat.n_##name##_lit.value = value;\
    return nn;\
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

astnode_t *astnode_new_string(neo_mempool_t *pool, srcspan_t value) {
    neo_asd(pool);
    astnode_t *nn = neo_mempool_alloc(pool, sizeof(astnode_t));
    nn->type = ASTNODE_STRING_LIT;
    nn->dat.n_string_lit.span = value;
    nn->dat.n_string_lit.hash = srcspan_hash(value);
    return nn;
}

astnode_t *astnode_new_ident(neo_mempool_t *pool, srcspan_t value) {
    astnode_t *nn = astnode_new_string(pool, value);
    nn->type = ASTNODE_IDENT_LIT;
    return nn;
}

static const uint64_t NEO_UNUSED ast_node_block_masks[BLOCK__COUNT] = { /* This table contains masks of the allowed ASTNODE_* types for each block type inside a node_block_t. */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_CLASS), /* BLOCK_MODULE */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_METHOD)|astmask(ASTNODE_VARIABLE), /* BLOCK_CLASS */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE)|astmask(ASTNODE_BRANCH) /* BLOCK_LOCAL */
    |astmask(ASTNODE_LOOP)|astmask(ASTNODE_UNARY_OP)|astmask(ASTNODE_BINARY_OP)|astmask(ASTNODE_GROUP) /* BLOCK_LOCAL */
    |astmask(ASTNODE_RETURN)|astmask(ASTNODE_BREAK)|astmask(ASTNODE_CONTINUE), /* BLOCK_LOCAL */
    astmask(ASTNODE_ERROR)|astmask(ASTNODE_VARIABLE) /* BLOCK_PARAMLIST */
};

void node_block_push_child(neo_mempool_t *pool, node_block_t *block, astnode_t *node) {
    neo_asd(pool && block);
    if (neo_unlikely(!node)) {
        return;
    } else if (!block->nodes || !block->len) { /* No nodes yet, so allocate. */
        block->len = 1;
        block->nodes = neo_mempool_alloc(pool, (block->cap=1<<8)*sizeof(astnode_t *));
        *block->nodes = node;
    } else if (block->len >= block->cap) { /* Reallocate if necessary. */
        block->nodes = neo_mempool_alloc(pool, (block->cap<<=2)*sizeof(astnode_t *)); /* Wasting a lot of memory here, because we can't individually free the previous block. :/ */
        block->nodes[block->len++] = node;
    } else { /* Otherwise, just push. */
        block->nodes[block->len++] = node;
    }
}

#undef impl_ast_node_literal_factory
#undef impl_ast_node_hull_factory
#undef impl_ast_node_factory

static void astnode_visit_root_impl(astnode_t *root, void (*visitor)(astnode_t *node, void *user), void *user, size_t *c) {
    if (neo_unlikely(!root)) { return; } /* Skip NULL nodes. */
    neo_asd(visitor && c);
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
            neo_asd(!!(ASTNODE_LEAF_MASK&astmask(root->type)));
        } return; /* Visitor invocation is redundant. */

        case ASTNODE_GROUP: {
            const node_group_t *data = &root->dat.n_group;
            astnode_visit_root_impl(data->child_expr, visitor, user, c);
        } break;

        case ASTNODE_UNARY_OP: {
            const node_unary_op_t *data = &root->dat.n_unary_op;
            astnode_visit_root_impl(data->expr, visitor, user, c);
        } break;

        case ASTNODE_BINARY_OP: {
            const node_binary_op_t *data = &root->dat.n_binary_op;
            astnode_visit_root_impl(data->left_expr, visitor, user, c);
            astnode_visit_root_impl(data->right_expr, visitor, user, c);
        } break;

        case ASTNODE_METHOD: {
            const node_method_t *data = &root->dat.n_method;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->params, visitor, user, c);
            astnode_visit_root_impl(data->ret_type, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;

        case ASTNODE_BLOCK: {
            const node_block_t *data = &root->dat.n_block;
            for (uint32_t i = 0; i < data->len; ++i) {
                astnode_visit_root_impl(data->nodes[i], visitor, user, c);
            }
        } break;

        case ASTNODE_VARIABLE: {
            const node_variable_t *data = &root->dat.n_variable;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->type, visitor, user, c);
            astnode_visit_root_impl(data->init_expr, visitor, user, c);
        } break;

        case ASTNODE_RETURN: {
            const node_return_t *data = &root->dat.n_return;
            astnode_visit_root_impl(data->child_expr, visitor, user, c);
        } break;

        case ASTNODE_BRANCH: {
            const node_branch_t *data = &root->dat.n_branch;
            astnode_visit_root_impl(data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(data->true_block, visitor, user, c);
            astnode_visit_root_impl(data->false_block, visitor, user, c);
        } break;

        case ASTNODE_LOOP: {
            const node_loop_t *data = &root->dat.n_loop;
            astnode_visit_root_impl(data->cond_expr, visitor, user, c);
            astnode_visit_root_impl(data->true_block, visitor, user, c);
        } break;

        case ASTNODE_CLASS: {
            const node_class_t *data = &root->dat.n_class;
            astnode_visit_root_impl(data->ident, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;

        case ASTNODE_MODULE: {
            const node_module_t *data = &root->dat.n_module;
            astnode_visit_root_impl(data->name, visitor, user, c);
            astnode_visit_root_impl(data->body, visitor, user, c);
        } break;

        default: {
            neo_panic("invalid node type: %d", root->type);
        }
    }
    (*visitor)(root, user);
}

size_t astnode_visit(astnode_t *root, void (*visitor)(astnode_t *node, void *user), void *user) {
    size_t c = 0;
    astnode_visit_root_impl(root, visitor, user, &c);
    return c;
}

static void ast_validator(astnode_t *node, void *user) {
    (void)node;
    (void)user;
}

void astnode_validate(astnode_t *root) {
    astnode_visit(root, &ast_validator, NULL);
}
