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
    osi_data.page_size = info.dwPageSize ? (uint32_t)info.dwPageSize : 1<<12;
#elif NEO_OS_LINUX || NEO_OS_BSD
    long ps = sysconf(_SC_PAGESIZE);
    osi_data.page_size = ps > 0 ? (uint32_t)ps : 1<<12;
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
    self->needle = neo_memalloc(NULL, cap);
    memset(self->needle, 0, cap); /* Zero the memory. */
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
        self->needle = neo_memalloc(self->needle, self->cap);
        size_t delta = self->cap-old;
        memset((uint8_t *)self->needle+old, 0, delta); /* Zero the new memory. */
    }
    void *p = (uint8_t *)self->needle+self->len;
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

void neo_mempool_free(neo_mempool_t *self) {
    neo_dassert(self);
    neo_memalloc(self->needle, 0);
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
    bool heap = len >= (1u<<12);
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
            uint64_t v1 = *(const uint64_t *)(buf+pos);
            uint64_t v2 = *(const uint64_t *)(buf+pos+sizeof(v1));
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

uint8_t *neo_strdup2(const uint8_t *str) {
    neo_assert(str && "Invalid ptr to clone");
    size_t len = strlen((const char *)str); /* strlen also works with UTF-8 strings to find the end \0. */
    uint8_t *dup = neo_memalloc(NULL, (len+1)*sizeof(*dup));
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

char *neo_strdup(const char *str) {
    neo_assert(str && "Invalid ptr to clone");
    size_t len = strlen(str); /* strlen also works with UTF-8 strings to find the end \0. */
    char *dup = neo_memalloc(NULL, (len+1)*sizeof(*dup));
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}
