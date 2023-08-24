// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include "neo_core.h"
#include <gtest/gtest.h>
#include <cstring>

#include "neo_lexer.c"

TEST(lexer, complex_statement) {
    const auto *src = reinterpret_cast<const std::uint8_t*>("let x=0x22&129>>>=x\nnew Class()\nlet #*lol*# y class == 23.3%x\n#hello");
    const auto *filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");
    const source_t *source = source_from_memory(filename, src);

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, source);
    token_t *tok, *t;
    size_t len = lexer_drain(&lexer, &tok);
    t = tok;
    ASSERT_EQ(len, 22);

    ASSERT_EQ(tok->type, TOK_KW_LET);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("let")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 1);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_LI_IDENT);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("x")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 5);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_OP_ASSIGN);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("=")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 6);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_LI_INT);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("22")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 7);
    ASSERT_EQ(tok->radix, RADIX_HEX);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_OP_BIT_AND);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("&")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 11);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_LI_INT);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("129")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 12);
    ASSERT_EQ(tok->radix, RADIX_DEC);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_OP_BIT_ROR_ASSIGN);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from(">>>=")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 15);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_LI_IDENT);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("x")));
    ASSERT_EQ(tok->line, 1);
    ASSERT_EQ(tok->col, 19);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let x=0x22&129>>>=x")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_PU_NEWLINE);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("\n")));
    ASSERT_EQ(tok->line, 2);
    ASSERT_EQ(tok->col, 0);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("new Class()")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_KW_NEW);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("new")));
    ASSERT_EQ(tok->line, 2);
    ASSERT_EQ(tok->col, 1);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("new Class()")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_LI_IDENT);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("Class")));
    ASSERT_EQ(tok->line, 2);
    ASSERT_EQ(tok->col, 5);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("new Class()")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_PU_L_PAREN);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("(")));
    ASSERT_EQ(tok->line, 2);
    ASSERT_EQ(tok->col, 10);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("new Class()")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_PU_R_PAREN);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from(")")));
    ASSERT_EQ(tok->line, 2);
    ASSERT_EQ(tok->col, 11);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("new Class()")));
    ++tok;

    ASSERT_EQ(tok->type, TOK_PU_NEWLINE);
    ASSERT_TRUE(srcspan_eq(tok->lexeme, srcspan_from("\n")));
    ASSERT_EQ(tok->line, 3);
    ASSERT_EQ(tok->col, 0);
    ASSERT_TRUE(srcspan_eq(tok->lexeme_line, srcspan_from("let #*lol*# y class == 23.3%x")));
    ++tok;

    ASSERT_TRUE(is_done(&lexer));
    neo_memalloc(t, 0);
    lexer_free(&lexer);
    source_free(source);
}

