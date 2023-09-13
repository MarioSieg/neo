// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <array>
#include <unordered_set>
#include <gtest/gtest.h>
#include <neo_vm.h>

TEST(prng, diff_int) { // This test just checks that all randoms are different ouch
    prng_state_t prng {};
    prng_init_seed(&prng, 0);

    std::unordered_set<neo_int_t> gen {};
    constexpr std::size_t lim {100'000};
    gen.reserve(lim);

    for (std::size_t i {}; i < lim; ++i) {
        neo_int_t r {prng_next_i64(&prng)};
        ASSERT_NE(r, 0);
        ASSERT_TRUE(gen.insert(r).second);
    }

    std::cout << "Here are 100 random integers:" << std::endl;
    for (std::size_t i {}; i < 100; ++i) {
        std::cout << std::dec << prng_next_i64(&prng) << std::endl;
    }

    std::cout << "PRNG SEED[4]" << std::endl;
    for (std::size_t i {}; auto&& seed : prng.s) {
        std::cout << "seed[" << i++ << "] = " << seed << std::endl;
    }
}

TEST(prng, diff_float) { // This test just checks that all randoms are different ouch
    prng_state_t prng {};
    prng_init_seed(&prng, 0);

    std::unordered_set<neo_float_t> gen {};
    constexpr std::size_t lim {100'000};
    gen.reserve(lim);

    for (std::size_t i {}; i < lim; ++i) {
        neo_float_t r {prng_next_f64(&prng)};
        ASSERT_NE(r, 0.0);
        ASSERT_TRUE(r > 0.0);
        ASSERT_TRUE(r < 1.0);
        ASSERT_TRUE(gen.insert(r).second);
    }

    std::cout << "Here are 100 random floats:" << std::endl;
    for (std::size_t i {}; i < 100; ++i) {
        std::cout << prng_next_f64(&prng) << std::endl;
    }

    std::cout << "PRNG SEED[4]" << std::endl;
    for (std::size_t i {}; auto&& seed : prng.s) {
        std::cout << "seed[" << i++ << "] = " << seed << std::endl;
    }
}

#if 0
TEST(vm_exec, iror) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=0xfe},
        record_t{.as_int=8},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IROR),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    bytecode_t bcode {
    .p=code.data(),
    .len=code.size()
    };
    bcode.pool.p = constpool.data();
    bcode.pool.tags = tags.data();
    bcode.pool.len = constpool.size();
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0xfe00000000000000);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, irol) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=0x7fffffffffffffff},
        record_t{.as_int=8},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IROL),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0xffffffffffffff7f);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, islr) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0x80),
        bci_comp_mod1_imm24(OPC_IPUSH, 7),
        bci_comp_mod1_no_imm(OPC_ISLR),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 1);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, isar) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0x80),
        bci_comp_mod1_imm24(OPC_IPUSH, 7),
        bci_comp_mod1_no_imm(OPC_ISAR),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 1);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, isal) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 1),
        bci_comp_mod1_imm24(OPC_IPUSH, 48),
        bci_comp_mod1_no_imm(OPC_ISAL),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, UINT64_C(1)<<48);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ixor) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xf0),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xff),
        bci_comp_mod1_no_imm(OPC_IXOR),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
            .p=code.data(),
            .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0x0f);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ior) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xf0),
        bci_comp_mod1_imm24(OPC_IPUSH, 0x0f),
        bci_comp_mod1_no_imm(OPC_IOR),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0xff);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, iand) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0x7fffef),
        bci_comp_mod1_imm24(OPC_IPUSH, 255),
        bci_comp_mod1_no_imm(OPC_IAND),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
            .p=code.data(),
            .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0xef);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, imod_zero_division) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=22},
        record_t{.as_int=0},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IMOD),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
            .p=code.data(),
            .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_ZERODIV);
}

TEST(vm_exec, imod) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 35),
        bci_comp_mod1_imm24(OPC_IPUSH, 4),
        bci_comp_mod1_no_imm(OPC_IMOD),
        bci_comp_mod1_imm24(OPC_IPUSH, 3),
        bci_comp_mod1_no_imm(OPC_IMOD),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, (35%4)%3);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, idiv_zero_division) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=22},
        record_t{.as_int=0},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IDIV),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_ZERODIV);
}

