/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

/*
** About this file:
** Important stuff used by the compiler and runtime are defined here.
*/

#ifndef NEO_CORE_H
#define NEO_CORE_H

/* -------- Prelude-------- */
#include <float.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEO_VER_MAJOR 2
#define NEO_VER_MINOR 1

#define NEO_DBG 0

#define NEO_COM_CLANG 0
#define NEO_COM_CLANG_ANALYZER 0
#define NEO_COM_GCC 0
#define NEO_COM_MSVC 0

#define NEO_CPU_ENDIAN_BIG 0
#define NEO_CPU_ENDIAN_LITTLE 0

#define NEO_CPU_AARCH64 0
#define NEO_CPU_MIPS 0
#define NEO_CPU_PPC 0
#define NEO_CPU_RISCV 0
#define NEO_CPU_AMD64 0

#define NEO_CRT_BIONIC 0
#define NEO_CRT_BSD 0
#define NEO_CRT_GLIBC 0
#define NEO_CRT_LIBCXX 0
#define NEO_CRT_MINGW 0
#define NEO_CRT_MSVC 0
#define NEO_CRT_NEWLIB 0

#define NEO_OS_ANDROID 0
#define NEO_OS_BSD 0
#define NEO_OS_HAIKU 0
#define NEO_OS_HURD 0
#define NEO_OS_IOS 0
#define NEO_OS_LINUX 0
#define NEO_OS_NX 0
#define NEO_OS_OSX 0
#define NEO_OS_PS4 0
#define NEO_OS_PS5 0
#define NEO_OS_RPI 0
#define NEO_OS_WINDOWS 0
#define NEO_OS_WINRT 0
#define NEO_OS_XBOXONE 0

#if defined(__clang__)
#	undef  NEO_COM_CLANG
#	define NEO_COM_CLANG (__clang_major__*10000+__clang_minor__*100+__clang_patchlevel__)
#	if defined(__clang_analyzer__)
#		undef  NEO_COM_CLANG_ANALYZER
#		define NEO_COM_CLANG_ANALYZER 1
#	endif
#elif defined(_MSC_VER)
#	undef  NEO_COM_MSVC
#	define NEO_COM_MSVC _MSC_VER
#elif defined(__GNUC__)
#	undef  NEO_COM_GCC
#	define NEO_COM_GCC (__GNUC__*10000+__GNUC_MINOR__*100+__GNUC_PATCHLEVEL__)
#else
#	error "Unknown NEO_COM_?"
#endif

#if defined(__arm__) && !defined(__aarch64__)
#   error "32-bit ARM is not supported. Please use a 64-bit ARM compiler."
#endif
#if (defined(_M_IX86) || defined(__i386__)) && (!defined(_M_X64) || defined(__x86_64__))
#   error "32-bit x86 is not supported. Please use a 64-bit x86 compiler."
#endif

#if defined(__aarch64__) || defined(_M_ARM)
#	undef  NEO_CPU_AARCH64
#	define NEO_CPU_AARCH64 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(__MIPSEL__) || defined(__mips_isa_rev) || defined(__mips64)
#	undef  NEO_CPU_MIPS
#	define NEO_CPU_MIPS 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(_M_PPC) || defined(__powerpc__) || defined(__powerpc64__)
#	undef  NEO_CPU_PPC
#	define NEO_CPU_PPC 1
#	define NEO_CACHE_LINE_SIZE 128
#elif defined(__riscv) || defined(__riscv__) || defined(RISCVEL)
#	undef  NEO_CPU_RISCV
#	define NEO_CPU_RISCV 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(_M_X64) || defined(__x86_64__)
#	undef  NEO_CPU_AMD64
#	define NEO_CPU_AMD64 1
#	define NEO_CACHE_LINE_SIZE 64
#else
#	error "Unknown NEO_CPU_?"
#endif

#if NEO_CPU_PPC
#	if defined(__BIG_ENDIAN__)
#		undef  NEO_CPU_ENDIAN_BIG
#		define NEO_CPU_ENDIAN_BIG 1
#	else
#		undef  NEO_CPU_ENDIAN_LITTLE
#		define NEO_CPU_ENDIAN_LITTLE 1
#	endif
#else
#	undef  NEO_CPU_ENDIAN_LITTLE
#	define NEO_CPU_ENDIAN_LITTLE 1
#endif

