/*
 * nxa5_l2p.c - 256-NEA5 / 256-NIA5 / 256-NCA5, portable L2 for 32-bit CPUs.
 * KSG: AES-256 in CTR mode with a 32-bit big-endian counter in IV bytes
 * 12..15 (TS 35.243 clause 6.3). AES uses four 1 KiB T-tables in RAM, so a
 * round costs 16 look-ups and 16 XORs on 32-bit words.
 *
 * Like L1, table look-ups depend on secret data. On CPUs with a data cache
 * this can leak timing; Section "Constant-time testing" covers this.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include "tab_l2p.h"
#include <string.h>

typedef struct {
    uint32_t rk[60];      /* round keys as little-endian column words */
    uint32_t iv[3];       /* IV bytes 0..11, already XORed with rk[0..2] */
    uint32_t ctr;         /* next counter value (IV bytes 12..15, big-endian) */
} aes_st;

static inline uint32_t subword(uint32_t w)
{
    return (uint32_t)L2P_SBOX[w & 0xFF] | ((uint32_t)L2P_SBOX[(w >> 8) & 0xFF] << 8)
         | ((uint32_t)L2P_SBOX[(w >> 16) & 0xFF] << 16) | ((uint32_t)L2P_SBOX[w >> 24] << 24);
}

/* AES-256 key expansion (FIPS 197, 5.2) on little-endian words. */
static void aes_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    static const uint8_t RCON[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
    aes_st *a = (aes_st *)p;
    uint32_t *w = a->rk;
    for (int i = 0; i < 8; i++) w[i] = l2p_ld(key + 4 * i);
    for (int i = 8; i < 60; i++) {
        uint32_t t = w[i - 1];
        if (i % 8 == 0) t = subword((t >> 8) | (t << 24)) ^ RCON[i / 8 - 1];   /* RotWord, SubWord, Rcon */
        else if (i % 8 == 4) t = subword(t);
        w[i] = w[i - 8] ^ t;
    }
    for (int i = 0; i < 3; i++) a->iv[i] = l2p_ld(iv + 4 * i) ^ w[i];
    a->ctr = 0;
}

static void aes_ctr(aes_st *a, uint8_t *out, size_t n)
{
    const uint32_t *rk = a->rk;
    uint32_t c = a->ctr;
    for (size_t b = 0; b < n; b++, c++, out += 16) {
        uint32_t s0 = a->iv[0], s1 = a->iv[1], s2 = a->iv[2];
        uint32_t s3 = ((c >> 24) | ((c >> 8) & 0xFF00) | ((c << 8) & 0xFF0000) | (c << 24)) ^ rk[3];
        uint32_t t0, t1, t2, t3;
        for (int r = 1; r < 13; r += 2) {
            L2P_ROUND(t0, t1, t2, t3, s0, s1, s2, s3);
            t0 ^= rk[4 * r]; t1 ^= rk[4 * r + 1]; t2 ^= rk[4 * r + 2]; t3 ^= rk[4 * r + 3];
            L2P_ROUND(s0, s1, s2, s3, t0, t1, t2, t3);
            s0 ^= rk[4 * r + 4]; s1 ^= rk[4 * r + 5]; s2 ^= rk[4 * r + 6]; s3 ^= rk[4 * r + 7];
        }
        L2P_ROUND(t0, t1, t2, t3, s0, s1, s2, s3);                       /* round 13 */
        t0 ^= rk[52]; t1 ^= rk[53]; t2 ^= rk[54]; t3 ^= rk[55];
#define LAST(x0, x1, x2, x3) ((uint32_t)L2P_SBOX[(x0) & 0xFF] | ((uint32_t)L2P_SBOX[((x1) >> 8) & 0xFF] << 8) \
                            | ((uint32_t)L2P_SBOX[((x2) >> 16) & 0xFF] << 16) | ((uint32_t)L2P_SBOX[(x3) >> 24] << 24))
        l2p_st(out,      LAST(t0, t1, t2, t3) ^ rk[56]);                 /* round 14: no MixColumns */
        l2p_st(out + 4,  LAST(t1, t2, t3, t0) ^ rk[57]);
        l2p_st(out + 8,  LAST(t2, t3, t0, t1) ^ rk[58]);
        l2p_st(out + 12, LAST(t3, t0, t1, t2) ^ rk[59]);
#undef LAST
    }
    a->ctr = c;
}

/* H, Q, P: counters 0..2 with the AI bit (bit 0 of IV byte 0) set; then the
 * keystream restarts at counter 0 with AI = 0 (TS 35.243 clause 6.3). */
static void aes_hqp(void *p, uint8_t hqp[48])
{
    aes_st *a = (aes_st *)p;
    uint32_t save = a->iv[0];
    a->iv[0] = ((save ^ a->rk[0]) | 1u) ^ a->rk[0];
    aes_ctr(a, hqp, 3);
    a->iv[0] = save;
    a->ctr = 0;
}
static void aes_blocks(void *p, uint8_t *out, size_t n) { aes_ctr((aes_st *)p, out, n); }

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
