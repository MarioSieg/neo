/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_core.h"

#if NEO_OS_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <locale.h>
#   include <fcntl.h>
#   include <io.h>
#endif

void neo_assert_impl(const char *expr, const char *file, int line) {
    neo_panic("%s:%d <- Fatal internal NEO error! Assertion failed: '%s'", file, line, expr);
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

void neo_osi_init(void) {
#if NEO_OS_WINDOWS
    neo_assert(setlocale(LC_ALL, ".UTF-8") && "failed to set locale");
#endif
}

void neo_osi_shutdown(void) {

}

void *neo_defmemalloc(void *blk, size_t len) {
    if (neo_unlikely(!len)) { /* deallocation */
        neo_dealloc(blk);
        return NULL;
    } else if(neo_likely(!blk)) {  /* allocation */
        blk = neo_alloc_malloc(len);
        neo_assert(blk && "allocation failed");
        return blk;
    } else { /* reallocation */
        void *newblock = neo_alloc_realloc(blk, len);
        neo_assert(newblock && "reallocation failed");
        return newblock;
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
    neo_dbg_assert(fp && filepath && mode);
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

unicode_err_t neo_utf8_validate(const uint8_t *buf, size_t len, size_t *ppos) { /* Validates the UTF-8 string and returns an error code and error position. */
    neo_dbg_assert(buf && ppos);
    size_t pos = 0;
    uint32_t cp;
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
