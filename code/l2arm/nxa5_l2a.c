/*
 * nxa5_l2a.c - 256-NEA5 / 256-NIA5 / 256-NCA5 on the ARMv8 AES instructions (L2).
 * KSG: AES-256 in CTR mode with a 32-bit big-endian counter in IV bytes
 * 12..15 (TS 35.243 clause 6.3); eight blocks in flight.
 *
 * One AES round on ARM is AESE (AddRoundKey, SubBytes, ShiftRows) then
 * AESMC (MixColumns). So rounds 1..13 are AESMC(AESE(s, rk[r-1])), and the
 * last is AESE(s, rk[13]) XOR rk[14]. The key schedule also uses AESE, for
 * SubWord, so no table look-ups remain.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include <arm_neon.h>
#include <string.h>

typedef struct {
    uint8x16_t rk[15];
    uint8_t iv[16];
} aes_st;

/* SubWord with the AES instruction: with the word copied into all four
 * columns, ShiftRows changes nothing, so AESE with a zero key gives SubBytes. */
static inline uint32_t subword(uint32_t w)
{
    uint8x16_t v = vaeseq_u8(vreinterpretq_u8_u32(vdupq_n_u32(w)), vdupq_n_u8(0));
    return vgetq_lane_u32(vreinterpretq_u32_u8(v), 0);
}

/* AES-256 key expansion (FIPS 197, 5.2) on 32-bit little-endian words. */
static void aes_init(void *s, const uint8_t key[32], const uint8_t iv[16])
{
    static const uint8_t RCON[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
    aes_st *a = (aes_st *)s;
    uint32_t w[60];
    memcpy(w, key, 32);
    for (int i = 8; i < 60; i++) {
        uint32_t t = w[i - 1];
        if (i % 8 == 0) t = subword((t >> 8) | (t << 24)) ^ RCON[i / 8 - 1];   /* RotWord, SubWord, Rcon */
        else if (i % 8 == 4) t = subword(t);
        w[i] = w[i - 8] ^ t;
    }
    for (int r = 0; r < 15; r++) a->rk[r] = vreinterpretq_u8_u32(vld1q_u32(w + 4 * r));
    memset(w, 0, sizeof w);
    memcpy(a->iv, iv, 16);
    memset(a->iv + 12, 0, 4);
}

static inline uint8x16_t ctr_block(uint8x16_t base, uint32_t c)
{
    return vreinterpretq_u8_u32(vsetq_lane_u32(__builtin_bswap32(c), vreinterpretq_u32_u8(base), 3));
}

/* N counter blocks; N is a compile-time constant at each call. */
static inline __attribute__((always_inline))
void ctrN(const aes_st *a, uint8x16_t base, uint32_t ctr, uint8_t *out, const int N)
{
    uint8x16_t b[8];
#pragma GCC unroll 8
    for (int j = 0; j < N; j++) b[j] = ctr_block(base, ctr + (uint32_t)j);
#pragma GCC unroll 13
    for (int r = 0; r < 13; r++) {
        uint8x16_t k = a->rk[r];
#pragma GCC unroll 8
        for (int j = 0; j < N; j++) b[j] = vaesmcq_u8(vaeseq_u8(b[j], k));
    }
#pragma GCC unroll 8
    for (int j = 0; j < N; j++) vst1q_u8(out + 16 * j, veorq_u8(vaeseq_u8(b[j], a->rk[13]), a->rk[14]));
}

static void aes_hqp(void *s, uint8_t hqp[48])
{
    aes_st *a = (aes_st *)s;
    uint8_t iv[16];
    memcpy(iv, a->iv, 16);
    iv[0] |= 0x01;                                    /* AI = 1, counters 0, 1, 2 */
    ctrN(a, vld1q_u8(iv), 0, hqp, 3);
}

static void aes_blocks(void *s, uint8_t *out, size_t n)
{
    aes_st *a = (aes_st *)s;
    uint8x16_t base = vld1q_u8(a->iv);
    uint32_t ctr = ((uint32_t)a->iv[12] << 24) | ((uint32_t)a->iv[13] << 16) | ((uint32_t)a->iv[14] << 8) | a->iv[15];
    for (; n >= 8; n -= 8, ctr += 8, out += 128) ctrN(a, base, ctr, out, 8);
    if (n & 4) { ctrN(a, base, ctr, out, 4); ctr += 4; out += 64; }
    if (n & 2) { ctrN(a, base, ctr, out, 2); ctr += 2; out += 32; }
    if (n & 1) { ctrN(a, base, ctr, out, 1); ctr += 1; }
    a->iv[12] = (uint8_t)(ctr >> 24); a->iv[13] = (uint8_t)(ctr >> 16);
    a->iv[14] = (uint8_t)(ctr >> 8);  a->iv[15] = (uint8_t)ctr;
}

static const ksg2_ops KSG_AES = {aes_init, aes_hqp, aes_blocks};

void nea5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_AES, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_AES, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca5_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    aes_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG_AES, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
