/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_vm.h"

bool vm_validate(const vmisolate_t *isolate, const bytecode_t *bcode) {
    neo_as(isolate && bcode);
    const bci_instr_t *code = bcode->p;
    size_t len = bcode->len;
    if (neo_unlikely(!code || !len)) {
        neo_error("invalid code pointer or length: %zu", len);
        return false;
    }
    if (neo_unlikely(bci_unpackopc(code[0]) != OPC_NOP)) {
        neo_error("first instruction must be NOP, but instead is: %s", opc_mnemonic[bci_unpackopc(code[0])]);
        return false;
    }
    if (neo_unlikely(bci_unpackopc(code[len-1]) != OPC_HLT)) {
        neo_error("last instruction must be HLT, but instead is: %s", opc_mnemonic[bci_unpackopc(code[len-1])]);
        return false;
    }
    for (size_t i = 0; i < len; ++i) { /* Validate the encoding of all instructions. */
        if (neo_unlikely(!bci_validate_instr(code[i]))) {
            neo_error("invalid instruction at index: %zu", i);
            return false;
        }
        opcode_t opc = bci_unpackopc(code[i]);
        if (neo_unlikely(opc == OPC_LDC)) { /* Specific instruction validation. */
            uint32_t umm24 = bci_mod1unpack_umm24(code[i]);
            if (neo_unlikely(umm24 >= isolate->constpool.len)) {
                neo_error("invalid constant pool index: %" PRIi32, umm24);
                return false;
            }
        }
    }
    return true;
}

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

#define STK_PADD_MAGIC (~0ull)

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

#define imulov(x, y, rr) if (neo_unlikely(i64_mul_overflow((x), (y), (rr)))) { *r = 0; return true; /* Overflow happened. */ }
#define umulov(x, y, rr) if (neo_unlikely(u64_mul_overflow((x), (y), (rr)))) { *r = 0; return true; /* Overflow happened. */ }

bool vmop_upow64(neo_uint_t x, neo_uint_t k, neo_uint_t *r) {
    neo_asd(r);
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
    neo_asd(r);
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
