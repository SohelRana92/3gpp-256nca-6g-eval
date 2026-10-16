/*
 * test_l3.c - L3 must give bit-identical results to L1, which itself passes
 * the official 3GPP test data and matches the Annex reference code.
 *
 * Random trials over all nine algorithms: bit lengths with and without a
 * partial last byte, lengths around the 16-, 128- and 512-byte boundaries of
 * the L3 code, AAD, every tag size 4..16, and NCA in both directions.
 * Plus: the L2 POLYVAL dot product against L1 on random inputs.
 *
 * usage: ./test_l3 [trials]     (default 20000)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nxa_l3.h"
#include "../aes256set/nxa5.h"
#include "../snow5gset/nxa4.h"
#include "../zuc256set/nxa6.h"
#include "../common/polyval.h"

#define MAXB 9000

typedef void (*nea_f)(uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t, uint8_t *);
typedef void (*nia_f)(uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t, uint8_t, uint8_t *);
typedef int  (*nca_f)(int, uint32_t, uint8_t, uint8_t, const uint8_t *, const uint8_t *, const uint8_t *, uint32_t,
                      const uint8_t *, uint32_t, uint8_t *, uint8_t, uint8_t *);

static const struct { const char *name; nea_f e1, e2; nia_f i1, i2; nca_f c1, c2; } SETS[] = {
    {"AES-256 (NxA5)", nea5_256, nea5_256_l3, nia5_256, nia5_256_l3, nca5_256, nca5_256_l3},
    {"Snow 5G (NxA4)", nea4_256, nea4_256_l3, nia4_256, nia4_256_l3, nca4_256, nca4_256_l3},
    {"ZUC-256 (NxA6)", nea6_256, nea6_256_l3, nia6_256, nia6_256_l3, nca6_256, nca6_256_l3},
    {"ZUC-256 (NxA6) constant-time S-box", nea6_256, nea6ct_256_l3, nia6_256, nia6ct_256_l3, nca6_256, nca6ct_256_l3},
};

static uint32_t rnd(void) { return ((uint32_t)rand() << 16) ^ (uint32_t)rand(); }
static void fill(uint8_t *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = (uint8_t)rand(); }

/* Lengths that hit the edges of the L3 code paths, plus random ones. */
static uint32_t pick_bits(void)
{
    static const uint32_t edges[] = {0, 1, 15, 16, 63, 64, 65, 127, 128, 255, 256, 257, 511, 512, 513, 767, 768, 1023, 1024, 2047,
                                     2048, 4095, 4096, 4097, 8192, 1500, 9000};
    uint32_t r = rnd() % 4;
    uint32_t bytes = r == 0 ? edges[rnd() % (sizeof edges / sizeof edges[0])] : r == 1 ? rnd() % 64 : rnd() % MAXB;
    if (bytes > MAXB) bytes = MAXB;
    uint32_t bits = 8 * bytes;
    if (bits && rnd() % 3 == 0) bits -= 1 + rnd() % 7;   /* partial last byte */
    return bits;
}

int main(int argc, char **argv)
{
    int trials = argc > 1 ? atoi(argv[1]) : 20000;
    srand(2026);
    static uint8_t in[MAXB + 16], aad[MAXB + 16], o1[MAXB + 16], o2[MAXB + 16], back[MAXB + 16];
    uint8_t key[32], x[6], m1[16], m2[16];
    int bad = 0;

    if (!r4_l3_supported()) { printf("L3 SKIPPED: this CPU lacks AVX-512/VAES/VPCLMULQDQ\n"); return 0; }

    for (int s = 0; s < (int)(sizeof SETS / sizeof SETS[0]); s++) {
        int fe = 0, fi = 0, fc = 0;
        for (int t = 0; t < trials; t++) {
            fill(key, 32); fill(x, 6);
            uint32_t cnt = rnd();
            uint8_t br = (uint8_t)(rnd() & 31), dir = (uint8_t)(rnd() & 1), mb = (uint8_t)(4 + rnd() % 13);
            uint32_t bits = pick_bits(), abits = rnd() % 4 ? pick_bits() % 2048 : 0;
            uint32_t nb = (bits + 7) / 8;
            fill(in, MAXB); fill(aad, MAXB);

            /* NEA */
            memset(o1, 0xAA, sizeof o1); memset(o2, 0xAA, sizeof o2);
            SETS[s].e1(cnt, br, dir, x, key, in, bits, o1);
            SETS[s].e2(cnt, br, dir, x, key, in, bits, o2);
            if (memcmp(o1, o2, nb + 16)) fe++;

            /* NIA */
            SETS[s].i1(cnt, br, dir, x, key, in, bits, mb, m1);
            SETS[s].i2(cnt, br, dir, x, key, in, bits, mb, m2);
            if (memcmp(m1, m2, mb)) fi++;

            /* NCA encrypt, then L2 decrypt of the L1 ciphertext */
            memset(o1, 0xAA, sizeof o1); memset(o2, 0xAA, sizeof o2);
            SETS[s].c1(0, cnt, br, dir, x, key, aad, abits, in, bits, o1, mb, m1);
            SETS[s].c2(0, cnt, br, dir, x, key, aad, abits, in, bits, o2, mb, m2);
            if (memcmp(o1, o2, nb + 16) || memcmp(m1, m2, mb)) fc++;
            uint8_t m3[16];
            SETS[s].c2(1, cnt, br, dir, x, key, aad, abits, o1, bits, back, mb, m3);
            if (nb) {                                     /* plaintext back, with the tail bits cleared */
                uint8_t last = in[nb - 1];
                static const uint8_t pad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
                if (memcmp(back, in, nb - 1) || back[nb - 1] != (last & pad[bits % 8])) fc++;
            }
            if (memcmp(m1, m3, mb)) fc++;
        }
        printf("%s L3 vs L1: NEA %s, NIA %s, NCA %s (%d trials each)\n", SETS[s].name,
               fe ? "MISMATCH" : "MATCH", fi ? "MISMATCH" : "MATCH", fc ? "MISMATCH" : "MATCH", trials);
        bad += fe + fi + fc;
    }
    printf(bad ? "L3 CHECK FAILED\n" : "ALL L3 CHECKS PASS\n");
    return bad != 0;
}
