/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_VM_H
#define NEO_VM_H

#include "neo_core.h"
#include "neo_object.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NEO_EXPORT NEO_NODISCARD neo_uint_t vmop_upow64_no_ov(neo_uint_t x, neo_uint_t k); /* Unsigned r = x ^ k. o overflow checks. */
extern NEO_EXPORT NEO_NODISCARD neo_int_t vmop_ipow64_no_ov(neo_int_t x, neo_int_t k); /* Signed r = x ^ k. No overflow checks. */
extern NEO_EXPORT NEO_NODISCARD bool vmop_upow64(neo_uint_t x, neo_uint_t k, neo_uint_t *r); /* Signed r = x ^ k. Return true on overflow. */
extern NEO_EXPORT NEO_NODISCARD bool vmop_ipow64(neo_int_t x, neo_int_t k, neo_int_t *r); /* Signed r = x ^ k. Return true on overflow. */

typedef struct opstck_t {
    record_t *p;
    size_t len;
} opstck_t;

#define VMSTK_DEFAULT_SIZE (1024ull*1024ull*1ull) /* Default stack size: 1 MB */
#define VMSTK_DEFAULT_ELEMTS (VMSTK_DEFAULT_SIZE>>3) /* Default stack element count. */

typedef enum vminterrupt_t {
    VMINT_OK = 0,
    VMINT_STK_UNDERFLOW,
    VMINT_STK_OVERFLOW,
    VMINT_ARI_OVERFLOW,
    VMINT_ARI_ZERODIV,

    VMINT__COUNT
} vminterrupt_t;

typedef struct vmisolate_t {
    const char *name;
    uint64_t id;
    opstck_t stack;
    constpool_t constpool;
    vminterrupt_t interrupt;
    const bci_instr_t *ip;
    const record_t *sp;
    ptrdiff_t ip_delta;
    ptrdiff_t sp_delta;
} vmisolate_t;

typedef struct bytecode_t {
    const bci_instr_t *p;
    size_t len;
} bytecode_t;

extern NEO_NODISCARD bool vm_validate(const vmisolate_t *isolate, const bytecode_t *bcode);
extern NEO_HOTPROC NEO_NODISCARD bool vm_exec(vmisolate_t *isolate, const bytecode_t *bcode);

#ifdef __cplusplus
}
#endif
#endif
