/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* Implementation of the VM (virtual machine) isolate, hot-routines used by the VM and helpers. */

#include "neo_vm.h"

#include <math.h>

void stk_alloc(opstck_t *self, size_t bsize, size_t bwarmup) {
    neo_dassert(self != NULL, "self is NULL");
    bsize = bsize && bsize % sizeof(record_t) == 0 ? bsize : VMSTK_DEF_SIZE;
    bwarmup = bwarmup && bwarmup % sizeof(record_t) == 0 ? bwarmup : VMSTK_DEF_WARMUP;
    self->len = bsize>>3; /* Bytes to record count -> / sizeof(record_t) */
    self->p = neo_memalloc(NULL, bsize); /* Todo: use mmap/VirtualAlloc. */
    memset(self->p, 0, bwarmup); /* Warmup region, preallocate pages. */
}

void stk_free(opstck_t *self, bool poison) {
    neo_dassert(self != NULL, "self is NULL");
    if (poison) {
        memset(self->p, 0, self->len*sizeof(*self->p));
    }
    neo_memalloc(self->p, 0);
}

void vm_init(vm_isolate_t **self, const char *name) {
    neo_assert(self != NULL, "self must not be NULL");
    *self = neo_memalloc(NULL, sizeof(**self));
    memset(*self, 0, sizeof(**self));
    if (name) {
#if NEO_COM_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
        strncpy((*self)->name, name, sizeof((*self)->name));
#if NEO_COM_GCC
#pragma GCC diagnostic pop
#endif
        (**self).name[sizeof((*self)->name)-1] = '\0';
    }
    static volatile int64_t mkid = 0x1000;
    uint64_t tid = neo_tid();
    (**self).id = neo_atomic_fetch_add(&mkid, 1, NEO_MEMORD_RELX); /* Generate ID. */
    (**self).id ^= (int64_t)((tid >> 32) | (tid & ~(uint32_t)0)); /* Mix in thread ID. */
    stk_alloc(&(**self).stack, VMSTK_DEF_SIZE, VMSTK_DEF_WARMUP); /* Allocate stack. */
    gc_init(&(**self).gc_context, (**self).stack.p, (**self).stack.len);
    (**self).io_input = stdin;
    (**self).io_output = stdout;
    (**self).io_error = stderr;
    prng_from_seed(&(*self)->prng, (double)((**self).id >> 32)); /* Init PRNG. */
}

void vm_free(vm_isolate_t **self) {
    neo_assert(self != NULL, "self must not be NULL");
    gc_free(&(**self).gc_context);
    stk_free(&(**self).stack, true);
    neo_memalloc(*self, 0);
    memset(*self, 0, sizeof(**self));
    *self = NULL;
}

/* ---- Core VM Routines ---- */

#define NEO_VM_COMPUTED_GOTO
#ifdef NEO_VM_COMPUTED_GOTO
#   define decl_op(name) __##name##__:
#   define dispatch() goto *jump_table[bci_unpackopc(*++ip)];
#   define zone_enter() dispatch()
#   define zone_exit()
#   define label_ref(op) [OPC_##op]=&&__##op##__
#else
#   define decl_op(name) case OPC_##name:
#   define dispatch() break;
#   define zone_enter() for (;;) { switch(bci_unpackopc(*++ip)) {
#   define zone_exit() }}
#   define label_ref(op)
#endif

#define STK_PADD_MAGIC (~(uint64_t)0)

#define stk_check_ov(n)\
    if (neo_unlikely((uintptr_t)(sp+(n))>spe)) {\
        vif = VMINT_STK_OVERFLOW;/* Stack overflow occurred, abort. */\
        goto exit;\
    }

#define stk_check_uv(n)\
    if (neo_unlikely((uintptr_t)(sp-(n))<sps)) {\
        vif = VMINT_STK_UNDERFLOW;/* Stack underflow occurred, abort. */\
        goto exit;\
    }

#define push(field, x)\
    stk_check_ov(1)\
    (*++sp).as_##field = (x)

#define pop(n)\
    stk_check_uv(n)\
    sp -= (n)

