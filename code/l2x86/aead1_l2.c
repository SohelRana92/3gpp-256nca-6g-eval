/*
 * aead1_l2.c - 256-AEAD1 with a PCLMULQDQ POLYVAL (L2, x86-64).
 *
 * Mac5G uses POLYVAL's dot product a.b = a*b*x^-128 (RFC 8452). With the
 * powers H^1..H^8 (in the dot sense), eight blocks are absorbed with one
 * reduction:  A' = (A^X1).H^8 ^ X2.H^7 ^ ... ^ X8.H^1.
 */
#include "aead1_l2.h"
#include "nxa_l2.h"
#include <immintrin.h>
#include <string.h>

/* ---- POLYVAL ------------------------------------------------------------ */
/* Unreduced 256-bit carry-less product of a and b, as (lo, hi). */
static inline void clmul256(__m128i a, __m128i b, __m128i *lo, __m128i *hi)
{
    __m128i t0 = _mm_clmulepi64_si128(a, b, 0x00);
    __m128i t3 = _mm_clmulepi64_si128(a, b, 0x11);
    __m128i t1 = _mm_xor_si128(_mm_clmulepi64_si128(a, b, 0x10), _mm_clmulepi64_si128(a, b, 0x01));
    *lo = _mm_xor_si128(t0, _mm_slli_si128(t1, 8));
    *hi = _mm_xor_si128(t3, _mm_srli_si128(t1, 8));
}

/* (lo, hi) * x^-128 mod x^128 + x^127 + x^126 + x^121 + 1 (two folding steps). */
static inline __m128i reduce(__m128i lo, __m128i hi)
{
    const __m128i poly = _mm_set_epi32((int)0xc2000000, 0, 0, 1);
    __m128i t = _mm_clmulepi64_si128(lo, poly, 0x10);
    lo = _mm_xor_si128(_mm_shuffle_epi32(lo, 78), t);
    t = _mm_clmulepi64_si128(lo, poly, 0x10);
    lo = _mm_xor_si128(_mm_shuffle_epi32(lo, 78), t);
    return _mm_xor_si128(lo, hi);
}

static inline __m128i dot(__m128i a, __m128i b)
{
    __m128i lo, hi;
    clmul256(a, b, &lo, &hi);
    return reduce(lo, hi);
}

void polyval_dot_l2(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    _mm_storeu_si128((__m128i *)r, dot(_mm_loadu_si128((const __m128i *)a), _mm_loadu_si128((const __m128i *)b)));
}

typedef struct {
    __m128i a, h[8];      /* accumulator; h[i] = H^(i+1) */
    int npow;             /* powers computed so far (1 or 8) */
} mac_t;

static void mac_blocks(mac_t *m, const uint8_t *x, size_t n)
{
    __m128i a = m->a;
    if (n >= 8 && m->npow < 8) {
        for (int i = 1; i < 8; i++) m->h[i] = dot(m->h[i - 1], m->h[0]);
        m->npow = 8;
    }
    while (n >= 8) {
        __m128i lo, hi, l, h;
        clmul256(_mm_xor_si128(a, _mm_loadu_si128((const __m128i *)x)), m->h[7], &lo, &hi);
        for (int i = 1; i < 8; i++) {
            clmul256(_mm_loadu_si128((const __m128i *)(x + 16 * i)), m->h[7 - i], &l, &h);
            lo = _mm_xor_si128(lo, l);
            hi = _mm_xor_si128(hi, h);
        }
        a = reduce(lo, hi);
        x += 128; n -= 8;
    }
    while (n--) {
        a = dot(_mm_xor_si128(a, _mm_loadu_si128((const __m128i *)x)), m->h[0]);
        x += 16;
    }
    m->a = a;
}

/* Absorb len_bits of a stream: whole blocks directly, then the tail (with the
 * unused bits of the last byte cleared) as one zero-padded block. */
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
#define CHUNK 512   /* bytes per keystream / MAC pass; a multiple of 128 */

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
        m.a = _mm_setzero_si128();
        m.h[0] = _mm_load_si128((const __m128i *)hqp);
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
            _mm_storeu_si128((__m128i *)(obs + off + i),
                             _mm_xor_si128(_mm_loadu_si128((const __m128i *)(ibs + off + i)),
                                           _mm_load_si128((const __m128i *)(ks + i))));
        for (; i < n; i++) obs[off + i] = (uint8_t)(ibs[off + i] ^ ks[i]);
        if (last) obs[s_bytes - 1] &= bitpad[s_length % 8];

        if (mac_bytes) {
            const uint8_t *c = (mode == 0 ? obs : ibs) + off;   /* the ciphertext */
            if (!last) mac_blocks(&m, c, n / 16);
            else mac_bits(&m, c, s_length - 8 * off);
        }
    }

    if (mac_bytes) {
        __m128i len = _mm_set_epi32(0, (int)aad_length, 0, (int)s_length);   /* little-endian */
        __m128i a = dot(_mm_xor_si128(m.a, len), _mm_load_si128((const __m128i *)(hqp + 16)));
        a = _mm_xor_si128(a, _mm_load_si128((const __m128i *)(hqp + 32)));
        _Alignas(16) uint8_t full[16];
        _mm_store_si128((__m128i *)full, a);
        memcpy(mac, full, mac_bytes);
    }
}
