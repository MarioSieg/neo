// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_ast.h>

extern "C" astnode_t *get_mock_var(neo_mempool_t *pool);
extern "C" astnode_t *get_mock_class(neo_mempool_t *pool);

TEST(ast, new_error) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    node_error_t node_data {
        .message = "Oh no error :("
    };

    astnode_t *node = astnode_new_error(&pool, &node_data);
    ASSERT_EQ(node->type, ASTNODE_ERROR);
    ASSERT_STREQ(node->dat.n_error.message, "Oh no error :(");

    neo_mempool_free(&pool);
}

TEST(ast, new_int) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    astnode_t *node = astnode_new_int(&pool, 42);
    ASSERT_EQ(node->type, ASTNODE_INT_LIT);
    ASSERT_EQ(node->dat.n_int_lit.value, 42);

    neo_mempool_free(&pool);
}

TEST(ast, new_char) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    astnode_t *node = astnode_new_char(&pool, U'ä');
    ASSERT_EQ(node->type, ASTNODE_CHAR_LIT);
    ASSERT_EQ(node->dat.n_char_lit.value, U'ä');

    neo_mempool_free(&pool);
}

TEST(ast, new_float) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    astnode_t *node = astnode_new_float(&pool, 42.0);
    ASSERT_EQ(node->type, ASTNODE_FLOAT_LIT);
    ASSERT_DOUBLE_EQ(node->dat.n_float_lit.value, 42.0);

    neo_mempool_free(&pool);
}

TEST(ast, new_string) {
    neo_mempool_t pool {};
    neo_mempool_init(&pool, 1024);

    astnode_t *node = astnode_new_string(&pool, srcspan_from("Noelle!"));
    ASSERT_EQ(node->type, ASTNODE_STRING_LIT);
    ASSERT_TRUE(srcspan_eq(node->dat.n_string_lit.span, srcspan_from("Noelle!")));
    ASSERT_EQ(node->dat.n_string_lit.hash, srcspan_hash(srcspan_from("Noelle!")));

    neo_mempool_free(&pool);
}

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
    ASSERT_TRUE(block.nodes[0]->type == ASTNODE_VARIABLE);
    
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
    unaryOpNodeData.type = UNOP_MINUS;
    unaryOpNodeData.expr = operandNode;
    astnode_t *unaryOpNode = astnode_new_unary_op(&mempool, &unaryOpNodeData);

    ASSERT_EQ(unaryOpNode->type, ASTNODE_UNARY_OP);
    ASSERT_EQ(unaryOpNode->dat.n_unary_op.type, UNOP_MINUS);
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
