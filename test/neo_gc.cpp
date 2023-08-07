// (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#include <gtest/gtest.h>
#include <neo_gc.h>

TEST(gc, gc_alloc) {
    std::array<std::uintptr_t, 128> stk {};

    gc_context_t gc;
    gc_init(&gc, stk.data(), stk.data()+stk.size());

    static bool released = false;

    auto destructor = [](void *ptr) -> void {
        released = true;
    };

    auto *ptr = static_cast<int*>(gc_vmalloc(&gc, sizeof(int), +destructor));
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(*ptr, 0);
    *ptr = 10;
    ASSERT_EQ(*ptr, 10);

    gc_fatptr_t *fptr = gc_resolve_ptr(&gc, ptr);
    ASSERT_NE(fptr, nullptr);
    ASSERT_EQ(fptr->ptr, ptr);
    ASSERT_EQ(*static_cast<int*>(fptr->ptr), 10);
    ASSERT_EQ(fptr->size, sizeof(int));
    ASSERT_EQ(fptr->dtor, destructor);

    gc_free(&gc);
}
