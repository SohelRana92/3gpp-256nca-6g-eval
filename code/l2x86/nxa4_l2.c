/*
 * nxa4_l2.c - 256-NEA4 / 256-NIA4 / 256-NCA4: Snow 5G on SSSE3 + AES-NI (L2).
 *
 * Each LFSR (sixteen 16-bit cells) sits in two registers: lo = cells 0..7,
 * hi = cells 8..15, cell i in 16-bit lane i. Eight single clocks of the L1
 * code give, from the old state,
 *   new_a[j] = b[j] ^ a[7+j] ^ alpha(a[j]),  new_b[j] = a[j] ^ b[8+j] ^ beta(b[j]),
 * for j = 0..7, so one clock of the cipher updates all eight lanes at once.
 * The AES round with a zero key is one AESENC; byte k of a register is AES
 * state byte k (TS 35.240 figure 6.2.3-3).
 */
#include "ksg_l2.h"
#include "nxa_l2.h"
#include <immintrin.h>

typedef struct { __m128i alo, ahi, blo, bhi, r1, r2, r3; } snow_st;

static inline __m128i mulx(__m128i v, __m128i poly)          /* times x in GF(2^16), per lane */
{
    return _mm_xor_si128(_mm_slli_epi16(v, 1), _mm_and_si128(_mm_srai_epi16(v, 15), poly));
}

static inline __m128i clock1(snow_st *s)
{
    const __m128i alpha = _mm_set1_epi16(0x4a6d), beta = _mm_set1_epi16((short)0xcc87);
    const __m128i sigma = _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
    const __m128i zero = _mm_setzero_si128();

    __m128i z = _mm_xor_si128(_mm_add_epi16(s->r1, s->bhi), s->r2);        /* (R1 +16 T1) ^ R2 */

    __m128i tmp = _mm_add_epi16(s->r2, _mm_xor_si128(s->r3, s->ahi));     /* R2 +16 (R3 ^ T2) */
    s->r3 = _mm_aesenc_si128(s->r2, zero);
    s->r2 = _mm_aesenc_si128(s->r1, zero);
    s->r1 = _mm_shuffle_epi8(tmp, sigma);

    __m128i na = _mm_xor_si128(_mm_xor_si128(s->blo, _mm_alignr_epi8(s->ahi, s->alo, 14)), mulx(s->alo, alpha));
    __m128i nb = _mm_xor_si128(_mm_xor_si128(s->alo, s->bhi), mulx(s->blo, beta));
    s->alo = s->ahi; s->ahi = na;
    s->blo = s->bhi; s->bhi = nb;
    return z;
}

static void snow_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    snow_st *s = (snow_st *)p;
    __m128i k0 = _mm_loadu_si128((const __m128i *)key), k1 = _mm_loadu_si128((const __m128i *)(key + 16));
    s->ahi = k0;                                      /* a8..a15 = k0..k7  */
    s->alo = _mm_loadu_si128((const __m128i *)iv);    /* a0..a7  = iv0..iv7 */
    s->bhi = k1;                                      /* b8..b15 = k8..k15 */
    s->blo = s->r1 = s->r2 = s->r3 = _mm_setzero_si128();
    for (int t = 1; t <= 16; t++) {
        __m128i z = clock1(s);
        s->ahi = _mm_xor_si128(s->ahi, z);
        if (t == 15) s->r1 = _mm_xor_si128(s->r1, k0);
        if (t == 16) s->r1 = _mm_xor_si128(s->r1, k1);
    }
}

static void snow_hqp(void *p, uint8_t hqp[48])
{
    snow_st *s = (snow_st *)p;
    for (int i = 0; i < 3; i++) _mm_storeu_si128((__m128i *)(hqp + 16 * i), clock1(s));
}

static void snow_blocks(void *p, uint8_t *out, size_t n)
{
    snow_st s = *(snow_st *)p;                        /* keep the state in registers */
    for (size_t i = 0; i < n; i++) _mm_storeu_si128((__m128i *)(out + 16 * i), clock1(&s));
    *(snow_st *)p = s;
}

const ksg2_ops KSG2_SNOW = {snow_init, snow_hqp, snow_blocks};

void nea4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_SNOW, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_SNOW, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca4_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG2_SNOW, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
