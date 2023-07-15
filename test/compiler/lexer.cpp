// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_lexer.hpp>

using namespace neoc;

TEST(lexer, cursor) {
	auto src{std::make_shared<source_code>(u8"Abü23€!", "?.neo")};
    cursor cur {};
    cur.set_source(src);
    const auto* n{cur.get_needle()};
    ASSERT_EQ(cur.get_needle(), src->get_source_code().c_str());

    ASSERT_EQ(cur.get_needle(), n + 0);
    ASSERT_EQ(*cur.peek(), U'A');
    ASSERT_EQ(*cur.peek_next(), U'b');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 1);
    ASSERT_EQ(*cur.peek(), U'b');
    ASSERT_EQ(*cur.peek_next(), U'ü');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 2);
    ASSERT_EQ(*cur.peek(), U'ü');
    ASSERT_EQ(*cur.peek_next(), U'2');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 4);
    ASSERT_EQ(*cur.peek(), U'2');
    ASSERT_EQ(*cur.peek_next(), U'3');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 5);
    ASSERT_EQ(*cur.peek(), U'3');
    ASSERT_EQ(*cur.peek_next(), U'€');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 6);
    ASSERT_EQ(*cur.peek(), U'€');
    ASSERT_EQ(*cur.peek_next(), U'!');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 9);
    ASSERT_EQ(*cur.peek(), U'!');
    ASSERT_EQ(*cur.peek_next(), U'\0');
    cur.consume();
    ASSERT_EQ(cur.get_needle(), n + 10);
    ASSERT_EQ(*cur.peek(), U'\0');
    ASSERT_EQ(*cur.peek_next(), U'\0');

    ASSERT_EQ(*cur.peek(), 0);
    ASSERT_TRUE(cur.is_done());
}

