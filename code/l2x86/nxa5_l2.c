/*
 * nxa5_l2.c - 256-NEA5 / 256-NIA5 / 256-NCA5 on AES-NI (L2, x86-64).
 * KSG: AES-256 in CTR mode with a 32-bit big-endian counter in IV bytes
 * 12..15 (TS 35.243 clause 6.3); eight blocks in flight.
 */
#include "ksg_l2.h"
#include "nxa_l2.h"
#include <immintrin.h>
#include <string.h>


static inline __m128i exp_a(__m128i k, __m128i assist)
{
    assist = _mm_shuffle_epi32(assist, 0xff);
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    return _mm_xor_si128(k, assist);
}

static inline __m128i exp_b(__m128i k, __m128i prev)
{
    __m128i t = _mm_shuffle_epi32(_mm_aeskeygenassist_si128(prev, 0x00), 0xaa);
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    k = _mm_xor_si128(k, _mm_slli_si128(k, 4));
    return _mm_xor_si128(k, t);
}

static void aes_init(void *s, const uint8_t key[32], const uint8_t iv[16])
{
    aes_st *a = (aes_st *)s;
    __m128i *rk = a->rk;
    rk[0] = _mm_loadu_si128((const __m128i *)key);
    rk[1] = _mm_loadu_si128((const __m128i *)(key + 16));
#define STEP(i, rcon) \
    rk[i] = exp_a(rk[i - 2], _mm_aeskeygenassist_si128(rk[i - 1], rcon)); \
    if ((i) < 14) rk[(i) + 1] = exp_b(rk[i - 1], rk[i]);
    STEP(2, 0x01) STEP(4, 0x02) STEP(6, 0x04) STEP(8, 0x08) STEP(10, 0x10) STEP(12, 0x20) STEP(14, 0x40)
#undef STEP
    memcpy(a->iv, iv, 16);
    memset(a->iv + 12, 0, 4);
}

/* Encrypt N counter blocks (IV with counter ctr, ctr+1, ...). N is a
 * compile-time constant at every call, so the blocks stay in registers. */
static inline __attribute__((always_inline))
void ctrN(const aes_st *a, __m128i base, uint32_t ctr, uint8_t *out, const int N)
{
    __m128i b[8];
#pragma GCC unroll 8
    for (int j = 0; j < N; j++)
        b[j] = _mm_xor_si128(_mm_insert_epi32(base, (int)__builtin_bswap32(ctr + (uint32_t)j), 3), a->rk[0]);
#pragma GCC unroll 13
    for (int r = 1; r < 14; r++) {
        __m128i k = a->rk[r];
#pragma GCC unroll 8
        for (int j = 0; j < N; j++) b[j] = _mm_aesenc_si128(b[j], k);
    }
#pragma GCC unroll 8
    for (int j = 0; j < N; j++) _mm_storeu_si128((__m128i *)(out + 16 * j), _mm_aesenclast_si128(b[j], a->rk[14]));
}

static void aes_hqp(void *s, uint8_t hqp[48])
{
    aes_st *a = (aes_st *)s;
    uint8_t iv[16];
    memcpy(iv, a->iv, 16);
    iv[0] |= 0x01;                                    /* AI = 1, counters 0, 1, 2 */
    ctrN(a, _mm_loadu_si128((const __m128i *)iv), 0, hqp, 3);
    /* The keystream then starts again from counter 0 with AI = 0 (a->iv unchanged). */
}

/* Eight full blocks with constant trip counts, so all eight stay in registers. */
static inline void ctr8_full(const aes_st *a, __m128i base, uint32_t ctr, uint8_t *out)
{
    const __m128i *rk = a->rk;
    __m128i b0 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 0), 3);
    __m128i b1 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 1), 3);
    __m128i b2 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 2), 3);
    __m128i b3 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 3), 3);
    __m128i b4 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 4), 3);
    __m128i b5 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 5), 3);
    __m128i b6 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 6), 3);
    __m128i b7 = _mm_insert_epi32(base, (int)__builtin_bswap32(ctr + 7), 3);
    __m128i k = rk[0];
    b0 = _mm_xor_si128(b0, k); b1 = _mm_xor_si128(b1, k); b2 = _mm_xor_si128(b2, k); b3 = _mm_xor_si128(b3, k);
    b4 = _mm_xor_si128(b4, k); b5 = _mm_xor_si128(b5, k); b6 = _mm_xor_si128(b6, k); b7 = _mm_xor_si128(b7, k);
#pragma GCC unroll 13
    for (int r = 1; r < 14; r++) {
        k = rk[r];
        b0 = _mm_aesenc_si128(b0, k); b1 = _mm_aesenc_si128(b1, k); b2 = _mm_aesenc_si128(b2, k); b3 = _mm_aesenc_si128(b3, k);
        b4 = _mm_aesenc_si128(b4, k); b5 = _mm_aesenc_si128(b5, k); b6 = _mm_aesenc_si128(b6, k); b7 = _mm_aesenc_si128(b7, k);
    }
    k = rk[14];
    _mm_storeu_si128((__m128i *)(out +   0), _mm_aesenclast_si128(b0, k));
    _mm_storeu_si128((__m128i *)(out +  16), _mm_aesenclast_si128(b1, k));
    _mm_storeu_si128((__m128i *)(out +  32), _mm_aesenclast_si128(b2, k));
    _mm_storeu_si128((__m128i *)(out +  48), _mm_aesenclast_si128(b3, k));
    _mm_storeu_si128((__m128i *)(out +  64), _mm_aesenclast_si128(b4, k));
    _mm_storeu_si128((__m128i *)(out +  80), _mm_aesenclast_si128(b5, k));
    _mm_storeu_si128((__m128i *)(out +  96), _mm_aesenclast_si128(b6, k));
    _mm_storeu_si128((__m128i *)(out + 112), _mm_aesenclast_si128(b7, k));
}

static void aes_blocks(void *s, uint8_t *out, size_t n)
{
    aes_st *a = (aes_st *)s;
    __m128i base = _mm_loadu_si128((const __m128i *)a->iv);
    uint32_t ctr = ((uint32_t)a->iv[12] << 24) | ((uint32_t)a->iv[13] << 16) | ((uint32_t)a->iv[14] << 8) | a->iv[15];
    for (; n >= 8; n -= 8, ctr += 8, out += 128) ctr8_full(a, base, ctr, out);
    if (n & 4) { ctrN(a, base, ctr, out, 4); ctr += 4; out += 64; }
    if (n & 2) { ctrN(a, base, ctr, out, 2); ctr += 2; out += 32; }
    if (n & 1) { ctrN(a, base, ctr, out, 1); ctr += 1; }
    a->iv[12] = (uint8_t)(ctr >> 24); a->iv[13] = (uint8_t)(ctr >> 16);
    a->iv[14] = (uint8_t)(ctr >> 8);  a->iv[15] = (uint8_t)ctr;
}

const ksg2_ops KSG2_AES = {aes_init, aes_hqp, aes_blocks};

void nea5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_AES, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_AES, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca5_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG2_AES, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
