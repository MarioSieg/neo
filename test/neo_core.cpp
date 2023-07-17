/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>

#include <neo_core.h>

TEST(core, utf8_validate) {
    const uint8_t valid_utf8[] = {0x41, 0xC3, 0x89, 0xE6, 0x97, 0xA5};
    const uint8_t invalid_utf8[] = {0x41, 0xC3, 0x89, 0x80};
    neo_unicode_result_t result;

    result = neo_utf8_validate(valid_utf8, sizeof(valid_utf8));
    ASSERT_EQ(result.code, NEO_UNIERR_OK);

    result = neo_utf8_validate(invalid_utf8, sizeof(invalid_utf8));
    ASSERT_EQ(result.code, NEO_UNIERR_TOO_LONG);
}

TEST(core, utf8_count_codepoints) {
    const uint8_t utf8[] = {0x41, 0xC3, 0x89, 0xE6, 0x97, 0xA5};
    size_t count = neo_utf8_count_codepoints(utf8, sizeof(utf8));
    ASSERT_EQ(count, 3);
}

TEST(core, utf8_len_from_utf32) {
    const uint32_t utf32[] = {0x0041, 0x00C9, 0x65E5, 0x672C, 0x304D};
    size_t len = neo_utf8_len_from_utf32(utf32, sizeof(utf32) / sizeof(uint32_t));
    ASSERT_EQ(len, 12);
}

TEST(core, utf16_len_from_utf8) {
    const uint8_t utf8[] = {0x41, 0xC3, 0x89, 0xE6, 0x97, 0xA5};
    size_t len = neo_utf16_len_from_utf8(utf8, sizeof(utf8));
    ASSERT_EQ(len, 3);
}

TEST(core, utf16_len_from_utf32) {
    const uint32_t utf32[] = {0x0041, 0x00C9, 0x65E5, 0x672C, 0x304D};
    size_t len = neo_utf16_len_from_utf32(utf32, sizeof(utf32) / sizeof(uint32_t));
    ASSERT_EQ(len, 5);
}

TEST(core, utf32_validate) {
    const uint32_t valid_utf32[] = {0x0041, 0x00C9, 0x65E5, 0x672C, 0x304D};
    const uint32_t invalid_utf32[] = {0x0041, 0x00C9, 0x110000, 0x672C, 0x304D};
    neo_unicode_result_t result;

    result = neo_utf32_validate(valid_utf32, sizeof(valid_utf32) / sizeof(uint32_t));
    ASSERT_EQ(result.code, NEO_UNIERR_OK);

    result = neo_utf32_validate(invalid_utf32, sizeof(invalid_utf32) / sizeof(uint32_t));
    ASSERT_EQ(result.code, NEO_UNIERR_TOO_LARGE);
}

TEST(core, utf32_count_codepoints) {
    const uint32_t utf32[] = {0x0041, 0x00C9, 0x65E5, 0x672C, 0x304D};
    size_t count = neo_utf32_count_codepoints(utf32, sizeof(utf32) / sizeof(uint32_t));
    ASSERT_EQ(count, 5);
}

TEST(core, valid_utf8_to_utf32) {
    const uint8_t utf8[] = {0x41, 0xC3, 0x89, 0xE6, 0x97, 0xA5};
    const size_t utf8len = sizeof(utf8);
    uint32_t utf32[utf8len];
    size_t convertedlen = neo_valid_utf8_to_utf32(utf8, utf8len, utf32);
    ASSERT_EQ(convertedlen, 3);
    ASSERT_EQ(utf32[0], 0x00000041);
    ASSERT_EQ(utf32[1], 0x000000c9);
    ASSERT_EQ(utf32[2], 0x000065e5);
}

TEST(core, valid_utf32_to_utf8) {
    const uint32_t utf32[] = {0x0041, 0x00C9, 0x65E5, 0x672C, 0x304D};
    const size_t utf32len = sizeof(utf32) / sizeof(uint32_t);
    uint8_t utf8[utf32len * 4]; // Allocate enough space for the worst-case scenario
    size_t convertedlen = neo_valid_utf32_to_utf8(utf32, utf32len, utf8);
    ASSERT_GT(convertedlen, 0);
    ASSERT_TRUE(memcmp(utf8, "\x41\xC3\x89\xE6\x97\xA5\xE6\x9C\xAC\xE3\x81\x8D", convertedlen) == 0);
}

TEST(core, bound_check_within_bounds)
{
    int array[10];
    memset(array, 0, sizeof(array));
    int *ptr = &array[5];

    ASSERT_TRUE(neo_bnd_check(ptr, array, sizeof(array)));
}

