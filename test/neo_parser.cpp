/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neo_compiler.hpp>
#include <neo_parser.c>

#include <random>

inline const std::uint8_t *operator""_neo(const char8_t *s, size_t len) {
    return reinterpret_cast<const std::uint8_t *>(s);
}

static thread_local std::random_device rd {};
static thread_local std::mt19937_64 prng {rd()};

TEST(parse, advance) {
    error_vector_t ev;
    errvec_init(&ev);
    parser_t parser;
    parser_init(&parser, &ev);
    const source_t *src = source_from_memory_ref(u8"test"_neo, u8"3 1.4 hello"_neo, nullptr);

    parser_setup_source(&parser, src);

    ASSERT_EQ(parser.curr.type, TOK_LI_INT);
    ASSERT_EQ(parser.prev.type, TOK__COUNT);
    advance(&parser);

    ASSERT_EQ(parser.curr.type, TOK_LI_FLOAT);
    ASSERT_EQ(parser.prev.type, TOK_LI_INT);
    advance(&parser);

    ASSERT_EQ(parser.curr.type, TOK_LI_IDENT);
    ASSERT_EQ(parser.prev.type, TOK_LI_FLOAT);
    advance(&parser);

    source_free(src);
    parser_free(&parser);
    errvec_free(&ev);
}

TEST(parse, consume_match) {
    error_vector_t ev;
    errvec_init(&ev);
    parser_t parser;
    parser_init(&parser, &ev);
    const source_t *src = source_from_memory_ref(u8"test"_neo, u8"3 1.4 hello"_neo, nullptr);

    parser_setup_source(&parser, src);

    ASSERT_EQ(parser.curr.type, TOK_LI_INT);
    ASSERT_EQ(parser.prev.type, TOK__COUNT);
    advance(&parser);

    ASSERT_EQ(parser.curr.type, TOK_LI_FLOAT);
    ASSERT_EQ(parser.prev.type, TOK_LI_INT);

    ASSERT_FALSE(consume_match(&parser, TOK_LI_INT));

    ASSERT_EQ(parser.curr.type, TOK_LI_FLOAT); // no match, so curr is unchanged
    ASSERT_EQ(parser.prev.type, TOK_LI_INT); // no match, so prev is unchanged

    ASSERT_TRUE(consume_match(&parser, TOK_LI_FLOAT));

    ASSERT_EQ(parser.curr.type, TOK_LI_IDENT); // match, so curr is advanced
    ASSERT_EQ(parser.prev.type, TOK_LI_FLOAT);

    source_free(src);
    parser_free(&parser);
    errvec_free(&ev);
}

TEST(parse, consume_or_err) {
    error_vector_t ev;
    errvec_init(&ev);
    parser_t parser;
    parser_init(&parser, &ev);
    const source_t *src = source_from_memory_ref(u8"test"_neo, u8"3 1.4 hello"_neo, nullptr);

    parser_setup_source(&parser, src);

    ASSERT_EQ(parser.curr.type, TOK_LI_INT);

    consume_or_err(&parser, TOK_LI_INT, "expected int");
    ASSERT_FALSE(parser.error);
    ASSERT_EQ(parser.curr.type, TOK_LI_FLOAT);

    consume_or_err(&parser, TOK_LI_INT, "expected int");
    ASSERT_TRUE(parser.error);
    ASSERT_EQ(parser.curr.type, TOK_LI_FLOAT);
    ASSERT_STREQ(parser.prev_error, "expected int");

    source_free(src);
    parser_free(&parser);
    errvec_free(&ev);
}