#define generic_lexer_test(name, symbol, tokt)\
TEST(lexer, tok_##name) {\
    const source_t *source = source_from_memory(reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp"), reinterpret_cast<const std::uint8_t*>(symbol));\
    \
    lexer_t lexer;\
    lexer_init(&lexer);\
    \
    lexer_setup_source(&lexer, source);\
    ASSERT_EQ(peek(&lexer), symbol[0]);\
    token_t tok = lexer_scan_next(&lexer);\
    ASSERT_EQ(tok.type, tokt);\
    ASSERT_EQ(tok.lexeme.len, sizeof(symbol)-1);\
    ASSERT_EQ(std::memcmp(tok.lexeme.p, symbol, sizeof(symbol)-1), 0);\
    \
    ASSERT_TRUE(is_done(&lexer));\
    \
    lexer_free(&lexer);\
    source_free(source);\
}

generic_lexer_test(method, "method", TOK_KW_METHOD)
generic_lexer_test(let, "let", TOK_KW_LET)
generic_lexer_test(new, "new", TOK_KW_NEW)
generic_lexer_test(end, "end", TOK_KW_END)
generic_lexer_test(then, "then", TOK_KW_THEN)
generic_lexer_test(if, "if", TOK_KW_IF)
generic_lexer_test(else, "else", TOK_KW_ELSE)
generic_lexer_test(return, "return", TOK_KW_RETURN)
generic_lexer_test(class, "class", TOK_KW_CLASS)
generic_lexer_test(module, "module", TOK_KW_MODULE)
generic_lexer_test(break, "break", TOK_KW_BREAK)
generic_lexer_test(continue, "continue", TOK_KW_CONTINUE)
generic_lexer_test(while, "while", TOK_KW_WHILE)
generic_lexer_test(static, "static", TOK_KW_STATIC)
generic_lexer_test(do, "do", TOK_KW_DO)

generic_lexer_test(lparen, "(", TOK_PU_L_PAREN)
generic_lexer_test(rparen, ")", TOK_PU_R_PAREN)
generic_lexer_test(lbracket, "[", TOK_PU_L_BRACKET)
generic_lexer_test(rbracket, "]", TOK_PU_R_BRACKET)
generic_lexer_test(lbrace, "{", TOK_PU_L_BRACE)
generic_lexer_test(rbrace, "}", TOK_PU_R_BRACE)
generic_lexer_test(comma, ",", TOK_PU_COMMA)
generic_lexer_test(colon, ":", TOK_PU_COLON)
generic_lexer_test(at, "@", TOK_PU_AT)
generic_lexer_test(arrow, "->", TOK_PU_ARROW)
generic_lexer_test(newline, "\n", TOK_PU_NEWLINE)

generic_lexer_test(dot, ".", TOK_OP_DOT)
generic_lexer_test(assign, "=", TOK_OP_ASSIGN)
generic_lexer_test(add, "+", TOK_OP_ADD)
generic_lexer_test(sub, "-", TOK_OP_SUB)
generic_lexer_test(mul, "*", TOK_OP_MUL)
generic_lexer_test(pow, "**", TOK_OP_POW)
generic_lexer_test(add_no_ov, "!+", TOK_OP_ADD_NO_OV)
generic_lexer_test(sub_no_ov, "!-", TOK_OP_SUB_NO_OV)
generic_lexer_test(mul_no_ov, "!*", TOK_OP_MUL_NO_OV)
generic_lexer_test(pow_no_ov, "!**", TOK_OP_POW_NO_OV)
generic_lexer_test(div, "/", TOK_OP_DIV)
generic_lexer_test(mod, "%", TOK_OP_MOD)
generic_lexer_test(add_assign, "+=", TOK_OP_ADD_ASSIGN)
generic_lexer_test(sub_assign, "-=", TOK_OP_SUB_ASSIGN)
generic_lexer_test(mul_assign, "*=", TOK_OP_MUL_ASSIGN)
generic_lexer_test(pow_assign, "**=", TOK_OP_POW_ASSIGN)
generic_lexer_test(add_no_ov_assign, "!+=", TOK_OP_ADD_ASSIGN_NO_OV)
generic_lexer_test(sub_no_ov_assign, "!-=", TOK_OP_SUB_ASSIGN_NO_OV)
generic_lexer_test(mul_no_ov_assign, "!*=", TOK_OP_MUL_ASSIGN_NO_OV)
generic_lexer_test(pow_no_ov_assign, "!**=", TOK_OP_POW_ASSIGN_NO_OV)
generic_lexer_test(div_assign, "/=", TOK_OP_DIV_ASSIGN)
generic_lexer_test(mod_assign, "%=", TOK_OP_MOD_ASSIGN)
generic_lexer_test(inc, "++", TOK_OP_INC)
generic_lexer_test(dec, "--", TOK_OP_DEC)
generic_lexer_test(equal, "==", TOK_OP_EQUAL)
generic_lexer_test(not_equal, "!=", TOK_OP_NOT_EQUAL)
generic_lexer_test(less, "<", TOK_OP_LESS)
generic_lexer_test(less_equal, "<=", TOK_OP_LESS_EQUAL)
generic_lexer_test(greater, ">", TOK_OP_GREATER)
generic_lexer_test(greater_equal, ">=", TOK_OP_GREATER_EQUAL)
generic_lexer_test(bit_and, "&", TOK_OP_BIT_AND)
generic_lexer_test(bit_or, "|", TOK_OP_BIT_OR)
generic_lexer_test(bit_xor, "^", TOK_OP_BIT_XOR)
generic_lexer_test(bit_and_assign, "&=", TOK_OP_BIT_AND_ASSIGN)
generic_lexer_test(bit_or_assign, "|=", TOK_OP_BIT_OR_ASSIGN)
generic_lexer_test(bit_xor_assign, "^=", TOK_OP_BIT_XOR_ASSIGN)
generic_lexer_test(bit_ashl, "<<", TOK_OP_BIT_ASHL)
generic_lexer_test(bit_ashr, ">>", TOK_OP_BIT_ASHR)
generic_lexer_test(bit_rol, "<<<", TOK_OP_BIT_ROL)
generic_lexer_test(bit_ror, ">>>", TOK_OP_BIT_ROR)
generic_lexer_test(bit_lshr, ">>>>", TOK_OP_BIT_LSHR)
generic_lexer_test(bit_ashl_assign, "<<=", TOK_OP_BIT_ASHL_ASSIGN)
generic_lexer_test(bit_ashr_assign, ">>=", TOK_OP_BIT_ASHR_ASSIGN)
generic_lexer_test(bit_rol_assign, "<<<=", TOK_OP_BIT_ROL_ASSIGN)
generic_lexer_test(bit_ror_assign, ">>>=", TOK_OP_BIT_ROR_ASSIGN)
generic_lexer_test(bit_lshr_assign, ">>>>=", TOK_OP_BIT_LSHR_ASSIGN)
generic_lexer_test(bit_complo, "~", TOK_OP_BIT_COMPL)
generic_lexer_test(log_and, "and", TOK_OP_LOG_AND)
generic_lexer_test(log_or, "or", TOK_OP_LOG_OR)
generic_lexer_test(log_not, "not", TOK_OP_LOG_NOT)
generic_lexer_test(me_eof, "", TOK_ME_EOF)

#if 0 /* TODO: Check for lexer identifier unicode rule. */
TEST(lexer, identifier_literal) {
    static constexpr std::uint8_t bytes[] = {
        0x4c,0xc3,0xa4,0x63,0x6b,0x65,0x72,0x5f,0x4b,0xc3,0xa4,0xe2,0x82,0xac,0x65,0xf0,0x9f,0xa7,0x80, '\0'
    };

    source_t src {};
    src.src = bytes;
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_set_src(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'L');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_IDENT);
    ASSERT_EQ(tok.lexeme.len, sizeof(bytes)/sizeof(*bytes)-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.top, bytes, sizeof(bytes)/sizeof(*bytes)-1), 0);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}
