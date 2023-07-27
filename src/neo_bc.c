/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_bc.h"

#define _(_1, _2, _3, _4) [_1] = _2
const char *const opc_mnemonic[OPC__COUNT] = { opdef(_, NEO_SEP) };
#undef _
#define _(_1, _2, _3, _4) [_1] = _3
const int8_t opc_depth[OPC__COUNT] = { opdef(_, NEO_SEP) };
#undef _
#define _(_1, _2, _3, _4) [_1] = _4
const bool opc_imm[OPC__COUNT] = { opdef(_, NEO_SEP) };
#undef _

bool bci_validate_instr(bci_instr_t instr) {
    mode_t mod = bci_unpackmod(instr);
    if (neo_likely(mod == BCI_MOD1)) {
        opcode_t opc = bci_unpackopc(instr);
        if (neo_unlikely(opc >= OPC__COUNT)) { /* Invalid opcode value. */
            neo_error("invalid opcode: %" PRIx8, opc);
            return false;
        }
        int32_t imm24 = bci_mod1unpack_imm24(instr);
        if (!opc_imm[opc] && *(uint32_t *)&imm24 != BCI_MOD1IMM24UNUSED) { /* If imm24 is unused value must be set to BCI_MOD1IMM24UNUSED. */
            neo_error("invalid imm24 value: %" PRIx32, imm24);
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
    mode_t mod = bci_unpackmod(instr);
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