TEST(parse, consume_ident) {
    error_vector_t ev;
    errvec_init(&ev);
    parser_t parser;
    parser_init(&parser, &ev);
    const source_t *src = source_from_memory_ref(u8"test"_neo, u8"3 1.4 hello"_neo, nullptr);

    parser_setup_source(&parser, src);
    advance(&parser);
    advance(&parser);
    ASSERT_EQ(parser.curr.type, TOK_LI_IDENT);
    ASSERT_EQ(parser.prev.type, TOK_LI_FLOAT);

    astref_t ident = consume_identifier(&parser, "expected identifier");
    ASSERT_EQ(parser.curr.type, TOK_ME_EOF);
    ASSERT_EQ(parser.prev.type, TOK_LI_IDENT);
    ASSERT_FALSE(parser.error);

    ASSERT_FALSE(astref_isnull(ident));
    ASSERT_TRUE(astpool_isvalidref(&parser.pool, ident));
    astnode_t *node = astpool_resolve(&parser.pool, ident);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(srcspan_eq(node->dat.n_ident_lit.span, srcspan_from("hello")));
    ASSERT_EQ(node->type, ASTNODE_IDENT_LIT);

    source_free(src);
    parser_free(&parser);
    errvec_free(&ev);
}

// binary op between two literals
#define BINARY_OP_LITERAL_LITERAL(op, opcode2) \
    TEST(parse, bin_expr_literal_literal_##opcode2) {\
        error_vector_t ev;\
        errvec_init(&ev);\
        parser_t parser;\
        parser_init(&parser, &ev);\
        std::uniform_int_distribution<neo_int_t> dist {0, 0xffff};\
        neo_int_t a = dist(prng);\
        neo_int_t b = dist(prng);\
        std::string source = std::to_string(a) + " " #op " " + std::to_string(b);\
        std::cout << "Testing expression: " << source << "\n";\
        const source_t *src = source_from_memory_ref(u8"test"_neo, reinterpret_cast<const std::uint8_t *>(source.c_str()), nullptr);\
        \
        parser_setup_source(&parser, src);\
        \
        astref_t expr = rule_expr(&parser);\
        errvec_print(&ev, stderr, true);\
        ASSERT_FALSE(parser.error);\
        ASSERT_FALSE(astref_isnull(expr));\
        ASSERT_TRUE(astpool_isvalidref(&parser.pool, expr));\
        astnode_t *expr_node = astpool_resolve(&parser.pool, expr);\
        ASSERT_NE(expr_node, nullptr);\
        ASSERT_EQ(expr_node->type, ASTNODE_BINARY_OP);\
        ASSERT_EQ(expr_node->dat.n_binary_op.opcode, BINOP_##opcode2);\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.left_expr));\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.right_expr));\
        \
        astnode_t *left = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.left_expr);\
        ASSERT_NE(left, nullptr);\
        ASSERT_EQ(left->type, ASTNODE_INT_LIT);\
        ASSERT_EQ(left->dat.n_int_lit.value, a);\
        \
        astnode_t *right = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.right_expr);\
        ASSERT_NE(right, nullptr);\
        ASSERT_EQ(right->type, ASTNODE_INT_LIT);\
        ASSERT_EQ(right->dat.n_int_lit.value, b);\
        \
        source_free(src);\
        parser_free(&parser);\
        errvec_free(&ev);\
    }

// binary op between two idents
#define BINARY_OP_IDENT_IDENT(op, opcode2) \
    TEST(parse, bin_expr_ident_ident_##opcode2) {\
        error_vector_t ev;\
        errvec_init(&ev);\
        parser_t parser;\
        parser_init(&parser, &ev);\
        std::uniform_int_distribution<neo_int_t> dist {0, 0xffff};\
        auto identA = srcspan_from("x");\
        auto identB = srcspan_from("y");\
        std::string source = std::string(reinterpret_cast<const char *>(identA.p)) + " " #op " " + std::string(reinterpret_cast<const char *>(identB.p));\
        std::cout << "Testing expression: " << source << "\n";\
        const source_t *src = source_from_memory_ref(u8"test"_neo, reinterpret_cast<const std::uint8_t *>(source.c_str()), nullptr);\
        \
        parser_setup_source(&parser, src);\
        \
        astref_t expr = rule_expr(&parser);\
        errvec_print(&ev, stderr, true);\
        ASSERT_FALSE(parser.error);\
        ASSERT_FALSE(astref_isnull(expr));\
        ASSERT_TRUE(astpool_isvalidref(&parser.pool, expr));\
        astnode_t *expr_node = astpool_resolve(&parser.pool, expr);\
        ASSERT_NE(expr_node, nullptr);\
        ASSERT_EQ(expr_node->type, ASTNODE_BINARY_OP);\
        ASSERT_EQ(expr_node->dat.n_binary_op.opcode, BINOP_##opcode2);\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.left_expr));\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.right_expr));\
        \
        astnode_t *left = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.left_expr);\
        ASSERT_NE(left, nullptr);\
        ASSERT_EQ(left->type, ASTNODE_IDENT_LIT);\
        ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, identA));\
        \
        astnode_t *right = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.right_expr);\
        ASSERT_NE(right, nullptr);\
        ASSERT_EQ(right->type, ASTNODE_IDENT_LIT);\
        ASSERT_TRUE(srcspan_eq(right->dat.n_ident_lit.span, identB));\
        \
        source_free(src);\
        parser_free(&parser);\
        errvec_free(&ev);\
    }

