// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <fstream>
#include "neo_compiler.hpp"
#include <neo_compiler.c>

using namespace neo;

#define test_typeof_expr(expr2, expected_type, var2) \
    TEST(compiler, typeof_##expected_type##_##var2) { \
        static constexpr const char8_t *src = { \
            u8"class Test\n" \
                u8"func f()\n" \
                    u8"let x: int = " u8##expr2 "\n" \
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
        typeid_t tid;\
        bool ok = deduce_typeof_expr(pool, &ev, expr, &tid); \
        ASSERT_TRUE(ok); \
        errvec_print(&ev, stdout, true); \
        ASSERT_TRUE(errvec_isempty(ev)); \
        ASSERT_EQ(tid, expected_type); \
        errvec_free(&ev); \
    }

test_typeof_expr("22", TYPEID_INT, int)
test_typeof_expr("0xfefe ^ (32-1)", TYPEID_INT, int_expr)
test_typeof_expr("1.0", TYPEID_FLOAT, float)
test_typeof_expr("2.5*0.5", TYPEID_FLOAT, float_expr)

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

TEST(compiler, compiletest_file) {
    source_code source {"test/files/test.neo"};

    compiler compiler {};
    ASSERT_TRUE(compiler(source));
}

TEST(compiler, render_ast_test_small_file) {
    source_code source {"test/files/test_small.neo"};

    compiler compiler {};
    compiler |= COM_FLAG_RENDER_AST;
    ASSERT_TRUE(compiler(source));
}

#include <filesystem>
#include <vector>

[[nodiscard]] static auto load_all_source_files_from_dir(const std::string &dir) {
    std::vector<neo::source_code> files {};
    for (auto &&entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            files.emplace_back(neo::source_code{entry.path().string()});
        }
    }
    return files;
}

TEST(compile_files, accept) {
    std::vector<neo::source_code> sources {load_all_source_files_from_dir("test/files/semantic/accept")};
    for (auto &&src : sources) {
        std::cout << "Compiling valid: " << src.get_file_name() << std::endl;
        compiler compiler {};
        ASSERT_TRUE(compiler(src));
    }
}

TEST(compile_files, reject) {
    std::vector<neo::source_code> sources {load_all_source_files_from_dir("test/files/semantic/reject")};
    for (auto &&src : sources) {
        std::cout << "Parsing invalid: " << src.get_file_name() << std::endl;
        compiler compiler {};
        ASSERT_FALSE(compiler(src));
    }
}
