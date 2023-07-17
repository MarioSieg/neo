/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>

#include "neo_lexer.c"

TEST(lexer, utf8_seqlen_1) {
    const char8_t *str {u8"h"};
    ASSERT_EQ(utf8_seqlen(*str), 1);
}

TEST(lexer, utf8_seqlen_2) {
    const char8_t *str {u8"ä"};
    ASSERT_EQ(utf8_seqlen(*str), 2);
}

TEST(lexer, utf8_seqlen_3) {
    const char8_t *str {u8"€"};
    ASSERT_EQ(utf8_seqlen(*str), 3);
}

TEST(lexer, utf8_seqlen_4) {
    const char8_t *str {u8"\xf0\x9f\x98\x80"};
    ASSERT_EQ(utf8_seqlen(*str), 4);
}

TEST(lexer, single_byte_utf8) {
    const std::uint8_t* input{reinterpret_cast<const std::uint8_t*>("A")};
    const auto* tmp{input};
    std::uint32_t result{utf8_decode(&tmp)};
    ASSERT_EQ(result, static_cast<std::uint32_t>('A'));
    ASSERT_EQ(tmp, input + 1);
}

TEST(lexer, multi_byte_utf8) {
    const std::uint8_t* input{reinterpret_cast<const std::uint8_t*>("\xE2\x82\xAC")}; // UTF-8 encoding of the Euro sign '€'
    const auto* tmp{input};
    std::uint32_t result{utf8_decode(&tmp)};
    ASSERT_EQ(result, 0x20AC); // UTF-32 representation of '€'
    ASSERT_EQ(tmp, input + 3);
}

TEST(lexer, null_terminated_string) {
    const std::uint8_t* input{reinterpret_cast<const std::uint8_t*>("Hello, world!")}; // No valid UTF-8 sequences
    const auto* tmp{input};
    std::uint32_t result{utf8_decode(&tmp)};
    ASSERT_EQ(result, static_cast<std::uint32_t>('H')); // The first character in ASCII
    ASSERT_EQ(tmp, input + 1);
}
