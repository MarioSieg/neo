// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#ifndef NEORT_BC_H
#define NEORT_BC_H

#include <stdint.h>

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
**      Next 7 bits: shift value
**      Next bit: complement flag
**      Next 16 bits: signed 16-bit immediate value
**      Which effectively results in the following operation:
**      x = imm16 << shift; <- shift value is unsigned
**      if (com) x = ~x; <- complement flag inverts the result
*/

/* General instruction en/decoding (applies to mode 1 and 2) */
typedef uint32_t bci_instr_t;
typedef uint8_t bci_opc_t;
#define BCI_MAX 0xffffffffu
#define BCI_OPCMAX 127
#define BCI_MOD1 0
#define BCI_MOD2 1
#define bci_unpackopc(i) ((bci_opc_t)((i)&127))
#define bci_unpackmod(i) (((i)&128)>>7)
#define bci_packopc(i,opc) ((bci_instr_t)((i)|((opc)&127)))
#define bci_packmod(i,mod) ((bci_instr_t)((i)|(((mod)&1)<<7)))
#define bci_switchmod(i) ((bci_instr_t)(((i)^128)&~0xffffff00u))

/* Mode 1 macros */
#define BCI_MOD1IMM24MAX 0x007fffff
#define BCI_MOD1IMM24MIN 0x00800000
#define BCI_MOD1IMM24BIAS (1<<3)
#define bci_mod1imm24_sign(x) (((x)&0x800000)>>23)
#define bci_mod1unpack_imm24(i) ((int32_t)(((i)>>8)&0x00ffffffu))
#define bci_mod1pack_imm24(i,imm24) ((bci_instr_t)((i)|(((imm24)&0x00ffffffu)<<8)))

/* Mode 2 macros */
#define BCI_MOD2IMM16MAX (BCI_MOD1IMM24MAX>>8)
#define BCI_MOD2IMM16MIN (BCI_MOD1IMM24MIN>>8)
#define BCI_MOD2IMM16BIAS (BCI_MOD1IMM24BIAS<<1)
#define bci_mod2imm16_sign(x) (((x)&0x8000)>>15)
#define bci_mod2unpack_imm16(i) ((int32_t)(((i)>>16)&0xffffu))
#define bci_mod2pack_imm16(i,imm16) ((bci_instr_t)((i)|(((imm16)&0xffffu)<<16)))
#define bci_mod2unpack_com(i) (((i)>>15)&1)
#define bci_mod2pack_com(i,com) ((bci_instr_t)((i)|(((com)&1)<<15)))
#define bci_mod2unpack_shift(i) (((i)>>8)&127)
#define bci_mod2pack_shift(i,shift) ((bci_instr_t)((i)|(((shift)&127)<<8)))

/* Instruction composition */
#define bci_comp_mod1(i,opc,imm24) ((bci_instr_t)(bci_packmod(bci_packopc(i,opc),BCI_MOD1)|bci_mod1pack_imm24(0,imm24)))
#define bci_comp_mod2(i,opc,shift,com,imm16) ((bci_instr_t)(bci_packmod(bci_packopc(i,opc),BCI_MOD2)|bci_mod2pack_shift(0,shift)|bci_mod2pack_com(0,com)|bci_mod2pack_imm16(0,imm16)))

#endif
