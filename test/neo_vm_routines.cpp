// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_vm.h>

TEST(vm_ops, ceil_pos) {
    EXPECT_DOUBLE_EQ(vmop_ceil(3.7), std::ceil(3.7));
    EXPECT_DOUBLE_EQ(vmop_ceil(5.2), std::ceil(5.2));
}

TEST(vm_ops, ceil_neg) {
    EXPECT_DOUBLE_EQ(vmop_ceil(-3.7), std::ceil(-3.7));
    EXPECT_DOUBLE_EQ(vmop_ceil(-5.2), std::ceil(-5.2));
}

TEST(vm_ops, floor_pos) {
    EXPECT_DOUBLE_EQ(vmop_floor(3.7), std::floor(3.7));
    EXPECT_DOUBLE_EQ(vmop_floor(5.2), std::floor(5.2));
}

TEST(vm_ops, floor_neg) {
    EXPECT_DOUBLE_EQ(vmop_floor(-3.7), std::floor(-3.7));
    EXPECT_DOUBLE_EQ(vmop_floor(-5.2), std::floor(-5.2));
}

TEST(vm_ops, mod_pos) {
    EXPECT_DOUBLE_EQ(vmop_mod(7.0, 3.0), std::fmod(7.0, 3.0));
    EXPECT_DOUBLE_EQ(vmop_mod(10.5, 3.0), std::fmod(10.5, 3.0));
}

TEST(vm_ops, mod_neg) {
    EXPECT_DOUBLE_EQ(vmop_mod(-7.0, 3.0), std::fmod(-7.0, 3.0));
    EXPECT_DOUBLE_EQ(vmop_mod(-10.5, 3.0), std::fmod(-10.5, 3.0));
}

TEST(vm_ops, ceil_zero) {
    EXPECT_DOUBLE_EQ(vmop_ceil(0.0), std::ceil(0.0));
}

TEST(vm_ops, floor_zero) {
    EXPECT_DOUBLE_EQ(vmop_floor(0.0), std::floor(0.0));
}

TEST(vm_ops, mod_zero) {
    EXPECT_DOUBLE_EQ(vmop_mod(0.0, 3.0), std::fmod(0.0, 3.0));
    EXPECT_TRUE(isnan(vmop_mod(7.0, 0.0))); // Division by zero, expect NaN.
}

TEST(vm_ops, ceil_large_numbers) {
    EXPECT_DOUBLE_EQ(vmop_ceil(1e15), std::ceil(1e15));
    EXPECT_DOUBLE_EQ(vmop_ceil(1e30), std::ceil(1e30));
}

TEST(vm_ops, floor_large_numbers) {
    EXPECT_DOUBLE_EQ(vmop_floor(1e15), std::floor(1e15));
    EXPECT_DOUBLE_EQ(vmop_floor(1e30), std::floor(1e30));
}

TEST(vm_ops, mod_large_numbers) {
    EXPECT_DOUBLE_EQ(vmop_mod(1e15, 3.0), std::fmod(1e15, 3.0));
    EXPECT_DOUBLE_EQ(vmop_mod(1e30, 3.0), std::fmod(1e30, 3.0));
}

