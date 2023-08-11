/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_lexer.h"
#include "neo_core.h"

#define _(_1, _2) [_1] = _2
static const char *tok_lexemes[TOK__COUNT] = { tkdef(_, NEO_SEP) };
#undef _
#define _(_1, _2) [_1] = #_1
static const char *tok_names[TOK__COUNT] = { tkdef(_, NEO_SEP) };
#undef _
#define _(_1, _2) [_1] = ((sizeof(_2)-1)&255)
static const uint8_t tok_lens[TOK__COUNT] = { tkdef(_, NEO_SEP) };
#undef _
const toktype_t KW_MAPPINGS[KW_MAPPING_CUSTOM_N] = {
    TOK_LI_TRUE,
    TOK_LI_FALSE,
    TOK_OP_LOG_AND,
    TOK_OP_LOG_OR,
    TOK_OP_LOG_NOT
};

static uint32_t utf8_seqlen(uint32_t x) { /* Computes the length of incoming UTF-8 sequence in bytes. Assumes valid UTF-8. */
    if (neo_likely(x > 0 && x < 0x80)) { return 1; } /* ASCII and most common case. */
    else if ((x>>5) == 0x6/*0000'0110*/) { return 2; } /* 2 bytes */
    else if ((x>>4) == 0xe/*0000'1110*/) { return 3; } /* 3 bytes */
    else if ((x>>3) == 0x1e/*0001'1110*/) { return 4; } /* 4 bytes */
    else { return 0; } /* Terminated reached or invalid UTF-8 -> we're done here. */
}

static uint32_t utf8_decode(const uint8_t **p) { /* Decodes utf-8 sequence into UTF-32 codepoint and increments needle. Assumes valid UTF-8. */
    uint32_t cp = (uint32_t)**p;
    uint32_t len = utf8_seqlen(cp);
    if (neo_likely(len == 1)) { ++*p; return cp & 0x7f/*0111'1111*/; } /* ASCII and most common case. */
    else if (neo_unlikely(len == 0)) { return 0; }
    else {
        switch (len) {
            case 2: cp = ((cp<<6) & 0x7ff/*0111'1111'1111*/)|(*++*p & 0x3f/*0011'1111*/); break; /* 2 bytes */
            case 3: /* 3 bytes */
                cp = ((cp<<12) & 0xffff/*1111'1111'1111'1111*/)|((*++*p<<6) & 0xfff/*1111'1111'1111*/);
                cp += *++*p & 0x3f/*0011'1111*/;
                break;
            case 4: /* 4 bytes */
                cp = ((cp<<18) & 0x1fffff/*0001'1111'1111'1111'1111'1111*/)|((*++*p<<12) & 0x3ffff/*0011'1111'1111'1111'1111*/);
                cp += (*++*p<<6) & 0xfff/*1111'1111'1111*/;
                cp += *++*p & 0x3f/*0011'1111*/;
                break;
            default:;
        }
    }
    ++*p;
    return cp;
}

bool source_load(source_t *self, const uint8_t *path) {
    neo_dassert(self && path);
    FILE *f = NULL;
    if (neo_unlikely(!neo_fopen(&f, path, NEO_FMODE_R | NEO_FMODE_BIN))) {
        neo_error("Failed to open file '%s'.", path);
        return false;
    }
    /* Check for BOM and skip it if present */
    uint8_t bom[3];
    size_t bom_len = fread(bom, sizeof(*bom),  sizeof(bom), f);
    bool has_bom = false;
    if (bom_len == sizeof(bom) && memcmp(bom, "\xef\xbb\xbf", sizeof(bom)) == 0) {
        has_bom = true; /* BOM found */
    }
    else { rewind(f); } /* No BOM, rewind to start of file */
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    rewind(f);
    if (has_bom) {
        fseek(f, sizeof(bom), SEEK_SET); /* BOM detected, skip it */
        size -= sizeof(bom);
    }
    if (neo_unlikely(!size)) { /* File is empty */
        fclose(f);
        return false;
    }
    uint8_t *buf = (uint8_t *)neo_memalloc(NULL, size+2); /* +1 for \n +1 for \0 */
    if (neo_unlikely(fread(buf, sizeof(*buf), size, f) != size)) {
        neo_memalloc(buf, 0);
        fclose(f);
        neo_error("Failed to read source file: '%s'", path);
        return false;
    }
    fclose(f);
#if NEO_OS_WINDOWS
    /* We read the file as binary file, so we need to replace \r\n by ourselves, fuck you Windows! */
    for (size_t i = 0; i < size; ++i) {
        if (buf[i] == '\r' && buf[i+1] == '\n') {
            buf[i] = '\n';
            memmove(buf+i+1, buf+i+2, size-i-2); /* Fold data downwards. */
            --size, --i;
        }
    }
#endif
    buf[size] = '\n'; /* Append final newline */
    buf[size+1] = '\0'; /* Append terminator */
    self->filename = path;
    self->src = buf;
    self->len = size;
    return true;
}

