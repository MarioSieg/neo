/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* x86-64/AMD64 machine code emitter and CPU detector. Code generation is done in reverse. */

#include "neo_core.h"

#if NEO_COM_GCC /* Undefine these if your GCC is old and doesn't support cpuid.h. */
    #define HAVE_GCC_GET_CPUID
    #define USE_GCC_GET_CPUID
#endif

#define VLI_MAX 15 /* Max 15. bytes of VLI instructions. */
#define VEX_SUPPORT /* Enable AVX VEX (Vector Extensions) prefix encoding. */
#define EVEX_SUPPORT /* Enable AVX512-F EVEX (Enhanced Vector Extensions) prefix encoding. */

#if NEO_COM_MSVC
#   include <intrin.h>
#elif defined(HAVE_GCC_GET_CPUID) && defined(USE_GCC_GET_CPUID)
#   include <cpuid.h>
#endif

/* Contains flags of all detected CPU features. */
typedef enum extended_isa_t {
    AMD64ISA_DEFAULT = 0,
    AMD64ISA_AVX2 = 1<<0,
    AMD64ISA_SSE42 = 1<<1,
    AMD64ISA_PCLMULQDQ = 1<<2,
    AMD64ISA_BMI1 = 1<<3,
    AMD64ISA_BMI2 = 1<<4,
    AMD64ISA_AVX512F = 1<<5,
    AMD64ISA_AVX512DQ = 1<<6,
    AMD64ISA_AVX512IFMA = 1<<7,
    AMD64ISA_AVX512PF = 1<<8,
    AMD64ISA_AVX512ER = 1<<9,
    AMD64ISA_AVX512CD = 1<<10,
    AMD64ISA_AVX512BW = 1<<11,
    AMD64ISA_AVX512VL = 1<<12,
    AMD64ISA_AVX512VBMI2 = 1<<13,
    AMD64ISA_AVX512VPOPCNTDQ = 1<<14
} extended_isa_t;
#define ECX_PCLMULDQD (1u<<1)
#define ECX_SSE42 (1u<<20)
#define ECX_OSXSAVE ((1u<<26)|(1u<<27))
#define ECX_AVX512VBMI (1u<<1)
#define ECX_AVX512VBMI2 (1u<<6)
#define ECX_AVX512VNNI (1u<<11)
#define ECX_AVX512BITALG (1u<<12)
#define ECX_AVX512VPOPCNT (1u<<14)
#define EBX_BMI1 (1u<<3)
#define EBX_AVX2 (1u<<5)
#define EBX_BMI2 (1u<<8)
#define EBX_AVX512F (1u<<16)
#define EBX_AVX512DQ (1u<<17)
#define EBX_AVX512IFMA (1u<<21)
#define EBX_AVX512CD (1u<<28)
#define EBX_AVX512BW (1u<<30)
#define EBX_AVX512VL (1u<<31)
#define EDX_AVX512VP2INTERSECT (1u<<8)
#define XCR0_AVX256 (1ull<<2) /* 256-bit %ymm* save/restore */
#define XCR0_AVX512 (7ull<<5) /* 512-bit %zmm* save/restore */

