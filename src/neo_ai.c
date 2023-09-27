/* (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com */

#include "neo_ai.h"

f16_t f16_from_f32(float f) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)
    static const float scale_to_inf = 0x1.0p+112f;
    static const float scale_to_zero = 0x1.0p-110f;
#else
    static const uint32_t sti = 0x77800000;
    static const uint32_t stz = 0x08800000;
    const float scale_to_inf = *(float *)&sti;
    const float scale_to_zero = *(float *)&stz;
#endif
    float base = (fabsf(f) * scale_to_inf) * scale_to_zero;
    uint32_t w = *(uint32_t *)&f;
    uint32_t shl1_w = w + w;
    uint32_t sign = w & 0x80000000u;
    uint32_t bias = shl1_w & 0xff000000u;
    bias = bias < 0x71000000u ? 0x71000000u : bias;
    uint32_t bsi = (bias >> 1) + 0x07800000u;
    base = *(float *)&bsi + base;
    uint32_t bits = *(uint32_t *)&base;
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
    const float exp_scale = *(float *)&es;
#endif
    uint32_t norm = (two_w >> 4) + exp_offset;
    float normalized_value = *(float *)&norm * exp_scale;
    uint32_t magic_mask = UINT32_C(126) << 23;
    static const float magic_bias = 0.5f;
    uint32_t denorm = (two_w >> 17) | magic_mask;
    float denormalized_value = *(float *)&denorm - magic_bias;
    uint32_t denormalized_cutoff = 1u << 27;
    uint32_t result = sign
        | (two_w < denormalized_cutoff
        ? *(uint32_t *)&denormalized_value
        : *(uint32_t *)&normalized_value);
    return *(float *)&result;
}
