/*
 * zuc_sbox_ct.h - Constant-time ZUC S-box layer for the L2/L3 code.
 *
 * sbox(x) = S0[x31..24] || S1[x23..16] || S0[x15..8] || S1[x7..0].
 * The table L2 code reads S0 and S1 from memory at key-dependent addresses,
 * and the dudect test shows that this leaks. Here every look-up is a register
 * shuffle over the whole 256-byte table, so neither the instruction stream nor
 * the memory addresses depend on x:
 *   x86-64 (AVX-512 VBMI): VPERMI2B selects from 128 bytes; two of them cover
 *                          256 bytes, and bit 7 of each index picks the half.
 *   AArch64 (NEON):        TBL selects from 64 bytes and returns 0 outside;
 *                          four of them, with the index lowered by 64 each
 *                          time, cover 256 bytes, and the results are ORed.
 * Both S0 and S1 are looked up for all four bytes; a constant byte mask then
 * keeps S0 in bytes 3 and 1 and S1 in bytes 2 and 0.
 */
#ifndef R4_ZUC_SBOX_CT_H
#define R4_ZUC_SBOX_CT_H
#include <stdint.h>
#include "../zuc256set/zuc_sbox.h"

#if defined(__x86_64__)
#include <immintrin.h>
#if !defined(__AVX512VBMI__) || !defined(__AVX512BW__)
#error "zuc_sbox_ct.h needs -mavx512f -mavx512bw -mavx512vbmi"
#endif
static inline __attribute__((always_inline)) __m512i lut256(__m512i idx, const uint8_t *t)
{
    __m512i lo = _mm512_permutex2var_epi8(_mm512_loadu_si512(t), idx, _mm512_loadu_si512(t + 64));
    __m512i hi = _mm512_permutex2var_epi8(_mm512_loadu_si512(t + 128), idx, _mm512_loadu_si512(t + 192));
    return _mm512_mask_blend_epi8(_mm512_movepi8_mask(idx), lo, hi);     /* bit 7 set -> upper half */
}
static inline __attribute__((always_inline)) uint32_t zuc_sbox_ct(uint32_t x)
{
    __m512i idx = _mm512_castsi128_si512(_mm_cvtsi32_si128((int)x));
    __m512i s0 = lut256(idx, ZUC_S0), s1 = lut256(idx, ZUC_S1);
    __m512i r = _mm512_mask_blend_epi8(0xA, s1, s0);                    /* bytes 1 and 3 from S0 */
    return (uint32_t)_mm_cvtsi128_si32(_mm512_castsi512_si128(r));
}

#elif defined(__aarch64__)
#include <arm_neon.h>
static inline __attribute__((always_inline)) uint8x16_t lut256(uint8x16_t idx, const uint8_t *t)
{
    const uint8x16_t k64 = vdupq_n_u8(64);
    uint8x16x4_t a = vld1q_u8_x4(t), b = vld1q_u8_x4(t + 64), c = vld1q_u8_x4(t + 128), d = vld1q_u8_x4(t + 192);
    uint8x16_t i1 = vsubq_u8(idx, k64), i2 = vsubq_u8(i1, k64), i3 = vsubq_u8(i2, k64);
    return vorrq_u8(vorrq_u8(vqtbl4q_u8(a, idx), vqtbl4q_u8(b, i1)), vorrq_u8(vqtbl4q_u8(c, i2), vqtbl4q_u8(d, i3)));
}
static inline __attribute__((always_inline)) uint32_t zuc_sbox_ct(uint32_t x)
{
    static const uint8_t m[16] = {0, 0xFF, 0, 0xFF};                    /* bytes 1 and 3 from S0 */
    uint8x16_t idx = vreinterpretq_u8_u32(vsetq_lane_u32(x, vdupq_n_u32(0), 0));
    uint8x16_t r = vbslq_u8(vld1q_u8(m), lut256(idx, ZUC_S0), lut256(idx, ZUC_S1));
    return vgetq_lane_u32(vreinterpretq_u32_u8(r), 0);
}

#else
#error "zuc_sbox_ct.h: no constant-time S-box for this architecture"
#endif
#endif
