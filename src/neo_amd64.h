/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* x86-64/AMD64 machine code emitter. Code generation is done in reverse. */

#include "neo_core.h"

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

enum { XM_INDIRECT, XM_SIGNED_DISP8, XN_SIGNED_DISP32, XM_DIRECT }; /* MODRM addressing modes. */
#define emit_modrm(mod, ro, rx) ((mcode_t)(((mod)&3)<<6)|(((ro)&7)<<3)|((rx)&7))

/* Pack VLA SSE opcodes. Little endian byte order.  */
#define sse_packpd(o) ((uint32_t)(0x00000f66u|((0x##o##u&255)<<16))) /* 66 0f = packed double prec */
#define sse_packsd(o) ((uint32_t)(0x00000ff2u|((0x##o##u&255)<<16))) /* f2 0f = scalar double prec */
#ifdef SINGLE32
#define sse_packps(o) ((uint32_t)(0xaa00000fu|((0x##o##u&0xffu)<<8)))  /*    0f = packed single prec */
#define sse_packss(o) ((uint32_t)(0x00000ff3u|((0x##o##u&0xffu)<<16))) /* f3 0f = scalar single prec */
#endif

typedef enum sse_opcode_t {
    XO_MOVSD = sse_packsd(10), XO_MOVAPD = sse_packpd(28), XO_MOVUPD = sse_packpd(10),
    XO_ADDSD = sse_packsd(58), XO_ADDPD = sse_packpd(58),
    XO_SUBSD = sse_packsd(5c), XO_SUBPD = sse_packpd(5c),
    XO_MULSD = sse_packsd(59), XO_MULPD = sse_packpd(59),
    XO_DIVSD = sse_packsd(5e), XO_DIVPD = sse_packpd(5e),
    XO_MINSD = sse_packsd(5d), XO_MINPD = sse_packpd(5d),
    XO_MAXSD = sse_packsd(5f), XO_MAXPD = sse_packpd(5f),
} sse_opcode_t;

typedef enum opcode_t {
    XI_INT3 = 0xcc, XI_NOP = 0x90, XI_RET = 0xc3,
    XI_CALL = 0xe8, XI_JMP = 0xe9
} opcode_t;

typedef enum coco_t { /* Condition codes. */
    COCO_EQ = 0,  COCO_E   = 0,  COCO_Z  = 0,   COCO_NE  = 1,  COCO_NZ  = 1,
    COCO_LT = 2,  COCO_B   = 2,  COCO_C  = 2,   COCO_NAE = 2,  COCO_LE  = 3,
    COCO_BE = 3,  COCO_NA  = 3,  COCO_GT = 4,   COCO_A   = 4,  COCO_NBE = 4,
    COCO_GE = 5,  COCO_AE  = 5,  COCO_NB = 5,   COCO_NC  = 5,  COCO_LZ  = 6,
    COCO_S  = 6,  COCO_GEZ = 7,  COCO_NS = 7,   COCO_P   = 8,  COCO_PE  = 8,
    COCO_NP = 9,  COCO_PO  = 9,  COCO_O  = 10,  COCO_NO  = 11,
    COCO__LEN
} coco_t;
