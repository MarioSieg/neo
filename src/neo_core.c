/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_core.h"

#if NEO_OS_LINUX
#   include <unistd.h>
#   include <sys/mman.h>

#elif NEO_OS_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   error "Platform not yet implemented"
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

void neo_assert_impl(const char *expr, const char *file, int line) {
    neo_panic("%s:%d <- fatal neo error! assertion failed: '%s'", file, line, expr);
}

void *neo_sys_allocator(void *blk, size_t size) {
    if (neo_unlikely(!size)) { /* deallocation */
        neo_dealloc(blk);
        return NULL;
    } else if(neo_likely(!blk)) {  /* allocation */
        blk = neo_alloc_malloc(size);
        neo_assert(blk && "allocation failed");
        return blk;
    } else { /* reallocation */
        void *newblock = neo_alloc_realloc(blk, size);
        neo_assert(newblock && "reallocation failed");
        return newblock;
    }
}

#ifdef NEO_ALLOC_STATS
#define allocinfo(...) neo_info(__VA_ARGS__)
#else
#define allocinfo(...) (void)0
#endif

static volatile int64_t alloc_delta = 0;
static volatile int64_t alloc_total = 0;
void *neo_dbg_allocator(void *blk, size_t size) {
    if (!size) { /* deallocation */
        if (neo_likely(blk)) {
            blk = (uint8_t*)blk-sizeof(neo_alloc_header_t);
            neo_atomic_fetch_sub(&alloc_delta, (int64_t)((neo_alloc_header_t*)blk)->len, NEO_MEMORD_SEQ_CST);
            neo_dealloc(blk);
            allocinfo("[neo_alloc_hook] deallocated block: %p", blk);
        }
        return NULL;
    } else if(!blk) { /* allocation */
        size += sizeof(neo_alloc_header_t);
        blk = neo_alloc_malloc(size);
        neo_assert(blk && "allocation failed");
        neo_alloc_header_t *hdr = (neo_alloc_header_t*)blk;
        hdr->len = size;
        neo_atomic_fetch_add(&alloc_delta, (int64_t)size, NEO_MEMORD_SEQ_CST);
        neo_atomic_fetch_add(&alloc_total, (int64_t)size, NEO_MEMORD_RELX);
        void *r = (uint8_t*)blk+sizeof(*hdr);
        allocinfo("[neo_alloc_hook] allocated block: %p of %zub", blk, len);
        return r;
    } else { /* reallocation */
        blk = (uint8_t*)blk-sizeof(neo_alloc_header_t);
        size_t old = ((neo_alloc_header_t*)blk)->len;
        void *newblock = neo_alloc_realloc(blk, size);
        neo_assert(newblock && "reallocation failed");
        neo_alloc_header_t *hdr = (neo_alloc_header_t*)newblock;
        hdr->len = size;
        void *r = (uint8_t*)newblock+sizeof(*hdr);
        int64_t delta = llabs((int64_t)size-(int64_t)old);
        neo_atomic_fetch_add(&alloc_delta, delta, NEO_MEMORD_SEQ_CST);
        neo_atomic_fetch_add(&alloc_total, delta, NEO_MEMORD_RELX);
        allocinfo("[neo_alloc_hook] reallocated block: %p -> %p", blk, newblock);
        return r;
    }
}

#undef allocinfo

#if NEO_DBG
neo_allocproc_t *neo_alloc_hook = &neo_dbg_allocator; /* use debug-allocator in debug builds */
#else
neo_AllocProc *neo_alloc_hook = &neo_sys_allocator;
#endif

void neo_alloc_finalize(void) {
    if (neo_alloc_hook == &neo_dbg_allocator) {
        neo_alloc_dump();
        neo_info("[neo_alloc_hook] delta not freed: %" PRIi64 "b", alloc_delta);
        neo_assert(alloc_delta == 0 && "memory leak detected");
    }
}

void neo_alloc_dump(void) {
    if (neo_alloc_hook == &neo_dbg_allocator) {
        double mb = (double)alloc_total;
        mb /= (1024.0*1024.0);
        (void)mb;
        neo_info("[neo_alloc_hook] total allocated: %.03f MB", mb);
    }
}

