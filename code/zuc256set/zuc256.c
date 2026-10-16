/*
 * zuc256.c - ZUC-256 core (3GPP TS 35.246, clause 6).
 */
#include "zuc256.h"
#include "zuc_sbox.h"

#define M31 0x7FFFFFFFu   /* 2^31 - 1 */

/* a + b mod (2^31 - 1), branch-free (clause 6.2.2, NOTE 2). */
static uint32_t add31(uint32_t a, uint32_t b)
{
    uint32_t w = a + b;                       /* a, b < 2^31, so no 32-bit overflow */
    return (w & M31) + (w >> 31);
}

/* Multiply by 2^k in GF(2^31 - 1): a 31-bit left rotation (clause 6.2.2, NOTE 1). */
static uint32_t rot31(uint32_t x, int k) { return ((x << k) | (x >> (31 - k))) & M31; }

static uint32_t rotl32(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

/* v = 2^15 s15 + 2^17 s13 + 2^21 s10 + 2^20 s4 + (1 + 2^8) s0 mod (2^31 - 1). */
static uint32_t lfsr_feedback(const uint32_t s[16])
{
    uint32_t v = s[0];
    v = add31(v, rot31(s[0], 8));
    v = add31(v, rot31(s[4], 20));
    v = add31(v, rot31(s[10], 21));
    v = add31(v, rot31(s[13], 17));
    v = add31(v, rot31(s[15], 15));
    return v;
}

static void lfsr_shift_in(uint32_t s[16], uint32_t tmp)
{
    if (tmp == 0) tmp = M31;                  /* step 4 / step 3: 0 is written as 2^31 - 1 */
    for (int i = 0; i < 15; i++) s[i] = s[i + 1];
    s[15] = tmp;
}

/* LFSRWithInitialisationMode(u) and LFSRWithWorkMode() (clause 6.2.2). */
static void lfsr_init_mode(uint32_t s[16], uint32_t u) { lfsr_shift_in(s, add31(lfsr_feedback(s), u)); }
static void lfsr_work_mode(uint32_t s[16])             { lfsr_shift_in(s, lfsr_feedback(s)); }

/* Bit-reorganisation (clause 6.2.3). H = bits 30..15, L = bits 15..0 of a cell. */
static void bit_reorg(const uint32_t s[16], uint32_t x[4])
{
    x[0] = (((s[15] >> 15) & 0xFFFF) << 16) | (s[14] & 0xFFFF);   /* s15H || s14L */
    x[1] = ((s[11] & 0xFFFF) << 16) | ((s[9] >> 15) & 0xFFFF);    /* s11L || s9H  */
    x[2] = ((s[7] & 0xFFFF) << 16) | ((s[5] >> 15) & 0xFFFF);     /* s7L  || s5H  */
    x[3] = ((s[2] & 0xFFFF) << 16) | ((s[0] >> 15) & 0xFFFF);     /* s2L  || s0H  */
}

/* S = (S0, S1, S2, S3) with S2 = S0, S3 = S1; byte x0 is the most significant (clause 6.2.5). */
static uint32_t sbox32(uint32_t x)
{
    return ((uint32_t)ZUC_S0[(x >> 24) & 0xFF] << 24) | ((uint32_t)ZUC_S1[(x >> 16) & 0xFF] << 16) |
           ((uint32_t)ZUC_S0[(x >> 8) & 0xFF] << 8)   |  (uint32_t)ZUC_S1[x & 0xFF];
}

/* Linear transforms L1 and L2 (clause 6.2.6). */
static uint32_t L1(uint32_t x) { return x ^ rotl32(x, 2) ^ rotl32(x, 10) ^ rotl32(x, 18) ^ rotl32(x, 24); }
static uint32_t L2(uint32_t x) { return x ^ rotl32(x, 8) ^ rotl32(x, 14) ^ rotl32(x, 22) ^ rotl32(x, 30); }

/* Nonlinear function F (clause 6.2.4). */
static uint32_t F(zuc256_state *st, const uint32_t x[4])
{
    uint32_t w = (x[0] ^ st->r1) + st->r2;
    uint32_t a = st->r1 + x[1];
    uint32_t b = st->r2 ^ x[2];
    st->r1 = sbox32(L1((a << 16) | (b >> 16)));    /* A_L || B_H */
    st->r2 = sbox32(L2((b << 16) | (a >> 16)));    /* B_L || A_H */
    return w;
}

/* The 7-bit constants d_0..d_15 (clause 6.2.7). */
static const uint8_t D[16] = {0x64, 0x43, 0x7b, 0x2a, 0x11, 0x05, 0x51, 0x42,
                              0x1a, 0x31, 0x18, 0x66, 0x14, 0x2e, 0x01, 0x5c};

static uint32_t cell(uint8_t k, uint8_t d, uint8_t b2, uint8_t b3)
{
    /* s = K (8 bits) || d (7 bits) || byte || byte, K most significant (31 bits). */
    return ((uint32_t)k << 23) | ((uint32_t)d << 16) | ((uint32_t)b2 << 8) | b3;
}

void zuc256_init(zuc256_state *st, const uint8_t key[32], const uint8_t iv[16])
{
    for (int i = 0; i < 7; i++)  st->s[i] = cell(key[i], D[i], key[16 + i], key[24 + i]);   /* s0..s6  */
    for (int i = 7; i < 15; i++) st->s[i] = cell(key[i], D[i], iv[i - 7], iv[i + 1]);      /* s7..s14 */
    st->s[15] = cell(key[15], D[15], key[23], key[31]);                                     /* s15     */
    st->r1 = st->r2 = 0;

    uint32_t x[4];
    for (int r = 1; r <= ZUC256_P; r++) {
        bit_reorg(st->s, x);
        uint32_t w = F(st, x);
        lfsr_init_mode(st->s, w >> 1);
    }
    bit_reorg(st->s, x);
    (void)F(st, x);                              /* output of F is discarded */
    lfsr_work_mode(st->s);
}

void zuc256_word(zuc256_state *st, uint8_t z[4])
{
    uint32_t x[4];
    bit_reorg(st->s, x);
    uint32_t w = F(st, x) ^ x[3];
    lfsr_work_mode(st->s);
    z[0] = (uint8_t)(w >> 24); z[1] = (uint8_t)(w >> 16); z[2] = (uint8_t)(w >> 8); z[3] = (uint8_t)w;
}
