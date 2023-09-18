/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_core.h"

NEO_THREAD_LOCAL void *volatile neo_tls_proxy = NULL;

#if NEO_OS_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <locale.h>
#   include <fcntl.h>
#   include <io.h>
#elif NEO_OS_POSIX
#   include <unistd.h>
#else
#   error "unsupported platform"
#endif

void neo_assert_impl(const char *expr, const char *file, int line) {
    neo_panic("%s:%d Internal NEO ERROR - expression: '%s'", file, line, expr);
}

void neo_panic(const char *msg, ...) {
    fprintf(stderr, "%s", NEO_CCRED);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "%s", NEO_CCRESET);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
    neo_unreachable();
}

static neo_osi_t osi_data; /* Global OSI data. Only set once inneo_osi_init(). */

void neo_osi_init(void) {
    memset(&osi_data, 0, sizeof(osi_data));
#if NEO_OS_WINDOWS
    neo_assert(setlocale(LC_ALL, ".UTF-8") && "failed to set locale");
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    osi_data.page_size = info.dwPageSize ? (uint32_t)info.dwPageSize : 0x1000;
#elif NEO_OS_LINUX || NEO_OS_BSD
    long ps = sysconf(_SC_PAGESIZE);
    osi_data.page_size = ps > 0 ? (uint32_t)ps : 0x1000;
#else
#   error "unsupported platform"
#endif
}

void neo_osi_shutdown(void) {

}

const neo_osi_t *neo_osi = &osi_data;

void *neo_defmemalloc(void *blk, size_t len) {
    if (!len) { /* deallocation */
        neo_dealloc(blk);
        return NULL;
    } else if(!blk) {  /* allocation */
        blk = neo_alloc_malloc(len);
        neo_assert(blk && "allocation failed");
        return blk;
    } else { /* reallocation */
        void *newblock = neo_alloc_realloc(blk, len);
        neo_assert(newblock && "reallocation failed");
        return newblock;
    }
}

void neo_mempool_init(neo_mempool_t *self, size_t cap) {
    neo_dassert(self);
    memset(self, 0, sizeof(*self));
    cap = cap ? cap : 1<<9;
    self->cap = cap;
    self->top = neo_memalloc(NULL, cap);
    memset(self->top, 0, cap); /* Zero the memory. */
}

void *neo_mempool_alloc(neo_mempool_t *self, size_t len) {
    neo_dassert(self);
    neo_assert(len);
    size_t total = self->len+len;
    if (total >= self->cap) {
        size_t old = self->cap;
        do {
            self->cap<<=1;
        } while (self->cap <= total);
        self->top = neo_memalloc(self->top, self->cap);
        size_t delta = self->cap-old;
        memset((uint8_t *)self->top + old, 0, delta); /* Zero the new memory. */
    }
    void *p = (uint8_t *)self->top + self->len;
    self->len += len;
    ++self->num_allocs;
    return p;
}

void *neo_mempool_alloc_aligned(neo_mempool_t *self, size_t len, size_t align) {
    neo_dassert(self);
    neo_dassert(align && align >= sizeof(void*) && !(align&(align-1)));
    uintptr_t off = (uintptr_t)align-1+sizeof(void *);
    void *p = neo_mempool_alloc(self, len + off);
    return (void *)(((uintptr_t)p+off) & ~(align-1));
}

size_t neo_mempool_alloc_idx(neo_mempool_t *self, size_t len, uint32_t base, size_t lim, void **pp) {
    neo_dassert(self && len);
    size_t idx = self->len+base*len;
    neo_assert(idx <= lim && "Pool index limit reached");
    void *p = neo_mempool_alloc(self, len);
    if (pp) { *pp = p; }
    idx /= len;
    return idx;
}

void *neo_mempool_realloc(neo_mempool_t *self, void *blk, size_t oldlen, size_t newlen) {
    neo_dassert(self);
    neo_assert(blk && oldlen && newlen);
    if (neo_unlikely(oldlen == newlen)) { return blk; }
    const void *prev = blk; /* We need to copy the old data into the new block. */
    blk = neo_mempool_alloc(self, newlen);
    memcpy(blk, prev, oldlen); /* Copy the old data into the new block. This is safe because the old data is still in the self. */
    return blk;
}

void neo_mempool_reset(neo_mempool_t *self) {
    neo_dassert(self);
    self->len = 0;
    self->num_allocs = 0;
}

void neo_mempool_free(neo_mempool_t *self) {
    neo_dassert(self);
    neo_memalloc(self->top, 0);
}

bool record_eq(record_t a, record_t b, rtag_t tag) {
    switch (tag) {
        case RT_INT: return a.as_int == b.as_int;
        case RT_FLOAT: return a.as_float == b.as_float; /* TODO: Use ULP-based comparison? */
        case RT_CHAR: return a.as_char == b.as_char;
        case RT_BOOL: return a.as_bool == b.as_bool;
        case RT_REF: return a.as_ref == b.as_ref;
        default: return false;
    }
}

