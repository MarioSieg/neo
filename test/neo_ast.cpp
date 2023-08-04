// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_ast.h>

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