TEST(core, bound_check_at_lower_bound)
{
    int array[10];
    memset(array, 0, sizeof(array));
    int *ptr = array;

    ASSERT_TRUE(neo_bnd_check(ptr, array, sizeof(array)));
}

TEST(core, bound_check_at_upper_bound)
{
    int array[10];
    memset(array, 0, sizeof(array));
    int *ptr = &array[9];

    ASSERT_TRUE(neo_bnd_check(ptr, array, sizeof(array)));
}

TEST(core, bound_check_outside_bounds)
{
    static volatile int array[10];
    memset((void*)array, 0, sizeof(array));
    volatile int *volatile ptr = array; /* to silence -Werror=array-bounds */
    ptr += 12;

    ASSERT_FALSE(neo_bnd_check((void*)ptr, (void*)array, sizeof(array)));
}

TEST(core, bound_check_empty_bounds)
{
    int array[1];
    memset(array, 0, sizeof(array));
    int *ptr = NULL;

    ASSERT_FALSE(neo_bnd_check(ptr, array, 0));
}

TEST(core, neo_leb128_decode_u64)
{
    uint8_t input[3] = {0xe5, 0x8e, 0x26};
    uint64_t o = 0;
    neo_leb128_decode_u64(input, input + sizeof(input), &o);
    ASSERT_EQ(o, 624485);
}

TEST(core, neo_leb128_encode_u64)
{
    uint8_t buf[8];
    size_t delta = neo_leb128_encode_u64(buf, buf + sizeof(buf), 624485);
    ASSERT_EQ(3, delta);
    ASSERT_EQ(0xe5, buf[0]);
    ASSERT_EQ(0x8e, buf[1]);
    ASSERT_EQ(0x26, buf[2]);
}

TEST(core, neo_leb128_decode_i64)
{
    uint8_t input[3] = { 0xc0, 0xbb, 0x78 };
    int64_t o = 0;
    neo_leb128_decode_i64(input, input + sizeof(input), &o);
    ASSERT_EQ(o, -123456);
}

TEST(core, neo_leb128_encode_i64)
{
    uint8_t buf[8];
    size_t delta = neo_leb128_encode_i64(buf, buf + sizeof(buf), -123456);
    ASSERT_EQ(3, delta);
    ASSERT_EQ(0xc0, buf[0]);
    ASSERT_EQ(0xbb, buf[1]);
    ASSERT_EQ(0x78, buf[2]);
}

TEST(core, neo_leb128_encode_decode_i64_max)
{
    uint8_t buf1[10];
    size_t a = neo_leb128_encode_i64(buf1, buf1 + sizeof(buf1), INT64_MAX);
    int64_t x;
    size_t b = neo_leb128_decode_i64(buf1, buf1 + sizeof(buf1), &x);
    ASSERT_EQ(10, a);
    ASSERT_EQ(a, b);
    ASSERT_EQ(x, INT64_MAX);
}

TEST(core, neo_leb128_encode_decode_i64_min)
{
    uint8_t buf1[10];
    size_t a = neo_leb128_encode_i64(buf1, buf1 + sizeof(buf1), INT64_MIN);
    int64_t x;
    size_t b = neo_leb128_decode_i64(buf1, buf1 + sizeof(buf1), &x);
    ASSERT_EQ(10, a);
    ASSERT_EQ(a, b);
    ASSERT_EQ(x, INT64_MIN);
}

TEST(core, neo_leb128_encode_decode_u64_max)
{
    uint8_t buf1[10];
    size_t a = neo_leb128_encode_u64(buf1, buf1 + sizeof(buf1), UINT64_MAX);
    uint64_t x;
    size_t b = neo_leb128_decode_u64(buf1, buf1 + sizeof(buf1), &x);
    ASSERT_EQ(10, a);
    ASSERT_EQ(a, b);
    ASSERT_EQ(x, UINT64_MAX);
}

TEST(core, neo_leb128_encode_decode_u64_min)
{
    uint8_t buf1[10];
    size_t a = neo_leb128_encode_u64(buf1, buf1 + sizeof(buf1), 0);
    uint64_t x;
    size_t b = neo_leb128_decode_u64(buf1, buf1 + sizeof(buf1), &x);
    ASSERT_EQ(1, a);
    ASSERT_EQ(a, b);
    ASSERT_EQ(x, 0);
}

