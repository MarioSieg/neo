// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_core.h>

TEST(core, neo_ror64)
{
    ASSERT_EQ(neo_ror64(UINT64_C(0x0000000000000001), 0), UINT64_C(0x0000000000000001));
    ASSERT_EQ(neo_ror64(UINT64_C(1), 12), UINT64_C(1)<<52);
    ASSERT_EQ(neo_ror64(UINT64_C(0xffffffffffffffee), 8), UINT64_C(0xeeffffffffffffff));
}

TEST(core, neo_rol64)
{
    ASSERT_EQ(neo_rol64(UINT64_C(0x0000000000000001), 0), UINT64_C(0x0000000000000001));
    ASSERT_EQ(neo_rol64(UINT64_C(1), 12), UINT64_C(1)<<12);
    ASSERT_EQ(neo_rol64(UINT64_C(0xabffffffffffffff), 8), UINT64_C(0xffffffffffffffab));
}

TEST(core, neo_bswap32)
{
    ASSERT_EQ(neo_bswap32(UINT32_C(0xabcdef12)), UINT32_C(0x12efcdab));
    ASSERT_EQ(neo_bswap32(UINT32_C(0x00000000)), UINT32_C(0x00000000));
    ASSERT_EQ(neo_bswap32(UINT32_C(0xffffffff)), UINT32_C(0xffffffff));
}

TEST(core, neo_bswap64)
{
    ASSERT_EQ(neo_bswap64(UINT64_C(0xabcdef1234567890)), UINT64_C(0x9078563412efcdab));
    ASSERT_EQ(neo_bswap64(UINT64_C(0x0000000000000000)), UINT64_C(0x0000000000000000));
    ASSERT_EQ(neo_bswap64(UINT64_C(0xffffffffffffffff)), UINT64_C(0xffffffffffffffff));
}

TEST(core, osi_page_size) {
    neo_osi_init();
    ASSERT_NE(neo_osi->page_size, 0);
    neo_osi_shutdown();
}

TEST(core, neo_bsf32)
{
    uint32_t x = 0x08040000;
    ASSERT_EQ(neo_bsf32(x), 18);

    x = 0x00000100;
    ASSERT_EQ(neo_bsf32(x), 8);

    x = 0x00000001;
    ASSERT_EQ(neo_bsf32(x), 0);
}

TEST(core, neo_bsr32)
{
    ASSERT_EQ(neo_bsr32(0x80000000u), 31);
    ASSERT_EQ(neo_bsr32(0x40000000u), 30);
    ASSERT_EQ(neo_bsr32(0x20000000u), 29);
    ASSERT_EQ(neo_bsr32(0x10000000u), 28);
    ASSERT_EQ(neo_bsr32(0x08000000u), 27);
    ASSERT_EQ(neo_bsr32(0x04000000u), 26);
    ASSERT_EQ(neo_bsr32(0x02000000u), 25);
    ASSERT_EQ(neo_bsr32(0x01000000u), 24);
    ASSERT_EQ(neo_bsr32(0x00FF0000u), 23);
    ASSERT_EQ(neo_bsr32(0x0000FF00u), 15);
    ASSERT_EQ(neo_bsr32(0x000000FFu), 7);
    ASSERT_EQ(neo_bsr32(0x00000000u), 0);
}

TEST(core, neo_atomic_compare_exchange)
{
    bool result;
    volatile int64_t shared_var = 0;
    int64_t expected = 0;
    int64_t desired = 10;
    result = neo_atomic_compare_exchange_weak(&shared_var, &expected, &desired, NEO_MEMORD_SEQ_CST, NEO_MEMORD_RELX);
    ASSERT_TRUE(result);
    ASSERT_EQ(shared_var, 10);

    shared_var = 5;
    expected = 0;
    desired = 10;
    result = neo_atomic_compare_exchange_weak(&shared_var, &expected, &desired, NEO_MEMORD_SEQ_CST, NEO_MEMORD_RELX);
    ASSERT_FALSE(result);
    ASSERT_EQ(shared_var, 5);

    shared_var = 0;
    expected = 0;
    desired = 10;
    result = neo_atomic_compare_exchange_strong(&shared_var, &expected, &desired, NEO_MEMORD_SEQ_CST, NEO_MEMORD_RELX);
    ASSERT_TRUE(result);
    ASSERT_EQ(shared_var, 10);

    shared_var = 5;
    expected = 0;
    desired = 10;
    result = neo_atomic_compare_exchange_strong(&shared_var, &expected, &desired, NEO_MEMORD_SEQ_CST, NEO_MEMORD_RELX);
    ASSERT_FALSE(result);
    ASSERT_EQ(shared_var, 5);
}

