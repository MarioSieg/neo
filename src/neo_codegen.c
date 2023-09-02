/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AST -> Bytecode. */

#include "neo_codegen.h"

void bc_init(bytecode_t *bc) {
    neo_dassert(bc);
    memset(bc, 0, sizeof(*bc));
    bc->cap = 32;
    bc->p = neo_memalloc(NULL, sizeof(*bc->p)*bc->cap);
}

void bc_append(bytecode_t *bc, bci_instr_t instr) {
    neo_dassert(bc);
}

void bc_free(bytecode_t *bc) {
    neo_dassert(bc);
    neo_memalloc(bc->p, 0);
}