#if defined(_DURANGO) || defined(_XBOX_ONE)
#	undef  NEO_OS_XBOXONE
#	define NEO_OS_XBOXONE 1
#elif defined(_WIN32) || defined(_WIN64)
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	if defined(_MSC_VER) && (_MSC_VER >= 1700) && !defined(_USING_V110_SDK71_)
#		include <winapifamily.h>
#	endif
#	if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#		undef  NEO_OS_WINDOWS
#		if !defined(WINVER) && !defined(_WIN32_WINNT)
#			define WINVER 0x0601
#			define _WIN32_WINNT 0x0601
#		endif
#		define NEO_OS_WINDOWS _WIN32_WINNT
#	else
#		undef  NEO_OS_WINRT
#		define NEO_OS_WINRT 1
#	endif
#elif defined(__ANDROID__)
#	include <sys/cdefs.h>
#	undef  NEO_OS_ANDROID
#	define NEO_OS_ANDROID __ANDROID_API__
#elif defined(__VCCOREVER__)
#	undef  NEO_OS_RPI
#	define NEO_OS_RPI 1
#elif defined(__linux__)
#	undef  NEO_OS_LINUX
#	define NEO_OS_LINUX 1
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) || defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
#	undef  NEO_OS_IOS
#	define NEO_OS_IOS 1
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#	undef  NEO_OS_OSX
#	define NEO_OS_OSX __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#elif defined(__ORBIS__)
#	undef  NEO_OS_PS4
#	define NEO_OS_PS4 1
#elif defined(__PROSPERO__)
#	undef  NEO_OS_PS5
#	define NEO_OS_PS5 1
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#	undef  NEO_OS_BSD
#	define NEO_OS_BSD 1
#elif defined(__GNU__)
#	undef  NEO_OS_HURD
#	define NEO_OS_HURD 1
#elif defined(__NX__)
#	undef  NEO_OS_NX
#	define NEO_OS_NX 1
#elif defined(__HAIKU__)
#	undef  NEO_OS_HAIKU
#	define NEO_OS_HAIKU 1
#endif

#if defined(__BIONIC__)
#	undef  NEO_CRT_BIONIC
#	define NEO_CRT_BIONIC 1
#elif defined(_MSC_VER)
#	undef  NEO_CRT_MSVC
#	define NEO_CRT_MSVC 1
#elif defined(__GLIBC__)
#	undef  NEO_CRT_GLIBC
#	define NEO_CRT_GLIBC (__GLIBC__ * 10000 + __GLIBC_MINOR__ * 100)
#elif defined(__MINGW32__) || defined(__MINGW64__)
#	undef  NEO_CRT_MINGW
#	define NEO_CRT_MINGW 1
#elif defined(__apple_build_version__) || defined(__ORBIS__) || defined(__EMSCRIPTEN__) || defined(__llvm__) || defined(__HAIKU__)
#	undef  NEO_CRT_LIBCXX
#	define NEO_CRT_LIBCXX 1
#elif NEO_OS_BSD
#	undef  NEO_CRT_BSD
#	define NEO_CRT_BSD 1
#endif

#define NEO_OS_POSIX (NEO_OS_ANDROID || NEO_OS_BSD || NEO_OS_HAIKU || NEO_OS_HURD || NEO_OS_IOS || NEO_OS_LINUX || NEO_OS_NX || NEO_OS_OSX || NEO_OS_PS4 || NEO_OS_PS5 || NEO_OS_RPI)
#define NEO_OS_OS_CONSOLE (NEO_OS_NX || NEO_OS_PS4 || NEO_OS_PS5 || NEO_OS_WINRT || NEO_OS_XBOXONE)
#define NEO_OS_OS_DESKTOP (NEO_OS_BSD || NEO_OS_HAIKU || NEO_OS_HURD || NEO_OS_LINUX || NEO_OS_OSX || NEO_OS_WINDOWS)
#define NEO_OS_OS_EMBEDDED (NEO_OS_RPI)
#define NEO_OS_OS_MOBILE (NEO_OS_ANDROID || NEO_OS_IOS)

#if NEO_COM_GCC
#	define NEO_COM_NAME "GCC " NEO_STRINGIZE(__GNUC__) "." NEO_STRINGIZE(__GNUC_MINOR__) "." NEO_STRINGIZE(__GNUC_PATCHLEVEL__)
#elif NEO_COM_CLANG
#	define NEO_COM_NAME "Clang " NEO_STRINGIZE(__clang_major__) "." NEO_STRINGIZE(__clang_minor__) "." NEO_STRINGIZE(__clang_patchlevel__)
#elif NEO_COM_MSVC
#	if NEO_COM_MSVC >= 1930
#		define NEO_COM_NAME "MSVC 17.0"
#	elif NEO_COM_MSVC >= 1920
#		define NEO_COM_NAME "MSVC 16.0"
#	elif NEO_COM_MSVC >= 1910
#		define NEO_COM_NAME "MSVC 15.0"
#	elif NEO_COM_MSVC >= 1900
#		define NEO_COM_NAME "MSVC 14.0"
#	elif NEO_COM_MSVC >= 1800
#		define NEO_COM_NAME "MSVC 12.0"
#	elif NEO_COM_MSVC >= 1700
#		define NEO_COM_NAME "MSVC 11.0"
#	elif NEO_COM_MSVC >= 1600
#		define NEO_COM_NAME "MSVC 10.0"
#	elif NEO_COM_MSVC >= 1500
#		define NEO_COM_NAME "MSVC 9.0"
#	else
#		define NEO_COM_NAME "MSVC"
#	endif
#endif