#if NEO_OS_LINUX
neo_static_assert(sizeof(int) == sizeof(uint32_t));
static inline int map_protflags(neo_PageAccess acc) {
    neo_dbg_assert((acc & NEO_PA_R) != 0 && "page access not readable"); /* Pages must at least be readable because of the header */
    int prot = PROT_NONE;
    prot |= !!(acc & NEO_PA_R)*PROT_READ;
    prot |= !!(acc & NEO_PA_W)*PROT_WRITE;
    prot |= !!(acc & NEO_PA_X)*PROT_EXEC;
    return prot;
}
#elif NEO_OS_WINDOWS
neo_static_assert(sizeof(DWORD) == sizeof(uint32_t));
static inline DWORD map_protflags(neo_pageaccess_t acc) {
    DWORD r = 0;
    neo_dbg_assert((acc & NEO_PA_R) != 0 && "page access not readable"); /* Pages must at least be readable because of the header */
    if (acc == NEO_PA_R) {
        r = PAGE_READONLY;
    } else if (acc == (NEO_PA_R|NEO_PA_W)) {
        r = PAGE_READWRITE;
    } else if (acc == (NEO_PA_R|NEO_PA_X)) {
        r = PAGE_EXECUTE_READ;
    } else if (acc == (NEO_PA_R|NEO_PA_W|NEO_PA_X)) {
        r = PAGE_EXECUTE_READWRITE; /* *uncanny feeling* */
    } else {
        neo_panic("invalid page access combo");
    }
    return r;
}
#else
#   error "not implemented"
#endif

/*
** Ideally, I would like to protect the header against writing using:
** NEO_PA_R <- Read only
** But to protect only some bytes where sizeof(x) < PAGE_SIZE,
** we waste alot of space, since the header is tiny and I
** do not want to waste a whole page for it...
*/
bool neo_valloc(void **ptr, size_t size, neo_pageaccess_t access, void *hint, uint16_t poison) {
    if (neo_unlikely(!ptr)) { return false; }
    access &= 7;
    neo_pageaccess_t patch = access;
    access |= NEO_PA_R|NEO_PA_W; /* always needed for header, correct access is patched later using vprotect */
    size += sizeof(neo_vheader_t);
    void *base = NULL;
    uint32_t os_prot;
#if NEO_OS_LINUX
    int prot = map_protflags(access);
    int e = errno;
    base = mmap(hint, size, prot, MAP_PRIVATE|MAP_ANONYMOUS|(MAP_POPULATE*NEO_VALLOC_PREPOPULATE), -1, 0);
    errno = e;
    memcpy(&os_prot, &prot, sizeof(prot));
#elif NEO_OS_WINDOWS
    DWORD prot = map_protflags(access);
    DWORD err = GetLastError();
    base = VirtualAlloc(hint, size, MEM_RESERVE|MEM_COMMIT|MEM_TOP_DOWN, prot);
    SetLastError(err);
#if NEO_VALLOC_PREPOPULATE
    if (neo_likely(base)) {
        memset(base, 0, size); /* Prepopulate manually by writing */
    }
#endif
    memcpy(&os_prot, &prot, sizeof(prot));
#else
#   error "not implemented"
#endif
    if (neo_unlikely(!base)) {
        return false;
    }
    if ((poison >> 8) == 255) { /* poison before updating header */
#if NEO_OS_WINDOWS && NEO_VALLOC_PREPOPULATE
        if (poison & 255) {
#endif
        memset(base, poison & 255, size);
#if NEO_OS_WINDOWS && NEO_VALLOC_PREPOPULATE
        }
#endif
    }
    neo_vheader_t *hdr = (neo_vheader_t*)base;
    hdr->access = access & 7;
    hdr->os_access = os_prot;
    hdr->len = size;
    *ptr = (uint8_t*)base+sizeof(*hdr);
    return patch == access ? true : neo_vprotect(*ptr, patch); /* if access without write requested, patch now to access rights */
}

