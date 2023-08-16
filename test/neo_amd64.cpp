// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_amd64.h>

TEST(amd64, detect_cpu) {
    extended_isa_t isa = detect_cpu_isa();
    ASSERT_NE(isa, 0);
    ASSERT_TRUE((isa & AMD64ISA_SSE42)); /* If this fails on your older CPU, just ignore it. */
}
