/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_bc.h"

#define _(_1, mnemonic, _3, _4, _5) [_1] = mnemonic
const char *const opc_mnemonic[OPC__COUNT] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, op, _4, _5) [_1] = (op&127)
const uint8_t opc_stack_ops[OPC__COUNT] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, _3, rtv, _5) [_1] = (rtv&127)
const uint8_t opc_stack_rtvs[OPC__COUNT] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, op, rtv, _5) [_1] = (int8_t)(-(int8_t)(op)+(int8_t)(rtv))
const int8_t opc_depth[OPC__COUNT] = {opdef(_, NEO_SEP)};
#undef _
#define _(_1, _2, _3, _4, imm) [_1] = imm
const uint8_t opc_imm[OPC__COUNT] = {opdef(_, NEO_SEP)};
#undef _

bool bci_validate_instr(bci_instr_t instr) {
    int mod = bci_unpackmod(instr);
    if (neo_likely(mod == BCI_MOD1)) {
        opcode_t opc = bci_unpackopc(instr);
        if (neo_unlikely(opc >= OPC__COUNT)) { /* Invalid opcode value. */
            neo_error("invalid opcode: %" PRIx8, opc);
            return false;
        }
        if (neo_unlikely(opc_imm[opc] == IMM_NONE && bci_mod1unpack_imm24(instr) != 0)) { /* Instruction does not use immediate value, but it is not zero. */
            neo_error("instruction does not use immediate value, but it is not zero: %" PRIx32, instr);
            return false;
        }
        return true;
    } else {
        neo_error("mode2 instructions are not supported yet: %" PRIx8, mod);
        /* NYI */
        return false;
    }
}

void bci_dump_instr(bci_instr_t instr, FILE *out) {
    neo_asd(out);
    int mod = bci_unpackmod(instr);
    if (neo_likely(mod == BCI_MOD1)) {
        opcode_t opc = bci_unpackopc(instr);
        if (opc_imm[opc]) {
            fprintf(out, "%s #%" PRIi32 "\n", opc_mnemonic[opc], bci_mod1unpack_imm24(instr));
        } else {
            fprintf(out, "%s\n", opc_mnemonic[opc]);
        }
    } else {
        /* NYI */
    }
}