TEST(core, neo_atomic_exchange)
{
    volatile int64_t shared_var = 5;
    volatile int64_t shared_var2 = 3;

    int64_t prev_value = neo_atomic_exchange(&shared_var, 10, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 10);
    ASSERT_EQ(prev_value, 5);

    prev_value = neo_atomic_exchange(&shared_var, 20, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 20);
    ASSERT_EQ(prev_value, 10);

    prev_value = neo_atomic_exchange(&shared_var, 30, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 30);
    ASSERT_EQ(prev_value, 20);

    prev_value = neo_atomic_exchange(&shared_var, 40, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 40);
    ASSERT_EQ(prev_value, 30);

    prev_value = neo_atomic_exchange(&shared_var, 50, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, 50);
    ASSERT_EQ(prev_value, 40);

    prev_value = neo_atomic_exchange(&shared_var2, 4, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 4);
    ASSERT_EQ(prev_value, 3);
}

TEST(core, neo_atomic_fetch_xor)
{
    volatile int64_t shared_var = 5;
    volatile int64_t shared_var2 = 3;

    neo_atomic_fetch_xor(&shared_var, 7, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 2);

    neo_atomic_fetch_xor(&shared_var, 5, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 7);

    neo_atomic_fetch_xor(&shared_var, 6, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 1);

    neo_atomic_fetch_xor(&shared_var, 4, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 5);

    neo_atomic_fetch_xor(&shared_var, 7, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, 2);

    neo_atomic_fetch_xor(&shared_var2, 1, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 2);
}

TEST(core, neo_atomic_fetch_or)
{
    volatile int64_t shared_var = 0;
    volatile int64_t shared_var2 = 5;

    neo_atomic_fetch_or(&shared_var, 5, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 5);

    neo_atomic_fetch_or(&shared_var, 7, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 7);

    neo_atomic_fetch_or(&shared_var, 6, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 7);

    neo_atomic_fetch_or(&shared_var, 4, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 7);

    neo_atomic_fetch_or(&shared_var, 8, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, 15);

    neo_atomic_fetch_or(&shared_var2, 3, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 7);

}

TEST(core, neo_atomic_fetch_and) {
    volatile int64_t shared_var = 15;
    volatile int64_t shared_var2 = 1;

    neo_atomic_fetch_and(&shared_var, 5, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 5);

    neo_atomic_fetch_and(&shared_var, 7, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 5);

    neo_atomic_fetch_and(&shared_var, 6, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 4);

    neo_atomic_fetch_and(&shared_var, 4, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 4);

    neo_atomic_fetch_and(&shared_var, 7, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, 4);

    neo_atomic_fetch_and(&shared_var2, 3, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 1);

}

TEST(core, neo_atomic_fetch_sub) {
    volatile int64_t shared_var = 10;
    volatile int64_t shared_var2 = 5;

    neo_atomic_fetch_sub(&shared_var, 1, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 9);

    neo_atomic_fetch_sub(&shared_var, 1, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 8);

    neo_atomic_fetch_sub(&shared_var, 2, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 6);

    neo_atomic_fetch_sub(&shared_var, 3, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 3);

    neo_atomic_fetch_sub(&shared_var, 4, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, -1);

    neo_atomic_fetch_sub(&shared_var2, -5, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 10);

}

TEST(core, neo_atomic_fetch_add)
{
    volatile int64_t shared_var = 0;
    volatile int64_t shared_var2 = 5;

    neo_atomic_fetch_add(&shared_var, 1, NEO_MEMORD_RELX);
    ASSERT_EQ(shared_var, 1);

    neo_atomic_fetch_add(&shared_var, 1, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var, 2);

    neo_atomic_fetch_add(&shared_var, 2, NEO_MEMORD_ACQ);
    ASSERT_EQ(shared_var, 4);

    neo_atomic_fetch_add(&shared_var, 3, NEO_MEMORD_REL);
    ASSERT_EQ(shared_var, 7);

    neo_atomic_fetch_add(&shared_var, 4, NEO_MEMORD_ACQ_REL);
    ASSERT_EQ(shared_var, 11);

    neo_atomic_fetch_add(&shared_var2, -5, NEO_MEMORD_SEQ_CST);
    ASSERT_EQ(shared_var2, 0);

}