// binary op between an ident and a literal
#define BINARY_OP_IDENT_LITERAL(op, opcode2) \
    TEST(parse, bin_expr_ident_literal_##opcode2) {\
        error_vector_t ev;\
        errvec_init(&ev);\
        parser_t parser;\
        parser_init(&parser, &ev);\
        std::uniform_int_distribution<neo_int_t> dist {0, 0xffff};\
        auto identA = srcspan_from("x");\
        neo_int_t b = dist(prng);\
        std::string source = std::string(reinterpret_cast<const char *>(identA.p)) + " " #op " " + std::to_string(b);\
        std::cout << "Testing expression: " << source << "\n";\
        const source_t *src = source_from_memory_ref(u8"test"_neo, reinterpret_cast<const std::uint8_t *>(source.c_str()), nullptr);\
        \
        parser_setup_source(&parser, src);\
        \
        astref_t expr = rule_expr(&parser);\
        errvec_print(&ev, stderr, true);\
        ASSERT_FALSE(parser.error);\
        ASSERT_FALSE(astref_isnull(expr));\
        ASSERT_TRUE(astpool_isvalidref(&parser.pool, expr));\
        astnode_t *expr_node = astpool_resolve(&parser.pool, expr);\
        ASSERT_NE(expr_node, nullptr);\
        ASSERT_EQ(expr_node->type, ASTNODE_BINARY_OP);\
        ASSERT_EQ(expr_node->dat.n_binary_op.opcode, BINOP_##opcode2);\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.left_expr));\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.right_expr));\
        \
        astnode_t *left = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.left_expr);\
        ASSERT_NE(left, nullptr);\
        ASSERT_EQ(left->type, ASTNODE_IDENT_LIT);\
        ASSERT_TRUE(srcspan_eq(left->dat.n_ident_lit.span, identA));\
        \
        astnode_t *right = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.right_expr);\
        ASSERT_NE(right, nullptr);\
        ASSERT_EQ(right->type, ASTNODE_INT_LIT);\
        ASSERT_EQ(right->dat.n_int_lit.value, b);\
        \
        source_free(src);\
        parser_free(&parser);\
        errvec_free(&ev);\
    }

