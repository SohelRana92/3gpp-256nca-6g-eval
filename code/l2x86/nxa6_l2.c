/*
 * nxa6_l2.c - 256-NEA6 / 256-NIA6 / 256-NCA6: ZUC-256 (P = 48), L2.
 *
 * ZUC is a 32-bit scalar design, so L2 keeps it scalar and removes the
 * overheads of L1: the LFSR is a ring of 16 cells addressed with constant
 * offsets inside a 16-step unrolled loop (no shifting), the mod 2^31-1 sum
 * is one 64-bit accumulation with two folds, the S-box layer uses four
 * pre-shifted 32-bit tables, and words are made sixteen at a time.
 */
#include "aead1_l2.h"   /* portable C: also built for ARM (../l2arm) */
#include "nxa_l2.h"
#include "../zuc256set/zuc_sbox.h"
#include <string.h>

/* Built a second time as nxa6_l2ct.c with R4_ZUC_CT defined: the same code with a
 * constant-time S-box layer (vector table look-ups) and the names *6ct_256_l2. */
#ifdef R4_ZUC_CT
#define KSG2_ZUC    KSG2_ZUC_CT
#define nea6_256_l2 nea6ct_256_l2
#define nia6_256_l2 nia6ct_256_l2
#define nca6_256_l2 nca6ct_256_l2
#include "zuc_sbox_ct.h"
#endif

#define M31 0x7FFFFFFFu
#define ROT31(x, k) ((((x) << (k)) | ((x) >> (31 - (k)))) & M31)
#define ROTL(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

typedef struct {
    uint32_t s[16], r1, r2;
    uint8_t buf[64];          /* 16 keystream words, big-endian */
    unsigned pos;             /* next unused byte of buf (64 = empty) */
} zuc_st;

/* S-box layer with pre-shifted 32-bit tables (built once from S0 and S1). */
#ifdef R4_ZUC_CT
static inline __attribute__((always_inline)) uint32_t sbox(uint32_t x) { return zuc_sbox_ct(x); }
#else
static uint32_t T0[256], T1[256], T2[256], T3[256];
__attribute__((constructor)) static void build_tables(void)
{
    for (int i = 0; i < 256; i++) {
        T0[i] = (uint32_t)ZUC_S0[i] << 24; T1[i] = (uint32_t)ZUC_S1[i] << 16;
        T2[i] = (uint32_t)ZUC_S0[i] << 8;  T3[i] = ZUC_S1[i];
    }
}
static inline uint32_t sbox(uint32_t x)
{
    return T0[x >> 24] | T1[(x >> 16) & 0xFF] | T2[(x >> 8) & 0xFF] | T3[x & 0xFF];
}
#endif
static inline uint32_t l1(uint32_t x) { return x ^ ROTL(x, 2) ^ ROTL(x, 10) ^ ROTL(x, 18) ^ ROTL(x, 24); }
static inline uint32_t l2(uint32_t x) { return x ^ ROTL(x, 8) ^ ROTL(x, 14) ^ ROTL(x, 22) ^ ROTL(x, 30); }

/* One step at ring offset k: logical cell j is s[(k + j) & 15]. Returns W ^ X3.
 * In initialisation mode W >> 1 enters the feedback. The new cell replaces s_0. */
static inline __attribute__((always_inline)) uint32_t step(uint32_t *s, uint32_t *r1, uint32_t *r2, int k, int init)
{
#define C(j) s[((k) + (j)) & 15]
    uint32_t x0 = ((C(15) & 0x7FFF8000u) << 1) | (C(14) & 0xFFFFu);
    uint32_t x1 = ((C(11) & 0xFFFFu) << 16) | (C(9) >> 15);
    uint32_t x2 = ((C(7) & 0xFFFFu) << 16) | (C(5) >> 15);
    uint32_t x3 = ((C(2) & 0xFFFFu) << 16) | (C(0) >> 15);
    uint32_t w = (x0 ^ *r1) + *r2;
    uint32_t w1 = *r1 + x1, w2 = *r2 ^ x2;
    *r1 = sbox(l1((w1 << 16) | (w2 >> 16)));
    *r2 = sbox(l2((w2 << 16) | (w1 >> 16)));
    uint64_t f = (uint64_t)C(0) + ROT31(C(0), 8) + ROT31(C(4), 20) + ROT31(C(10), 21) + ROT31(C(13), 17) + ROT31(C(15), 15);
    if (init) f += w >> 1;
    f = (f & M31) + (f >> 31);
    f = (f & M31) + (f >> 31);
    C(0) = f ? (uint32_t)f : M31;
    return w ^ x3;
#undef C
}