#if NEO_OS_ANDROID
#	define NEO_OS_NAME "Android " NEO_STRINGIZE(NEO_OS_ANDROID)
#elif NEO_OS_BSD
#	define NEO_OS_NAME "BSD"
#elif NEO_OS_HAIKU
#	define NEO_OS_NAME "Haiku"
#elif NEO_OS_HURD
#	define NEO_OS_NAME "Hurd"
#elif NEO_OS_IOS
#	define NEO_OS_NAME "iOS"
#elif NEO_OS_LINUX
#	define NEO_OS_NAME "Linux"
#elif NEO_OS_NX
#	define NEO_OS_NAME "NX"
#elif NEO_OS_OSX
#	define NEO_OS_NAME "OSX"
#elif NEO_OS_PS4
#	define NEO_OS_NAME "PlayStation 4"
#elif NEO_OS_PS5
#	define NEO_OS_NAME "PlayStation 5"
#elif NEO_OS_RPI
#	define NEO_OS_NAME "RaspberryPi"
#elif NEO_OS_WINDOWS
#	define NEO_OS_NAME "Windows"
#elif NEO_OS_WINRT
#	define NEO_OS_NAME "WinRT"
#elif NEO_OS_XBOXONE
#	define NEO_OS_NAME "Xbox One"
#else
#	error "Unknown NEO_OS_?"
#endif

#if NEO_CPU_AARCH64
#	define NEO_CPU_NAME "AArch64"
#elif NEO_CPU_MIPS
#	define NEO_CPU_NAME "MIPS"
#elif NEO_CPU_PPC
#	define NEO_CPU_NAME "PowerPC"
#elif NEO_CPU_RISCV
#	define NEO_CPU_NAME "RISC-V"
#elif NEO_CPU_AMD64
#	define NEO_CPU_NAME "AMD64"
#endif

#if NEO_CRT_BIONIC
#	define NEO_CRT_NAME "Bionic libc"
#elif NEO_CRT_BSD
#	define NEO_CRT_NAME "BSD libc"
#elif NEO_CRT_GLIBC
#	define NEO_CRT_NAME "GNU C Library"
#elif NEO_CRT_MSVC
#	define NEO_CRT_NAME "MSVC C Runtime"
#elif NEO_CRT_MINGW
#	define NEO_CRT_NAME "MinGW C Runtime"
#elif NEO_CRT_LIBCXX
#	define NEO_CRT_NAME "Clang C Library"
#elif NEO_CRT_NEWLIB
#	define NEO_CRT_NAME "Newlib"
#elif NEO_CRT_NONE
#	define NEO_CRT_NAME "None"
#else
#	error "Unknown NEO_CRT_?"
#endif

#ifdef NDEBUG
#	undef NEO_DBG
#	define NEO_DBG 0
#else
#	undef NEO_DBG
#	define NEO_DBG 1
#endif


/* ---- Compiler Specific Intrinsics ---- */

#if NEO_COM_GCC ^ NEO_COM_CLANG
#   define NEO_EXPORT __attribute__((visibility("default")))
#   define NEO_NODISCARD __attribute__((warn_unused_result))
#	define NEO_NORET __attribute__((noreturn))
#	define NEO_ALIGN(x) __attribute__((aligned(x)))
#	define NEO_AINLINE inline __attribute__((always_inline))
#	define NEO_NOINLINE __attribute__((noinline))
#   define NEO_HOTPROC __attribute__((hot))
#   define NEO_COLDPROC __attribute__((cold))
#   define NEO_PACKED __attribute__((packed))
#   define NEO_FALLTHROUGH __attribute__((fallthrough))
#   define NEO_UNUSED __attribute__((unused))
#	define neo_likely(x) __builtin_expect(!!(x), 1)
#	define neo_unlikely(x) __builtin_expect(!!(x), 0)
#   if NEO_CPU_AMD64
#       define neo_unreachable() __asm__ __volatile__("int $3")
#   else
#       define neo_unreachable() __builtin_unreachable()
#   endif
#define neo_bsf32(x) (__builtin_ctz((x)))
#define neo_bsr32(x) (__builtin_clz((x))^31)
#   define neo_bswap32(x) __builtin_bswap32(x)
#   define neo_bswap64(x) __builtin_bswap64(x)
#   if NEO_CPU_AMD64
#   include <x86intrin.h>
#   define neo_rol64(x, n) __rolq(x,n)
#   define neo_ror64(x, n) __rorq(x,n)
#endif
#elif defined(_MSC_VER)
#	include <intrin.h>
#   define NEO_EXPORT __declspec(dllexport)
#   define NEO_NODISCARD _Check_return_
#	define NEO_NORET __declspec(noreturn)
#	define NEO_ALIGN(x)	__declspec(align(x))
#	define NEO_AINLINE __forceinline
#	define NEO_NOINLINE __declspec(noinline)
#   define NEO_HOTPROC
#   define NEO_COLDPROC
#   define NEO_PACKED
#   define NEO_FALLTHROUGH
#   define NEO_UNUSED
#	define neo_likely(x) (x)
#	define neo_unlikely(x) (x)
#   define neo_unreachable() __assume(0)
unsigned char _BitScanForward(unsigned long *r, unsigned long x);
unsigned char _BitScanReverse(unsigned long *r, unsigned long x);
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)
static NEO_AINLINE int neo_bsf32(uint32_t x) {
    unsigned long r;
    _BitScanForward(&r, (unsigned long)x);
    return (int)r;
}
static NEO_AINLINE int neo_bsr32(uint32_t x) {
    unsigned long r;
    _BitScanReverse(&r, (unsigned long)x);
    return (int)r;
}
extern unsigned long _byteswap_ulong(unsigned long x);
extern uint64_t _byteswap_uint64(uint64_t x);
#   define neo_bswap32(x) _byteswap_ulong(x)
#   define neo_bswap64(x) _byteswap_uint64(x)
#	define neo_rol64(x, n) _rotl64(x,n)
#	define neo_ror64(x, n) _rotr64(x, n)
/* MSVC does not define some feature macros of implied instruction sets. */
#if defined(__AVX2__) || defined(__AVX512F__)
#ifndef __FMA__
#   define __FMA__
#endif
#ifndef __F16C__
#   define __F16C__
#endif
#ifndef __SSE3__
#   define __SSE3__
#endif
#endif
#else
#   error "Unsupported compiler"
#endif

