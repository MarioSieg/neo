/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <cstring>

#include "neo_lexer.c"

TEST(lexer, utf8_seqlen_1) {
    static constexpr  std::uint8_t str[] = {'h'};
    ASSERT_EQ(utf8_seqlen(*str), 1);
}

TEST(lexer, utf8_seqlen_2) {
    static constexpr  std::uint8_t str[] = {0xc3, 0xa4}; // So MSVC just shits itself when encoding some Emojis or Unicode characters, so I'll encode them by myself. Thanks MSVC :)
    ASSERT_EQ(utf8_seqlen(*str), 2);
}

TEST(lexer, utf8_seqlen_3) {
    static constexpr  std::uint8_t str[] = {0xe2, 0x82, 0xac};
    ASSERT_EQ(utf8_seqlen(*str), 3);
}

TEST(lexer, utf8_seqlen_4) {
    static constexpr  std::uint8_t str[] = {0xf0,0x9f,0x98,0x80};
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

TEST(lexer, neo_utf8_validate) {
    const std::uint8_t* input{reinterpret_cast<const std::uint8_t*>("\xE2\x82\xAC")}; // UTF-8 encoding of the Euro sign '€'
    std::size_t pos{};
    ASSERT_EQ(neo_utf8_validate(input, 3, &pos), NEO_UNIERR_OK);
}

TEST(lexer, null_terminated_string) {
    const std::uint8_t* input{reinterpret_cast<const std::uint8_t*>("Hello, world!")}; // No valid UTF-8 sequences
    const auto* tmp{input};
    std::uint32_t result{utf8_decode(&tmp)};
    ASSERT_EQ(result, static_cast<std::uint32_t>('H')); // The first character in ASCII
    ASSERT_EQ(tmp, input + 1);
}

TEST(lexer, src_load) {
    source_t src = {0};
    static constexpr std::uint8_t path[] = {
        0x74,0x65,0x73,0x74,0x2f,0x66,0x69,0x6c,0x65,0x73,0x2f,0x68,0x61,0x6c,
        0x6c,0xc3,0xb6,0x63,0x68,0x65,0x6e,0x2e,0x6e,0x65,0x6f, '\0'
    };
    ASSERT_TRUE(source_load(&src, path));
    std::uint8_t expected[] = {
        0xc3,0x84,0x70,0x66,0xe2,0x82,0xac,0x6c,0x20,0x73,0x69,0x6e,0x64,
        0x20,0x6c,0x65,0x63,0x6b,0x65,0x72,0x21, '\n', '&', '\0'
    };;
    ASSERT_EQ(src.len, (sizeof(expected)/sizeof(*expected))-1);
    ASSERT_EQ(std::memcmp(src.src, expected, src.len), 0);
}
