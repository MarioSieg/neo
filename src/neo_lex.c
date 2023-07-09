/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_lex.h"

#include <stdio.h>

#define _(_1, _2) [_1] = _2
static const char* token_type_names[TOK_COUNT_] = {
	INJECT_TOKEN_TYPES(_, NEO_ENUM_SEP)
};
#undef _
#define _(_1, _2) [_1] = ((sizeof(_2)-1)&255)
static const uint8_t token_lengths[TOK_COUNT_] = {
	INJECT_TOKEN_TYPES(_, NEO_ENUM_SEP)
};
#undef _

static const uint8_t utf8_seq_lut[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};
#define utf8_seq_len(c) (utf8_seq_lut[*(c)]+1) /* Lookup length of UTF-8 sequence. */

/* UTF-32 unicode code point helpers */
#define cp32_within_interval(c, a, b) ((a) <= (c) && (c) <= (b))
#define cp32_is_asc(c) ((c) < 0x80)
#define cp32_is_asc_whitespace(c) (c == ' ' || c == '\t' || c == '\n' || c == '\r')
#define cp32_is_asc_digit(c) (cp32_within_interval(c, '0', '9'))
#define cp32_is_asc_hex_digit(c) (cp32_within_interval(c, '0', '9') || cp32_within_interval(c, 'a', 'f') || cp32_within_interval(c, 'A', 'F'))
#define cp32_is_asc_oct_digit(c) (cp32_within_interval(c, '0', '7'))
#define cp32_is_asc_bin_digit(c) (c == '0' || c == '1')
#define cp32_is_asc_alpha(c) (cp32_within_interval(c, 'a', 'z') || cp32_within_interval(c, 'A', 'Z'))
#define cp32_is_asc_alnum(c) (cp32_is_asc_alpha(c) || cp32_is_asc_digit(c))
static inline bool cp32_is_uni_whitespace(uint32_t c) {
	return cp32_is_asc_whitespace(c)
		|| c == 0x0085  /* NEXT LINE LATIN1 */
		|| c == 0x200e  /* LEFT-TO-RIGHT BIDI MARK */
		|| c == 0x200f  /* RIGHT-TO-LEFT BIDI MARK */
		|| c == 0x2028  /* LINE SEPARATOR */
		|| c == 0x2029; /* PARAGRAPH SEPARATOR */
}
static inline bool cp32_is_uni_ident_start(uint32_t c) { /* Identifier start. */
	return c == '_' || c == '$' || cp32_is_asc_alpha(c);
}
static inline bool cp32_is_uni_ident_cont(uint32_t c) { /* Identifier continuation. */
	return c == '_' || c == '$' || cp32_is_asc_alnum(c);
}
static void cp32_print(FILE *f, uint32_t c) {
	if (cp32_is_asc(c)) {
		fputc(c, f);
	}
	else {
		fprintf(f, "\\u%04x", c);
	}
}