/* ---- Misc ---- */
#if NEO_OS_WINDOWS
#	include <malloc.h>
#elif NEO_OS_BSD
#   include <stdlib.h>
#elif NEO_OS_LINUX
#	include <alloca.h>
#else
#   error "unsupported platform"
#endif
#if !defined(neo_rol64) || !defined(neo_ror64)
#	define neo_rol64(x, n) (((x)<<(n))|((x)>>(-(int)(n)&((sizeof(x)<<3)-1))))
#	define neo_ror64(x, n) (((x)<<(-(int)(n)&((sizeof(x)<<3)-1)))|((x)>>(n)))
#endif
#define neo_padx(x, n) (((x)+((n)-1))&(~((n)-1)))
#define neo_bnd_check(p, o, l) neo_likely(((uintptr_t)(p)>=(uintptr_t)(o)) && ((uintptr_t)(p)<((uintptr_t)(o)+(l))))
#define neo_assert_name2(name, line) name ## line
#define neo_assert_name(line) neo_assert_name2(_assert_, line)
#define neo_static_assert(expr) extern void neo_assert_name(__LINE__)(bool STATIC_ASSERTION_FAILED[((expr)?1:-1)])
extern NEO_EXPORT NEO_COLDPROC NEO_NORET void neo_panic(const char *msg, ...);
#define NEO_SEP ,

