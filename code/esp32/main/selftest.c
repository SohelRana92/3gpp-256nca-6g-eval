/*
 * selftest.c - Correctness checks that run on the ESP32 before any timing.
 *
 * 1. Cross-platform known answers. On a PC, the L1 code passes the official
 *    3GPP test data and matches the Annex reference code. We run the same L1
 *    code there on fixed pseudo-random cases (lengths in bits, AAD, all tag
 *    sizes, decryption) and store one 64-bit digest of all outputs per
 *    algorithm in golden.h. The ESP32 build of L1 must reproduce every digest.
 * 2. L2 (portable 32-bit) and HW (AES accelerator) must give bit-identical
 *    results to L1 on random cases, and the L2 POLYVAL dot product must
 *    match L1.
 *
 * Built for the PC with -DR4_GEN_GOLDEN to print golden.h.
 */
#include <stdio.h>
#include <string.h>
#include "selftest.h"
#include "../../aes256set/nxa5.h"
#include "../../snow5gset/nxa4.h"
#include "../../zuc256set/nxa6.h"
#include "../../common/polyval.h"
#ifndef R4_GEN_GOLDEN
#include "../../l2x86/nxa_l2.h"
#include "nxa5_hw.h"
#include "golden.h"
#endif

#define MAXB 2048          /* bytes per case: keeps L1 on the ESP32 to a few seconds */
#define NGOLD 64           /* fixed cases per set for the digests */

typedef void (*nea_f)(uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t, uint8_t *);
typedef void (*nia_f)(uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t, uint8_t, uint8_t *);
typedef int  (*nca_f)(int, uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t,
                      const uint8_t *, uint32_t, uint8_t *, uint8_t, uint8_t *);

static const nea_f E1[3] = {nea5_256, nea4_256, nea6_256};
static const nia_f I1[3] = {nia5_256, nia4_256, nia6_256};
static const nca_f C1[3] = {nca5_256, nca4_256, nca6_256};

