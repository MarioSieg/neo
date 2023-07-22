/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_lexer.h"
#include "neo_core.h"

static uint32_t utf8_seqlen(uint32_t x) { /* Computes the length of incoming UTF-8 sequence in bytes. Assumes valid UTF-8. */
    if (neo_likely(x > 0 && x < 0x80)) { return 1; } /* ASCII and most common case. */
    else if ((x>>5) == 0x6) { return 2; } /* 2 bytes */
    else if ((x>>4) == 0xe) { return 3; } /* 3 bytes */
    else if ((x>>3) == 0x1e) { return 4; } /* 4 bytes */
    else { return 0; } /* Terminated reached or invalid UTF-8 -> we're done here. */
}

static uint32_t utf8_decode(const uint8_t **p) { /* Decodes utf-8 sequence into UTF-32 codepoint and increments needle. Assumes valid UTF-8. */
    uint32_t cp = (uint32_t)**p;
    uint32_t len = utf8_seqlen(cp);
    if (neo_likely(len == 1)) { ++*p; return cp & 0x7f; } /* ASCII and most common case. */
    else if (neo_unlikely(len == 0)) { return 0; }
    else {
        switch (len) {
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
            default:;
        }
    }
    ++*p;
    return cp;
}

bool source_load(source_t *self, const uint8_t *path) {
    neo_dbg_assert(self && path);
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
static NEO_UNUSED void consume(lexer_t *self) {
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