bool neo_vprotect(void *ptr, neo_pageaccess_t access) {
    if (neo_unlikely(!ptr)) { return false; }
    ptr = (uint8_t*)ptr-sizeof(neo_vheader_t);
    access &= 7;
    neo_vheader_t *hdr = (neo_vheader_t*)(ptr); /* header is always readable */
    if (neo_unlikely(hdr->access == access)) { return true; } /* protection already applied */
    if (neo_unlikely((hdr->access & NEO_PA_W) == 0)
        && !neo_vprotect((uint8_t*)ptr+sizeof(neo_vheader_t), NEO_PA_R | NEO_PA_W)) { /* we need write protection to update the header */
        return false;
    }
#if NEO_OS_LINUX
    int32_t prot = map_protflags(access);
    hdr->access = access;
    hdr->os_access = *(uint32_t*)&prot;
    int e = errno;
    int r = mprotect(ptr, hdr->len, prot);
    errno = e;
    if (neo_unlikely(r != 0)) { return false; }
#elif NEO_OS_WINDOWS
    DWORD dummy;
    DWORD flags = map_protflags(access);
    hdr->access = access;
    hdr->os_access = *(uint32_t*)&flags;
    DWORD e = GetLastError();
    BOOL r = VirtualProtect(ptr, hdr->len, flags, &dummy); /* protect destination */
    SetLastError(e);
    if (neo_unlikely(!r)) { return false; }
#else
#   error "not implemented"
#endif
    return true;
}

bool neo_vfree(void **ptr, bool poison) {
    if (neo_unlikely(!ptr)) { return false; }
    *ptr = (uint8_t*)*ptr-sizeof(neo_vheader_t);
    const neo_vheader_t *hdr = (const neo_vheader_t*)*ptr; /* header is always readable */
    if (neo_unlikely((hdr->access & (NEO_PA_R|NEO_PA_W)) == 0)) { /* region is not R|W but we need R|W for freeing */
        neo_vprotect((uint8_t*)*ptr+sizeof(neo_vheader_t), NEO_PA_R | NEO_PA_W); /* protect with original ptr because vprotect does the header handling itself */
    }
    size_t len = hdr->len;
    if (neo_likely(poison)) {
        memset(*ptr, (rand()%0x100)&255, len); /* pseudo random poison */
    }
#if NEO_OS_LINUX
    int e = errno;
    int r = munmap(*ptr, len);
    errno = e;
    if (neo_unlikely(r != 0)) { return false; }
#elif NEO_OS_WINDOWS
    DWORD err = GetLastError();
    BOOL r = VirtualFree(*ptr, 0, MEM_RELEASE);
    SetLastError(err);
    if (neo_unlikely(!r)) { return false; }
#else
#   error "not implemented"
#endif
    *ptr = NULL;
    return true;
}

uint32_t neo_hash_bernstein(const void *key, size_t len) {
    uint32_t r = 0x1505;
    const uint8_t *p = (const uint8_t*)key;
    for (size_t i = 0; i < len; ++i) {
        r = ((r << 5) + r) + p[i]; /* r * 33 + c */
    }
    return r;
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
    size_t blocks = len>>3;
    uint64_t h = 0x811c9dc5;
    for (size_t i = 0; i < blocks; ++i) {
        h ^= *(const uint64_t*)key;
        h *= 0xbf58476d1ce4e5b9;
        key = (const uint8_t*)key+8;
    }
    uint64_t l = len & 0xff;
    switch (l % 8) {
        case 7: l |= (uint64_t)((const uint8_t*)key)[6]<<56; NEO_FALLTHROUGH;
        case 6: l |= (uint64_t)((const uint8_t*)key)[5]<<48; NEO_FALLTHROUGH;
        case 5: l |= (uint64_t)((const uint8_t*)key)[4]<<40; NEO_FALLTHROUGH;
        case 4: l |= (uint64_t)((const uint8_t*)key)[3]<<32; NEO_FALLTHROUGH;
        case 3: l |= (uint64_t)((const uint8_t*)key)[2]<<24; NEO_FALLTHROUGH;
        case 2: l |= (uint64_t)((const uint8_t*)key)[1]<<16; NEO_FALLTHROUGH;
        case 1:
            l |= (uint64_t)*(const uint8_t*)key<<8;
            h ^= l;
            h *= 0xd6e8feb86659fd93;
    }
    return (uint32_t)(h^h>>32);
}

