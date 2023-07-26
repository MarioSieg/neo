// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_vm.h>

TEST(vm_exec, ipush) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1(OPC_NOP, 0),
        bci_comp_mod1(OPC_IPUSH, 0xabcdef),
        bci_comp_mod1(OPC_HLT, 0)
    };

    ASSERT_TRUE(vm_exec(&vm, code.data(), code.size()));
    ASSERT_EQ(stack[0].as_uint, UINT64_MAX);
    ASSERT_EQ(stack[1].as_int, 0xabcdef); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, 2);
}

TEST(vm_exec, stack_underflow) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1(OPC_NOP, 0),
        bci_comp_mod1(OPC_POP, 0),
        bci_comp_mod1(OPC_HLT, 0)
    };

    ASSERT_FALSE(vm_exec(&vm, code.data(), code.size()));
    ASSERT_EQ(vm.interrupt, VMINT_STK_UNDERFLOW);
    ASSERT_EQ(vm.sp_delta, 0);
}

TEST(vm_exec, stack_overflow) {
    std::array<record_t, 3> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1(OPC_NOP, 0),
        bci_comp_mod1(OPC_IPUSH, 0xabcdef),
        bci_comp_mod1(OPC_IPUSH, 0xfefe),
        bci_comp_mod1(OPC_IPUSH, 0xfefe),
        bci_comp_mod1(OPC_HLT, 0)
    };

    ASSERT_FALSE(vm_exec(&vm, code.data(), code.size()));
    ASSERT_EQ(stack[1].as_int, 0xabcdef); /* + 1 because one is padding */
    ASSERT_EQ(stack[2].as_int, 0xfefe); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp, stack.data() + 2);
    ASSERT_EQ(vm.sp_delta, 2);
    ASSERT_EQ(vm.ip_delta, 3);
    ASSERT_EQ(vm.interrupt, VMINT_STK_OVERFLOW);
}

TEST(vm_exec, stack_overflow2) {
    std::array<record_t, 1> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1(OPC_NOP, 0),
        bci_comp_mod1(OPC_IPUSH, 0xabcdef),
        bci_comp_mod1(OPC_HLT, 0)
    };

    ASSERT_FALSE(vm_exec(&vm, code.data(), code.size()));
    ASSERT_EQ(vm.interrupt, VMINT_STK_OVERFLOW);
}
