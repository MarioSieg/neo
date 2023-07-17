/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_lexer.h"
#include "neo_core.h"

static unsigned utf8_seqlen(uint32_t x) { /* Computes the length of incoming UTF-8 sequence in bytes. Assumes valid UTF-8. */
    if (neo_likely(x > 0 && x < 0x80)) { return 1; } /* ASCII and most common case. */
    else if ((x>>5) == 0x6) { return 2; } /* 2 bytes */
    else if ((x>>4) == 0xe) { return 3; } /* 3 bytes */
    else if ((x>>3) == 0x1e) { return 4; } /* 4 bytes */
    else { return 0; } /* Terminated reached or invalid UTF-8 -> we're done here. */
}

static uint32_t utf8_decode(const uint8_t **p) { /* Decodes utf-8 sequence into UTF-32 codepoint and increments needle. Assumes valid UTF-8. */
    uint32_t cp = (uint32_t)**p;
    unsigned len = utf8_seqlen(cp);
    if (neo_likely(len == 1)) { ++*p; return cp & 0x7f; } /* ASCII and most common case. */
    else if (neo_unlikely(len == 0)) { return 0; }
    else {
        switch (len) {
            default:;
            case 2: cp = ((cp<<6) & 0x7ff)|(*++*p & 0x3f); break; /* 2 bytes */
            case 3: /* 3 bytes */
                cp = ((cp<<12) & 0xffff)|((*++*p<<6) & 0xfff);
                cp += *++*p & 0x3f;
                break;
            case 4: /* 4 bytes */
                cp = ((cp<<18) & 0x1fffff)|((*++*p<<12) & 0x3ffff);
                cp += (*++*p<<6) & 0xfff;
                cp += *++*p & 0x3f;
                break;
        }
    }
    ++*p;
    return cp;
}

typedef enum { UNIERR_OK, UNIERR_TOO_SHORT, UNIERR_TOO_LONG, UNIERR_TOO_LARGE, UNIERR_OVERLONG, UNIERR_HEADER_BITS, UNIERR_SURROGATE } unicode_err_t;

static __attribute__((unused)) unicode_err_t utf8_validate(const uint8_t *buf, size_t len, size_t *ppos) { /* Validates the UTF-8 string and returns an error code and error position. */
    neo_dbg_assert(buf && ppos);
    size_t pos = 0;
    uint32_t cp = 0;
    while (pos < len) {
        size_t np = pos+16;
        if (np <= len) { /* If it is safe to read 8 more bytes and check that they are ASCII. */
            uint64_t v1 = *(const uint64_t *)(buf+pos);
            uint64_t v2 = *(const uint64_t *)(buf+pos+sizeof(v1));
            if (!((v1|v2)&UINT64_C(0x8080808080808080))) {
                pos = np;
                continue;
            }
        }
        uint8_t b = buf[pos];
        while (b < 0x80) {
            if (neo_likely(++pos == len)) { *ppos = len; return UNIERR_OK; }
            b = buf[pos];
        }
        if ((b & 0xe0) == 0xc0) {
            np = pos+2;
            if (neo_unlikely(np > len)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            cp = (b & 0x1fu)<<6u | (buf[pos+1] & 0x3fu);
            if (neo_unlikely((cp < 0x80) || (0x7ff < cp))) { *ppos = pos; return UNIERR_OVERLONG; }
        } else if ((b & 0xf0) == 0xe0) {
            np = pos+3;
            if (neo_unlikely(np > len)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+2] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            cp = (b & 0xfu)<<12u | (buf[pos+1] & 0x3fu)<<6u | (buf[pos+2] & 0x3fu);
            if (neo_unlikely((cp < 0x800) || (0xffff < cp))) { *ppos = pos; return UNIERR_OVERLONG; }
            if (neo_unlikely(0xd7ff < cp && cp < 0xe000)) { *ppos = pos; return UNIERR_SURROGATE; }
        } else if ((b & 0xf8) == 0xf0) {
            np = pos+4;
            if (neo_unlikely(np > len)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+2] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+3] & 0xc0) != 0x80)) { *ppos = pos; return UNIERR_TOO_SHORT; }
            cp = (b & 0x7u)<<18u | (buf[pos+1] & 0x3fu)<<12u | (buf[pos+2] & 0x3fu)<<6u | (buf[pos+3] & 0x3fu);
            if (neo_unlikely(cp <= 0xffff)) { *ppos = pos; return UNIERR_OVERLONG; }
            if (neo_unlikely(0x10ffff < cp)) { *ppos = pos; return UNIERR_TOO_LARGE; }
        } else { /* We either have too many continuation bytes or an invalid leading byte. */
            if (neo_unlikely((b & 0xc0) == 0x80)) { *ppos = pos; return UNIERR_TOO_LONG; }
            else { *ppos = pos; return UNIERR_HEADER_BITS; }
        }
        pos = np;
    }
    *ppos = len;
    return UNIERR_OK;
}