#endif

TEST(lexer, float_literal) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("30.123456789");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'3');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_FLOAT);
    ASSERT_EQ(tok.lexeme.len, sizeof("30.123456789")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "30.123456789", sizeof("30.123456789")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_DEC);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, int_literal_dec) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("01234567890_100111");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'0');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("01234567890_100111")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "01234567890_100111", sizeof("01234567890_100111")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_DEC);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, int_literal_hex) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("0x123_45678_90abcdefA_BCDEF");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'0');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("123_45678_90abcdefA_BCDEF")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "123_45678_90abcdefA_BCDEF", sizeof("123_45678_90abcdefA_BCDEF")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_HEX);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, int_literal_bin) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("0b111_1010");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'0');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("111_1010")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "111_1010", sizeof("111_1010")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_BIN);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, int_literal_octal) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("0o01234567");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    ASSERT_EQ(peek(&lexer), U'0');
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    ASSERT_EQ(tok.lexeme.len, sizeof("01234567")-1);
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "01234567", sizeof("01234567")-1), 0);
    ASSERT_EQ(tok.radix, RADIX_OCT);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, string_literal) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("\"I'm in Vienna on vacations and damn this city is beautiful!\"");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_STRING);
    ASSERT_EQ(tok.lexeme.len, sizeof("I'm in Vienna on vacations and damn this city is beautiful!")-1); /* Quotes must be removed. */
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "I'm in Vienna on vacations and damn this city is beautiful!", sizeof("I'm in Vienna on vacations and damn this city is beautiful!")-1), 0);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, string_literal_sandwitch) {
    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>("3\"hey!\" 1.5");
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);
    token_t tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_INT);
    tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_STRING);
    ASSERT_EQ(tok.lexeme.len, sizeof("hey!")-1); /* Quotes must be removed. */
    ASSERT_EQ(std::memcmp(tok.lexeme.p, "hey!", sizeof("hey!")-1), 0);
    tok = lexer_scan_next(&lexer);
    ASSERT_EQ(tok.type, TOK_LI_FLOAT);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, consume_whitespace) {
    static constexpr const char8_t* srcstr {u8"A \t\r\vB#   ad\tssF         \nC#*noelle\nssv\t       *#D"};

    source_t src {};
    src.src = reinterpret_cast<const std::uint8_t*>(srcstr);
    src.len = std::strlen(reinterpret_cast<const char*>(src.src));
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_t lexer;
    lexer_init(&lexer);

    lexer_setup_source(&lexer, &src);

    ASSERT_EQ(peek(&lexer), U'A');
    consume(&lexer);

    consume_whitespace(&lexer);
    ASSERT_EQ(peek(&lexer), U'B');
    consume(&lexer);

    consume_whitespace(&lexer);
    ASSERT_EQ(peek(&lexer), U'\n');
    ASSERT_EQ(peek_next(&lexer), U'C');
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'C');
    consume(&lexer);

    consume_whitespace(&lexer);
    ASSERT_EQ(peek(&lexer), U'D');
    consume(&lexer);

    ASSERT_TRUE(is_done(&lexer));

    lexer_free(&lexer);
}

