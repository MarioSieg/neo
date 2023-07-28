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

#if _WIN32
#	include <malloc.h>
#else
#	include <alloca.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NEO_VER_MAJOR 0
#define NEO_VER_MINOR 1

/* ---- Compiler & Platform Macros ---- */

#define NEO_DBG 0 /* debug mode */

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
#	define NEO_COM_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#	if defined(__clang_analyzer__)
#		undef  NEO_COM_CLANG_ANALYZER
#		define NEO_COM_CLANG_ANALYZER 1
#	endif
#elif defined(_MSC_VER)
#	undef  NEO_COM_MSVC
#	define NEO_COM_MSVC _MSC_VER
#elif defined(__GNUC__)
#	undef  NEO_COM_GCC
#	define NEO_COM_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#	error "Unknown NEO_COM_?"
#endif

#if defined(__arm__)     \
 || defined(__aarch64__) \
 || defined(_M_ARM)
#	undef  NEO_CPU_AARCH64
#	define NEO_CPU_AARCH64 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(__MIPSEL__)     \
 ||   defined(__mips_isa_rev) \
 ||   defined(__mips64)
#	undef  NEO_CPU_MIPS
#	define NEO_CPU_MIPS 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(_M_PPC)        \
 ||   defined(__powerpc__)   \
 ||   defined(__powerpc64__)
#	undef  NEO_CPU_PPC
#	define NEO_CPU_PPC 1
#	define NEO_CACHE_LINE_SIZE 128
#elif defined(__riscv)   \
 ||   defined(__riscv__) \
 ||   defined(RISCVEL)
#	undef  NEO_CPU_RISCV
#	define NEO_CPU_RISCV 1
#	define NEO_CACHE_LINE_SIZE 64
#elif defined(_M_IX86)    \
 ||   defined(_M_X64)     \
 ||   defined(__i386__)   \
 ||   defined(__x86_64__)
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
#               define WINVER 0x0601
#				define _WIN32_WINNT 0x0601
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
#elif  defined(__linux__)
#	undef  NEO_OS_LINUX
#	define NEO_OS_LINUX 1
#elif  defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) \
	|| defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
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
#elif  defined(__FreeBSD__)        \
	|| defined(__FreeBSD_kernel__) \
	|| defined(__NetBSD__)         \
	|| defined(__OpenBSD__)        \
	|| defined(__DragonFly__)
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

#if !NEO_CRT_NONE
#	if defined(__BIONIC__)
#		undef  NEO_CRT_BIONIC
#		define NEO_CRT_BIONIC 1
#	elif defined(_MSC_VER)
#		undef  NEO_CRT_MSVC
#		define NEO_CRT_MSVC 1
#	elif defined(__GLIBC__)
#		undef  NEO_CRT_GLIBC
#		define NEO_CRT_GLIBC (__GLIBC__ * 10000 + __GLIBC_MINOR__ * 100)
#	elif defined(__MINGW32__) || defined(__MINGW64__)
#		undef  NEO_CRT_MINGW
#		define NEO_CRT_MINGW 1
#	elif defined(__apple_build_version__) || defined(__ORBIS__) || defined(__EMSCRIPTEN__) || defined(__llvm__) || defined(__HAIKU__)
#		undef  NEO_CRT_LIBCXX
#		define NEO_CRT_LIBCXX 1
#	elif NEO_OS_BSD
#		undef  NEO_CRT_BSD
#		define NEO_CRT_BSD 1
#	endif

#	if !NEO_CRT_BIONIC \
	&& !NEO_CRT_BSD    \
	&& !NEO_CRT_GLIBC  \
	&& !NEO_CRT_LIBCXX \
	&& !NEO_CRT_MINGW  \
	&& !NEO_CRT_MSVC   \
	&& !NEO_CRT_NEWLIB
#		undef  NEO_CRT_NONE
#		define NEO_CRT_NONE 1
#	endif
#endif

#define NEO_OS_POSIX (0   \
	||  NEO_OS_ANDROID    \
	||  NEO_OS_BSD        \
	||  NEO_OS_HAIKU      \
	||  NEO_OS_HURD       \
	||  NEO_OS_IOS        \
	||  NEO_OS_LINUX      \
	||  NEO_OS_NX         \
	||  NEO_OS_OSX        \
	||  NEO_OS_PS4        \
	||  NEO_OS_PS5        \
	||  NEO_OS_RPI        \
	)

#define NEO_OS_NONE !(0   \
	||  NEO_OS_ANDROID    \
	||  NEO_OS_BSD        \
	||  NEO_OS_HAIKU      \
	||  NEO_OS_HURD       \
	||  NEO_OS_IOS        \
	||  NEO_OS_LINUX      \
	||  NEO_OS_NX         \
	||  NEO_OS_OSX        \
	||  NEO_OS_PS4        \
	||  NEO_OS_PS5        \
	||  NEO_OS_RPI        \
	||  NEO_OS_WINDOWS    \
	||  NEO_OS_WINRT      \
	||  NEO_OS_XBOXONE    \
	)

