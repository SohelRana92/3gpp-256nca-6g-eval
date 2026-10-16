/*
 * aead1_l2p.c - 256-AEAD1 with a 4-bit table POLYVAL (portable L2, 32-bit CPUs).
 *
 * POLYVAL and GHASH are the same field with reversed bit order. RFC 8452,
 * Appendix A:  POLYVAL(H, X) = ByteReverse(GHASH(mulX_GHASH(ByteReverse(H)),
 * ByteReverse(X))). We therefore keep the MAC accumulator in the GHASH order
 * and use Shoup's method with 4-bit tables: sixteen multiples of the hash
 * key (256 bytes), built once per packet, then 32 table steps per block on
 * 32-bit words (no 64-bit arithmetic, which a 32-bit CPU splits in two).
 *
 * The table index is secret data. On CPUs with a data cache this can leak
 * timing, as for every table-based GHASH.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include <string.h>

typedef uint32_t __attribute__((may_alias)) u32a;   /* word access to byte buffers */

/* i * key for the 16 nibbles i, GHASH order, as four 32-bit words (w[0] most significant). */
typedef struct { uint32_t m[16][4]; } gtab;

static inline uint32_t ld32be(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static inline uint32_t ld32le(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

/* v = v * x in GHASH order (shift right by one, reduce with 0xE1 << 120) */
static inline void mulx(uint32_t v[4])
{
    uint32_t c = (v[3] & 1) ? 0xE1000000u : 0;
    v[3] = (v[3] >> 1) | (v[2] << 31);
    v[2] = (v[2] >> 1) | (v[1] << 31);
    v[1] = (v[1] >> 1) | (v[0] << 31);
    v[0] = (v[0] >> 1) ^ c;
}

/* Table for the POLYVAL key h (16 bytes, POLYVAL order). */
static void gtab_init(gtab *t, const uint8_t h[16])
{
    uint32_t v[4];
    for (int k = 0; k < 4; k++) v[k] = ld32le(h + 12 - 4 * k);       /* ByteReverse, as big-endian words */
    mulx(v);                                                          /* mulX_GHASH */
    for (int k = 0; k < 4; k++) { t->m[0][k] = 0; t->m[8][k] = v[k]; }  /* bit 0 of a nibble = x^0 */
    for (int i = 4; i > 0; i >>= 1) {                                 /* x^1, x^2, x^3 */
        mulx(v);
        for (int k = 0; k < 4; k++) t->m[i][k] = v[k];
    }
    for (int i = 2; i <= 8; i *= 2)
        for (int j = 1; j < i; j++)
            for (int k = 0; k < 4; k++) t->m[i + j][k] = t->m[i][k] ^ t->m[j][k];
}

static const uint32_t LAST4[16] = {0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
                                   0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0};

/* z = z * x^4, then z ^= M[n] */
#define STEP(n) do {                                                   \
    uint32_t r_ = z3 & 0xF;                                           \
    z3 = (z3 >> 4) | (z2 << 28); z2 = (z2 >> 4) | (z1 << 28);         \
    z1 = (z1 >> 4) | (z0 << 28); z0 = (z0 >> 4) ^ (LAST4[r_] << 16);   \
    const uint32_t *m_ = t->m[(n)];                                   \
    z0 ^= m_[0]; z1 ^= m_[1]; z2 ^= m_[2]; z3 ^= m_[3];               \
} while (0)

/* Absorb n blocks: A = (A ^ ByteReverse(X)) * key, with A in GHASH order as
 * four words a[0..3]. ByteReverse(X) read as big-endian words is X read as
 * little-endian words in reverse order, so no byte reversal is needed.
 * One out-of-line function serves every caller, so that the compiler gives
 * the loop the same registers everywhere. */
static __attribute__((noinline)) void mac_blocks(const gtab *t, uint32_t a[4], const uint8_t *d, size_t n)
{
    uint32_t z0 = a[0], z1 = a[1], z2 = a[2], z3 = a[3];
    for (; n; n--, d += 16) {
        uint32_t x[4] = {z0 ^ ld32le(d + 12), z1 ^ ld32le(d + 8), z2 ^ ld32le(d + 4), z3 ^ ld32le(d)};
        /* Shoup: nibbles from the least significant end of the GHASH value */
        uint32_t w = x[3];
        z0 = t->m[w & 0xF][0]; z1 = t->m[w & 0xF][1]; z2 = t->m[w & 0xF][2]; z3 = t->m[w & 0xF][3];
        STEP((w >> 4) & 0xF);
        for (int s = 8; s < 32; s += 4) STEP((w >> s) & 0xF);
        for (int k = 2; k >= 0; k--) {
            w = x[k];
            for (int s = 0; s < 32; s += 4) STEP((w >> s) & 0xF);
        }
    }
    a[0] = z0; a[1] = z1; a[2] = z2; a[3] = z3;
}

/* POLYVAL-order bytes of the accumulator */
static void acc_bytes(const uint32_t a[4], uint8_t out[16])
{
    for (int k = 0; k < 4; k++) {           /* word k holds GHASH bytes 4k..4k+3 = POLYVAL bytes 15-4k..12-4k */
        out[15 - 4 * k] = (uint8_t)(a[k] >> 24); out[14 - 4 * k] = (uint8_t)(a[k] >> 16);
        out[13 - 4 * k] = (uint8_t)(a[k] >> 8);  out[12 - 4 * k] = (uint8_t)a[k];
    }
}

void polyval_dot_l2(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    gtab t;
    uint32_t acc[4] = {0, 0, 0, 0};
    gtab_init(&t, b);
    mac_blocks(&t, acc, a, 1);
    acc_bytes(acc, r);
}

static void mac_bits(const gtab *t, uint32_t acc[4], const uint8_t *d, uint32_t len_bits)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    uint32_t nbytes = (len_bits + 7) / 8;
    if (!nbytes) return;
    uint32_t whole = (len_bits % 8) ? nbytes - 1 : nbytes;
    uint32_t aligned = whole & ~15u;
    mac_blocks(t, acc, d, aligned / 16);
    uint32_t rest = nbytes - aligned;
    if (rest) {
        uint8_t tail[16] = {0};
        memcpy(tail, d + aligned, rest);
        tail[rest - 1] &= bitpad[len_bits % 8];
        mac_blocks(t, acc, tail, 1);
    }
}