TEST(lexer, consume) {
    lexer_t lexer;
    lexer_init(&lexer);

    static constexpr std::uint8_t srctext[] = {
        0x48,0xc3,0xa4,0x6c,0x6c,0xe2,0x82,0xac,0x2c,0x20,0x57,0xc3,0xb6,0x72,0x6c,0xf0,0x9f,0x98,0x80, '\0'
    };

    source_t src {};
    src.src = srctext;
    src.len = sizeof(srctext)/sizeof(*srctext)-1;
    src.filename = reinterpret_cast<const std::uint8_t*>(u8"test/neo_lexer.cpp");

    lexer_setup_source(&lexer, &src);
    const uint8_t* n{lexer.needle};

    ASSERT_EQ(peek(&lexer), U'H');
    ASSERT_EQ(peek_next(&lexer), U'ä');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 0, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'ä');
    ASSERT_EQ(peek_next(&lexer), U'l');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 1, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'l');
    ASSERT_EQ(peek_next(&lexer), U'l');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 3, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'l');
    ASSERT_EQ(peek_next(&lexer), U'€');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 4, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'€');
    ASSERT_EQ(peek_next(&lexer), U',');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 5, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U',');
    ASSERT_EQ(peek_next(&lexer), U' ');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 8, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U' ');
    ASSERT_EQ(peek_next(&lexer), U'W');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 9, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'W');
    ASSERT_EQ(peek_next(&lexer), U'ö');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 10, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'ö');
    ASSERT_EQ(peek_next(&lexer), U'r');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 11, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'r');
    ASSERT_EQ(peek_next(&lexer), U'l');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 13, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'l');
    ASSERT_EQ(peek_next(&lexer), U'😀');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 14, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'😀');
    ASSERT_EQ(peek_next(&lexer), U'\0');
    ASSERT_FALSE(is_done(&lexer));
    ASSERT_EQ(n + 15, lexer.needle);
    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'\0');
    ASSERT_EQ(peek_next(&lexer), U'\0');
    ASSERT_TRUE(is_done(&lexer));
    ASSERT_EQ(n + 19, lexer.needle);

    consume(&lexer);
    ASSERT_EQ(peek(&lexer), U'\0');
    ASSERT_EQ(peek_next(&lexer), U'\0');
    ASSERT_TRUE(is_done(&lexer));
    ASSERT_EQ(n + 19, lexer.needle);

    lexer_free(&lexer);
}

TEST(lexer, utf8_seqlen_1) {
    static constexpr std::uint8_t str[] = {'h', '\0'};
    ASSERT_EQ(utf8_seqlen(*str), 1);
}

TEST(lexer, utf8_seqlen_2) {
    static constexpr std::uint8_t str[] = {0xc3, 0xa4, '\0'}; // So MSVC just shits itself when encoding some Emojis or Unicode characters, so I'll encode them by myself. Thanks MSVC :)
    ASSERT_EQ(utf8_seqlen(*str), 2);
}

TEST(lexer, utf8_seqlen_3) {
    static constexpr std::uint8_t str[] = {0xe2, 0x82, 0xac, '\0'};
    ASSERT_EQ(utf8_seqlen(*str), 3);
}

TEST(lexer, utf8_seqlen_4) {
    static constexpr std::uint8_t str[] = {0xf0,0x9f,0x98,0x80, '\0'};
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
    static constexpr std::uint8_t path[] = {
        0x74,0x65,0x73,0x74,0x2f,0x66,0x69,0x6c,0x65,0x73,0x2f,0x68,0x61,0x6c,
        0x6c,0xc3,0xb6,0x63,0x68,0x65,0x6e,0x2e,0x6e,0x65,0x6f, '\0'
    };
    const source_t *src = source_from_file(path, NULL);
    ASSERT_NE(src, nullptr);
    std::uint8_t expected[] = {
        0xc3,0x84,0x70,0x66,0xe2,0x82,0xac,0x6c,0x20,0x73,0x69,0x6e,0x64,
        0x20,0x6c,0x65,0x63,0x6b,0x65,0x72,0x21, '\n', '&', '\n', '\0'
    };
    ASSERT_EQ(src->len, (sizeof(expected)/sizeof(*expected))-1);
    ASSERT_EQ(std::memcmp(src->src, expected, src->len), 0);
    ASSERT_EQ(std::memcmp(src->filename, path, sizeof(path)/sizeof(*path)), 0);
    source_free(src);
}