// binary op between a literal and an ident
#define BINARY_OP_LITERAL_IDENT(op, opcode2) \
    TEST(parse, bin_expr_literal_ident_##opcode2) {\
        error_vector_t ev;\
        errvec_init(&ev);\
        parser_t parser;\
        parser_init(&parser, &ev);\
        std::uniform_int_distribution<neo_int_t> dist {0, 0xffff};\
        neo_int_t a = dist(prng);\
        auto identB = srcspan_from("y");\
        std::string source = std::to_string(a) + " " #op " " + std::string(reinterpret_cast<const char *>(identB.p));\
        std::cout << "Testing expression: " << source << "\n";\
        const source_t *src = source_from_memory_ref(u8"test"_neo, reinterpret_cast<const std::uint8_t *>(source.c_str()), nullptr);\
        \
        parser_setup_source(&parser, src);\
        \
        astref_t expr = rule_expr(&parser);\
        errvec_print(&ev, stderr, true);\
        ASSERT_FALSE(parser.error);\
        ASSERT_FALSE(astref_isnull(expr));\
        ASSERT_TRUE(astpool_isvalidref(&parser.pool, expr));\
        astnode_t *expr_node = astpool_resolve(&parser.pool, expr);\
        ASSERT_NE(expr_node, nullptr);\
        ASSERT_EQ(expr_node->type, ASTNODE_BINARY_OP);\
        ASSERT_EQ(expr_node->dat.n_binary_op.opcode, BINOP_##opcode2);\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.left_expr));\
        ASSERT_FALSE(astref_isnull(expr_node->dat.n_binary_op.right_expr));\
        \
        astnode_t *left = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.left_expr);\
        ASSERT_NE(left, nullptr);\
        ASSERT_EQ(left->type, ASTNODE_INT_LIT);\
        ASSERT_EQ(left->dat.n_int_lit.value, a);\
        \
        astnode_t *right = astpool_resolve(&parser.pool, expr_node->dat.n_binary_op.right_expr);\
        ASSERT_NE(right, nullptr);\
        ASSERT_EQ(right->type, ASTNODE_IDENT_LIT);\
        ASSERT_TRUE(srcspan_eq(right->dat.n_ident_lit.span, identB));\
        \
        source_free(src);\
        parser_free(&parser);\
        errvec_free(&ev);\
    }


BINARY_OP_LITERAL_LITERAL(=, ASSIGN)
BINARY_OP_LITERAL_LITERAL(+, ADD)
BINARY_OP_LITERAL_LITERAL(-, SUB)
BINARY_OP_LITERAL_LITERAL(*, MUL)
BINARY_OP_LITERAL_LITERAL(**, POW)
BINARY_OP_LITERAL_LITERAL(!+, ADD_NO_OV)
BINARY_OP_LITERAL_LITERAL(!-, SUB_NO_OV)
BINARY_OP_LITERAL_LITERAL(!*, MUL_NO_OV)
BINARY_OP_LITERAL_LITERAL(!**, POW_NO_OV)
BINARY_OP_LITERAL_LITERAL(/, DIV)
BINARY_OP_LITERAL_LITERAL(%, MOD)
BINARY_OP_LITERAL_LITERAL(+=, ADD_ASSIGN)
BINARY_OP_LITERAL_LITERAL(-=, SUB_ASSIGN)
BINARY_OP_LITERAL_LITERAL(*=, MUL_ASSIGN)
BINARY_OP_LITERAL_LITERAL(**=, POW_ASSIGN)
BINARY_OP_LITERAL_LITERAL(!+=, ADD_ASSIGN_NO_OV)
BINARY_OP_LITERAL_LITERAL(!-=, SUB_ASSIGN_NO_OV)
BINARY_OP_LITERAL_LITERAL(!*=, MUL_ASSIGN_NO_OV)
BINARY_OP_LITERAL_LITERAL(!**=, POW_ASSIGN_NO_OV)
BINARY_OP_LITERAL_LITERAL(/=, DIV_ASSIGN)
BINARY_OP_LITERAL_LITERAL(%=, MOD_ASSIGN)
BINARY_OP_LITERAL_LITERAL(==, EQUAL)
BINARY_OP_LITERAL_LITERAL(!=, NOT_EQUAL)
BINARY_OP_LITERAL_LITERAL(<, LESS)
BINARY_OP_LITERAL_LITERAL(<=, LESS_EQUAL)
BINARY_OP_LITERAL_LITERAL(>, GREATER)
BINARY_OP_LITERAL_LITERAL(>=, GREATER_EQUAL)
BINARY_OP_LITERAL_LITERAL(&, BIT_AND)
BINARY_OP_LITERAL_LITERAL(|, BIT_OR)
BINARY_OP_LITERAL_LITERAL(^, BIT_XOR)
BINARY_OP_LITERAL_LITERAL(&=, BIT_AND_ASSIGN)
BINARY_OP_LITERAL_LITERAL(|=, BIT_OR_ASSIGN)
BINARY_OP_LITERAL_LITERAL(^=, BIT_XOR_ASSIGN)
BINARY_OP_LITERAL_LITERAL(<<, BIT_ASHL)
BINARY_OP_LITERAL_LITERAL(>>, BIT_ASHR)
BINARY_OP_LITERAL_LITERAL(<<<, BIT_ROL)
BINARY_OP_LITERAL_LITERAL(>>>, BIT_ROR)
BINARY_OP_LITERAL_LITERAL(>>>>, BIT_LSHR)
BINARY_OP_LITERAL_LITERAL(<<=, BIT_ASHL_ASSIGN)
BINARY_OP_LITERAL_LITERAL(>>=, BIT_ASHR_ASSIGN)
BINARY_OP_LITERAL_LITERAL(<<<=, BIT_ROL_ASSIGN)
BINARY_OP_LITERAL_LITERAL(>>>=, BIT_ROR_ASSIGN)
BINARY_OP_LITERAL_LITERAL(>>>>=, BIT_LSHR_ASSIGN)
BINARY_OP_LITERAL_LITERAL(and, LOG_AND)
BINARY_OP_LITERAL_LITERAL(or, LOG_OR)

