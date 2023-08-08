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

    auto *ptr = static_cast<int*>(gc_vmalloc(&gc,sizeof(int), +destructor));
    ASSERT_EQ(released, false);
    ASSERT_EQ(*ptr, 0);
    ASSERT_EQ(*ptr, 0);
    ASSERT_NE(ptr, nullptr);
    *ptr = 10;
    ASSERT_EQ(*ptr, 10);

    gc_fatptr_t *fptr = gc_resolve_ptr(&gc, ptr);
    ASSERT_NE(fptr, nullptr);
    ASSERT_EQ(fptr->ptr, ptr);
    ASSERT_EQ(*static_cast<int*>(fptr->ptr), 10);
    ASSERT_EQ(fptr->size, sizeof(int));
    ASSERT_EQ(fptr->dtor, destructor);

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

    static bool released1 = false;
    static bool released2 = false;

    auto destructor1 = [](void *ptr) -> void {
        released1 = true;
    };

    auto destructor2 = [](void *ptr) -> void {
        released2 = true;
    };

    struct dummy {
        neo_int_t data1;
        neo_float_t data2;
        void *my_ptr;
        neo_bool_t data3;
    };

    auto *ptr1 = static_cast<dummy*>(gc_vmalloc_root(&gc, sizeof(dummy), +destructor1));
    auto *ptr2 = static_cast<int*>(gc_vmalloc(&gc, sizeof(int), +destructor2));
    ASSERT_EQ(released1, false);
    ASSERT_EQ(released2, false);
    constexpr std::uint8_t zm[sizeof(dummy)] {};
    ASSERT_EQ(std::memcmp(ptr1, zm, sizeof(dummy)), 0);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    gc_fatptr_t *fptr1 = gc_resolve_ptr(&gc, ptr1);
    ASSERT_NE(fptr1, nullptr);
    ASSERT_EQ(fptr1->ptr, ptr1);
    ASSERT_EQ(fptr1->size, sizeof(dummy));
    ASSERT_EQ(fptr1->dtor, destructor1);

    gc_fatptr_t *fptr2 = gc_resolve_ptr(&gc, ptr2);
    ASSERT_NE(fptr2, nullptr);
    ASSERT_EQ(fptr2->ptr, ptr2);
    ASSERT_EQ(fptr2->size, sizeof(int));
    ASSERT_EQ(fptr2->dtor, destructor2);

    ptr1->my_ptr = ptr2;
    gc_collect(&gc);
    ASSERT_EQ(released1, false); // should not be released1 -> is root
    ASSERT_EQ(released2, false); // should not be released2, heap references value

    ptr1->my_ptr = nullptr; // clear reference
    gc_collect(&gc);
    ASSERT_EQ(released1, false); // 1 is root
    ASSERT_EQ(released2, true); // should be released1

    gc_free(&gc);
}