TEST(vm_exec, idiv) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 32),
        bci_comp_mod1_imm24(OPC_IPUSH, 4),
        bci_comp_mod1_no_imm(OPC_IDIV),
        bci_comp_mod1_imm24(OPC_IPUSH, 4),
        bci_comp_mod1_no_imm(OPC_IDIV),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, (32/4)/4);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ipowo_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX},
        record_t{.as_int=3},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IPOWO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, NEO_INT_MAX*NEO_INT_MAX*NEO_INT_MAX);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ipowo) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 2),
        bci_comp_mod1_imm24(OPC_IPUSH, 2),
        bci_comp_mod1_no_imm(OPC_IPOWO),
        bci_comp_mod1_imm24(OPC_IPUSH, 2),
        bci_comp_mod1_no_imm(OPC_IPOWO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 16);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, imulo_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX},
        record_t{.as_int=3},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IMULO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, NEO_INT_MAX*3);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, imulo) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, -10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_IMULO),
        bci_comp_mod1_imm24(OPC_IPUSH, 1),
        bci_comp_mod1_no_imm(OPC_IMULO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, -50);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, isubo_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MIN},
        record_t{.as_int=1},
};
    std::vector<std::uint8_t> tags {
            RT_INT,
            RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_ISUBO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, NEO_INT_MIN-1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, isubo) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_ISUBO),
        bci_comp_mod1_imm24(OPC_IPUSH, 7),
        bci_comp_mod1_no_imm(OPC_ISUBO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, -2);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, iaddo_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX},
        record_t{.as_int=1},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_no_imm(OPC_IADDO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, NEO_INT_MAX+NEO_INT_MAX);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, iaddo) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_IADDO),
        bci_comp_mod1_imm24(OPC_IPUSH, -22),
        bci_comp_mod1_no_imm(OPC_IADDO),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, -7);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ipow_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX},
        record_t{.as_int=3},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IPOW),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_OVERFLOW);
}

TEST(vm_exec, ipow) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 2),
        bci_comp_mod1_imm24(OPC_IPUSH, 4),
        bci_comp_mod1_no_imm(OPC_IPOW),
        bci_comp_mod1_imm24(OPC_IPUSH, 3),
        bci_comp_mod1_no_imm(OPC_IPOW),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 4096);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, imul_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX},
        record_t{.as_int=2},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_IMUL),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_OVERFLOW);
}

TEST(vm_exec, imul) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_IMUL),
        bci_comp_mod1_imm24(OPC_IPUSH, 2),
        bci_comp_mod1_no_imm(OPC_IMUL),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 100);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, isub_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MIN},
        record_t{.as_int=1},
    };
    std::vector<std::uint8_t> tags {
        RT_INT,
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 1),
        bci_comp_mod1_no_imm(OPC_ISUB),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_OVERFLOW);
}

TEST(vm_exec, isub) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_ISUB),
        bci_comp_mod1_imm24(OPC_IPUSH, -22),
        bci_comp_mod1_no_imm(OPC_ISUB),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 27);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, iadd_overflow) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MAX}
    };
    std::vector<std::uint8_t> tags {
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_no_imm(OPC_IADD),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_OVERFLOW);
}

TEST(vm_exec, iadd_overflow_neg) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {
        record_t{.as_int=NEO_INT_MIN}
    };
    std::vector<std::uint8_t> tags {
        RT_INT
    };
    ASSERT_EQ(constpool.size(), tags.size());

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_no_imm(OPC_IADD),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_ARI_OVERFLOW);
}