#define get_fmodstr(_)\
    if (mode == NEO_FMODE_R) { modstr = _##"r"; }\
    else if (mode == NEO_FMODE_W) { modstr = _##"w"; }\
    else if (mode == NEO_FMODE_A) { modstr = _##"a"; }\
    else if (mode == (NEO_FMODE_R|NEO_FMODE_TXT)) { modstr = _##"rt"; }\
    else if (mode == (NEO_FMODE_W|NEO_FMODE_TXT)) { modstr = _##"wt"; }\
    else if (mode == (NEO_FMODE_A|NEO_FMODE_TXT)) { modstr = _##"at"; }\
    else if (mode == (NEO_FMODE_R|NEO_FMODE_BIN)) { modstr = _##"rb"; }\
    else if (mode == (NEO_FMODE_W|NEO_FMODE_BIN)) { modstr = _##"wb"; }\
    else if (mode == (NEO_FMODE_A|NEO_FMODE_BIN)) { modstr = _##"ab"; }\
    else { neo_panic("Invalid file mode: %d", mode); }

bool neo_fopen(FILE **fp, const uint8_t *filepath, int mode) {
    neo_dassert(fp && filepath && mode);
    *fp = NULL;
#if NEO_OS_WINDOWS
    int len = MultiByteToWideChar(CP_UTF8, 0, (const CHAR *)filepath, -1, NULL, 0);
    if (neo_unlikely(!len)) {
        return false;
    }
    bool heap = len > 0x1000;
    WCHAR *wstr = heap ? neo_memalloc(NULL, len*sizeof(*wstr)) : alloca(len*sizeof(*wstr));
    if (neo_unlikely(!MultiByteToWideChar(CP_UTF8, 0, (const CHAR *)filepath, -1, wstr, len))) {
        return false;
    }
    const WCHAR *modstr = NULL;
    get_fmodstr(L);
    errno_t e = _wfopen_s(fp, wstr, modstr);
    if (heap) { neo_memalloc(wstr, 0); }
    return !e && *fp != NULL;
#else
    const char *modstr = NULL;
    get_fmodstr()
    return (*fp = fopen((const char *)filepath, modstr)) != NULL;
#endif
}

#undef get_fmodstr

neo_unicode_error_t neo_utf8_validate(const uint8_t *buf, size_t len, size_t *ppos) { /* Validates the UTF-8 string and returns an error code and error position. */
    neo_dassert(buf && ppos);
    size_t pos = 0;
    uint32_t cp;
    while (pos < len) {
        size_t np = pos+16;
        if (np <= len) { /* If it is safe to read 8 more bytes and check that they are ASCII. */
            uint64_t v1, v2;
            memcpy(&v1, buf+pos, sizeof(v1));
            memcpy(&v2, buf+pos+sizeof(v1), sizeof(v2));
            if (!((v1|v2) & UINT64_C(0x8080808080808080))) {
                pos = np;
                continue;
            }
        }
        uint8_t b = buf[pos];
        while (b < 0x80) {
            if (neo_likely(++pos == len)) { *ppos = len; return NEO_UNIERR_OK; }
            b = buf[pos];
        }
        if ((b & 0xe0) == 0xc0) {
            np = pos+2;
            if (neo_unlikely(np > len)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            cp = (b & 0x1fu)<<6u | (buf[pos+1] & 0x3fu);
            if (neo_unlikely((cp < 0x80) || (0x7ff < cp))) { *ppos = pos; return NEO_UNIERR_OVERLONG; }
        } else if ((b & 0xf0) == 0xe0) {
            np = pos+3;
            if (neo_unlikely(np > len)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+2] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            cp = (b & 0xfu)<<12u | (buf[pos+1] & 0x3fu)<<6u | (buf[pos+2] & 0x3fu);
            if (neo_unlikely((cp < 0x800) || (0xffff < cp))) { *ppos = pos; return NEO_UNIERR_OVERLONG; }
            if (neo_unlikely(0xd7ff < cp && cp < 0xe000)) { *ppos = pos; return NEO_UNIERR_SURROGATE; }
        } else if ((b & 0xf8) == 0xf0) {
            np = pos+4;
            if (neo_unlikely(np > len)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+1] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+2] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            if (neo_unlikely((buf[pos+3] & 0xc0) != 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_SHORT; }
            cp = (b & 0x7u)<<18u | (buf[pos+1] & 0x3fu)<<12u | (buf[pos+2] & 0x3fu)<<6u | (buf[pos+3] & 0x3fu);
            if (neo_unlikely(cp <= 0xffff)) { *ppos = pos; return NEO_UNIERR_OVERLONG; }
            if (neo_unlikely(0x10ffff < cp)) { *ppos = pos; return NEO_UNIERR_TOO_LARGE; }
        } else { /* We either have too many continuation bytes or an invalid leading byte. */
            if (neo_unlikely((b & 0xc0) == 0x80)) { *ppos = pos; return NEO_UNIERR_TOO_LONG; }
            else { *ppos = pos; return NEO_UNIERR_HEADER_BITS; }
        }
        pos = np;
    }
    *ppos = len;
    return NEO_UNIERR_OK;
}

bool neo_utf8_is_ascii(const uint8_t *buf, size_t len) {
    neo_dassert(buf);
    for (size_t i = 0; i < len; ++i) {
        if (neo_unlikely(buf[i] > 0x7f)) { return false; }
    }
    return true;
}

uint32_t neo_hash_x17(const void *key, size_t len) {
    uint32_t r = 0x1505;
    const uint8_t *p = (const uint8_t*)key;
    for (size_t i = 0; i < len; ++i) {
        r = 17 * r + (p[i] - ' ');
    }
    return r^(r>>16);
}

uint32_t neo_hash_fnv1a(const void *key, size_t len) {
    size_t blocks = len >> 3;
    uint64_t r = 0x811c9dc5;
    const uint8_t *data = (const uint8_t *)key;
    for (size_t i = 0; i < blocks; ++i) {
        uint64_t tmp;
        memcpy(&tmp, data, sizeof(tmp));
        r ^= tmp;
        r *= 0xbf58476d1ce4e5b9;
        data += 8;
    }
    size_t rest = len & 255;
    switch (len % 8)
    {
        case 7: rest |= (uint64_t)data[6] << 56; NEO_FALLTHROUGH; /* fallthrough */
        case 6: rest |= (uint64_t)data[5] << 48; NEO_FALLTHROUGH; /* fallthrough */
        case 5: rest |= (uint64_t)data[4] << 40; NEO_FALLTHROUGH; /* fallthrough */
        case 4: rest |= (uint64_t)data[3] << 32; NEO_FALLTHROUGH; /* fallthrough */
        case 3: rest |= (uint64_t)data[2] << 24; NEO_FALLTHROUGH; /* fallthrough */
        case 2: rest |= (uint64_t)data[1] << 16; NEO_FALLTHROUGH; /* fallthrough */
        case 1:
            rest |= (uint64_t)data[0] << 8;
            r ^= rest;
            r *= 0xd6e8feb86659fd93;
    }
    return (uint32_t )(r ^ (r >> 32));
}

#define	ROTL32(x, r) ((x << r) | (x >> (32 - r)))
#define FMIX32(h) h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16;

uint64_t neo_hash_mumrmur3_86_128(const void *key, size_t len, uint32_t seed) {
    const uint8_t * data = (const uint8_t*)key;
    int64_t nblocks = (int64_t)len / 16;
    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;
    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;
    const uint32_t * blocks = (const uint32_t *)(data + nblocks*16);
    for (int64_t i = -nblocks; i; ++i) {
        uint32_t k1 = blocks[i*4+0];
        uint32_t k2 = blocks[i*4+1];
        uint32_t k3 = blocks[i*4+2];
        uint32_t k4 = blocks[i*4+3];
        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = ROTL32(h1, 19);
        h1 += h2;
        h1 = h1 * 5 + 0x561ccd1b;
        k2 *= c2;
        k2 = ROTL32(k2, 16);
        k2 *= c3;
        h2 ^= k2;
        h2 = ROTL32(h2, 17);
        h2 += h3;
        h2 = h2 * 5 + 0x0bcaa747;
        k3 *= c3;
        k3 = ROTL32(k3, 17);
        k3 *= c4;
        h3 ^= k3;
        h3 = ROTL32(h3, 15);
        h3 += h4;
        h3 = h3 * 5 + 0x96cd1c35;
        k4 *= c4;
        k4 = ROTL32(k4, 18);
        k4 *= c1;
        h4 ^= k4;
        h4 = ROTL32(h4, 13);
        h4 += h1;
        h4 = h4 * 5 + 0x32ac3b17;
    }
    const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;
    switch(len & 15) {
        case 15: k4 ^= (uint32_t)tail[14] << 16; NEO_FALLTHROUGH;
        case 14: k4 ^= (uint32_t)tail[13] << 8; NEO_FALLTHROUGH;
        case 13: k4 ^= (uint32_t)tail[12] << 0;
            k4 *= c4;
            k4 = ROTL32(k4, 18);
            k4 *= c1;
            h4 ^= k4;
            NEO_FALLTHROUGH;
        case 12: k3 ^= (uint32_t)tail[11] << 24; NEO_FALLTHROUGH;
        case 11: k3 ^= (uint32_t)tail[10] << 16; NEO_FALLTHROUGH;
        case 10: k3 ^= (uint32_t)tail[9] << 8; NEO_FALLTHROUGH;
        case  9: k3 ^= (uint32_t)tail[8] << 0;
            k3 *= c3;
            k3 = ROTL32(k3, 17);
            k3 *= c4;
            h3 ^= k3;
            NEO_FALLTHROUGH;
        case  8: k2 ^= (uint32_t)tail[7] << 24; NEO_FALLTHROUGH;
        case  7: k2 ^= (uint32_t)tail[6] << 16;  NEO_FALLTHROUGH;
        case  6: k2 ^= (uint32_t)tail[5] << 8;  NEO_FALLTHROUGH;
        case  5: k2 ^= (uint32_t)tail[4] << 0;
            k2 *= c2;
            k2 = ROTL32(k2, 16);
            k2 *= c3;
            h2 ^= k2;
            NEO_FALLTHROUGH;
        case  4: k1 ^= (uint32_t)tail[3] << 24;  NEO_FALLTHROUGH;
        case  3: k1 ^= (uint32_t)tail[2] << 16; NEO_FALLTHROUGH;
        case  2: k1 ^= (uint32_t)tail[1] << 8;  NEO_FALLTHROUGH;
        case  1: k1 ^= (uint32_t)tail[0] << 0;
            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };
    h1 ^= (uint32_t)len;
    h2 ^= (uint32_t)len;
    h3 ^= (uint32_t)len;
    h4 ^= (uint32_t)len;
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;
    FMIX32(h1);
    FMIX32(h2);
    FMIX32(h3);
    FMIX32(h4);
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;
    return (((uint64_t)h2) << 32) | h1;
}

#define u8load64_le(p) \
    {  (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) | \
        ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) | \
        ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) | \
        ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56)) }
#define u64split8_le(p, v) \
    { u8load32_le((p), (uint32_t)((v))); \
      u8load32_le((p) + 4, (uint32_t)((v) >> 32)); }
#define u8load32_le(p, v) \
    { (p)[0] = (uint8_t)((v)); \
      (p)[1] = (uint8_t)((v) >> 8); \
      (p)[2] = (uint8_t)((v) >> 16); \
      (p)[3] = (uint8_t)((v) >> 24); }
#define rol64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define stipround() \
    { v0 += v1; v1 = rol64(v1, 13); \
      v1 ^= v0; v0 = rol64(v0, 32); \
      v2 += v3; v3 = rol64(v3, 16); \
      v3 ^= v2; \
      v0 += v3; v3 = rol64(v3, 21); \
      v3 ^= v0; \
      v2 += v1; v1 = rol64(v1, 17); \
      v1 ^= v2; v2 = rol64(v2, 32); }

uint64_t neo_hash_sip64(const void *key, size_t len, uint64_t seed0, uint64_t seed1) {
    const uint8_t *in = (const uint8_t *)key;
    uint64_t k0 = u8load64_le((uint8_t *)&seed0);
    uint64_t k1 = u8load64_le((uint8_t *)&seed1);
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ k1;
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    const uint8_t *end = in + len - (len % sizeof(uint64_t));
    for (; in != end; in += 8) {
        uint64_t m = u8load64_le(in);
        v3 ^= m;
        stipround();
        stipround();
        v0 ^= m;
    }
    uint64_t b = ((uint64_t)len) << 56;
    switch (len & 7) {
        case 7: b |= ((uint64_t)in[6]) << 48; NEO_FALLTHROUGH;
        case 6: b |= ((uint64_t)in[5]) << 40; NEO_FALLTHROUGH;
        case 5: b |= ((uint64_t)in[4]) << 32; NEO_FALLTHROUGH;
        case 4: b |= ((uint64_t)in[3]) << 24; NEO_FALLTHROUGH;
        case 3: b |= ((uint64_t)in[2]) << 16; NEO_FALLTHROUGH;
        case 2: b |= ((uint64_t)in[1]) << 8;  NEO_FALLTHROUGH;
        case 1: b |= ((uint64_t)in[0]); break;
        case 0: break;
    }
    v3 ^= b;
    stipround();
    stipround();
    v0 ^= b;
    v2 ^= 0xff;
    stipround();
    stipround();
    stipround();
    stipround();
    b = v0 ^ v1 ^ v2 ^ v3;
    uint64_t out = 0;
    u64split8_le((uint8_t*)&out, b);
    return out;
}

#undef stipround
#undef rol64
#undef u8load32_le
#undef u64split8_le
#undef u8load64_le

neo_static_assert(sizeof(char) == sizeof(uint8_t));
uint8_t *neo_strdup2(const uint8_t *str) {
   return (uint8_t *)neo_strdup((const char *)str);
}

char *neo_strdup(const char *str) {
    neo_assert(str && "Invalid ptr to clone");
    size_t len = strlen(str); /* strlen also works with UTF-8 strings to find the end \0. */
    char *dup = neo_memalloc(NULL, (len+1)*sizeof(*dup));
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

void neo_printutf8(FILE *f, const uint8_t *str) {
    neo_assert(f != NULL && "Invalid file ptr");
    if (neo_unlikely(!str || !*str)) { return; }
#if NEO_OS_LINUX
    fputs((const char *)str, f); /* Linux terminal is UTF-8 by default. */
#else
#  error "unsupported platform"
#endif
}

/*
** String Scanning which
** replaces the standard C library's strtod() family functions.
** Slightly modified LuaJIT code which is:
** Copyright (C) 2005-2023 Mike Pall.
** For full copyright information see LICENSE.
*/

/*
** Rationale for the builtin string to number conversion library:
**
** It removes a dependency on libc's strtod(), which is a true portability
** nightmare. Mainly due to the plethora of supported OS and toolchain
** combinations. Sadly, the various implementations
** a) are often buggy, incomplete (no hex floats) and/or imprecise,
** b) sometimes crash or hang on certain inputs,
** c) return non-standard NaNs that need to be filtered out, and
** d) fail if the locale-specific decimal separator is not a dot,
**    which can only be fixed with atrocious workarounds.
**
** Also, most of the strtod() implementations are hopelessly bloated,
** which is not just an I-cache hog, but a problem for static linkage
** on embedded systems, too.
**
** OTOH the builtin conversion function is very compact. Even though it
** does a lot more, like parsing long longs, octal or imaginary numbers
** and returning the result in different formats:
** a) It needs less than 3 KB (!) of machine code (on x64 with -Os),
** b) it doesn't perform any dynamic allocation and,
** c) it needs only around 600 bytes of stack space.
**
** The builtin function is faster than strtod() for typical inputs, e.g.
** "123", "1.5" or "1e6". Arguably, it's slower for very large exponents,
** which are not very common (this could be fixed, if needed).
**
** And most importantly, the builtin function is equally precise on all
** platforms. It correctly converts and rounds any input to a double.
** If this is not the case, please send a bug report -- but PLEASE verify
** that the implementation you're comparing to is not the culprit!
**
** The implementation quickly pre-scans the entire string first and
** handles simple integers on-the-fly. Otherwise, it dispatches to the
** base-specific parser. Hex and octal is straightforward.
**
** Decimal to binary conversion uses a fixed-length circular buffer in
** base 100. Some simple cases are handled directly. For other cases, the
** number in the buffer is up-scaled or down-scaled until the integer part
** is in the proper range. Then the integer part is rounded and converted
** to a double which is finally rescaled to the result. Denormals need
** special treatment to prevent incorrect 'double rounding'.
*/

#define CHAR_CNTRL	0x01
#define CHAR_SPACE	0x02
#define CHAR_PUNCT	0x04
#define CHAR_DIGIT	0x08
#define CHAR_XDIGIT	0x10
#define CHAR_UPPER	0x20
#define CHAR_LOWER	0x40
#define CHAR_IDENT	0x80
#define CHAR_ALPHA	(CHAR_LOWER|CHAR_UPPER)
#define CHAR_ALNUM	(CHAR_ALPHA|CHAR_DIGIT)
#define CHAR_GRAPH	(CHAR_ALNUM|CHAR_PUNCT)

/* Only pass -1 or 0..255 to these macros. Never pass a signed char! */
#define char_isa(c, t)	((char_bits+1)[(c)] & t)
#define char_iscntrl(c)	char_isa((c), CHAR_CNTRL)
#define char_isspace(c)	char_isa((c), CHAR_SPACE)
#define char_ispunct(c)	char_isa((c), CHAR_PUNCT)
#define char_isdigit(c)	char_isa((c), CHAR_DIGIT)
#define char_isxdigit(c)	char_isa((c), CHAR_XDIGIT)
#define char_isupper(c)	char_isa((c), CHAR_UPPER)
#define char_islower(c)	char_isa((c), CHAR_LOWER)
#define char_isident(c)	char_isa((c), CHAR_IDENT)
#define char_isalpha(c)	char_isa((c), CHAR_ALPHA)
#define char_isalnum(c)	char_isa((c), CHAR_ALNUM)
#define char_isgraph(c)	char_isa((c), CHAR_GRAPH)

#define char_toupper(c)	((c) - (char_islower(c) >> 1))
#define char_tolower(c)	((c) + char_isupper(c))

static const uint8_t char_bits[257] = {
    0,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    2,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    152,152,152,152,152,152,152,152,152,152,  4,  4,  4,  4,  4,  4,
    4,176,176,176,176,176,176,160,160,160,160,160,160,160,160,160,
    160,160,160,160,160,160,160,160,160,160,160,  4,  4,  4,  4,132,
    4,208,208,208,208,208,208,192,192,192,192,192,192,192,192,192,
    192,192,192,192,192,192,192,192,192,192,192,  4,  4,  4,  4,  1,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};


#define setnanV(o) ((o)->ru64 = 0xfff8000000000000ull)
#define setpinfV(o)	((o)->ru64 = 0x7ff0000000000000ull)
#define setminfV(o)	((o)->ru64 = 0xfff0000000000000ull)

/* Definitions for circular decimal digit buffer (base 100 = 2 digits/byte). */
#define NEO_STRSCAN_DIG	1024
#define NEO_STRSCAN_MAXDIG	800	 /* 772 + extra are sufficient. */
#define NEO_STRSCAN_DDIG (NEO_STRSCAN_DIG/2)
#define NEO_STRSCAN_DMASK (NEO_STRSCAN_DDIG-1)
#define NEO_STRSCAN_MAXEXP	(1<<20)

/* Helpers for circular buffer. */
#define dnext(a) (((a)+1) & NEO_STRSCAN_DMASK)
#define dprev(a) (((a)-1) & NEO_STRSCAN_DMASK)
#define dlen(lo, hi) ((int32_t)(((lo)-(hi)) & NEO_STRSCAN_DMASK))
#define casecmp(c, k) (((c) | 0x20) == k)

/* Final conversion to double. */
static void strscan_double(uint64_t x, record_t *o, int32_t ex2, int32_t neg) {
    neo_dassert(o != NULL);
    double n;
    /* Avoid double rounding for denormals. */
    if (neo_unlikely(ex2 <= -1075 && x != 0)) {
#if NEO_COM_GCC ^ NEO_COM_CLANG
        int32_t b = (int32_t)(__builtin_clzll(x)^63);
#else
        int32_t b = (x>>32)
                    ? 32+(int32_t)neo_bsr32((uint32_t)(x>>32)) :
                    (int32_t)fls((uint32_t)x);
#endif
        if ((int32_t)b + ex2 <= -1023 && (int32_t)b + ex2 >= -1075) {
            uint64_t rb = (uint64_t)1 << (-1075-ex2);
            if ((x & rb) && ((x & (rb+rb+rb-1)))) x += rb+rb;
            x = (x & ~(rb+rb-1));
        }
    }
    /* Convert to double using a signed int64_t conversion, then rescale. */
    neo_assert((int64_t)x >= 0 && "bad double conversion");
    n = (double)(int64_t)x;
    if (neg) { n = -n; }
    if (ex2) { n = ldexp(n, ex2); }
    o->as_float = n;
}

/* Parse hexadecimal number. */
static neo_strscan_format_t strscan_hex(
    const uint8_t *p,
    record_t *o,
    neo_strscan_format_t fmt,
    uint32_t opt,
    int32_t ex2,
    int32_t neg,
    uint32_t dig
) {
    neo_dassert(p != NULL && o != NULL);
    uint64_t x = 0;
    uint32_t i;
    for (i = dig > 16 ? 16 : dig ; i; i--, p++) {     /* Scan hex digits. */
        uint32_t d = (*p != '.' ? *p : *++p); if (d > '9') d += 9;
        x = (x << 4) + (d & 15);
    }
    for (i = 16; i < dig; i++, p++) { /* Summarize rounding-effect of excess digits. */
        x |= ((*p != '.' ? *p : *++p) != '0'), ex2 += 4;
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (!(opt & NEO_STRSCAN_OPT_TONUM)
                && x < 0x80000000u+(uint32_t)neg
                && !(x == 0 && neg)) {
                o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
                return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
            }
            if (!(opt & NEO_STRSCAN_OPT_C)) { fmt = NEO_STRSCAN_NUM; break; }
            NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if (dig > 8) { return NEO_STRSCAN_ERROR; }
            o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
            return NEO_STRSCAN_U32;
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            if (dig > 16) { return NEO_STRSCAN_ERROR; }
            o->ru64 = neg ? ~x+1u : x;
            return fmt;
        default:
            break;
    }
    /* Reduce range, then convert to double. */
    if ((x & 0xc00000000000000ull)) { x = (x >> 2) | (x & 3); ex2 += 2; }
    strscan_double(x, o, ex2, neg);
    return fmt;
}

/* Parse octal number. */
static neo_strscan_format_t strscan_oct(
    const uint8_t *p,
    record_t *o,
    neo_strscan_format_t fmt,
    int32_t neg,
    uint32_t dig
) {
    neo_dassert(p != NULL && o != NULL);
    uint64_t x = 0;
    if (dig > 22 || (dig == 22 && *p > '1')) { return NEO_STRSCAN_ERROR; }
    while (dig-- > 0) {     /* Scan octal digits. */
        if (!(*p >= '0' && *p <= '7')) { return NEO_STRSCAN_ERROR; }
        x = (x << 3) + (*p++ & 7);
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (x >= 0x80000000u+(uint32_t)neg) {
                fmt = NEO_STRSCAN_U32;
            } NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if ((x >> 32)) { return NEO_STRSCAN_ERROR; }
            o->ri32 = neg ? (int32_t)(~(uint32_t)x+1u) : (int32_t)x;
            break;
        default:
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            o->ru64 = neg ? ~x+1u : x;
            break;
    }
    return fmt;
}

/* Parse decimal number. */
static neo_strscan_format_t strscan_dec(
    const uint8_t *p,
    record_t *o,
    neo_strscan_format_t fmt,
    uint32_t opt,
    int32_t ex10,
    int32_t neg,
    uint32_t dig
) {
    neo_dassert(p != NULL && o != NULL);
    uint8_t xi[NEO_STRSCAN_DDIG], *xip = xi;
    if (dig) {
        uint32_t i = dig;
        if (i > NEO_STRSCAN_MAXDIG) {
            ex10 += (int32_t)(i - NEO_STRSCAN_MAXDIG);
            i = NEO_STRSCAN_MAXDIG;
        }
        if ((((uint32_t)ex10^i) & 1)) { /* Scan unaligned leading digit. */
            *xip++ = ((*p != '.' ? *p : *++p) & 15), i--, p++;
        }
        for ( ; i > 1; i -= 2) { /* Scan aligned double-digits. */
            uint32_t d = 10 * ((*p != '.' ? *p : *++p) & 15); p++;
            *xip++ = (uint8_t)(d + ((*p != '.' ? *p : *++p) & 15)); p++;
        }
        /* Scan and realign trailing digit. */
        if (i) { *xip++ = 10 * ((*p != '.' ? *p : *++p) & 15), ex10--, dig++, p++; }
        if (dig > NEO_STRSCAN_MAXDIG) { /* Summarize rounding-effect of excess digits. */
            do {
                if ((*p != '.' ? *p : *++p) != '0') {
                    xip[-1] |= 1;
                    break;
                }
                ++p;
            } while (--dig > NEO_STRSCAN_MAXDIG);
            dig = NEO_STRSCAN_MAXDIG;
        } else {  /* Simplify exponent. */
            while (ex10 > 0 && dig <= 18) {
                *xip++ = 0;
                ex10 -= 2;
                dig += 2;
            }
        }
    } else {  /* Only got zeros. */
        ex10 = 0;
        xi[0] = 0;
    }
    if (dig <= 20 && ex10 == 0) {  /* Fast path for numbers in integer format (but handles e.g. 1e6, too). */
        uint8_t *xis;
        uint64_t x = xi[0];
        double n;
        for (xis = xi+1; xis < xip; xis++) x = x * 100 + *xis;
        if (!(dig == 20 && (xi[0] > 18 || (int64_t)x >= 0))) {  /* No overflow? */
            /* Format-specific handling. */
            switch (fmt) {
                case NEO_STRSCAN_INT:
                    if (!(opt & NEO_STRSCAN_OPT_TONUM) && x < 0x80000000u+(uint32_t)neg) {
                        o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
                        return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
                    }
                    if (!(opt & NEO_STRSCAN_OPT_C)) { fmt = NEO_STRSCAN_NUM; goto plainnumber; }
                    NEO_FALLTHROUGH;
                case NEO_STRSCAN_U32:
                    if ((x >> 32) != 0) { return NEO_STRSCAN_ERROR; }
                    o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
                    return NEO_STRSCAN_U32;
                case NEO_STRSCAN_I64:
                case NEO_STRSCAN_U64:
                    o->ru64 = neg ? ~x+1u : x;
                    return fmt;
                default:
                plainnumber:  /* Fast path for plain numbers < 2^63. */
                    if ((int64_t)x < 0) { break; }
                    n = (double)(int64_t)x;
                    if (neg) n = -n;
                    o->as_float = n;
                    return fmt;
            }
        }
    }

    if (fmt == NEO_STRSCAN_INT) { /* Slow non-integer path. */
        if ((opt & NEO_STRSCAN_OPT_C)) return NEO_STRSCAN_ERROR;
        fmt = NEO_STRSCAN_NUM;
    } else if (fmt > NEO_STRSCAN_INT) {
        return NEO_STRSCAN_ERROR;
    }
    {
        uint32_t hi = 0, lo = (uint32_t)(xip-xi);
        int32_t ex2 = 0, idig = (int32_t)lo + (ex10 >> 1);
        neo_assert(lo > 0 && (ex10 & 1) == 0 && "bad lo ex10");
        /* Handle simple overflow/underflow. */
        if (idig > 310/2) { if (neg) { setminfV(o); } else { setpinfV(o); } return fmt; }
        else if (idig < -326/2) { o->as_float = neg ? -0.0 : 0.0; return fmt; }
        while (idig < 9 && idig < dlen(lo, hi)) { /* Scale up until we have at least 17 or 18 integer part digits. */
            uint32_t i, cy = 0;
            ex2 -= 6;
            for (i = dprev(lo); ; i = dprev(i)) {
                uint32_t d = (uint32_t)(xi[i] << 6) + cy;
                cy = (((d >> 2) * 5243) >> 17);
                d = d - cy * 100;  /* Div/mod 100. */
                xi[i] = (uint8_t)d;
                if (i == hi) { break; }
                if (d == 0 && i == dprev(lo)){  lo = i; }
            }
            if (cy) {
                hi = dprev(hi);
                if (xi[dprev(lo)] == 0) { lo = dprev(lo); }
                else if (hi == lo) { lo = dprev(lo); xi[dprev(lo)] |= xi[lo]; }
                xi[hi] = (uint8_t)cy;
                ++idig;
            }
        }
        while (idig > 9) { /* Scale down until no more than 17 or 18 integer part digits remain. */
            uint32_t i = hi, cy = 0;
            ex2 += 6;
            do {
                cy += xi[i];
                xi[i] = (cy >> 6) & 255;
                cy = 100 * (cy & 0x3f);
                if (xi[i] == 0 && i == hi) { hi = dnext(hi), idig--; }
                i = dnext(i);
            } while (i != lo);
            while (cy) {
                if (hi == lo) { xi[dprev(lo)] |= 1; break; }
                xi[lo] = (cy >> 6) & 255;
                lo = dnext(lo);
                cy = 100 * (cy & 0x3f);
            }
        }
        { /* Collect integer part digits and convert to rescaled double. */
            uint64_t x = xi[hi];
            uint32_t i;
            for (i = dnext(hi); --idig > 0 && i != lo; i = dnext(i)) {
                x = x * 100 + xi[i];
            }
            if (i == lo) {
                while (--idig >= 0) {
                    x = x * 100;
                }
            } else {  /* Gather round bit from remaining digits. */
                x <<= 1; --ex2;
                do {
                    if (xi[i]) {
                        x |= 1;
                        break;
                    }
                    i = dnext(i);
                } while (i != lo);
            }
            strscan_double(x, o, ex2, neg);
        }
    }
    return fmt;
}

/* Parse binary number. */
static neo_strscan_format_t strscan_bin(
    const uint8_t *p,
    record_t *o,
    neo_strscan_format_t fmt,
    uint32_t opt,
    int32_t ex2,
    int32_t neg,
    uint32_t dig
) {
    neo_dassert(p != NULL && o != NULL);
    uint64_t x = 0;
    uint32_t i;
    if (ex2 || dig > 64) { return NEO_STRSCAN_ERROR; }
    for (i = dig; i; i--, p++) {     /* Scan binary digits. */
        if ((*p & ~1) != '0') { return NEO_STRSCAN_ERROR; }
        x = (x << 1) | (*p & 1);
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (!(opt & NEO_STRSCAN_OPT_TONUM) && x < 0x80000000u+(uint32_t)neg) {
                o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
                return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
            }
            if (!(opt & NEO_STRSCAN_OPT_C)) { fmt = NEO_STRSCAN_NUM; break; }
            NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if (dig > 32) return NEO_STRSCAN_ERROR;
            o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
            return NEO_STRSCAN_U32;
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            o->ru64 = neg ? ~x+1u : x;
            return fmt;
        default:
            break;
    }
    /* Reduce range, then convert to double. */
    if ((x & 0xc00000000000000ull)) { x = (x >> 2) | (x & 3); ex2 += 2; }
    strscan_double(x, o, ex2, neg);
    return fmt;
}

/* Scan string containing a number. Returns format. Returns value in o. */
neo_strscan_format_t neo_strscan_scan(
    const uint8_t *p,
    size_t len,
    record_t *o,
    neo_strscan_opt_t opt
) {
    neo_dassert(p != NULL && o != NULL);
    if (!len || !*p) { o->ru64 = 0; return NEO_STRSCAN_EMPTY; }
    int32_t neg = 0;
    const uint8_t *pe = p + len;
    /* Remove leading space, parse sign and non-numbers. */
    if (neo_unlikely(!char_isdigit(*p))) {
        while (char_isspace(*p)) { p++; }
        if (*p == '+' || *p == '-') { neg = (*p++ == '-'); }
        if (neo_unlikely(*p >= 'A')) {  /* Parse "inf", "infinity" or "nan". */
            record_t tmp;
            setnanV(&tmp);
            if (casecmp(p[0],'i') && casecmp(p[1],'n') && casecmp(p[2],'f')) {
                if (neg) {
                    setminfV(&tmp);
                } else {
                    setpinfV(&tmp);
                }
                p += 3;
                if (casecmp(p[0],'i') && casecmp(p[1],'n') && casecmp(p[2],'i') &&
                    casecmp(p[3],'t') && casecmp(p[4],'y')) { p += 5; }
            } else if (casecmp(p[0],'n') && casecmp(p[1],'a') && casecmp(p[2],'n')) {
                p += 3;
            }
            while (char_isspace(*p)) { ++p; }
            if (*p || p < pe) { return NEO_STRSCAN_ERROR; }
            o->ru64 = tmp.ru64;
            return NEO_STRSCAN_NUM;
        }
    }

    /* Parse regular number. */
    {
        neo_strscan_format_t fmt = NEO_STRSCAN_INT;
        int cmask = CHAR_DIGIT;
        int base = (opt & NEO_STRSCAN_OPT_C) && *p == '0' ? 0 : 10;
        const uint8_t *sp, *dp = NULL;
        uint32_t dig = 0, hasdig = 0, x = 0;
        int32_t ex = 0;
        if (neo_unlikely(*p <= '0')) { /* Determine base and skip leading zeros. */
            if (*p == '0') {
                if (casecmp(p[1], 'x')) { /* Hex */
                    base = 16, cmask = CHAR_XDIGIT, p += 2;
                } else if (casecmp(p[1], 'b')) { /* Bin */
                    base = 2, cmask = CHAR_DIGIT, p += 2;
                } else if (casecmp(p[1], 'c')) { /* Oct */
                    base = 0, p += 2;
                }
            }
            for ( ; ; p++) {
                if (*p == '0') {
                    hasdig = 1;
                } else if (*p == '.') {
                    if (dp) return NEO_STRSCAN_ERROR;
                    dp = p;
                } else {
                    break;
                }
            }
        }
        for (sp = p; ; p++) { /* Preliminary digit and decimal point scan. */
            if (neo_likely(char_isa(*p, cmask))) {
                x = x * 10 + (*p & 15);  /* For fast path below. */
                ++dig;
            } else if (*p == '.') {
                if (dp) { return NEO_STRSCAN_ERROR; }
                dp = p;
            } else {
                break;
            }
        }
        if (!(hasdig | dig)) { return NEO_STRSCAN_ERROR; }
        if (dp) { /* Handle decimal point. */
            if (base == 2) { return NEO_STRSCAN_ERROR; }
            fmt = NEO_STRSCAN_NUM;
            if (dig) {
                ex = (int32_t)(dp-(p-1)); dp = p-1;
                while (ex < 0 && *dp-- == '0') { ++ex, --dig; }  /* Skip trailing zeros. */
                if (ex <= -NEO_STRSCAN_MAXEXP) { return NEO_STRSCAN_ERROR; }
                if (base == 16) { ex *= 4; }
            }
        }
        if (base >= 10 && casecmp(*p, (uint32_t)(base == 16 ? 'p' : 'e'))) {  /* Parse exponent. */
            uint32_t xx;
            int negx = 0;
            fmt = NEO_STRSCAN_NUM; p++;
            if (*p == '+' || *p == '-') { negx = (*p++ == '-'); }
            if (!char_isdigit(*p)) { return NEO_STRSCAN_ERROR; }
            xx = (*p++ & 15);
            while (char_isdigit(*p)) {
                xx = xx * 10 + (*p & 15);
                if (xx >= NEO_STRSCAN_MAXEXP) { return NEO_STRSCAN_ERROR; }
                p++;
            }
            ex += negx ? (int32_t)(~xx+1u) : (int32_t)xx;
        }
        /* Parse suffix. */
        if (*p) {
            /* I (IMAG), U (U32), LL (I64), ULL/LLU (U64), L (long), UL/LU (ulong). */
            /* NYI: f (float). Not needed until cp_number() handles non-integers. */
            if (casecmp(*p, 'i')) {
                if (!(opt & NEO_STRSCAN_OPT_IMAG)) { return NEO_STRSCAN_ERROR; }
                ++p; fmt = NEO_STRSCAN_IMAG;
            } else if (fmt == NEO_STRSCAN_INT) {
                if (casecmp(*p, 'u')) { p++, fmt = NEO_STRSCAN_U32; }
                if (casecmp(*p, 'l')) {
                    ++p;
                    if (casecmp(*p, 'l')) { p++, fmt += NEO_STRSCAN_I64 - NEO_STRSCAN_INT; }
                    else if (!(opt & NEO_STRSCAN_OPT_C)) {  return NEO_STRSCAN_ERROR; }
                    else if (sizeof(long) == 8) { fmt += NEO_STRSCAN_I64 - NEO_STRSCAN_INT; }
                }
                if (casecmp(*p, 'u') && (fmt == NEO_STRSCAN_INT || fmt == NEO_STRSCAN_I64)) {
                    ++p, fmt += NEO_STRSCAN_U32 - NEO_STRSCAN_INT;
                }
                if ((fmt == NEO_STRSCAN_U32 && !(opt & NEO_STRSCAN_OPT_C)) ||
                    (fmt >= NEO_STRSCAN_I64 && !(opt & NEO_STRSCAN_OPT_LL))) {
                    return NEO_STRSCAN_ERROR;
                }
            }
            while (char_isspace(*p)) { ++p; }
            if (*p && p < pe) { return NEO_STRSCAN_ERROR; }
        }
        if (p < pe) { return NEO_STRSCAN_ERROR; }
        if (fmt == NEO_STRSCAN_INT && base == 10 && /* Fast path for decimal 32-bit integers. */
            (dig < 10 || (dig == 10 && *sp <= '2' && x < 0x80000000u+(uint32_t)neg))) {
            if ((opt & NEO_STRSCAN_OPT_TONUM)) {
                o->as_float = neg ? -(double)x : (double)x;
                return NEO_STRSCAN_NUM;
            } else if (x == 0 && neg) {
                o->as_float = -0.0;
                return NEO_STRSCAN_NUM;
            } else {
                o->ri32 = neg ? (int32_t)(~x+1u) : (int32_t)x;
                return NEO_STRSCAN_INT;
            }
        }
        if (base == 0 && !(fmt == NEO_STRSCAN_NUM || fmt == NEO_STRSCAN_IMAG)) { /* Dispatch to base-specific parser. */
            return strscan_oct(sp, o, fmt, neg, dig);
        } else if (base == 16) {
            fmt = strscan_hex(sp, o, fmt, opt, ex, neg, dig);
        } else if (base == 2) {
            fmt = strscan_bin(sp, o, fmt, opt, ex, neg, dig);
        } else {
            fmt = strscan_dec(sp, o, fmt, opt, ex, neg, dig);
        }
        /* Try to convert number to integer, if requested. */
        if (fmt == NEO_STRSCAN_NUM && (opt & NEO_STRSCAN_OPT_TOINT) && o->ru64 != 0x8000000000000000ull) {
            double n = o->as_float;
            int32_t i = (int32_t)(n);
            if (n == (double)i) { o->ri32 = i; return NEO_STRSCAN_INT; }
        }
        return fmt;
    }
}