static void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) { /* Query CPUID. */
    neo_dassert(eax != NULL && ebx != NULL && ecx != NULL && edx != NULL, "Invalid arguments");
#if NEO_COM_MSVC
    int cpu_info[4];
    __cpuidex(cpu_info, *eax, *ecx);
    *eax = cpu_info[0];
    *ebx = cpu_info[1];
    *ecx = cpu_info[2];
    *edx = cpu_info[3];
#elif NEO_COM_GCC && defined(HAVE_GCC_GET_CPUID) && defined(USE_GCC_GET_CPUID)
    uint32_t level = *eax;
  __get_cpuid(level, eax, ebx, ecx, edx);
#else
    uint32_t a = *eax, b, c = *ecx, d;
    __asm__ __volatile__("cpuid\n\t" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
#endif
}

static inline uint64_t xgetbv() { /* Query extended control register value. */
#if NEO_COM_MSVC
    return _xgetbv(0);
#else
    uint32_t lo, hi;
    __asm__ __volatile__("xgetbv\n\t" : "=a" (lo), "=d" (hi) : "c" (0));
    return lo | ((uint64_t)hi << 32); /* Combine lo and high words. */
#endif
}

/* Detect all supported CPU extension on host. */
static extended_isa_t detect_cpu_isa() {
    uint32_t eax;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    uint32_t host_isa = AMD64ISA_DEFAULT;
    eax = 0x1;
    cpuid(&eax, &ebx, &ecx, &edx);
    if (ecx & ECX_SSE42) { host_isa |= AMD64ISA_SSE42; }
    if (ecx & ECX_PCLMULDQD) { host_isa |= AMD64ISA_PCLMULQDQ; }
    if ((ecx & ECX_OSXSAVE) != ECX_OSXSAVE) { return (extended_isa_t)host_isa; }
    uint64_t xcr0 = xgetbv(); /* Required to check kernel support for extended 256-bit %ymm* register save/restore. */
    /* AVX, BMI detection. */
    if ((xcr0 & XCR0_AVX256) == 0) { return (extended_isa_t)host_isa; } /* OS does not support AVX-256 bit YMM contexts, the hardware features don't matter now. */
    eax = 0x7;
    ecx = 0x0;
    cpuid(&eax, &ebx, &ecx, &edx);
    if (ebx & EBX_AVX2) { host_isa |= AMD64ISA_AVX2; }
    if (ebx & EBX_BMI1) { host_isa |= AMD64ISA_BMI1; }
    if (ebx & EBX_BMI2) { host_isa |= AMD64ISA_BMI2; }
    /* AVX 512* detection. */
    if ((xcr0 & XCR0_AVX512) != XCR0_AVX512) { return (extended_isa_t)host_isa; }/* OS does not support AVX-512 bit ZMM contexts, the hardware features don't matter now. */
    if (ebx & EBX_AVX512F) { host_isa |= AMD64ISA_AVX512F; }
    if (ebx & EBX_AVX512BW) { host_isa |= AMD64ISA_AVX512BW; }
    if (ebx & EBX_AVX512CD) { host_isa |= AMD64ISA_AVX512CD; }
    if (ebx & EBX_AVX512DQ) { host_isa |= AMD64ISA_AVX512DQ; }
    if (ebx & EBX_AVX512VL) { host_isa |= AMD64ISA_AVX512VL; }
    if (ecx & ECX_AVX512VBMI2) { host_isa |= AMD64ISA_AVX512VBMI2; }
    if (ecx & ECX_AVX512VPOPCNT) { host_isa |= AMD64ISA_AVX512VPOPCNTDQ; }
    return (extended_isa_t)host_isa;
}

typedef uint8_t mcode_t; /* Machine code type. */
#define VLA_MAX 15

typedef enum gpr_t { /* General purpose 64/32-bit registers. */
    RID_RAX, RID_RCX, RID_RDX, RID_RBX, RID_RSP, RID_RBP, RID_RSI, RID_RDI, /* RAX, RCX, RDX, RBX <- order is weird, but correct. */
    RID_R8,  RID_R9,  RID_R10, RID_R11, RID_R12, RID_R13, RID_R14, RID_R15,
    RID__LEN
} gpr_t;
neo_static_assert(RID__LEN == 16 && RID_RDI == 7);
typedef enum fpr_t { /* SSE 128-bit SIMD floating point registers. */
    FID_XMM0, FID_XMM1, FID_XMM2,  FID_XMM3,  FID_XMM4,  FID_XMM5,  FIX_XMM6,  FID_XMM7,
    FID_XMM8, FID_XMM9, FID_XMM10, FID_XMM11, FID_XMM12, FID_XMM13, FIX_XMM14, FID_XMM15,
    FID__LEN
} fpr_t;
neo_static_assert(FID__LEN == 16);
#define RID_MAX RID__LEN+FID__LEN

/* Calling conventions. */
#if NEO_OS_WINDOWS /* Of course Windows has a different ABI, fuck you again Windows. */
#   define RID_RA1 RID_RCX /* Register integer argument 1. */
#   define RID_RA2 RID_RDX /* Register integer argument 2. */
#   define RID_RA3 RID_R8 /* Register integer argument 3. */
#   define RID_RA4 RID_R9 /* Register integer argument 4. */
#   define CALLEE_REG_MASK ((1u<<RID_RAX)|(1u<<RID_RCX)|(1u<<RID_RDX)|(1u<<RID_R8)|(1u<<RID_R9)|(1u<<RID_R10))
#   define CALLEE_SAVED_REG_MASK ((1u<<RID_RDI)|(1u<<RID_RSI)|(1u<<RID_RBX)|(1u<<RID_R12)|(1u<<RID_R13)|(1u<<RID_R14)|(1u<<RID_R15)|(1u<<RID_RBP))
#else
#   define RID_RA1 RID_RDI /* Register integer argument 1. */
#   define RID_RA2 RID_RSI /* Register integer argument 2. */
#   define RID_RA3 RID_RDX /* Register integer argument 3. */
#   define RID_RA4 RID_RCX /* Register integer argument 4. */
#   define CALLEE_REG_MASK ((1u<<RID_RAX)|(1u<<RID_RCX)|(1u<<RID_RDX)|(1u<<RID_RSI)|(1u<<RID_RDI)|(1u<<RID_R8)|(1u<<RID_R9)|(1u<<RID_R10))
#   define CALLEE_SAVED_REG_MASK ((1u<<RID_RBX)|(1u<<RID_R12)|(1u<<RID_R13)|(1u<<RID_R14)|(1u<<RID_R15)|(1u<<RID_RBP))
#endif
#define RA_IRET RID_RAX /* Integer return register. */
#define RA_FRET RID_XMM0 /* Float return register. */
neo_static_assert(CALLEE_REG_MASK <= 0xffff);
neo_static_assert(CALLEE_SAVED_REG_MASK <= 0xffff);

/* Pack VLA SSE opcodes. Little endian byte order.  */
#define sse_packpd(o) ((uint32_t)(0x00000f66u|((0x##o##u&255)<<16))) /* 66 0f = packed double prec */
#define sse_packsd(o) ((uint32_t)(0x00000ff2u|((0x##o##u&255)<<16))) /* f2 0f = scalar double prec */
#define sse_packps(o) ((uint32_t)(0xfe00000fu|((0x##o##u&255)<<8)))  /*    0f = packed single prec, 0xfe magic => no opcode prefix required */
#define sse_packss(o) ((uint32_t)(0x00000ff3u|((0x##o##u&255)<<16))) /* f3 0f = scalar single prec */

/* General instructions. */
typedef enum genop_t {
    XI_INT3 = 0xcc, XI_NOP = 0x90, XI_RET = 0xc3,
    XI_CALL = 0xe8, XI_JMP = 0xe9
} opcode_t;

/* ALU instructions. */
typedef enum aluop_t {
    XA_ADD, XA_OR, XA_ADC, XA_SBB, XA_AND, XA_SUB, XA_XOR, XA_CMP,
    XA__LEN
} aluop_t;

/* Baseline SSE, SSE2 instructions. AVX and AVX-512 support is planned. */
typedef enum sseop_t {
    XO_MOVSD = sse_packsd(10), XO_MOVAPD = sse_packpd(28), XO_MOVUPD = sse_packpd(10),
    XO_ADDSD = sse_packsd(58), XO_ADDPD = sse_packpd(58),
    XO_SUBSD = sse_packsd(5c), XO_SUBPD = sse_packpd(5c),
    XO_MULSD = sse_packsd(59), XO_MULPD = sse_packpd(59),
    XO_DIVSD = sse_packsd(5e), XO_DIVPD = sse_packpd(5e),
    XO_MINSD = sse_packsd(5d), XO_MINPD = sse_packpd(5d),
    XO_MAXSD = sse_packsd(5f), XO_MAXPD = sse_packpd(5f),
} sseop_t;

typedef enum coco_t { /* Branch condition codes. */
    COCO_EQ = 0,  COCO_E   = 0,  COCO_Z  = 0,   COCO_NE  = 1,  COCO_NZ  = 1,
    COCO_LT = 2,  COCO_B   = 2,  COCO_C  = 2,   COCO_NAE = 2,  COCO_LE  = 3,
    COCO_BE = 3,  COCO_NA  = 3,  COCO_GT = 4,   COCO_A   = 4,  COCO_NBE = 4,
    COCO_GE = 5,  COCO_AE  = 5,  COCO_NB = 5,   COCO_NC  = 5,  COCO_LZ  = 6,
    COCO_S  = 6,  COCO_GEZ = 7,  COCO_NS = 7,   COCO_P   = 8,  COCO_PE  = 8,
    COCO_NP = 9,  COCO_PO  = 9,  COCO_O  = 10,  COCO_NO  = 11,
    COCO__LEN
} coco_t;

typedef union imm_t { /* Immediate operand. */
    uint64_t u64;
    int64_t i64;
    uint32_t u32;
    int32_t i32;
    uint8_t u8;
    int8_t i8;
    double f64;
    void *p;
} imm_t;
neo_static_assert(sizeof(imm_t) == 8);

enum { XM_INDIRECT = 0, XM_SIGNED_DISP8, XN_SIGNED_DISP32, XM_DIRECT }; /* MODRM addressing modes. */
enum { REX_B = 1<<0, REX_X = 1<<1, REX_R = 1<<2, REX_W = 1<<3 }; /* REX prefix bits. */
#define pack_modrm(mod, ro, rx) ((mcode_t)(((mod)&3)<<6)|(((ro)&7)<<3)|((rx)&7))
#define checku8(x) ((int32_t)(uint8_t)(x) == (x))
#define checki8(x) ((int32_t)(int8_t)(x) == (x))
#define checku16(x) ((int32_t)(uint16_t)(x) == (x))
#define checki16(x) ((int32_t)(int16_t)(x) == (x))
#define checku32(x) ((uint32_t)(x) == (x))
#define checki32(x) ((int32_t)(x) == (x))

/* Emit REX prefix. */
static void emit_rex(mcode_t **mxp, mcode_t mod, mcode_t idx, mcode_t rmo, bool x64) {
    mcode_t rex = 0x40;
    rex |= x64 ? REX_W : 0;
    rex |= mod & ~7 ? REX_R : 0;
    rex |= idx & ~7 ? REX_X : 0;
    rex |= rmo & ~7 ? REX_B : 0;
    if (rex != 0x40) { *--*mxp = rex; }
}

/* REX + OPC. */
static NEO_AINLINE void emit_si_opc(mcode_t **mxp, mcode_t opc, mcode_t r, bool x64) {
    *--*mxp = opc | (r & 7);
    emit_rex(mxp, 0, 0, r, x64);
}
/* REX + OPC + MODRM. */
static NEO_AINLINE void emit_si_opc_modrm(mcode_t **mxp, mcode_t opc, mcode_t r0, mcode_t mod, bool x64) {
    *--*mxp = pack_modrm(XM_DIRECT, mod, r0);
    *--*mxp = opc;
    emit_rex(mxp, 0, 0, r0, x64);
}

/* OP reg, reg. OP is an ALU opcode like add, sub, xor etc. Example: addq %r8, %rax. */
static void xop_rr(mcode_t **mxp, aluop_t opc, gpr_t dst, gpr_t src, bool x64) {
    *--*mxp = pack_modrm(XM_DIRECT, dst, src);
    *--*mxp = (opc << 3) + 3;
    emit_rex(mxp, dst, 0, src, x64);
}

/* MOV reg, imm. Example: movq $10, %rax. */
static void mov_ri(mcode_t **mxp, gpr_t reg, imm_t x) {
    if (x.u64 == 0) { /* Optimization: xorl %reg, %reg for zeroing. */
        xop_rr(mxp, XA_XOR, reg, reg, false);
        return;
    }
    bool x64 = !checku32(x.u64); /* Requires 64-bit load. */
    if (x64) { /* Full 64-bit load. Example: movabsq $10, %rax. */
        *mxp -= 8;
        *(uint64_t *)*mxp = *(uint64_t *)&x;
    } else { /* Small 32-bit load. Example: movl $10, %eax. */
        *mxp -= 4;
        *(uint32_t *)*mxp = *(uint32_t *)&x;
    }
    emit_si_opc(mxp, 0xb8, reg, x64);
}

/* OP reg, imm. OP is an ALU opcode like add, sub, xor etc. Example: addq $10, %rax. */
static void xop_ri(mcode_t **mxp, aluop_t opc, gpr_t reg, imm_t x, bool x64) {
    neo_assert(checku32(x.u64), "32-bit Imm out of range: " PRIu64, x.u64);
    mcode_t *p = *mxp; /* Pointer to current machine code buffer. */
    if (checku8(x.u64)) { /* Small 8-bit immediate. */
        *--p = *(mcode_t *)&x;
        *--p = pack_modrm(XM_DIRECT, opc, reg);
        *--p = 0x83;
        emit_rex(&p, 0, 0, reg, x64);
    } else if (reg == RID_RAX) { /* Optimize for accumulator. */
        *(uint32_t *)p = *(uint32_t *)&x;
        p -= 4;
        *--p = (opc << 3) + 5;
        emit_rex(&p, 0, 0, 0, x64);
    } else { /* Full 32-bit immediate. */
        p -= 4;
        *(uint32_t *)p = *(uint32_t *)&x;
        *--p = pack_modrm(XM_DIRECT, opc, reg);
        *--p = 0x81;
        emit_rex(&p, 0, 0, 0, x64);
    }
    *mxp = p; /* Update pointer to current machine code buffer. */
}

#ifdef NEO_EXTENSION_DISASSEMBLER

#include <Zydis/Zydis.h>

static NEO_COLDPROC void dump_assembly(const mcode_t *p, size_t len, FILE *f) {
    neo_dassert(p != NULL && f != NULL, "Invalid arguments");
    fprintf(f, "Machine Code Block @%p, Len: %zu\n", p, len);
    uintptr_t rip = (uintptr_t)p;
    size_t offset = 0;
    ZydisDisassembledInstruction instruction;
    while (ZYAN_SUCCESS(ZydisDisassembleATT(
        ZYDIS_MACHINE_MODE_LONG_64,
        rip,
        p + offset,
        len - offset,
        &instruction
    ))) {
        fprintf(
            f,
            "%s%016" PRIX64 "%s %s%s%s ",
            NEO_CCMAGENTA,
            rip,
            NEO_CCRESET,
            NEO_CCBLUE,
            instruction.text,
            NEO_CCRESET
        );
        fputs(NEO_CCCYAN, f);
        for (uint8_t i = 0; i < instruction.info.length; ++i) {
            fprintf(f, "%02X ", p[offset + i]);
        }
        fputs(NEO_CCRESET, f);
        fputc('\n', f);
        offset += instruction.info.length;
        rip += instruction.info.length;
    }
}

#endif