TEST(vm_exec, iadd) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 10),
        bci_comp_mod1_imm24(OPC_IPUSH, 5),
        bci_comp_mod1_no_imm(OPC_IADD),
        bci_comp_mod1_imm24(OPC_IPUSH, -22),
        bci_comp_mod1_no_imm(OPC_IADD),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, -7);
    ASSERT_EQ(vm.sp, stack.data() + 1); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 1);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ipush) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xfefe),
        bci_comp_mod1_imm24(OPC_IPUSH, BCI_MOD1IMM24MIN),
        bci_comp_mod1_imm24(OPC_IPUSH, BCI_MOD1IMM24MAX),
        bci_comp_mod1_no_imm(OPC_POP),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[0].as_uint, UINT64_MAX);
    ASSERT_EQ(stack[1].as_int, 0xfefe); /* + 1 because one is padding */
    ASSERT_EQ(stack[2].as_int, BCI_MOD1IMM24MIN); /* + 1 because one is padding */
    ASSERT_EQ(stack[3].as_int, BCI_MOD1IMM24MAX); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp, stack.data() + 2); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 2);
    ASSERT_EQ(vm.ip_delta, code.size() - 1);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, ldc) {
    std::array<record_t, 8> stack {};
    std::vector<record_t> constpool {};
    constpool.resize(BCI_MOD1UMM24MAX + 1);
    constpool[0].as_int=BCI_MOD1IMM24MIN;
    constpool[BCI_MOD1UMM24MAX].as_int=0xfefe;
    std::vector<std::uint8_t> tags {};
    tags.resize(constpool.size());
    tags[0] = RT_INT;
    tags[BCI_MOD1UMM24MAX] = RT_INT;

    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();
    vm.constpool.p = constpool.data();
    vm.constpool.tags = tags.data();
    vm.constpool.len = constpool.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_umm24(OPC_LDC, 0),
        bci_comp_mod1_umm24(OPC_LDC, BCI_MOD1UMM24MAX),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_TRUE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[0].as_uint, UINT64_MAX);
    ASSERT_EQ(stack[1].as_int, BCI_MOD1IMM24MIN); /* + 1 because one is padding */
    ASSERT_EQ(stack[2].as_int, 0xfefe); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp, stack.data() + 2); /* + 1 because one is padding */
    ASSERT_EQ(vm.sp_delta, 2);
    ASSERT_EQ(vm.ip_delta, 3);
    ASSERT_EQ(vm.interrupt, VMINT_OK);
}

TEST(vm_exec, stack_underflow) {
    std::array<record_t, 8> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_no_imm(OPC_POP),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_STK_UNDERFLOW);
    ASSERT_EQ(vm.sp_delta, 0);
}

TEST(vm_exec, stack_overflow) {
    std::array<record_t, 3> stack {};
    vmisolate_t vm {};
    vm.stack.p = stack.data();
    vm.stack.len = stack.size();

    std::vector<bci_instr_t> code {
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xdead),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xfefe),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xfefe),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(stack[1].as_int, 0xdead); /* + 1 because one is padding */
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
        bci_comp_mod1_no_imm(OPC_NOP),
        bci_comp_mod1_imm24(OPC_IPUSH, 0xdead),
        bci_comp_mod1_no_imm(OPC_HLT)
    };

    const bytecode_t bcode {
        .p=code.data(),
        .len=code.size()
    };
    ASSERT_TRUE(bc_validate(&bcode, &vm));
    ASSERT_FALSE(vm_exec(&vm, &bcode));
    ASSERT_EQ(vm.interrupt, VMINT_STK_OVERFLOW);
}

TEST(constant_pool, put_has_get) {
    constpool_t cp {};
    constpool_init(&cp, 0);
    ASSERT_EQ(cp.len, 0);

    ASSERT_EQ(constpool_put(&cp, RT_INT, record_t {.as_int=12345}), 0);
    ASSERT_EQ(constpool_put(&cp, RT_INT, record_t {.as_int=255}), 1);
    ASSERT_EQ(constpool_put(&cp, RT_INT, record_t {.as_int=255}), 1); /* Entry already exists. */

    ASSERT_TRUE(constpool_has(&cp, 0));
    ASSERT_TRUE(constpool_has(&cp, 1));

    record_t value {};
    rtag_t tag {};
    ASSERT_TRUE(constpool_get(&cp, 0, &value, &tag));
    ASSERT_EQ(value.as_int, 12345);
    ASSERT_EQ(tag, RT_INT);

    ASSERT_TRUE(constpool_get(&cp, 1, &value, &tag));
    ASSERT_EQ(value.as_int, 255);
    ASSERT_EQ(tag, RT_INT);

    constpool_free(&cp);
}
#endif