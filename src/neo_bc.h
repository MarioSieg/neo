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
**      Next 24 bits: signed 24-bit immediate value (mode 1) or shift value (mode 2)
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

#define opdef(_, __)\
    _(OPC_HLT, "hlt")/* Halt VM execution. */__ \
    _(OPC_NOP, "nop")/* NO-Operation. */__\
    _(OPC_IPUSH, "ipush")/* Push 24-bit int value. */__\
    _(OPC_IPUSH0, "ipush0")/* Push int value 0. */__\
    _(OPC_IPUSH1, "ipush1")/* Push int value 1. */__\
    _(OPC_IPUSH2, "ipush2")/* Push int value 2. */__\
    _(OPC_IPUSHM1, "ipushm1")/* Push int value -1. */__\
    _(OPC_FPUSH0, "fpush0")/* Push float value 0.0. */__\
    _(OPC_FPUSH1, "fpush1")/* Push float value 1.0. */__\
    _(OPC_FPUSH2, "fpush2")/* Push float value 2.0. */__\
    _(OPC_FPUSH05, "fpush05")/* Push float value 0.5. */__\
    _(OPC_FPUSHM1, "fpushm1")/* Push float value -1.0. */__ \
    _(OPC_POP, "pop")/* Pop one stack record. */__\
    _(OPC_LDC, "ldc")/* Load constant from constant pool. */

#define _(_1, _2) _1
typedef enum {
    opdef(_, NEO_SEP),
    OPC__COUNT,
    OPC__MAX = 127
} opcode_t;
#undef _
neo_static_assert(OPC__COUNT <= OPC__MAX);

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
#define bci_switchmod(i) ((bci_instr_t)(((i)^128)&~0xffffff00u))

/* Mode 1 macros. */
#define BCI_MOD1IMM24MAX 0x007fffff
#define BCI_MOD1IMM24MIN 0x00800000
#define BCI_MOD1IMM24BIAS (1<<3)
#define bci_mod1imm24_sign(x) (((x)&0x800000)>>23)
#define bci_mod1unpack_imm24(i) ((int32_t)(((i)>>8)&0x00ffffffu))
#define bci_mod1pack_imm24(i, imm24) ((bci_instr_t)((i)|(((imm24)&0x00ffffffu)<<8)))
#define bci_mod1_imm24_fit(x) ((x)&~0xffffffull)

/* Mode 2 macros. */
/* NYI */

/* Instruction composition. */
#define bci_comp_mod1(opc, imm24) ((bci_instr_t)(((opc)&127)|(1<<7)|(((imm24)&0x00ffffffu)<<8)))

#ifdef __cplusplus
}
#endif

#endif
