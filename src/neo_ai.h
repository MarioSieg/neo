/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AI core - tensors and math operations on tensors. */

#ifndef NEO_AI_H
#define NEO_AI_H

#include "neo_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t f16_t;
typedef union f32_punner_t {
    uint32_t u;
    float f;
} f32_punner_t;
neo_static_assert(sizeof(f16_t) == 2);
neo_static_assert(sizeof(float) == 4);
neo_static_assert(sizeof(float) == sizeof(f32_punner_t));

/*
** Some abbreviations:
** GELU = (Gaussian Error Linear Unit) -> GELU is an activation function commonly used in deep learning. It is defined as a smooth approximation of the rectifier linear unit (ReLU) activation function.
** RELU = (Rectified Linear Unit) -> The rectified linear activation function or ReLU for short is a piecewise linear function that will output the input directly if is positive, otherwise, it will output zero.
** TANH = (Hyperbolic Tangent) -> The hyperbolic tangent, also known as tanh, is a hyperbolic function used in some neural networks. It is very similar to the sigmoid activation function: its output is bound between -1 and 1, instead of 0 and 1 for the sigmoid.
** SIGM = (Sigmoid) -> The sigmoid function, also called logistic function gives an ‘S’ shaped curve that can take any real-valued number and map it into a value between 0 and 1.
 */


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

#define TS_MAX_DIMS 4 /* 4D tensors are the maximum. */
#define TS_MAX_SRC 8 /* Maximum number of source tensors. */
#define TS_MAX_PARAMS 32 /* Maximum number of parameters. */

/* Represents an N-dimensional tensor. */
typedef struct tensor_t tensor_t;
struct tensor_t {
    tensor_type_t type : 8; /* Type of the tensor. */
    compute_device_t device : 8; /* Device where the tensor is located. */
    uint8_t num_dims; /* Number of dimensions. */
    uint64_t elemts[TS_MAX_DIMS]; /* Number of elements in each dimension. */
    void *data; /* Pointer to the data. */
    uint8_t params[TS_MAX_PARAMS]; /* Parameters of the tensor. */
    bool is_param; /* Is this tensor a parameter? */
    struct tensor_t *grad; /* Gradient tensor. */
    struct tensor_t *src[TS_MAX_SRC]; /* Source tensors. */
    struct tensor_t *view_src; /* Source tensor of a view. */
    size_t view_off; /* Offset in bytes of the view. */
    void *usr; /* User data. */
    uint8_t name[64]; /* Name of the tensor. */
    struct {
        uint64_t runs;
        uint64_t cycles;
        uint64_t cycle_us;
    } perf; /* Performance data. */
};

#define GELU_F16
#define GELU_F16_FAST

/* We store multiple lookup tables internally, precompute the values once at boot. Should be called lazily, if the AI module is loaded. */
extern NEO_EXPORT void precompute_luts(void);

#ifdef __cplusplus
}
#endif
#endif