#define NEO_OS_OS_CONSOLE  (0 \
	||  NEO_OS_NX             \
	||  NEO_OS_PS4            \
	||  NEO_OS_PS5            \
	||  NEO_OS_WINRT          \
	||  NEO_OS_XBOXONE        \
	)

#define NEO_OS_OS_DESKTOP  (0 \
	||  NEO_OS_BSD            \
	||  NEO_OS_HAIKU          \
	||  NEO_OS_HURD           \
	||  NEO_OS_LINUX          \
	||  NEO_OS_OSX            \
	||  NEO_OS_WINDOWS        \
	)

#define NEO_OS_OS_EMBEDDED (0 \
	||  NEO_OS_RPI            \
	)

#define NEO_OS_OS_MOBILE   (0 \
	||  NEO_OS_ANDROID        \
	||  NEO_OS_IOS            \
	)

#if NEO_COM_GCC
#	define NEO_COM_NAME "GCC "       \
    NEO_STRINGIZE(__GNUC__) "."       \
    NEO_STRINGIZE(__GNUC_MINOR__) "." \
    NEO_STRINGIZE(__GNUC_PATCHLEVEL__)
#elif NEO_COM_CLANG
#	define NEO_COM_NAME "Clang "      \
    NEO_STRINGIZE(__clang_major__) "." \
    NEO_STRINGIZE(__clang_minor__) "." \
    NEO_STRINGIZE(__clang_patchlevel__)
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
#	define NEO_OS_NAME "Android " \
				NEO_STRINGIZE(NEO_OS_ANDROID)
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
#elif NEO_OS_NONE
#	define NEO_OS_NAME "None"
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
#   undef NEO_DBG
#   define NEO_DBG 0
#else
#   undef NEO_DBG
#   define NEO_DBG 1
#endif

/* ---- Compiler Specific Intrinsics ---- */

