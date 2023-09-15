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