#define c32_is_within(c, min, max) ((c) >= (min) && (c) <= (max))
#define c32_is_ascii(c) ((c) < 0x80)
#define c32_is_ascii_whitespace(c) ((c) == ' ' || (c) == '\t' || (c) == '\v' || (c) == '\r') /* \n is no whitespace - it's a punctuation token. */
#define c32_is_ascii_digit(c) c32_is_within((c), '0', '9')
#define c32_is_ascii_hex_digit(c) (c32_is_ascii_digit(c) || c32_is_within((c), 'a', 'f') || c32_is_within((c), 'A', 'F'))
#define c32_is_ascii_bin_digit(c) ((c) == '0' || (c) == '1')
#define c32_is_ascii_oct_digit(c) c32_is_within((c), '0', '7')
#define c32_is_ascii_alpha(c) (c32_is_within((c), 'a', 'z') || c32_is_within((c), 'A', 'Z'))
#define c32_is_ascii_alphanumeric(c) (c32_is_ascii_alpha(c) || c32_is_ascii_digit(c))
#define c32_to_ascii_lower(c) ((c)|' ') /* Only works if c is ASCII. */
#define c32_to_ascii_upper(c) ((c)&~' ') /* Only works if c is ASCII. */
static NEO_AINLINE bool c32_is_ident_start(uint32_t c) {
    return c == '_' || c32_is_ascii_alpha(c); /* TODO: Identifier restrictions? */
}
static NEO_AINLINE bool c32_is_ident_cont(uint32_t c) {
    return c == '_' || c32_is_ascii_alphanumeric(c); /* TODO: Identifier restrictions? */
}

static NEO_AINLINE bool c32_is_whitespace(uint32_t c) {
    return c32_is_ascii_whitespace(c)
        || c == 0x0085u  /* NEXT-LINE from latin1 */
        || c == 0x200eu  /* LEFT-TO-RIGHT BIDI MARK */
        || c == 0x200fu  /* RIGHT-TO-LEFT BIDI MARK */
        || c == 0x2028u  /* LINE-SEPARATOR */
        || c == 0x2029u; /* PARAGRAPH-SEPARATOR */
}

static NEO_AINLINE void decode_cached_tmp(lexer_t *self) {
    neo_dassert(self && self->src && self->needle);
    const uint8_t *tmp = self->needle;
    self->cp_curr = utf8_decode(&tmp);
    self->cp_next = utf8_decode(&tmp);
}

#define peek(l) ((l)->cp_curr)
#define peek_next(l) ((l)->cp_next)
#define is_done(l) (peek(l) == 0)
static void consume(lexer_t *self) {
    neo_dassert(self && self->src && self->needle);
    if (neo_unlikely(is_done(self))) { /* We're done here. */
        self->line_end = self->src+self->src_dat.len;
        return;
    }
    else if (peek(self) == '\n') { /* Newline just started. */
        ++self->line;
        self->col = 1; /* Reset */
        self->line_start = self->needle+1;
        /* Find next line ending. */
        do { ++self->line_end; }
        while (*self->line_end && *self->line_end != '\n');
    } else { /* No special event, just increment column. */
        ++self->col;
    }
    neo_dassert(neo_bnd_check(self->needle, self->src, self->src_dat.len));
    self->needle += utf8_seqlen(*self->needle); /* Increment needle to next UTF-8 sequence. */
    decode_cached_tmp(self); /* Decode cached codepoints. */
}

