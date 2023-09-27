/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AI core - tensors and math operations on tensors. */

#ifndef NEO_AST_H
#define NEO_AST_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t f16_t;

neo_static_assert(sizeof(f16_t) == 2);
neo_static_assert(sizeof(float) == 4);

extern NEO_EXPORT f16_t f16_from_f32(float f);
extern NEO_EXPORT float f16_to_f32(f16_t f);

typedef enum tensor_type_t {
    /* Floating-Point formats. */
    TT_F32, /* 32-bit single precision. */
    TT_F16, /* 16-bit half precision. */
    /* Integer Formats. */
    TT_I8,
    TT_I16,
    TT_I32,
    /* Quantizations */
    TT_Q4_0,
    TT_Q4_1,
    TT_Q5_0,
    TT_Q5_1,
    TT_Q8_0,
    TT_Q8_1,
    /* K-Quantizations */
    TT_Q2_K,
    TT_Q3_K,
    TT_Q4_K,
    TT_Q5_K,
    TT_Q6_K,
    TT_Q8_K,

    TT__LEN
} tensor_type_t;
neo_static_assert(TT__LEN <= 255);

typedef enum compute_device_t {
    CD_CPU,
    CD_GPU,
    CD_TPU,

    CD__LEN
} compute_device_t;
neo_static_assert(CD__LEN <= 255);

/* Represents an N-dimensional tensor. */
typedef struct tensor_t tensor_t;
struct tensor_t {
    tensor_type_t type : 8; /* Type of the tensor. */
    compute_device_t device : 8; /* Device where the tensor is located. */
    uint32_t num_dims; /* Number of dimensions. */
    void *data; /* Pointer to the data. */
    size_t len; /* In bytes. */
    void *usr; /* User data. */
    uint8_t name[64]; /* Name of the tensor. */
    struct {
        uint64_t runs;
        uint64_t cycles;
        uint64_t cycle_ns;
    } perf; /* Performance data. */
};

#ifdef __cplusplus
}
#endif
#endif