static void gen16(zuc_st *z)
{
    uint32_t r1 = z->r1, r2 = z->r2;
#pragma GCC unroll 16
    for (int k = 0; k < 16; k++) {
        uint32_t w = step(z->s, &r1, &r2, k, 0);
        z->buf[4 * k] = (uint8_t)(w >> 24); z->buf[4 * k + 1] = (uint8_t)(w >> 16);
        z->buf[4 * k + 2] = (uint8_t)(w >> 8); z->buf[4 * k + 3] = (uint8_t)w;
    }
    z->r1 = r1; z->r2 = r2;
    z->pos = 0;
}

static const uint8_t D[16] = {0x64, 0x43, 0x7b, 0x2a, 0x11, 0x05, 0x51, 0x42,
                              0x1a, 0x31, 0x18, 0x66, 0x14, 0x2e, 0x01, 0x5c};
#define CELL(k, d, b2, b3) (((uint32_t)(k) << 23) | ((uint32_t)(d) << 16) | ((uint32_t)(b2) << 8) | (b3))

static void zuc_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    zuc_st *z = (zuc_st *)p;
    uint32_t *s = z->s;
    for (int i = 0; i < 7; i++)  s[i] = CELL(key[i], D[i], key[16 + i], key[24 + i]);
    for (int i = 7; i < 15; i++) s[i] = CELL(key[i], D[i], iv[i - 7], iv[i + 1]);
    s[15] = CELL(key[15], D[15], key[23], key[31]);
    uint32_t r1 = 0, r2 = 0;
    for (int round = 0; round < 48; round += 16) {          /* P = 48 = 3 x 16 */
#pragma GCC unroll 16
        for (int k = 0; k < 16; k++) (void)step(s, &r1, &r2, k, 1);
    }
    /* Final clock: F once (output discarded), then one work-mode LFSR clock.
     * Then rotate the ring so that logical cell 0 is s[0] again. */
    (void)step(s, &r1, &r2, 0, 0);
    uint32_t first = s[0];
    memmove(s, s + 1, 15 * sizeof(uint32_t));
    s[15] = first;
    z->r1 = r1; z->r2 = r2;
    z->pos = 64;
}

static void take(zuc_st *z, uint8_t *out, size_t nbytes)    /* nbytes is a multiple of 16 */
{
    while (nbytes) {
        if (z->pos == 64) gen16(z);
        memcpy(out, z->buf + z->pos, 16);
        z->pos += 16; out += 16; nbytes -= 16;
    }
}

static void zuc_hqp(void *p, uint8_t hqp[48]) { take((zuc_st *)p, hqp, 48); }

static void zuc_blocks(void *p, uint8_t *out, size_t n)
{
    zuc_st *z = (zuc_st *)p;
    while (n && z->pos != 64) { take(z, out, 16); out += 16; n--; }   /* drain the buffer */
    while (n >= 4) {                                                   /* whole batches straight to out */
        gen16(z);
        memcpy(out, z->buf, 64);
        z->pos = 64; out += 64; n -= 4;
    }
    if (n) take(z, out, 16 * n);
}

const ksg2_ops KSG2_ZUC = {zuc_init, zuc_hqp, zuc_blocks};

void nea6_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    zuc_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_ZUC, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia6_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    zuc_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG2_ZUC, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca6_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    zuc_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG2_ZUC, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