TEST(core, valloc_r)
{
    int32_t *p = nullptr;
    ASSERT_TRUE(neo_valloc((void**)&p, sizeof(*p), NEO_PA_R, nullptr, neo_valloc_poison(0xaa)));
    neo_vheader_t *h = neo_vheader_of(p);
    ASSERT_EQ(sizeof(*p) + sizeof(*h), h->len);
    ASSERT_EQ(NEO_PA_R, h->access);
    ASSERT_NE(0, h->os_access);
    ASSERT_NE(nullptr, p);
    ASSERT_EQ(0xaaaaaaaa, *p); /* poison */
    ASSERT_TRUE(neo_vfree((void**)&p, false));
    ASSERT_EQ(nullptr, p);
}

TEST(core, valloc_rw)
{
    int32_t *p = nullptr;
    ASSERT_TRUE(neo_valloc((void**)&p, sizeof(*p), (neo_pageaccess_t)(NEO_PA_R | NEO_PA_W), nullptr, NEO_VALLOC_NOPOISON));
    neo_vheader_t *h = neo_vheader_of(p);
    ASSERT_EQ(sizeof(*p) + sizeof(*h), h->len);
    ASSERT_EQ((NEO_PA_R | NEO_PA_W), h->access);
    ASSERT_NE(0, h->os_access);
    ASSERT_NE(nullptr, p);
    *p = 10; /* Writeable */
    ASSERT_EQ(10, *p); /* Readable */
    ASSERT_TRUE(neo_vfree((void**)&p, false));
    ASSERT_EQ(nullptr, p);
}

TEST(core, valloc_rw_poison)
{
    int32_t *p = nullptr;
    ASSERT_TRUE(neo_valloc((void**)&p, sizeof(*p), (neo_pageaccess_t)(NEO_PA_R | NEO_PA_W), nullptr, neo_valloc_poison(0xfe)));
    neo_vheader_t *h = neo_vheader_of(p);
    ASSERT_EQ(sizeof(*p) + sizeof(*h), h->len);
    ASSERT_EQ((NEO_PA_R | NEO_PA_W), h->access);
    ASSERT_NE(0, h->os_access);
    ASSERT_NE(nullptr, p);
    ASSERT_EQ(0xfefefefe, *(uint32_t*)p); /* poison */
    *p = 10; /* Writeable */
    ASSERT_EQ(10, *p); /* Readable */
    ASSERT_TRUE(neo_vfree((void**)&p, false));
    ASSERT_EQ(nullptr, p);
}

TEST(core, valloc_vprotect)
{
    int32_t *p = nullptr;
    ASSERT_TRUE(neo_valloc((void**)&p, sizeof(*p), (neo_pageaccess_t)(NEO_PA_R | NEO_PA_W), nullptr, neo_valloc_poison(0xfe)));
    neo_vheader_t *h = neo_vheader_of(p);
    ASSERT_EQ(sizeof(*p) + sizeof(*h), h->len);
    ASSERT_EQ((NEO_PA_R | NEO_PA_W), h->access);
    ASSERT_NE(0, h->os_access);
    uint32_t os = h->os_access;
    ASSERT_TRUE(neo_vprotect(p, (neo_pageaccess_t)(NEO_PA_R | NEO_PA_W | NEO_PA_X)));
    ASSERT_EQ(sizeof(*p) + sizeof(*h), h->len);
    ASSERT_EQ((NEO_PA_R | NEO_PA_W | NEO_PA_X), h->access);
    ASSERT_NE(0, h->os_access);
    ASSERT_NE(os, h->os_access);
    ASSERT_TRUE(neo_vfree((void**)&p, false));
    ASSERT_EQ(nullptr, p);
}

#if NEO_CPU_AMD64
TEST(core, valloc_rwx)
{
    uint8_t *p = nullptr;
    ASSERT_TRUE(neo_valloc((void**)&p, 8+1, (neo_pageaccess_t)(NEO_PA_R | NEO_PA_W | NEO_PA_X), NULL, neo_valloc_poison(0xfe)));
    *(uint64_t*)p = UINT64_C(0xc300003039c0c748); /* little endian: movq $12345, %rax retq */
    p[sizeof(uint64_t)] = 0xcc; /* int3 */
    ASSERT_EQ(12345, neo_vmachine_exec(p, 0));
    ASSERT_TRUE(neo_vfree((void**)&p, false));
    ASSERT_EQ(nullptr, p);
}
#endif

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

TEST(core, neo_bswap32)
{
    uint32_t x = 0x78563412;
    uint32_t expected = 0x12345678;
    ASSERT_EQ(neo_bswap32(x), expected);

    x = 0xF0A1B2C3;
    expected = 0xC3B2A1F0;
    ASSERT_EQ(neo_bswap32(x), expected);
}

