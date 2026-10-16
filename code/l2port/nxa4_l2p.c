/*
 * nxa4_l2p.c - 256-NEA4 / 256-NIA4 / 256-NCA4: Snow 5G, portable L2 for 32-bit CPUs.
 *
 * Each LFSR is held as two halves of four 32-bit words, two 16-bit cells per
 * word (cell 2k in the low half of word k). All eight new cells of a clock
 * depend only on the old state, so they are computed two at a time with
 * 16-bit lane arithmetic inside 32-bit words. The halves swap roles each
 * clock; unrolling by two clocks avoids copying them.
 * The FSM registers are four little-endian words each, which is also the
 * AES state layout, so AES_R is one T-table round with a zero key.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include "tab_l2p.h"

typedef struct { uint32_t al[4], ah[4], bl[4], bh[4], r1[4], r2[4], r3[4]; } snow_st;

/* times x in GF(2^16) on both 16-bit lanes of w */
static inline uint32_t mulx2(uint32_t w, uint32_t poly2)
{
    uint32_t m = (w >> 15) & 0x00010001u;
    return ((w << 1) & 0xFFFEFFFEu) ^ (poly2 & (m * 0xFFFFu));
}

/* x [+]16 y on both lanes */
static inline uint32_t add16x2(uint32_t x, uint32_t y)
{
    return ((x & 0x7FFF7FFFu) + (y & 0x7FFF7FFFu)) ^ ((x ^ y) & 0x80008000u);
}

/* One clock. Returns z in z[]. The new LFSR halves are written over al and bl,
 * so after the call the roles are: low = ah, high = al (and the same for b). */
static inline __attribute__((always_inline)) void clock1(uint32_t *al, const uint32_t *ah, uint32_t *bl, const uint32_t *bh,
                                                         uint32_t *r1, uint32_t *r2, uint32_t *r3, uint32_t z[4])
{
    uint32_t tmp[4], n2[4], n3[4];
    for (int k = 0; k < 4; k++) {
        z[k] = add16x2(r1[k], bh[k]) ^ r2[k];
        tmp[k] = add16x2(r2[k], r3[k] ^ ah[k]);
    }
    L2P_ROUND(n3[0], n3[1], n3[2], n3[3], r2[0], r2[1], r2[2], r2[3]);     /* R3 = AES_R(R2) */
    L2P_ROUND(n2[0], n2[1], n2[2], n2[3], r1[0], r1[1], r1[2], r1[3]);     /* R2 = AES_R(R1) */
    {                                                                      /* R1 = sigma(tmp) */
        uint32_t x0 = (tmp[0] & 0x00FF00FFu) | ((tmp[1] & 0x00FF00FFu) << 8);
        uint32_t x1 = ((tmp[0] >> 8) & 0x00FF00FFu) | (tmp[1] & 0xFF00FF00u);
        uint32_t y0 = (tmp[2] & 0x00FF00FFu) | ((tmp[3] & 0x00FF00FFu) << 8);
        uint32_t y1 = ((tmp[2] >> 8) & 0x00FF00FFu) | (tmp[3] & 0xFF00FF00u);
        r1[0] = (x0 & 0xFFFFu) | (y0 << 16);
        r1[1] = (x1 & 0xFFFFu) | (y1 << 16);
        r1[2] = (x0 >> 16) | (y0 & 0xFFFF0000u);
        r1[3] = (x1 >> 16) | (y1 & 0xFFFF0000u);
    }
    for (int k = 0; k < 4; k++) { r2[k] = n2[k]; r3[k] = n3[k]; }

    /* LFSRupdate: a_new[j] = b_j ^ a_{7+j} ^ alpha(a_j), b_new[j] = a_j ^ b_{8+j} ^ beta(b_j) */
    uint32_t a7[4] = {(al[3] >> 16) | (ah[0] << 16), (ah[0] >> 16) | (ah[1] << 16),
                      (ah[1] >> 16) | (ah[2] << 16), (ah[2] >> 16) | (ah[3] << 16)};
    for (int k = 0; k < 4; k++) {
        uint32_t na = bl[k] ^ a7[k] ^ mulx2(al[k], 0x4a6d4a6du);
        uint32_t nb = al[k] ^ bh[k] ^ mulx2(bl[k], 0xcc87cc87u);
        al[k] = na; bl[k] = nb;
    }
}

static void snow_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    snow_st *s = (snow_st *)p;
    for (int k = 0; k < 4; k++) {
        s->ah[k] = l2p_ld(key + 4 * k);
        s->al[k] = l2p_ld(iv + 4 * k);
        s->bh[k] = l2p_ld(key + 16 + 4 * k);
        s->bl[k] = 0;
        s->r1[k] = s->r2[k] = s->r3[k] = 0;
    }
    uint32_t z[4];
    for (int t = 1; t <= 16; t += 2) {                                  /* clocks t and t + 1 */
        clock1(s->al, s->ah, s->bl, s->bh, s->r1, s->r2, s->r3, z);          /* high half is now al */
        for (int k = 0; k < 4; k++) s->al[k] ^= z[k];
        if (t == 15) for (int k = 0; k < 4; k++) s->r1[k] ^= l2p_ld(key + 4 * k);
        clock1(s->ah, s->al, s->bh, s->bl, s->r1, s->r2, s->r3, z);          /* high half is ah again */
        for (int k = 0; k < 4; k++) s->ah[k] ^= z[k];
        if (t == 15) for (int k = 0; k < 4; k++) s->r1[k] ^= l2p_ld(key + 16 + 4 * k);
    }
}

/* n keystream blocks; the halves are back in place after each pair of clocks */
static void snow_gen(snow_st *s, uint8_t *out, size_t n)
{
    snow_st c = *s;
    uint32_t z[4];
    for (; n >= 2; n -= 2, out += 32) {
        clock1(c.al, c.ah, c.bl, c.bh, c.r1, c.r2, c.r3, z);
        for (int k = 0; k < 4; k++) l2p_st(out + 4 * k, z[k]);
        clock1(c.ah, c.al, c.bh, c.bl, c.r1, c.r2, c.r3, z);
        for (int k = 0; k < 4; k++) l2p_st(out + 16 + 4 * k, z[k]);
    }
    if (n) {                                /* odd block: one clock, then swap the halves back */
        clock1(c.al, c.ah, c.bl, c.bh, c.r1, c.r2, c.r3, z);
        for (int k = 0; k < 4; k++) {
            l2p_st(out + 4 * k, z[k]);
            uint32_t t = c.al[k]; c.al[k] = c.ah[k]; c.ah[k] = t;
            t = c.bl[k]; c.bl[k] = c.bh[k]; c.bh[k] = t;
        }
    }
    *s = c;
}

static void snow_hqp(void *p, uint8_t hqp[48]) { snow_gen((snow_st *)p, hqp, 3); }
static void snow_blocks(void *p, uint8_t *out, size_t n) { snow_gen((snow_st *)p, out, n); }

static const ksg2_ops KSG_SNOW = {snow_init, snow_hqp, snow_blocks};

void nea4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca4_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
