// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_gc.h>
#include <cstring>

TEST(gc, gc_alloc_stack_ref) {
    std::array<std::uintptr_t, 8> stk {};
    stk.front() = UINT64_C(0xfe'fe'fe'fe'fe'fe'fe'fe);
    stk.back() = UINT64_C(0xbe'be'be'be'be'be'be'be);

    gc_context_t gc;
    gc_init(&gc, stk.data(), stk.data()+stk.size());

    static bool released = false;

    auto destructor = [](void *ptr) -> void {
        released = true;
    };
    gc.dtor_hook = +destructor;

    auto *ptr = static_cast<int64_t*>(gc_objalloc(&gc, sizeof(int64_t), GCF_NONE));
    ASSERT_EQ(released, false);
    ASSERT_EQ(*ptr, 0);
    ASSERT_EQ(*ptr, 0);
    ASSERT_NE(ptr, nullptr);
    *ptr = 10;
    ASSERT_EQ(*ptr, 10);

    gc_fatptr_t *fptr = gc_resolve_ptr(&gc, ptr);
    ASSERT_NE(fptr, nullptr);
    ASSERT_EQ(fptr->ptr, ptr);
    ASSERT_EQ(*static_cast<int64_t*>(fptr->ptr), 10);
    ASSERT_EQ(fptr->size, sizeof(int64_t));

    stk[2] = reinterpret_cast<std::uintptr_t>(ptr); // create artifical stack reference
    gc_collect(&gc);
    ASSERT_EQ(released, false); // should not be released, stack references value

    stk[2] = 0; // clear reference
    gc_collect(&gc);
    ASSERT_EQ(released, true); // should be released

    gc_free(&gc);
}

TEST(gc, gc_alloc_heap_ref) {
    std::array<std::uintptr_t, 8> stk {};
    stk.front() = UINT64_C(0xfe'fe'fe'fe'fe'fe'fe'fe);
    stk.back() = UINT64_C(0xbe'be'be'be'be'be'be'be);

    gc_context_t gc;
    gc_init(&gc, stk.data(), stk.data()+stk.size());

    static int free_count = 0;

    auto destructor1 = [](void *ptr) -> void {
        ++free_count;
    };
    gc.dtor_hook = +destructor1;

    struct dummy {
        neo_int_t data1;
        neo_float_t data2;
        void *my_ptr;
        neo_bool_t data3;
    };

    auto *root = static_cast<dummy*>(gc_objalloc(&gc, sizeof(dummy), GCF_ROOT));
    auto *ptr2 = static_cast<int64_t*>(gc_objalloc(&gc, sizeof(int64_t), GCF_NONE));
    ASSERT_EQ(free_count, 0);
    constexpr std::uint8_t zm[sizeof(dummy)] {};
    ASSERT_EQ(std::memcmp(root, zm, sizeof(dummy)), 0);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(ptr2, nullptr);

    gc_fatptr_t *fptr1 = gc_resolve_ptr(&gc, root);
    ASSERT_NE(fptr1, nullptr);
    ASSERT_EQ(fptr1->ptr, root);
    ASSERT_EQ(fptr1->size, sizeof(dummy));

    gc_fatptr_t *fptr2 = gc_resolve_ptr(&gc, ptr2);
    ASSERT_NE(fptr2, nullptr);
    ASSERT_EQ(fptr2->ptr, ptr2);
    ASSERT_EQ(fptr2->size, sizeof(int64_t));

    root->my_ptr = ptr2;
    gc_collect(&gc);
    ASSERT_EQ(free_count, 0); // should not be released1 -> is root

    root->my_ptr = nullptr; // clear reference
    gc_collect(&gc);
    ASSERT_EQ(free_count, 1); // should not be released1 -> is root

    // Root must be freed manually.
    gc_objfree(&gc, root);

    gc_free(&gc);
}

TEST(gc, gc_alloc_huge_2gb) {
    std::array<std::uintptr_t, 8> stk {};
    stk.front() = UINT64_C(0xfe'fe'fe'fe'fe'fe'fe'fe);
    stk.back() = UINT64_C(0xbe'be'be'be'be'be'be'be);

    gc_context_t gc;
    gc_init(&gc, stk.data(), stk.data()+stk.size());

    size_t len = 1024ull*1024ull*1024ull*2ull;
    void *mem = gc_objalloc(&gc, len, GCF_ROOT);
    memset(mem, 0xff, len);
    ASSERT_EQ(static_cast<uint8_t *>(mem)[0], 0xff);
    ASSERT_EQ(static_cast<uint8_t *>(mem)[22], 0xff);
    ASSERT_EQ(static_cast<uint8_t *>(mem)[len-1], 0xff);

    const gc_fatptr_t *fptr = gc_resolve_ptr(&gc, mem);
    ASSERT_EQ(fptr->flags, GCF_ROOT);
    ASSERT_EQ(fptr->size, len);
    ASSERT_NE(fptr->hash, 0);

    gc_objfree(&gc, mem);

    gc_free(&gc);
}