#define peek(x) (sp+(x))

#if NEO_COM_MSVC
#   error "MSVC is not supported yet."
#else
#if !__has_builtin(__builtin_saddl_overflow)
#   error "missing intrinsic function: __builtin_saddl_overflow <- should be implemented by GCC/Clang."
#endif
#if !__has_builtin(__builtin_ssubl_overflow)
#   error "missing intrinsic function: __builtin_ssubl_overflow <- should be implemented by GCC/Clang."
#endif
#if !__has_builtin(__builtin_smull_overflow)
#   error "missing intrinsic function: __builtin_smull_overflow <- should be implemented by GCC/Clang."
#endif
#if !__has_builtin(__builtin_umull_overflow)
#   error "missing intrinsic function: __builtin_umull_overflow <- should be implemented by GCC/Clang."
#endif
neo_static_assert(sizeof(long) == sizeof(neo_int_t));
#define i64_add_overflow(x, y, r) __builtin_saddl_overflow((x), (y), (r))
#define i64_sub_overflow(x, y, r) __builtin_ssubl_overflow((x), (y), (r))
#define i64_mul_overflow(x, y, r) __builtin_smull_overflow((x), (y), (r))
#define u64_mul_overflow(x, y, r) __builtin_umull_overflow((x), (y), (r))
#endif

neo_uint_t vmop_upow64_no_ov(neo_uint_t x, neo_uint_t k) {
    neo_uint_t y;
    if (neo_unlikely(k == 0)) { return 1; }
    for (; (k & 1) == 0; k >>= 1) { x *= x; }
    y = x;
    if ((k >>= 1) != 0) {
        for (;;) {
            x *= x;
            if (k == 1) { break; }
            if (k & 1) { y *= x; }
            k >>= 1;
        }
        y *= x;
    }
    return y;
}

neo_int_t vmop_ipow64_no_ov(register neo_int_t x, register neo_int_t k) {
    if (neo_unlikely(k == 0)) { return 1; }
    else if (neo_unlikely(k < 0)) {
        if (x == 0) { return NEO_INT_MAX; }
        else if (x == 1) { return 1; }
        else if (x == -1) { return k & 1 ? -1 : 1; }
        else { return 0; }
    } else { return (neo_int_t)vmop_upow64_no_ov((neo_uint_t)x, (neo_uint_t)k); }
}

#define imulov(x, y, rr) \
    if (neo_unlikely(i64_mul_overflow((x), (y), (rr)))) { \
        *r = 0; return true; /* Overflow happened. */ \
    }

#define umulov(x, y, rr) \
    if (neo_unlikely(u64_mul_overflow((x), (y), (rr)))) { \
        *r = 0; return true; /* Overflow happened. */ \
    }

bool vmop_upow64(neo_uint_t x, neo_uint_t k, neo_uint_t *r) {
    neo_uint_t y;
    if (neo_unlikely(k == 0)) { return 1; }
    for (; (k & 1) == 0; k >>= 1) { umulov(x, x, &x) }
    y = x;
    if ((k >>= 1) != 0) {
        for (;;) {
            umulov(x, x, &x)
            if (k == 1) { break; }
            if (k & 1) { umulov(y, x, &y) }
            k >>= 1;
        }
        umulov(y, x, &y)
    }
    *r = y;
    return false;
}
bool vmop_ipow64(neo_int_t x, neo_int_t k, neo_int_t *r) { /* Exponentiation by squaring. */
    if (neo_unlikely(k == 0)) { *r = 1; return false; }
    else if (neo_unlikely(k < 0)) {
        switch (x) {
            case 0: *r = NEO_INT_MAX; return false;
            case 1: *r = 1; return false;
            case -1: *r = k & 1 ? -1 : 1; return false;
            default: *r = 0; return false;
        }
    } else {
        for (; (k & 1) == 0; k >>= 1) { imulov(x, x, &x) }
        neo_int_t y = x;
        if ((k >>= 1) != 0) {
            for (;;) {
                imulov(x, x, &x)
                if (k == 1) { break; }
                if (k & 1) { imulov(y, x, &y) }
                k >>= 1;
            }
            imulov(y, x, &y)
        }
        *r = y;
        return false;
    }
}

