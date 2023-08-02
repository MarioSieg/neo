/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_BC_H
#define NEO_BC_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
** Bytecode instructions are 32-bit wide and stored in host byte order when in memory.
** +----------------+----------------+----------------+----------------+
** |.OPC[7]..MOD[1].|......................IMM[24].....................| MODE1
** +----------------+----------------+----------------+----------------+
** |.OPC[7]..MOD[1].|SHIFT[7]..COM[1]|.............IMM[16].............| MODE2
** +----------------+----------------+----------------+----------------+
** MSB                                                               LSB
** First 7 bits: opcode value
** Next bit: mode (0 = mode 1, 1 = mode 2)
** For mode 1:
**      Next 24 bits: signed or unsigned (depends on the instruction) 24-bit immediate value (mode 1) or shift value (mode 2)
** For mode 2:
** NYI
*/

/*
** Handling of immediate values/constants in the bytecode.
** If x is an integer to which applies x < 2^24 then x can be pushed using the push instruction:
** >>> ipush #10
** If x is an integer has the constant scalar value of 0, 1, 2 or -1 the following instructions can be used:
** >>> ipush0
** >>> ipush1
** >>> ipush2
** >>> ipushm1
** If x a floating-point value and x has the constant scalar value of 0.0, 1.0, 2.0, 0.5 or -1.0 then the following instructions can be used:
** >>> fpush0
** >>> fpush1
** >>> fpush2
** >>> fpush05
** >>> fpushm1
** If none of the above applies to x, then x must be loaded from the constant pool:
** >>> ldc #<idx>
**
*/

enum {
    IMM_NONE = 0, /* Immediate unused. */
    IMM_I24 = 1, /* 24-bit signed integer. */
    IMM_U24 = 2 /* 24-bit unsigned integer. */
};

#define opdef(_, __) /* Enum | Mnemonic | Stack-OPS | Stack-RTVs | IMM_* Mode */\
    _(OPC_HLT, "hlt", 0, 0, IMM_NONE)/* Halt VM execution. */__ \
    _(OPC_NOP, "nop", 0, 0, IMM_NONE)/* NO-Operation. */__\
    _(OPC_IPUSH, "ipush", 0, 1, IMM_I24)/* Push 24-bit int value. */__\
    _(OPC_IPUSH0, "ipush0", 0, 1, IMM_NONE)/* Push int value 0. */__\
    _(OPC_IPUSH1, "ipush1", 0, 1, IMM_NONE)/* Push int value 1. */__\
    _(OPC_IPUSH2, "ipush2", 0, 1, IMM_NONE)/* Push int value 2. */__\
    _(OPC_IPUSHM1, "ipushm1", 0, 1, IMM_NONE)/* Push int value -1. */__\
    _(OPC_FPUSH0, "fpush0", 0, 1, IMM_NONE)/* Push float value +0.0. */__\
    _(OPC_FPUSH1, "fpush1", 0, 1, IMM_NONE)/* Push float value 1.0. */__\
    _(OPC_FPUSH2, "fpush2", 0, 1, IMM_NONE)/* Push float value 2.0. */__\
    _(OPC_FPUSH05, "fpush05", 0, 1, IMM_NONE)/* Push float value 0.5. */__\
    _(OPC_FPUSHM1, "fpushm1", 0, 1, IMM_NONE)/* Push float value -1.0. */__ \
    _(OPC_POP, "pop", 1, 0, IMM_NONE)/* Pop one stack record. */__\
    _(OPC_LDC, "ldc", 0, 1, IMM_U24)/* Load constant from constant pool. */__\
    _(OPC_IADD, "iadd", 2, 1, IMM_NONE)/* Integer addition with overflow check. */__\
    _(OPC_ISUB, "isub", 2, 1, IMM_NONE)/* Integer subtraction with overflow check. */__\
    _(OPC_IMUL, "imul", 2, 1, IMM_NONE)/* Integer multiplication with overflow check. */__\
    _(OPC_IPOW, "ipow", 2, 1, IMM_NONE)/* Integer exponentiation with overflow check. */__\
    _(OPC_IADDO, "iaddo", 2, 1, IMM_NONE)/* Integer addition without overflow check. */__\
    _(OPC_ISUBO, "isubo", 2, 1, IMM_NONE)/* Integer subtraction without overflow check. */__\
    _(OPC_IMULO, "imulo", 2, 1, IMM_NONE)/* Integer multiplication without overflow check. */__\
    _(OPC_IPOWO, "ipowo", 2, 1, IMM_NONE)/* Integer exponentiation without overflow check. */__\
    _(OPC_IDIV, "idiv", 2, 1, IMM_NONE)/* Integer division. */__\
    _(OPC_IMOD, "imod", 2, 1, IMM_NONE)/* Integer modulo. */__\
    _(OPC_IAND, "iand", 2, 1, IMM_NONE)/* Integer bitwise conjunction (AND). */__\
    _(OPC_IOR, "ior", 2, 1, IMM_NONE)/* Integer bitwise disjunction (OR). */__\
    _(OPC_IXOR, "ixor", 2, 1, IMM_NONE)/* Integer bitwise exclusive disjunction (XOR). */__\
    _(OPC_ISAL, "isal", 2, 1, IMM_NONE)/* Integer bitwise arithmetic left shift. */__\
    _(OPC_ISAR, "isar", 2, 1, IMM_NONE)/* Integer bitwise arithmetic right shift. */__\
    _(OPC_ISLR, "islr", 2, 1, IMM_NONE)/* Integer bitwise logical right shift. */__\
    _(OPC_IROL, "irol", 2, 1, IMM_NONE)/* Integer bitwise arithmetic left rotation. */__\
    _(OPC_IROR, "iror", 2, 1, IMM_NONE)/* Integer bitwise arithmetic right rotation. */