static uint32_t S;                                      /* xorshift32 */
static uint32_t rnd(void) { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
static void fill(uint8_t *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static uint32_t pick_bits(void)
{
    static const uint32_t edges[] = {0, 1, 15, 16, 17, 127, 128, 129, 511, 512, 513, 1023, 1024, 1500, 2047, 2048};
    uint32_t r = rnd() % 4;
    uint32_t bytes = r == 0 ? edges[rnd() % (sizeof edges / sizeof edges[0])] : r == 1 ? rnd() % 64 : rnd() % MAXB;
    if (bytes > MAXB) bytes = MAXB;
    uint32_t bits = 8 * bytes;
    if (bits && rnd() % 3 == 0) bits -= 1 + rnd() % 7;  /* partial last byte */
    return bits;
}

static uint32_t W_IN[(MAXB + 16) / 4], W_AAD[(MAXB + 16) / 4], W_O1[(MAXB + 16) / 4], W_O2[(MAXB + 16) / 4];
#define IN ((uint8_t *)W_IN)
#define AADB ((uint8_t *)W_AAD)
#define O1 ((uint8_t *)W_O1)
#define O2 ((uint8_t *)W_O2)

typedef struct { uint8_t key[32], x[6], br, dir, mb; uint32_t cnt, bits, abits; } case_t;

static void make_case(case_t *c)
{
    fill(c->key, 32); fill(c->x, 6);
    c->cnt = rnd(); c->br = (uint8_t)(rnd() & 31); c->dir = (uint8_t)(rnd() & 1); c->mb = (uint8_t)(4 + rnd() % 13);
    c->bits = pick_bits(); c->abits = rnd() % 4 ? pick_bits() % 1024 : 0;
    fill(IN, MAXB); fill(AADB, MAXB);
}

static uint64_t fnv(uint64_t h, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
    return h;
}

void r4_l1_digests(uint64_t d[9])
{
    for (int s = 0; s < 3; s++) {
        uint64_t he = 0xcbf29ce484222325ull, hi = he, hc = he;
        S = 0x52340001u + (uint32_t)s;
        for (int t = 0; t < NGOLD; t++) {
            case_t c; make_case(&c);
            uint32_t nb = (c.bits + 7) / 8;
            uint8_t m[16];
            E1[s](c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, O1);
            he = fnv(he, O1, nb);
            I1[s](c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, c.mb, m);
            hi = fnv(hi, m, c.mb);
            C1[s](0, c.cnt, c.br, c.dir, c.x, c.key, AADB, c.abits, IN, c.bits, O1, c.mb, m);
            hc = fnv(fnv(hc, O1, nb), m, c.mb);
            C1[s](1, c.cnt, c.br, c.dir, c.x, c.key, AADB, c.abits, O1, c.bits, O2, c.mb, m);   /* decrypt */
            hc = fnv(fnv(hc, O2, nb), m, c.mb);
        }
        d[3 * s] = he; d[3 * s + 1] = hi; d[3 * s + 2] = hc;
    }
}

#ifdef R4_GEN_GOLDEN
int main(void)
{
    uint64_t d[9];
    r4_l1_digests(d);
    printf("/* golden.h - generated on a PC by gen_golden (L1 code, which passes the official\n"
           " * 3GPP test data there). Order: NEA5 NIA5 NCA5 NEA4 NIA4 NCA4 NEA6 NIA6 NCA6. */\n");
    printf("static const uint64_t GOLDEN[9] = {\n");
    for (int i = 0; i < 9; i++) printf("    0x%016llxull,\n", (unsigned long long)d[i]);
    printf("};\n");
    return 0;
}
#else
static const char *const NAMES[3] = {"AES-256 (NxA5)", "Snow 5G (NxA4)", "ZUC-256 (NxA6)"};
static const nea_f E2[3] = {nea5_256_l2, nea4_256_l2, nea6_256_l2};
static const nia_f I2[3] = {nia5_256_l2, nia4_256_l2, nia6_256_l2};
static const nca_f C2[3] = {nca5_256_l2, nca4_256_l2, nca6_256_l2};

/* Compare one implementation (e, i, c) with L1 on random cases. Returns failures. */
static int equiv(int s, nea_f e, nia_f i, nca_f c2, int trials, uint32_t seed, int *fe, int *fi, int *fc)
{
    static const uint8_t pad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    S = seed;
    *fe = *fi = *fc = 0;
    for (int t = 0; t < trials; t++) {
        case_t c; make_case(&c);
        uint32_t nb = (c.bits + 7) / 8;
        uint8_t m1[16], m2[16], m3[16];
        memset(O1, 0xAA, MAXB + 16); memset(O2, 0xAA, MAXB + 16);
        E1[s](c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, O1);
        e(c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, O2);
        if (memcmp(O1, O2, nb + 16)) (*fe)++;
        I1[s](c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, c.mb, m1);
        i(c.cnt, c.br, c.dir, c.x, c.key, IN, c.bits, c.mb, m2);
        if (memcmp(m1, m2, c.mb)) (*fi)++;
        memset(O1, 0xAA, MAXB + 16); memset(O2, 0xAA, MAXB + 16);
        C1[s](0, c.cnt, c.br, c.dir, c.x, c.key, AADB, c.abits, IN, c.bits, O1, c.mb, m1);
        c2(0, c.cnt, c.br, c.dir, c.x, c.key, AADB, c.abits, IN, c.bits, O2, c.mb, m2);
        if (memcmp(O1, O2, nb + 16) || memcmp(m1, m2, c.mb)) (*fc)++;
        c2(1, c.cnt, c.br, c.dir, c.x, c.key, AADB, c.abits, O1, c.bits, O2, c.mb, m3);   /* decrypt L1 output */
        if (nb && (memcmp(O2, IN, nb - 1) || O2[nb - 1] != (IN[nb - 1] & pad[c.bits % 8]))) (*fc)++;
        if (memcmp(m1, m3, c.mb)) (*fc)++;
    }
    return *fe + *fi + *fc;
}

static const char *ok(int f) { return f ? "MISMATCH" : "MATCH"; }

int r4_selftest(int trials)
{
    int bad = 0;
    static const char *const ALG[9] = {"256-NEA5", "256-NIA5", "256-NCA5", "256-NEA4", "256-NIA4",
                                       "256-NCA4", "256-NEA6", "256-NIA6", "256-NCA6"};
    uint64_t d[9];
    r4_l1_digests(d);
    int gbad = 0;
    for (int i = 0; i < 9; i++) if (d[i] != GOLDEN[i]) { gbad++; printf("# L1 %s digest MISMATCH\n", ALG[i]); }
    printf("# L1 vs PC known answers (%d fixed cases per set, 9 algorithms): %s\n", NGOLD, ok(gbad));
    bad += gbad;

    int pbad = 0;
    S = 777;
    for (int t = 0; t < 2000; t++) {
        uint8_t a[16], b[16], r1[16], r2[16];
        fill(a, 16); fill(b, 16);
        polyval_dot(r1, a, b); polyval_dot_l2(r2, a, b);
        if (memcmp(r1, r2, 16)) pbad++;
    }
    printf("# POLYVAL dot L2 vs L1: %s (2000 trials)\n", ok(pbad));
    bad += pbad;

    for (int s = 0; s < 3; s++) {
        int fe, fi, fc;
        bad += equiv(s, E2[s], I2[s], C2[s], trials, 1000u + (uint32_t)s, &fe, &fi, &fc);
        printf("# %s L2 vs L1: NEA %s, NIA %s, NCA %s (%d trials each)\n", NAMES[s], ok(fe), ok(fi), ok(fc), trials);
    }
#ifdef ESP_PLATFORM
    {
        int fe, fi, fc;
        bad += equiv(0, nea5_256_hw, nia5_256_hw, nca5_256_hw, trials, 2000u, &fe, &fi, &fc);
        printf("# %s HW vs L1: NEA %s, NIA %s, NCA %s (%d trials each)\n", NAMES[0], ok(fe), ok(fi), ok(fc), trials);
    }
#endif
    printf(bad ? "# SELF-TEST FAILED\n" : "# ALL ESP32 SELF-TESTS PASS\n");
    return bad;
}
#endif
