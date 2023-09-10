// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <cstring>

#include "neo_lexer.h"
#include "neo_compiler.h"

TEST(utils, comerror_from_token) {
    const source_t *source = source_from_memory_ref(reinterpret_cast<const std::uint8_t *>(u8"test.neo"),
                                                    reinterpret_cast<const std::uint8_t *>("01234567890_100111"), nullptr);

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, source);
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("01234567890_100111")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "01234567890_100111", sizeof("01234567890_100111")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_DEC);

    const compile_error_t *error = comerror_from_token(COMERR_INTERNAL_COMPILER_ERROR, &tok,
                                                       reinterpret_cast<const uint8_t *>("Oh no!"));
    ASSERT_NE(error, nullptr);
    ASSERT_EQ(error->line, 1);
    ASSERT_EQ(error->col, 1);
    ASSERT_STREQ((const char *)error->lexeme, "01234567890_100111");
    ASSERT_STREQ((const char *)error->lexeme_line, "01234567890_100111");
    ASSERT_STREQ((const char *)error->file, "test.neo");
    ASSERT_STREQ((const char *)error->msg, "Oh no!");
    comerror_free(error);

    lexer_free(&lexer);
    source_free(source);
}

TEST(utils, errvec_push) {
    const source_t *source = source_from_memory_ref(reinterpret_cast<const std::uint8_t *>(u8"test.neo"),
                                                    reinterpret_cast<const std::uint8_t *>("01234567890_100111"), nullptr);

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, source);
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("01234567890_100111")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "01234567890_100111", sizeof("01234567890_100111")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_DEC);

    error_vector_t ev;
    errvec_init(&ev);
    ASSERT_FALSE(errvec_isempty(ev));
    ASSERT_EQ(ev.len, 0);
    ASSERT_EQ(ev.cap, 0);
    ASSERT_EQ(ev.p, nullptr);

    errvec_push(&ev, comerror_from_token(COMERR_INTERNAL_COMPILER_ERROR, &tok,
                                         reinterpret_cast<const uint8_t *>("Oh no!")));
    ASSERT_TRUE(errvec_isempty(ev));
    ASSERT_EQ(ev.len, 1);
    ASSERT_NE(ev.cap, 0);
    ASSERT_NE(ev.p, nullptr);

    errvec_push(&ev, comerror_new(COMERR_INTERNAL_COMPILER_ERROR, 0, 0, NULL, NULL, NULL,
                                  reinterpret_cast<const uint8_t *>("Helpy")));
    ASSERT_TRUE(errvec_isempty(ev));
    ASSERT_EQ(ev.len, 2);

    ASSERT_STREQ((const char *)ev.p[0]->msg, "Oh no!");
    ASSERT_STREQ((const char *)ev.p[1]->msg, "Helpy");

    errvec_free(&ev);

    lexer_free(&lexer);
    source_free(source);
}
