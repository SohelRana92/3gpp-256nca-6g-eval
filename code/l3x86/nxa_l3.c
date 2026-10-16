/*
 * nxa_l3.c - L3 for x86-64: 256-AEAD1 with a 512-bit VPCLMULQDQ POLYVAL, a
 * 512-bit VAES AES-256 KSG, and the L2 Snow 5G and ZUC-256 KSGs.
 *
 * POLYVAL over 16 blocks with one reduction (a.b = a*b*x^-128, RFC 8452):
 *   A' = (A^X1).H^16 ^ X2.H^15 ^ ... ^ X16.H^1,
 * done as four 512-bit carry-less products, each covering four blocks.
 */
#include "nxa_l3.h"
#include "../l2x86/ksg_l2.h"
#include "../l2x86/nxa_l2.h"

/* Below this many bytes (data + AAD) the 512-bit set-up costs more than it
 * saves, so L3 hands the packet to L2 (measured on Zen 4, see README). */
#define L3_MIN_BYTES 512
#include <immintrin.h>
#include <string.h>

int r4_l3_supported(void)
{
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") &&
           __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("vaes") &&
           __builtin_cpu_supports("vpclmulqdq") && __builtin_cpu_supports("aes") &&
           __builtin_cpu_supports("pclmul");
}

/* ---- POLYVAL ------------------------------------------------------------ */
static inline void clmul256(__m128i a, __m128i b, __m128i *lo, __m128i *hi)
{
    __m128i t0 = _mm_clmulepi64_si128(a, b, 0x00);
    __m128i t3 = _mm_clmulepi64_si128(a, b, 0x11);
    __m128i t1 = _mm_xor_si128(_mm_clmulepi64_si128(a, b, 0x10), _mm_clmulepi64_si128(a, b, 0x01));
    *lo = _mm_xor_si128(t0, _mm_slli_si128(t1, 8));
    *hi = _mm_xor_si128(t3, _mm_srli_si128(t1, 8));
}

static inline void clmul256x4(__m512i a, __m512i b, __m512i *lo, __m512i *hi)
{
    __m512i t0 = _mm512_clmulepi64_epi128(a, b, 0x00);
    __m512i t3 = _mm512_clmulepi64_epi128(a, b, 0x11);
    __m512i t1 = _mm512_xor_si512(_mm512_clmulepi64_epi128(a, b, 0x10), _mm512_clmulepi64_epi128(a, b, 0x01));
    *lo = _mm512_xor_si512(t0, _mm512_bslli_epi128(t1, 8));
    *hi = _mm512_xor_si512(t3, _mm512_bsrli_epi128(t1, 8));
}

static inline __m128i fold4(__m512i v)                    /* XOR of the four 128-bit lanes */
{
    __m256i h = _mm256_xor_si256(_mm512_castsi512_si256(v), _mm512_extracti64x4_epi64(v, 1));
    return _mm_xor_si128(_mm256_castsi256_si128(h), _mm256_extracti128_si256(h, 1));
}

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

/* Four independent dot products, one per 128-bit lane. */
static inline __m512i dot4(__m512i a, __m512i b)
{
    const __m512i poly = _mm512_broadcast_i32x4(_mm_set_epi32((int)0xc2000000, 0, 0, 1));
    __m512i lo, hi;
    clmul256x4(a, b, &lo, &hi);
    __m512i t = _mm512_clmulepi64_epi128(lo, poly, 0x10);
    lo = _mm512_xor_si512(_mm512_shuffle_epi32(lo, (_MM_PERM_ENUM)78), t);
    t = _mm512_clmulepi64_epi128(lo, poly, 0x10);
    lo = _mm512_xor_si512(_mm512_shuffle_epi32(lo, (_MM_PERM_ENUM)78), t);
    return _mm512_xor_si512(lo, hi);
}

typedef struct {
    __m128i a;
    __m128i hp[20];           /* hp[k] = H^k (dot powers), k = 1..16; hp[0], hp[17..19] unused */
    __m512i hz[4];            /* hz[g] = (H^(16-4g), H^(15-4g), H^(14-4g), H^(13-4g)) by lane */
    int npow;                 /* highest power ready: 1, 4, 8, 12 or 16 */
} mac_t;

/* H^2..H^8 or H^2..H^16. Up to H^4 one at a time; then (H^1..H^4) times H^4
 * gives H^5..H^8 in one 4-lane step, and so on: 6 sequential steps for 16. */