size_t neo_leb128_encode_u64(uint8_t *begin, uint8_t *end, uint64_t x) {
    neo_dbg_assert(begin);
    neo_dbg_assert(end);
    uint8_t *p = begin;
    while (p < end) {
        uint8_t k = x & 0x7f;
        x >>= 7;
        if (x != 0) {
            k |= 0x80;
        }
        *p++ = k;
        if ((k & 0x80) == 0) {
            break;
        }
    }
    return (size_t)(p - begin);
}

size_t neo_leb128_encode_i64(uint8_t *begin, uint8_t *end, int64_t x) {
    neo_dbg_assert(begin);
    neo_dbg_assert(end);
    uint8_t *p = begin;
    while (p < end) {
        uint8_t k = x & 0x7f;
        x >>= 7;
        if ((x == 0 && (k & 0x40) == 0) || (x == -1 && (k & 0x40) != 0)) {
            *p++ = k;
            break;
        }
        *p++ = k | 0x80;
    }
    return (size_t)(p - begin);
}

size_t neo_leb128_decode_u64(const uint8_t *begin, const uint8_t *end, uint64_t *o) {
    neo_dbg_assert(begin && end && o);
    const uint8_t *p = begin;
    uint64_t r = 0;
    unsigned s = 0;
    while (p < end) {
        uint8_t k = *p++;
        r |= ((uint64_t)(k & 0x7f)) << s;
        s += 7;
        if ((k & 0x80) == 0) {
            break;
        }
    }
    *o = r;
    return (size_t)(p - begin);
}

size_t neo_leb128_decode_i64(const uint8_t *begin, const uint8_t *end, int64_t *o) {
    neo_dbg_assert(begin && end && o);
    const uint8_t *p = begin;
    int64_t r = 0;
    unsigned s = 0;
    while (p < end) {
        uint8_t k = *p++;
        r |= ((int64_t)(k & 0x7f)) << s;
        s += 7;
        if ((k & 0x80) == 0) {
            if (s < (sizeof(*o) << 3) && ((k & 0x40) != 0)) {
                r |= -(int64_t)(UINT64_C(1) << s);
            }
            break;
        }
    }
    *o = r;
    return (size_t)(p - begin);
}