#define c32_is_within(c, min, max) ((c) >= (min) && (c) <= (max))
#define c32_is_ascii(c) ((c) < 0x80)
#define c32_is_ascii_whitespace(c) ((c) == ' ' || (c) == '\t' || (c) == '\v' || (c) == '\r') /* \n is no whitespace - it's a punctuation token in Neo */
#define c32_is_ascii_digit(c) c32_is_within((c), '0', '9')
#define c32_is_ascii_hexdigit(c) (c32_is_ascii_digit(c) || c32_is_within((c), 'a', 'f') || c32_is_within((c), 'A', 'F'))
#define c32_is_ascii_bin_digit(c) ((c) == '0' || (c) == '1')
#define c32_is_ascii_oct_digit(c) c32_is_within((c), '0', '7')
#define c32_is_ascii_alpha(c) (c32_is_within((c), 'a', 'z') || c32_is_within((c), 'A', 'Z'))
#define c32_is_ascii_alphanumeric(c) (c32_is_ascii_alpha(c) || c32_is_ascii_digit(c))
#define c32_is_ascii_identifier_start(c) (c32_is_ascii_alpha(c) || (c) == '_')
#define c32_is_ascii_identifier_part(c) (c32_is_ascii_alphanumeric(c) || (c) == '_')

static NEO_AINLINE bool c32_is_whitespace(uint32_t c) {
    return c32_is_ascii_whitespace(c)
        || c == 0x0085u  /* NEXT-LINE from latin1 */
        || c == 0x200eu  /* LEFT-TO-RIGHT BIDI MARK */
        || c == 0x200fu  /* RIGHT-TO-LEFT BIDI MARK */
        || c == 0x2028u  /* LINE-SEPARATOR */
        || c == 0x2029u; /* PARAGRAPH-SEPARATOR */
}

static NEO_AINLINE void decode_cached_tmp(lexer_t *self) {
    neo_dbg_assert(self && self->src && self->needle);
    const uint8_t *tmp = self->needle;
    self->cp_curr = utf8_decode(&tmp);
    self->cp_next = utf8_decode(&tmp);
}

#define peek(l) ((l)->cp_curr)
#define peek_next(l) ((l)->cp_next)
#define is_done(l) (peek(l) == 0)
static __attribute__((unused)) void consume(lexer_t *self) {
    neo_dbg_assert(self && self->src && self->needle);
    neo_dbg_assert(neo_bnd_check(self->needle, self->src, self->src_len));
    if (neo_unlikely(is_done(self))) { return; } /* We're done here. */
    else if (neo_unlikely(peek(self) == '\n')) { /* Newline just started. */
        ++self->line;
        self->col = 1;
        self->line_start = self->needle+1;
    } else { /* No special event, just increment column. */
        ++self->col;
    }
    self->needle += utf8_seqlen(*self->needle); /* Increment needle to next UTF-8 sequence. */
    decode_cached_tmp(self); /* Decode cached codepoints. */
}

void lexer_set_src(lexer_t *self, const uint8_t *src, size_t src_len) {
    neo_dbg_assert(self && src && src_len);
    self->src_len = src_len;
    self->src = self->needle = self->tok_start = self->line_start = src;
    self->line = self->col = 1;
    decode_cached_tmp(self); /* Decode cached codepoints. */
}
