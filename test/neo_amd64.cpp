// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_amd64.h>
#include <Zydis/Zydis.h>

[[nodiscard]] static std::vector<ZydisDisassembledInstruction> disassemble(const mcode_t *p, std::size_t len) {
    neo_dassert(p);
    auto rip = std::bit_cast<std::uintptr_t>(p);
    std::size_t offset = 0;
    ZydisDisassembledInstruction instruction {};
    std::vector<ZydisDisassembledInstruction> result {};
    while (ZYAN_SUCCESS(ZydisDisassembleATT(
        ZYDIS_MACHINE_MODE_LONG_64,
        rip,
        p + offset,
        len - offset,
        &instruction
    ))) {
        printf("%016" PRIX64 "  %s\n", rip, instruction.text);
        result.emplace_back(instruction);
        offset += instruction.info.length;
        rip += instruction.info.length;
    }
    return result;
}

TEST(amd64, detect_cpu) {
    extended_isa_t isa = detect_cpu_isa();
    ASSERT_NE(isa, 0);
    ASSERT_TRUE((isa & AMD64ISA_SSE42)); /* If this fails on your older CPU, just ignore it. */
}

TEST(amd64, emit_mov_reg_imm_zero) {
    constexpr auto len = 1024<<3;
    mcode_t buf[len] {};
    mcode_t *p = buf+len;
    mov_ri(&p, RID_RAX, (imm_t) {
            .u64 = 0
    });
    std::vector<ZydisDisassembledInstruction> instructions = disassemble(p, buf+len-p);
    ASSERT_EQ(instructions.size(), 1);
    ASSERT_EQ(instructions[0].info.mnemonic, ZYDIS_MNEMONIC_XOR);
    ASSERT_EQ(instructions[0].info.operand_count_visible, 2);
    ASSERT_EQ(instructions[0].operands[0].type, ZYDIS_OPERAND_TYPE_REGISTER);
    ASSERT_EQ(instructions[0].operands[0].reg.value, ZYDIS_REGISTER_EAX);
    ASSERT_EQ(instructions[0].operands[1].type, ZYDIS_OPERAND_TYPE_REGISTER);
    ASSERT_EQ(instructions[0].operands[1].reg.value, ZYDIS_REGISTER_EAX);
}

TEST(amd64, emit_mov_reg_imm_32) {
    constexpr auto len = 1024<<3;
    mcode_t buf[len] {};
    mcode_t *p = buf+len;
    mov_ri(&p, RID_RAX, (imm_t) {
        .u64 = 10
    });
    std::vector<ZydisDisassembledInstruction> instructions = disassemble(p, buf+len-p);
    ASSERT_EQ(instructions.size(), 1);
    ASSERT_EQ(instructions[0].info.mnemonic, ZYDIS_MNEMONIC_MOV);
    ASSERT_EQ(instructions[0].info.operand_count_visible, 2);
    ASSERT_EQ(instructions[0].operands[0].type, ZYDIS_OPERAND_TYPE_REGISTER);
    ASSERT_EQ(instructions[0].operands[0].reg.value, ZYDIS_REGISTER_EAX);
    ASSERT_EQ(instructions[0].operands[1].type, ZYDIS_OPERAND_TYPE_IMMEDIATE);
    ASSERT_EQ(instructions[0].operands[1].imm.value.u, 10);
}

TEST(amd64, emit_mov_reg_imm_64) {
    constexpr auto len = 1024<<3;
    mcode_t buf[len] {};
    mcode_t *p = buf+len;
    mov_ri(&p, RID_RAX, (imm_t) {
        .u64 = 0xffffffffull << 3
    });
    std::vector<ZydisDisassembledInstruction> instructions = disassemble(p, buf+len-p);
    ASSERT_EQ(instructions.size(), 1);
    ASSERT_EQ(instructions[0].info.mnemonic, ZYDIS_MNEMONIC_MOV);
    ASSERT_EQ(instructions[0].info.operand_count_visible, 2);
    ASSERT_EQ(instructions[0].operands[0].type, ZYDIS_OPERAND_TYPE_REGISTER);
    ASSERT_EQ(instructions[0].operands[0].reg.value, ZYDIS_REGISTER_RAX);
    ASSERT_EQ(instructions[0].operands[1].type, ZYDIS_OPERAND_TYPE_IMMEDIATE);
    ASSERT_EQ(instructions[0].operands[1].imm.value.u, 0xffffffffull<<3);
}

#if 0
TEST(amd64, emit_alu_reg_imm64) {
    constexpr auto len = 1024<<3;
    mcode_t buf[len] {};
    mcode_t *p = buf+len;
    xop_ri(&p, XA_ADD, RID_R12, (imm_t) {
        .u32 = 200
    }, true);
    std::vector<ZydisDisassembledInstruction> instructions = disassemble(p, buf+len-p);
    ASSERT_EQ(instructions.size(), 1);
}
#endif
