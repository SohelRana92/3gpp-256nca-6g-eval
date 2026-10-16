/*
 * snow5g.c - Snow 5G core (3GPP TS 35.240, clause 6).
 */
#include "snow5g.h"
#include "../common/aes256.h"
#include <string.h>

/* alpha and beta (clauses 6.2.1.1, 6.2.1.2): multiply by x in GF(2^16). */
static uint16_t alpha(uint16_t v) { uint16_t r = (uint16_t)(v << 1); if (v & 0x8000) r ^= 0x4a6d; return r; }
static uint16_t beta(uint16_t v)  { uint16_t r = (uint16_t)(v << 1); if (v & 0x8000) r ^= 0xcc87; return r; }

/* Load eight 16-bit words w_8..w_15 (w_8 least significant) as a 128-bit
 * value in little-endian byte order: bytes 2i, 2i+1 hold w_{8+i}. */
static void words_hi_to_bytes(const uint16_t w[16], uint8_t out[16])
{
    for (int i = 0; i < 8; i++) { out[2 * i] = (uint8_t)w[8 + i]; out[2 * i + 1] = (uint8_t)(w[8 + i] >> 8); }
}

/* x [+]16 y: eight independent additions modulo 2^16 on 16-bit lanes. */
static void add16(uint8_t r[16], const uint8_t x[16], const uint8_t y[16])
{
    for (int i = 0; i < 8; i++) {
        uint16_t s = (uint16_t)((x[2 * i] | (x[2 * i + 1] << 8)) + (y[2 * i] | (y[2 * i + 1] << 8)));
        r[2 * i] = (uint8_t)s; r[2 * i + 1] = (uint8_t)(s >> 8);
    }
}

/* sigma (clause 6.2.1.3): output byte j takes input byte SIGMA[j]. */
static const uint8_t SIGMA[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};

/* LFSRupdate (clause 6.2.2.1): eight single clocks of both registers. */
static void lfsr_update(snow5g_state *st)
{
    for (int step = 0; step < 8; step++) {
        uint16_t ta = (uint16_t)(st->b[0] ^ st->a[7] ^ alpha(st->a[0]));
        uint16_t tb = (uint16_t)(st->a[0] ^ st->b[8] ^ beta(st->b[0]));
        memmove(st->a, st->a + 1, 15 * sizeof(uint16_t)); st->a[15] = ta;
        memmove(st->b, st->b + 1, 15 * sizeof(uint16_t)); st->b[15] = tb;
    }
}

/* FSMupdate (clause 6.2.3). */
static void fsm_update(snow5g_state *st)
{
    uint8_t t2[16], x[16], tmp[16];
    words_hi_to_bytes(st->a, t2);                 /* T2 = a15 || ... || a8 */
    for (int i = 0; i < 16; i++) x[i] = (uint8_t)(st->r3[i] ^ t2[i]);
    add16(tmp, st->r2, x);                        /* tmp = R2 [+]16 (R3 xor T2) */
    memcpy(st->r3, st->r2, 16); aes_round_zero_key(st->r3);   /* R3 = AES_R(R2) */
    memcpy(st->r2, st->r1, 16); aes_round_zero_key(st->r2);   /* R2 = AES_R(R1) */
    for (int j = 0; j < 16; j++) st->r1[j] = tmp[SIGMA[j]];   /* R1 = sigma(tmp) */
}

/* One clock: z = (R1 [+]16 T1) xor R2, then FSMupdate and LFSRupdate. */
static void snow_clock(snow5g_state *st, uint8_t z[16])
{
    uint8_t t1[16];
    words_hi_to_bytes(st->b, t1);                 /* T1 = b15 || ... || b8 */
    add16(z, st->r1, t1);
    for (int i = 0; i < 16; i++) z[i] ^= st->r2[i];
    fsm_update(st);
    lfsr_update(st);
}

void snow5g_init(snow5g_state *st, const uint8_t key[32], const uint8_t iv[16])
{
    /* Table 6.3-1: k_i = KEY[2i+1] || KEY[2i], iv_i = IV[2i+1] || IV[2i]. */
    for (int i = 0; i < 8; i++) {
        st->a[8 + i] = (uint16_t)(key[2 * i] | (key[2 * i + 1] << 8));            /* a8..a15 = k0..k7  */
        st->a[i]     = (uint16_t)(iv[2 * i] | (iv[2 * i + 1] << 8));              /* a0..a7  = iv0..iv7 */
        st->b[8 + i] = (uint16_t)(key[16 + 2 * i] | (key[16 + 2 * i + 1] << 8));  /* b8..b15 = k8..k15 */
        st->b[i]     = 0;
    }
    memset(st->r1, 0, 16); memset(st->r2, 0, 16); memset(st->r3, 0, 16);
    for (int t = 1; t <= 16; t++) {
        uint8_t z[16];
        snow_clock(st, z);
        for (int i = 0; i < 8; i++)                                              /* (a15..a8) ^= z */
            st->a[8 + i] ^= (uint16_t)(z[2 * i] | (z[2 * i + 1] << 8));
        if (t == 15) for (int i = 0; i < 16; i++) st->r1[i] ^= key[i];             /* R1 ^= (k7..k0)  */
        if (t == 16) for (int i = 0; i < 16; i++) st->r1[i] ^= key[16 + i];        /* R1 ^= (k15..k8) */
    }
}

void snow5g_keystream(snow5g_state *st, uint8_t z[16])
{
    snow_clock(st, z);
}
