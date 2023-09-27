/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Implementation of the VM (virtual machine) isolate, hot-routines used by the VM and helpers. */

#ifndef NEO_VM_H
#define NEO_VM_H

#include "neo_core.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- VM-Environment. ---- */

typedef struct opstck_t {
    record_t *p;
    size_t len;
} opstck_t;

#define VMSTK_DEF_SIZE (1024ull*1024ull*1ull) /* Default stack size: 1 MB. Must be a multiple of 8 */
#define VMSTK_DEF_ELEMTS (VMSTK_DEF_SIZE>>3) /* Default stack element count.  */
#define VMSTK_DEF_WARMUP 0x4000 /* 16 KiB Warmup region in bytes [SP, SP+0x4000]. Must be a multiple of 8 */
neo_static_assert(sizeof(record_t) == 8 && VMSTK_DEF_SIZE % sizeof(record_t) == 0);
neo_static_assert(VMSTK_DEF_WARMUP % sizeof(record_t) == 0);
extern NEO_EXPORT void stk_alloc(opstck_t *self, size_t bsize, size_t bwarmup); /* Len = stack size in bytes. Must be a multiple of 8. Warmup = bwarmup region in bytes. Must be a multiple of 8 */
extern NEO_EXPORT void stk_free(opstck_t *self, bool poison);

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

typedef enum vm_interrupt_t {
    VMINT_OK = 0,
    VMINT_SYS_SYSCALL,
    VMINT_STK_UNDERFLOW,
    VMINT_STK_OVERFLOW,
    VMINT_ARI_OVERFLOW,
    VMINT_ARI_ZERODIV,
    VMINT__LEN
} vm_interrupt_t;
neo_static_assert(VMINT__LEN <= 255);

typedef struct vm_isolate_t vm_isolate_t;
struct vm_isolate_t {
    char name[128]; /* Name of the isolate. */
    int64_t id; /* Unique ID of the isolate. */
    opstck_t stack; /* Stack. */
    FILE *io_input; /* Input stream. */
    FILE *io_output; /* Output stream. */
    FILE *io_error; /* Error stream. */
    prng_state_t prng; /* PRNG state. */
    void (*pre_exec_hook)(vm_isolate_t *isolate, const bytecode_t *bcode); /* Pre-execution hook. */
    void (*post_exec_hook)(vm_isolate_t *isolate, const bytecode_t *bcode, vm_interrupt_t result); /* Post-execution hook. */
    struct {
        vm_interrupt_t interrupt; /* Interrupt code. */
        const bci_instr_t *ip; /* Instruction pointer. */
        const record_t *sp; /* Stack pointer. */
        ptrdiff_t ip_delta; /* Instruction pointer delta. */
        ptrdiff_t sp_delta; /* Stack pointer delta. */
        uint32_t invocs; /* Invocation count. */
        uint32_t invocs_ok; /* Invocation count. */
        uint32_t invocs_err; /* Invocation count. */
    } rstate; /* Result state. */
};

extern NEO_EXPORT void vm_init(vm_isolate_t **self, const char *name);
extern NEO_EXPORT void vm_free(vm_isolate_t **self);
extern NEO_HOTPROC NEO_NODISCARD NEO_EXPORT bool vm_exec(vm_isolate_t *self, const bytecode_t *bcode);

#ifdef __cplusplus
}
#endif
#endif
