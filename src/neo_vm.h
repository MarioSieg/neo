/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_VM_H
#define NEO_VM_H

#include "neo_core.h"
#include "neo_bc.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct constpool_t {
    record_t *p;
    uint8_t /*rtag_t*/ *tags;
    size_t len;
} constpool_t;

typedef struct opstck_t {
    record_t *p;
    size_t len;
} opstck_t;

#define VMSTK_DEFAULT_SIZE (1024ull*1024ull*1ull) /* Default stack size: 1 MB */
#define VMSTK_DEFAULT_ELEMTS (VMSTK_DEFAULT_SIZE>>3) /* Default stack element count. */

typedef enum vminterrupt_t {
    VMINT_OK = 0,
    VMINT_STK_UNDERFLOW,
    VMINT_STK_OVERFLOW
} vminterrupt_t;

typedef struct vmisolate_t {
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

extern bool vm_validate(const vmisolate_t *isolate, const bytecode_t *bcode);
extern bool NEO_HOTPROC vm_exec(vmisolate_t *isolate, const bytecode_t *bcode);

#ifdef __cplusplus
}
#endif
#endif