neo_float_t vmop_ceil(neo_float_t x) {
    return ceil(x);
}

neo_float_t vmop_floor(neo_float_t x) {
    return floor(x);
}

neo_float_t vmop_mod(neo_float_t x, neo_float_t y) {
    return fmod(x, y);
}

/* ---- PRNG ---- */

/*
** NEO uses a Linear Feedback Shift Register (LFSR) (also known aus Tausworthe) random number generator,
** with a periodic length of 2^223. The generator provides a very good random distribution, but is not cryptographically secure.
** NEO has two random generation functions:
** -> int.random() and float.random(), which are not cryptographically secure and are powered by this generator
** and
** -> int.randomSecure() and float.randomSecure(), which are cryptographically secure, and are powered by the OS's CSPRNG.
** Generator algorithm based on:
** Tables of maximally-equidistributed combined LFSR generators, Pierre L'Ecuyer, 1991.
** Generator seeded as:
** L = 64
** J = 4
** k = 233
** LGp = 230
** N_1 = 59
*/

void prng_init_seed(prng_state_t *self, uint64_t noise) {
    neo_dassert(self != NULL, "self is NULL");
    noise = noise ? noise : neo_tid(); /* Use the noise to add thread-local entropy. */
    self->s[0] = 0xa0d277570a345b8cull ^ noise; /* Precomputed constants from prng_from_seed(0.0). */
    self->s[1] = 0x764a296c5d4aa64full ^ noise;
    self->s[2] = 0x51220704070adeaaull ^ noise;
    self->s[3] = 0x2a2717b5a7b7b927ull ^ noise;
}

void prng_from_seed(prng_state_t *self, double seed) {
    neo_dassert(self != NULL, "self is NULL");
    seed = seed != 0.0 ? seed : 5.249176108649e-01; /* Default seed. */
    uint32_t r = 0x11090601;  /* Four 8 bit-seeds merged into a scalar. */
    for (size_t i = 0; i < sizeof(self->s)/sizeof(*self->s); ++i) {
        uint32_t m = 1u<<(r&255); /* Mask. */
        r >>= 8;
        double d = seed = seed * 3.14159265358979323846 + 2.7182818284590452354;
        union { double d; uint64_t u; } u = { .d = d };
        if (u.u < m) { u.u += m; }
        self->s[i] = u.u;
    }
    for (int i = 0; i < (rand() % (64 + 1 - 16) + 16); ++i) {
        (void)prng_next_i64(self);
    }
}

#define tausworthe223_gen(self, z, r, i, k, q, v) \
  z = (self)->s[i]; \
  z = (((z << q) ^ z) >> (k-v)) ^ ((z & ((uint64_t)(int64_t)-1 << (64-k))) << v); \
  r ^= z; \
  (self)->s[i] = z

#define tausworthe223_step(self, z, r) \
  tausworthe223_gen(self, z, r, 0, 63, 31, 18); /* Index 0 */\
  tausworthe223_gen(self, z, r, 1, 58, 19, 28); /* Index 1 */\
  tausworthe223_gen(self, z, r, 2, 55, 24,  7); /* Index 2 */\
  tausworthe223_gen(self, z, r, 3, 47, 21,  8)  /* Index 3 */

neo_int_t prng_next_i64(prng_state_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    uint64_t z, r = 0;
    tausworthe223_step(self, z, r);
    union { uint64_t u; int64_t i; } u = { .u = r };
    return u.i;
}

neo_float_t prng_next_f64(prng_state_t *self) {
    neo_dassert(self != NULL, "self is NULL");
    uint64_t z, r = 0;
    tausworthe223_step(self, z, r);
    r = (r & 0xfffffffffffffull) | 0x3ff0000000000000ull; /* IEEE-754 binary-64 pattern in the range 1.0 <= d < 2.0. */
    union { uint64_t u; double d; } u = { .u = r };
    return u.d - 1.0;
}

/* ---- Core VM Helpers ---- */

