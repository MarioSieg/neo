/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include <gtest/gtest.h>
#include <neort_bc.h>

TEST(runtime, bytecode_pack_opc) {
    bci_instr_t instr{bci_packopc(0, 127)};
    ASSERT_EQ(bci_unpackopc(instr), 127);
    ASSERT_EQ(instr, 127);
}

TEST(runtime, bytecode_pack_mod) {
    bci_instr_t instr{bci_packmod(0, BCI_MOD2)};
    ASSERT_EQ(bci_unpackmod(instr), BCI_MOD2);
    ASSERT_EQ(instr, BCI_MOD2<<7);
}

TEST(runtime, bytecode_pack_opc_mod) {
    bci_instr_t instr{bci_packopc(0, 8)| bci_packmod(0, BCI_MOD2)};
    ASSERT_EQ(bci_unpackopc(instr), 8);
    ASSERT_EQ(bci_unpackmod(instr), BCI_MOD2);
    ASSERT_EQ(instr, 0b1000'1000);
    ASSERT_EQ(instr >> 8, 0);
}

TEST(runtime, bytecode_switch_mod) {
    bci_instr_t instr{bci_packopc(0, 8)| bci_packmod(0, BCI_MOD2)};
    instr = bci_switchmod(instr);
    ASSERT_EQ(bci_unpackopc(instr), 8);
    ASSERT_EQ(bci_unpackmod(instr), BCI_MOD1);
    ASSERT_EQ(instr >> 8, 0);
}

TEST(runtime, bytecode_mod1_imm24_sign) {
    bci_instr_t instr{bci_packopc(0, 8)| bci_packmod(0, BCI_MOD2)};
    instr = bci_switchmod(instr);
    ASSERT_EQ(bci_unpackopc(instr), 8);
    ASSERT_EQ(bci_unpackmod(instr), BCI_MOD1);
    ASSERT_EQ(instr >> 8, 0);
}

TEST(runtime, bci_mod1imm24_sign) {
    EXPECT_EQ(0, bci_mod1imm24_sign(0x000000));
    EXPECT_EQ(1, bci_mod1imm24_sign(0x800000));
    EXPECT_EQ(1, bci_mod1imm24_sign(0xff8000));
    EXPECT_EQ(0, bci_mod1imm24_sign(0x7f8000));
}

TEST(runtime, bci_mod1unpack_imm24) {
    EXPECT_EQ(0x00000000, bci_mod1unpack_imm24(0x00000000));
    EXPECT_EQ(0x00000001, bci_mod1unpack_imm24(0x00000100));
    EXPECT_EQ(0x00ffffff, bci_mod1unpack_imm24(0x00ffffff00));
}

TEST(runtime, bci_mod1pack_imm24) {
    EXPECT_EQ(0x00000123u<<8, bci_mod1pack_imm24(0x00000000, 0x00000123));
    EXPECT_EQ(0x00fedcbau<<8, bci_mod1pack_imm24(0x00000000, 0x00fedcba));
}

TEST(runtime, bci_mod2imm16_sign) {
    EXPECT_EQ(0, bci_mod2imm16_sign(0x0000));
    EXPECT_EQ(1, bci_mod2imm16_sign(0x8000));
    EXPECT_EQ(1, bci_mod2imm16_sign(0xff80));
    EXPECT_EQ(0, bci_mod2imm16_sign(0x7f80));
}

TEST(runtime, bci_mod2unpack_imm16) {
    EXPECT_EQ(0x0000, bci_mod2unpack_imm16(0x00000000));
    EXPECT_EQ(0x0001, bci_mod2unpack_imm16(0x00010000));
    EXPECT_EQ(0xffff, bci_mod2unpack_imm16(0xffff0000));
}

TEST(runtime, bci_mod2pack_imm16) {
    EXPECT_EQ(0x00012345u<<16, bci_mod2pack_imm16(0x00000000, 0x12345));
    EXPECT_EQ(0x00fedcbau<<16, bci_mod2pack_imm16(0x00000000, 0xfedcba));
}

TEST(runtime, bci_mod2unpack_com) {
    EXPECT_EQ(0, bci_mod2unpack_com(0x0000));
    EXPECT_EQ(1, bci_mod2unpack_com(0x8000));
    EXPECT_EQ(1, bci_mod2unpack_com(0xff80));
    EXPECT_EQ(0, bci_mod2unpack_com(0x7f80));
}
