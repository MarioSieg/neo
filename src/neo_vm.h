/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_VM_H
#define NEO_VM_H

#include "neo_core.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NEO_EXPORT NEO_NODISCARD bool vmop_ipow64(neo_int_t x, neo_int_t k, neo_int_t *r); /* Signed r = x ^ k. Return true on overflow. */
extern NEO_EXPORT NEO_NODISCARD neo_int_t vmop_ipow64_no_ov(neo_int_t x, neo_int_t k); /* Signed r = x ^ k. No overflow checks. */

typedef NEO_ALIGN(8) union record_t {
    neo_int_t as_int;
    neo_uint_t as_uint;
    neo_float_t as_float;
    neo_char_t as_char;
    neo_bool_t as_bool;
    void *as_ref;
    uint8_t raw[sizeof(neo_int_t)];
} record_t;
neo_static_assert(sizeof(record_t) == 8);

typedef enum rtag_t {
    RT_INT,
    RT_FLOAT,
    RT_CHAR,
    RT_BOOL,
    RT_REF
} rtag_t;
neo_static_assert(RT_REF <= 255);

extern NEO_EXPORT bool record_eq(record_t a, record_t b, rtag_t tag);

typedef uint32_t cpkey_t; /* Constant pool index key. */
#define CONSTPOOL_MAX BCI_MOD1UMM24MAX /* Maximum constant pool index, because the ldc immediate is an 24-bit unsigned integer. */
typedef struct constpool_t {
    record_t *p;
    uint8_t /*rtag_t*/ *tags;
    uint32_t len;
    uint32_t cap;
} constpool_t;

extern NEO_EXPORT void constpool_init(constpool_t *self, uint32_t cap /* capacity or 0 */);
extern NEO_EXPORT cpkey_t constpool_put(constpool_t *self, rtag_t tag, record_t value);
extern NEO_EXPORT bool constpool_has(const constpool_t *self, cpkey_t idx);
extern NEO_EXPORT bool constpool_get(constpool_t *self, cpkey_t idx, record_t *value, rtag_t *tag);
extern NEO_EXPORT void constpool_free(constpool_t *self);

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
