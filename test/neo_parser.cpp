/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neo_parser.c>

inline const std::uint8_t *operator""_neo(const char8_t *s, size_t len) {
    return reinterpret_cast<const std::uint8_t *>(s);
}

TEST(parse, advance) {
    error_vector_t ev;
    errvec_init(&ev);
    parser_t parser;
    parser_init(&parser, &ev);
    const source_t *src = source_from_memory(u8"test"_neo, u8"3 1.4 hello"_neo);

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
    const source_t *src = source_from_memory(u8"test"_neo, u8"3 1.4 hello"_neo);

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
    const source_t *src = source_from_memory(u8"test"_neo, u8"3 1.4 hello"_neo);

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
    const source_t *src = source_from_memory(u8"test"_neo, u8"3 1.4 hello"_neo);

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

    source_free(src);
    parser_free(&parser);
    errvec_free(&ev);
}

static inline bool parse_int2(srcspan_t str, neo_int_t *x) {
    return parse_int((const char *) str.p, str.len, RADIX_UNKNOWN, x);
}

TEST(parse, int_invalid) {
    neo_int_t x;
    ASSERT_FALSE(parse_int2(srcspan_from(""), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("_+"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("_-"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+-"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-+"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0x"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+0x"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0x"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+0c"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0c"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("_11"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("_11_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("11_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0b"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0c"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+0b"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0b_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0c_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0b_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0x_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0xfF_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0b11_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0c11_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0x_fF_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0b_11_"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0c_11_"), &x));
    ASSERT_EQ(0, x);
}

TEST(parse, int_overflow) {
    neo_int_t x;
    ASSERT_FALSE(parse_int2(srcspan_from("9223__37203685_4775808"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_FALSE(parse_int2(srcspan_from("+9223__37203685_4775808"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0x7fff_ffffffff__ffff0"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_FALSE(
            parse_int2(srcspan_from("0b1111111111_1111111111111111_111111111111111_____111111111111__11111111111"),
                       &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_FALSE(
            parse_int2(srcspan_from("0b11111111111111111111__111111111111111111111111__11111111111111111111111"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_FALSE(
            parse_int2(srcspan_from("-0b100000__0000000000000000000000000000000000000000000000000000000011111101"),
                       &x));
    ASSERT_EQ(NEO_INT_MAX, x);
}

TEST(parse, int_underflow) {
    neo_int_t x;
    ASSERT_FALSE(parse_int2(srcspan_from("-922337203_6854775810"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0x800000000000000_0f"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0b10000000000000000000000000000000__00000000000000000000000000000001"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
}

TEST(parse, int_dec) {
    neo_int_t x;
    ASSERT_TRUE(parse_int2(srcspan_from("0"), &x));
    ASSERT_EQ(0, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0"), &x));
    ASSERT_EQ(0, x);
    ASSERT_TRUE(parse_int2(srcspan_from("1"), &x));
    ASSERT_EQ(1, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-1"), &x));
    ASSERT_EQ(-1, x);
    ASSERT_TRUE(parse_int2(srcspan_from("1000_000_000"), &x));
    ASSERT_EQ(1000000000, x);
    ASSERT_TRUE(parse_int2(srcspan_from("123"), &x));
    ASSERT_EQ(123, x);
    ASSERT_TRUE(parse_int2(srcspan_from("123456789"), &x));
    ASSERT_EQ(123456789, x);
    ASSERT_TRUE(parse_int2(srcspan_from("+123"), &x));
    ASSERT_EQ(123, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-81_92"), &x));
    ASSERT_EQ(-8192, x);
    ASSERT_TRUE(parse_int2(srcspan_from("9223372036854775807"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-92233720__36854775808"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
}

TEST(parse, int_oct) {
    neo_int_t x;
    ASSERT_TRUE(parse_int2(srcspan_from("0c0"), &x));
    ASSERT_EQ(0, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0c0"), &x));
    ASSERT_EQ(0, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c1"), &x));
    ASSERT_EQ(1, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c10"), &x));
    ASSERT_EQ(010, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0c1"), &x));
    ASSERT_EQ(-1, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c73465_45000"), &x));
    ASSERT_EQ(1000000000, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c173"), &x));
    ASSERT_EQ(123, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c726746425"), &x));
    ASSERT_EQ(123456789, x);
    ASSERT_TRUE(parse_int2(srcspan_from("+0c173"), &x));
    ASSERT_EQ(123, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0c20_0_00"), &x));
    ASSERT_EQ(-8192, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0c777777777777777777777"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0c1000000000000000000000"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
    ASSERT_FALSE(parse_int2(srcspan_from("0c8"), &x));
    ASSERT_EQ(0, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0c9"), &x));
    ASSERT_EQ(0, x);
}

TEST(parse, int_hex) {
    neo_int_t x;
    ASSERT_TRUE(parse_int2(srcspan_from("0xff"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0xFF"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0x0123456789"), &x));
    ASSERT_EQ(0x0123456789, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0xabcdef"), &x));
    ASSERT_EQ(0xabcdef, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0xABCDEF"), &x));
    ASSERT_EQ(0xabcdef, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0xabcdef"), &x));
    ASSERT_EQ(0xabcdef, x);
    ASSERT_TRUE(parse_int2(srcspan_from("+0xff"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0x7f"), &x));
    ASSERT_EQ(-0x7f, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0x7ffff_fffffffffff"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0x8000000_000000000"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
}

TEST(parse, int_bin) {
    neo_int_t x;
    ASSERT_TRUE(parse_int2(srcspan_from("0b11111__111"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0B11111__111"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("+0b11111__111"), &x));
    ASSERT_EQ(0xff, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0b010___11101"), &x));
    ASSERT_EQ(-0x5d, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0B010___11101"), &x));
    ASSERT_EQ(-0x5d, x);
    ASSERT_TRUE(parse_int2(srcspan_from("0b0111111111111111111111111111111111111111111111111111111111111111"), &x));
    ASSERT_EQ(NEO_INT_MAX, x);
    ASSERT_TRUE(parse_int2(srcspan_from("-0b1000000000000000000000000000000000000000000000000000000000000000"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
}