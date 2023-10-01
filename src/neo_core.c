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
#   include <time.h>

#else
#   error "unsupported platform"
#endif

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
    neo_assert(setlocale(LC_ALL, ".UTF-8"), "Failed to set locale");
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

#if NEO_OS_WINDOWS
#error "Todo"
uint64_t neo_hp_clock_ms(void) {
    return 0;
}

uint64_t neo_hp_clock_us(void) {
    return 0;
}

#else

uint64_t neo_hp_clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

uint64_t neo_hp_clock_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

#endif

void neo_mempool_init(neo_mempool_t *self, size_t cap) {
    neo_dassert(self != NULL, "self is NULL");
    memset(self, 0, sizeof(*self));
    cap = cap ? cap : 1 << 9;
    self->cap = cap;
    self->top = neo_memalloc(NULL, cap);
    memset(self->top, 0, cap); /* Zero the memory. */
}

void *neo_mempool_alloc(neo_mempool_t *self, size_t len) {
    neo_dassert(self != NULL, "self is NULL");
    neo_assert(len != 0, "Allocation length must not be zero");
    size_t total = self->len + len;
    if (total >= self->cap) {
        size_t old = self->cap;
        do {
            self->cap <<= 1;
        } while (self->cap <= total);
        self->top = neo_memalloc(self->top, self->cap);
        size_t delta = self->cap - old;
        memset((uint8_t *)self->top + old, 0, delta); /* Zero the new memory. */
    }
    void *p = (uint8_t *)self->top + self->len;
    self->len += len;
    ++self->num_allocs;
    return p;
}

void *neo_mempool_alloc_aligned(neo_mempool_t *self, size_t len, size_t align) {
    neo_dassert(self != NULL, "self is NULL");
    neo_dassert(align != 0 && align >= sizeof(void *) && !(align & (align - 1)), "Invalid alignment: %zu", align);
    uintptr_t off = (uintptr_t)align - 1 + sizeof(void *);
    void *p = neo_mempool_alloc(self, len + off);
    return (void *)(((uintptr_t)p + off) & ~(align - 1));
}

size_t neo_mempool_alloc_idx(neo_mempool_t *self, size_t len, uint32_t base, size_t lim, void **pp) {
    neo_dassert(self != NULL && len != 0, "self is NULL");
    size_t idx = self->len + base * len;
    neo_assert(idx <= lim, "Pool index limit reached. Max: %zu, Current: %zu", lim, idx);
    void *p = neo_mempool_alloc(self, len);
    if (pp) { *pp = p; }
    idx /= len;
    return idx;
}

void *neo_mempool_realloc(neo_mempool_t *self, void *blk, size_t oldlen, size_t newlen) {
    neo_dassert(self != NULL, "self is NULL");
    neo_assert(blk != NULL && oldlen != 0 && newlen != 0, "Invalid arguments");
    if (neo_unlikely(oldlen == newlen)) { return blk; }
    const void *prev = blk; /* We need to copy the old data into the new block. */
    blk = neo_mempool_alloc(self, newlen);
    memcpy(blk, prev,
           oldlen); /* Copy the old data into the new block. This is safe because the old data is still in the self. */
    return blk;
}

void neo_mempool_reset(neo_mempool_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    self->len = 0;
    self->num_allocs = 0;
}

void neo_mempool_free(neo_mempool_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    neo_memalloc(self->top, 0);
}