/* Assert for debug and release builds. */
#define neo_assert(expr, msg, ...) \
    if (neo_unlikely(!(expr))) { \
        neo_panic("%s:%d Assertion failed: " #expr " <- " msg, __FILE__, __LINE__, ## __VA_ARGS__);\
    }

#if NEO_DBG
#   define neo_dassert(expr, msg, ...) neo_assert(expr, msg, ## __VA_ARGS__) /* Assert for debug only builds. */
#else
#   define neo_dassert(expr, msg, ...) (void)(expr) /* Disable in release builds. */
#endif

/* ---- Logging ---- */

#define NEO_ENUM_SEP ,
#define NEO_STRINGIZE(x) NEO_STRINGIZE2(x)
#define NEO_STRINGIZE2(x) #x
#define NEO_CCRED "\x1b[31m"
#define NEO_CCGREEN "\x1b[32m"
#define NEO_CCYELLOW "\x1b[33m"
#define NEO_CCBLUE "\x1b[34m"
#define NEO_CCMAGENTA "\x1b[35m"
#define NEO_CCCYAN "\x1b[36m"
#define NEO_CCRESET "\x1b[0m"

#define SRC_FILE __FILE__ ":" NEO_STRINGIZE(__LINE__)

#if NEO_DBG && !defined(NEO_NO_LOGGING)
#   define neo_info(msg, ...) fprintf(stdout,  "[neo] " SRC_FILE " " msg "\n", ## __VA_ARGS__)
#   define neo_warn(msg, ...) fprintf(stderr,  "[neo] " SRC_FILE " " NEO_CCYELLOW msg NEO_CCRESET "\n", ## __VA_ARGS__)
#else
#   define neo_info(msg, ...)
#   define neo_warn(msg, ...)
#endif

#ifndef NEO_NO_LOGGING /* By default, error logging is enabled in production. */
#   define neo_error(msg, ...) fprintf(stderr, "[neo] " SRC_FILE " " NEO_CCRED msg NEO_CCRESET "\n", ## __VA_ARGS__)
#else
#   define neo_error(msg, ...)
#endif

/* ---- OS interface ---- */
typedef struct neo_osi_t {
    uint32_t page_size;
} neo_osi_t;

extern NEO_EXPORT void neo_osi_init(void);
extern NEO_EXPORT void neo_osi_shutdown(void);
extern NEO_EXPORT const neo_osi_t *neo_osi;
extern NEO_EXPORT uint64_t neo_hp_clock_ms(void);
extern NEO_EXPORT uint64_t neo_hp_clock_us(void);

/* ---- Memory ---- */

#if NEO_COM_GCC ^ NEO_COM_CLANG
#   define NEO_ALLOC_ROUTINE __attribute__((malloc))
#   define NEO_ALLOC_ROUTINE_SIZE(size) __attribute__((alloc_size(size)))
#else
#   define NEO_ALLOC_ROUTINE_SIZE(size)
#   define NEO_ALLOC_ROUTINE
#endif

/* Bundled memory allocator API. */
#define NEO_ALLOC_HEAP_ARRAY_SIZE 47
#define NEO_ALLOC_ENABLE_THREAD_CACHE 1
#define NEO_ALLOC_ENABLE_GLOBAL_CACHE 1
#define NEO_ALLOC_ENABLE_VALIDATION NEO_DBG
#define NEO_ALLOC_ENABLE_STATS NEO_DBG
#define NEO_ALLOC_ENABLE_ASSERTS NEO_DBG
#define NEO_ALLOC_ENABLE_PRELOAD 0
#define NEO_ALLOC_ENABLE_UNMAP 0
#define NEO_ALLOC_ENABLE_UNLIMITED_CACHE 1
#define NEO_ALLOC_ENABLE_ADAPTIVE_THREAD_CACHE 1
#define NEO_ALLOC_SPAN_MAP_COUNT 64
#define NEO_ALLOC_GLOBAL_CACHE_MULTIPLIER 8

typedef struct neo_alloc_global_stats_t {
    size_t mapped;
    size_t mapped_peak;
    size_t cached;
    size_t huge_alloc;
    size_t huge_alloc_peak;
    size_t mapped_total;
    size_t unmapped_total;
} neo_alloc_global_stats_t;

typedef struct neo_alloc_thread_stats_t {
    size_t sizecache;
    size_t spancache;
    size_t thread_to_global;
    size_t global_to_thread;
    struct {
        size_t current;
        size_t peak;
        size_t to_global;
        size_t from_global;
        size_t to_cache;
        size_t from_cache;
        size_t to_reserved;
        size_t from_reserved;
        size_t map_calls;
    } span_use[64];
    struct {
        size_t alloc_current;
        size_t alloc_peak;
        size_t alloc_total;
        size_t free_total;
        size_t spans_to_cache;
        size_t spans_from_cache;
        size_t spans_from_reserved;
        size_t map_calls;
    } size_use[128];
} neo_alloc_thread_stats_t;

typedef struct neo_alloc_config_t {
    void *(*memory_map)(size_t size, size_t *offset);
    void (*memory_unmap)(void *address, size_t size, size_t offset, size_t release);
    int (*map_fail_callback)(size_t size);
    size_t page_size;
    size_t span_size;
    size_t span_map_count;
    int enable_huge_pages;
    const char *page_name;
    const char *huge_page_name;
} neo_alloc_config_t;

extern NEO_EXPORT void neo_allocator_init(void); /* Initialize global memory allocator. */
extern NEO_EXPORT void neo_allocator_shutdown(void); /* Shutdown global memory allocator. */
extern NEO_EXPORT NEO_NODISCARD NEO_ALLOC_ROUTINE NEO_ALLOC_ROUTINE_SIZE(1) NEO_HOTPROC void *neo_allocator_alloc(size_t len);
extern NEO_EXPORT NEO_NODISCARD NEO_ALLOC_ROUTINE NEO_ALLOC_ROUTINE_SIZE(1) NEO_HOTPROC void *neo_allocator_alloc_aligned(size_t len, size_t align);
extern NEO_EXPORT NEO_NODISCARD NEO_ALLOC_ROUTINE NEO_ALLOC_ROUTINE_SIZE(2) NEO_HOTPROC void *neo_allocator_realloc(void *blk, size_t len);
extern NEO_EXPORT NEO_NODISCARD NEO_ALLOC_ROUTINE NEO_ALLOC_ROUTINE_SIZE(2) NEO_HOTPROC void *neo_allocator_realloc_aligned(void *blk, size_t len, size_t align);
extern NEO_EXPORT NEO_NODISCARD size_t neo_allocator_bin_useable_size(void *blk);
extern NEO_EXPORT void neo_allocator_free(void *blk);
extern NEO_EXPORT void neo_allocator_thread_enter(void); /* Setup thread-local allocator state for a new thread. */
extern NEO_EXPORT void neo_allocator_thread_leave(void); /* Setup thread-local allocator state for a new thread. */
extern NEO_EXPORT void neo_allocator_memthread_collect(void); /* Collect local heaps. */

/* System memory allocator. */
#ifdef NEO_USE_SYSTEM_ALLOCATOR
static inline void *neo_memalloc_impl(void *blk, size_t len) {
    if (!len) { /* deallocation */
        free(blk);
        return NULL;
    } else if(!blk) {  /* allocation */
        blk = malloc(len);
        neo_assert(blk != NULL, "Memory allocation of %zub failed", len);
        return blk;
    } else { /* reallocation */
        void *newblock = realloc(blk, len);
        neo_assert(newblock != NULL, "Memory reallocation of %zub failed", len);
        return newblock;
    }
}
#else /* Use faster, bundled allocator. */
static inline void *neo_memalloc_impl(void *blk, size_t len) {
    if (!len) { /* deallocation */
        neo_allocator_free(blk);
        return NULL;
    } else if(!blk) {  /* allocation */
        blk = neo_allocator_alloc(len);
        neo_assert(blk != NULL, "Memory allocation of %zub failed", len);
        return blk;
    } else { /* reallocation */
        void *newblock = neo_allocator_realloc(blk, len);
        neo_assert(newblock != NULL, "Memory reallocation of %zub failed", len);
        return newblock;
    }
}
#endif
#define neo_memalloc(blk, len) neo_memalloc_impl(blk, len)

/*
** A simple sequential (bump) allocator.
** Memory allocation is fast, but individual block deallocation is not possible.
** All memory is freed simultaneously when the pool is destroyed with neo_mempool_free.
*/
typedef struct neo_mempool_t {
    void *top;
    size_t len;
    size_t cap;
    size_t num_allocs;
} neo_mempool_t;

#define neo_mempool_getelementptr(self, idx, type) (((type *)((self).top))+(idx))
#define neo_mempool_top(self, type) ((type *)(self).needle)
extern NEO_EXPORT void neo_mempool_init(neo_mempool_t *self, size_t cap);
extern NEO_EXPORT void *neo_mempool_alloc(neo_mempool_t *self, size_t len);
extern NEO_EXPORT void *neo_mempool_alloc_aligned(neo_mempool_t *self, size_t len, size_t align);
extern NEO_EXPORT size_t neo_mempool_alloc_idx(neo_mempool_t *self, size_t len, uint32_t base, size_t lim, void **pp);
extern NEO_EXPORT void *neo_mempool_realloc(neo_mempool_t *self, void *blk, size_t oldlen, size_t newlen);
extern NEO_EXPORT void neo_mempool_reset(neo_mempool_t *self);
extern NEO_EXPORT void neo_mempool_free(neo_mempool_t *self);

/* ---- Types ---- */

typedef int64_t neo_int_t;
typedef double neo_float_t;
typedef uint32_t neo_char_t;
typedef uint8_t neo_bool_t;

typedef uint64_t neo_uint_t; /* Unsigned equivalent of neo_int_t, but no Neo builtin type */

neo_static_assert(sizeof(neo_int_t) == 8);
neo_static_assert(sizeof(neo_uint_t) == sizeof(neo_int_t));
neo_static_assert(sizeof(neo_float_t) == 8);
neo_static_assert(sizeof(neo_char_t) == 4);
neo_static_assert(sizeof(neo_bool_t) == 1);
neo_static_assert(-1 == ~0); /* Check for 2's complement integers */

#define NEO_INT_C(x) INT64_C(x)
#define NEO_UINT_C(x) UINT64_C(x)
#define NEO_INT_MAX INT64_MAX
#define NEO_INT_MIN INT64_MIN
#define NEO_FLOAT_MAX DBL_MAX
#define NEO_FLOAT_MIN DBL_MIN
#define NEO_CHAR_MAX UINT32_MAX
#define NEO_CHAR_MIN 0
#define NEO_TRUE 1	/* Only for Neo's builtin neo_bool_t type. For regular C code use bool and true/false from stdbool.h! */
#define NEO_FALSE 0 /* Only for Neo's builtin neo_bool_t type. For regular C code use bool and true/false from stdbool.h! */

neo_static_assert(NEO_INT_MAX == INT64_MAX && NEO_INT_MAX > 0);
neo_static_assert(NEO_INT_MIN == INT64_MIN && NEO_INT_MIN < 0);

/* ---- Thread Implementation ---- */
#if NEO_COM_GCC ^ NEO_COM_CLANG
#define NEO_THREAD_LOCAL __thread
typedef enum neo_memord_t {
    NEO_MEMORD_RELX = __ATOMIC_RELAXED,
    NEO_MEMORD_ACQ = __ATOMIC_ACQUIRE,
    NEO_MEMORD_REL = __ATOMIC_RELEASE,
    NEO_MEMORD_ACQ_REL = __ATOMIC_ACQ_REL,
    NEO_MEMORD_SEQ_CST = __ATOMIC_SEQ_CST
} neo_memord_t;
static NEO_AINLINE void neo_atomic_store(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    __atomic_store_n(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_load(volatile int64_t *ptr, neo_memord_t order) {
    return __atomic_load_n(ptr, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_add(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_fetch_add(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_sub(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_fetch_sub(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_and(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_fetch_and(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_or(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_fetch_or(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_xor(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_fetch_xor(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_exchange(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    return __atomic_exchange_n(ptr, x, (int)order);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_weak(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_memord_t order_succ, neo_memord_t order_fail) {
    return __atomic_compare_exchange(ptr, exp, des, true, (int)order_succ, (int)order_fail);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_strong(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_memord_t order_succ, neo_memord_t order_fail) {
    return __atomic_compare_exchange(ptr, exp, des, false, (int)order_succ, (int)order_fail);
}
#if /* We only support 64-bit, remember? */ \
   (defined(__GLIBC__)   && (NEO_CPU_AMD64 ^ NEO_CPU_AARCH64)) \
|| (defined(__APPLE__)   && (NEO_CPU_AMD64 ^ NEO_CPU_AARCH64)) \
|| (defined(__BIONIC__)  && (NEO_CPU_AMD64 ^ NEO_CPU_AARCH64)) \
|| (defined(__FreeBSD__) && (NEO_CPU_AMD64 ^ NEO_CPU_AARCH64)) \
|| (defined(__OpenBSD__) && (NEO_CPU_AMD64 ^ NEO_CPU_AARCH64))
static inline void *neo_tls_get_slot(size_t slot) { /* Lookup thread-local-storage (TLS) slot via specific TLS register at specified index. */
    void *res;
    const size_t ofs = slot*sizeof(void *);
    #if defined(__APPLE__) && defined(NEO_CPU_AMD64)
        __asm__ __volatile__(
            "movq %%gs:%1, %0"
            : "=r" (res) : "m" (*((void **)ofs)) :
        );  /* x86-64 OSX: %GS */
    #elif defined(NEO_CPU_AMD64)
        __asm__ __volatile__(
            "movq %%fs:%1, %0"
            : "=r" (res) : "m" (*((void **)ofs)) :
        );  /* x86-64 Linux and BSD: %FS. */
    #elif defined(NEO_CPU_AARCH64)
        void **tcb; (void)ofs;
        #if defined(__APPLE__) /* M1 fixup. */
            __asm__ __volatile__(
                "mrs %0, tpidrro_el0\n"
                "bic %0, %0, #7"
                : "=r" (tcb)
            );
        #else
            __asm__ __volatile__(
                "mrs %0, tpidr_el0"
                : "=r" (tcb)
            );
        #endif
        res = tcb[slot];
    #else
    #   error "Unsupported architecture"
    #endif
    return res;
}
static NEO_AINLINE size_t neo_tid(void) {
#ifdef __BIONIC__
    return (uintptr_t)neo_tls_get_slot(1);
#else
    return (uintptr_t)neo_tls_get_slot(0);
#endif
}
#else
extern NEO_EXPORT NEO_THREAD_LOCAL void *volatile neo_tls_proxy;
/* Portable impl. */
static NEO_AINLINE size_t neo_tid(void) {
    return (uintptr_t)&neo_tls_proxy;
}
#endif

#elif NEO_COM_MSVC
#define NEO_THREAD_LOCAL __declspec(thread)
typedef enum {
    NEO_MEMORD_RELX,
    NEO_MEMORD_ACQ,
    NEO_MEMORD_REL,
    NEO_MEMORD_ACQ_REL,
    NEO_MEMORD_SEQ_CST
} neo_memord_t;
static NEO_AINLINE void neo_atomic_store(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    _InterlockedExchange64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_load(volatile int64_t *ptr, neo_memord_t order) {
    (void)order;
    int64_t r;
    _InterlockedExchange64(&r, *ptr);
    return r;
}
static NEO_AINLINE int64_t neo_atomic_fetch_add(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedExchangeAdd64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_sub(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedExchangeAdd64(ptr, -x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_and(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedAnd64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_or(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedOr64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_xor(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedXor64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_exchange(volatile int64_t *ptr, int64_t x, neo_memord_t order) {
    (void)order;
    return _InterlockedExchange64(ptr, x);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_weak(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_memord_t order_succ, neo_memord_t order_fail) {
    (void)order_succ;
    (void)order_fail;
    return _InterlockedCompareExchange64(ptr, *des, *exp) == *exp;
}
static NEO_AINLINE bool neo_atomic_compare_exchange_strong(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_memord_t order_succ, neo_memord_t order_fail) {
    (void)order_succ;
    (void)order_fail;
    return _InterlockedCompareExchange64(ptr, *des, *exp) == *exp;
}
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static NEO_AINLINE size_t neo_tid(void) {
  return (size_t)NtCurrentTeb(); /* x86-64 or AArch64. */
}
#else
#   error "Unsupported compiler"
#endif

typedef enum rtag_t { /* Record type tag. */
    RT_INT = 0,
    RT_FLOAT,
    RT_CHAR,
    RT_BOOL,
    RT_REF,
    RT__LEN
} rtag_t;
neo_static_assert(RT__LEN <= 255);

/* Untagged raw record value data. */
typedef union NEO_ALIGN(8) record_t {
    neo_int_t as_int;
    neo_uint_t as_uint;
    neo_float_t as_float;
    neo_char_t as_char;
    neo_bool_t as_bool;
    void *as_ref; /* Reference type. */
    uint64_t ru64;
    int64_t ri64;
    uint32_t ru32;
    int32_t ri32;
    uint16_t ru16;
    int16_t ri16;
    uint8_t ru8;
    int8_t ri8;
    struct {
        uint32_t lo;
        uint32_t hi;
    } ru32x2;
} record_t;
neo_static_assert(sizeof(record_t) == 8);
#define rec_setnan(o) ((o).ru64 = 0xfff8000000000000ull) /* Set NaN. */
#define rec_setpinf(o) ((o).ru64 = 0x7ff0000000000000ull) /* Set +Inf. */
#define rec_setminf(o) ((o).ru64 = 0xfff0000000000000ull) /* Set -Inf. */
#define rec_isnan(o) (((o).ru64 & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) /* Check if NaN. */

/* Tagged record value. */
typedef struct NEO_ALIGN(8) tvalue_t {
    rtag_t tag : 8;
    uint64_t reserved : 56; /* Reserved for future use. */
    record_t val;
} tvalue_t;
neo_static_assert(sizeof(tvalue_t) == 16);

extern NEO_EXPORT bool record_eq(record_t a, record_t b, rtag_t tag);

typedef enum neo_fmode_t {
    NEO_FMODE_R /* read */,
    NEO_FMODE_W /* write */,
    NEO_FMODE_A /* append */,
    NEO_FMODE_BIN /* read */,
    NEO_FMODE_TXT /* text */
} neo_fmode_t;
extern NEO_EXPORT bool neo_fopen(FILE **fp, const uint8_t *filepath, /* neo_fmode_t */ int mode);

/* ---- Unicode Handling ---- */

typedef enum neo_unicode_err_t {
    NEO_UNIERR_OK,
    NEO_UNIERR_TOO_SHORT,
    NEO_UNIERR_TOO_LONG,
    NEO_UNIERR_TOO_LARGE,
    NEO_UNIERR_OVERLONG,
    NEO_UNIERR_HEADER_BITS,
    NEO_UNIERR_SURROGATE
} neo_unicode_error_t;
extern NEO_EXPORT neo_unicode_error_t neo_utf8_validate(const uint8_t *buf, size_t len, size_t *ppos);
extern NEO_EXPORT bool neo_utf8_is_ascii(const uint8_t *buf, size_t len);

/* ---- Hash Functions ---- */

extern NEO_EXPORT uint32_t neo_hash_x17(const void *key, size_t len);
extern NEO_EXPORT uint32_t neo_hash_fnv1a(const void *key, size_t len);
extern NEO_EXPORT uint64_t neo_hash_mumrmur3_86_128(const void *key, size_t len, uint32_t seed);
extern NEO_EXPORT uint64_t neo_hash_sip64(const void *key, size_t len, uint64_t seed0, uint64_t seed1);

/* ---- Utilities ---- */

extern NEO_EXPORT uint8_t *neo_strdup2(const uint8_t *str, size_t *out_len); /* Duplicate zero-terminated string to new dynamically allocated memory. */
extern NEO_EXPORT char *neo_strdup(const char *str, size_t *out_len); /* Duplicate zero-terminated string to new dynamically allocated memory. */
extern NEO_EXPORT void neo_printutf8(FILE *f, const uint8_t *str); /* Print UTF-8 string to stdout. */

/* ---- Frozen Embedded BLOBS (contains in neo_blobs.c) ---- */

extern NEO_EXPORT const char neo_blobs_license[];

/* ---- String Scanning ---- */

/* Options for accepted/returned formats. */
typedef enum neo_strscan_opt_t {
    NEO_STRSCAN_OPT_NONE = 0,
    NEO_STRSCAN_OPT_TOINT = 1 << 0,  /* Convert to int32_t, if possible. */
    NEO_STRSCAN_OPT_TONUM = 1 << 1,  /* Always convert to double. */
    NEO_STRSCAN_OPT_IMAG = 1 << 2, /* Allow imaginary numbers. */
    NEO_STRSCAN_OPT_LL = 1 << 3, /* Allow long long suffix. */
    NEO_STRSCAN_OPT_C = 1 << 4 /* Allow complex suffix. */
} neo_strscan_opt_t;

/* Returned format. */
typedef enum neo_strscan_format_t {
    NEO_STRSCAN_ERROR,
    NEO_STRSCAN_EMPTY,
    NEO_STRSCAN_NUM, NEO_STRSCAN_IMAG,
    NEO_STRSCAN_INT, NEO_STRSCAN_U32, NEO_STRSCAN_I64, NEO_STRSCAN_U64,
} neo_strscan_format_t;

/* Scan string containing a number. Returns format. Returns value in o. */
/* I (IMAG), U (U32), LL (I64), ULL/LLU (U64), L (long), UL/LU (ulong). */
/* NYI: f (float). Not needed until cp_number() handles non-integers. */
extern NEO_EXPORT neo_strscan_format_t neo_strscan_scan(const uint8_t *p, size_t len, record_t *o, neo_strscan_opt_t opt);

/* ---- String Formatting ---- */

extern NEO_EXPORT uint8_t *neo_fmt_int(uint8_t *p, neo_int_t x); /* Format int to buffer. Does NOT zero-terminate the buffer. */
extern NEO_EXPORT uint8_t *neo_fmt_float(uint8_t *p, neo_float_t x); /* Format float to buffer. Does NOT zero-terminate the buffer. */
extern NEO_EXPORT uint8_t *neo_fmt_ptr(uint8_t *p, const void *v); /* Format ptr to buffer. Does NOT zero-terminate the buffer. */

#ifdef __cplusplus
}
#endif
#endif