TEST(core, neo_bswap64)
{
    uint64_t x = 0x0123456789ABCDEF;
    uint64_t expected = 0xEFCDAB8967452301;
    ASSERT_EQ(neo_bswap64(x), expected);

    x = 0xFEDCBA9876543210;
    expected = 0x1032547698BADCFE;
    ASSERT_EQ(neo_bswap64(x), expected);
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

TEST(core, hash_x17_basic)
{
    const char *key = "hello";
    size_t len = strlen(key);
    uint32_t expected_hash = 0xc7c685ff;
    uint32_t actual_hash = neo_hash_x17(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_x17_empty)
{
    const char *key = "";
    size_t len = strlen(key);
    uint32_t expected_hash = 0x1505;
    uint32_t actual_hash = neo_hash_x17(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_x17_null_key)
{
    const char *key = NULL;
    size_t len = 0;
    uint32_t expected_hash = 0x1505;
    uint32_t actual_hash = neo_hash_x17(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_x17_long_key)
{
    const char *key = "This is a very long key that will test the hash function's ability to handle large inputs.";
    size_t len = strlen(key);
    uint32_t expected_hash = 0xdc20762;
    uint32_t actual_hash = neo_hash_x17(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_x17_equals)
{
    const char *key = "hello";
    size_t len = strlen(key);
    ASSERT_EQ(neo_hash_x17(key, len), neo_hash_x17(key, len));
}

TEST(core, hash_x17_not_equals)
{
    const char *key1 = "hello";
    size_t len1 = strlen(key1);
    const char *key2 = "hell";
    size_t len2 = strlen(key1);
    ASSERT_NE(neo_hash_x17(key1, len1), neo_hash_x17(key2, len2));
}

TEST(core, hash_bernstein_basic)
{
    const char *key = "hello";
    size_t len = strlen(key);
    uint32_t expected_hash = 0xf923099;
    uint32_t actual_hash = neo_hash_bernstein(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_bernstein_empty)
{
    const char *key = "";
    size_t len = strlen(key);
    uint32_t expected_hash = 0x1505;
    uint32_t actual_hash = neo_hash_bernstein(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_bernstein_null_key)
{
    const char *key = NULL;
    size_t len = 0;
    uint32_t expected_hash = 0x1505;
    uint32_t actual_hash = neo_hash_bernstein(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_bernstein_long_key)
{
    const char *key = "This is a very long key that will test the hash function's ability to handle large inputs.";
    size_t len = strlen(key);
    uint32_t expected_hash = 0x2f71a400;
    uint32_t actual_hash = neo_hash_bernstein(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_bernstein_equals)
{
    const char *key = "hello";
    size_t len = strlen(key);
    ASSERT_EQ(neo_hash_bernstein(key, len), neo_hash_bernstein(key, len));
}

TEST(core, hash_bernstein_not_equals)
{
    const char *key1 = "hello";
    size_t len1 = strlen(key1);
    const char *key2 = "hell";
    size_t len2 = strlen(key1);
    ASSERT_NE(neo_hash_bernstein(key1, len1), neo_hash_bernstein(key2, len2));
}

TEST(core, hash_fnv1a_basic)
{
    const char *key = "hello";
    size_t len = strlen(key);
    uint32_t actual_hash = neo_hash_fnv1a(key, len);
    ASSERT_NE(0, actual_hash);
}

TEST(core, hash_fnv1a_empty)
{
    const char *key = "";
    size_t len = strlen(key);
    uint32_t expected_hash = 0x811c9dc5;
    uint32_t actual_hash = neo_hash_fnv1a(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_fnv1a_null_key)
{
    const char *key = NULL;
    size_t len = 0;
    uint32_t expected_hash = 0x811c9dc5;
    uint32_t actual_hash = neo_hash_fnv1a(key, len);
    ASSERT_EQ(expected_hash, actual_hash);
}

TEST(core, hash_fnv1a_long_key)
{
    const char *key = "This is a very long key that will test the hash function's ability to handle large inputs.";
    size_t len = strlen(key);
    uint32_t actual_hash = neo_hash_fnv1a(key, len);
    ASSERT_NE(0, actual_hash);
}

TEST(core, hash_fnv1a_equals)
{
    const char *key = "hello";
    size_t len = strlen(key);
    ASSERT_EQ(neo_hash_fnv1a(key, len), neo_hash_fnv1a(key, len));
}

TEST(core, hash_fnv1a_not_equals)
{
    const char *key1 = "hello";
    size_t len1 = strlen(key1);
    const char *key2 = "hell";
    size_t len2 = strlen(key1);
    ASSERT_NE(neo_hash_fnv1a(key1, len1), neo_hash_fnv1a(key2, len2));
}
