/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#ifndef NEO_CORE_H
#define NEO_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEO_ENUM_SEP ,

#define neo_assert_name2(name, line) name ## line
#define neo_assert_name(line) neo_assert_name2(_assert_, line)
#define neo_static_assert(expr) extern void neo_assert_name(__LINE__)(bool STATIC_ASSERTION_FAILED[((expr)?1:-1)])

#ifdef __cplusplus
}
#endif
#endif