#if NEO_COM_GCC || NEO_COM_CLANG
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
static NEO_AINLINE int neo_bsf32(uint32_t x) {
    if (neo_unlikely(!x)) { return 0; }
    return __builtin_ctz(x);
}
static NEO_AINLINE int neo_bsr32(uint32_t x) {
    if (neo_unlikely(!x)) { return 0; }
    return __builtin_clz(x)^31;
}
#   define neo_bswap32(x) __builtin_bswap32(x)
#   define neo_bswap64(x) __builtin_bswap64(x)
#	define neo_rol(x, n) __builtin_rotl(x,n)
#	define neo_ror(x, n) __builtin_rotr(x,n)
typedef enum
{
    NEO_MEMORD_RELX = __ATOMIC_RELAXED,
    NEO_MEMORD_ACQ = __ATOMIC_ACQUIRE,
    NEO_MEMORD_REL = __ATOMIC_RELEASE,
    NEO_MEMORD_ACQ_REL = __ATOMIC_ACQ_REL,
    NEO_MEMORD_SEQ_CST = __ATOMIC_SEQ_CST
} neo_MemOrd;
static NEO_AINLINE void neo_atomic_store(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	__atomic_store_n(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_load(volatile int64_t *ptr, neo_MemOrd order)
{
	return __atomic_load_n(ptr, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_add(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_fetch_add(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_sub(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_fetch_sub(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_and(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_fetch_and(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_or(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_fetch_or(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_fetch_xor(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_fetch_xor(ptr, x, (int)order);
}
static NEO_AINLINE int64_t neo_atomic_exchange(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
	return __atomic_exchange_n(ptr, x, (int)order);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_weak(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_MemOrd order_succ, neo_MemOrd order_fail)
{
    return __atomic_compare_exchange(ptr, exp, des, true, (int)order_succ, (int)order_fail);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_strong(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_MemOrd order_succ, neo_MemOrd order_fail)
{
    return __atomic_compare_exchange(ptr, exp, des, false, (int)order_succ, (int)order_fail);
}
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
#	define neo_rol(x, n) _rotl64(x,n)
#	define neo_ror(x, n) _rotr64(x, n)
typedef enum
{
    NEO_MEMORD_RELX,
    NEO_MEMORD_ACQ,
    NEO_MEMORD_REL,
    NEO_MEMORD_ACQ_REL,
    NEO_MEMORD_SEQ_CST
} neo_MemOrd;
static NEO_AINLINE void neo_atomic_store(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    _InterlockedExchange64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_load(volatile int64_t *ptr, neo_MemOrd order)
{
    (void)order;
    int64_t r;
    _InterlockedExchange64(&r, *ptr);
    return r;
}
static NEO_AINLINE int64_t neo_atomic_fetch_add(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedExchangeAdd64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_sub(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedExchangeAdd64(ptr, -x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_and(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedAnd64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_or(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedOr64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_fetch_xor(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedXor64(ptr, x);
}
static NEO_AINLINE int64_t neo_atomic_exchange(volatile int64_t *ptr, int64_t x, neo_MemOrd order)
{
    (void)order;
    return _InterlockedExchange64(ptr, x);
}
static NEO_AINLINE bool neo_atomic_compare_exchange_weak(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_MemOrd order_succ, neo_MemOrd order_fail)
{
    (void)order_succ;
    (void)order_fail;
    return _InterlockedCompareExchange64(ptr, *des, *exp) == *exp;
}
static NEO_AINLINE bool neo_atomic_compare_exchange_strong(volatile int64_t *ptr, int64_t *exp, int64_t *des, neo_MemOrd order_succ, neo_MemOrd order_fail)
{
    (void)order_succ;
    (void)order_fail;
    return _InterlockedCompareExchange64(ptr, *des, *exp) == *exp;
}
#endif

/* ---- Misc ---- */
#if !defined(neo_rol) || !defined(neo_ror)
#	define neo_rol(x, n) (((x)<<(n))|((x)>>(-(int)(n)&((sizeof(x)<<3)-1))))
#	define neo_ror(x, n) (((x)<<(-(int)(n)&((sizeof(x)<<3)-1)))|((x)>>(n)))
#endif
#define neo_bnd_check(p, o, l) neo_likely(((uintptr_t)(p)>=(uintptr_t)(o)) && ((uintptr_t)(p)<((uintptr_t)(o)+(l))))
#define neo_assert_name2(name, line) name ## line
#define neo_assert_name(line) neo_assert_name2(_assert_, line)
#define neo_static_assert(expr) extern void neo_assert_name(__LINE__)(bool STATIC_ASSERTION_FAILED[((expr)?1:-1)])
extern NEO_EXPORT NEO_COLDPROC NEO_NORET void neo_panic(const char *msg, ...);
extern NEO_EXPORT NEO_COLDPROC NEO_NORET void neo_assert_impl(const char *expr, const char *file, int line);
#define NEO_SEP ,

#define neo_as(ex) (void)(neo_likely(ex)||(neo_assert_impl(#ex, __FILE__, __LINE__), 0)) /* Assert for debug and release builds. */
#if NEO_DBG
#   define neo_asd(ex) neo_as(ex) /* Assert for debug only builds. */
#else
#   define neo_asd(ex) /* Assert for debug only builds. */
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
#   define neo_info(msg, ...) fprintf(stdout,  "[neo] " SRC_FILE " " msg "\n", __VA_ARGS__)
#   define neo_warn(msg, ...) fprintf(stderr,  "[neo] " SRC_FILE " " NEO_CCYELLOW msg NEO_CCRESET "\n", __VA_ARGS__)
#   define neo_error(msg, ...) fprintf(stderr, "[neo] " SRC_FILE " " NEO_CCRED msg NEO_CCRESET "\n", __VA_ARGS__)
#else
#   define neo_info(msg, ...)
#   define neo_warn(msg, ...)
#   define neo_error(msg, ...)
#endif

/* ---- OS interface ---- */
extern NEO_EXPORT void neo_osi_init(void);
extern NEO_EXPORT void neo_osi_shutdown(void);

/* ---- Memory ---- */
#ifndef neo_alloc_malloc
#   define neo_alloc_malloc(size) calloc(1, size)
#endif
#ifndef neo_alloc_realloc
#   define neo_alloc_realloc(blk, size) realloc((blk),(size))
#endif
#ifndef neo_dealloc
#   define neo_dealloc(blk) free(blk)
#endif
extern void *neo_defmemalloc(void *blk, size_t len);
#define neo_memalloc(blk, len) neo_defmemalloc(blk, len)

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

/* ---- Misc ---- */

typedef enum { NEO_FMODE_R /* read */, NEO_FMODE_W /* write */, NEO_FMODE_A /* append */, NEO_FMODE_BIN /* read */, NEO_FMODE_TXT /* text */ } neo_fmode_t;
extern NEO_EXPORT bool neo_fopen(FILE **fp, const uint8_t *filepath, /* neo_fmode_t */ int mode);
typedef enum { NEO_UNIERR_OK, NEO_UNIERR_TOO_SHORT, NEO_UNIERR_TOO_LONG, NEO_UNIERR_TOO_LARGE, NEO_UNIERR_OVERLONG, NEO_UNIERR_HEADER_BITS, NEO_UNIERR_SURROGATE } unicode_err_t;
extern NEO_EXPORT unicode_err_t neo_utf8_validate(const uint8_t *buf, size_t len, size_t *ppos);

#ifdef __cplusplus
}
#endif
#endif