bool record_eq(record_t a, record_t b, rtag_t tag) {
    switch (tag) {
        case RT_INT:
            return a.as_int == b.as_int;
        case RT_FLOAT:
            return a.as_float == b.as_float; /* TODO: Use ULP-based comparison? */
        case RT_CHAR:
            return a.as_char == b.as_char;
        case RT_BOOL:
            return a.as_bool == b.as_bool;
        case RT_REF:
            return a.as_ref == b.as_ref;
        default:
            return false;
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
    neo_dassert(fp != NULL && filepath != NULL && mode != 0, "Invalid arguments");
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
    neo_dassert(buf != NULL && ppos != NULL, "Invalid arguments");
    size_t pos = 0;
    uint32_t cp;
    while (pos < len) {
        size_t np = pos + 16;
        if (np <= len) { /* If it is safe to read 8 more bytes and check that they are ASCII. */
            uint64_t v1, v2;
            memcpy(&v1, buf + pos, sizeof(v1));
            memcpy(&v2, buf + pos + sizeof(v1), sizeof(v2));
            if (!((v1 | v2) & UINT64_C(0x8080808080808080))) {
                pos = np;
                continue;
            }
        }
        uint8_t b = buf[pos];
        while (b < 0x80) {
            if (neo_likely(++pos == len)) {
                *ppos = len;
                return NEO_UNIERR_OK;
            }
            b = buf[pos];
        }
        if ((b & 0xe0) == 0xc0) {
            np = pos + 2;
            if (neo_unlikely(np > len)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            cp = (b & 0x1fu) << 6u | (buf[pos + 1] & 0x3fu);
            if (neo_unlikely((cp < 0x80) || (0x7ff < cp))) {
                *ppos = pos;
                return NEO_UNIERR_OVERLONG;
            }
        } else if ((b & 0xf0) == 0xe0) {
            np = pos + 3;
            if (neo_unlikely(np > len)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 2] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            cp = (b & 0xfu) << 12u | (buf[pos + 1] & 0x3fu) << 6u | (buf[pos + 2] & 0x3fu);
            if (neo_unlikely((cp < 0x800) || (0xffff < cp))) {
                *ppos = pos;
                return NEO_UNIERR_OVERLONG;
            }
            if (neo_unlikely(0xd7ff < cp && cp < 0xe000)) {
                *ppos = pos;
                return NEO_UNIERR_SURROGATE;
            }
        } else if ((b & 0xf8) == 0xf0) {
            np = pos + 4;
            if (neo_unlikely(np > len)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 2] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            if (neo_unlikely((buf[pos + 3] & 0xc0) != 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_SHORT;
            }
            cp = (b & 0x7u) << 18u | (buf[pos + 1] & 0x3fu) << 12u | (buf[pos + 2] & 0x3fu) << 6u |
                 (buf[pos + 3] & 0x3fu);
            if (neo_unlikely(cp <= 0xffff)) {
                *ppos = pos;
                return NEO_UNIERR_OVERLONG;
            }
            if (neo_unlikely(0x10ffff < cp)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_LARGE;
            }
        } else { /* We either have too many continuation bytes or an invalid leading byte. */
            if (neo_unlikely((b & 0xc0) == 0x80)) {
                *ppos = pos;
                return NEO_UNIERR_TOO_LONG;
            } else {
                *ppos = pos;
                return NEO_UNIERR_HEADER_BITS;
            }
        }
        pos = np;
    }
    *ppos = len;
    return NEO_UNIERR_OK;
}

bool neo_utf8_is_ascii(const uint8_t *buf, size_t len) {
    neo_dassert(buf != NULL, "Invalid arguments");
    for (size_t i = 0; i < len; ++i) {
        if (neo_unlikely(buf[i] > 0x7f)) { return false; }
    }
    return true;
}

uint32_t neo_hash_x17(const void *key, size_t len) {
    uint32_t r = 0x1505;
    const uint8_t *p = (const uint8_t *)key;
    for (size_t i = 0; i < len; ++i) {
        r = 17 * r + (p[i] - ' ');
    }
    return r ^ (r >> 16);
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
    switch (len % 8) {
        case 7:
            rest |= (uint64_t)data[6] << 56;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 6:
            rest |= (uint64_t)data[5] << 48;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 5:
            rest |= (uint64_t)data[4] << 40;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 4:
            rest |= (uint64_t)data[3] << 32;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 3:
            rest |= (uint64_t)data[2] << 24;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 2:
            rest |= (uint64_t)data[1] << 16;
                    NEO_FALLTHROUGH; /* fallthrough */
        case 1:
            rest |= (uint64_t)data[0] << 8;
            r ^= rest;
            r *= 0xd6e8feb86659fd93;
    }
    return (uint32_t)(r ^ (r >> 32));
}

#define rol32(x, r) ((x<<r)|(x>>(32-r)))
#define fmix32(h) h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16;

uint64_t neo_hash_mumrmur3_86_128(const void *key, size_t len, uint32_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    int64_t nblocks = (int64_t)len / 16;
    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;
    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 16);
    for (int64_t i = -nblocks; i; ++i) {
        uint32_t k1 = blocks[i * 4 + 0];
        uint32_t k2 = blocks[i * 4 + 1];
        uint32_t k3 = blocks[i * 4 + 2];
        uint32_t k4 = blocks[i * 4 + 3];
        k1 *= c1;
        k1 = rol32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = rol32(h1, 19);
        h1 += h2;
        h1 = h1 * 5 + 0x561ccd1b;
        k2 *= c2;
        k2 = rol32(k2, 16);
        k2 *= c3;
        h2 ^= k2;
        h2 = rol32(h2, 17);
        h2 += h3;
        h2 = h2 * 5 + 0x0bcaa747;
        k3 *= c3;
        k3 = rol32(k3, 17);
        k3 *= c4;
        h3 ^= k3;
        h3 = rol32(h3, 15);
        h3 += h4;
        h3 = h3 * 5 + 0x96cd1c35;
        k4 *= c4;
        k4 = rol32(k4, 18);
        k4 *= c1;
        h4 ^= k4;
        h4 = rol32(h4, 13);
        h4 += h1;
        h4 = h4 * 5 + 0x32ac3b17;
    }
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);
    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;
    switch (len & 15) {
        case 15:
            k4 ^= (uint32_t)tail[14] << 16;
                    NEO_FALLTHROUGH;
        case 14:
            k4 ^= (uint32_t)tail[13] << 8;
                    NEO_FALLTHROUGH;
        case 13:
            k4 ^= (uint32_t)tail[12] << 0;
            k4 *= c4;
            k4 = rol32(k4, 18);
            k4 *= c1;
            h4 ^= k4;
                    NEO_FALLTHROUGH;
        case 12:
            k3 ^= (uint32_t)tail[11] << 24;
                    NEO_FALLTHROUGH;
        case 11:
            k3 ^= (uint32_t)tail[10] << 16;
                    NEO_FALLTHROUGH;
        case 10:
            k3 ^= (uint32_t)tail[9] << 8;
                    NEO_FALLTHROUGH;
        case 9:
            k3 ^= (uint32_t)tail[8] << 0;
            k3 *= c3;
            k3 = rol32(k3, 17);
            k3 *= c4;
            h3 ^= k3;
                    NEO_FALLTHROUGH;
        case 8:
            k2 ^= (uint32_t)tail[7] << 24;
                    NEO_FALLTHROUGH;
        case 7:
            k2 ^= (uint32_t)tail[6] << 16;
                    NEO_FALLTHROUGH;
        case 6:
            k2 ^= (uint32_t)tail[5] << 8;
                    NEO_FALLTHROUGH;
        case 5:
            k2 ^= (uint32_t)tail[4] << 0;
            k2 *= c2;
            k2 = rol32(k2, 16);
            k2 *= c3;
            h2 ^= k2;
                    NEO_FALLTHROUGH;
        case 4:
            k1 ^= (uint32_t)tail[3] << 24;
                    NEO_FALLTHROUGH;
        case 3:
            k1 ^= (uint32_t)tail[2] << 16;
                    NEO_FALLTHROUGH;
        case 2:
            k1 ^= (uint32_t)tail[1] << 8;
                    NEO_FALLTHROUGH;
        case 1:
            k1 ^= (uint32_t)tail[0] << 0;
            k1 *= c1;
            k1 = rol32(k1, 15);
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
    fmix32(h1);
    fmix32(h2);
    fmix32(h3);
    fmix32(h4);
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
        case 7:
            b |= ((uint64_t)in[6]) << 48;
                    NEO_FALLTHROUGH;
        case 6:
            b |= ((uint64_t)in[5]) << 40;
                    NEO_FALLTHROUGH;
        case 5:
            b |= ((uint64_t)in[4]) << 32;
                    NEO_FALLTHROUGH;
        case 4:
            b |= ((uint64_t)in[3]) << 24;
                    NEO_FALLTHROUGH;
        case 3:
            b |= ((uint64_t)in[2]) << 16;
                    NEO_FALLTHROUGH;
        case 2:
            b |= ((uint64_t)in[1]) << 8;
                    NEO_FALLTHROUGH;
        case 1:
            b |= ((uint64_t)in[0]);
            break;
        case 0:
            break;
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
    u64split8_le((uint8_t *)&out, b);
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
    neo_assert(str != NULL, "String ptr is NULL");
    size_t len = strlen(str); /* strlen also works with UTF-8 strings to find the end \0. */
    char *dup = neo_memalloc(NULL, (len + 1) * sizeof(*dup));
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

void neo_printutf8(FILE *f, const uint8_t *str) {
    neo_assert(str != NULL, "String ptr is NULL");
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

#define CHAR_CNTRL 0x01
#define CHAR_SPACE 0x02
#define CHAR_PUNCT 0x04
#define CHAR_DIGIT 0x08
#define CHAR_XDIGIT 0x10
#define CHAR_UPPER 0x20
#define CHAR_LOWER 0x40
#define CHAR_IDENT 0x80
#define CHAR_ALPHA (CHAR_LOWER|CHAR_UPPER)
#define CHAR_ALNUM (CHAR_ALPHA|CHAR_DIGIT)
#define CHAR_GRAPH (CHAR_ALNUM|CHAR_PUNCT)

/* Only pass -1 or 0..255 to these macros. Never pass a signed char! */
#define char_isa(c, t) ((char_bits+1)[(c)] & t)
#define char_iscntrl(c) char_isa((c), CHAR_CNTRL)
#define char_isspace(c) char_isa((c), CHAR_SPACE)
#define char_ispunct(c) char_isa((c), CHAR_PUNCT)
#define char_isdigit(c) char_isa((c), CHAR_DIGIT)
#define char_isxdigit(c) char_isa((c), CHAR_XDIGIT)
#define char_isupper(c) char_isa((c), CHAR_UPPER)
#define char_islower(c) char_isa((c), CHAR_LOWER)
#define char_isident(c) char_isa((c), CHAR_IDENT)
#define char_isalpha(c) char_isa((c), CHAR_ALPHA)
#define char_isalnum(c) char_isa((c), CHAR_ALNUM)
#define char_isgraph(c) char_isa((c), CHAR_GRAPH)
#define char_toupper(c) ((c)-(char_islower(c)>>1))
#define char_tolower(c) ((c)+char_isupper(c))

static const uint8_t char_bits[257] = {
        0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 4, 4, 4, 4, 4, 4,
        4, 176, 176, 176, 176, 176, 176, 160, 160, 160, 160, 160, 160, 160, 160, 160,
        160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 4, 4, 4, 4, 132,
        4, 208, 208, 208, 208, 208, 208, 192, 192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 4, 4, 4, 4, 1,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
};


/* Definitions for circular decimal digit buffer (base 100 = 2 digits/byte). */
#define NEO_STRSCAN_DIG    1024
#define NEO_STRSCAN_MAXDIG    800     /* 772 + extra are sufficient. */
#define NEO_STRSCAN_DDIG (NEO_STRSCAN_DIG/2)
#define NEO_STRSCAN_DMASK (NEO_STRSCAN_DDIG-1)
#define NEO_STRSCAN_MAXEXP    (1<<20)

/* Helpers for circular buffer. */
#define dnext(a) (((a)+1)&NEO_STRSCAN_DMASK)
#define dprev(a) (((a)-1)&NEO_STRSCAN_DMASK)
#define dlen(lo, hi) ((int32_t)(((lo)-(hi))&NEO_STRSCAN_DMASK))
#define casecmp(c, k) (((c)|0x20)==(k))

/* Final conversion to double. */
static void strscan_double(uint64_t x, record_t *o, int32_t ex2, int32_t neg) {
    neo_dassert(o != NULL, "Invalid arguments");
    double n;
    /* Avoid double rounding for denormals. */
    if (neo_unlikely(ex2 <= -1075 && x != 0)) {
#if NEO_COM_GCC ^ NEO_COM_CLANG
        int32_t b = (int32_t)(__builtin_clzll(x) ^ 63);
#else
        int32_t b = (x>>32)
                    ? 32+(int32_t)neo_bsr32((uint32_t)(x>>32)) :
                    (int32_t)neo_bsr32((uint32_t)x);
#endif
        if ((int32_t)b + ex2 <= -1023 && (int32_t)b + ex2 >= -1075) {
            uint64_t rb = (uint64_t)1 << (-1075 - ex2);
            if ((x & rb) && ((x & (rb + rb + rb - 1)))) { x += rb + rb; }
            x = (x & ~(rb + rb - 1));
        }
    }
    /* Convert to double using a signed int64_t conversion, then rescale. */
    neo_assert((int64_t)x >= 0, "Bad double conversion");
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
    neo_dassert(p != NULL && o != NULL, "Invalid arguments");
    uint64_t x = 0;
    uint32_t i;
    for (i = dig > 16 ? 16 : dig; i; i--, p++) {     /* Scan hex digits. */
        uint32_t d = (*p != '.' ? *p : *++p);
        if (d > '9') d += 9;
        x = (x << 4) + (d & 15);
    }
    for (i = 16; i < dig; i++, p++) { /* Summarize rounding-effect of excess digits. */
        x |= ((*p != '.' ? *p : *++p) != '0'), ex2 += 4;
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (!(opt & NEO_STRSCAN_OPT_TONUM)
                && x < 0x80000000u + (uint32_t)neg
                && !(x == 0 && neg)) {
                o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
                return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
            }
            if (!(opt & NEO_STRSCAN_OPT_C)) {
                fmt = NEO_STRSCAN_NUM;
                break;
            }
                    NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if (dig > 8) { return NEO_STRSCAN_ERROR; }
            o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
            return NEO_STRSCAN_U32;
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            if (dig > 16) { return NEO_STRSCAN_ERROR; }
            o->ru64 = neg ? ~x + 1u : x;
            return fmt;
        default:
            break;
    }
    /* Reduce range, then convert to double. */
    if ((x & 0xc00000000000000ull)) {
        x = (x >> 2) | (x & 3);
        ex2 += 2;
    }
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
    neo_dassert(p != NULL && o != NULL, "Invalid arguments");
    uint64_t x = 0;
    if (dig > 22 || (dig == 22 && *p > '1')) { return NEO_STRSCAN_ERROR; }
    while (dig-- > 0) {     /* Scan octal digits. */
        if (!(*p >= '0' && *p <= '7')) { return NEO_STRSCAN_ERROR; }
        x = (x << 3) + (*p++ & 7);
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (x >= 0x80000000u + (uint32_t)neg) {
                fmt = NEO_STRSCAN_U32;
            }
                    NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if ((x >> 32)) { return NEO_STRSCAN_ERROR; }
            o->ri32 = neg ? (int32_t)(~(uint32_t)x + 1u) : (int32_t)x;
            break;
        default:
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            o->ru64 = neg ? ~x + 1u : x;
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
    neo_dassert(p != NULL && o != NULL, "Invalid arguments");
    uint8_t xi[NEO_STRSCAN_DDIG], *xip = xi;
    if (dig) {
        uint32_t i = dig;
        if (i > NEO_STRSCAN_MAXDIG) {
            ex10 += (int32_t)(i - NEO_STRSCAN_MAXDIG);
            i = NEO_STRSCAN_MAXDIG;
        }
        if ((((uint32_t)ex10 ^ i) & 1)) { /* Scan unaligned leading digit. */
            *xip++ = ((*p != '.' ? *p : *++p) & 15), i--, p++;
        }
        for (; i > 1; i -= 2) { /* Scan aligned double-digits. */
            uint32_t d = 10 * ((*p != '.' ? *p : *++p) & 15);
            p++;
            *xip++ = (uint8_t)(d + ((*p != '.' ? *p : *++p) & 15));
            p++;
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
        for (xis = xi + 1; xis < xip; xis++) x = x * 100 + *xis;
        if (!(dig == 20 && (xi[0] > 18 || (int64_t)x >= 0))) {  /* No overflow? */
            /* Format-specific handling. */
            switch (fmt) {
                case NEO_STRSCAN_INT:
                    if (!(opt & NEO_STRSCAN_OPT_TONUM) && x < 0x80000000u + (uint32_t)neg) {
                        o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
                        return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
                    }
                    if (!(opt & NEO_STRSCAN_OPT_C)) {
                        fmt = NEO_STRSCAN_NUM;
                        goto plainnumber;
                    }
                            NEO_FALLTHROUGH;
                case NEO_STRSCAN_U32:
                    if ((x >> 32) != 0) { return NEO_STRSCAN_ERROR; }
                    o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
                    return NEO_STRSCAN_U32;
                case NEO_STRSCAN_I64:
                case NEO_STRSCAN_U64:
                    o->ru64 = neg ? ~x + 1u : x;
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
        uint32_t hi = 0, lo = (uint32_t)(xip - xi);
        int32_t ex2 = 0, idig = (int32_t)lo + (ex10 >> 1);
        neo_assert(lo > 0 && (ex10 & 1) == 0, "Bad lo ex10: %" PRIx32, ex10);
        /* Handle simple overflow/underflow. */
        if (idig > 310 / 2) {
            if (neg) { rec_setminf(*o); }
            else { rec_setpinf(*o); }
            return fmt;
        } else if (idig < -326 / 2) {
            o->as_float = neg ? -0.0 : 0.0;
            return fmt;
        }
        while (idig < 9 && idig < dlen(lo, hi)) { /* Scale up until we have at least 17 or 18 integer part digits. */
            uint32_t i, cy = 0;
            ex2 -= 6;
            for (i = dprev(lo);; i = dprev(i)) {
                uint32_t d = (uint32_t)(xi[i] << 6) + cy;
                cy = (((d >> 2) * 5243) >> 17);
                d = d - cy * 100;  /* Div/mod 100. */
                xi[i] = (uint8_t)d;
                if (i == hi) { break; }
                if (d == 0 && i == dprev(lo)) { lo = i; }
            }
            if (cy) {
                hi = dprev(hi);
                if (xi[dprev(lo)] == 0) { lo = dprev(lo); }
                else if (hi == lo) {
                    lo = dprev(lo);
                    xi[dprev(lo)] |= xi[lo];
                }
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
                if (hi == lo) {
                    xi[dprev(lo)] |= 1;
                    break;
                }
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
                    x *= 100;
                }
            } else {  /* Gather round bit from remaining digits. */
                x <<= 1;
                --ex2;
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
    neo_dassert(p != NULL && o != NULL, "Invalid arguments");
    uint64_t x = 0;
    uint32_t i;
    if (ex2 || dig > 64) { return NEO_STRSCAN_ERROR; }
    for (i = dig; i; i--, p++) {     /* Scan binary digits. */
        if ((*p & ~1) != '0') { return NEO_STRSCAN_ERROR; }
        x = (x << 1) | (*p & 1);
    }
    switch (fmt) {     /* Format-specific handling. */
        case NEO_STRSCAN_INT:
            if (!(opt & NEO_STRSCAN_OPT_TONUM) && x < 0x80000000u + (uint32_t)neg) {
                o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
                return NEO_STRSCAN_INT;  /* Fast path for 32-bit integers. */
            }
            if (!(opt & NEO_STRSCAN_OPT_C)) {
                fmt = NEO_STRSCAN_NUM;
                break;
            }
                    NEO_FALLTHROUGH;
        case NEO_STRSCAN_U32:
            if (dig > 32) return NEO_STRSCAN_ERROR;
            o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
            return NEO_STRSCAN_U32;
        case NEO_STRSCAN_I64:
        case NEO_STRSCAN_U64:
            o->ru64 = neg ? ~x + 1u : x;
            return fmt;
        default:
            break;
    }
    /* Reduce range, then convert to double. */
    if ((x & 0xc00000000000000ull)) {
        x = (x >> 2) | (x & 3);
        ex2 += 2;
    }
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
    neo_dassert(p != NULL && o != NULL, "Invalid arguments");
    if (!len || !*p) {
        o->ru64 = 0;
        return NEO_STRSCAN_EMPTY;
    }
    int32_t neg = 0;
    const uint8_t *pe = p + len;
    /* Remove leading space, parse sign and non-numbers. */
    if (neo_unlikely(!char_isdigit(*p))) {
        while (char_isspace(*p)) { p++; }
        if (*p == '+' || *p == '-') { neg = (*p++ == '-'); }
        if (neo_unlikely(*p >= 'A')) {  /* Parse "inf", "infinity" or "nan". */
            record_t tmp;
            rec_setnan(tmp);
            if (casecmp(p[0], 'i') && casecmp(p[1], 'n') && casecmp(p[2], 'f')) {
                if (neg) {
                    rec_setminf(tmp);
                } else {
                    rec_setpinf(tmp);
                }
                p += 3;
                if (casecmp(p[0], 'i') && casecmp(p[1], 'n') && casecmp(p[2], 'i') &&
                    casecmp(p[3], 't') && casecmp(p[4], 'y')) { p += 5; }
            } else if (casecmp(p[0], 'n') && casecmp(p[1], 'a') && casecmp(p[2], 'n')) {
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
            for (;; p++) {
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
        for (sp = p;; p++) { /* Preliminary digit and decimal point scan. */
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
                ex = (int32_t)(dp - (p - 1));
                dp = p - 1;
                while (ex < 0 && *dp-- == '0') { ++ex, --dig; }  /* Skip trailing zeros. */
                if (ex <= -NEO_STRSCAN_MAXEXP) { return NEO_STRSCAN_ERROR; }
                if (base == 16) { ex *= 4; }
            }
        }
        if (base >= 10 && casecmp(*p, (uint32_t)(base == 16 ? 'p' : 'e'))) {  /* Parse exponent. */
            uint32_t xx;
            int negx = 0;
            fmt = NEO_STRSCAN_NUM;
            p++;
            if (*p == '+' || *p == '-') { negx = (*p++ == '-'); }
            if (!char_isdigit(*p)) { return NEO_STRSCAN_ERROR; }
            xx = (*p++ & 15);
            while (char_isdigit(*p)) {
                xx = xx * 10 + (*p & 15);
                if (xx >= NEO_STRSCAN_MAXEXP) { return NEO_STRSCAN_ERROR; }
                p++;
            }
            ex += negx ? (int32_t)(~xx + 1u) : (int32_t)xx;
        }
        /* Parse suffix. */
        if (*p) {
            /* I (IMAG), U (U32), LL (I64), ULL/LLU (U64), L (long), UL/LU (ulong). */
            /* NYI: f (float). Not needed until cp_number() handles non-integers. */
            if (casecmp(*p, 'i')) {
                if (!(opt & NEO_STRSCAN_OPT_IMAG)) { return NEO_STRSCAN_ERROR; }
                ++p;
                fmt = NEO_STRSCAN_IMAG;
            } else if (fmt == NEO_STRSCAN_INT) {
                if (casecmp(*p, 'u')) { p++, fmt = NEO_STRSCAN_U32; }
                if (casecmp(*p, 'l')) {
                    ++p;
                    if (casecmp(*p, 'l')) { p++, fmt += NEO_STRSCAN_I64 - NEO_STRSCAN_INT; }
                    else if (!(opt & NEO_STRSCAN_OPT_C)) { return NEO_STRSCAN_ERROR; }
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
            (dig < 10 || (dig == 10 && *sp <= '2' && x < 0x80000000u + (uint32_t)neg))) {
            if ((opt & NEO_STRSCAN_OPT_TONUM)) {
                o->as_float = neg ? -(double)x : (double)x;
                return NEO_STRSCAN_NUM;
            } else if (x == 0 && neg) {
                o->as_float = -0.0;
                return NEO_STRSCAN_NUM;
            } else {
                o->ri32 = neg ? (int32_t)(~x + 1u) : (int32_t)x;
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
            if (n == (double)i) {
                o->ri32 = i;
                return NEO_STRSCAN_INT;
            }
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
static const int16_t rescale_e[] = {RESCALE_EXPONENTS(-, +)};
static const double rescale_n[] = {RESCALE_EXPONENTS(ONE_E_P, ONE_E_N)};
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
static uint32_t ndigits_dec_threshold[11] = {
        0, 9U, 99U, 999U, 9999U, 99999U, 999999U,
        9999999U, 99999999U, 999999999U, 0xffffffffU
};

#define wint_r(x, sh, sc) { uint32_t d = (x*(((1<<sh)+sc-1)/sc))>>sh; x -= d*sc; *p++ = (uint8_t)('0'+d); }

static uint8_t *fmt_i32(uint8_t *p, int32_t k) { /* Write integer to buffer. */
    neo_dassert(p != NULL, "Invalid arguments");
    uint32_t u = (uint32_t)k;
    if (k < 0) {
        u = ~u + 1u;
        *p++ = '-';
    }
    if (u < 10000) {
        if (u < 10) { goto dig1; }
        if (u < 100) { goto dig2; }
        if (u < 1000) { goto dig3; }
    } else {
        uint32_t v = u / 10000;
        u -= v * 10000;
        if (v < 10000) {
            if (v < 10) { goto dig5; }
            if (v < 100) { goto dig6; }
            if (v < 1000) { goto dig7; }
        } else {
            uint32_t w = v / 10000;
            v -= w * 10000;
            if (w >= 10) {wint_r(w, 10, 10)}
            *p++ = (uint8_t)('0' + w);
        }
        wint_r(v, 23, 1000)
        dig7:
        wint_r(v, 12, 100)
        dig6:
        wint_r(v, 10, 10)
        dig5:
        *p++ = (uint8_t)('0' + v);
    }
    wint_r(u, 23, 1000)
    dig3:
    wint_r(u, 12, 100)
    dig2:
    wint_r(u, 10, 10)
    dig1:
    *p++ = (uint8_t)('0' + u);
    return p;
}

#undef wint_r

uint8_t *neo_fmt_int(uint8_t *p, neo_int_t x) {
    neo_dassert(p != NULL, "Invalid arguments");
    p = fmt_i32(p, (int32_t)(x & ~(uint32_t)0));
    return (x >> 32) == 0 ? p : fmt_i32(p, (int32_t)(x >> 32));
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
#define STRFMT_T_HEX    0x0010    /* STRFMT_UINT */
#define STRFMT_T_OCT    0x0020    /* STRFMT_UINT */
#define STRFMT_T_FP_A    0x0000    /* STRFMT_NUM */
#define STRFMT_T_FP_E    0x0010    /* STRFMT_NUM */
#define STRFMT_T_FP_F    0x0020    /* STRFMT_NUM */
#define STRFMT_T_FP_G    0x0030    /* STRFMT_NUM */
#define STRFMT_T_QUOTED    0x0010    /* STRFMT_STR */

/* Format flags. */
#define STRFMT_F_LEFT    0x0100
#define STRFMT_F_PLUS    0x0200
#define STRFMT_F_ZERO    0x0400
#define STRFMT_F_SPACE    0x0800
#define STRFMT_F_ALT    0x1000
#define STRFMT_F_UPPER    0x2000

/* Format indicator fields. */
#define STRFMT_SH_WIDTH    16
#define STRFMT_SH_PREC    24
#define strfmt_width(sf) (((sf)>>STRFMT_SH_WIDTH)&255u)
#define strfmt_prec(sf) ((((sf)>>STRFMT_SH_PREC)&255u)-1u)
#define strfmt_fp(sf) (((sf)>>4)&3)

/* Formats for conversion characters. */
#define STRFMT_A (STRFMT_NUM|STRFMT_T_FP_A)
#define STRFMT_C (STRFMT_CHAR)
#define STRFMT_D (STRFMT_INT)
#define STRFMT_E (STRFMT_NUM|STRFMT_T_FP_E)
#define STRFMT_F (STRFMT_NUM|STRFMT_T_FP_F)
#define STRFMT_G (STRFMT_NUM|STRFMT_T_FP_G)
#define STRFMT_I STRFMT_D
#define STRFMT_O (STRFMT_UINT|STRFMT_T_OCT)
#define STRFMT_P (STRFMT_PTR)
#define STRFMT_Q (STRFMT_STR|STRFMT_T_QUOTED)
#define STRFMT_S (STRFMT_STR)
#define STRFMT_U (STRFMT_UINT)
#define STRFMT_X (STRFMT_UINT|STRFMT_T_HEX)
#define STRFMT_G14 (STRFMT_G|((14+1)<<STRFMT_SH_PREC))

#define ND_MUL2K_MAX_SHIFT 29
#define nd_mul2k_div1e9(val) ((uint32_t)((val)/1000000000))

/* Multiply nd by 2^k and add carry_in (ndlo is assumed to be zero). */
static uint32_t nd_mul2k(uint32_t *nd, uint32_t ndhi, uint32_t k, uint32_t carry_in, sfmt_t sf) {
    neo_dassert(nd != NULL, "Invalid arguments");
    uint32_t i, ndlo = 0, start = 1;
    /* Performance hacks. */
    if (k > ND_MUL2K_MAX_SHIFT * 2 && strfmt_fp(sf) != strfmt_fp(STRFMT_T_FP_F)) {
        start = ndhi - (strfmt_prec(sf) + 17) / 8;
    }
    /* Real logic. */
    while (k >= ND_MUL2K_MAX_SHIFT) {
        for (i = ndlo; i <= ndhi; i++) {
            uint64_t val = ((uint64_t)nd[i] << ND_MUL2K_MAX_SHIFT) | carry_in;
            carry_in = nd_mul2k_div1e9(val);
            nd[i] = (uint32_t)val - carry_in * 1000000000;
        }
        if (carry_in) {
            nd[++ndhi] = carry_in;
            carry_in = 0;
            if (start++ == ndlo) { ++ndlo; }
        }
        k -= ND_MUL2K_MAX_SHIFT;
    }
    if (k) {
        for (i = ndlo; i <= ndhi; i++) {
            uint64_t val = ((uint64_t)nd[i] << k) | carry_in;
            carry_in = nd_mul2k_div1e9(val);
            nd[i] = (uint32_t)val - carry_in * 1000000000;
        }
        if (carry_in) { nd[++ndhi] = carry_in; }
    }
    return ndhi;
}

/* Divide nd by 2^k (ndlo is assumed to be zero). */
static uint32_t nd_div2k(uint32_t *nd, uint32_t ndhi, uint32_t k, sfmt_t sf) {
    neo_dassert(nd != NULL, "Invalid arguments");
    uint32_t ndlo = 0, stop1 = ~0u, stop2 = ~0u;
    /* Performance hacks. */
    if (!ndhi) {
        if (!nd[0]) {
            return 0;
        } else {
            uint32_t s = (uint32_t)neo_bsf32(nd[0]);
            if (s >= k) {
                nd[0] >>= k;
                return 0;
            }
            nd[0] >>= s;
            k -= s;
        }
    }
    if (k > 18) {
        if (strfmt_fp(sf) == strfmt_fp(STRFMT_T_FP_F)) {
            stop1 = (uint32_t)(63 - (int32_t)strfmt_prec(sf) / 9);
        } else {
            int32_t floorlog2 = (int32_t)(ndhi * 29 + (uint32_t)neo_bsr32(nd[ndhi]) - k);
            int32_t floorlog10 = (int32_t)(floorlog2 * 0.30102999566398114);
            stop1 = (uint32_t)(62 + (floorlog10 - (int32_t)strfmt_prec(sf)) / 9);
            stop2 = (uint32_t)(61 + (int32_t)ndhi - (int32_t)strfmt_prec(sf) / 8);
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
            if (carry) {
                ndlo = (ndlo - 1) & 0x3f;
                nd[ndlo] = carry;
            }
            if (!nd[ndhi]) {
                ndhi = (ndhi - 1) & 0x3f;
                stop2--;
            }
        } else if (!nd[ndhi]) {
            if (ndhi != ndlo) {
                ndhi = (ndhi - 1) & 0x3f;
                stop2--;
            } else { return ndlo; }
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
        if (carry) {
            ndlo = (ndlo - 1) & 0x3f;
            nd[ndlo] = carry;
        }
    }
    return ndlo;
}

/* Add m*10^e to nd (assumes ndlo <= e/9 <= ndhi and 0 <= m <= 9). */
static uint32_t nd_add_m10e(uint32_t *nd, uint32_t ndhi, uint8_t m, int32_t e) {
    neo_dassert(nd != NULL, "Invalid arguments");
    uint32_t i, carry;
    if (e >= 0) {
        i = (uint32_t)e / 9;
        carry = m * (ndigits_dec_threshold[e - (int32_t)i * 9] + 1);
    } else {
        int32_t f = (e - 8) / 9;
        i = (uint32_t)(64 + f);
        carry = m * (ndigits_dec_threshold[e - f * 9] + 1);
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
    neo_dassert(p != NULL, "Invalid arguments");
    uint32_t v = u / 10000, w;
    u -= v * 10000;
    w = v / 10000;
    v -= w * 10000;
    *p++ = (uint8_t)('0' + w);
    wint_r(v, 23, 1000)
    wint_r(v, 12, 100)
    wint_r(v, 10, 10)
    *p++ = (uint8_t)('0' + v);
    wint_r(u, 23, 1000)
    wint_r(u, 12, 100)
    wint_r(u, 10, 10)
    *p++ = (uint8_t)('0' + u);
    return p;
}

#undef wint_r

/* Test whether two "nd" values are equal in their most significant digits. */
static bool nd_similar(uint32_t *nd, uint32_t ndhi, uint32_t *ref, size_t hilen, size_t prec) {
    neo_dassert(nd != NULL && ref != NULL, "Invalid arguments");
    uint8_t nd9[9], ref9[9];
    if (hilen <= prec) {
        if (neo_unlikely(nd[ndhi] != *ref)) { return 0; }
        prec -= hilen;
        --ref;
        ndhi = (ndhi - 1) & 0x3f;
        if (prec >= 9) {
            if (neo_unlikely(nd[ndhi] != *ref)) { return 0; }
            prec -= 9;
            --ref;
            ndhi = (ndhi - 1) & 0x3f;
        }
    } else {
        prec -= hilen - 9;
    }
    neo_assert(prec < 9, "Bad precision: %zu", prec);
    fmt_wuint9(nd9, nd[ndhi]);
    fmt_wuint9(ref9, *ref);
    return !memcmp(nd9, ref9, prec) && (nd9[prec] < '5') == (ref9[prec] < '5');
}

static uint8_t *fmt_f64(uint8_t *p, neo_float_t x, sfmt_t sf) {
    neo_dassert(p != NULL, "Invalid arguments");
    size_t width = strfmt_width(sf), prec = strfmt_prec(sf), len;
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
    } else if (strfmt_fp(sf) == strfmt_fp(STRFMT_T_FP_A)) {
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
            uint32_t shift = t.ru32x2.hi ? 20 - (uint32_t)neo_bsr32(t.ru32x2.hi) : 52 -
                                                                                   (uint32_t)neo_bsr32(t.ru32x2.lo);
            e = -1022 - (int32_t)shift;
            t.ru64 <<= shift;
        }
        /* abs(n) == t.u64 * 2^(e - 52) */
        /* If n != 0, bit 52 of t.u64 is set, and is the highest set bit. */
        if ((int32_t)prec < 0) {
            /* Default precision: use smallest precision giving exact result. */
            prec = t.ru32x2.lo ? 13 - (uint32_t)neo_bsf32(t.ru32x2.lo) / 4 : 5 - (uint32_t)neo_bsf32(
                    t.ru32x2.hi | 0x100000) / 4;
        } else if (prec < 13) {
            /* Precision is sufficiently low as to maybe require rounding. */
            t.ru64 += (((uint64_t)1) << (51 - prec * 4));
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
            if (prec < 13) { t.ru64 >>= (52 - prec * 4); }
            else { while (prec > 13) { p[prec--] = '0'; }}
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
        if (strfmt_fp(sf) == strfmt_fp(STRFMT_T_FP_G)) {
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
                goto load_t_lo;
                rescale_failed:
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
                neo_assert(0 <= eidx && eidx < 128, "Bad eidx: %" PRIi32, eidx);
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
                            if (ndlo == ndhi) {
                                prec = 0;
                                break;
                            }
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
                p[1] = '.';
                p += 2;
                prec -= (size_t)(q - p);
                p = q; /* Account for digits already emitted. */
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
                    while ((int32_t)prec > 0) {
                        *p++ = '0';
                        prec--;
                    }
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
                            if (ndlo == 63) {
                                prec = 0;
                                break;
                            }
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
                    while ((int32_t)prec > 0) {
                        *p++ = '0';
                        prec--;
                    }
                    p += (int32_t)prec;
                }
            }
        }
    }
    if ((sf & STRFMT_F_LEFT))
        while (width-- > len) {
            *p++ = ' ';
        }
    return p;
}

uint8_t *neo_fmt_float(uint8_t *p, neo_float_t x) {
    neo_dassert(p != NULL, "Invalid arguments");
    return fmt_f64(p, x, STRFMT_G14);
}

uint8_t *neo_fmt_ptr(uint8_t *p, const void *v) {
    neo_dassert(p != NULL, "Invalid arguments");
    ptrdiff_t x = (ptrdiff_t)v;
    size_t n = 2 + 2 * sizeof(ptrdiff_t);
    if (!x) {
        memcpy(p, "null", 4);
        return p + 4;
    }
    n = 2 + 2 * 4 + ((x >> 32) ? 2 + 2 * ((size_t)neo_bsr32((uint32_t)(x >> 32)) >> 3) : 0);
    p[0] = '0';
    p[1] = 'x';
    for (size_t i = n - 1; i >= 2; --i, x >>= 4) {
        p[i] = ((const uint8_t *)"0123456789abcdef")[(x & 15)];
    }
    return p + n;
}

/*
** NEO bundles a modified version or rpmalloc.
** Maybe consider mimalloc too - benchmarks required.
*/

/* rpmalloc.h  -  Memory allocator  -  Public Domain  -  2016 Mattias Jansson
 *
 * This library provides a cross-platform lock free thread caching malloc implementation in C11.
 * The latest source code is always available at
 *
 * https://github.com/mjansson/rpmalloc
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#define SMALL_GRANULARITY 16
#define SMALL_GRANULARITY_SHIFT 4
#define SMALL_CLASS_COUNT 65
#define SMALL_SIZE_LIMIT (SMALL_GRANULARITY*(SMALL_CLASS_COUNT-1))
#define MEDIUM_GRANULARITY 512
#define MEDIUM_GRANULARITY_SHIFT 9
#define MEDIUM_CLASS_COUNT 61
#define SIZE_CLASS_COUNT (SMALL_CLASS_COUNT+MEDIUM_CLASS_COUNT)
#define LARGE_CLASS_COUNT 63
#define MEDIUM_SIZE_LIMIT (SMALL_SIZE_LIMIT+(MEDIUM_GRANULARITY*MEDIUM_CLASS_COUNT))
#define LARGE_SIZE_LIMIT ((LARGE_CLASS_COUNT*_memory_span_size)-SPAN_HEADER_SIZE)
#define SPAN_HEADER_SIZE 128
#define MAX_THREAD_SPAN_CACHE 400
#define THREAD_SPAN_CACHE_TRANSFER 64
#define MAX_THREAD_SPAN_LARGE_CACHE 100
#define THREAD_SPAN_LARGE_CACHE_TRANSFER 6
#define MAX_ALLOC_SIZE (((size_t)-1)-_memory_span_size)
#define poff(ptr, ofs) (void *)((uint8_t *)(ptr)+(ptrdiff_t)(ofs))
#define pdelta(first, second) (ptrdiff_t)((const uint8_t *)(first)-(const uint8_t *)(second))
#define INVALID_POINTER ((void *)((uintptr_t)-1))
#define SIZE_CLASS_LARGE SIZE_CLASS_COUNT
#define SIZE_CLASS_HUGE ((uint32_t)-1)
#define SPAN_FLAG_MASTER 1U
#define SPAN_FLAG_SUBSPAN 2U
#define SPAN_FLAG_ALIGNED_BLOCKS 4U
#define SPAN_FLAG_UNMAPPED_MASTER 8U

neo_static_assert((SMALL_GRANULARITY & (SMALL_GRANULARITY - 1)) == 0 && "Small granularity must be power of two");
neo_static_assert((SPAN_HEADER_SIZE & (SPAN_HEADER_SIZE - 1)) == 0 && "Span header size must be power of two");

#if defined(__clang__) || defined(__GNUC__)
# define MEM_ALLOCATOR
# if (defined(__clang_major__) && (__clang_major__ < 4)) || (defined(__GNUC__) && defined(NEO_ALLOC_ENABLE_PRELOAD) && NEO_ALLOC_ENABLE_PRELOAD)
# define MEM_ATTRIB_MALLOC
# define MEM_ATTRIB_ALLOC_SIZE(size)
# define MEM_ATTRIB_ALLOC_SIZE2(count, size)
# else
# define MEM_ATTRIB_MALLOC __attribute__((__malloc__))
# define MEM_ATTRIB_ALLOC_SIZE(size) __attribute__((alloc_size(size)))
# define MEM_ATTRIB_ALLOC_SIZE2(count, size)  __attribute__((alloc_size(count, size)))
# endif
# define MEM_CDECL
#elif defined(_MSC_VER)
# define MEM_ALLOCATOR __declspec(allocator) __declspec(restrict)
# define MEM_ATTRIB_MALLOC
# define MEM_ATTRIB_ALLOC_SIZE(size)
# define MEM_ATTRIB_ALLOC_SIZE2(count,size)
# define MEM_CDECL __cdecl
#else
# define MEM_ALLOCATOR
# define MEM_ATTRIB_MALLOC
# define MEM_ATTRIB_ALLOC_SIZE(size)
# define MEM_ATTRIB_ALLOC_SIZE2(count,size)
# define MEM_CDECL
#endif

#ifndef MEM_CONFIGURABLE
#define MEM_CONFIGURABLE 0
#endif

#ifndef MEM_FIRST_CLASS_HEAPS
#define MEM_FIRST_CLASS_HEAPS 0
#endif

#define MEM_NO_PRESERVE    1
#define MEM_GROW_OR_FAIL   2

#if MEM_FIRST_CLASS_HEAPS

typedef struct heap_t memheap_t;

memheap_t *memheap_acquire(void);
void memheap_release(memheap_t *heap);
MEM_ALLOCATOR void *memheap_alloc(memheap_t *heap, size_t size) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE(2);
MEM_ALLOCATOR void *memheap_aligned_alloc(memheap_t *heap, size_t alignment, size_t size) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE(3);
MEM_ALLOCATOR void *memheap_calloc(memheap_t *heap, size_t num, size_t size) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE2(2, 3);
MEM_ALLOCATOR void *memheap_aligned_calloc(memheap_t *heap, size_t alignment, size_t num, size_t size) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE2(2, 3);
MEM_ALLOCATOR void *memheap_realloc(memheap_t *heap, void *ptr, size_t size, unsigned flags) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE(3);
MEM_ALLOCATOR void *memheap_aligned_realloc(memheap_t *heap, void *ptr, size_t alignment, size_t size, unsigned flags) MEM_ATTRIB_MALLOC MEM_ATTRIB_ALLOC_SIZE(4);
void memheap_free(memheap_t *heap, void *ptr);
void memheap_free_all(memheap_t *heap);
void memheap_thread_set_current(memheap_t *heap);
memheap_t *memget_heap_for_ptr(void *ptr);

#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-macros"
#pragma clang diagnostic ignored "-Wunused-function"
#if __has_warning("-Wreserved-identifier")
#pragma clang diagnostic ignored "-Wreserved-identifier"
#endif
#if __has_warning("-Wstatic-in-inline")
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#endif
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-macros"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if !defined(__has_builtin)
#define __has_builtin(b) 0
#endif

#if defined(__GNUC__) || defined(__clang__)

#if __has_builtin(__builtin_memcpy_inline)
#define _memmemcpy_const(x, y, s) __builtin_memcpy_inline(x, y, s)
#else
#define _memmemcpy_const(x, y, s)                                            \
    do {                                                        \
        neo_static_assert(__builtin_choose_expr(__builtin_constant_p(s), 1, 0) && "Len must be a constant integer");    \
        memcpy(x, y, s);                                            \
    } while (0)
#endif

#if __has_builtin(__builtin_memset_inline)
#define _memmemset_const(x, y, s) __builtin_memset_inline(x, y, s)
#else
#define _memmemset_const(x, y, s)                                            \
    do {                                                        \
        neo_static_assert(__builtin_choose_expr(__builtin_constant_p(s), 1, 0) && "Len must be a constant integer");    \
        memset(x, y, s);                                            \
    } while (0)
#endif
#else
#define _memmemcpy_const(x, y, s) memcpy(x, y, s)
#define _memmemset_const(x, y, s) memset(x, y, s)
#endif

#if __has_builtin(__builtin_assume)
#define memassume(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
#define memassume(cond)                                                \
    do {                                                        \
        if (!__builtin_expect(cond, 0))                                        \
            __builtin_unreachable();                                    \
    } while (0)
#elif defined(_MSC_VER)
#define memassume(cond) __assume(cond)
#else
#define memassume(cond) 0
#endif

#if NEO_ALLOC_ENABLE_UNMAP && !NEO_ALLOC_ENABLE_GLOBAL_CACHE
#error Must use global cache if unmap is disabled
#endif

#if NEO_ALLOC_ENABLE_UNMAP
#undef NEO_ALLOC_ENABLE_UNLIMITED_CACHE
#define NEO_ALLOC_ENABLE_UNLIMITED_CACHE 1
#endif

#if !NEO_ALLOC_ENABLE_GLOBAL_CACHE
#undef NEO_ALLOC_ENABLE_UNLIMITED_CACHE
#define NEO_ALLOC_ENABLE_UNLIMITED_CACHE 0
#endif

#if !NEO_ALLOC_ENABLE_THREAD_CACHE
#undef NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE
#define NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE 0
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
#  define PLATFORM_WINDOWS 1
#  define PLATFORM_POSIX 0
#else
#  define PLATFORM_WINDOWS 0
#  define PLATFORM_POSIX 1
#endif

#if PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  if NEO_ALLOC_ENABLE_VALIDATION
#    include <intsafe.h>
#  endif
#else

#  include <unistd.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <time.h>

#  if defined(__linux__) || defined(__ANDROID__)

#    include <sys/prctl.h>

#    if !defined(PR_SET_VMA)
#      define PR_SET_VMA 0x53564d41
#      define PR_SET_VMA_ANON_NAME 0
#    endif
#  endif
#  if defined(__APPLE__)
#    include <TargetConditionals.h>
#    if !TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#    include <mach/mach_vm.h>
#    include <mach/vm_statistics.h>
#    endif
#    include <pthread.h>
#  endif
#  if defined(__HAIKU__) || defined(__TINYC__)
#    include <pthread.h>
#  endif
#endif

#include <stdint.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
#include <fibersapi.h>
static DWORD fls_key;
#endif

#if PLATFORM_POSIX

#  include <sys/mman.h>
#  include <sched.h>

#  ifdef __FreeBSD__
#    include <sys/sysctl.h>
#    define MAP_HUGETLB MAP_ALIGNED_SUPER
#    ifndef PROT_MAX
#      define PROT_MAX(f) 0
#    endif
#  else
#    define PROT_MAX(f) 0
#  endif
#  ifdef __sun
extern int madvise(caddr_t, size_t, int);
#  endif
#  ifndef MAP_UNINITIALIZED
#    define MAP_UNINITIALIZED 0
#  endif
#endif

#include <errno.h>

#if NEO_ALLOC_ENABLE_STATS
#  include <stdio.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)

typedef volatile int32_t atomic32_t;
typedef volatile int64_t atomic64_t;
typedef volatile void *atomicptr_t;

static NEO_AINLINE int32_t atomic_load32(atomic32_t* src) { return *src; }
static NEO_AINLINE void    atomic_store32(atomic32_t* dst, int32_t val) { *dst = val; }
static NEO_AINLINE int32_t atomic_incr32(atomic32_t* val) { return (int32_t)InterlockedIncrement(val); }
static NEO_AINLINE int32_t atomic_decr32(atomic32_t* val) { return (int32_t)InterlockedDecrement(val); }
static NEO_AINLINE int32_t atomic_add32(atomic32_t* val, int32_t add) { return (int32_t)InterlockedExchangeAdd(val, add) + add; }
static NEO_AINLINE int     atomic_cas32_acquire(atomic32_t* dst, int32_t val, int32_t ref) { return (InterlockedCompareExchange(dst, val, ref) == ref) ? 1 : 0; }
static NEO_AINLINE void    atomic_store32_release(atomic32_t* dst, int32_t val) { *dst = val; }
static NEO_AINLINE int64_t atomic_load64(atomic64_t* src) { return *src; }
static NEO_AINLINE int64_t atomic_add64(atomic64_t* val, int64_t add) { return (int64_t)InterlockedExchangeAdd64(val, add) + add; }
static NEO_AINLINE void*   atomic_load_ptr(atomicptr_t* src) { return (void*)*src; }
static NEO_AINLINE void    atomic_store_ptr(atomicptr_t* dst, void* val) { *dst = val; }
static NEO_AINLINE void    atomic_store_ptr_release(atomicptr_t* dst, void* val) { *dst = val; }
static NEO_AINLINE void*   atomic_exchange_ptr_acquire(atomicptr_t* dst, void* val) { return (void*)InterlockedExchangePointer((void* volatile*)dst, val); }
static NEO_AINLINE int     atomic_cas_ptr(atomicptr_t* dst, void* val, void* ref) { return (InterlockedCompareExchangePointer((void* volatile*)dst, val, ref) == ref) ? 1 : 0; }

#else

#include <stdatomic.h>

typedef volatile _Atomic(int32_t)
atomic32_t;
typedef volatile _Atomic(int64_t)
atomic64_t;
typedef volatile _Atomic(
void*)
atomicptr_t;

static NEO_AINLINE int32_t atomic_load32(atomic32_t *src) { return atomic_load_explicit(src, memory_order_relaxed); }

static NEO_AINLINE void atomic_store32(atomic32_t *dst, int32_t val) {
    atomic_store_explicit(dst, val, memory_order_relaxed);
}

static NEO_AINLINE int32_t atomic_incr32(atomic32_t *val) {
    return atomic_fetch_add_explicit(val, 1, memory_order_relaxed) + 1;
}

static NEO_AINLINE int32_t atomic_decr32(atomic32_t *val) {
    return atomic_fetch_add_explicit(val, -1, memory_order_relaxed) - 1;
}

static NEO_AINLINE int32_t atomic_add32(atomic32_t *val, int32_t add) {
    return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add;
}

static NEO_AINLINE int atomic_cas32_acquire(atomic32_t *dst, int32_t val, int32_t ref) {
    return atomic_compare_exchange_weak_explicit(dst, &ref, val, memory_order_acquire, memory_order_relaxed);
}

static NEO_AINLINE void atomic_store32_release(atomic32_t *dst, int32_t val) {
    atomic_store_explicit(dst, val, memory_order_release);
}

static NEO_AINLINE int64_t atomic_load64(atomic64_t *val) { return atomic_load_explicit(val, memory_order_relaxed); }

static NEO_AINLINE int64_t atomic_add64(atomic64_t *val, int64_t add) {
    return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add;
}

static NEO_AINLINE void *atomic_load_ptr(atomicptr_t *src) { return atomic_load_explicit(src, memory_order_relaxed); }

static NEO_AINLINE void atomic_store_ptr(atomicptr_t *dst, void *val) {
    atomic_store_explicit(dst, val, memory_order_relaxed);
}

static NEO_AINLINE void atomic_store_ptr_release(atomicptr_t *dst, void *val) {
    atomic_store_explicit(dst, val, memory_order_release);
}

static NEO_AINLINE void *atomic_exchange_ptr_acquire(atomicptr_t *dst, void *val) {
    return atomic_exchange_explicit(dst, val, memory_order_acquire);
}

static NEO_AINLINE int atomic_cas_ptr(atomicptr_t *dst, void *val, void *ref) {
    return atomic_compare_exchange_weak_explicit(dst, &ref, val, memory_order_relaxed, memory_order_relaxed);
}

#endif

int meminitialize(void);

int meminitialize_config(const neo_alloc_config_t *config);

const neo_alloc_config_t *memconfig(void);

void memfinalize(void);

void memthread_initialize(void);

void memthread_finalize(int release_caches);

void memthread_collect(void);

int memis_thread_initialized(void);

void memthread_statistics(neo_alloc_thread_stats_t *stats);

void memglobal_statistics(neo_alloc_global_stats_t *stats);

void memdump_statistics(void *file);

size_t memusable_size(void *ptr);

#if NEO_ALLOC_ENABLE_STATS
#  define _memstat_inc(counter) atomic_incr32(counter)
#  define _memstat_dec(counter) atomic_decr32(counter)
#  define _memstat_add(counter, value) atomic_add32(counter, (int32_t)(value))
#  define _memstat_add64(counter, value) atomic_add64(counter, (int64_t)(value))
#  define _memstat_add_peak(counter, value, peak) do { int32_t _cur_count = atomic_add32(counter, (int32_t)(value)); if (_cur_count > (peak)) peak = _cur_count; } while (0)
#  define _memstat_sub(counter, value) atomic_add32(counter, -(int32_t)(value))
#  define _memstat_inc_alloc(heap, class_idx) do { \
    int32_t alloc_current = atomic_incr32(&heap->size_class_use[class_idx].alloc_current); \
    if (alloc_current > heap->size_class_use[class_idx].alloc_peak) \
        heap->size_class_use[class_idx].alloc_peak = alloc_current; \
    atomic_incr32(&heap->size_class_use[class_idx].alloc_total); \
} while(0)
#  define _memstat_inc_free(heap, class_idx) do { \
    atomic_decr32(&heap->size_class_use[class_idx].alloc_current); \
    atomic_incr32(&heap->size_class_use[class_idx].free_total); \
} while(0)
#else
#  define _memstat_inc(counter) do {} while(0)
#  define _memstat_dec(counter) do {} while(0)
#  define _memstat_add(counter, value) do {} while(0)
#  define _memstat_add64(counter, value) do {} while(0)
#  define _memstat_add_peak(counter, value, peak) do {} while (0)
#  define _memstat_sub(counter, value) do {} while(0)
#  define _memstat_inc_alloc(heap, class_idx) do {} while(0)
#  define _memstat_inc_free(heap, class_idx) do {} while(0)
#endif

typedef struct heap_t heap_t;
typedef struct span_t span_t;
typedef struct span_list_t span_list_t;
typedef struct span_active_t span_active_t;
typedef struct size_class_t size_class_t;
typedef struct global_cache_t global_cache_t;

#if NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE || NEO_ALLOC_ENABLE_STATS
struct span_use_t {
    atomic32_t current;
    atomic32_t high;
#if NEO_ALLOC_ENABLE_STATS
    atomic32_t spans_deferred;
    atomic32_t spans_to_global;
    atomic32_t spans_from_global;
    atomic32_t spans_to_cache;
    atomic32_t spans_from_cache;
    atomic32_t spans_to_reserved;
    atomic32_t spans_from_reserved;
    atomic32_t spans_map_calls;
#endif
};
typedef struct span_use_t span_use_t;
#endif

#if NEO_ALLOC_ENABLE_STATS
struct size_class_use_t {
    atomic32_t alloc_current;
    int32_t alloc_peak;
    atomic32_t alloc_total;
    atomic32_t free_total;
    atomic32_t spans_current;
    int32_t spans_peak;
    atomic32_t spans_to_cache;
    atomic32_t spans_from_cache;
    atomic32_t spans_from_reserved;
    atomic32_t spans_map_calls;
    int32_t unused;
};
typedef struct size_class_use_t size_class_use_t;
#endif

struct span_t {
    void *free_list;
    uint32_t block_count;
    uint32_t size_class;
    uint32_t free_list_limit;
    uint32_t used_count;
    atomicptr_t free_list_deferred;
    uint32_t list_size;
    uint32_t block_size;
    uint32_t flags;
    uint32_t span_count;
    uint32_t total_spans;
    uint32_t offset_from_master;
    atomic32_t remaining_spans;
    uint32_t align_offset;
    heap_t *heap;
    span_t *next;
    span_t *prev;
};

neo_static_assert(sizeof(span_t) <= SPAN_HEADER_SIZE && "span size mismatch");

struct span_cache_t {
    size_t count;
    span_t *span[MAX_THREAD_SPAN_CACHE];
};
typedef struct span_cache_t span_cache_t;

struct span_large_cache_t {
    size_t count;
    span_t *span[MAX_THREAD_SPAN_LARGE_CACHE];
};
typedef struct span_large_cache_t span_large_cache_t;

struct heap_size_class_t {
    void *free_list;
    span_t *partial_span;
    span_t *cache;
};
typedef struct heap_size_class_t heap_size_class_t;

struct heap_t {
    uintptr_t owner_thread;
    heap_size_class_t size_class[SIZE_CLASS_COUNT];
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    span_cache_t span_cache;
#endif
    atomicptr_t span_free_deferred;
    size_t full_span_count;
    span_t *span_reserve;
    span_t *span_reserve_master;
    uint32_t spans_reserved;
    atomic32_t child_count;
    heap_t *next_heap;
    heap_t *next_orphan;
    int32_t id;
    int finalize;
    heap_t *master_heap;
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    span_large_cache_t span_large_cache[LARGE_CLASS_COUNT - 1];
#endif
#if MEM_FIRST_CLASS_HEAPS
    span_t*      full_span[SIZE_CLASS_COUNT];
span_t*      large_huge_span;
#endif
#if NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE || NEO_ALLOC_ENABLE_STATS
    span_use_t span_use[LARGE_CLASS_COUNT];
#endif
#if NEO_ALLOC_ENABLE_STATS
    size_class_use_t size_class_use[SIZE_CLASS_COUNT + 1];
    atomic64_t   thread_to_global;
    atomic64_t   global_to_thread;
#endif
};

struct size_class_t {
    uint32_t block_size;
    uint16_t block_count;
    uint16_t class_idx;
};

neo_static_assert(sizeof(size_class_t) == 8 && "Size class size mismatch");

struct global_cache_t {
    atomic32_t lock;
    uint32_t count;
#if NEO_ALLOC_ENABLE_STATS
    size_t insert_count;
    size_t extract_count;
#endif
    span_t *span[NEO_ALLOC_GLOBAL_CACHE_MULTIPLIER * MAX_THREAD_SPAN_CACHE];
    span_t *overflow;
};

#define _memory_default_span_size (64 * 1024)
#define _memory_default_span_size_shift 16
#define _memory_default_span_mask (~((uintptr_t)(_memory_span_size - 1)))

static int _meminitialized;
static uintptr_t _memmain_thread_id;
static neo_alloc_config_t _memory_config;
static size_t _memory_page_size;
static size_t _memory_page_size_shift;
static size_t _memory_map_granularity;
#if MEM_CONFIGURABLE
static size_t _memory_span_size;
static size_t _memory_span_size_shift;
static uintptr_t _memory_span_mask;
#else
#define _memory_span_size _memory_default_span_size
#define _memory_span_size_shift _memory_default_span_size_shift
#define _memory_span_mask _memory_default_span_mask
#endif
static size_t _memory_span_map_count;
static size_t _memory_heap_reserve_count;
static size_class_t _memory_size_class[SIZE_CLASS_COUNT];
static size_t _memory_medium_size_limit;
static atomic32_t _memory_heap_id;
static int _memory_huge_pages;
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
static global_cache_t _memory_span_cache[LARGE_CLASS_COUNT];
#endif
static span_t *_memory_global_reserve;
static size_t _memory_global_reserve_count;
static span_t *_memory_global_reserve_master;
static heap_t *_memory_heaps[NEO_ALLOC_HEAP_ARRAY_SIZE];
static atomic32_t _memory_global_lock;
static heap_t *_memory_orphan_heaps;
#if MEM_FIRST_CLASS_HEAPS
static heap_t* _memory_first_class_orphan_heaps;
#endif
#if NEO_ALLOC_ENABLE_STATS
static atomic64_t _allocation_counter;
static atomic64_t _deallocation_counter;
static atomic32_t _memory_active_heaps;
static atomic32_t _mapped_pages;
static int32_t _mapped_pages_peak;
static atomic32_t _master_spans;
static atomic32_t _unmapped_master_spans;
static atomic32_t _mapped_total;
static atomic32_t _unmapped_total;
static atomic32_t _mapped_pages_os;
static atomic32_t _huge_pages_current;
static int32_t _huge_pages_peak;
#endif

#if ((defined(__APPLE__) || defined(__HAIKU__)) && NEO_ALLOC_ENABLE_PRELOAD) || defined(__TINYC__)
static pthread_key_t _memory_thread_heap;
#else
#  ifdef _MSC_VER
#    define _Thread_local __declspec(thread)
#    define TLS_MODEL
#  else
#    ifndef __HAIKU__
#      define TLS_MODEL __attribute__((tls_model("initial-exec")))
#    else
#      define TLS_MODEL
#    endif
#    if !defined(__clang__) && defined(__GNUC__)
#      define _Thread_local __thread
#    endif
#  endif
static _Thread_local heap_t *_memory_thread_heap TLS_MODEL;
#endif

static inline heap_t *
get_thread_heap_raw(void) {
#if (defined(__APPLE__) || defined(__HAIKU__)) && NEO_ALLOC_ENABLE_PRELOAD
    return pthread_getspecific(_memory_thread_heap);
#else
    return _memory_thread_heap;
#endif
}

static inline heap_t *
get_thread_heap(void) {
    heap_t *heap = get_thread_heap_raw();
#if NEO_ALLOC_ENABLE_PRELOAD
    if (neo_likely(heap != 0))
        return heap;
    meminitialize();
    return get_thread_heap_raw();
#else
    return heap;
#endif
}

static inline uintptr_t get_thread_id(void) {
#if defined(_WIN32)
    return (uintptr_t)((void *)NtCurrentTeb());
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__CYGWIN__)
    uintptr_t tid;
#  if defined(__i386__)
    __asm__("movl %%gs:0, %0" : "=r" (tid) : : );
#  elif defined(__x86_64__)
#    if defined(__MACH__)
    __asm__("movq %%gs:0, %0" : "=r" (tid) : : );
#    else
    __asm__("movq %%fs:0, %0" : "=r" (tid) : : );
#    endif
#  elif defined(__arm__)
    __asm__ volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r" (tid));
#  elif defined(__aarch64__)
#    if defined(__MACH__)
        __asm__ volatile ("mrs %0, tpidrro_el0" : "=r" (tid));
#    else
    __asm__ volatile ("mrs %0, tpidr_el0" : "=r" (tid));
#    endif
#  else
#    error This platform needs implementation of get_thread_id()
#  endif
    return tid;
#else
#    error This platform needs implementation of get_thread_id()
#endif
}

static void set_thread_heap(heap_t *heap) {
#if ((defined(__APPLE__) || defined(__HAIKU__)) && NEO_ALLOC_ENABLE_PRELOAD) || defined(__TINYC__)
    pthread_setspecific(_memory_thread_heap, heap);
#else
    _memory_thread_heap = heap;
#endif
    if (heap)
        heap->owner_thread = get_thread_id();
}

extern void memset_main_thread(void);

void memset_main_thread(void) {
    _memmain_thread_id = get_thread_id();
}

static void _memspin(void) {
#if defined(_MSC_VER)
    _mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pause":: : "memory");
#elif defined(__aarch64__) || (defined(__arm__) && __ARM_ARCH >= 7)
    __asm__ volatile("yield" ::: "memory");
#elif defined(__powerpc__) || defined(__powerpc64__)
            __asm__ volatile("or 27,27,27");
#elif defined(__sparc__)
    __asm__ volatile("rd %ccr, %g0 \n\trd %ccr, %g0 \n\trd %ccr, %g0");
#else
    struct timespec ts = {0};
    nanosleep(&ts, 0);
#endif
}

#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
static void NTAPI
_memthread_destructor(void* value) {
#if NEO_ALLOC_ENABLE_OVERRIDE
        if (get_thread_id() == _memmain_thread_id) { return; }
#endif
    if (value) { memthread_finalize(1); }
}
#endif

static void _memset_name(void *address, size_t size) {
#if defined(__linux__) || defined(__ANDROID__)
    const char *name = _memory_huge_pages ? _memory_config.huge_page_name : _memory_config.page_name;
    if (address == MAP_FAILED || !name)
        return;
    (void)prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (uintptr_t)address, size, (uintptr_t)name);
#else
    (void)sizeof(size);
    (void)sizeof(address);
#endif
}

static void *_memmmap(size_t size, size_t *offset) {
    neo_assert(!(size % _memory_page_size), "Invalid mmap size");
    neo_assert(size >= _memory_page_size, "Invalid mmap size");
    void *address = _memory_config.memory_map(size, offset);
    if (neo_likely(address != 0)) {
        _memstat_add_peak(&_mapped_pages, (size >> _memory_page_size_shift), _mapped_pages_peak);
        _memstat_add(&_mapped_total, (size >> _memory_page_size_shift));
    }
    return address;
}

static void _memunmap(void *address, size_t size, size_t offset, size_t release) {
    neo_assert(!release || (release >= size), "Invalid unmap size");
    neo_assert(!release || (release >= _memory_page_size), "Invalid unmap size");
    if (release) {
        neo_assert(!(release % _memory_page_size), "Invalid unmap size");
        _memstat_sub(&_mapped_pages, (release >> _memory_page_size_shift));
        _memstat_add(&_unmapped_total, (release >> _memory_page_size_shift));
    }
    _memory_config.memory_unmap(address, size, offset, release);
}

static void *_memmmap_os(size_t size, size_t *offset) {
    size_t padding = ((size >= _memory_span_size) && (_memory_span_size > _memory_map_granularity)) ? _memory_span_size : 0;
    neo_assert(size >= _memory_page_size, "Invalid mmap size");
#if PLATFORM_WINDOWS
    void* ptr = VirtualAlloc(0, size + padding, (_memory_huge_pages ? MEM_LARGE_PAGES : 0) | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ptr) {
        if (_memory_config.map_fail_callback) {
            if (_memory_config.map_fail_callback(size + padding))
                return _memmmap_os(size, offset);
        } else {
            neo_assert(ptr, "Failed to map virtual memory block");
        }
        return 0;
    }
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_UNINITIALIZED;
#  if defined(__APPLE__) && !TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    int fd = (int)VM_MAKE_TAG(240U);
    if (_memory_huge_pages)
        fd |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
    void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, flags, fd, 0);
#  elif defined(MAP_HUGETLB)
    void *ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE | PROT_MAX(PROT_READ | PROT_WRITE),
                     (_memory_huge_pages ? MAP_HUGETLB : 0) | flags, -1, 0);
#    if defined(MADV_HUGEPAGE)
    if ((ptr == MAP_FAILED || !ptr) && _memory_huge_pages) {
        ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (ptr && ptr != MAP_FAILED) {
            int prm = madvise(ptr, size + padding, MADV_HUGEPAGE);
            (void)prm;
            neo_assert((prm == 0), "Failed to promote the page to THP");
        }
    }
#    endif
    _memset_name(ptr, size + padding);
#  elif defined(MAP_ALIGNED)
    const size_t align = (sizeof(size_t) * 8) - (size_t)(__builtin_clzl(size - 1));
    void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, (_memory_huge_pages ? MAP_ALIGNED(align) : 0) | flags, -1, 0);
#  elif defined(MAP_ALIGN)
    caddr_t base = (_memory_huge_pages ? (caddr_t)(4 << 20) : 0);
    void* ptr = mmap(base, size + padding, PROT_READ | PROT_WRITE, (_memory_huge_pages ? MAP_ALIGN : 0) | flags, -1, 0);
#  else
    void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, flags, -1, 0);
#  endif
    if ((ptr == MAP_FAILED) || !ptr) {
        if (_memory_config.map_fail_callback) {
            if (_memory_config.map_fail_callback(size + padding))
                return _memmmap_os(size, offset);
        } else if (errno != ENOMEM) {
            neo_assert((ptr != MAP_FAILED) && ptr, "Failed to map virtual memory block");
        }
        return 0;
    }
#endif
    _memstat_add(&_mapped_pages_os, (int32_t)((size + padding) >> _memory_page_size_shift));
    if (padding) {
        size_t final_padding = padding - ((uintptr_t)ptr & ~_memory_span_mask);
        neo_assert(final_padding <= _memory_span_size, "Internal failure in padding");
        neo_assert(final_padding <= padding, "Internal failure in padding");
        neo_assert(!(final_padding % 8), "Internal failure in padding");
        ptr = poff(ptr, final_padding);
        *offset = final_padding >> 3;
    }
    neo_assert((size < _memory_span_size) || !((uintptr_t)ptr & ~_memory_span_mask), "Internal failure in padding");
    return ptr;
}

static void _memunmap_os(void *address, size_t size, size_t offset, size_t release) {
    neo_assert(release || (offset == 0), "Invalid unmap size");
    neo_assert(!release || (release >= _memory_page_size), "Invalid unmap size");
    neo_assert(size >= _memory_page_size, "Invalid unmap size");
    if (release && offset) {
        offset <<= 3;
        address = poff(address, -(int32_t)offset);
        if ((release >= _memory_span_size) && (_memory_span_size > _memory_map_granularity)) {
            release += _memory_span_size;
        }
    }
#if !NEO_ALLOC_ENABLE_UNMAP
#if PLATFORM_WINDOWS
    if (!VirtualFree(address, release ? 0 : size, release ? MEM_RELEASE : MEM_DECOMMIT)) {
        neo_assert(0, "Failed to unmap virtual memory block");
    }
#else
    if (release) {
        if (munmap(address, release)) {
            neo_assert(0, "Failed to unmap virtual memory block");
        }
    } else {
#if defined(MADV_FREE_REUSABLE)
        int ret;
        while ((ret = madvise(address, size, MADV_FREE_REUSABLE)) == -1 && (errno == EAGAIN))
            errno = 0;
        if ((ret == -1) && (errno != 0)) {
#elif defined(MADV_DONTNEED)
        if (madvise(address, size, MADV_DONTNEED)) {
#elif defined(MADV_PAGEOUT)
            if (madvise(address, size, MADV_PAGEOUT)) {
#elif defined(MADV_FREE)
        if (madvise(address, size, MADV_FREE)) {
#else
        if (posix_madvise(address, size, POSIX_MADV_DONTNEED)) {
#endif
            neo_assert(0, "Failed to madvise virtual memory block as free");
        }
    }
#endif
#endif
    if (release)
        _memstat_sub(&_mapped_pages_os, release >> _memory_page_size_shift);
}

static void _memspan_mark_as_subspan_unless_master(span_t *master, span_t *subspan, size_t span_count);

static span_t *_memglobal_get_reserved_spans(size_t span_count) {
    span_t *span = _memory_global_reserve;
    _memspan_mark_as_subspan_unless_master(_memory_global_reserve_master, span, span_count);
    _memory_global_reserve_count -= span_count;
    if (_memory_global_reserve_count)
        _memory_global_reserve = (span_t *)poff(span, span_count << _memory_span_size_shift);
    else
        _memory_global_reserve = 0;
    return span;
}

static void _memglobal_set_reserved_spans(span_t *master, span_t *reserve, size_t reserve_span_count) {
    _memory_global_reserve_master = master;
    _memory_global_reserve_count = reserve_span_count;
    _memory_global_reserve = reserve;
}

static void _memspan_double_link_list_add(span_t **head, span_t *span) {
    if (*head)
        (*head)->prev = span;
    span->next = *head;
    *head = span;
}

static void _memspan_double_link_list_pop_head(span_t **head, span_t *span) {
    neo_assert(*head == span, "Linked list corrupted");
    span = *head;
    *head = span->next;
}

static void _memspan_double_link_list_remove(span_t **head, span_t *span) {
    neo_assert(*head, "Linked list corrupted");
    if (*head == span) {
        *head = span->next;
    } else {
        span_t *next_span = span->next;
        span_t *prev_span = span->prev;
        prev_span->next = next_span;
        if (neo_likely(next_span != 0))
            next_span->prev = prev_span;
    }
}

static void _memheap_cache_insert(heap_t *heap, span_t *span);

static void _memheap_finalize(heap_t *heap);

static void _memheap_set_reserved_spans(heap_t *heap, span_t *master, span_t *reserve, size_t reserve_span_count);

static void _memspan_mark_as_subspan_unless_master(span_t *master, span_t *subspan, size_t span_count) {
    neo_assert((subspan != master) || (subspan->flags & SPAN_FLAG_MASTER), "Span master pointer and/or flag mismatch");
    if (subspan != master) {
        subspan->flags = SPAN_FLAG_SUBSPAN;
        subspan->offset_from_master = (uint32_t)((uintptr_t)pdelta(subspan, master) >> _memory_span_size_shift);
        subspan->align_offset = 0;
    }
    subspan->span_count = (uint32_t)span_count;
}

static span_t *_memspan_map_from_reserve(heap_t *heap, size_t span_count) {
    span_t *span = heap->span_reserve;
    heap->span_reserve = (span_t *)poff(span, span_count * _memory_span_size);
    heap->spans_reserved -= (uint32_t)span_count;

    _memspan_mark_as_subspan_unless_master(heap->span_reserve_master, span, span_count);
    if (span_count <= LARGE_CLASS_COUNT)
        _memstat_inc(&heap->span_use[span_count - 1].spans_from_reserved);

    return span;
}

static size_t _memspan_align_count(size_t span_count) {
    size_t request_count = (span_count > _memory_span_map_count) ? span_count : _memory_span_map_count;
    if ((_memory_page_size > _memory_span_size) && ((request_count * _memory_span_size) % _memory_page_size))
        request_count += _memory_span_map_count - (request_count % _memory_span_map_count);
    return request_count;
}

static void _memspan_initialize(span_t *span, size_t total_span_count, size_t span_count, size_t align_offset) {
    span->total_spans = (uint32_t)total_span_count;
    span->span_count = (uint32_t)span_count;
    span->align_offset = (uint32_t)align_offset;
    span->flags = SPAN_FLAG_MASTER;
    atomic_store32(&span->remaining_spans, (int32_t)total_span_count);
}

static void _memspan_unmap(span_t *span);

static span_t *_memspan_map_aligned_count(heap_t *heap, size_t span_count) {
    size_t aligned_span_count = _memspan_align_count(span_count);
    size_t align_offset = 0;
    span_t *span = (span_t *)_memmmap(aligned_span_count * _memory_span_size, &align_offset);
    if (!span)
        return 0;
    _memspan_initialize(span, aligned_span_count, span_count, align_offset);
    _memstat_inc(&_master_spans);
    if (span_count <= LARGE_CLASS_COUNT)
        _memstat_inc(&heap->span_use[span_count - 1].spans_map_calls);
    if (aligned_span_count > span_count) {
        span_t *reserved_spans = (span_t *)poff(span, span_count * _memory_span_size);
        size_t reserved_count = aligned_span_count - span_count;
        if (heap->spans_reserved) {
            _memspan_mark_as_subspan_unless_master(heap->span_reserve_master, heap->span_reserve, heap->spans_reserved);
            _memheap_cache_insert(heap, heap->span_reserve);
        }
        if (reserved_count > _memory_heap_reserve_count) {
            neo_assert(atomic_load32(&_memory_global_lock) == 1, "Global spin lock not held as expected");
            size_t remain_count = reserved_count - _memory_heap_reserve_count;
            reserved_count = _memory_heap_reserve_count;
            span_t *remain_span = (span_t *)poff(reserved_spans, reserved_count * _memory_span_size);
            if (_memory_global_reserve) {
                _memspan_mark_as_subspan_unless_master(_memory_global_reserve_master, _memory_global_reserve,
                                                       _memory_global_reserve_count);
                _memspan_unmap(_memory_global_reserve);
            }
            _memglobal_set_reserved_spans(span, remain_span, remain_count);
        }
        _memheap_set_reserved_spans(heap, span, reserved_spans, reserved_count);
    }
    return span;
}

static span_t *_memspan_map(heap_t *heap, size_t span_count) {
    if (span_count <= heap->spans_reserved)
        return _memspan_map_from_reserve(heap, span_count);
    span_t *span = 0;
    int use_global_reserve =
            (_memory_page_size > _memory_span_size) || (_memory_span_map_count > _memory_heap_reserve_count);
    if (use_global_reserve) {
        while (!atomic_cas32_acquire(&_memory_global_lock, 1, 0))
            _memspin();
        if (_memory_global_reserve_count >= span_count) {
            size_t reserve_count = (!heap->spans_reserved ? _memory_heap_reserve_count : span_count);
            if (_memory_global_reserve_count < reserve_count)
                reserve_count = _memory_global_reserve_count;
            span = _memglobal_get_reserved_spans(reserve_count);
            if (span) {
                if (reserve_count > span_count) {
                    span_t *reserved_span = (span_t *)poff(span, span_count << _memory_span_size_shift);
                    _memheap_set_reserved_spans(heap, _memory_global_reserve_master, reserved_span,
                                                reserve_count - span_count);
                }
                span->span_count = (uint32_t)span_count;
            }
        }
    }
    if (!span)
        span = _memspan_map_aligned_count(heap, span_count);
    if (use_global_reserve)
        atomic_store32_release(&_memory_global_lock, 0);
    return span;
}

static void _memspan_unmap(span_t *span) {
    neo_assert((span->flags & SPAN_FLAG_MASTER) || (span->flags & SPAN_FLAG_SUBSPAN), "Span flag corrupted");
    neo_assert(!(span->flags & SPAN_FLAG_MASTER) || !(span->flags & SPAN_FLAG_SUBSPAN), "Span flag corrupted");

    int is_master = !!(span->flags & SPAN_FLAG_MASTER);
    span_t *master = is_master ? span : ((span_t *)poff(span,
                                                        -(intptr_t)((uintptr_t)span->offset_from_master *
                                                                    _memory_span_size)));
    neo_assert(is_master || (span->flags & SPAN_FLAG_SUBSPAN), "Span flag corrupted");
    neo_assert(master->flags & SPAN_FLAG_MASTER, "Span flag corrupted");

    size_t span_count = span->span_count;
    if (!is_master) {
        neo_assert(span->align_offset == 0, "Span align offset corrupted");
        if (_memory_span_size >= _memory_page_size)
            _memunmap(span, span_count * _memory_span_size, 0, 0);
    } else {
        span->flags |= SPAN_FLAG_MASTER | SPAN_FLAG_SUBSPAN | SPAN_FLAG_UNMAPPED_MASTER;
        _memstat_add(&_unmapped_master_spans, 1);
    }

    if (atomic_add32(&master->remaining_spans, -(int32_t)span_count) <= 0) {
        neo_assert(!!(master->flags & SPAN_FLAG_MASTER) && !!(master->flags & SPAN_FLAG_SUBSPAN),
                   "Span flag corrupted");
        size_t unmap_count = master->span_count;
        if (_memory_span_size < _memory_page_size)
            unmap_count = master->total_spans;
        _memstat_sub(&_master_spans, 1);
        _memstat_sub(&_unmapped_master_spans, 1);
        _memunmap(master, unmap_count * _memory_span_size, master->align_offset,
                  (size_t)master->total_spans * _memory_span_size);
    }
}

static void _memspan_release_to_cache(heap_t *heap, span_t *span) {
    neo_assert(heap == span->heap, "Span heap pointer corrupted");
    neo_assert(span->size_class < SIZE_CLASS_COUNT, "Invalid span size class");
    neo_assert(span->span_count == 1, "Invalid span count");
#if NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE || NEO_ALLOC_ENABLE_STATS
    atomic_decr32(&heap->span_use[0].current);
#endif
    _memstat_dec(&heap->size_class_use[span->size_class].spans_current);
    if (!heap->finalize) {
        _memstat_inc(&heap->span_use[0].spans_to_cache);
        _memstat_inc(&heap->size_class_use[span->size_class].spans_to_cache);
        if (heap->size_class[span->size_class].cache)
            _memheap_cache_insert(heap, heap->size_class[span->size_class].cache);
        heap->size_class[span->size_class].cache = span;
    } else {
        _memspan_unmap(span);
    }
}

static uint32_t free_list_partial_init(void **list, void **first_block, void *page_start, void *block_start, uint32_t block_count,
                                       uint32_t block_size) {
    neo_assert(block_count, "Internal failure");
    *first_block = block_start;
    if (block_count > 1) {
        void *free_block = poff(block_start, block_size);
        void *block_end = poff(block_start, (size_t)block_size * block_count);
        if (block_size < (_memory_page_size >> 1)) {
            void *page_end = poff(page_start, _memory_page_size);
            if (page_end < block_end)
                block_end = page_end;
        }
        *list = free_block;
        block_count = 2;
        void *next_block = poff(free_block, block_size);
        while (next_block < block_end) {
            *((void **)free_block) = next_block;
            free_block = next_block;
            ++block_count;
            next_block = poff(next_block, block_size);
        }
        *((void **)free_block) = 0;
    } else {
        *list = 0;
    }
    return block_count;
}

static void *_memspan_initialize_new(heap_t *heap, heap_size_class_t *heap_size_class, span_t *span, uint32_t class_idx) {
    neo_assert(span->span_count == 1, "Internal failure");
    size_class_t *size_class = _memory_size_class + class_idx;
    span->size_class = class_idx;
    span->heap = heap;
    span->flags &= ~SPAN_FLAG_ALIGNED_BLOCKS;
    span->block_size = size_class->block_size;
    span->block_count = size_class->block_count;
    span->free_list = 0;
    span->list_size = 0;
    atomic_store_ptr_release(&span->free_list_deferred, 0);

    void *block;
    span->free_list_limit = free_list_partial_init(&heap_size_class->free_list, &block,
                                                   span, poff(span, SPAN_HEADER_SIZE),
                                                   size_class->block_count, size_class->block_size);
    if (span->free_list_limit < span->block_count) {
        _memspan_double_link_list_add(&heap_size_class->partial_span, span);
        span->used_count = span->free_list_limit;
    } else {
#if MEM_FIRST_CLASS_HEAPS
        _memspan_double_link_list_add(&heap->full_span[class_idx], span);
#endif
        ++heap->full_span_count;
        span->used_count = span->block_count;
    }
    return block;
}

static void _memspan_extract_free_list_deferred(span_t *span) {
    do {
        span->free_list = atomic_exchange_ptr_acquire(&span->free_list_deferred, INVALID_POINTER);
    } while (span->free_list == INVALID_POINTER);
    span->used_count -= span->list_size;
    span->list_size = 0;
    atomic_store_ptr_release(&span->free_list_deferred, 0);
}

static int _memspan_is_fully_utilized(span_t *span) {
    neo_assert(span->free_list_limit <= span->block_count, "Span free list corrupted");
    return !span->free_list && (span->free_list_limit >= span->block_count);
}

static int _memspan_finalize(heap_t *heap, size_t iclass, span_t *span, span_t **list_head) {
    void *free_list = heap->size_class[iclass].free_list;
    span_t *class_span = (span_t *)((uintptr_t)free_list & _memory_span_mask);
    if (span == class_span) {
        void *block = span->free_list;
        void *last_block = 0;
        while (block) {
            last_block = block;
            block = *((void **)block);
        }
        uint32_t free_count = 0;
        block = free_list;
        while (block) {
            ++free_count;
            block = *((void **)block);
        }
        if (last_block) {
            *((void **)last_block) = free_list;
        } else {
            span->free_list = free_list;
        }
        heap->size_class[iclass].free_list = 0;
        span->used_count -= free_count;
    }
    neo_assert(span->list_size == span->used_count, "Memory leak detected");
    if (span->list_size == span->used_count) {
        _memstat_dec(&heap->span_use[0].current);
        _memstat_dec(&heap->size_class_use[iclass].spans_current);
        if (list_head)
            _memspan_double_link_list_remove(list_head, span);
        _memspan_unmap(span);
        return 1;
    }
    return 0;
}

#if NEO_ALLOC_ENABLE_GLOBAL_CACHE

static void _memglobal_cache_finalize(global_cache_t *cache) {
    while (!atomic_cas32_acquire(&cache->lock, 1, 0)) {
        _memspin();
    }
    for (size_t ispan = 0; ispan < cache->count; ++ispan) {
        _memspan_unmap(cache->span[ispan]);
    }
    cache->count = 0;
    while (cache->overflow) {
        span_t *span = cache->overflow;
        cache->overflow = span->next;
        _memspan_unmap(span);
    }
    atomic_store32_release(&cache->lock, 0);
}

static void _memglobal_cache_insert_spans(span_t **span, size_t span_count, size_t count) {
    const size_t cache_limit = (span_count == 1) ?
                               NEO_ALLOC_GLOBAL_CACHE_MULTIPLIER * MAX_THREAD_SPAN_CACHE :
                               NEO_ALLOC_GLOBAL_CACHE_MULTIPLIER * (MAX_THREAD_SPAN_LARGE_CACHE - (span_count >> 1));
    global_cache_t *cache = &_memory_span_cache[span_count - 1];
    size_t insert_count = count;
    while (!atomic_cas32_acquire(&cache->lock, 1, 0)) {
        _memspin();
    }
#if NEO_ALLOC_ENABLE_STATS
    cache->insert_count += count;
#endif
    if ((cache->count + insert_count) > cache_limit) {
        insert_count = cache_limit - cache->count;
    }
    memcpy(cache->span + cache->count, span, sizeof(span_t *) * insert_count);
    cache->count += (uint32_t)insert_count;
#if NEO_ALLOC_ENABLE_UNLIMITED_CACHE
    while (insert_count < count) {
#else
        while ((_memory_page_size > _memory_span_size) && (insert_count < count)) {
#endif
        span_t *current_span = span[insert_count++];
        current_span->next = cache->overflow;
        cache->overflow = current_span;
    }
    atomic_store32_release(&cache->lock, 0);
    span_t *keep = 0;
    for (size_t ispan = insert_count; ispan < count; ++ispan) {
        span_t *current_span = span[ispan];
        if ((current_span->flags & SPAN_FLAG_MASTER) &&
            (atomic_load32(&current_span->remaining_spans) > (int32_t)current_span->span_count)) {
            current_span->next = keep;
            keep = current_span;
        } else {
            _memspan_unmap(current_span);
        }
    }

    if (keep) {
        while (!atomic_cas32_acquire(&cache->lock, 1, 0))
            _memspin();

        size_t islot = 0;
        while (keep) {
            for (; islot < cache->count; ++islot) {
                span_t *current_span = cache->span[islot];
                if (!(current_span->flags & SPAN_FLAG_MASTER) || ((current_span->flags & SPAN_FLAG_MASTER) &&
                                                                  (atomic_load32(&current_span->remaining_spans) <=
                                                                   (int32_t)current_span->span_count))) {
                    _memspan_unmap(current_span);
                    cache->span[islot] = keep;
                    break;
                }
            }
            if (islot == cache->count)
                break;
            keep = keep->next;
        }

        if (keep) {
            span_t *tail = keep;
            while (tail->next)
                tail = tail->next;
            tail->next = cache->overflow;
            cache->overflow = keep;
        }

        atomic_store32_release(&cache->lock, 0);
    }
}

static size_t _memglobal_cache_extract_spans(span_t **span, size_t span_count, size_t count) {
    global_cache_t *cache = &_memory_span_cache[span_count - 1];

    size_t extract_count = 0;
    while (!atomic_cas32_acquire(&cache->lock, 1, 0))
        _memspin();

#if NEO_ALLOC_ENABLE_STATS
    cache->extract_count += count;
#endif
    size_t want = count - extract_count;
    if (want > cache->count)
        want = cache->count;

    memcpy(span + extract_count, cache->span + (cache->count - want), sizeof(span_t *) * want);
    cache->count -= (uint32_t)want;
    extract_count += want;

    while ((extract_count < count) && cache->overflow) {
        span_t *current_span = cache->overflow;
        span[extract_count++] = current_span;
        cache->overflow = current_span->next;
    }

#if NEO_ALLOC_ENABLE_ASSERTS
    for (size_t ispan = 0; ispan < extract_count; ++ispan) {
        neo_assert(span[ispan]->span_count == span_count, "Global cache span count mismatch");
    }
#endif

    atomic_store32_release(&cache->lock, 0);

    return extract_count;
}

#endif

static void _memdeallocate_huge(span_t *);

static void _memheap_set_reserved_spans(heap_t *heap, span_t *master, span_t *reserve, size_t reserve_span_count) {
    heap->span_reserve_master = master;
    heap->span_reserve = reserve;
    heap->spans_reserved = (uint32_t)reserve_span_count;
}

static void _memheap_cache_adopt_deferred(heap_t *heap, span_t **single_span) {
    span_t *span = (span_t *)((void *)atomic_exchange_ptr_acquire(&heap->span_free_deferred, 0));
    while (span) {
        span_t *next_span = (span_t *)span->free_list;
        neo_assert(span->heap == heap, "Span heap pointer corrupted");
        if (neo_likely(span->size_class < SIZE_CLASS_COUNT)) {
            neo_assert(heap->full_span_count, "Heap span counter corrupted");
            --heap->full_span_count;
            _memstat_dec(&heap->span_use[0].spans_deferred);
#if MEM_FIRST_CLASS_HEAPS
            _memspan_double_link_list_remove(&heap->full_span[span->size_class], span);
#endif
            _memstat_dec(&heap->span_use[0].current);
            _memstat_dec(&heap->size_class_use[span->size_class].spans_current);
            if (single_span && !*single_span)
                *single_span = span;
            else
                _memheap_cache_insert(heap, span);
        } else {
            if (span->size_class == SIZE_CLASS_HUGE) {
                _memdeallocate_huge(span);
            } else {
                neo_assert(span->size_class == SIZE_CLASS_LARGE, "Span size class invalid");
                neo_assert(heap->full_span_count, "Heap span counter corrupted");
                --heap->full_span_count;
#if MEM_FIRST_CLASS_HEAPS
                _memspan_double_link_list_remove(&heap->large_huge_span, span);
#endif
                uint32_t idx = span->span_count - 1;
                _memstat_dec(&heap->span_use[idx].spans_deferred);
                _memstat_dec(&heap->span_use[idx].current);
                if (!idx && single_span && !*single_span)
                    *single_span = span;
                else
                    _memheap_cache_insert(heap, span);
            }
        }
        span = next_span;
    }
}

static void _memheap_unmap(heap_t *heap) {
    if (!heap->master_heap) {
        if ((heap->finalize > 1) && !atomic_load32(&heap->child_count)) {
            span_t *span = (span_t *)((uintptr_t)heap & _memory_span_mask);
            _memspan_unmap(span);
        }
    } else {
        if (atomic_decr32(&heap->master_heap->child_count) == 0) {
            _memheap_unmap(heap->master_heap);
        }
    }
}

static void _memheap_global_finalize(heap_t *heap) {
    if (heap->finalize++ > 1) {
        --heap->finalize;
        return;
    }

    _memheap_finalize(heap);

#if NEO_ALLOC_ENABLE_THREAD_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        span_cache_t *span_cache;
        if (!iclass)
            span_cache = &heap->span_cache;
        else
            span_cache = (span_cache_t *)(heap->span_large_cache + (iclass - 1));
        for (size_t ispan = 0; ispan < span_cache->count; ++ispan)
            _memspan_unmap(span_cache->span[ispan]);
        span_cache->count = 0;
    }
#endif

    if (heap->full_span_count) {
        --heap->finalize;
        return;
    }

    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        if (heap->size_class[iclass].free_list || heap->size_class[iclass].partial_span) {
            --heap->finalize;
            return;
        }
    }
    size_t list_idx = (size_t)heap->id % NEO_ALLOC_HEAP_ARRAY_SIZE;
    heap_t *list_heap = _memory_heaps[list_idx];
    if (list_heap == heap) {
        _memory_heaps[list_idx] = heap->next_heap;
    } else {
        while (list_heap->next_heap != heap)
            list_heap = list_heap->next_heap;
        list_heap->next_heap = heap->next_heap;
    }

    _memheap_unmap(heap);
}

static void _memheap_cache_insert(heap_t *heap, span_t *span) {
    if (neo_unlikely(heap->finalize != 0)) {
        _memspan_unmap(span);
        _memheap_global_finalize(heap);
        return;
    }
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    size_t span_count = span->span_count;
    _memstat_inc(&heap->span_use[span_count - 1].spans_to_cache);
    if (span_count == 1) {
        span_cache_t *span_cache = &heap->span_cache;
        span_cache->span[span_cache->count++] = span;
        if (span_cache->count == MAX_THREAD_SPAN_CACHE) {
            const size_t remain_count = MAX_THREAD_SPAN_CACHE - THREAD_SPAN_CACHE_TRANSFER;
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
            _memstat_add64(&heap->thread_to_global, THREAD_SPAN_CACHE_TRANSFER * _memory_span_size);
            _memstat_add(&heap->span_use[span_count - 1].spans_to_global, THREAD_SPAN_CACHE_TRANSFER);
            _memglobal_cache_insert_spans(span_cache->span + remain_count, span_count, THREAD_SPAN_CACHE_TRANSFER);
#else
            for (size_t ispan = 0; ispan < THREAD_SPAN_CACHE_TRANSFER; ++ispan)
                _memspan_unmap(span_cache->span[remain_count + ispan]);
#endif
            span_cache->count = remain_count;
        }
    } else {
        size_t cache_idx = span_count - 2;
        span_large_cache_t *span_cache = heap->span_large_cache + cache_idx;
        span_cache->span[span_cache->count++] = span;
        const size_t cache_limit = (MAX_THREAD_SPAN_LARGE_CACHE - (span_count >> 1));
        if (span_cache->count == cache_limit) {
            const size_t transfer_limit = 2 + (cache_limit >> 2);
            const size_t transfer_count = (THREAD_SPAN_LARGE_CACHE_TRANSFER <= transfer_limit
                                           ? THREAD_SPAN_LARGE_CACHE_TRANSFER : transfer_limit);
            const size_t remain_count = cache_limit - transfer_count;
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
            _memstat_add64(&heap->thread_to_global, transfer_count * span_count * _memory_span_size);
            _memstat_add(&heap->span_use[span_count - 1].spans_to_global, transfer_count);
            _memglobal_cache_insert_spans(span_cache->span + remain_count, span_count, transfer_count);
#else
            for (size_t ispan = 0; ispan < transfer_count; ++ispan)
                _memspan_unmap(span_cache->span[remain_count + ispan]);
#endif
            span_cache->count = remain_count;
        }
    }
#else
    (void)sizeof(heap);
    _memspan_unmap(span);
#endif
}

static span_t *_memheap_thread_cache_extract(heap_t *heap, size_t span_count) {
    span_t *span = 0;
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    span_cache_t *span_cache;
    if (span_count == 1)
        span_cache = &heap->span_cache;
    else
        span_cache = (span_cache_t *)(heap->span_large_cache + (span_count - 2));
    if (span_cache->count) {
        _memstat_inc(&heap->span_use[span_count - 1].spans_from_cache);
        return span_cache->span[--span_cache->count];
    }
#endif
    return span;
}

static span_t *_memheap_thread_cache_deferred_extract(heap_t *heap, size_t span_count) {
    span_t *span = 0;
    if (span_count == 1) {
        _memheap_cache_adopt_deferred(heap, &span);
    } else {
        _memheap_cache_adopt_deferred(heap, 0);
        span = _memheap_thread_cache_extract(heap, span_count);
    }
    return span;
}

static span_t *_memheap_reserved_extract(heap_t *heap, size_t span_count) {
    if (heap->spans_reserved >= span_count)
        return _memspan_map(heap, span_count);
    return 0;
}

static span_t *_memheap_global_cache_extract(heap_t *heap, size_t span_count) {
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    span_cache_t *span_cache;
    size_t wanted_count;
    if (span_count == 1) {
        span_cache = &heap->span_cache;
        wanted_count = THREAD_SPAN_CACHE_TRANSFER;
    } else {
        span_cache = (span_cache_t *)(heap->span_large_cache + (span_count - 2));
        wanted_count = THREAD_SPAN_LARGE_CACHE_TRANSFER;
    }
    span_cache->count = _memglobal_cache_extract_spans(span_cache->span, span_count, wanted_count);
    if (span_cache->count) {
        _memstat_add64(&heap->global_to_thread, span_count * span_cache->count * _memory_span_size);
        _memstat_add(&heap->span_use[span_count - 1].spans_from_global, span_cache->count);
        return span_cache->span[--span_cache->count];
    }
#else
    span_t* span = 0;
    size_t count = _memglobal_cache_extract_spans(&span, span_count, 1);
    if (count) {
        _memstat_add64(&heap->global_to_thread, span_count * count * _memory_span_size);
        _memstat_add(&heap->span_use[span_count - 1].spans_from_global, count);
        return span;
    }
#endif
#endif
    (void)sizeof(heap);
    (void)sizeof(span_count);
    return 0;
}

static void _meminc_span_statistics(heap_t *heap, size_t span_count, uint32_t class_idx) {
    (void)sizeof(heap);
    (void)sizeof(span_count);
    (void)sizeof(class_idx);
#if NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE || NEO_ALLOC_ENABLE_STATS
    uint32_t idx = (uint32_t)span_count - 1;
    uint32_t current_count = (uint32_t)atomic_incr32(&heap->span_use[idx].current);
    if (current_count > (uint32_t)atomic_load32(&heap->span_use[idx].high))
        atomic_store32(&heap->span_use[idx].high, (int32_t)current_count);
    _memstat_add_peak(&heap->size_class_use[class_idx].spans_current, 1, heap->size_class_use[class_idx].spans_peak);
#endif
}

static span_t *_memheap_extract_new_span(heap_t *heap, heap_size_class_t *heap_size_class, size_t span_count, uint32_t class_idx) {
    span_t *span;
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    if (heap_size_class && heap_size_class->cache) {
        span = heap_size_class->cache;
        heap_size_class->cache = (heap->span_cache.count ? heap->span_cache.span[--heap->span_cache.count] : 0);
        _meminc_span_statistics(heap, span_count, class_idx);
        return span;
    }
#endif
    (void)sizeof(class_idx);
    size_t base_span_count = span_count;
    size_t limit_span_count = (span_count > 2) ? (span_count + (span_count >> 1)) : span_count;
    if (limit_span_count > LARGE_CLASS_COUNT)
        limit_span_count = LARGE_CLASS_COUNT;
    do {
        span = _memheap_thread_cache_extract(heap, span_count);
        if (neo_likely(span != 0)) {
            _memstat_inc(&heap->size_class_use[class_idx].spans_from_cache);
            _meminc_span_statistics(heap, span_count, class_idx);
            return span;
        }
        span = _memheap_thread_cache_deferred_extract(heap, span_count);
        if (neo_likely(span != 0)) {
            _memstat_inc(&heap->size_class_use[class_idx].spans_from_cache);
            _meminc_span_statistics(heap, span_count, class_idx);
            return span;
        }
        span = _memheap_reserved_extract(heap, span_count);
        if (neo_likely(span != 0)) {
            _memstat_inc(&heap->size_class_use[class_idx].spans_from_reserved);
            _meminc_span_statistics(heap, span_count, class_idx);
            return span;
        }
        span = _memheap_global_cache_extract(heap, span_count);
        if (neo_likely(span != 0)) {
            _memstat_inc(&heap->size_class_use[class_idx].spans_from_cache);
            _meminc_span_statistics(heap, span_count, class_idx);
            return span;
        }
        ++span_count;
    } while (span_count <= limit_span_count);
    span = _memspan_map(heap, base_span_count);
    _meminc_span_statistics(heap, base_span_count, class_idx);
    _memstat_inc(&heap->size_class_use[class_idx].spans_map_calls);
    return span;
}

static void _memheap_initialize(heap_t *heap) {
    _memmemset_const(heap, 0, sizeof(heap_t));
    heap->id = 1 + atomic_incr32(&_memory_heap_id);

    size_t list_idx = (size_t)heap->id % NEO_ALLOC_HEAP_ARRAY_SIZE;
    heap->next_heap = _memory_heaps[list_idx];
    _memory_heaps[list_idx] = heap;
}

static void _memheap_orphan(heap_t *heap, int first_class) {
    heap->owner_thread = (uintptr_t)-1;
#if MEM_FIRST_CLASS_HEAPS
    heap_t** heap_list = (first_class ? &_memory_first_class_orphan_heaps : &_memory_orphan_heaps);
#else
    (void)sizeof(first_class);
    heap_t **heap_list = &_memory_orphan_heaps;
#endif
    heap->next_orphan = *heap_list;
    *heap_list = heap;
}

static heap_t *
_memheap_allocate_new(void) {
    size_t heap_size = sizeof(heap_t);
    size_t aligned_heap_size = 16 * ((heap_size + 15) / 16);
    size_t request_heap_count = 16;
    size_t heap_span_count =
            ((aligned_heap_size * request_heap_count) + sizeof(span_t) + _memory_span_size - 1) / _memory_span_size;
    size_t block_size = _memory_span_size * heap_span_count;
    size_t span_count = heap_span_count;
    span_t *span = 0;
    if (_memory_global_reserve_count >= heap_span_count) {
        span = _memglobal_get_reserved_spans(heap_span_count);
    }
    if (!span) {
        if (_memory_page_size > block_size) {
            span_count = _memory_page_size / _memory_span_size;
            block_size = _memory_page_size;
            size_t possible_heap_count = (block_size - sizeof(span_t)) / aligned_heap_size;
            if (possible_heap_count >= (request_heap_count * 16))
                request_heap_count *= 16;
            else if (possible_heap_count < request_heap_count)
                request_heap_count = possible_heap_count;
            heap_span_count = ((aligned_heap_size * request_heap_count) + sizeof(span_t) + _memory_span_size - 1) /
                              _memory_span_size;
        }

        size_t align_offset = 0;
        span = (span_t *)_memmmap(block_size, &align_offset);
        if (!span)
            return 0;

        _memstat_inc(&_master_spans);
        _memspan_initialize(span, span_count, heap_span_count, align_offset);
    }

    size_t remain_size = _memory_span_size - sizeof(span_t);
    heap_t *heap = (heap_t *)poff(span, sizeof(span_t));
    _memheap_initialize(heap);

    size_t num_heaps = remain_size / aligned_heap_size;
    if (num_heaps < request_heap_count)
        num_heaps = request_heap_count;
    atomic_store32(&heap->child_count, (int32_t)num_heaps - 1);
    heap_t *extra_heap = (heap_t *)poff(heap, aligned_heap_size);
    while (num_heaps > 1) {
        _memheap_initialize(extra_heap);
        extra_heap->master_heap = heap;
        _memheap_orphan(extra_heap, 1);
        extra_heap = (heap_t *)poff(extra_heap, aligned_heap_size);
        --num_heaps;
    }

    if (span_count > heap_span_count) {
        size_t remain_count = span_count - heap_span_count;
        size_t reserve_count = (remain_count > _memory_heap_reserve_count ? _memory_heap_reserve_count : remain_count);
        span_t *remain_span = (span_t *)poff(span, heap_span_count * _memory_span_size);
        _memheap_set_reserved_spans(heap, span, remain_span, reserve_count);

        if (remain_count > reserve_count) {
            remain_span = (span_t *)poff(remain_span, reserve_count * _memory_span_size);
            reserve_count = remain_count - reserve_count;
            _memglobal_set_reserved_spans(span, remain_span, reserve_count);
        }
    }

    return heap;
}

static heap_t *
_memheap_extract_orphan(heap_t **heap_list) {
    heap_t *heap = *heap_list;
    *heap_list = (heap ? heap->next_orphan : 0);
    return heap;
}

static heap_t *
_memheap_allocate(int first_class) {
    heap_t *heap = 0;
    while (!atomic_cas32_acquire(&_memory_global_lock, 1, 0))
        _memspin();
    if (first_class == 0)
        heap = _memheap_extract_orphan(&_memory_orphan_heaps);
#if MEM_FIRST_CLASS_HEAPS
    if (!heap)
        heap = _memheap_extract_orphan(&_memory_first_class_orphan_heaps);
#endif
    if (!heap)
        heap = _memheap_allocate_new();
    atomic_store32_release(&_memory_global_lock, 0);
    if (heap)
        _memheap_cache_adopt_deferred(heap, 0);
    return heap;
}

static void _memheap_release(void *heapptr, int first_class, int release_cache) {
    heap_t *heap = (heap_t *)heapptr;
    if (!heap)
        return;
    _memheap_cache_adopt_deferred(heap, 0);
    if (release_cache || heap->finalize) {
#if NEO_ALLOC_ENABLE_THREAD_CACHE
        for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
            span_cache_t *span_cache;
            if (!iclass)
                span_cache = &heap->span_cache;
            else
                span_cache = (span_cache_t *)(heap->span_large_cache + (iclass - 1));
            if (!span_cache->count)
                continue;
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
            if (heap->finalize) {
                for (size_t ispan = 0; ispan < span_cache->count; ++ispan)
                    _memspan_unmap(span_cache->span[ispan]);
            } else {
                _memstat_add64(&heap->thread_to_global, span_cache->count * (iclass + 1) * _memory_span_size);
                _memstat_add(&heap->span_use[iclass].spans_to_global, span_cache->count);
                _memglobal_cache_insert_spans(span_cache->span, iclass + 1, span_cache->count);
            }
#else
            for (size_t ispan = 0; ispan < span_cache->count; ++ispan)
                _memspan_unmap(span_cache->span[ispan]);
#endif
            span_cache->count = 0;
        }
#endif
    }

    if (get_thread_heap_raw() == heap)
        set_thread_heap(0);

#if NEO_ALLOC_ENABLE_STATS
    atomic_decr32(&_memory_active_heaps);
    neo_assert(atomic_load32(&_memory_active_heaps) >= 0, "Still active heaps during finalization");
#endif

    if (get_thread_id() != _memmain_thread_id) {
        while (!atomic_cas32_acquire(&_memory_global_lock, 1, 0))
            _memspin();
    }
    _memheap_orphan(heap, first_class);
    atomic_store32_release(&_memory_global_lock, 0);
}

static void _memheap_release_raw(void *heapptr, int release_cache) {
    _memheap_release(heapptr, 0, release_cache);
}

static void _memheap_release_raw_fc(void *heapptr) {
    _memheap_release_raw(heapptr, 1);
}

static void _memheap_finalize(heap_t *heap) {
    if (heap->spans_reserved) {
        span_t *span = _memspan_map(heap, heap->spans_reserved);
        _memspan_unmap(span);
        heap->spans_reserved = 0;
    }

    _memheap_cache_adopt_deferred(heap, 0);

    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        if (heap->size_class[iclass].cache)
            _memspan_unmap(heap->size_class[iclass].cache);
        heap->size_class[iclass].cache = 0;
        span_t *span = heap->size_class[iclass].partial_span;
        while (span) {
            span_t *next = span->next;
            _memspan_finalize(heap, iclass, span, &heap->size_class[iclass].partial_span);
            span = next;
        }
        if (heap->size_class[iclass].free_list) {
            span_t *class_span = (span_t *)((uintptr_t)heap->size_class[iclass].free_list & _memory_span_mask);
            span_t **list = 0;
#if MEM_FIRST_CLASS_HEAPS
            list = &heap->full_span[iclass];
#endif
            --heap->full_span_count;
            if (!_memspan_finalize(heap, iclass, class_span, list)) {
                if (list)
                    _memspan_double_link_list_remove(list, class_span);
                _memspan_double_link_list_add(&heap->size_class[iclass].partial_span, class_span);
            }
        }
    }

#if NEO_ALLOC_ENABLE_THREAD_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        span_cache_t *span_cache;
        if (!iclass)
            span_cache = &heap->span_cache;
        else
            span_cache = (span_cache_t *)(heap->span_large_cache + (iclass - 1));
        for (size_t ispan = 0; ispan < span_cache->count; ++ispan)
            _memspan_unmap(span_cache->span[ispan]);
        span_cache->count = 0;
    }
#endif
    neo_assert(!atomic_load_ptr(&heap->span_free_deferred), "Heaps still active during finalization");
}

static void *free_list_pop(void **list) {
    void *block = *list;
    *list = *((void **)block);
    return block;
}

static void *_memallocate_from_heap_fallback(heap_t *heap, heap_size_class_t *heap_size_class, uint32_t class_idx) {
    span_t *span = heap_size_class->partial_span;
    memassume(heap != 0);
    if (neo_likely(span != 0)) {
        neo_assert(span->block_count == _memory_size_class[span->size_class].block_count, "Span block count corrupted");
        neo_assert(!_memspan_is_fully_utilized(span), "Internal failure");
        void *block;
        if (span->free_list) {
            block = free_list_pop(&span->free_list);
            heap_size_class->free_list = span->free_list;
            span->free_list = 0;
        } else {
            void *block_start = poff(span,
                                     SPAN_HEADER_SIZE + ((size_t)span->free_list_limit * span->block_size));
            span->free_list_limit += free_list_partial_init(&heap_size_class->free_list, &block,
                                                            (void *)((uintptr_t)block_start &
                                                                     ~(_memory_page_size - 1)), block_start,
                                                            span->block_count - span->free_list_limit,
                                                            span->block_size);
        }
        neo_assert(span->free_list_limit <= span->block_count, "Span block count corrupted");
        span->used_count = span->free_list_limit;

        if (atomic_load_ptr(&span->free_list_deferred))
            _memspan_extract_free_list_deferred(span);

        if (!_memspan_is_fully_utilized(span))
            return block;

        _memspan_double_link_list_pop_head(&heap_size_class->partial_span, span);
#if MEM_FIRST_CLASS_HEAPS
        _memspan_double_link_list_add(&heap->full_span[class_idx], span);
#endif
        ++heap->full_span_count;
        return block;
    }

    span = _memheap_extract_new_span(heap, heap_size_class, 1, class_idx);
    if (neo_likely(span != 0)) {
        return _memspan_initialize_new(heap, heap_size_class, span, class_idx);
    }

    return 0;
}

static void *_memallocate_small(heap_t *heap, size_t size) {
    neo_assert(heap, "No thread heap");
    const uint32_t class_idx = (uint32_t)((size + (SMALL_GRANULARITY - 1)) >> SMALL_GRANULARITY_SHIFT);
    heap_size_class_t *heap_size_class = heap->size_class + class_idx;
    _memstat_inc_alloc(heap, class_idx);
    if (neo_likely(heap_size_class->free_list != 0))
        return free_list_pop(&heap_size_class->free_list);
    return _memallocate_from_heap_fallback(heap, heap_size_class, class_idx);
}

static void *_memallocate_medium(heap_t *heap, size_t size) {
    neo_assert(heap, "No thread heap");
    const uint32_t base_idx = (uint32_t)(SMALL_CLASS_COUNT +
                                         ((size - (SMALL_SIZE_LIMIT + 1)) >> MEDIUM_GRANULARITY_SHIFT));
    const uint32_t class_idx = _memory_size_class[base_idx].class_idx;
    heap_size_class_t *heap_size_class = heap->size_class + class_idx;
    _memstat_inc_alloc(heap, class_idx);
    if (neo_likely(heap_size_class->free_list != 0))
        return free_list_pop(&heap_size_class->free_list);
    return _memallocate_from_heap_fallback(heap, heap_size_class, class_idx);
}

static void *_memallocate_large(heap_t *heap, size_t size) {
    neo_assert(heap, "No thread heap");
    size += SPAN_HEADER_SIZE;
    size_t span_count = size >> _memory_span_size_shift;
    if (size & (_memory_span_size - 1))
        ++span_count;

    span_t *span = _memheap_extract_new_span(heap, 0, span_count, SIZE_CLASS_LARGE);
    if (!span)
        return span;

    neo_assert(span->span_count >= span_count, "Internal failure");
    span->size_class = SIZE_CLASS_LARGE;
    span->heap = heap;

#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_add(&heap->large_huge_span, span);
#endif
    ++heap->full_span_count;

    return poff(span, SPAN_HEADER_SIZE);
}

static void *_memallocate_huge(heap_t *heap, size_t size) {
    neo_assert(heap, "No thread heap");
    _memheap_cache_adopt_deferred(heap, 0);
    size += SPAN_HEADER_SIZE;
    size_t num_pages = size >> _memory_page_size_shift;
    if (size & (_memory_page_size - 1))
        ++num_pages;
    size_t align_offset = 0;
    span_t *span = (span_t *)_memmmap(num_pages * _memory_page_size, &align_offset);
    if (!span)
        return span;

    span->size_class = SIZE_CLASS_HUGE;
    span->span_count = (uint32_t)num_pages;
    span->align_offset = (uint32_t)align_offset;
    span->heap = heap;
    _memstat_add_peak(&_huge_pages_current, num_pages, _huge_pages_peak);

#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_add(&heap->large_huge_span, span);
#endif
    ++heap->full_span_count;

    return poff(span, SPAN_HEADER_SIZE);
}

static void *_memallocate(heap_t *heap, size_t size) {
    _memstat_add64(&_allocation_counter, 1);
    if (neo_likely(size <= SMALL_SIZE_LIMIT))
        return _memallocate_small(heap, size);
    else if (size <= _memory_medium_size_limit)
        return _memallocate_medium(heap, size);
    else if (size <= LARGE_SIZE_LIMIT)
        return _memallocate_large(heap, size);
    return _memallocate_huge(heap, size);
}

static void *_memaligned_allocate(heap_t *heap, size_t alignment, size_t size) {
    if (alignment <= SMALL_GRANULARITY)
        return _memallocate(heap, size);

#if NEO_ALLOC_ENABLE_VALIDATION
    if ((size + alignment) < size) {
        errno = EINVAL;
        return 0;
    }
    if (alignment & (alignment - 1)) {
        errno = EINVAL;
        return 0;
    }
#endif

    if ((alignment <= SPAN_HEADER_SIZE) && ((size + SPAN_HEADER_SIZE) < _memory_medium_size_limit)) {
        size_t multiple_size = size ? (size + (SPAN_HEADER_SIZE - 1)) & ~(uintptr_t)(SPAN_HEADER_SIZE - 1)
                                    : SPAN_HEADER_SIZE;
        neo_assert(!(multiple_size % SPAN_HEADER_SIZE), "Failed alignment calculation");
        if (multiple_size <= (size + alignment))
            return _memallocate(heap, multiple_size);
    }

    void *ptr = 0;
    size_t align_mask = alignment - 1;
    if (alignment <= _memory_page_size) {
        ptr = _memallocate(heap, size + alignment);
        if ((uintptr_t)ptr & align_mask) {
            ptr = (void *)(((uintptr_t)ptr & ~(uintptr_t)align_mask) + alignment);
            span_t *span = (span_t *)((uintptr_t)ptr & _memory_span_mask);
            span->flags |= SPAN_FLAG_ALIGNED_BLOCKS;
        }
        return ptr;
    }

    if (alignment & align_mask) {
        errno = EINVAL;
        return 0;
    }
    if (alignment >= _memory_span_size) {
        errno = EINVAL;
        return 0;
    }

    size_t extra_pages = alignment / _memory_page_size;

    size_t num_pages = 1 + (size / _memory_page_size);
    if (size & (_memory_page_size - 1))
        ++num_pages;

    if (extra_pages > num_pages)
        num_pages = 1 + extra_pages;

    size_t original_pages = num_pages;
    size_t limit_pages = (_memory_span_size / _memory_page_size) * 2;
    if (limit_pages < (original_pages * 2))
        limit_pages = original_pages * 2;

    size_t mapped_size, align_offset;
    span_t *span;

    retry:
    align_offset = 0;
    mapped_size = num_pages * _memory_page_size;

    span = (span_t *)_memmmap(mapped_size, &align_offset);
    if (!span) {
        errno = ENOMEM;
        return 0;
    }
    ptr = poff(span, SPAN_HEADER_SIZE);

    if ((uintptr_t)ptr & align_mask)
        ptr = (void *)(((uintptr_t)ptr & ~(uintptr_t)align_mask) + alignment);

    if (((size_t)pdelta(ptr, span) >= _memory_span_size) ||
        (poff(ptr, size) > poff(span, mapped_size)) ||
        (((uintptr_t)ptr & _memory_span_mask) != (uintptr_t)span)) {
        _memunmap(span, mapped_size, align_offset, mapped_size);
        ++num_pages;
        if (num_pages > limit_pages) {
            errno = EINVAL;
            return 0;
        }
        goto retry;
    }

    span->size_class = SIZE_CLASS_HUGE;
    span->span_count = (uint32_t)num_pages;
    span->align_offset = (uint32_t)align_offset;
    span->heap = heap;
    _memstat_add_peak(&_huge_pages_current, num_pages, _huge_pages_peak);

#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_add(&heap->large_huge_span, span);
#endif
    ++heap->full_span_count;

    _memstat_add64(&_allocation_counter, 1);

    return ptr;
}

static void _memdeallocate_direct_small_or_medium(span_t *span, void *block) {
    heap_t *heap = span->heap;
    neo_assert(heap->owner_thread == get_thread_id() || !heap->owner_thread || heap->finalize, "Internal failure");
    if (neo_unlikely(_memspan_is_fully_utilized(span))) {
        span->used_count = span->block_count;
#if MEM_FIRST_CLASS_HEAPS
        _memspan_double_link_list_remove(&heap->full_span[span->size_class], span);
#endif
        _memspan_double_link_list_add(&heap->size_class[span->size_class].partial_span, span);
        --heap->full_span_count;
    }
    *((void **)block) = span->free_list;
    --span->used_count;
    span->free_list = block;
    if (neo_unlikely(span->used_count == span->list_size)) {
        if (span->used_count) {
            void *free_list;
            do {
                free_list = atomic_exchange_ptr_acquire(&span->free_list_deferred, INVALID_POINTER);
            } while (free_list == INVALID_POINTER);
            atomic_store_ptr_release(&span->free_list_deferred, free_list);
        }
        _memspan_double_link_list_remove(&heap->size_class[span->size_class].partial_span, span);
        _memspan_release_to_cache(heap, span);
    }
}

static void _memdeallocate_defer_free_span(heap_t *heap, span_t *span) {
    if (span->size_class != SIZE_CLASS_HUGE)
        _memstat_inc(&heap->span_use[span->span_count - 1].spans_deferred);
    do {
        span->free_list = (void *)atomic_load_ptr(&heap->span_free_deferred);
    } while (!atomic_cas_ptr(&heap->span_free_deferred, span, span->free_list));
}

static void _memdeallocate_defer_small_or_medium(span_t *span, void *block) {
    void *free_list;
    do {
        free_list = atomic_exchange_ptr_acquire(&span->free_list_deferred, INVALID_POINTER);
    } while (free_list == INVALID_POINTER);
    *((void **)block) = free_list;
    uint32_t free_count = ++span->list_size;
    int all_deferred_free = (free_count == span->block_count);
    atomic_store_ptr_release(&span->free_list_deferred, block);
    if (all_deferred_free) {
        _memdeallocate_defer_free_span(span->heap, span);
    }
}

static void _memdeallocate_small_or_medium(span_t *span, void *p) {
    _memstat_inc_free(span->heap, span->size_class);
    if (span->flags & SPAN_FLAG_ALIGNED_BLOCKS) {
        void *blocks_start = poff(span, SPAN_HEADER_SIZE);
        uint32_t block_offset = (uint32_t)pdelta(p, blocks_start);
        p = poff(p, -(int32_t)(block_offset % span->block_size));
    }
#if MEM_FIRST_CLASS_HEAPS
    int defer = (span->heap->owner_thread && (span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#else
    int defer = ((span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#endif
    if (!defer)
        _memdeallocate_direct_small_or_medium(span, p);
    else
        _memdeallocate_defer_small_or_medium(span, p);
}

static void _memdeallocate_large(span_t *span) {
    neo_assert(span->size_class == SIZE_CLASS_LARGE, "Bad span size class");
    neo_assert(!(span->flags & SPAN_FLAG_MASTER) || !(span->flags & SPAN_FLAG_SUBSPAN), "Span flag corrupted");
    neo_assert((span->flags & SPAN_FLAG_MASTER) || (span->flags & SPAN_FLAG_SUBSPAN), "Span flag corrupted");
#if MEM_FIRST_CLASS_HEAPS
    int defer = (span->heap->owner_thread && (span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#else
    int defer = ((span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#endif
    if (defer) {
        _memdeallocate_defer_free_span(span->heap, span);
        return;
    }
    neo_assert(span->heap->full_span_count, "Heap span counter corrupted");
    --span->heap->full_span_count;
#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_remove(&span->heap->large_huge_span, span);
#endif
#if NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE || NEO_ALLOC_ENABLE_STATS
    size_t idx = span->span_count - 1;
    atomic_decr32(&span->heap->span_use[idx].current);
#endif
    heap_t *heap = span->heap;
    neo_assert(heap, "No thread heap");
#if NEO_ALLOC_ENABLE_THREAD_CACHE
    const int set_as_reserved = ((span->span_count > 1) && (heap->span_cache.count == 0) && !heap->finalize &&
                                 !heap->spans_reserved);
#else
    const int set_as_reserved = ((span->span_count > 1) && !heap->finalize && !heap->spans_reserved);
#endif
    if (set_as_reserved) {
        heap->span_reserve = span;
        heap->spans_reserved = span->span_count;
        if (span->flags & SPAN_FLAG_MASTER) {
            heap->span_reserve_master = span;
        } else {
            span_t *master = (span_t *)poff(span, -(intptr_t)((size_t)span->offset_from_master *
                                                              _memory_span_size));
            heap->span_reserve_master = master;
            neo_assert(master->flags & SPAN_FLAG_MASTER, "Span flag corrupted");
            neo_assert(atomic_load32(&master->remaining_spans) >= (int32_t)span->span_count,
                       "Master span count corrupted");
        }
        _memstat_inc(&heap->span_use[idx].spans_to_reserved);
    } else {
        _memheap_cache_insert(heap, span);
    }
}

static void _memdeallocate_huge(span_t *span) {
    neo_assert(span->heap, "No span heap");
#if MEM_FIRST_CLASS_HEAPS
    int defer = (span->heap->owner_thread && (span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#else
    int defer = ((span->heap->owner_thread != get_thread_id()) && !span->heap->finalize);
#endif
    if (defer) {
        _memdeallocate_defer_free_span(span->heap, span);
        return;
    }
    neo_assert(span->heap->full_span_count, "Heap span counter corrupted");
    --span->heap->full_span_count;
#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_remove(&span->heap->large_huge_span, span);
#endif

    size_t num_pages = span->span_count;
    _memunmap(span, num_pages * _memory_page_size, span->align_offset, num_pages * _memory_page_size);
    _memstat_sub(&_huge_pages_current, num_pages);
}

static void _memadjust_size_class(size_t iclass) {
    size_t block_size = _memory_size_class[iclass].block_size;
    size_t block_count = (_memory_span_size - SPAN_HEADER_SIZE) / block_size;

    _memory_size_class[iclass].block_count = (uint16_t)block_count;
    _memory_size_class[iclass].class_idx = (uint16_t)iclass;

    if (iclass >= SMALL_CLASS_COUNT) {
        size_t prevclass = iclass;
        while (prevclass > 0) {
            --prevclass;
            if (_memory_size_class[prevclass].block_count == _memory_size_class[iclass].block_count)
                _memmemcpy_const(_memory_size_class + prevclass, _memory_size_class + iclass,
                                 sizeof(_memory_size_class[iclass]));
            else
                break;
        }
    }
}

extern inline int meminitialize(void) {
    if (_meminitialized) {
        memthread_initialize();
        return 0;
    }
    return meminitialize_config(0);
}

int meminitialize_config(const neo_alloc_config_t *config) {
    if (_meminitialized) {
        memthread_initialize();
        return 0;
    }
    _meminitialized = 1;

    if (config)
        memcpy(&_memory_config, config, sizeof(neo_alloc_config_t));
    else
        _memmemset_const(&_memory_config, 0, sizeof(neo_alloc_config_t));

    if (!_memory_config.memory_map || !_memory_config.memory_unmap) {
        _memory_config.memory_map = _memmmap_os;
        _memory_config.memory_unmap = _memunmap_os;
    }

#if PLATFORM_WINDOWS
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));
    GetSystemInfo(&system_info);
    _memory_map_granularity = system_info.dwAllocationGranularity;
#else
    _memory_map_granularity = (size_t)sysconf(_SC_PAGESIZE);
#endif

#if MEM_CONFIGURABLE
    _memory_page_size = _memory_config.page_size;
#else
    _memory_page_size = 0;
#endif
    _memory_huge_pages = 0;
    if (!_memory_page_size) {
#if PLATFORM_WINDOWS
        _memory_page_size = system_info.dwPageSize;
#else
        _memory_page_size = _memory_map_granularity;
        if (_memory_config.enable_huge_pages) {
#if defined(__linux__)
            size_t huge_page_size = 0;
            FILE *meminfo = fopen("/proc/meminfo", "r");
            if (meminfo) {
                char line[128];
                while (!huge_page_size && fgets(line, sizeof(line) - 1, meminfo)) {
                    line[sizeof(line) - 1] = 0;
                    if (strstr(line, "Hugepagesize:"))
                        huge_page_size = (size_t)strtol(line + 13, 0, 10) * 1024;
                }
                fclose(meminfo);
            }
            if (huge_page_size) {
                _memory_huge_pages = 1;
                _memory_page_size = huge_page_size;
                _memory_map_granularity = huge_page_size;
            }
#elif defined(__FreeBSD__)
            int rc;
            size_t sz = sizeof(rc);

            if (sysctlbyname("vm.pmap.pg_ps_enabled", &rc, &sz, NULL, 0) == 0 && rc == 1) {
                static size_t defsize = 2 * 1024 * 1024;
                int nsize = 0;
                size_t sizes[4] = {0};
                _memory_huge_pages = 1;
                _memory_page_size = defsize;
                if ((nsize = getpagesizes(sizes, 4)) >= 2) {
                    nsize --;
                    for (size_t csize = sizes[nsize]; nsize >= 0 && csize; --nsize, csize = sizes[nsize]) {
                                                neo_assert(!(csize & (csize -1)) && !(csize % 1024), "Invalid page size");
                        if (defsize < csize) {
                            _memory_page_size = csize;
                            break;
                        }
                    }
                }
                _memory_map_granularity = _memory_page_size;
            }
#elif defined(__APPLE__) || defined(__NetBSD__)
            _memory_huge_pages = 1;
            _memory_page_size = 2 * 1024 * 1024;
            _memory_map_granularity = _memory_page_size;
#endif
        }
#endif
    } else {
        if (_memory_config.enable_huge_pages)
            _memory_huge_pages = 1;
    }

#if PLATFORM_WINDOWS
    if (_memory_config.enable_huge_pages) {
        HANDLE token = 0;
        size_t large_page_minimum = GetLargePageMinimum();
        if (large_page_minimum)
            OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
        if (token) {
            LUID luid;
            if (LookupPrivilegeValue(0, SE_LOCK_MEMORY_NAME, &luid)) {
                TOKEN_PRIVILEGES token_privileges;
                memset(&token_privileges, 0, sizeof(token_privileges));
                token_privileges.PrivilegeCount = 1;
                token_privileges.Privileges[0].Luid = luid;
                token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                if (AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, 0, 0)) {
                    if (GetLastError() == ERROR_SUCCESS)
                        _memory_huge_pages = 1;
                }
            }
            CloseHandle(token);
        }
        if (_memory_huge_pages) {
            if (large_page_minimum > _memory_page_size)
                _memory_page_size = large_page_minimum;
            if (large_page_minimum > _memory_map_granularity)
                _memory_map_granularity = large_page_minimum;
        }
    }
#endif

    size_t min_span_size = 256;
    size_t max_page_size;
#if UINTPTR_MAX > 0xFFFFFFFF
    max_page_size = 4096ULL * 1024ULL * 1024ULL;
#else
    max_page_size = 4 * 1024 * 1024;
#endif
    if (_memory_page_size < min_span_size)
        _memory_page_size = min_span_size;
    if (_memory_page_size > max_page_size)
        _memory_page_size = max_page_size;
    _memory_page_size_shift = 0;
    size_t page_size_bit = _memory_page_size;
    while (page_size_bit != 1) {
        ++_memory_page_size_shift;
        page_size_bit >>= 1;
    }
    _memory_page_size = ((size_t)1 << _memory_page_size_shift);

#if MEM_CONFIGURABLE
    if (!_memory_config.span_size) {
        _memory_span_size = _memory_default_span_size;
        _memory_span_size_shift = _memory_default_span_size_shift;
        _memory_span_mask = _memory_default_span_mask;
    } else {
        size_t span_size = _memory_config.span_size;
        if (span_size > (256 * 1024))
            span_size = (256 * 1024);
        _memory_span_size = 4096;
        _memory_span_size_shift = 12;
        while (_memory_span_size < span_size) {
            _memory_span_size <<= 1;
            ++_memory_span_size_shift;
        }
        _memory_span_mask = ~(uintptr_t)(_memory_span_size - 1);
    }
#endif

    _memory_span_map_count = (_memory_config.span_map_count ? _memory_config.span_map_count : NEO_ALLOC_SPAN_MAP_COUNT);
    if ((_memory_span_size * _memory_span_map_count) < _memory_page_size)
        _memory_span_map_count = (_memory_page_size / _memory_span_size);
    if ((_memory_page_size >= _memory_span_size) && ((_memory_span_map_count * _memory_span_size) % _memory_page_size))
        _memory_span_map_count = (_memory_page_size / _memory_span_size);
    _memory_heap_reserve_count = (_memory_span_map_count > NEO_ALLOC_SPAN_MAP_COUNT) ? NEO_ALLOC_SPAN_MAP_COUNT
                                                                                     : _memory_span_map_count;

    _memory_config.page_size = _memory_page_size;
    _memory_config.span_size = _memory_span_size;
    _memory_config.span_map_count = _memory_span_map_count;
    _memory_config.enable_huge_pages = _memory_huge_pages;

#if ((defined(__APPLE__) || defined(__HAIKU__)) && NEO_ALLOC_ENABLE_PRELOAD) || defined(__TINYC__)
    if (pthread_key_create(&_memory_thread_heap, _memheap_release_raw_fc))
        return -1;
#endif
#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
    fls_key = FlsAlloc(&_memthread_destructor);
#endif

    size_t iclass = 0;
    _memory_size_class[iclass].block_size = SMALL_GRANULARITY;
    _memadjust_size_class(iclass);
    for (iclass = 1; iclass < SMALL_CLASS_COUNT; ++iclass) {
        size_t size = iclass * SMALL_GRANULARITY;
        _memory_size_class[iclass].block_size = (uint32_t)size;
        _memadjust_size_class(iclass);
    }
    _memory_medium_size_limit = (_memory_span_size - SPAN_HEADER_SIZE) >> 1;
    if (_memory_medium_size_limit > MEDIUM_SIZE_LIMIT)
        _memory_medium_size_limit = MEDIUM_SIZE_LIMIT;
    for (iclass = 0; iclass < MEDIUM_CLASS_COUNT; ++iclass) {
        size_t size = SMALL_SIZE_LIMIT + ((iclass + 1) * MEDIUM_GRANULARITY);
        if (size > _memory_medium_size_limit) {
            _memory_medium_size_limit = SMALL_SIZE_LIMIT + (iclass * MEDIUM_GRANULARITY);
            break;
        }
        _memory_size_class[SMALL_CLASS_COUNT + iclass].block_size = (uint32_t)size;
        _memadjust_size_class(SMALL_CLASS_COUNT + iclass);
    }

    _memory_orphan_heaps = 0;
#if MEM_FIRST_CLASS_HEAPS
    _memory_first_class_orphan_heaps = 0;
#endif
#if NEO_ALLOC_ENABLE_STATS
    atomic_store32(&_memory_active_heaps, 0);
    atomic_store32(&_mapped_pages, 0);
    _mapped_pages_peak = 0;
    atomic_store32(&_master_spans, 0);
    atomic_store32(&_mapped_total, 0);
    atomic_store32(&_unmapped_total, 0);
    atomic_store32(&_mapped_pages_os, 0);
    atomic_store32(&_huge_pages_current, 0);
    _huge_pages_peak = 0;
#endif
    memset(_memory_heaps, 0, sizeof(_memory_heaps));
    atomic_store32_release(&_memory_global_lock, 0);

    memthread_initialize();
    return 0;
}

void memfinalize(void) {
    memthread_finalize(1);

    if (_memory_global_reserve) {
        atomic_add32(&_memory_global_reserve_master->remaining_spans, -(int32_t)_memory_global_reserve_count);
        _memory_global_reserve_master = 0;
        _memory_global_reserve_count = 0;
        _memory_global_reserve = 0;
    }
    atomic_store32_release(&_memory_global_lock, 0);

    for (size_t list_idx = 0; list_idx < NEO_ALLOC_HEAP_ARRAY_SIZE; ++list_idx) {
        heap_t *heap = _memory_heaps[list_idx];
        while (heap) {
            heap_t *next_heap = heap->next_heap;
            heap->finalize = 1;
            _memheap_global_finalize(heap);
            heap = next_heap;
        }
    }

#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass)
        _memglobal_cache_finalize(&_memory_span_cache[iclass]);
#endif

#if (defined(__APPLE__) || defined(__HAIKU__)) && NEO_ALLOC_ENABLE_PRELOAD
    pthread_key_delete(_memory_thread_heap);
#endif
#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
    FlsFree(fls_key);
    fls_key = 0;
#endif
#if NEO_ALLOC_ENABLE_STATS
    neo_assert(atomic_load32(&_mapped_pages) == 0, "Memory leak detected");
neo_assert(atomic_load32(&_mapped_pages_os) == 0, "Memory leak detected");
#endif

    _meminitialized = 0;
}

extern inline void memthread_initialize(void) {
    if (!get_thread_heap_raw()) {
        heap_t *heap = _memheap_allocate(0);
        if (heap) {
            _memstat_inc(&_memory_active_heaps);
            set_thread_heap(heap);
#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
            FlsSetValue(fls_key, heap);
#endif
        }
    }
}

void memthread_finalize(int release_caches) {
    heap_t *heap = get_thread_heap_raw();
    if (heap)
        _memheap_release_raw(heap, release_caches);
    set_thread_heap(0);
#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
    FlsSetValue(fls_key, 0);
#endif
}

int memis_thread_initialized(void) {
    return (get_thread_heap_raw() != 0) ? 1 : 0;
}

const neo_alloc_config_t *memconfig(void) { return &_memory_config; }

extern inline size_t memusable_size(void *ptr) { return neo_allocator_bin_useable_size(ptr); }

extern inline void memthread_collect(void) {}

void memthread_statistics(neo_alloc_thread_stats_t *stats) {
    memset(stats, 0, sizeof(neo_alloc_thread_stats_t));
    heap_t *heap = get_thread_heap_raw();
    if (!heap)
        return;

    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        size_class_t *size_class = _memory_size_class + iclass;
        span_t *span = heap->size_class[iclass].partial_span;
        while (span) {
            size_t free_count = span->list_size;
            size_t block_count = size_class->block_count;
            if (span->free_list_limit < block_count)
                block_count = span->free_list_limit;
            free_count += (block_count - span->used_count);
            stats->sizecache += free_count * size_class->block_size;
            span = span->next;
        }
    }

#if NEO_ALLOC_ENABLE_THREAD_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        span_cache_t *span_cache;
        if (!iclass)
            span_cache = &heap->span_cache;
        else
            span_cache = (span_cache_t *)(heap->span_large_cache + (iclass - 1));
        stats->spancache += span_cache->count * (iclass + 1) * _memory_span_size;
    }
#endif

    span_t *deferred = (span_t *)atomic_load_ptr(&heap->span_free_deferred);
    while (deferred) {
        if (deferred->size_class != SIZE_CLASS_HUGE)
            stats->spancache += (size_t)deferred->span_count * _memory_span_size;
        deferred = (span_t *)deferred->free_list;
    }

#if NEO_ALLOC_ENABLE_STATS
    stats->thread_to_global = (size_t)atomic_load64(&heap->thread_to_global);
    stats->global_to_thread = (size_t)atomic_load64(&heap->global_to_thread);

    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        stats->span_use[iclass].current = (size_t)atomic_load32(&heap->span_use[iclass].current);
        stats->span_use[iclass].peak = (size_t)atomic_load32(&heap->span_use[iclass].high);
        stats->span_use[iclass].to_global = (size_t)atomic_load32(&heap->span_use[iclass].spans_to_global);
        stats->span_use[iclass].from_global = (size_t)atomic_load32(&heap->span_use[iclass].spans_from_global);
        stats->span_use[iclass].to_cache = (size_t)atomic_load32(&heap->span_use[iclass].spans_to_cache);
        stats->span_use[iclass].from_cache = (size_t)atomic_load32(&heap->span_use[iclass].spans_from_cache);
        stats->span_use[iclass].to_reserved = (size_t)atomic_load32(&heap->span_use[iclass].spans_to_reserved);
        stats->span_use[iclass].from_reserved = (size_t)atomic_load32(&heap->span_use[iclass].spans_from_reserved);
        stats->span_use[iclass].map_calls = (size_t)atomic_load32(&heap->span_use[iclass].spans_map_calls);
    }
    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        stats->size_use[iclass].alloc_current = (size_t)atomic_load32(&heap->size_class_use[iclass].alloc_current);
        stats->size_use[iclass].alloc_peak = (size_t)heap->size_class_use[iclass].alloc_peak;
        stats->size_use[iclass].alloc_total = (size_t)atomic_load32(&heap->size_class_use[iclass].alloc_total);
        stats->size_use[iclass].free_total = (size_t)atomic_load32(&heap->size_class_use[iclass].free_total);
        stats->size_use[iclass].spans_to_cache = (size_t)atomic_load32(&heap->size_class_use[iclass].spans_to_cache);
        stats->size_use[iclass].spans_from_cache = (size_t)atomic_load32(&heap->size_class_use[iclass].spans_from_cache);
        stats->size_use[iclass].spans_from_reserved = (size_t)atomic_load32(&heap->size_class_use[iclass].spans_from_reserved);
        stats->size_use[iclass].map_calls = (size_t)atomic_load32(&heap->size_class_use[iclass].spans_map_calls);
    }
#endif
}

void memglobal_statistics(neo_alloc_global_stats_t *stats) {
    memset(stats, 0, sizeof(neo_alloc_global_stats_t));
#if NEO_ALLOC_ENABLE_STATS
    stats->mapped = (size_t)atomic_load32(&_mapped_pages) * _memory_page_size;
    stats->mapped_peak = (size_t)_mapped_pages_peak * _memory_page_size;
    stats->mapped_total = (size_t)atomic_load32(&_mapped_total) * _memory_page_size;
    stats->unmapped_total = (size_t)atomic_load32(&_unmapped_total) * _memory_page_size;
    stats->huge_alloc = (size_t)atomic_load32(&_huge_pages_current) * _memory_page_size;
    stats->huge_alloc_peak = (size_t)_huge_pages_peak * _memory_page_size;
#endif
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        global_cache_t *cache = &_memory_span_cache[iclass];
        while (!atomic_cas32_acquire(&cache->lock, 1, 0))
            _memspin();
        uint32_t count = cache->count;
#if NEO_ALLOC_ENABLE_UNLIMITED_CACHE
        span_t *current_span = cache->overflow;
        while (current_span) {
            ++count;
            current_span = current_span->next;
        }
#endif
        atomic_store32_release(&cache->lock, 0);
        stats->cached += count * (iclass + 1) * _memory_span_size;
    }
#endif
}

#if NEO_ALLOC_ENABLE_STATS

static void _memory_heap_dump_statistics(heap_t* heap, void* file) {
    fprintf(file, "Heap %d stats:\n", heap->id);
    fprintf(file, "Class   CurAlloc  PeakAlloc   TotAlloc    TotFree  BlkSize BlkCount SpansCur SpansPeak  PeakAllocMiB  ToCacheMiB FromCacheMiB FromReserveMiB MmapCalls\n");
    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        if (!atomic_load32(&heap->size_class_use[iclass].alloc_total))
            continue;
        fprintf(file, "%3u:  %10u %10u %10u %10u %8u %8u %8d %9d %13zu %11zu %12zu %14zu %9u\n", (uint32_t)iclass,
            atomic_load32(&heap->size_class_use[iclass].alloc_current),
            heap->size_class_use[iclass].alloc_peak,
            atomic_load32(&heap->size_class_use[iclass].alloc_total),
            atomic_load32(&heap->size_class_use[iclass].free_total),
            _memory_size_class[iclass].block_size,
            _memory_size_class[iclass].block_count,
            atomic_load32(&heap->size_class_use[iclass].spans_current),
            heap->size_class_use[iclass].spans_peak,
            ((size_t)heap->size_class_use[iclass].alloc_peak * (size_t)_memory_size_class[iclass].block_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->size_class_use[iclass].spans_to_cache) * _memory_span_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->size_class_use[iclass].spans_from_cache) * _memory_span_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->size_class_use[iclass].spans_from_reserved) * _memory_span_size) / (size_t)(1024 * 1024),
            atomic_load32(&heap->size_class_use[iclass].spans_map_calls));
    }
    fprintf(file, "Spans  Current     Peak Deferred  PeakMiB  Cached  ToCacheMiB FromCacheMiB ToReserveMiB FromReserveMiB ToGlobalMiB FromGlobalMiB  MmapCalls\n");
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        if (!atomic_load32(&heap->span_use[iclass].high) && !atomic_load32(&heap->span_use[iclass].spans_map_calls))
            continue;
        fprintf(file, "%4u: %8d %8u %8u %8zu %7u %11zu %12zu %12zu %14zu %11zu %13zu %10u\n", (uint32_t)(iclass + 1),
            atomic_load32(&heap->span_use[iclass].current),
            atomic_load32(&heap->span_use[iclass].high),
            atomic_load32(&heap->span_use[iclass].spans_deferred),
            ((size_t)atomic_load32(&heap->span_use[iclass].high) * (size_t)_memory_span_size * (iclass + 1)) / (size_t)(1024 * 1024),
#if NEO_ALLOC_ENABLE_THREAD_CACHE
            (unsigned int)(!iclass ? heap->span_cache.count : heap->span_large_cache[iclass - 1].count),
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_to_cache) * (iclass + 1) * _memory_span_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_from_cache) * (iclass + 1) * _memory_span_size) / (size_t)(1024 * 1024),
#else
            0, (size_t)0, (size_t)0,
#endif
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_to_reserved) * (iclass + 1) * _memory_span_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_from_reserved) * (iclass + 1) * _memory_span_size) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_to_global) * (size_t)_memory_span_size * (iclass + 1)) / (size_t)(1024 * 1024),
            ((size_t)atomic_load32(&heap->span_use[iclass].spans_from_global) * (size_t)_memory_span_size * (iclass + 1)) / (size_t)(1024 * 1024),
            atomic_load32(&heap->span_use[iclass].spans_map_calls));
    }
    fprintf(file, "Full spans: %zu\n", heap->full_span_count);
    fprintf(file, "ThreadToGlobalMiB GlobalToThreadMiB\n");
    fprintf(file, "%17zu %17zu\n", (size_t)atomic_load64(&heap->thread_to_global) / (size_t)(1024 * 1024), (size_t)atomic_load64(&heap->global_to_thread) / (size_t)(1024 * 1024));
}

#endif

void memdump_statistics(void *file) {
#if NEO_ALLOC_ENABLE_STATS
    for (size_t list_idx = 0; list_idx < NEO_ALLOC_HEAP_ARRAY_SIZE; ++list_idx) {
        heap_t* heap = _memory_heaps[list_idx];
        while (heap) {
            int need_dump = 0;
            for (size_t iclass = 0; !need_dump && (iclass < SIZE_CLASS_COUNT); ++iclass) {
                if (!atomic_load32(&heap->size_class_use[iclass].alloc_total)) {
                    neo_assert(!atomic_load32(&heap->size_class_use[iclass].free_total), "Heap statistics counter mismatch");
                    neo_assert(!atomic_load32(&heap->size_class_use[iclass].spans_map_calls), "Heap statistics counter mismatch");
                    continue;
                }
                need_dump = 1;
            }
            for (size_t iclass = 0; !need_dump && (iclass < LARGE_CLASS_COUNT); ++iclass) {
                if (!atomic_load32(&heap->span_use[iclass].high) && !atomic_load32(&heap->span_use[iclass].spans_map_calls))
                    continue;
                need_dump = 1;
            }
            if (need_dump)
                _memory_heap_dump_statistics(heap, file);
            heap = heap->next_heap;
        }
    }
    fprintf(file, "Global stats:\n");
    size_t huge_current = (size_t)atomic_load32(&_huge_pages_current) * _memory_page_size;
    size_t huge_peak = (size_t)_huge_pages_peak * _memory_page_size;
    fprintf(file, "HugeCurrentMiB HugePeakMiB\n");
    fprintf(file, "%14zu %11zu\n", huge_current / (size_t)(1024 * 1024), huge_peak / (size_t)(1024 * 1024));

#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
    fprintf(file, "GlobalCacheMiB\n");
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        global_cache_t* cache = _memory_span_cache + iclass;
        size_t global_cache = (size_t)cache->count * iclass * _memory_span_size;

        size_t global_overflow_cache = 0;
        span_t* span = cache->overflow;
        while (span) {
            global_overflow_cache += iclass * _memory_span_size;
            span = span->next;
        }
        if (global_cache || global_overflow_cache || cache->insert_count || cache->extract_count)
            fprintf(file, "%4zu: %8zuMiB (%8zuMiB overflow) %14zu insert %14zu extract\n", iclass + 1, global_cache / (size_t)(1024 * 1024), global_overflow_cache / (size_t)(1024 * 1024), cache->insert_count, cache->extract_count);
    }
#endif

    size_t mapped = (size_t)atomic_load32(&_mapped_pages) * _memory_page_size;
    size_t mapped_os = (size_t)atomic_load32(&_mapped_pages_os) * _memory_page_size;
    size_t mapped_peak = (size_t)_mapped_pages_peak * _memory_page_size;
    size_t mapped_total = (size_t)atomic_load32(&_mapped_total) * _memory_page_size;
    size_t unmapped_total = (size_t)atomic_load32(&_unmapped_total) * _memory_page_size;
    fprintf(file, "MappedMiB MappedOSMiB MappedPeakMiB MappedTotalMiB UnmappedTotalMiB\n");
    fprintf(file, "%9zu %11zu %13zu %14zu %16zu\n",
        mapped / (size_t)(1024 * 1024),
        mapped_os / (size_t)(1024 * 1024),
        mapped_peak / (size_t)(1024 * 1024),
        mapped_total / (size_t)(1024 * 1024),
        unmapped_total / (size_t)(1024 * 1024));

    fprintf(file, "\n");
#if 0
    int64_t allocated = atomic_load64(&_allocation_counter);
    int64_t deallocated = atomic_load64(&_deallocation_counter);
    fprintf(file, "Allocation count: %lli\n", allocated);
    fprintf(file, "Deallocation count: %lli\n", deallocated);
    fprintf(file, "Current allocations: %lli\n", (allocated - deallocated));
    fprintf(file, "Master spans: %d\n", atomic_load32(&_master_spans));
    fprintf(file, "Dangling master spans: %d\n", atomic_load32(&_unmapped_master_spans));
#endif
#endif
    (void)sizeof(file);
}

#if MEM_FIRST_CLASS_HEAPS

extern inline memheap_t*
memheap_acquire(void) {
                    heap_t* heap = _memheap_allocate(1);
    memassume(heap != NULL);
    heap->owner_thread = 0;
    _memstat_inc(&_memory_active_heaps);
    return heap;
}

extern inline void memheap_release(memheap_t* heap) {
    if (heap)
        _memheap_release(heap, 1, 1);
}

extern inline MEM_ALLOCATOR void*
memheap_alloc(memheap_t* heap, size_t size) {
#if NEO_ALLOC_ENABLE_VALIDATION
    if (size >= MAX_ALLOC_SIZE) {
        errno = EINVAL;
        return 0;
    }
#endif
    return _memallocate(heap, size);
}

extern inline MEM_ALLOCATOR void*
memheap_aligned_alloc(memheap_t* heap, size_t alignment, size_t size) {
#if NEO_ALLOC_ENABLE_VALIDATION
    if (size >= MAX_ALLOC_SIZE) {
        errno = EINVAL;
        return 0;
    }
#endif
    return _memaligned_allocate(heap, alignment, size);
}

extern inline MEM_ALLOCATOR void*
memheap_calloc(memheap_t* heap, size_t num, size_t size) {
    return memheap_aligned_calloc(heap, 0, num, size);
}

extern inline MEM_ALLOCATOR void*
memheap_aligned_calloc(memheap_t* heap, size_t alignment, size_t num, size_t size) {
    size_t total;
#if NEO_ALLOC_ENABLE_VALIDATION
#if PLATFORM_WINDOWS
    int err = SizeTMult(num, size, &total);
    if ((err != S_OK) || (total >= MAX_ALLOC_SIZE)) {
        errno = EINVAL;
        return 0;
    }
#else
    int err = __builtin_umull_overflow(num, size, &total);
    if (err || (total >= MAX_ALLOC_SIZE)) {
        errno = EINVAL;
        return 0;
    }
#endif
#else
    total = num * size;
#endif
    void* block = _memaligned_allocate(heap, alignment, total);
    if (block)
        memset(block, 0, total);
    return block;
}

extern inline MEM_ALLOCATOR void*
memheap_realloc(memheap_t* heap, void* ptr, size_t size, unsigned int flags) {
#if NEO_ALLOC_ENABLE_VALIDATION
    if (size >= MAX_ALLOC_SIZE) {
        errno = EINVAL;
        return ptr;
    }
#endif
    return _memreallocate(heap, ptr, size, 0, flags);
}

extern inline MEM_ALLOCATOR void*
memheap_aligned_realloc(memheap_t* heap, void* ptr, size_t alignment, size_t size, unsigned int flags) {
#if NEO_ALLOC_ENABLE_VALIDATION
    if ((size + alignment < size) || (alignment > _memory_page_size)) {
        errno = EINVAL;
        return 0;
    }
#endif
    return _memaligned_reallocate(heap, ptr, alignment, size, 0, flags);
}

extern inline void memheap_free(memheap_t* heap, void* ptr) {
    (void)sizeof(heap);
    _memdeallocate(ptr);
}

extern inline void memheap_free_all(memheap_t* heap) {
    span_t* span;
    span_t* next_span;

    _memheap_cache_adopt_deferred(heap, 0);

    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        span = heap->size_class[iclass].partial_span;
        while (span) {
            next_span = span->next;
            _memheap_cache_insert(heap, span);
            span = next_span;
        }
        heap->size_class[iclass].partial_span = 0;
        span = heap->full_span[iclass];
        while (span) {
            next_span = span->next;
            _memheap_cache_insert(heap, span);
            span = next_span;
        }
    }
    memset(heap->size_class, 0, sizeof(heap->size_class));
    memset(heap->full_span, 0, sizeof(heap->full_span));

    span = heap->large_huge_span;
    while (span) {
        next_span = span->next;
        if (neo_unlikely(span->size_class == SIZE_CLASS_HUGE))
            _memdeallocate_huge(span);
        else
            _memheap_cache_insert(heap, span);
        span = next_span;
    }
    heap->large_huge_span = 0;
    heap->full_span_count = 0;

#if NEO_ALLOC_ENABLE_THREAD_CACHE
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        span_cache_t* span_cache;
        if (!iclass)
            span_cache = &heap->span_cache;
        else
            span_cache = (span_cache_t*)(heap->span_large_cache + (iclass - 1));
        if (!span_cache->count)
            continue;
#if NEO_ALLOC_ENABLE_GLOBAL_CACHE
        _memstat_add64(&heap->thread_to_global, span_cache->count * (iclass + 1) * _memory_span_size);
        _memstat_add(&heap->span_use[iclass].spans_to_global, span_cache->count);
        _memglobal_cache_insert_spans(span_cache->span, iclass + 1, span_cache->count);
#else
        for (size_t ispan = 0; ispan < span_cache->count; ++ispan)
            _memspan_unmap(span_cache->span[ispan]);
#endif
        span_cache->count = 0;
    }
#endif

#if NEO_ALLOC_ENABLE_STATS
    for (size_t iclass = 0; iclass < SIZE_CLASS_COUNT; ++iclass) {
        atomic_store32(&heap->size_class_use[iclass].alloc_current, 0);
        atomic_store32(&heap->size_class_use[iclass].spans_current, 0);
    }
    for (size_t iclass = 0; iclass < LARGE_CLASS_COUNT; ++iclass) {
        atomic_store32(&heap->span_use[iclass].current, 0);
    }
#endif
}

extern inline void memheap_thread_set_current(memheap_t* heap) {
    heap_t* prev_heap = get_thread_heap_raw();
    if (prev_heap != heap) {
        set_thread_heap(heap);
        if (prev_heap)
            memheap_release(prev_heap);
    }
}

extern inline memheap_t*
memget_heap_for_ptr(void* ptr)
{
        span_t* span = (span_t*)((uintptr_t)ptr & _memory_span_mask);
    if (span)
    {
        return span->heap;
    }
    return 0;
}

#endif

static volatile int64_t alloc_init = 0;

static inline void check_allocator_online(void) {
    if (neo_unlikely(!neo_atomic_load(&alloc_init, NEO_MEMORD_RELX))) {
        neo_warn("Allocator not initialized");
        neo_allocator_init();
    }
}

void neo_allocator_init(void) {
    meminitialize();
    neo_atomic_store(&alloc_init, 1, NEO_MEMORD_RELX);
}

void neo_allocator_shutdown(void) {
    memfinalize();
}

void *neo_allocator_alloc(size_t len) {
    check_allocator_online();
    neo_assert(len != 0 && len < MAX_ALLOC_SIZE, "Allocation with invalid size: %zub, max: %zub", len, MAX_ALLOC_SIZE);
    heap_t *heap = get_thread_heap();
    _memstat_add64(&_allocation_counter, 1);
    if (neo_likely(len <= SMALL_SIZE_LIMIT)) {
        return _memallocate_small(heap, len);
    } else if (len <= _memory_medium_size_limit) {
        return _memallocate_medium(heap, len);
    } else if (len <= LARGE_SIZE_LIMIT) {
        return _memallocate_large(heap, len);
    }
    return _memallocate_huge(heap, len);
}

void *neo_allocator_alloc_aligned(size_t len, size_t align) {
    check_allocator_online();
    neo_assert(len != 0 && len < MAX_ALLOC_SIZE && align, "Allocation with invalid size: %zub, max: %zub", len,
               MAX_ALLOC_SIZE);
    neo_assert(len + align >= len && (align & (align - 1)) == 0, "Allocation with invalid alignment: %zu", align);
    if (align <= SMALL_GRANULARITY) { return neo_allocator_alloc(len); }
    else if ((align <= SPAN_HEADER_SIZE) && ((len + SPAN_HEADER_SIZE) < _memory_medium_size_limit)) {
        size_t multiple_size = len ? (len + (SPAN_HEADER_SIZE - 1)) & ~(uintptr_t)(SPAN_HEADER_SIZE - 1)
                                   : SPAN_HEADER_SIZE;
        neo_assert(!(multiple_size % SPAN_HEADER_SIZE), "Failed alignment calculation");
        if (multiple_size <= (len + align)) { return neo_allocator_alloc(multiple_size); }
    }
    void *ptr = 0;
    size_t align_mask = align - 1;
    if (align <= _memory_page_size) {
        ptr = neo_allocator_alloc(len + align);
        if ((uintptr_t)ptr & align_mask) {
            ptr = (void *)(((uintptr_t)ptr & ~(uintptr_t)align_mask) + align);
            span_t *span = (span_t *)((uintptr_t)ptr & _memory_span_mask);
            span->flags |= SPAN_FLAG_ALIGNED_BLOCKS;
        }
        return ptr;
    }
    neo_assert(!(align & align_mask) && align < _memory_span_size, "Invalid alignment");
    size_t extra_pages = align / _memory_page_size;
    size_t num_pages = 1 + (len / _memory_page_size);
    if (len & (_memory_page_size - 1)) { ++num_pages; }
    if (extra_pages > num_pages) { num_pages = 1 + extra_pages; }
    size_t original_pages = num_pages;
    size_t limit_pages = (_memory_span_size / _memory_page_size) << 1;
    if (limit_pages < original_pages << 1) { limit_pages = original_pages << 1; }
    size_t mapped_size;
    size_t align_offset;
    span_t *span;
    retry:
    align_offset = 0;
    mapped_size = num_pages * _memory_page_size;
    span = (span_t *)_memmmap(mapped_size, &align_offset);
    neo_assert(span != NULL, "Out of memory");
    ptr = poff(span, SPAN_HEADER_SIZE);
    if ((uintptr_t)ptr & align_mask) {
        ptr = (void *)(((uintptr_t)ptr & ~(uintptr_t)align_mask) + align);
    }
    if (((size_t)pdelta(ptr, span) >= _memory_span_size) ||
        (poff(ptr, len) > poff(span, mapped_size)) ||
        (((uintptr_t)ptr & _memory_span_mask) != (uintptr_t)span)) {
        _memunmap(span, mapped_size, align_offset, mapped_size);
        ++num_pages;
        neo_assert(num_pages <= limit_pages, "Page limit reached: %zu", limit_pages);
        goto retry;
    }
    heap_t *heap = get_thread_heap();
    span->size_class = SIZE_CLASS_HUGE;
    span->span_count = (uint32_t)num_pages;
    span->align_offset = (uint32_t)align_offset;
    span->heap = heap;
    _memstat_add_peak(&_huge_pages_current, num_pages, _huge_pages_peak);
#if MEM_FIRST_CLASS_HEAPS
    _memspan_double_link_list_add(&heap->large_huge_span, span);
#endif
    ++heap->full_span_count;
    _memstat_add64(&_allocation_counter, 1);
    return ptr;
}

void *neo_allocator_realloc(void *blk, size_t len) {
    static const unsigned flags = 0;
    check_allocator_online();
    neo_assert(len < MAX_ALLOC_SIZE, "Allocation with invalid size: %zub, max: %zub", len, MAX_ALLOC_SIZE);
    size_t oldsize = 0;
    if (blk) {
        span_t *span = (span_t *)((uintptr_t)blk & _memory_span_mask);
        if (neo_likely(span->size_class < SIZE_CLASS_COUNT)) {
            neo_assert(span->span_count == 1, "Span counter corrupted");
            void *blocks_start = poff(span, SPAN_HEADER_SIZE);
            uint32_t block_offset = (uint32_t)pdelta(blk, blocks_start);
            uint32_t block_idx = block_offset / span->block_size;
            void *block = poff(blocks_start, (size_t)block_idx * span->block_size);
            if (!oldsize) { oldsize = (size_t)((ptrdiff_t)span->block_size - pdelta(blk, block)); }
            if ((size_t)span->block_size >= len) {
                if (blk != block && !(flags & MEM_NO_PRESERVE)) {
                    memmove(block, blk, oldsize);
                }
                return block;
            }
        } else if (span->size_class == SIZE_CLASS_LARGE) {
            size_t total_size = len + SPAN_HEADER_SIZE;
            size_t num_spans = total_size >> _memory_span_size_shift;
            if (total_size & (_memory_span_mask - 1)) { ++num_spans; }
            size_t current_spans = span->span_count;
            void *block = poff(span, SPAN_HEADER_SIZE);
            if (!oldsize) {
                oldsize = (current_spans * _memory_span_size) - (size_t)pdelta(blk, block) - SPAN_HEADER_SIZE;
            }
            if ((current_spans >= num_spans) && (total_size >= (oldsize >> 1))) {
                if (blk != block && !(flags & MEM_NO_PRESERVE)) {
                    memmove(block, blk, oldsize);
                }
                return block;
            }
        } else {
            size_t total_size = len + SPAN_HEADER_SIZE;
            size_t num_pages = total_size >> _memory_page_size_shift;
            if (total_size & (_memory_page_size - 1)) { ++num_pages; }
            size_t current_pages = span->span_count;
            void *block = poff(span, SPAN_HEADER_SIZE);
            if (!oldsize) {
                oldsize = (current_pages * _memory_page_size) - (size_t)pdelta(blk, block) - SPAN_HEADER_SIZE;
            }
            if ((current_pages >= num_pages) && (num_pages >= (current_pages >> 1))) {
                if (blk != block && !(flags & MEM_NO_PRESERVE)) {
                    memmove(block, blk, oldsize);
                }
                return block;
            }
        }
    } else {
        oldsize = 0;
    }
    if (flags & MEM_GROW_OR_FAIL) { return NULL; }
    size_t lower_bound = oldsize + (oldsize >> 2) + (oldsize >> 3);
    size_t new_size = (len > lower_bound) ? len : ((len > oldsize) ? lower_bound : len);
    void *block = neo_allocator_alloc(len);
    if (blk && block) {
        if (!(flags & MEM_NO_PRESERVE)) {
            memcpy(block, blk, oldsize < new_size ? oldsize : new_size);
        }
        neo_allocator_free(blk);
    }
    return block;
}

void *neo_allocator_realloc_aligned(void *blk, size_t len, size_t align) {
    static const unsigned flags = 0;
    check_allocator_online();
    neo_assert(len < MAX_ALLOC_SIZE, "Allocation with invalid size: %zub, max: %zub", len, MAX_ALLOC_SIZE);
    neo_assert(len + align >= len && (align & (align - 1)) == 0, "Allocation with invalid alignment: %zu", align);
    size_t oldsize = 0;
    if (align <= SMALL_GRANULARITY) { return neo_allocator_realloc(blk, len); }
    bool no_alloc = (flags & MEM_GROW_OR_FAIL) != 0;
    size_t usablesize = neo_allocator_bin_useable_size(blk);
    if ((usablesize >= len) && !((uintptr_t)blk & (align - 1))) {
        if (no_alloc || len >= (usablesize >> 1)) { return blk; }
    }
    void *block = (!no_alloc ? neo_allocator_alloc_aligned(len, align) : 0);
    if (neo_likely(block != 0)) {
        if (!(flags & MEM_NO_PRESERVE) && blk) {
            if (!oldsize) { oldsize = usablesize; }
            memcpy(block, blk, oldsize < len ? oldsize : len);
        }
        neo_allocator_free(blk);
    }
    return block;
}

size_t neo_allocator_bin_useable_size(void *blk) {
    if (neo_unlikely(!blk)) { return 0; }
    span_t *span = (span_t *)((uintptr_t)blk & _memory_span_mask);
    if (span->size_class < SIZE_CLASS_COUNT) {
        void *blocks_start = poff(span, SPAN_HEADER_SIZE);
        return span->block_size - ((size_t)pdelta(blk, blocks_start) % span->block_size);
    }
    if (span->size_class == SIZE_CLASS_LARGE) {
        size_t current_spans = span->span_count;
        return (current_spans * _memory_span_size) - (size_t)pdelta(blk, span);
    }
    size_t current_pages = span->span_count;
    return (current_pages * _memory_page_size) - (size_t)pdelta(blk, span);
}

void neo_allocator_free(void *blk) {
    check_allocator_online();
    if (neo_unlikely(!blk)) { return; }
    _memstat_add64(&_deallocation_counter, 1);
    span_t *span = (span_t *)((uintptr_t)blk & _memory_span_mask);
    if (neo_unlikely(!span)) { return; }
    if (neo_likely(span->size_class < SIZE_CLASS_COUNT)) {
        _memdeallocate_small_or_medium(span, blk);
    } else if (span->size_class == SIZE_CLASS_LARGE) {
        _memdeallocate_large(span);
    } else {
        _memdeallocate_huge(span);
    }
}

void neo_allocator_thread_enter(void) {
    check_allocator_online();
    memthread_initialize();
}

void neo_allocator_thread_leave(void) {
    check_allocator_online();
    memthread_finalize(1);
}
