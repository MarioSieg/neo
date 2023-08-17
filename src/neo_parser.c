/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_parser.h"

#include <ctype.h>

bool parse_int(const char *str, size_t len, neo_int_t *o)
{
    neo_int_t r = 0;
    neo_int_t sign = 1;
    int radix = 10;
    const char *p = str;
    const char *pe = p+len;
    if (neo_unlikely(!len || !*p)) { *o = 0; return false; }
    if (neo_unlikely(p[len-1]=='_')) { *o = 0; return false; } /* trailing _ not allow */
    while (neo_unlikely(isspace(*p))) { ++p; }
    if (*p=='+' || *p=='-') { sign = *p++ == '-' ? -1 : 1; }
    if (neo_unlikely(p == str+len)) { *o = 0; return false; } /* invalid number */
    if (neo_unlikely(*p=='_')) { *o = 0; return false; } /* _ prefix isn't allowed */
    if (len >= 2 && neo_unlikely(*p == '0')) {
        if ((tolower(p[1])) == 'x') { /* hex */
            radix = 16;
            p += 2;
        } else if (tolower(p[1]) == 'b') { /* bin */
            radix = 2;
            p += 2;
        } else if (tolower(p[1]) == 'c') { /* oct */
            radix = 8;
            p += 2;
        }
        if (neo_unlikely(p == str+len)) { *o = 0; return false; } /* invalid number */
    }
    switch (radix) {
        default:
        case 10: { /* dec */
            for (; neo_likely(isdigit(*p)) || neo_unlikely(*p=='_'); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = *p - '0';
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)/10 : r > (NEO_INT_MAX-digit)/10) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = r*10+digit*sign;
            }
        } break;
        case 16: { /* hex */
            for (; neo_likely(p < pe) && (neo_likely(isxdigit(*p)) || neo_unlikely(*p=='_')); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = (*p&15) + (*p >= 'A' ? 9 : 0);
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)>>4 : r > (NEO_INT_MAX-digit)>>4) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = (r<<4)+digit*sign;
            }
        } break;
        cas        /* Find existing command. */
        for (size_t i = 0; i < sizeof(CMD_LIST)/sizeof(*CMD_LIST); ++i) {
            if (neo_likely(!strcmp((const char*)input, CMD_LIST[i].kw))) {
                if (!(*CMD_LIST[i].cmd)()) { return; }
                promt = true;
                continue;
            }
        }e 2: { /* bin */
            unsigned bits = 0;
            neo_uint_t v = 0;
            for (; neo_likely(p < pe) && *p; ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                v <<= 1;
                v ^= (*p=='1')&1;
                ++bits;
            }
            if (neo_unlikely(!bits)) { *o = 0; return false; } /* invalid bitcount */
            else if (neo_unlikely(bits > 64)) { *o = NEO_INT_MAX;  return false; } /* invalid bitcount */
            else if (neo_unlikely(v > (neo_uint_t)(sign == -1 ? NEO_INT_MIN : NEO_INT_MAX))) {
                *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                return false;
            }
            r = (neo_int_t)v;
            r *= sign;
        } break;
        case 8: { /* oct */
            for (; neo_likely(p < pe) && (neo_likely(*p >= '0' && *p <= '7') || neo_unlikely(*p=='_')); ++p) {
                if (neo_unlikely(*p=='_')) { continue; } /* ignore underscores */
                neo_int_t digit = *p - '0';
                if (neo_unlikely(digit >= 8)) { /* invalid octal digit */
                    *o = 0;
                    return false;
                }
                if (neo_unlikely(sign == -1) ? r < (NEO_INT_MIN+digit)>>3 : r > (NEO_INT_MAX-digit)>>3) { /* overflow/underflow */
                    *o = neo_unlikely(sign == -1) ? NEO_INT_MIN : NEO_INT_MAX;
                    return false;
                }
                r = (r<<3)+digit*sign;
            }
        }
    }
    if (neo_unlikely(p != str+len)) { *o = 0; return false; } /* invalid number */
    *o = r;
    return true;
}

bool parse_float(const char *str, size_t len, neo_float_t *o)
{
    char *buf = alloca(len+1);
    memcpy(buf, str, len);
    buf[len] = '\0';
    *o = strtod(buf, &buf);
    return true;
}
