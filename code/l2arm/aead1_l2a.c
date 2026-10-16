/*
 * aead1_l2a.c - 256-AEAD1 with a PMULL POLYVAL (L2, ARMv8-A + crypto).
 *
 * Same structure as ../l2x86/aead1_l2.c. A field element is a uint64x2_t
 * whose lane 0 holds bytes 0..7 (little-endian), as RFC 8452 requires.
 * a.b = a*b*x^-128; eight blocks share one reduction with H^1..H^8.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include <arm_neon.h>
#include <string.h>

/* ---- POLYVAL ------------------------------------------------------------ */
static inline uint64x2_t pmull(uint64_t a, uint64_t b)
{
    return vreinterpretq_u64_p128(vmull_p64((poly64_t)a, (poly64_t)b));
}

/* Unreduced 256-bit carry-less product (lo, hi). */
static inline void clmul256(uint64x2_t a, uint64x2_t b, uint64x2_t *lo, uint64x2_t *hi)
{
    const uint64x2_t z = vdupq_n_u64(0);
    uint64x2_t t0 = pmull(vgetq_lane_u64(a, 0), vgetq_lane_u64(b, 0));
    uint64x2_t t3 = pmull(vgetq_lane_u64(a, 1), vgetq_lane_u64(b, 1));
    uint64x2_t t1 = veorq_u64(pmull(vgetq_lane_u64(a, 0), vgetq_lane_u64(b, 1)),
                              pmull(vgetq_lane_u64(a, 1), vgetq_lane_u64(b, 0)));
    *lo = veorq_u64(t0, vextq_u64(z, t1, 1));        /* t1 << 64  */
    *hi = veorq_u64(t3, vextq_u64(t1, z, 1));        /* t1 >> 64  */
}

/* (lo, hi) * x^-128 mod x^128 + x^127 + x^126 + x^121 + 1, two folding steps
 * (the same steps as the x86 code, with 0xC2000000_00000000 as the constant). */
static inline uint64x2_t reduce(uint64x2_t lo, uint64x2_t hi)
{
    const uint64_t c = 0xC200000000000000ULL;
    uint64x2_t t = pmull(vgetq_lane_u64(lo, 0), c);
    lo = veorq_u64(vextq_u64(lo, lo, 1), t);
    t = pmull(vgetq_lane_u64(lo, 0), c);
    lo = veorq_u64(vextq_u64(lo, lo, 1), t);
    return veorq_u64(lo, hi);
}

static inline uint64x2_t dot(uint64x2_t a, uint64x2_t b)
{
    uint64x2_t lo, hi;
    clmul256(a, b, &lo, &hi);
    return reduce(lo, hi);
}

static inline uint64x2_t ld(const uint8_t *p) { return vreinterpretq_u64_u8(vld1q_u8(p)); }

void polyval_dot_l2(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    vst1q_u8(r, vreinterpretq_u8_u64(dot(ld(a), ld(b))));
}

typedef struct {
    uint64x2_t a, h[8];
    int npow;
} mac_t;

static void mac_blocks(mac_t *m, const uint8_t *x, size_t n)
{
    uint64x2_t a = m->a;
    if (n >= 8 && m->npow < 8) {
        for (int i = 1; i < 8; i++) m->h[i] = dot(m->h[i - 1], m->h[0]);
        m->npow = 8;
    }
    while (n >= 8) {
        uint64x2_t lo, hi, l, h;
        clmul256(veorq_u64(a, ld(x)), m->h[7], &lo, &hi);
        for (int i = 1; i < 8; i++) {
            clmul256(ld(x + 16 * i), m->h[7 - i], &l, &h);
            lo = veorq_u64(lo, l);
            hi = veorq_u64(hi, h);
        }
        a = reduce(lo, hi);
        x += 128; n -= 8;
    }
    while (n--) {
        a = dot(veorq_u64(a, ld(x)), m->h[0]);
        x += 16;
    }
    m->a = a;
}

static void mac_bits(mac_t *m, const uint8_t *d, uint32_t len_bits)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    uint32_t nbytes = (len_bits + 7) / 8;
    if (!nbytes) return;
    uint32_t whole = (len_bits % 8) ? nbytes - 1 : nbytes;
    uint32_t aligned = whole & ~15u;
    mac_blocks(m, d, aligned / 16);
    uint32_t rest = nbytes - aligned;
    if (rest) {
        uint8_t tail[16] = {0};
        memcpy(tail, d + aligned, rest);
        tail[rest - 1] &= bitpad[len_bits % 8];
        mac_blocks(m, tail, 1);
    }
}

/* ---- 256-AEAD1 ---------------------------------------------------------- */
#define CHUNK 512

void aead1_l2(const ksg2_ops *k, void *st, const uint8_t key[32], const uint8_t iv[16], int mode,
              uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
              const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    _Alignas(16) uint8_t hqp[48];
    _Alignas(16) uint8_t ks[CHUNK];
    mac_t m;

    k->init(st, key, iv);
    if (mac_bytes) {
        k->hqp(st, hqp);
        m.a = vdupq_n_u64(0);
        m.h[0] = ld(hqp);
        m.npow = 1;
        if (aad_length) mac_bits(&m, aad, aad_length);
    }

    uint32_t s_bytes = (s_length + 7) / 8;
    for (uint32_t off = 0; off < s_bytes; off += CHUNK) {
        uint32_t n = s_bytes - off < CHUNK ? s_bytes - off : CHUNK;
        int last = off + n == s_bytes;
        k->blocks(st, ks, (n + 15) / 16);
        uint32_t i = 0;
        for (; i + 16 <= n; i += 16)
            vst1q_u8(obs + off + i, veorq_u8(vld1q_u8(ibs + off + i), vld1q_u8(ks + i)));
        for (; i < n; i++) obs[off + i] = (uint8_t)(ibs[off + i] ^ ks[i]);
        if (last) obs[s_bytes - 1] &= bitpad[s_length % 8];

        if (mac_bytes) {
            const uint8_t *c = (mode == 0 ? obs : ibs) + off;
            if (!last) mac_blocks(&m, c, n / 16);
            else mac_bits(&m, c, s_length - 8 * off);
        }
    }

    if (mac_bytes) {
        uint64x2_t len = vcombine_u64(vcreate_u64((uint64_t)s_length), vcreate_u64((uint64_t)aad_length));
        uint64x2_t a = dot(veorq_u64(m.a, len), ld(hqp + 16));
        a = veorq_u64(a, ld(hqp + 32));
        _Alignas(16) uint8_t full[16];
        vst1q_u8(full, vreinterpretq_u8_u64(a));
        memcpy(mac, full, mac_bytes);
    }
}