static NEO_AINLINE bool ismatch(lexer_t *self, uint32_t c) {
    neo_dassert(self);
    if (peek(self) == c) {
        consume(self);
        return true;
    }
    return false;
}

#define COMMENT_START '#'
#define COMMENT_BLOCK '*'

static void consume_whitespace(lexer_t *self) {
    if (c32_is_whitespace(peek(self))) {
        consume(self);
    } else if (peek(self) == COMMENT_START) { /* We've reached a comment. */
        if (peek_next(self) == COMMENT_BLOCK) { /* Consume block comment. */
            consume(self); consume(self);
            while (!is_done(self) && !(peek(self) == COMMENT_BLOCK && peek_next(self) == COMMENT_START)) {
                consume(self);
            }
            consume(self); consume(self);
        } else {
            while (!is_done(self) && peek(self) != '\n') { /* Consume line comment. */
                consume(self);
            }
        }
    } else {
        return;
    }
    consume_whitespace(self); /* Recurse. TODO: Replace with iteration. (Prefer iteration over recursion). */
}

static token_t mktok(const lexer_t *self, toktype_t type, int pdelta) {
    neo_dassert(self);
    (void)pdelta; /* TODO */
    token_t tok;
    memset(&tok, 0, sizeof(tok));
    tok.type = type;
    if (neo_likely(type != TOK_ME_EOF)) { /* Regular token case. */
        ptrdiff_t delta = self->needle-self->tok_start;
        neo_assert(delta >= 0 && "invalid lexeme length");
        const uint8_t *lp = self->needle-delta;
        neo_assert(neo_bnd_check(lp, self->src, self->src_dat.len) && "invalid lexeme pointer"); /* bounds check */
        tok.lexeme = (srcspan_t){.p=lp,.len=(uint32_t)delta};
        tok.col = (uint32_t)abs((int32_t)self->col-(int32_t)delta);
        delta = self->line_end-self->line_start; /* Line delta */
        neo_assert(delta >= 0 && "invalid lexeme line length");
        tok.lexeme_line = (srcspan_t){.p=self->line_start,.len=(uint32_t)delta};
    } else { /* EOF or empty token case. */
        tok.lexeme = (srcspan_t){.p=(const uint8_t *)"",.len=0};
    }
    tok.line = self->line;
    tok.file = self->src_dat.filename;
    return tok;
}

static token_t consume_numeric_literal(lexer_t *self) { /* Consumes either int or float literal. */
    neo_dassert(self);
    toktype_t type = TOK_LI_INT; /* Assume integer literal by default. */
    radix_t rdx = RADIX_DEC; /* Assume decimal by default. */
    if (peek(self) == '0') { /* Check for radix prefix. */
        switch (c32_to_ascii_lower(peek_next(self))&127) {
            case 'b': rdx = RADIX_BIN; break;
            case 'o': rdx = RADIX_OCT; break;
            case 'x': rdx = RADIX_HEX; break;
        }
        if (rdx != RADIX_DEC) { /* Because trailing decimal zeros are allowed. */
            consume(self); consume(self);
        }
    }
    while (c32_is_ascii_hex_digit(peek(self)) || peek(self) == '_') { /* Consume integer literal (or first integer part of float literal: float(int.int)). */
        consume(self);
    }
    if (peek(self) == '.') { /* Check for float literal. */
        type = TOK_LI_FLOAT;
        consume(self);
        while (c32_is_ascii_hex_digit(peek(self)) || peek(self) == '_') { /* Consume second integer part of float literal. */
            consume(self);
        }
    }
    token_t tok = mktok(self, type, 0);
    tok.radix = rdx;
    if (rdx != RADIX_DEC) { /* If radix is not decimal, we need to strip the radix prefix. (0*) */
        tok.lexeme.p += 2;
        tok.lexeme.len -= 2;
    }
    return tok;
}

static bool kw_found(const lexer_t *self, toktype_t i) {
    return *tok_lexemes[i] == *self->tok_start
        && tok_lens[i] == llabs(self->needle-self->tok_start)
        && memcmp(tok_lexemes[i], self->tok_start, tok_lens[i]) == 0;
}