neo_unicode_result_t neo_utf8_validate(const uint8_t *buf, size_t len) {
    neo_dbg_assert(buf);
    size_t pos = 0;
    uint32_t code_point = 0;
    while (pos < len) {
        /* check if the next 8 bytes are ascii. */
        size_t next_pos = pos + 16;
        if (next_pos <= len) { /* if it is safe to read 8 more bytes, check that they are ascii */
            uint64_t v1 = *(const uint64_t *)(buf + pos);
            uint64_t v2 = *(const uint64_t *)(buf + pos + sizeof(v1));
            if (!((v1 | v2) & UINT64_C(0x8080808080808080))) {
                pos = next_pos;
                continue;
            }
        }
        uint8_t byte = buf[pos];
        while (byte < 0x80) {
            if (neo_likely(++pos == len)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_OK, .err_pos = len }; }
            byte = buf[pos];
        }
        if ((byte & 0xe0) == 0xc0) {
            next_pos = pos + 2;
            if (neo_unlikely(next_pos > len)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            code_point = (byte & 0x1fu) << 6u | (buf[pos + 1] & 0x3fu);
            if (neo_unlikely((code_point < 0x80) || (0x7ff < code_point))) { return (neo_unicode_result_t) { .code = NEO_UNIERR_OVERLONG, .err_pos = pos }; }
        } else if ((byte & 0xf0) == 0xe0) {
            next_pos = pos + 3;
            if (neo_unlikely(next_pos > len)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 2] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            code_point = (byte & 0xfu) << 12u | (buf[pos + 1] & 0x3fu) << 6u | (buf[pos + 2] & 0x3fu);
            if (neo_unlikely((code_point < 0x800) || (0xffff < code_point))) { return (neo_unicode_result_t) { .code = NEO_UNIERR_OVERLONG, .err_pos = pos }; }
            if (neo_unlikely(0xd7ff < code_point && code_point < 0xe000)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_SURROGATE, .err_pos = pos }; }
        } else if ((byte & 0xf8) == 0xf0) {
            next_pos = pos + 4;
            if (neo_unlikely(next_pos > len)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 1] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 2] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            if (neo_unlikely((buf[pos + 3] & 0xc0) != 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_SHORT, .err_pos = pos }; }
            // range check
            code_point = (byte & 0x7u) << 18u | (buf[pos + 1] & 0x3fu) << 12u | (buf[pos + 2] & 0x3fu) << 6u | (buf[pos + 3] & 0x3fu);
            if (neo_unlikely(code_point <= 0xffff)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_OVERLONG, .err_pos = pos }; }
            if (neo_unlikely(0x10ffff < code_point)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_LARGE, .err_pos = pos }; }
        } else { /* we either have too many continuation bytes or an invalid leading byte */
            if (neo_unlikely((byte & 0xc0) == 0x80)) { return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_LONG, .err_pos = pos }; }
            else { return (neo_unicode_result_t) { .code = NEO_UNIERR_HEADER_BITS, .err_pos = pos }; }
        }
        pos = next_pos;
    }
    return (neo_unicode_result_t) { .code = NEO_UNIERR_OK, .err_pos = len };
}

size_t neo_utf8_count_codepoints(const uint8_t *buf, size_t len) {
    neo_dbg_assert(buf);
    const int8_t *p = (const int8_t *)(buf);
    size_t r = 0;
    for(size_t i = 0; i < len; ++i) {
        if(p[i] > -65) { ++r; }
    }
    return r;
}

size_t neo_utf8_len_from_utf32(const uint32_t *buf, size_t len) {
    neo_dbg_assert(buf);
    size_t r = 0;
    for(size_t i = 0; i < len; ++i) {
        if(buf[i] <= 0x7f) { ++r; } /* ASCII */
        else if(buf[i] <= 0x7ff) { r += 2; } /* two-byte */
        else if(buf[i] <= 0xffff) { r += 3; } /* three-byte */
        else { r += 4; } /* four-bytes */
    }
    return r;
}

size_t neo_utf16_len_from_utf8(const uint8_t *buf, size_t len) {
    neo_dbg_assert(buf);
    const int8_t *p = (const int8_t *)(buf);
    size_t r = 0;
    for(size_t i = 0; i < len; ++i) {
        if(p[i] > -65) { ++r; }
        if((uint8_t)(p[i]) >= 240) { ++r; }
    }
    return r;
}

size_t neo_utf16_len_from_utf32(const uint32_t *buf, size_t len) {
    neo_dbg_assert(buf);
    const uint32_t* p = buf;
    size_t r = 0;
    for(size_t i = 0; i < len; ++i) {
        if(p[i] <= 0xffff) { ++r; } /* non-surrogate word */
        else { r += 2; } /* surrogate pair */
    }
    return r;
}

neo_unicode_result_t neo_utf32_validate(const uint32_t *buf, size_t len) {
    neo_dbg_assert(buf);
    size_t pos = 0;
    for(; pos < len; ++pos) {
        uint32_t word = buf[pos];
        if(word > 0x10ffff) {
            return (neo_unicode_result_t) { .code = NEO_UNIERR_TOO_LARGE, .err_pos = pos };
        }
        if(word >= 0xd800 && word <= 0xdfff) {
            return (neo_unicode_result_t) { .code = NEO_UNIERR_SURROGATE, .err_pos = pos };
        }
    }
    return (neo_unicode_result_t) { .code = NEO_UNIERR_OK, .err_pos = pos };
}

