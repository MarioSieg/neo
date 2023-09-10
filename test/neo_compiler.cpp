// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <fstream>
#include "neo_compiler.hpp"

using namespace neo;

TEST(compiler, compile_test_file) {
    neo_source_code source {reinterpret_cast<const std::uint8_t *>(u8"test/files/特羅洛洛.neo")};

    neo_compiler compiler {};
    ASSERT_TRUE(compiler(source));
}

TEST(compiler, render_ast_test_file) {
    neo_source_code source {"test/files/test.neo"};

    neo_compiler compiler {};
    compiler |= COM_FLAG_RENDER_AST;
    ASSERT_TRUE(compiler(source));
}
