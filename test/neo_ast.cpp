// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_ast.h>

extern "C" astnode_t *get_mock_var(neo_mempool_t *pool);
extern "C" astnode_t *get_mock_class(neo_mempool_t *pool);

TEST(ast, allocate_node) {
    astpool_t pool {};
    astpool_init(&pool);

    for (int i = 1; i < 0xffff; ++i) {
        astref_t ref = astnode_new_int(&pool, i == 0xffff>>1 ? 42 : 11);
        ASSERT_EQ(ref, i);
        ASSERT_NE(ref, ASTREF_NULL);
        ASSERT_FALSE(astref_isnull(ref));
        ASSERT_TRUE(astpool_isvalidref(&pool, ref));
        astnode_t *node = astpool_resolve(&pool, ref);
        ASSERT_EQ(node->type, ASTNODE_INT_LIT);
        ASSERT_EQ(node->dat.n_int_lit.value, i == 0xffff>>1 ? 42 : 11);
    }

    ASSERT_EQ(pool.node_pool.len, sizeof(astnode_t)*(0xffff-1));
    astnode_t *node = neo_mempool_getelementptr(pool.node_pool, (0xffff>>1)-1, astnode_t);
    ASSERT_EQ(node->dat.n_int_lit.value, 42);
    node = neo_mempool_getelementptr(pool.node_pool, 22, astnode_t);
    ASSERT_EQ(node->dat.n_int_lit.value, 11);

    astref_t ref = ASTREF_NULL;
    ASSERT_TRUE(astref_isnull(ref));
    ASSERT_FALSE(astpool_isvalidref(&pool, ref));
    node = astpool_resolve(&pool, ref);
    ASSERT_EQ(node, nullptr);

    astpool_free(&pool);
}

#if 0
TEST(ast, block_push_children) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    node_block_t block {
        .blktype = BLOCK_LOCAL
    };
    ASSERT_EQ(block.len, 0);
    ASSERT_EQ(block.nodes, nullptr);

    astnode_t *var = get_mock_var(&pool);

    node_block_push_child(&pool, &block, var);
    ASSERT_EQ(block.len, 1);
    ASSERT_NE(block.nodes, nullptr);
    ASSERT_NE(block.cap, 0);
    ASSERT_EQ(block.nodes[0], var);
    ASSERT_TRUE(block.nodes[0]->opcode == ASTNODE_VARIABLE);
    
    for (int i {}; i < 512; ++i) {
        node_block_push_child(&pool, &block, var);
    }

    ASSERT_EQ(block.len, 1+512);
    ASSERT_NE(block.nodes, nullptr);
    ASSERT_NE(block.cap, 0);
    for (int i {}; i < block.len; ++i) {
        ASSERT_EQ(block.nodes[i], var);
    }

    astnode_t *node = astnode_new_block(&pool, &block);

    ASSERT_EQ(node->dat.n_block.len, 1+512);
    ASSERT_NE(node->dat.n_block.nodes, nullptr);
    ASSERT_NE(node->dat.n_block.cap, 0);

    neo_mempool_free(&pool);
}

TEST(ast, int_literal) {
    neo_mempool_t mempool {};
    neo_mempool_init(&mempool, 1024);

    astnode_t *intNode = astnode_new_int(&mempool, 42);

    ASSERT_EQ(intNode->type, ASTNODE_INT_LIT);
    ASSERT_EQ(intNode->dat.n_int_lit.value, 42);

    neo_mempool_free(&mempool);
}

TEST(ast, unary_op) {
    neo_mempool_t mempool {};
    neo_mempool_init(&mempool, 1024);

    astnode_t *operandNode = astnode_new_int(&mempool, 10);
    node_unary_op_t unaryOpNodeData;
    unaryOpNodeData.opcode = UNOP_MINUS;
    unaryOpNodeData.expr = operandNode;
    astnode_t *unaryOpNode = astnode_new_unary_op(&mempool, &unaryOpNodeData);

    ASSERT_EQ(unaryOpNode->type, ASTNODE_UNARY_OP);
    ASSERT_EQ(unaryOpNode->dat.n_unary_op.opcode, UNOP_MINUS);
    ASSERT_EQ(unaryOpNode->dat.n_unary_op.expr, operandNode);

    neo_mempool_free(&mempool);
}

TEST(ast, group) {
    neo_mempool_t mempool {};
    neo_mempool_init(&mempool, 1024);

    node_group_t groupData;
    astnode_t *childNode = astnode_new_int(&mempool, 42);
    groupData.child_expr = childNode;
    astnode_t *groupNode = astnode_new_group(&mempool, &groupData);

    ASSERT_EQ(groupNode->type, ASTNODE_GROUP);
    ASSERT_EQ(groupNode->dat.n_group.child_expr, childNode);

    neo_mempool_free(&mempool);
}

TEST(ast, visit) {
    neo_mempool_t mempool {};
    neo_mempool_init(&mempool, 32);

    astnode_t *mock = get_mock_class(&mempool);
    astnode_validate(mock);

    static void *mmock;
    mmock = mock;
    static int count {};
    auto visitor = [](astnode_t *node, void *data) -> void {
        ASSERT_EQ(data, mmock);
        ++count;
    };
    astnode_visit(mock, visitor, mock);
    ASSERT_EQ(count, 15);

    neo_mempool_free(&mempool);
}
#endif
