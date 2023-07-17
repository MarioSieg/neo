/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_core.h"

void neo_assert_impl(const char *expr, const char *file, int line) {
    neo_panic("%s:%d <- Fatal internal NEO error! Assertion failed: '%s'", file, line, expr);
}

void neo_panic(const char *msg, ...) {
    fprintf(stderr, "%s", NEO_CCRED);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "%s", NEO_CCRESET);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
    neo_unreachable();
}
