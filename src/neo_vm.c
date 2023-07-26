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

bool NEO_HOTPROC vm_exec(vmisolate_t *isolate, register const bci_instr_t *restrict ip, size_t len) {
    neo_as(isolate && ip && len && isolate->stack.len);
    neo_as(bci_unpackopc(ip[0]) == OPC_NOP && "(prologue) first instruction must be NOP");
    neo_as(bci_unpackopc(ip[len-1]) == OPC_HLT && "(epilogue) last instruction must be HLT");

    const uintptr_t ipb = (uintptr_t)ip; /* Instruction pointer backup for delta computation. */
    const uintptr_t spb = (uintptr_t)isolate->stack.p; /* Stack pointer backup for delta computation. */
    register const uintptr_t sps = (uintptr_t)isolate->stack.p+sizeof(*isolate->stack.p); /* Start of stack. +1 for padding. */
    register const uintptr_t spe = (uintptr_t)(isolate->stack.p+isolate->stack.len)-sizeof(*isolate->stack.p); /* End of stack (last element). */
    register record_t *restrict sp = isolate->stack.p; /* Current stack pointer. */
    register vminterrupt_t vif = VMINT_OK; /* VM interrupt flag. */

    sp->as_uint = STK_PADD_MAGIC;

#ifdef NEO_VM_COMPUTED_GOTO
    static const void *restrict const jump_table[OPC__MAX] = {
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
        label_ref(LDC)
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
            pop(1);
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
