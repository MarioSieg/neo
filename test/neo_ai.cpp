// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>

#include "neo_ai.h"

TEST(ai, precompute_luts) {
    auto now = std::chrono::high_resolution_clock::now();
    precompute_luts();
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
    std::cout << "precompute_luts took " << elapsed.count() << "ms" << std::endl;
}
