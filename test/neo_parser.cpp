/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neo_parser.h>

static inline bool parse_int2(srcspan_t str, neo_int_t *x)
{
    return parse_int((const char *) str.p, str.len, x);
}

TEST(parse, int_invalid)
{
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

TEST(parse, int_overflow)
{
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

TEST(parse, int_underflow)
{
    neo_int_t x;
    ASSERT_FALSE(parse_int2(srcspan_from("-922337203_6854775810"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0x800000000000000_0f"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
    ASSERT_FALSE(parse_int2(srcspan_from("-0b10000000000000000000000000000000__00000000000000000000000000000001"), &x));
    ASSERT_EQ(NEO_INT_MIN, x);
}

TEST(parse, int_dec)
{
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

TEST(parse, int_oct)
{
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

TEST(parse, int_hex)
{
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

TEST(parse, int_bin)
{
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