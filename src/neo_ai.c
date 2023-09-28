/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */
/* AI core - tensors and math operations on tensors. */

#include "neo_ai.h"

#include <math.h>

f16_t f16_from_f32(float f) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)
    static const float scale_to_inf = 0x1.0p+112f;
    static const float scale_to_zero = 0x1.0p-110f;
#else
    static const uint32_t sti = 0x77800000;
    static const uint32_t stz = 0x08800000;
    const float scale_to_inf = (f32_punner_t){.u=sti}.f;
    const float scale_to_zero =(f32_punner_t){.u=stz}.f;
#endif
    float base = (fabsf(f) * scale_to_inf) * scale_to_zero;
    uint32_t w = (f32_punner_t){.f = base}.u;
    uint32_t shl1_w = w + w;
    uint32_t sign = w & 0x80000000u;
    uint32_t bias = shl1_w & 0xff000000u;
    bias = bias < 0x71000000u ? 0x71000000u : bias;
    uint32_t bsi = (bias >> 1) + 0x07800000u;
    base = (f32_punner_t){.u=bsi}.f + base;
    uint32_t bits = (f32_punner_t){.f=base}.u;
    uint32_t exp_bits = (bits >> 13) & 0x00007c00u;
    uint32_t mantissa_bits = bits & 0x00000fffu;
    uint32_t nonsign = exp_bits + mantissa_bits;
    return (f16_t)((sign >> 16) | (shl1_w > 0xff000000u ? 0x7e00 : nonsign));
}

float f16_to_f32(f16_t h) {
    uint32_t w = (uint32_t)h << 16;
    uint32_t sign = w & 0x80000000u;
    uint32_t two_w = w + w;
    uint32_t exp_offset = 0xe0u << 23;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)
    static const float exp_scale = 0x1.0p-112f;
#else
    static const uint32_t es = 0x7800000;
    const float exp_scale = (f32_punner_t){.u=es}.f;
#endif
    uint32_t norm = (two_w >> 4) + exp_offset;
    float normalized_value = (f32_punner_t){.u=norm}.f * exp_scale;
    uint32_t magic_mask = UINT32_C(126) << 23;
    static const float magic_bias = 0.5f;
    uint32_t denorm = (two_w >> 17) | magic_mask;
    float denormalized_value = (f32_punner_t){.u=denorm}.f - magic_bias;
    uint32_t denormalized_cutoff = 1u << 27;
    uint32_t result = sign
        | (two_w < denormalized_cutoff
        ? (f32_punner_t){.f=denormalized_value}.u
        : (f32_punner_t){.f=normalized_value}.u);
    return (f32_punner_t){.u=result}.f;
}

#define GELU_COEF_A 0.044715f
#define GELU_QUICK_COEF (-1.702f)
#define SQRT_2_OVER_PI 0.79788456080286535587989211986876f

static f16_t lut_gelu_f16[1 << 16]; /* Precomputed gelu table for f16 (128 KB). */
static f16_t lut_gelu_quick_f16[1 << 16]; /* Precomputed quick gelu table for f16 (128 KB). */
static f16_t lut_silu_f16[1 << 16]; /* Precomputed silu table for f16 (128 KB). */
static f16_t lut_exp_f16[1 << 16]; /* Precomputed exp table for f16 (128 KB). */
static float lut_f32_f16[1 << 16]; /* Precomputed f32 table for f16 (256 KB). */

#define gelu_f32(x) (0.5f*(x)*(1.0f + tanhf(SQRT_2_OVER_PI*(x)*(1.0f+GELU_COEF_A*(x)*(x))))) /* Gaussian Error Linear Unit (GELU) function. */
#define gelu_f32_fast(x) ((x)*(1.0f/(1.0f+expf(GELU_QUICK_COEF*(x))))) /* Approx Gaussian Error Linear Unit (GELU) function. */
#define silu_f32(x) ((x)/(1.0f+expf(-(x)))) /* Sigmoid Linear Unit (SiLU) function. */

#ifdef GELU_F16
inline static void vec_gelu_f32(size_t n, float *y, const float *x) {
    for (size_t i = 0; i < n; ++i) {
        f16_t fp16 = f16_from_f32(x[i]);
        y[i] = f16_to_f32(lut_gelu_f16[*(uint16_t *)&fp16]);
    }
}
#else
inline static void vec_gelu_f32(size_t n, float *y, const float *x) {
    for (size_t i = 0; i < n; ++i) {
        y[i] = gelu_f32(x[i]);
    }
}
#endif

#ifdef GELU_F16_FAST
inline static void ggml_vec_gelu_quick_f32(size_t n, float *y, const float *x) {
    for (size_t i = 0; i < n; ++i) {
        f16_t fp16 = f16_from_f32(x[i]);
        y[i] = f16_to_f32(lut_gelu_quick_f16[*(uint16_t *)&fp16]);
    }
}
#else
inline static void ggml_vec_gelu_quick_f32(size_t n, float *y, const float *x) {
    for (size_t i = 0; i < n; ++i) {
        y[i] = gelu_f32_fast(x[i]);
    }
}
#endif

void precompute_luts(void) {
    for (size_t i = 0; i < 1 << 16; ++i) {
        uint16_t j = (uint16_t)i;
        float f = lut_f32_f16[i] = f16_to_f32(*(f16_t *)&j);
        lut_gelu_f16[i] = f16_from_f32(gelu_f32(f));
        lut_gelu_quick_f16[i] = f16_from_f32(gelu_f32_fast(f));
        lut_silu_f16[i] = f16_from_f32(silu_f32(f));
        lut_exp_f16[i] = f16_from_f32(expf(f));
    }
}