size_t neo_utf32_count_codepoints(const uint32_t* buf, size_t len) {
    (void)buf;
    return len;
}

size_t neo_valid_utf8_to_utf32(const uint8_t *buf, size_t len, uint32_t *out) {
    neo_dbg_assert(buf && out);
    size_t pos = 0;
    const uint32_t *start = out;
    while (pos < len) {
        if (pos + 8 <= len) { /* try to convert the next block of 8 ASCII bytes */
            if (!(*(const uint64_t *)(buf + pos) & UINT64_C(0x8080808080808080))) {
                size_t final_pos = pos + 8;
                while(pos < final_pos) {
                    *out++ = (uint32_t)(buf[pos]);
                    ++pos;
                }
                continue;
            }
        }
        uint8_t msb = buf[pos];
        if (neo_likely(msb < 0x80)) { /* 1 byte (ASCII) */
            *out++ = (uint32_t)(msb);
            ++pos;
        } else if ((msb & 0xe0) == 0xc0) { /* two-byte UTF-8 */
            if(neo_unlikely(pos + 1 >= len)) { break; }
            *out++ = (uint32_t)(((msb & 0x1f) << 6) | (buf[pos + 1] & 0x3f));
            pos += 2;
        } else if ((msb & 0xf0) == 0xe0) { /* three-byte UTF-8 */
            if(neo_unlikely(pos + 2 >= len)) { break; }
            *out++ = (uint32_t)(((msb & 0xf) << 12)
                                | ((buf[pos + 1] & 0x3f) << 6)
                                | (buf[pos + 2] & 0x3f));
            pos += 3;
        } else if ((msb & 0xf8) == 0xf0) { /* 4-byte UTF-8 word. */
            if(neo_unlikely(pos + 3 >= len)) { break; }
            uint32_t code_word = ((msb & 0x7u) << 18u )
                                 | ((buf[pos + 1] & 0x3fu) << 12u)
                                 | ((buf[pos + 2] & 0x3fu) << 6u)
                                 | (buf[pos + 3] & 0x3fu);
            *out++ = code_word;
            pos += 4;
        } else {
            return 0;
        }
    }
    return (size_t)(out - start);
}

size_t neo_valid_utf32_to_utf8(const uint32_t *buf, size_t len, uint8_t *out) {
    neo_dbg_assert(buf && out);
    size_t pos = 0;
    const uint8_t *start = out;
    while (pos < len) { /* try to convert the next block of 2 ASCII characters */
        if (pos + 2 <= len) { /* if it is safe to read 8 more bytes, check that they are ascii */
            if (!(*(const uint64_t *)(buf + pos) & UINT64_C(0xffffff80ffffff80))) {
                *out++ = buf[pos] & 0xff;
                *out++ = buf[pos+1] & 0xff;
                pos += 2;
                continue;
            }
        }
        uint32_t word = buf[pos];
        if (!(word & 0xffffff80)) {
            // will generate one UTF-8 bytes
            *out++ = (uint8_t)(word);
            ++pos;
        } else if (!(word & 0xfffff800)) { /* two utf-8 bytes */
            *out++ = ((word >> 6) | 0xc0) & 0xff;
            *out++ = ((word & 0x3f) | 0x80) & 0xff;
            ++pos;
        } else if (!(word & 0xffff0000)) { /* three utf-8 bytes */
            *out++ = ((word>>12) | 0xe0) & 0xff;
            *out++ = (((word>>6) & 0x3f) | 0x80) & 0xff;
            *out++ = ((word & 0x3f) | 0x80) & 0xff;
            ++pos;
        } else { /* four utf-8 bytes */
            *out++ = ((word>>18) | 0xf0) & 0xff;
            *out++ = (((word>>12) & 0x3f) | 0x80) & 0xff;
            *out++ = (((word>>6) & 0x3f) | 0x80) & 0xff;
            *out++ = ((word & 0x3f) | 0x80) & 0xff;
            ++pos;
        }
    }
    return (size_t)(out - start);
}