#define _(enumerator, _2, _3, _4, _5) enumerator
typedef enum {
    opdef(_, NEO_SEP),
    OPC__COUNT,
    OPC__MAX = 127
} opcode_t;
#undef _
neo_static_assert(OPC__COUNT <= OPC__MAX);
extern NEO_EXPORT const char *const opc_mnemonic[OPC__COUNT];
extern NEO_EXPORT const uint8_t opc_stack_ops[OPC__COUNT];
extern NEO_EXPORT const uint8_t opc_stack_rtvs[OPC__COUNT];
extern NEO_EXPORT const int8_t opc_depth[OPC__COUNT];
extern NEO_EXPORT const uint8_t /* IMM_* type */ opc_imm[OPC__COUNT];

/* General instruction en/decoding (applies to mode 1 and 2) */
typedef uint32_t bci_instr_t;
#define BCI_MAX 0xffffffffu
#define BCI_OPCMAX 127
#define BCI_MOD1 0
#define BCI_MOD2 1
#define bci_unpackopc(i) ((opcode_t)((i)&127))
#define bci_packopc(i, opc) ((bci_instr_t)((i)|((opc)&127)))
#define bci_unpackmod(i) (((i)&128)>>7)
#define bci_packmod(i, mod) ((bci_instr_t)((i)|(((mod)&1)<<7)))
#define bci_switchmod(i) ((bci_instr_t)(((i)^128)&255))

/* Mode 1 macros. */
#define BCI_MOD1IMM24MAX (0x007fffff)
#define BCI_MOD1IMM24MIN (-0x00800000)
#define BCI_MOD1UMM24MAX (0x00ffffff)
#define BCI_MOD1UMM24MIN (0x00000000)
#define bci_fits_i24(x) (((int64_t)(x)>=BCI_MOD1IMM24MIN)&&((int64_t)(x)<=BCI_MOD1IMM24MAX))
#define bci_fits_u24(x) ((int64_t)(x)>=0&&(int64_t)(x)<=BCI_MOD1UMM24MAX)
#define bci_u24tou32(x) ((uint32_t)(x))
#define bci_u32tou24(x) ((uint32_t)(x)&0x00ffffffu)
#define bci_i24toi32(x) (((int32_t)(x)<<8)>>8)
#define bci_i32toi24(x) ((uint32_t)((int32_t)(x)&(1<<23) ? ((int32_t)(x)&~-16777216)|-16777216 : ((int32_t)(x)&0x00ffffff)&~-16777216))
#define BCI_MOD1IMM24_BIAS (1<<3)
#define bci_mod1imm24_sign(x) (((x)&0x800000)>>23)
#define bci_mod1unpack_imm24(i) bci_i24toi32((i)>>8)
#define bci_mod1unpack_umm24(i) bci_u24tou32((i)>>8)
#define bci_mod1pack_imm24(i, imm24) ((bci_instr_t)((i)|(bci_i32toi24(imm24)<<BCI_MOD1IMM24_BIAS)))
#define bci_mod1pack_umm24(i, imm24) ((bci_instr_t)((i)|(bci_u32tou24(imm24)<<BCI_MOD1IMM24_BIAS)))

/* Mode 2 macros. */
/* NYI */

extern NEO_EXPORT bool bci_validate_instr(bci_instr_t instr);
extern NEO_EXPORT void bci_dump_instr(bci_instr_t instr, FILE *out);

/* Instruction composition. */
static inline NEO_NODISCARD bci_instr_t bci_comp_mod1_imm24(opcode_t opc, int32_t imm) {
    neo_as(bci_fits_i24(imm) && "24-bit signed imm out of range"); /* Verify immediate value. */
    neo_as(opc_imm[opc&127] == IMM_I24 && "invalid imm mode for instruction"); /* Verify immediate mode. */
    return bci_packopc(0, opc)|(bci_i32toi24(imm)<<BCI_MOD1IMM24_BIAS);
}
static inline NEO_NODISCARD bci_instr_t bci_comp_mod1_umm24(opcode_t opc, uint32_t imm) {
    neo_as(bci_fits_u24(imm) && "24-bit unsigned imm out of range"); /* Verify immediate value. */
    neo_as(opc_imm[opc&127] == IMM_U24 && "invalid imm mode for instruction"); /* Verify immediate mode. */
    return bci_packopc(0, opc)|(bci_u32tou24(imm)<<BCI_MOD1IMM24_BIAS);
}
static inline NEO_NODISCARD bci_instr_t bci_comp_mod1_no_imm(opcode_t opc) {
    neo_as(opc_imm[opc&127] == IMM_NONE && "invalid imm mode for instruction"); /* Verify immediate mode. */
    return bci_packopc(0, opc);
}

#ifdef __cplusplus
}
#endif

#endif
