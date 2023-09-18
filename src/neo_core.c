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
                    (int32_t)neo_bsr32((uint32_t)x);
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
        if (idig > 310/2) {
            if (neg) { rec_setminf(*o); }
            else { rec_setpinf(*o); }
            return fmt;
        }
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
            rec_setnan(tmp);
            if (casecmp(p[0],'i') && casecmp(p[1],'n') && casecmp(p[2],'f')) {
                if (neg) {
                    rec_setminf(tmp);
                } else {
                    rec_setpinf(tmp);
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

/* Rescale factors to push the exponent of a number towards zero. */
#define RESCALE_EXPONENTS(P, N) \
  P(308), P(289), P(270), P(250), P(231), P(212), P(193), P(173), P(154), \
  P(135), P(115), P(96), P(77), P(58), P(38), P(0), P(0), P(0), N(39), N(58), \
  N(77), N(96), N(116), N(135), N(154), N(174), N(193), N(212), N(231), \
  N(251), N(270), N(289)

#define ONE_E_P(X) 1e+0 ## X
#define ONE_E_N(X) 1e-0 ## X
static const int16_t rescale_e[] = { RESCALE_EXPONENTS(-, +) };
static const double rescale_n[] = { RESCALE_EXPONENTS(ONE_E_P, ONE_E_N) };
#undef ONE_E_N
#undef ONE_E_P
#undef RESCALE_EXPONENTS

/*
** For p in range -70 through 57, this table encodes pairs (m, e) such that
** 4*2^p <= (uint8_t)m*10^e, and is the smallest value for which this holds.
*/
static const int8_t four_ulp_m_e[] = {
    34, -21, 68, -21, 14, -20, 28, -20, 55, -20, 2, -19, 3, -19, 5, -19, 9, -19,
    -82, -18, 35, -18, 7, -17, -117, -17, 28, -17, 56, -17, 112, -16, -33, -16,
    45, -16, 89, -16, -78, -15, 36, -15, 72, -15, -113, -14, 29, -14, 57, -14,
    114, -13, -28, -13, 46, -13, 91, -12, -74, -12, 37, -12, 73, -12, 15, -11, 3,
    -11, 59, -11, 2, -10, 3, -10, 5, -10, 1, -9, -69, -9, 38, -9, 75, -9, 15, -7,
    3, -7, 6, -7, 12, -6, -17, -7, 48, -7, 96, -7, -65, -6, 39, -6, 77, -6, -103,
    -5, 31, -5, 62, -5, 123, -4, -11, -4, 49, -4, 98, -4, -60, -3, 4, -2, 79, -3,
    16, -2, 32, -2, 63, -2, 2, -1, 25, 0, 5, 1, 1, 2, 2, 2, 4, 2, 8, 2, 16, 2,
    32, 2, 64, 2, -128, 2, 26, 2, 52, 2, 103, 3, -51, 3, 41, 4, 82, 4, -92, 4,
    33, 4, 66, 4, -124, 5, 27, 5, 53, 5, 105, 6, 21, 6, 42, 6, 84, 6, 17, 7, 34,
    7, 68, 7, 2, 8, 3, 8, 6, 8, 108, 9, -41, 9, 43, 10, 86, 9, -84, 10, 35, 10,
    69, 10, -118, 11, 28, 11, 55, 12, 11, 13, 22, 13, 44, 13, 88, 13, -80, 13,
    36, 13, 71, 13, -115, 14, 29, 14, 57, 14, 113, 15, -30, 15, 46, 15, 91, 15,
    19, 16, 37, 16, 73, 16, 2, 17, 3, 17, 6, 17
};

/* min(2^32-1, 10^e-1) for e in range 0 through 10 */
static uint32_t ndigits_dec_threshold[] = {
    0, 9U, 99U, 999U, 9999U, 99999U, 999999U,
    9999999U, 99999999U, 999999999U, 0xffffffffU
};

#define wint_r(x, sh, sc) { uint32_t d = (x*(((1<<sh)+sc-1)/sc))>>sh; x -= d*sc; *p++ = (uint8_t)('0'+d); }
static uint8_t *fmt_i32(uint8_t *p, int32_t k) { /* Write integer to buffer. */
    neo_dassert(p != NULL);
    uint32_t u = (uint32_t)k;
    if (k < 0) { u = ~u+1u; *p++ = '-'; }
    if (u < 10000) {
        if (u < 10) { goto dig1; }
        if (u < 100) { goto dig2; }
        if (u < 1000) { goto dig3; }
    } else {
        uint32_t v = u / 10000; u -= v * 10000;
        if (v < 10000) {
            if (v < 10) { goto dig5; }
            if (v < 100) { goto dig6; }
            if (v < 1000) { goto dig7; }
        } else {
            uint32_t w = v / 10000; v -= w * 10000;
            if (w >= 10) { wint_r(w, 10, 10) }
            *p++ = (uint8_t)('0'+w);
        }
        wint_r(v, 23, 1000)
        dig7: wint_r(v, 12, 100)
        dig6: wint_r(v, 10, 10)
        dig5: *p++ = (uint8_t)('0'+v);
    }
    wint_r(u, 23, 1000)
    dig3: wint_r(u, 12, 100)
    dig2: wint_r(u, 10, 10)
    dig1: *p++ = (uint8_t)('0'+u);
    return p;
}
#undef wint_r

uint8_t *neo_fmt_int(uint8_t *p, neo_int_t x) {
    neo_dassert(p != NULL);
    p = fmt_i32(p, (int32_t) (x & 0xffffffffll));
    return fmt_i32(p, (int32_t) (x >> 32));
}

/* Format types (max. 16). */
typedef enum fmt_type_t {
    STRFMT_EOF, STRFMT_ERR, STRFMT_LIT,
    STRFMT_INT, STRFMT_UINT, STRFMT_NUM, STRFMT_STR, STRFMT_CHAR, STRFMT_PTR
} fmt_type_t;

typedef uint32_t sfmt_t;

/* Compute the number of digits in the decimal representation of x. */
static size_t ndigits_dec(uint32_t x) {
    size_t t = (size_t)((neo_bsr32(x | 1) * 77) >> 8) + 1; /* 2^8/77 is roughly log2(10) */
    return t + (x > ndigits_dec_threshold[t]);
}

/* Format subtypes (bits are reused). */
#define STRFMT_T_HEX	0x0010	/* STRFMT_UINT */
#define STRFMT_T_OCT	0x0020	/* STRFMT_UINT */
#define STRFMT_T_FP_A	0x0000	/* STRFMT_NUM */
#define STRFMT_T_FP_E	0x0010	/* STRFMT_NUM */
#define STRFMT_T_FP_F	0x0020	/* STRFMT_NUM */
#define STRFMT_T_FP_G	0x0030	/* STRFMT_NUM */
#define STRFMT_T_QUOTED	0x0010	/* STRFMT_STR */

/* Format flags. */
#define STRFMT_F_LEFT	0x0100
#define STRFMT_F_PLUS	0x0200
#define STRFMT_F_ZERO	0x0400
#define STRFMT_F_SPACE	0x0800
#define STRFMT_F_ALT	0x1000
#define STRFMT_F_UPPER	0x2000

/* Format indicator fields. */
#define STRFMT_SH_WIDTH	16
#define STRFMT_SH_PREC	24

#define STRFMT_TYPE(sf) ((FormatType)((sf) & 15))
#define STRFMT_WIDTH(sf) (((sf) >> STRFMT_SH_WIDTH) & 255u)
#define STRFMT_PREC(sf)	((((sf) >> STRFMT_SH_PREC) & 255u) - 1u)
#define STRFMT_FP(sf) (((sf) >> 4) & 3)

/* Formats for conversion characters. */
#define STRFMT_A	(STRFMT_NUM|STRFMT_T_FP_A)
#define STRFMT_C	(STRFMT_CHAR)
#define STRFMT_D	(STRFMT_INT)
#define STRFMT_E	(STRFMT_NUM|STRFMT_T_FP_E)
#define STRFMT_F	(STRFMT_NUM|STRFMT_T_FP_F)
#define STRFMT_G	(STRFMT_NUM|STRFMT_T_FP_G)
#define STRFMT_I	STRFMT_D
#define STRFMT_O	(STRFMT_UINT|STRFMT_T_OCT)
#define STRFMT_P	(STRFMT_PTR)
#define STRFMT_Q	(STRFMT_STR|STRFMT_T_QUOTED)
#define STRFMT_S	(STRFMT_STR)
#define STRFMT_U	(STRFMT_UINT)
#define STRFMT_X	(STRFMT_UINT|STRFMT_T_HEX)
#define STRFMT_G14	(STRFMT_G | ((14+1) << STRFMT_SH_PREC))

#define ND_MUL2K_MAX_SHIFT 29
#define ND_MUL2K_DIV1E9(val) ((uint32_t)((val)/1000000000))

/* Multiply nd by 2^k and add carry_in (ndlo is assumed to be zero). */
static uint32_t nd_mul2k(uint32_t* nd, uint32_t ndhi, uint32_t k, uint32_t carry_in, sfmt_t sf) {
    neo_dassert(nd != NULL);
    uint32_t i, ndlo = 0, start = 1;
    /* Performance hacks. */
    if (k > ND_MUL2K_MAX_SHIFT*2 && STRFMT_FP(sf) != STRFMT_FP(STRFMT_T_FP_F)) {
        start = ndhi - (STRFMT_PREC(sf) + 17) / 8;
    }
    /* Real logic. */
    while (k >= ND_MUL2K_MAX_SHIFT) {
        for (i = ndlo; i <= ndhi; i++) {
            uint64_t val = ((uint64_t)nd[i] << ND_MUL2K_MAX_SHIFT) | carry_in;
            carry_in = ND_MUL2K_DIV1E9(val);
            nd[i] = (uint32_t)val - carry_in * 1000000000;
        }
        if (carry_in) {
            nd[++ndhi] = carry_in; carry_in = 0;
            if (start++ == ndlo) { ++ndlo; }
        }
        k -= ND_MUL2K_MAX_SHIFT;
    }
    if (k) {
        for (i = ndlo; i <= ndhi; i++) {
            uint64_t val = ((uint64_t)nd[i] << k) | carry_in;
            carry_in = ND_MUL2K_DIV1E9(val);
            nd[i] = (uint32_t)val - carry_in * 1000000000;
        }
        if (carry_in) { nd[++ndhi] = carry_in; }
    }
    return ndhi;
}

/* Divide nd by 2^k (ndlo is assumed to be zero). */
static uint32_t nd_div2k(uint32_t* nd, uint32_t ndhi, uint32_t k, sfmt_t sf) {
    neo_dassert(nd != NULL);
    uint32_t ndlo = 0, stop1 = ~0u, stop2 = ~0u;
    /* Performance hacks. */
    if (!ndhi) {
        if (!nd[0]) {
            return 0;
        } else {
            uint32_t s = neo_bsf32(nd[0]);
            if (s >= k) { nd[0] >>= k; return 0; }
            nd[0] >>= s; k -= s;
        }
    }
    if (k > 18) {
        if (STRFMT_FP(sf) == STRFMT_FP(STRFMT_T_FP_F)) {
            stop1 = (uint32_t)(63 - (int32_t)STRFMT_PREC(sf) / 9);
        } else {
            int32_t floorlog2 = (int32_t)(ndhi * 29 + neo_bsr32(nd[ndhi]) - k);
            int32_t floorlog10 = (int32_t)(floorlog2 * 0.30102999566398114);
            stop1 = (uint32_t)(62 + (floorlog10 - (int32_t)STRFMT_PREC(sf)) / 9);
            stop2 = (uint32_t)(61 + (int32_t)ndhi - (int32_t)STRFMT_PREC(sf) / 8);
        }
    }
    /* Real logic. */
    while (k >= 9) {
        uint32_t i = ndhi, carry = 0;
        for (;;) {
            uint32_t val = nd[i];
            nd[i] = (val >> 9) + carry;
            carry = (val & 0x1ff) * 1953125;
            if (i == ndlo) { break; }
            i = (i - 1) & 0x3f;
        }
        if (ndlo != stop1 && ndlo != stop2) {
            if (carry) { ndlo = (ndlo - 1) & 0x3f; nd[ndlo] = carry; }
            if (!nd[ndhi]) { ndhi = (ndhi - 1) & 0x3f; stop2--; }
        } else if (!nd[ndhi]) {
            if (ndhi != ndlo) { ndhi = (ndhi - 1) & 0x3f; stop2--; }
            else { return ndlo; }
        }
        k -= 9;
    }
    if (k) {
        uint32_t mask = (1U << k) - 1, mul = 1000000000 >> k, i = ndhi, carry = 0;
        for (;;) {
            uint32_t val = nd[i];
            nd[i] = (val >> k) + carry;
            carry = (val & mask) * mul;
            if (i == ndlo) { break; }
            i = (i - 1) & 0x3f;
        }
        if (carry) { ndlo = (ndlo - 1) & 0x3f; nd[ndlo] = carry; }
    }
    return ndlo;
}

/* Add m*10^e to nd (assumes ndlo <= e/9 <= ndhi and 0 <= m <= 9). */
static uint32_t nd_add_m10e(uint32_t* nd, uint32_t ndhi, uint8_t m, int32_t e) {
    neo_dassert(nd != NULL);
    uint32_t i, carry;
    if (e >= 0) {
        i = (uint32_t)e/9;
        carry = m * (ndigits_dec_threshold[e - (int32_t)i*9] + 1);
    } else {
        int32_t f = (e-8)/9;
        i = (uint32_t)(64 + f);
        carry = m * (ndigits_dec_threshold[e - f*9] + 1);
    }
    for (;;) {
        uint32_t val = nd[i] + carry;
        if (neo_unlikely(val >= 1000000000)) {
            val -= 1000000000;
            nd[i] = val;
            if (neo_unlikely(i == ndhi)) {
                ndhi = (ndhi + 1) & 0x3f;
                nd[ndhi] = 1;
                break;
            }
            carry = 1;
            i = (i + 1) & 0x3f;
        } else {
            nd[i] = val;
            break;
        }
    }
    return ndhi;
}

#define wint_r(x, sh, sc) { uint32_t d = (x*(((1<<sh)+sc-1)/sc))>>sh; x -= d*sc; *p++ = (uint8_t)('0'+d); }
/* Write 9-digit unsigned integer to buffer. */
static uint8_t *fmt_wuint9(uint8_t *p, uint32_t u) {
    neo_dassert(p != NULL);
    uint32_t v = u / 10000, w;
    u -= v * 10000;
    w = v / 10000;
    v -= w * 10000;
    *p++ = (uint8_t)('0'+w);
    wint_r(v, 23, 1000)
    wint_r(v, 12, 100)
    wint_r(v, 10, 10)
    *p++ = (uint8_t)('0'+v);
    wint_r(u, 23, 1000)
    wint_r(u, 12, 100)
    wint_r(u, 10, 10)
    *p++ = (uint8_t)('0'+u);
    return p;
}
#undef wint_r

/* Test whether two "nd" values are equal in their most significant digits. */
static int nd_similar(uint32_t* nd, uint32_t ndhi, uint32_t* ref, size_t hilen, size_t prec) {
    neo_dassert(nd != NULL && ref != NULL);
    uint8_t nd9[9], ref9[9];
    if (hilen <= prec) {
        if (neo_unlikely(nd[ndhi] != *ref)) { return 0; }
        prec -= hilen; ref--; ndhi = (ndhi - 1) & 0x3f;
        if (prec >= 9) {
            if (neo_unlikely(nd[ndhi] != *ref)) { return 0; }
            prec -= 9; ref--; ndhi = (ndhi - 1) & 0x3f;
        }
    } else {
        prec -= hilen - 9;
    }
    neo_assert(prec < 9 && "bad precision");
    fmt_wuint9(nd9, nd[ndhi]);
    fmt_wuint9(ref9, *ref);
    return !memcmp(nd9, ref9, prec) && (nd9[prec] < '5') == (ref9[prec] < '5');
}

static uint8_t *fmt_f64(uint8_t *p, neo_float_t x, sfmt_t sf) {
    neo_dassert(p != NULL);
    size_t width = STRFMT_WIDTH(sf), prec = STRFMT_PREC(sf), len;
    record_t t;
    t.as_float = x;
    if (neo_unlikely((t.ru32x2.hi << 1) >= 0xffe00000)) {
        /* Handle non-finite values uniformly for %a, %e, %f, %g. */
        int prefix = 0, ch = (sf & STRFMT_F_UPPER) ? 0x202020 : 0;
        if (((t.ru32x2.hi & 0x000fffff) | t.ru32x2.lo) != 0) {
            ch ^= ('n' << 16) | ('a' << 8) | 'n';
            if ((sf & STRFMT_F_SPACE)) { prefix = ' '; }
        } else {
            ch ^= ('i' << 16) | ('n' << 8) | 'f';
            if ((t.ru32x2.hi & 0x80000000)) { prefix = '-'; }
            else if ((sf & STRFMT_F_PLUS)) { prefix = '+'; }
            else if ((sf & STRFMT_F_SPACE)) { prefix = ' '; }
        }
        len = 3 + (prefix != 0);
        if (!(sf & STRFMT_F_LEFT)) {
            while (width-- > len) { *p++ = ' '; }
        }
        if (prefix) *p++ = (uint8_t)prefix;
        *p++ = (uint8_t)(ch >> 16);
        *p++ = (uint8_t)(ch >> 8);
        *p++ = (uint8_t)ch;
    } else if (STRFMT_FP(sf) == STRFMT_FP(STRFMT_T_FP_A)) {
        /* %a */
        const uint8_t *hexdig = (const uint8_t *)((sf & STRFMT_F_UPPER) ? "0123456789ABCDEFPX" : "0123456789abcdefpx");
        int32_t e = (int32_t)(t.ru32x2.hi >> 20) & 0x7ff;
        uint8_t prefix = 0, eprefix = '+';
        if (t.ru32x2.hi & 0x80000000) { prefix = '-'; }
        else if ((sf & STRFMT_F_PLUS)) { prefix = '+'; }
        else if ((sf & STRFMT_F_SPACE)) { prefix = ' '; }
        t.ru32x2.hi &= 0xfffff;
        if (e) {
            t.ru32x2.hi |= 0x100000;
            e -= 1023;
        } else if (t.ru32x2.lo | t.ru32x2.hi) {
            /* Non-zero denormal - normalise it. */
            uint32_t shift = t.ru32x2.hi ? 20-(uint32_t)neo_bsr32(t.ru32x2.hi) : 52-(uint32_t)neo_bsr32(t.ru32x2.lo);
            e = -1022 - (int32_t)shift;
            t.ru64 <<= shift;
        }
        /* abs(n) == t.u64 * 2^(e - 52) */
        /* If n != 0, bit 52 of t.u64 is set, and is the highest set bit. */
        if ((int32_t)prec < 0) {
            /* Default precision: use smallest precision giving exact result. */
            prec = t.ru32x2.lo ? 13-(uint32_t)neo_bsf32(t.ru32x2.lo)/4 : 5-(uint32_t)neo_bsf32(t.ru32x2.hi|0x100000)/4;
        } else if (prec < 13) {
            /* Precision is sufficiently low as to maybe require rounding. */
            t.ru64 += (((uint64_t)1) << (51 - prec*4));
        }
        if (e < 0) {
            eprefix = '-';
            e = -e;
        }
        len = 5 + ndigits_dec((uint32_t)e) + prec + (prefix != 0)
              + ((prec | (sf & STRFMT_F_ALT)) != 0);
        if (!(sf & (STRFMT_F_LEFT | STRFMT_F_ZERO))) {
            while (width-- > len) { *p++ = ' '; }
        }
        if (prefix) { *p++ = prefix; }
        *p++ = '0';
        *p++ = hexdig[17]; /* x or X */
        if ((sf & (STRFMT_F_LEFT | STRFMT_F_ZERO)) == STRFMT_F_ZERO) {
            while (width-- > len) { *p++ = '0'; }
        }
        *p++ = ('0' + (t.ru32x2.hi >> 20)) & 255; /* Usually '1', sometimes '0' or '2'. */
        if ((prec | (sf & STRFMT_F_ALT))) {
            /* Emit fractional part. */
            uint8_t *q = p + 1 + prec;
            *p = '.';
            if (prec < 13) { t.ru64 >>= (52 - prec*4); }
            else { while (prec > 13) { p[prec--] = '0'; } }
            while (prec) {
                p[prec--] = hexdig[t.ru64 & 15];
                t.ru64 >>= 4;
            }
            p = q;
        }
        *p++ = hexdig[16]; /* p or P */
        *p++ = eprefix; /* + or - */
        p = fmt_i32(p, e);
    } else {
        /* %e or %f or %g - begin by converting n to "nd" format. */
        uint32_t nd[64];
        uint32_t ndhi = 0, ndlo, i;
        int32_t e = (t.ru32x2.hi >> 20) & 0x7ff, ndebias = 0;
        uint8_t prefix = 0, *q;
        if (t.ru32x2.hi & 0x80000000) { prefix = '-'; }
        else if ((sf & STRFMT_F_PLUS)) { prefix = '+'; }
        else if ((sf & STRFMT_F_SPACE)) { prefix = ' '; }
        prec += ((int32_t)prec >> 31) & 7; /* Default precision is 6. */
        if (STRFMT_FP(sf) == STRFMT_FP(STRFMT_T_FP_G)) {
            /* %g - decrement precision if non-zero (to make it like %e). */
            prec--;
            prec ^= (uint32_t)((int32_t)prec >> 31);
        }
        if ((sf & STRFMT_T_FP_E) && prec < 14 && x != 0) {
            /* Precision is sufficiently low that rescaling will probably work. */
            if ((ndebias = rescale_e[e >> 6])) {
                t.as_float = x * rescale_n[e >> 6];
                if (neo_unlikely(!e)) {
                    t.as_float *= 1e10, ndebias -= 10;
                }
                t.ru64 -= 2; /* Convert 2ulp below (later we convert 2ulp above). */
                nd[0] = 0x100000 | (t.ru32x2.hi & 0xfffff);
                e = ((int32_t)(t.ru32x2.hi >> 20) & 0x7ff) - 1075 - (ND_MUL2K_MAX_SHIFT < 29);
                goto load_t_lo; rescale_failed:
                t.as_float = x;
                e = (t.ru32x2.hi >> 20) & 0x7ff;
                ndebias = 0;
                ndhi = 0;
            }
        }
        nd[0] = t.ru32x2.hi & 0xfffff;
        if (e == 0) { ++e; }
        else { nd[0] |= 0x100000; }
        e -= 1043;
        if (t.ru32x2.lo) {
            e -= 32 + (ND_MUL2K_MAX_SHIFT < 29);
            load_t_lo:
#if ND_MUL2K_MAX_SHIFT >= 29
            nd[0] = (nd[0] << 3) | (t.ru32x2.lo >> 29);
            ndhi = nd_mul2k(nd, ndhi, 29, t.ru32x2.lo & 0x1fffffff, sf);
#elif ND_MUL2K_MAX_SHIFT >= 11
            ndhi = nd_mul2k(nd, ndhi, 11, t.ru32x2.lo >> 21, sf);
            ndhi = nd_mul2k(nd, ndhi, 11, (t.ru32x2.lo >> 10) & 0x7ff, sf);
            ndhi = nd_mul2k(nd, ndhi, 11, (t.ru32x2.lo <<  1) & 0x7ff, sf);
#else
#   error "ND_MUL2K_MAX_SHIFT too small"
#endif
        }
        if (e >= 0) {
            ndhi = nd_mul2k(nd, ndhi, (uint32_t)e, 0, sf);
            ndlo = 0;
        } else {
            ndlo = nd_div2k(nd, ndhi, (uint32_t)-e, sf);
            if (ndhi && !nd[ndhi]) { --ndhi; }
        }
        /* abs(n) == nd * 10^ndebias (for slightly loose interpretation of ==) */
        if ((sf & STRFMT_T_FP_E)) {
            /* %e or %g - assume %e and start by calculating nd's exponent (nde). */
            uint8_t eprefix = '+';
            int32_t nde = -1;
            size_t hilen;
            if (ndlo && !nd[ndhi]) {
                ndhi = 64;
                while (!nd[--ndhi]);
                nde -= 64 * 9;
            }
            hilen = ndigits_dec(nd[ndhi]);
            nde += (int32_t)(ndhi * 9 + hilen);
            if (ndebias) {
                /*
                ** Rescaling was performed, but this introduced some error, and might
                ** have pushed us across a rounding boundary. We check whether this
                ** error affected the result by introducing even more error (2ulp in
                ** either direction), and seeing whether a rounding boundary was
                ** crossed. Having already converted the -2ulp case, we save off its
                ** most significant digits, convert the +2ulp case, and compare them.
                */
                int32_t eidx = e
                    + 70 + (ND_MUL2K_MAX_SHIFT < 29)
                    + (t.ru32x2.lo >= 0xfffffffe && !(~t.ru32x2.hi << 12));
                const int8_t *m_e = four_ulp_m_e + eidx * 2;
                neo_assert(0 <= eidx && eidx < 128 && "bad eidx");
                nd[33] = nd[ndhi];
                nd[32] = nd[(ndhi - 1) & 0x3f];
                nd[31] = nd[(ndhi - 2) & 0x3f];
                nd_add_m10e(nd, ndhi, (uint8_t)*m_e, m_e[1]);
                if (neo_unlikely(!nd_similar(nd, ndhi, nd + 33, hilen, prec + 1))) {
                    goto rescale_failed;
                }
            }
            if ((int32_t)(prec - (size_t)nde) < (0x3f & -(int32_t)ndlo) * 9) {
                /* Precision is sufficiently low as to maybe require rounding. */
                ndhi = nd_add_m10e(nd, ndhi, 5, (int32_t)((size_t)nde - prec - 1));
                nde += (hilen != ndigits_dec(nd[ndhi]));
            }
            nde += ndebias;
            if ((sf & STRFMT_T_FP_F)) {
                /* %g */
                if ((int32_t)prec >= nde && nde >= -4) {
                    if (nde < 0) ndhi = 0;
                    prec -= (size_t)nde;
                    goto g_format_like_f;
                } else if (!(sf & STRFMT_F_ALT) && prec && width > 5) {
                    /* Decrease precision in order to strip trailing zeroes. */
                    uint8_t tail[9];
                    uint32_t maxprec = (uint32_t)(hilen - 1 + ((ndhi - ndlo) & 0x3f) * 9);
                    if (prec >= maxprec) prec = maxprec;
                    else ndlo = (uint32_t)((ndhi - (uint32_t)(((int32_t)(prec - hilen) + 9) / 9)) & 0x3f);
                    i = (uint32_t)(prec - hilen - (((ndhi - ndlo) & 0x3f) * 9) + 10);
                    fmt_wuint9(tail, nd[ndlo]);
                    while (prec && tail[--i] == '0') {
                        prec--;
                        if (!i) {
                            if (ndlo == ndhi) { prec = 0; break; }
                            fmt_wuint9(tail, nd[++ndlo]);
                            i = 9;
                        }
                    }
                }
            }
            if (nde < 0) {
                /* Make nde non-negative. */
                eprefix = '-';
                nde = -nde;
            }
            len = 3 + prec + (prefix != 0) + ndigits_dec((uint32_t)nde) + (nde < 10)
                  + ((prec | (sf & STRFMT_F_ALT)) != 0);
            if (!(sf & (STRFMT_F_LEFT | STRFMT_F_ZERO))) {
                while (width-- > len) { *p++ = ' '; }
            }
            if (prefix) { *p++ = (uint8_t)prefix; }
            if ((sf & (STRFMT_F_LEFT | STRFMT_F_ZERO)) == STRFMT_F_ZERO) {
                while (width-- > len) { *p++ = '0'; }
            }
            q = fmt_i32(p + 1, (int32_t)nd[ndhi]);
            p[0] = p[1]; /* Put leading digit in the correct place. */
            if ((prec | (sf & STRFMT_F_ALT))) {
                /* Emit fractional part. */
                p[1] = '.'; p += 2;
                prec -= (size_t)(q - p); p = q; /* Account for digits already emitted. */
                /* Then emit chunks of 9 digits (this may emit 8 digits too many). */
                for (i = ndhi; (int32_t)prec > 0 && i != ndlo; prec -= 9) {
                    i = (i - 1) & 0x3f;
                    p = fmt_wuint9(p, nd[i]);
                }
                if ((sf & STRFMT_T_FP_F) && !(sf & STRFMT_F_ALT)) {
                    /* %g (and not %#g) - strip trailing zeroes. */
                    p += (int32_t)prec & ((int32_t)prec >> 31);
                    while (p[-1] == '0') { p--; }
                    if (p[-1] == '.') p--;
                } else {
                    /* %e (or %#g) - emit trailing zeroes. */
                    while ((int32_t)prec > 0) { *p++ = '0'; prec--; }
                    p += (int32_t)prec;
                }
            } else {
                ++p;
            }
            *p++ = (sf & STRFMT_F_UPPER) ? 'E' : 'e';
            *p++ = eprefix; /* + or - */
            if (nde < 10) *p++ = '0'; /* Always at least two digits of exponent. */
            p = fmt_i32(p, nde);
        } else {
            /* %f (or, shortly, %g in %f style) */
            if (prec < (size_t)(0x3f & -(int32_t)ndlo) * 9) {
                /* Precision is sufficiently low as to maybe require rounding. */
                ndhi = nd_add_m10e(nd, ndhi, 5, (int32_t)(0 - prec - 1));
            }
            g_format_like_f:
            if ((sf & STRFMT_T_FP_E) && !(sf & STRFMT_F_ALT) && prec && width) {
                /* Decrease precision in order to strip trailing zeroes. */
                if (ndlo) {
                    /* nd has a fractional part; we need to look at its digits. */
                    uint8_t tail[9];
                    uint32_t maxprec = (64 - ndlo) * 9;
                    if (prec >= maxprec) prec = maxprec;
                    else ndlo = (uint32_t)(64 - (prec + 8) / 9);
                    i = (uint32_t)(prec - ((63 - ndlo) * 9));
                    fmt_wuint9(tail, nd[ndlo]);
                    while (prec && tail[--i] == '0') {
                        prec--;
                        if (!i) {
                            if (ndlo == 63) { prec = 0; break; }
                            fmt_wuint9(tail, nd[++ndlo]);
                            i = 9;
                        }
                    }
                } else {
                    /* nd has no fractional part, so precision goes straight to zero. */
                    prec = 0;
                }
            }
            len = ndhi * 9 + ndigits_dec(nd[ndhi]) + prec + (prefix != 0)
                  + ((prec | (sf & STRFMT_F_ALT)) != 0);
            if (!(sf & (STRFMT_F_LEFT | STRFMT_F_ZERO))) {
                while (width-- > len) *p++ = ' ';
            }
            if (prefix) *p++ = prefix;
            if ((sf & (STRFMT_F_LEFT | STRFMT_F_ZERO)) == STRFMT_F_ZERO) {
                while (width-- > len) *p++ = '0';
            }
            /* Emit integer part. */
            p = fmt_i32(p, (int32_t)nd[ndhi]);
            i = ndhi;
            while (i) p = fmt_wuint9(p, nd[--i]);
            if ((prec | (sf & STRFMT_F_ALT))) {
                /* Emit fractional part. */
                *p++ = '.';
                /* Emit chunks of 9 digits (this may emit 8 digits too many). */
                while ((int32_t)prec > 0 && i != ndlo) {
                    i = (i - 1) & 0x3f;
                    p = fmt_wuint9(p, nd[i]);
                    prec -= 9;
                }
                if ((sf & STRFMT_T_FP_E) && !(sf & STRFMT_F_ALT)) {
                    /* %g (and not %#g) - strip trailing zeroes. */
                    p += (int32_t)prec & ((int32_t)prec >> 31);
                    while (p[-1] == '0') p--;
                    if (p[-1] == '.') p--;
                } else {
                    /* %f (or %#g) - emit trailing zeroes. */
                    while ((int32_t)prec > 0) { *p++ = '0'; prec--; }
                    p += (int32_t)prec;
                }
            }
        }
    }
    if ((sf & STRFMT_F_LEFT)) while (width-- > len) {
        *p++ = ' ';
    }
    return p;
}

uint8_t *neo_fmt_float(uint8_t *p, neo_float_t x) {
    neo_dassert(p != NULL);
    return fmt_f64(p, x, STRFMT_G14);
}

uint8_t *neo_fmt_ptr(uint8_t *p, const void *v) {
    neo_dassert(p != NULL);
    ptrdiff_t x = (ptrdiff_t)v;
    size_t i, n = 2+2*sizeof(ptrdiff_t);
    if (!x) {
        *p++ = 'n';
        *p++ = 'u';
        *p++ = 'l';
        *p++ = 'l';
        return p;
    }
    n = 2+2*4+((x>>32) ? 2+2*((size_t)neo_bsr32((uint32_t)(x>>32))>>3) : 0);
    p[0] = '0';
    p[1] = 'x';
    for (i = n-1; i >= 2; i--, x >>= 4) {
        p[i] = ((const uint8_t *)"0123456789abcdef")[(x & 15)];
    }
    return p+n;
}