static token_t consume_keyword_or_identifier(lexer_t *self) {
    neo_dassert(self);
    while (c32_is_ident_cont(peek(self))) {
        consume(self);
    }
    for (int i = KWR_START; i <= KWR_END; ++i) { /* Search for builtin keyword. */
        if (kw_found(self, (toktype_t)i)) { /* Keyword found. */
            return mktok(self, (toktype_t)i, 0);
        }
    }
    for (int i = 0; i < KW_MAPPING_CUSTOM_N; ++i) { /* Search for custom keyword mapping. */
        if (kw_found(self, KW_MAPPINGS[i])) { /* Keyword found. */
            return mktok(self, KW_MAPPINGS[i], 0);
        }
    }
    return mktok(self, TOK_LI_IDENT, 0); /* No builtin keyword found, return identifier. */
}

void lexer_init(lexer_t *self) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
}

void lexer_set_src(lexer_t *self, const source_t *src) {
    neo_dassert(self && src);
    self->src_dat = *src;
    self->src = self->needle = self->tok_start = self->line_start = self->line_end = self->src_dat.src;
    self->line = self->col = 1;
    decode_cached_tmp(self); /* Decode cached codepoints. */
    /* Find first line ending. */
    while (*self->line_end && *self->line_end != '\n') {
        ++self->line_end;
    }
}