#define CHUNK 512   /* bytes of keystream per pass; a multiple of 16 */

void aead1_l2(const ksg2_ops *k, void *st, const uint8_t key[32], const uint8_t iv[16], int mode,
              uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
              const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    uint32_t hqp32[12], ks32[CHUNK / 4];             /* word arrays: 4-byte aligned */
    uint8_t *hqp = (uint8_t *)hqp32, *ks = (uint8_t *)ks32;
    uint32_t acc[4] = {0, 0, 0, 0};
    gtab th;

    k->init(st, key, iv);
    if (mac_bytes) {
        k->hqp(st, hqp);
        gtab_init(&th, hqp);
        if (aad_length) mac_bits(&th, acc, aad, aad_length);
    }

    uint32_t s_bytes = (s_length + 7) / 8;
    for (uint32_t off = 0; off < s_bytes; off += CHUNK) {
        uint32_t n = s_bytes - off < CHUNK ? s_bytes - off : CHUNK;
        int last = off + n == s_bytes;
        k->blocks(st, ks, (n + 15) / 16);
        const uint8_t *in = ibs + off;
        uint8_t *out = obs + off;
        uint32_t i = 0;
        if ((((uintptr_t)in | (uintptr_t)out) & 3) == 0)              /* word-aligned: XOR by words */
            for (; i + 4 <= n; i += 4)
                *(u32a *)(out + i) = *(const u32a *)(in + i) ^ ks32[i / 4];
        for (; i < n; i++) out[i] = (uint8_t)(in[i] ^ ks[i]);
        if (last) obs[s_bytes - 1] &= bitpad[s_length % 8];

        if (mac_bytes) {
            const uint8_t *c = (mode == 0 ? obs : ibs) + off;          /* the ciphertext */
            if (!last) mac_blocks(&th, acc, c, n / 16);
            else mac_bits(&th, acc, c, s_length - 8 * off);
        }
    }

    if (mac_bytes) {
        uint8_t len[16] = {0};
        len[0] = (uint8_t)s_length; len[1] = (uint8_t)(s_length >> 8);
        len[2] = (uint8_t)(s_length >> 16); len[3] = (uint8_t)(s_length >> 24);
        len[8] = (uint8_t)aad_length; len[9] = (uint8_t)(aad_length >> 8);
        len[10] = (uint8_t)(aad_length >> 16); len[11] = (uint8_t)(aad_length >> 24);
        gtab tq;
        gtab_init(&tq, hqp + 16);                                     /* final dot with Q */
        uint8_t ab[16], full[16];
        acc_bytes(acc, ab);
        for (int i = 0; i < 16; i++) len[i] ^= ab[i];                 /* A ^ L, POLYVAL order */
        uint32_t f[4] = {0, 0, 0, 0};
        mac_blocks(&tq, f, len, 1);                                   /* (A ^ L) . Q */
        acc_bytes(f, full);
        for (int i = 0; i < 16; i++) full[i] ^= hqp[32 + i];          /* ^ P */
        memcpy(mac, full, mac_bytes);
    }
}