#undef imulov
#undef umulov
#define i64_pow_overflow(...) vmop_ipow64(__VA_ARGS__)
#define pint(i) ((*peek(i)).as_int)
#define puint(i) ((*peek(i)).as_uint)
#define pfloat(i) ((*peek(i)).as_float)

#define bin_int_op(op)\
    pint(-1) op##= pint(0);\
    pop(1)

#define bin_int_op_call(proc)\
    pint(-1) = proc(pint(-1), pint(0));\
    pop(1)

#define ovchecked_bin_int_op(op)\
    if (neo_unlikely(i64_##op##_overflow(pint(-1), pint(0), &pint(-1)))) {\
        vif = VMINT_ARI_OVERFLOW;/* Overflow would happen. */\
        goto exit;\
    }\
    pop(1)

#define z_op(op, ev)\
    if (neo_unlikely(pint(0) == 0)) { /* Check for zero divison. */\
        vif = VMINT_ARI_ZERODIV;\
        goto exit;\
    } else if (neo_unlikely(pint(-1) == NEO_INT_MIN && pint(0) == -1)) { /* Check for overflow. */\
        pint(-1) = (ev);\
    } else {\
        bin_int_op(op);\
    }

/* ---- System Calls ---- */

#define impl_syscall(name) static bool syscall_##name(vm_isolate_t *self, record_t *sp)
#define syscall_check()   neo_dassert(self != NULL && sp != NULL, "self and sp must not be NULL")

impl_syscall(print_int) {
    syscall_check();
    uint8_t tmp [64];
    memset(tmp, 0, sizeof(tmp));
    neo_fmt_int(tmp, pint(0));
    fputs((const char *)tmp, self->io_output);
    return false;
}

impl_syscall(print_float) {
    syscall_check();
    uint8_t tmp [64];
    memset(tmp, 0, sizeof(tmp));
    neo_fmt_float(tmp, pfloat(0));
    fputs((const char *)tmp, self->io_output);
    return false;
}

impl_syscall(print_bool) {
    syscall_check();
    fputs(peek(0)->as_bool == NEO_TRUE ? "true" : "false", self->io_output);
    return false;
}

impl_syscall(print_char) {
    syscall_check();
    neo_char_t nc = peek(0)->as_char;
    if (nc < 0x80) { /* Is ASCII ? */
        fputc((char)nc, self->io_output);
    } else { /* TODO: UTF-8 encode. */
        uint8_t tmp[64];
        memset(tmp, 0, sizeof(tmp));
        neo_fmt_int(tmp, nc);
        fputs((const char *)tmp, self->io_output);
    }
    return false;
}

impl_syscall(print_ptr) {
    syscall_check();
    uint8_t tmp[64];
    memset(tmp, 0, sizeof(tmp));
    neo_fmt_ptr(tmp, sp);
    fputs((const char *)tmp, self->io_output);
    return false;
}

/* Dispatch table for system calls. For system calls see neo_bc.h. */
static bool (*const syscall_table[])(vm_isolate_t *self, record_t *sp) = {
    [SYSCALL_PRINT_INT] = &syscall_print_int,
    [SYSCALL_PRINT_FLOAT] = &syscall_print_float,
    [SYSCALL_PRINT_BOOL] = &syscall_print_bool,
    [SYSCALL_PRINT_CHAR] = &syscall_print_char,
    [SYSCALL_PRINT_PTR] = &syscall_print_ptr
};
neo_static_assert(sizeof(syscall_table)/sizeof(*syscall_table) == SYSCALL__LEN && "syscall_table size mismatch");

#undef impl_syscall

/* ---- Core VM Impl (Hot code) ---- */

NEO_HOTPROC bool vm_exec(vm_isolate_t *self, const bytecode_t *bcode) {
    neo_assert(self && bcode && bcode->p && bcode->len && self->stack.len, "self, bcode and stack must not be NULL and bcode and stack must not be empty");
    neo_assert(bci_unpackopc(bcode->p[0]) == OPC_NOP, "(Prologue-Code) First instruction must be NOP, but is: %s", opc_mnemonic[bci_unpackopc(bcode->p[0])]);
    neo_assert(bci_unpackopc(bcode->p[bcode->len-1]) == OPC_HLT, "(epilogue) last instruction must be HLT, but is: %s", opc_mnemonic[bci_unpackopc(bcode->p[0])]);

    if (self->pre_exec_hook) {
        (*self->pre_exec_hook)(self, bcode);
    }

    const uintptr_t ipb = (uintptr_t)bcode->p; /* Instruction pointer backup for delta computation. */
    const uintptr_t spb = (uintptr_t)self->stack.p; /* Stack pointer backup for delta computation. */

    register const bci_instr_t *restrict ip = bcode->p; /* Current instruction pointer. */
    register const uintptr_t sps = (uintptr_t)self->stack.p+sizeof(*self->stack.p); /* Start of stack. +1 for padding. */
    register const uintptr_t spe = (uintptr_t)(self->stack.p+self->stack.len)-sizeof(*self->stack.p); /* End of stack (last element). */
    register record_t *restrict sp = self->stack.p; /* Current stack pointer. */
    register const record_t *restrict cp = bcode->pool.p; /* Constant pool pointer. */
    register vm_interrupt_t vif = VMINT_OK; /* VM interrupt flag. */

    sp->as_uint = STK_PADD_MAGIC;

#ifdef NEO_VM_COMPUTED_GOTO
    static const void *restrict const jump_table[OPC__LEN] = {
        label_ref(HLT),
        label_ref(NOP),
        label_ref(SYSCALL),
        label_ref(IPUSH),
        label_ref(IPUSH0),
        label_ref(IPUSH1),
        label_ref(IPUSH2),
        label_ref(IPUSHM1),
        label_ref(FPUSH0),
        label_ref(FPUSH1),
        label_ref(FPUSH2),
        label_ref(FPUSH05),
        label_ref(FPUSHM1),
        label_ref(POP),
        label_ref(LDC),
        label_ref(IADD),
        label_ref(ISUB),
        label_ref(IMUL),
        label_ref(IPOW),
        label_ref(IADDO),
        label_ref(ISUBO),
        label_ref(IMULO),
        label_ref(IPOWO),
        label_ref(IDIV),
        label_ref(IMOD),
        label_ref(IAND),
        label_ref(IOR),
        label_ref(IXOR),
        label_ref(ISAL),
        label_ref(ISAR),
        label_ref(ISLR),
        label_ref(IROL),
        label_ref(IROR)
    };
#endif

    /* VM instruction labels/cases. MUST be in order! */

    zone_enter()

    decl_op(HLT) /* Halt VM execution. */
        goto exit;
    dispatch()

    decl_op(NOP) /* NO-Operation. */

    dispatch()

    decl_op(SYSCALL) {
        umm24_t call_id = bci_mod1unpack_umm24(*ip);
        int32_t depth = (int32_t)syscall_depths[call_id];
        stk_check_ov(depth); /* Check for stack overflow. */
        stk_check_uv(depth); /* Check for stack underflow. */
        if (neo_unlikely((*syscall_table[call_id])(self, sp))) {
            vif = VMINT_SYS_SYSCALL; /* System call failed, abort. */
            goto exit; /* System call failed, abort. */
        }
        sp -= depth; /* Pop depth delta. */
    }
    dispatch()

    decl_op(IPUSH) /* Push 24-bit int value. */
        push(int, bci_mod1unpack_imm24(*ip));
    dispatch()

    decl_op(IPUSH0) /* Push int value 0. */
        push(int, 0);
    dispatch()

    decl_op(IPUSH1) /* Push int value 1. */
        push(int, 1);
    dispatch()

    decl_op(IPUSH2) /* Push int value 2. */
        push(int, 2);
    dispatch()

    decl_op(IPUSHM1) /* Push int value -1. */
        push(int, -1);
    dispatch()

    decl_op(FPUSH0) /* Push float value 0.0. */
        push(float, 0.0);
    dispatch()

    decl_op(FPUSH1) /* Push float value 1.0. */
        push(float, 1.0);
    dispatch()

    decl_op(FPUSH2) /* Push float value 2.0. */
        push(float, 2.0);
    dispatch()

    decl_op(FPUSH05) /* Push float value 0.5. */
        push(float, 0.5);
    dispatch()

    decl_op(FPUSHM1) /* Push float value -1.0. */
        push(float, -1.0);
    dispatch()

    decl_op(POP) /* Pop one stack record. */
        pop(1);
    dispatch()

    decl_op(LDC) /* Load constant from constant pool. */
        push(int, cp[bci_mod1unpack_umm24(*ip)].as_int);
    dispatch()

    decl_op(IADD) /* Integer addition with overflow check. */
        ovchecked_bin_int_op(add);
    dispatch()

    decl_op(ISUB) /* Integer subtraction with overflow check. */
        ovchecked_bin_int_op(sub);
    dispatch()

    decl_op(IMUL) /* Integer multiplication with overflow check. */
        ovchecked_bin_int_op(mul);
    dispatch()

    decl_op(IPOW) /* Integer exponentiation with overflow check. */
        ovchecked_bin_int_op(pow);
    dispatch()

    decl_op(IADDO) /* Integer addition without overflow check. */
        bin_int_op(+);
    dispatch()

    decl_op(ISUBO) /* Integer subtraction without overflow check. */
        bin_int_op(-);
    dispatch()

    decl_op(IMULO) /* Integer multiplication without overflow check. */
        bin_int_op(*);
    dispatch()

    decl_op(IPOWO) /* Integer exponentiation without overflow check. */
        bin_int_op_call(vmop_ipow64_no_ov);
    dispatch()

    decl_op(IDIV) /* Integer division. */
        z_op(/, NEO_INT_MIN);
    dispatch()

    decl_op(IMOD) /* Integer modulo. */
        z_op(%, 0);
    dispatch()

    decl_op(IAND) /* Integer bitwise conjunction (AND). */
        bin_int_op(&);
    dispatch()

    decl_op(IOR) /* Integer bitwise disjunction (OR). */
        bin_int_op(|);
    dispatch()

    decl_op(IXOR) /* Integer bitwise exclusive disjunction (XOR). */
        bin_int_op(^);
    dispatch()

    decl_op(ISAL) /* Integer bitwise arithmetic left shift. */
        pint(-1) = pint(-1) << (puint(0) & 63);
        pop(1);
    dispatch()

    decl_op(ISAR) /* Integer bitwise arithmetic right shift. */
        pint(-1) = pint(-1) >> (puint(0) & 63);
        pop(1);
    dispatch()

    decl_op(ISLR) /* Integer bitwise logical right shift. */
        pint(-1) = (neo_int_t)((neo_uint_t)pint(-1) >> puint(0));
        pop(1);
    dispatch()

    decl_op(IROL) /* Integer bitwise arithmetic left rotation. */
        pint(-1) = (neo_int_t)neo_rol64((neo_uint_t)pint(-1), puint(0) & 63);
    pop(1);
    dispatch()

    decl_op(IROR) /* Integer bitwise arithmetic right rotation. */
        pint(-1) = (neo_int_t)neo_ror64((neo_uint_t)pint(-1), puint(0) & 63);
    dispatch()

#ifndef NEO_VM_COMPUTED_GOTO /* To suppress enumeration value ‘OPC__**’ not handled in switch [-Werror=switch]. */
    case OPC__LEN:
    case OPC__MAX: break;
#endif

    zone_exit()

exit:
    self->rstate.interrupt = vif;
    self->rstate.ip = ip;
    self->rstate.sp = sp;
    self->rstate.ip_delta = ip-(const bci_instr_t *)ipb;
    self->rstate.sp_delta = sp-(const record_t *)spb;
    ++self->rstate.invocs;
    if (vif == VMINT_OK) { ++self->rstate.invocs_ok; }
    else { ++self->rstate.invocs_err; }
    if (self->post_exec_hook) {
        (*self->post_exec_hook)(self, bcode, self->rstate.interrupt);
    }

    return vif == VMINT_OK;
}