BINARY_OP_IDENT_IDENT(=, ASSIGN)
BINARY_OP_IDENT_IDENT(+, ADD)
BINARY_OP_IDENT_IDENT(-, SUB)
BINARY_OP_IDENT_IDENT(*, MUL)
BINARY_OP_IDENT_IDENT(**, POW)
BINARY_OP_IDENT_IDENT(!+, ADD_NO_OV)
BINARY_OP_IDENT_IDENT(!-, SUB_NO_OV)
BINARY_OP_IDENT_IDENT(!*, MUL_NO_OV)
BINARY_OP_IDENT_IDENT(!**, POW_NO_OV)
BINARY_OP_IDENT_IDENT(/, DIV)
BINARY_OP_IDENT_IDENT(%, MOD)
BINARY_OP_IDENT_IDENT(+=, ADD_ASSIGN)
BINARY_OP_IDENT_IDENT(-=, SUB_ASSIGN)
BINARY_OP_IDENT_IDENT(*=, MUL_ASSIGN)
BINARY_OP_IDENT_IDENT(**=, POW_ASSIGN)
BINARY_OP_IDENT_IDENT(!+=, ADD_ASSIGN_NO_OV)
BINARY_OP_IDENT_IDENT(!-=, SUB_ASSIGN_NO_OV)
BINARY_OP_IDENT_IDENT(!*=, MUL_ASSIGN_NO_OV)
BINARY_OP_IDENT_IDENT(!**=, POW_ASSIGN_NO_OV)
BINARY_OP_IDENT_IDENT(/=, DIV_ASSIGN)
BINARY_OP_IDENT_IDENT(%=, MOD_ASSIGN)
BINARY_OP_IDENT_IDENT(==, EQUAL)
BINARY_OP_IDENT_IDENT(!=, NOT_EQUAL)
BINARY_OP_IDENT_IDENT(<, LESS)
BINARY_OP_IDENT_IDENT(<=, LESS_EQUAL)
BINARY_OP_IDENT_IDENT(>, GREATER)
BINARY_OP_IDENT_IDENT(>=, GREATER_EQUAL)
BINARY_OP_IDENT_IDENT(&, BIT_AND)
BINARY_OP_IDENT_IDENT(|, BIT_OR)
BINARY_OP_IDENT_IDENT(^, BIT_XOR)
BINARY_OP_IDENT_IDENT(&=, BIT_AND_ASSIGN)
BINARY_OP_IDENT_IDENT(|=, BIT_OR_ASSIGN)
BINARY_OP_IDENT_IDENT(^=, BIT_XOR_ASSIGN)
BINARY_OP_IDENT_IDENT(<<, BIT_ASHL)
BINARY_OP_IDENT_IDENT(>>, BIT_ASHR)
BINARY_OP_IDENT_IDENT(<<<, BIT_ROL)
BINARY_OP_IDENT_IDENT(>>>, BIT_ROR)
BINARY_OP_IDENT_IDENT(>>>>, BIT_LSHR)
BINARY_OP_IDENT_IDENT(<<=, BIT_ASHL_ASSIGN)
BINARY_OP_IDENT_IDENT(>>=, BIT_ASHR_ASSIGN)
BINARY_OP_IDENT_IDENT(<<<=, BIT_ROL_ASSIGN)
BINARY_OP_IDENT_IDENT(>>>=, BIT_ROR_ASSIGN)
BINARY_OP_IDENT_IDENT(>>>>=, BIT_LSHR_ASSIGN)
BINARY_OP_IDENT_IDENT(and, LOG_AND)
BINARY_OP_IDENT_IDENT(or, LOG_OR)

