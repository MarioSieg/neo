/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Implementation of the VM (virtual machine) isolate, hot-routines used by the VM and helpers. */

#ifndef NEO_VM_H
#define NEO_VM_H

#include "neo_core.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- VM-Intrinsic routines. ---- */

extern NEO_HOTPROC neo_uint_t vmop_upow64_no_ov(neo_uint_t x, neo_uint_t k); /* Unsigned r = x ^ k. o overflow checks. */
extern NEO_HOTPROC neo_int_t vmop_ipow64_no_ov(neo_int_t x, neo_int_t k); /* Signed r = x ^ k. No overflow checks. */
extern NEO_HOTPROC bool vmop_upow64(neo_uint_t x, neo_uint_t k, neo_uint_t *r); /* Signed r = x ^ k. Return true on overflow. */
extern NEO_HOTPROC bool vmop_ipow64(neo_int_t x, neo_int_t k, neo_int_t *r); /* Signed r = x ^ k. Return true on overflow. */
extern NEO_HOTPROC neo_float_t vmop_ceil(neo_float_t x); /* Ceil(x). */
extern NEO_HOTPROC neo_float_t vmop_floor(neo_float_t x); /* Floor(x). */
extern NEO_HOTPROC neo_float_t vmop_mod(neo_float_t x, neo_float_t y); /* x % y. */

/* ---- PRNG. ---- */

typedef struct prng_state_t { uint64_t s[4]; } prng_state_t;

extern void prng_init_seed(prng_state_t *self, uint64_t noise); /* Init PRNG with common seed. */
extern void prng_from_seed(prng_state_t *self, double seed); /* Init PRNG with custom seed. */
extern NEO_HOTPROC neo_int_t prng_next_i64(prng_state_t *self); /* Get next random integer. */
extern NEO_HOTPROC neo_float_t prng_next_f64(prng_state_t *self); /* Get next random float within [0, 1.0]. */

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

    VMINT__LEN
} vminterrupt_t;

typedef struct vmisolate_t vmisolate_t;
struct vmisolate_t {
    char name[128]; /* Name of the isolate. */
    uint64_t id; /* Unique ID of the isolate. */
    opstck_t stack; /* Stack. */
    vminterrupt_t interrupt; /* Interrupt code. */
    const bci_instr_t *ip; /* Instruction pointer. */
    const record_t *sp; /* Stack pointer. */
    ptrdiff_t ip_delta; /* Instruction pointer delta. */
    ptrdiff_t sp_delta; /* Stack pointer delta. */
    uint32_t invocs; /* Invocation count. */
    uint32_t invocs_ok; /* Invocation count. */
    uint32_t invocs_err; /* Invocation count. */
    void (*pre_exec_hook)(vmisolate_t *isolate, const bytecode_t *bcode); /* Pre-execution hook. */
    void (*post_exec_hook)(vmisolate_t *isolate, const bytecode_t *bcode, vminterrupt_t result); /* Post-execution hook. */
};

extern NEO_HOTPROC NEO_NODISCARD NEO_EXPORT bool vm_exec(vmisolate_t *self, const bytecode_t *bcode);

#ifdef __cplusplus
}
#endif
#endif
