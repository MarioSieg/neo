/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AST -> Bytecode. */

#ifndef NEO_CODEGEN_H
#define NEO_CODEGEN_H

#include "neo_ast.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bytecode_t {
    bci_instr_t *p;
    size_t cap;
    size_t len;
} bytecode_t;

extern NEO_EXPORT void bc_init(bytecode_t *bc);
extern NEO_EXPORT void bc_append(bytecode_t *bc, bci_instr_t instr);
extern NEO_EXPORT void bc_free(bytecode_t *bc);

#ifdef __cplusplus
}
#endif
#endif