BINARY_OP_IDENT_LITERAL(=, ASSIGN)
BINARY_OP_IDENT_LITERAL(+, ADD)
BINARY_OP_IDENT_LITERAL(-, SUB)
BINARY_OP_IDENT_LITERAL(*, MUL)
BINARY_OP_IDENT_LITERAL(**, POW)
BINARY_OP_IDENT_LITERAL(!+, ADD_NO_OV)
BINARY_OP_IDENT_LITERAL(!-, SUB_NO_OV)
BINARY_OP_IDENT_LITERAL(!*, MUL_NO_OV)
BINARY_OP_IDENT_LITERAL(!**, POW_NO_OV)
BINARY_OP_IDENT_LITERAL(/, DIV)
BINARY_OP_IDENT_LITERAL(%, MOD)
BINARY_OP_IDENT_LITERAL(+=, ADD_ASSIGN)
BINARY_OP_IDENT_LITERAL(-=, SUB_ASSIGN)
BINARY_OP_IDENT_LITERAL(*=, MUL_ASSIGN)
BINARY_OP_IDENT_LITERAL(**=, POW_ASSIGN)
BINARY_OP_IDENT_LITERAL(!+=, ADD_ASSIGN_NO_OV)
BINARY_OP_IDENT_LITERAL(!-=, SUB_ASSIGN_NO_OV)
BINARY_OP_IDENT_LITERAL(!*=, MUL_ASSIGN_NO_OV)
BINARY_OP_IDENT_LITERAL(!**=, POW_ASSIGN_NO_OV)
BINARY_OP_IDENT_LITERAL(/=, DIV_ASSIGN)
BINARY_OP_IDENT_LITERAL(%=, MOD_ASSIGN)
BINARY_OP_IDENT_LITERAL(==, EQUAL)
BINARY_OP_IDENT_LITERAL(!=, NOT_EQUAL)
BINARY_OP_IDENT_LITERAL(<, LESS)
BINARY_OP_IDENT_LITERAL(<=, LESS_EQUAL)
BINARY_OP_IDENT_LITERAL(>, GREATER)
BINARY_OP_IDENT_LITERAL(>=, GREATER_EQUAL)
BINARY_OP_IDENT_LITERAL(&, BIT_AND)
BINARY_OP_IDENT_LITERAL(|, BIT_OR)
BINARY_OP_IDENT_LITERAL(^, BIT_XOR)
BINARY_OP_IDENT_LITERAL(&=, BIT_AND_ASSIGN)
BINARY_OP_IDENT_LITERAL(|=, BIT_OR_ASSIGN)
BINARY_OP_IDENT_LITERAL(^=, BIT_XOR_ASSIGN)
BINARY_OP_IDENT_LITERAL(<<, BIT_ASHL)
BINARY_OP_IDENT_LITERAL(>>, BIT_ASHR)
BINARY_OP_IDENT_LITERAL(<<<, BIT_ROL)
BINARY_OP_IDENT_LITERAL(>>>, BIT_ROR)
BINARY_OP_IDENT_LITERAL(>>>>, BIT_LSHR)
BINARY_OP_IDENT_LITERAL(<<=, BIT_ASHL_ASSIGN)
BINARY_OP_IDENT_LITERAL(>>=, BIT_ASHR_ASSIGN)
BINARY_OP_IDENT_LITERAL(<<<=, BIT_ROL_ASSIGN)
BINARY_OP_IDENT_LITERAL(>>>=, BIT_ROR_ASSIGN)
BINARY_OP_IDENT_LITERAL(>>>>=, BIT_LSHR_ASSIGN)
BINARY_OP_IDENT_LITERAL(and, LOG_AND)
BINARY_OP_IDENT_LITERAL(or, LOG_OR)

