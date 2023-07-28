/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_vm.h"

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

#define STK_PADD_MAGIC 0xffffffffffffffffull

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

bool vmop_ipow64(neo_int_t x, neo_int_t k, neo_int_t *r) { /* Exponentiation by squaring. */
    neo_asd(r);
    if (neo_unlikely(k == 0)) {
        *r = 1; return false;
    } else if (neo_unlikely(k < 0)) {
        if (neo_unlikely(x == 0)) {
            *r = NEO_UINT_C(0x7fffffffffffffff); return false;
        } else if (x == 1) {
            *r = 1; return false;
        } else if (x == -1) {
            *r = k & 1 ? -1 : 1; return false;
        } else {
            *r = 0; return false;
        }
    } else {
        if (neo_unlikely(k == 0)) { *r = 1; return false; }
        for (; (k & 1) == 0; k >>= 1) {
            if (neo_unlikely(i64_mul_overflow(x, x, &x))) {
                *r = 0;
                return true; /* Overflow happened. */
            }
        }
        int64_t y = x;
        if ((k >>= 1) != 0) {
            for (;;) {
                if (neo_unlikely(i64_mul_overflow(x, x, &x))) {
                    *r = 0;
                    return true; /* Overflow happened. */
                }
                if (k == 1) { break; }
                else if (k & 1) {
                    if (neo_unlikely(i64_mul_overflow(y, x, &y))) {
                        *r = 0;
                        return true; /* Overflow happened. */
                    }
                }
                k >>= 1;
            }
            if (neo_unlikely(i64_mul_overflow(y, x, &y))) {
                *r = 0;
                return true; /* Overflow happened. */
            }
        }
        *r = y;
        return false;
    }
}

neo_int_t vmop_ipow64_no_ov(register neo_int_t x, register neo_int_t k) {
    if (neo_unlikely(k == 0)) { return 1; }
    else if (neo_unlikely(k < 0)) {
        if (x == 0) { return NEO_UINT_C(0x7fffffffffffffff); }
        else if (x == 1) { return 1; }
        else if (x == -1) { return k & 1 ? -1 : 1; }
        else { return 0; }
    } else {
        if (neo_unlikely(k == 0)) { return 1; }
        for (; (k & 1) == 0; k >>= 1) { x *= x; }
        register int64_t r = x;
        if ((k >>= 1) != 0) {
            for (;;) {
                x *= x;
                if (k == 1) { break; }
                if (k & 1) { r *= x; }
                k >>= 1;
            }
            r *= x;
        }
        return r;
    }
}

#define i64_pow_overflow(...) vmop_ipow64(__VA_ARGS__)

#define bin_int_op(op)\
    peek(-1)->as_int op##= peek(0)->as_int;\
    pop(1)

#define bin_int_op_call(proc)\
    peek(-1)->as_int = proc(peek(-1)->as_int, peek(0)->as_int);\
    pop(1)

#define ovchecked_bin_int_op(op)\
    if (neo_unlikely(i64_##op##_overflow(peek(-1)->as_int, peek(0)->as_int, &peek(-1)->as_int))) {\
        vif = VMINT_ARI_OVERFLOW;/* Overflow would happen. */\
        goto exit;\
    }\
    pop(1)

#define zerochecked_bin_int_op(op) \
    if (neo_unlikely(peek(0)->as_int == 0)) {/* Check if we would divide by zero. */\
        vif = VMINT_DIV_BY_ZERO;/* Zero division would happen. */\
        goto exit;\
    }\
    bin_int_op(op)

NEO_HOTPROC bool vm_exec(vmisolate_t *isolate, const bytecode_t *bcode) {
    neo_as(isolate && bcode && bcode->p && bcode->len && isolate->stack.len);
    neo_as(bci_unpackopc(bcode->p[0]) == OPC_NOP && "(prologue) first instruction must be NOP");
    neo_as(bci_unpackopc(bcode->p[bcode->len-1]) == OPC_HLT && "(epilogue) last instruction must be HLT");

    const uintptr_t ipb = (uintptr_t)bcode->p; /* Instruction pointer backup for delta computation. */
    const uintptr_t spb = (uintptr_t)isolate->stack.p; /* Stack pointer backup for delta computation. */

    register const bci_instr_t *restrict ip = bcode->p; /* Current instruction pointer. */
    register const uintptr_t sps = (uintptr_t)isolate->stack.p+sizeof(*isolate->stack.p); /* Start of stack. +1 for padding. */
    register const uintptr_t spe = (uintptr_t)(isolate->stack.p+isolate->stack.len)-sizeof(*isolate->stack.p); /* End of stack (last element). */
    register record_t *restrict sp = isolate->stack.p; /* Current stack pointer. */
    register const record_t *restrict cp = isolate->constpool.p; /* Constant pool pointer. */
    register vminterrupt_t vif = VMINT_OK; /* VM interrupt flag. */

    sp->as_uint = STK_PADD_MAGIC;

#ifdef NEO_VM_COMPUTED_GOTO
    static const void *restrict const jump_table[OPC__COUNT] = {
        label_ref(HLT),
        label_ref(NOP),
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
        label_ref(IADD_NOV),
        label_ref(ISUB_NOV),
        label_ref(IMUL_NOV),
        label_ref(IPOW_NOV),
    };
#endif

    zone_enter()

    decl_op(HLT) /* Halt VM execution. */
        goto exit;
    dispatch()

    decl_op(NOP) /* NO-Operation. */

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

    decl_op(IADD_NOV) /* Integer addition with overflow check. */
        bin_int_op(+);
    dispatch()

    decl_op(ISUB_NOV) /* Integer subtraction with overflow check. */
        bin_int_op(-);
    dispatch()

    decl_op(IMUL_NOV) /* Integer multiplication with overflow check. */
        bin_int_op(*);
    dispatch()

    decl_op(IPOW_NOV) /* Integer exponentiation with overflow check. */
        bin_int_op_call(vmop_ipow64_no_ov);
    dispatch()

#ifndef NEO_VM_COMPUTED_GOTO /* To suppress enumeration value ‘OPC__**’ not handled in switch [-Werror=switch]. */
    case OPC__COUNT:
        case OPC__MAX: break;
#endif

    zone_exit()

    exit:
    isolate->interrupt = vif;
    isolate->ip = ip;
    isolate->sp = sp;
    isolate->ip_delta = ip-(const bci_instr_t *)ipb;
    isolate->sp_delta = sp-(const record_t *)spb;
    return vif == VMINT_OK;
}
