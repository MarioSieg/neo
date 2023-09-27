// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_bc.h>

TEST(bytecode, append) {
    bytecode_t bc {};
    bc_init(&bc);
    ASSERT_EQ(bc.len, 0);
    bc_emit(&bc, bci_comp_mod1_imm24(OPC_IPUSH, 0));
    ASSERT_EQ(bc.len, 1);
    bc_emit(&bc, bci_comp_mod1_imm24(OPC_IPUSH, 1));
    ASSERT_EQ(bc.len, 2);
    bc_free(&bc);
}

TEST(bytecode, disassemble) {
    bytecode_t bc {};
    bc_init(&bc);
    bc_emit_ipush(&bc, 0);
    bc_emit_ipush(&bc, 0x7ffff);
    bc_emit_ipush(&bc, NEO_INT_MAX);
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_IXOR));
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_IXOR));
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_IADDO));
    bc_emit(&bc, bci_comp_mod1_imm24(OPC_IPUSH, 2));
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_IMULO));
    bc_emit_ipush(&bc, NEO_INT_MIN>>1);
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_ISUB));
    bc_emit(&bc, bci_comp_mod1_no_imm(OPC_POP));
    bc_finalize(&bc);
    bc_disassemble(&bc, stdout, true);
    bc_free(&bc);
}

TEST(bytecode, encode_imm24) {
    ASSERT_EQ(bci_mod1unpack_imm24(bci_comp_mod1_imm24(OPC_IPUSH, 0)), 0);
    ASSERT_EQ(bci_mod1unpack_imm24(bci_comp_mod1_imm24(OPC_IPUSH, BCI_MOD1IMM24MIN)), BCI_MOD1IMM24MIN);
    ASSERT_EQ(bci_mod1unpack_imm24(bci_comp_mod1_imm24(OPC_IPUSH, BCI_MOD1IMM24MAX)), BCI_MOD1IMM24MAX);
}

TEST(bytecode, encode_umm24) {
    ASSERT_EQ(bci_mod1unpack_umm24(bci_comp_mod1_umm24(OPC_LDC, 0)), 0);
    ASSERT_EQ(bci_mod1unpack_umm24(bci_comp_mod1_umm24(OPC_LDC, BCI_MOD1UMM24MIN)), BCI_MOD1UMM24MIN);
    ASSERT_EQ(bci_mod1unpack_umm24(bci_comp_mod1_umm24(OPC_LDC, BCI_MOD1UMM24MAX)), BCI_MOD1UMM24MAX);
}

TEST(bytecode, bci_fits_u24) {
    ASSERT_TRUE(bci_fits_u24(0));
    ASSERT_TRUE(bci_fits_u24(1));
    ASSERT_TRUE(bci_fits_u24(0x7fffff));
    ASSERT_TRUE(bci_fits_u24(BCI_MOD1IMM24MAX));
    ASSERT_FALSE(bci_fits_u24(-1));
    ASSERT_FALSE(bci_fits_u24(-0x800000));
    ASSERT_FALSE(bci_fits_u24(-0x800001));
    ASSERT_FALSE(bci_fits_u24(INT32_MIN));
    ASSERT_FALSE(bci_fits_u24(INT64_MIN));
}

TEST(bytecode, bci_fits_i24) {
    ASSERT_TRUE(bci_fits_i24(0));
    ASSERT_TRUE(bci_fits_i24(1));
    ASSERT_TRUE(bci_fits_i24(0x7fffff));
    ASSERT_TRUE(bci_fits_i24(-0x800000));
    ASSERT_TRUE(bci_fits_i24(-1));
    ASSERT_TRUE(bci_fits_i24(-0x7fffff));
    ASSERT_TRUE(bci_fits_i24(BCI_MOD1IMM24MAX));
    ASSERT_TRUE(bci_fits_i24(BCI_MOD1IMM24MIN));
    ASSERT_FALSE(bci_fits_i24(0x800000));
    ASSERT_FALSE(bci_fits_i24(-0x800001));
    ASSERT_FALSE(bci_fits_i24(INT32_MAX));
    ASSERT_FALSE(bci_fits_i24(INT64_MIN));
}

TEST(bytecode, u24tou32) {
    ASSERT_EQ(bci_u24tou32(0), 0);
    ASSERT_EQ(bci_u24tou32(1), 1);
    ASSERT_EQ(bci_u24tou32(0x7fffff), 0x7fffffu);
    ASSERT_EQ(bci_u24tou32(0x800000), 0x800000u);
    ASSERT_EQ(bci_u24tou32(0xffffff), 0xffffffu);
}

TEST(bytecode, u32tou24) {
    ASSERT_EQ(bci_u32tou24(0), 0);
    ASSERT_EQ(bci_u32tou24(1), 1);
    ASSERT_EQ(bci_u32tou24(0x7fffff), 0x7fffffu);
    ASSERT_EQ(bci_u32tou24(0x7f7fffff), 0x7fffffu);
    ASSERT_EQ(bci_u32tou24(0x800000), 0x800000u);
    ASSERT_EQ(bci_u32tou24(0xffffff), 0xffffffu);
    ASSERT_EQ(bci_u32tou24(0xffffffff), 0xffffffu);
}

TEST(bytecode, i24toi32) {
    ASSERT_EQ(bci_i24toi32(0), 0);
    ASSERT_EQ(bci_i24toi32(1), 1);
    ASSERT_EQ(bci_i24toi32(0x7fffff), 0x7fffffu);
    ASSERT_EQ(bci_i24toi32(0x800000), -0x800000);
    ASSERT_EQ(bci_i24toi32(23), 23);
    ASSERT_EQ(bci_i24toi32(-23), -23);
    ASSERT_EQ(bci_i24toi32(0xffffff), -1);
}

TEST(bytecode, i32toi24) {
    ASSERT_EQ(bci_i32toi24(0), 0);
    ASSERT_EQ(bci_i32toi24(1), 1);
    ASSERT_EQ(bci_i32toi24(0x7fffff), 0x7fffff);
    ASSERT_EQ(bci_i32toi24(0x7f7fffff), 0x7fffff);
    ASSERT_EQ(bci_i32toi24(0x800000), -0x800000);
    ASSERT_EQ(bci_i32toi24(0xffffff), -1);
    ASSERT_EQ(bci_i32toi24(0xffffffff), -1);
    ASSERT_EQ(bci_i32toi24(23), 23);
    ASSERT_EQ(bci_i32toi24(-23), -23);
}

TEST(bytecode, stack_depths) {
    ASSERT_EQ(opc_depths[OPC_HLT], 0);
    ASSERT_EQ(opc_depths[OPC_IPUSH], 1);
    ASSERT_EQ(opc_depths[OPC_POP], -1);
}