BINARY_OP_LITERAL_IDENT(=, ASSIGN)
BINARY_OP_LITERAL_IDENT(+, ADD)
BINARY_OP_LITERAL_IDENT(-, SUB)
BINARY_OP_LITERAL_IDENT(*, MUL)
BINARY_OP_LITERAL_IDENT(**, POW)
BINARY_OP_LITERAL_IDENT(!+, ADD_NO_OV)
BINARY_OP_LITERAL_IDENT(!-, SUB_NO_OV)
BINARY_OP_LITERAL_IDENT(!*, MUL_NO_OV)
BINARY_OP_LITERAL_IDENT(!**, POW_NO_OV)
BINARY_OP_LITERAL_IDENT(/, DIV)
BINARY_OP_LITERAL_IDENT(%, MOD)
BINARY_OP_LITERAL_IDENT(+=, ADD_ASSIGN)
BINARY_OP_LITERAL_IDENT(-=, SUB_ASSIGN)
BINARY_OP_LITERAL_IDENT(*=, MUL_ASSIGN)
BINARY_OP_LITERAL_IDENT(**=, POW_ASSIGN)
BINARY_OP_LITERAL_IDENT(!+=, ADD_ASSIGN_NO_OV)
BINARY_OP_LITERAL_IDENT(!-=, SUB_ASSIGN_NO_OV)
BINARY_OP_LITERAL_IDENT(!*=, MUL_ASSIGN_NO_OV)
BINARY_OP_LITERAL_IDENT(!**=, POW_ASSIGN_NO_OV)
BINARY_OP_LITERAL_IDENT(/=, DIV_ASSIGN)
BINARY_OP_LITERAL_IDENT(%=, MOD_ASSIGN)
BINARY_OP_LITERAL_IDENT(==, EQUAL)
BINARY_OP_LITERAL_IDENT(!=, NOT_EQUAL)
BINARY_OP_LITERAL_IDENT(<, LESS)
BINARY_OP_LITERAL_IDENT(<=, LESS_EQUAL)
BINARY_OP_LITERAL_IDENT(>, GREATER)
BINARY_OP_LITERAL_IDENT(>=, GREATER_EQUAL)
BINARY_OP_LITERAL_IDENT(&, BIT_AND)
BINARY_OP_LITERAL_IDENT(|, BIT_OR)
BINARY_OP_LITERAL_IDENT(^, BIT_XOR)
BINARY_OP_LITERAL_IDENT(&=, BIT_AND_ASSIGN)
BINARY_OP_LITERAL_IDENT(|=, BIT_OR_ASSIGN)
BINARY_OP_LITERAL_IDENT(^=, BIT_XOR_ASSIGN)
BINARY_OP_LITERAL_IDENT(<<, BIT_ASHL)
BINARY_OP_LITERAL_IDENT(>>, BIT_ASHR)
BINARY_OP_LITERAL_IDENT(<<<, BIT_ROL)
BINARY_OP_LITERAL_IDENT(>>>, BIT_ROR)
BINARY_OP_LITERAL_IDENT(>>>>, BIT_LSHR)
BINARY_OP_LITERAL_IDENT(<<=, BIT_ASHL_ASSIGN)
BINARY_OP_LITERAL_IDENT(>>=, BIT_ASHR_ASSIGN)
BINARY_OP_LITERAL_IDENT(<<<=, BIT_ROL_ASSIGN)
BINARY_OP_LITERAL_IDENT(>>>=, BIT_ROR_ASSIGN)
BINARY_OP_LITERAL_IDENT(>>>>=, BIT_LSHR_ASSIGN)
BINARY_OP_LITERAL_IDENT(and, LOG_AND)
BINARY_OP_LITERAL_IDENT(or, LOG_OR)