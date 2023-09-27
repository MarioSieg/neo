// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <fstream>
#include "neo_compiler.hpp"
#include <neo_compiler.c>

using namespace neo;

#define test_typeof_expr(expr2, expected_type, var) \
    TEST(compiler, typeof_##expected_type##_##var) { \
        static constexpr const char8_t *src = { \
            u8"class Test\n" \
                u8"method f()\n" \
                    u8##expr2 \
                u8"end\n" \
            u8"end\n" \
        }; \
        source_code source { \
            reinterpret_cast<const std::uint8_t *>(u8"test.neo"), \
            reinterpret_cast<const std::uint8_t *>(src) \
        }; \
        compiler compiler {}; \
        ASSERT_TRUE(compiler(source)); \
        const astpool_t *pool {}; \
        astref_t ast = compiler.get_ast_root(pool); \
        const astnode_t *node {astpool_resolve(pool, ast)}; \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_MODULE); \
        node = astpool_resolve(pool, node->dat.n_module.body); \
        ASSERT_EQ(node->type, ASTNODE_BLOCK); \
        ASSERT_NE(node->dat.n_block.len, 0); \
        node = astpool_resolve(pool, astpool_resolvelist(pool, node->dat.n_block.nodes)[0]); \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_CLASS); \
        node = astpool_resolve(pool, node->dat.n_class.body); \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_BLOCK); \
        node = astpool_resolve(pool, astpool_resolvelist(pool, node->dat.n_block.nodes)[0]); \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_FUNCTION); \
        node = astpool_resolve(pool, node->dat.n_method.body); \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_BLOCK); \
        node = astpool_resolve(pool, astpool_resolvelist(pool, node->dat.n_block.nodes)[0]); \
        ASSERT_NE(node, nullptr); \
        ASSERT_EQ(node->type, ASTNODE_VARIABLE); \
        const node_variable_t *var = &node->dat.n_variable; \
        astref_t expr = var->init_expr; \
        node = astpool_resolve(pool, expr); \
        ASSERT_NE(node, nullptr); \
        ASSERT_NE(astmask(node->type) & ASTNODE_EXPR_MASK, 0); \
        \
        error_vector_t ev {}; \
        errvec_init(&ev); \
        typeid_t type = deduce_typeof_expr(pool, &ev, expr); \
        errvec_print(&ev, stdout, true); \
        ASSERT_TRUE(errvec_isempty(ev)); \
        ASSERT_EQ(type, expected_type); \
        errvec_free(&ev); \
    }

TEST(compiler, compile_test_file) {
    source_code source {reinterpret_cast<const std::uint8_t *>(u8"test/files/特羅洛洛.neo")};

    compiler compiler {};
    ASSERT_TRUE(compiler(source));
}

TEST(compiler, render_ast_test_file) {
    source_code source {"test/files/test.neo"};

    compiler compiler {};
    compiler |= COM_FLAG_RENDER_AST;
    ASSERT_TRUE(compiler(source));
}