token_t lexer_scan_next(lexer_t *self) {
    neo_dassert(self && self->src);
    consume_whitespace(self); /* Consume whitespace and comments. */
    if (neo_unlikely(is_done(self))) { /* EOF? */
        return mktok(self, TOK_ME_EOF, 0);
    }
    self->tok_start = self->needle;
    if (c32_is_ascii_digit(peek(self))) { /* Digit or 0x, 0b or 0c? */
        return consume_numeric_literal(self);
    }
    uint32_t c = peek(self);
    consume(self);
    switch (c) {
        case '(': return mktok(self, TOK_PU_L_PAREN, 0);
        case ')': return mktok(self, TOK_PU_R_PAREN, 0);
        case '[': return mktok(self, TOK_PU_L_BRACKET, 0);
        case ']': return mktok(self, TOK_PU_R_BRACKET, 0);
        case '{': return mktok(self, TOK_PU_L_BRACE, 0);
        case '}': return mktok(self, TOK_PU_R_BRACE, 0);
        case ',': return mktok(self, TOK_PU_COMMA, 0);
        case '\n': return mktok(self, TOK_PU_NEWLINE, 0);

        case '.': return mktok(self, TOK_OP_DOT, 0);
        case '~': return mktok(self, TOK_OP_BIT_COMPL, 0);
        case '=': return mktok(self, ismatch(self, '=') ? TOK_OP_EQUAL : TOK_OP_ASSIGN, 0);
        case '+': return mktok(self, ismatch(self, '=') ? TOK_OP_ADD_ASSIGN : ismatch(self, '+') ? TOK_OP_INC : TOK_OP_ADD, 0);
        case '-': return mktok(self, ismatch(self, '>') ? TOK_PU_ARROW : ismatch(self, '=') ? TOK_OP_SUB_ASSIGN : ismatch(self, '-') ? TOK_OP_DEC : TOK_OP_SUB, 0);
        case '*': return mktok(self, ismatch(self, '*') ? ismatch(self, '=') ? TOK_OP_POW_ASSIGN : TOK_OP_POW : ismatch(self, '=') ? TOK_OP_MUL_ASSIGN : TOK_OP_MUL, 0);
        case '/': return mktok(self, ismatch(self, '=') ? TOK_OP_DIV_ASSIGN : TOK_OP_DIV, 0);
        case '%': return mktok(self, ismatch(self, '=') ? TOK_OP_MOD_ASSIGN : TOK_OP_MOD, 0);
        case '&': return mktok(self, ismatch(self, '&') ? TOK_OP_BIT_AND : ismatch(self, '=') ? TOK_OP_BIT_AND_ASSIGN : TOK_OP_BIT_AND, 0);
        case '|': return mktok(self, ismatch(self, '|') ? TOK_OP_BIT_OR : ismatch(self, '=') ? TOK_OP_BIT_OR_ASSIGN : TOK_OP_BIT_OR, 0);
        case '^': return mktok(self, ismatch(self, '=') ? TOK_OP_BIT_XOR_ASSIGN : TOK_OP_BIT_XOR, 0);
        case '!': {
            if (ismatch(self, '=')) {
                return mktok(self, TOK_OP_NOT_EQUAL, 0);
            } else if (ismatch(self, '+')) {
                return mktok(self, ismatch(self, '=') ? TOK_OP_ADD_ASSIGN_NO_OV : TOK_OP_ADD_NO_OV, 0);
            } else if (ismatch(self, '-')) {
                return mktok(self, ismatch(self, '=') ? TOK_OP_SUB_ASSIGN_NO_OV : TOK_OP_SUB_NO_OV, 0);
            } else if (ismatch(self, '*')) {
                if (ismatch(self, '*')) { /* Power operator */
                    return mktok(self, ismatch(self, '=') ? TOK_OP_POW_ASSIGN_NO_OV : TOK_OP_POW_NO_OV, 0);
                } else {
                    return mktok(self, ismatch(self, '=') ? TOK_OP_MUL_ASSIGN_NO_OV : TOK_OP_MUL_NO_OV, 0);
                }
            } else {
                return mktok(self, TOK_ME_ERR, 0); /* Unknown token. */
            }
        }
        case '<': {
            if (ismatch(self, '<')) {
                if (ismatch(self, '<')) {
                    return mktok(self, ismatch(self, '=') ? TOK_OP_BIT_ROL_ASSIGN : TOK_OP_BIT_ROL, 0);
                } else {
                    return mktok(self, ismatch(self, '=') ? TOK_OP_BIT_ASHL_ASSIGN : TOK_OP_BIT_ASHL, 0);
                }
            } else if (ismatch(self, '=')) {
                return mktok(self, TOK_OP_LESS_EQUAL, 0);
            } else {
                return mktok(self, TOK_OP_LESS, 0);
            }
        }
        case '>': {
            if (ismatch(self, '=')) { return mktok(self, TOK_OP_GREATER_EQUAL, 0); }
            else {
                if (ismatch(self, '>')) {
                    if (ismatch(self, '=')) { return mktok(self, TOK_OP_BIT_ASHR_ASSIGN, 0); }
                    else {
                        if (ismatch(self, '>')) {
                            if (ismatch(self, '>')) {
                                if (ismatch(self, '=')) { return mktok(self, TOK_OP_BIT_LSHR_ASSIGN, 0); }
                                else { return mktok(self, TOK_OP_BIT_LSHR, 0); }
                            } else {
                                if (ismatch(self, '=')) { return mktok(self, TOK_OP_BIT_ROR_ASSIGN, 0); }
                                else { return mktok(self, TOK_OP_BIT_ROR, 0); }
                            }
                        }
                        else { return mktok(self, TOK_OP_BIT_ASHR, 0); }
                    }
                } else { return mktok(self, TOK_OP_GREATER, 0); }
            }
        }
        default: {
            if (neo_likely(c32_is_ident_start(c))) { /* Identifier ? */
                return consume_keyword_or_identifier(self);
            } else {
                return mktok(self, TOK_ME_ERR, 0); /* Unknown token. */
            }
        }
    }
}

size_t lexer_drain(lexer_t *self, token_t **tok) {
    neo_dassert(self && tok);
    size_t cap = 1<<9, len = 0;
    *tok = (token_t *)neo_memalloc(NULL, cap*sizeof(**tok));
    for (;;) {
        token_t t = lexer_scan_next(self);
        if (neo_unlikely(t.type == TOK_ME_EOF)) { break; }
        if (neo_unlikely(len >= cap)) {
            *tok = (token_t *)neo_memalloc(*tok, (cap<<=1)*sizeof(**tok));
        }
        (*tok)[len++] = t;
    }
    return len;
}

void lexer_free(lexer_t *self) {
    neo_dassert(self);
    (void)self;
}

void token_dump(const token_t *self) {
    neo_dassert(self);
    printf("%" PRIu32 ":%" PRIu32 " Type: %s, Lexeme: %.*s\n",
        self->line,
        self->col,
        tok_names[self->type],
        (int)(self->type == TOK_PU_NEWLINE ? sizeof("\\n")-1 : self->lexeme.len),
        self->type == TOK_PU_NEWLINE ? (const uint8_t *)"\\n" : self->lexeme.p
    );
}
