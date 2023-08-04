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
        block->nodes = neo_mempool_alloc(pool, (block->cap<<=1)*sizeof(astnode_t *)); /* Wasting a lot of memory here, but we can't free the individual pool. :/ */
        block->nodes[block->len++] = node;
    } else { /* Otherwise, just push. */
        block->nodes[block->len++] = node;
    }
}

#undef impl_ast_node_literal_factory
#undef impl_ast_node_hull_factory
#undef impl_ast_node_factory