static void powers(mac_t *m, int upto)
{
    if (upto <= m->npow) return;
    if (m->npow < 4) {
        m->hp[2] = dot(m->hp[1], m->hp[1]);
        m->hp[3] = dot(m->hp[2], m->hp[1]);
        m->hp[4] = dot(m->hp[2], m->hp[2]);
        m->npow = 4;
    }
    __m512i h4 = _mm512_broadcast_i32x4(m->hp[4]);
    __m512i v = _mm512_loadu_si512((const void *)&m->hp[m->npow - 3]);   /* H^(n-3) .. H^n */
    while (m->npow < upto) {
        v = dot4(v, h4);                                                  /* next four powers */
        _mm512_storeu_si512((void *)&m->hp[m->npow + 1], v);
        m->npow += 4;
    }
    if (m->npow >= 16)
        for (int g = 0; g < 4; g++) {
            int top = 16 - 4 * g;                      /* lane j gets H^(top - j) */
            m->hz[g] = _mm512_inserti32x4(_mm512_inserti32x4(_mm512_inserti32x4(
                           _mm512_castsi128_si512(m->hp[top]), m->hp[top - 1], 1), m->hp[top - 2], 2), m->hp[top - 3], 3);
        }
}

static void mac_blocks(mac_t *m, const uint8_t *x, size_t n)
{
    __m128i a = m->a;
    if (n >= 32 || (n >= 16 && m->npow >= 16)) {
        powers(m, 16);
        while (n >= 16) {
            __m512i lo, hi, l, h;
            __m512i x0 = _mm512_xor_si512(_mm512_loadu_si512((const void *)x), _mm512_zextsi128_si512(a));
            clmul256x4(x0, m->hz[0], &lo, &hi);
            for (int g = 1; g < 4; g++) {
                clmul256x4(_mm512_loadu_si512((const void *)(x + 64 * g)), m->hz[g], &l, &h);
                lo = _mm512_xor_si512(lo, l);
                hi = _mm512_xor_si512(hi, h);
            }
            a = reduce(fold4(lo), fold4(hi));
            x += 256; n -= 16;
        }
    }
    if (n >= 8) {
        powers(m, 8);
        __m128i lo, hi, l, h;
        clmul256(_mm_xor_si128(a, _mm_loadu_si128((const __m128i *)x)), m->hp[8], &lo, &hi);
        for (int i = 1; i < 8; i++) {
            clmul256(_mm_loadu_si128((const __m128i *)(x + 16 * i)), m->hp[8 - i], &l, &h);
            lo = _mm_xor_si128(lo, l);
            hi = _mm_xor_si128(hi, h);
        }
        a = reduce(lo, hi);
        x += 128; n -= 8;
    }
    while (n--) {
        a = dot(_mm_xor_si128(a, _mm_loadu_si128((const __m128i *)x)), m->hp[1]);
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
#define CHUNK 512   /* bytes per pass, a multiple of 256; the keystream buffer has 64 bytes of slack */

static void aead1_l3(const ksg2_ops *k, void *st, const uint8_t key[32], const uint8_t iv[16], int mode,
                     uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
                     const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    _Alignas(64) uint8_t hqp[48];
    _Alignas(64) uint8_t ks[CHUNK + 64];
    mac_t m;

    k->init(st, key, iv);
    if (mac_bytes) {
        k->hqp(st, hqp);
        m.a = _mm_setzero_si128();
        m.hp[1] = _mm_load_si128((const __m128i *)hqp);
        m.npow = 1;
        if (aad_length) mac_bits(&m, aad, aad_length);
    }

    uint32_t s_bytes = (s_length + 7) / 8;
    for (uint32_t off = 0; off < s_bytes; off += CHUNK) {
        uint32_t n = s_bytes - off < CHUNK ? s_bytes - off : CHUNK;
        int last = off + n == s_bytes;
        k->blocks(st, ks, (n + 15) / 16);
        uint32_t i = 0;
        for (; i + 64 <= n; i += 64)
            _mm512_storeu_si512((void *)(obs + off + i),
                                _mm512_xor_si512(_mm512_loadu_si512((const void *)(ibs + off + i)),
                                                 _mm512_load_si512((const void *)(ks + i))));
        if (i < n) {                                  /* 1..63 bytes: one masked operation */
            __mmask64 mk = _bzhi_u64(~0ull, n - i);
            __m512i v = _mm512_maskz_loadu_epi8(mk, ibs + off + i);
            _mm512_mask_storeu_epi8(obs + off + i, mk, _mm512_xor_si512(v, _mm512_load_si512((const void *)(ks + i))));
        }
        if (last) obs[s_bytes - 1] &= bitpad[s_length % 8];

        if (mac_bytes) {
            const uint8_t *c = (mode == 0 ? obs : ibs) + off;
            if (!last) mac_blocks(&m, c, n / 16);
            else mac_bits(&m, c, s_length - 8 * off);
        }
    }

    if (mac_bytes) {
        __m128i len = _mm_set_epi32(0, (int)aad_length, 0, (int)s_length);
        __m128i a = dot(_mm_xor_si128(m.a, len), _mm_load_si128((const __m128i *)(hqp + 16)));
        a = _mm_xor_si128(a, _mm_load_si128((const __m128i *)(hqp + 32)));
        _Alignas(16) uint8_t full[16];
        _mm_store_si128((__m128i *)full, a);
        memcpy(mac, full, mac_bytes);
    }
}

/* ---- AES-256 KSG with 512-bit VAES -------------------------------------- */
/* Counter blocks are the IV with a 32-bit big-endian counter in bytes 12..15.
 * We keep little-endian counters in dword 3 of each 128-bit lane, add with
 * VPADDD (wraps mod 2^32, as in L1), and byte-swap them into place. Up to
 * three blocks beyond n may be written to out (the caller leaves room). */
static void aes_blocks_l3(void *s, uint8_t *out, size_t n)
{
    if (n < 16) { KSG2_AES.blocks(s, out, n); return; }   /* short runs: the 128-bit L2 code is quicker */
    aes_st *a = (aes_st *)s;
    uint32_t ctr = ((uint32_t)a->iv[12] << 24) | ((uint32_t)a->iv[13] << 16) | ((uint32_t)a->iv[14] << 8) | a->iv[15];
    __m512i base = _mm512_broadcast_i32x4(_mm_insert_epi32(_mm_loadu_si128((const __m128i *)a->iv), 0, 3));
    const __m512i swap = _mm512_broadcast_i32x4(_mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128,
                                                              -128, -128, -128, -128, 15, 14, 13, 12));
    const __m512i inc4 = _mm512_broadcast_i32x4(_mm_setr_epi32(0, 0, 0, 4));
    __m512i c = _mm512_add_epi32(_mm512_broadcast_i32x4(_mm_setr_epi32(0, 0, 0, (int)ctr)),
                                 _mm512_setr_epi32(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3));
    __m512i rk[15];
    for (int r = 0; r < 15; r++) rk[r] = _mm512_broadcast_i32x4(a->rk[r]);

    size_t left = n;
    while (left >= 16) {
        __m512i b0 = _mm512_or_si512(base, _mm512_shuffle_epi8(c, swap)); c = _mm512_add_epi32(c, inc4);
        __m512i b1 = _mm512_or_si512(base, _mm512_shuffle_epi8(c, swap)); c = _mm512_add_epi32(c, inc4);
        __m512i b2 = _mm512_or_si512(base, _mm512_shuffle_epi8(c, swap)); c = _mm512_add_epi32(c, inc4);
        __m512i b3 = _mm512_or_si512(base, _mm512_shuffle_epi8(c, swap)); c = _mm512_add_epi32(c, inc4);
        b0 = _mm512_xor_si512(b0, rk[0]); b1 = _mm512_xor_si512(b1, rk[0]);
        b2 = _mm512_xor_si512(b2, rk[0]); b3 = _mm512_xor_si512(b3, rk[0]);
#pragma GCC unroll 13
        for (int r = 1; r < 14; r++) {
            b0 = _mm512_aesenc_epi128(b0, rk[r]); b1 = _mm512_aesenc_epi128(b1, rk[r]);
            b2 = _mm512_aesenc_epi128(b2, rk[r]); b3 = _mm512_aesenc_epi128(b3, rk[r]);
        }
        _mm512_storeu_si512((void *)(out +   0), _mm512_aesenclast_epi128(b0, rk[14]));
        _mm512_storeu_si512((void *)(out +  64), _mm512_aesenclast_epi128(b1, rk[14]));
        _mm512_storeu_si512((void *)(out + 128), _mm512_aesenclast_epi128(b2, rk[14]));
        _mm512_storeu_si512((void *)(out + 192), _mm512_aesenclast_epi128(b3, rk[14]));
        out += 256; left -= 16;
    }
    while (left) {                                     /* groups of four; the last may overrun */
        __m512i b = _mm512_xor_si512(_mm512_or_si512(base, _mm512_shuffle_epi8(c, swap)), rk[0]);
        c = _mm512_add_epi32(c, inc4);
#pragma GCC unroll 13
        for (int r = 1; r < 14; r++) b = _mm512_aesenc_epi128(b, rk[r]);
        _mm512_storeu_si512((void *)out, _mm512_aesenclast_epi128(b, rk[14]));
        out += 64; left = left > 4 ? left - 4 : 0;
    }
    ctr += (uint32_t)n;
    a->iv[12] = (uint8_t)(ctr >> 24); a->iv[13] = (uint8_t)(ctr >> 16);
    a->iv[14] = (uint8_t)(ctr >> 8);  a->iv[15] = (uint8_t)ctr;
}

static void aes_init_l3(void *s, const uint8_t key[32], const uint8_t iv[16]) { KSG2_AES.init(s, key, iv); }
static void aes_hqp_l3(void *s, uint8_t hqp[48]) { KSG2_AES.hqp(s, hqp); }
static const ksg2_ops KSG3_AES = {aes_init_l3, aes_hqp_l3, aes_blocks_l3};

/* ---- the nine algorithms ------------------------------------------------ */
/* Each entry point is a thin front that jumps straight to L2 for short
 * packets, and a separate body (not inlined) holding the large aligned
 * stack frame for long ones. Keeping the frame out of the front path
 * matters: in the first version the short-packet hand-off cost 10-45 cycles. */
#define DEFINE_SET(N, OPS)                                                                              \
__attribute__((noinline)) static void nea##N##_big(uint32_t count, uint8_t bearer, uint8_t direction, \
        const uint8_t extra_iv[6], const uint8_t key[32], const uint8_t *ibs, uint32_t length,          \
        uint8_t *obs)                                                                                   \
{                                                                                                       \
    _Alignas(64) uint8_t st[KSG2_STATE_MAX]; uint8_t iv[16];                                            \
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);                                      \
    aead1_l3(&OPS, st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);                                 \
}                                                                                                       \
void nea##N##_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],     \
                     const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)         \
{                                                                                                       \
    if ((length + 7) / 8 < L3_MIN_BYTES) nea##N##_256_l2(count, bearer, direction, extra_iv, key, ibs, length, obs); \
    else nea##N##_big(count, bearer, direction, extra_iv, key, ibs, length, obs);                       \
}                                                                                                       \
__attribute__((noinline)) static void nia##N##_big(uint32_t count, uint8_t bearer, uint8_t direction, \
        const uint8_t extra_iv[6], const uint8_t key[32], const uint8_t *msg, uint32_t length,          \
        uint8_t mac_bytes, uint8_t *mac)                                                                \
{                                                                                                       \
    _Alignas(64) uint8_t st[KSG2_STATE_MAX]; uint8_t iv[16];                                            \
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);                              \
    aead1_l3(&OPS, st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);                         \
}                                                                                                       \
void nia##N##_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],     \
                     const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes,    \
                     uint8_t *mac)                                                                      \
{                                                                                                       \
    if ((length + 7) / 8 < L3_MIN_BYTES) nia##N##_256_l2(count, bearer, direction, extra_iv, key, msg, length, mac_bytes, mac); \
    else nia##N##_big(count, bearer, direction, extra_iv, key, msg, length, mac_bytes, mac);            \
}                                                                                                       \
__attribute__((noinline)) static int nca##N##_big(int mode, uint32_t count, uint8_t bearer,           \
        uint8_t direction, const uint8_t extra_iv[6], const uint8_t key[32], const uint8_t *aad,        \
        uint32_t aad_length, const uint8_t *ibs, uint32_t s_length, uint8_t *obs,                       \
        uint8_t mac_bytes, uint8_t *mac)                                                                \
{                                                                                                       \
    _Alignas(64) uint8_t st[KSG2_STATE_MAX]; uint8_t iv[16];                                            \
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);                              \
    aead1_l3(&OPS, st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);             \
    return 0;                                                                                           \
}                                                                                                       \
int nca##N##_256_l3(int mode, uint32_t count, uint8_t bearer, uint8_t direction,                       \
                    const uint8_t extra_iv[6], const uint8_t key[32], const uint8_t *aad,              \
                    uint32_t aad_length, const uint8_t *ibs, uint32_t s_length, uint8_t *obs,          \
                    uint8_t mac_bytes, uint8_t *mac)                                                    \
{                                                                                                       \
    if ((s_length + 7) / 8 + (aad_length + 7) / 8 < L3_MIN_BYTES)                                       \
        return nca##N##_256_l2(mode, count, bearer, direction, extra_iv, key, aad, aad_length,          \
                               ibs, s_length, obs, mac_bytes, mac);                                     \
    return nca##N##_big(mode, count, bearer, direction, extra_iv, key, aad, aad_length,                 \
                        ibs, s_length, obs, mac_bytes, mac);                                            \
}

DEFINE_SET(5, KSG3_AES)
DEFINE_SET(4, KSG2_SNOW)
DEFINE_SET(6, KSG2_ZUC)
DEFINE_SET(6ct, KSG2_ZUC_CT)
