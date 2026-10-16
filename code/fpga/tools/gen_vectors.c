/*
 * gen_vectors.c - test vectors for the FPGA designs, from the verified L1 code.
 *
 * L1 passes the official 3GPP test data and matches the Annex reference code.
 * We run it on fixed pseudo-random packets (NEA, NIA, NCA encrypt and decrypt;
 * lengths in bits, with and without AAD) and write one text file per set:
 *   P mac_en mode aad_bits msg_bits
 *   K key (32 bytes, byte 0 first)      I iv (16 bytes)
 *   A aad block (16 bytes) ...           M input block ...
 *   C expected output block ...          T expected 16-byte tag (if mac_en)
 *   E (end of packet)
 * Blocks are zero-padded; the hardware masks the unused bits itself.
 *
 * usage: ./gen_vectors <outdir>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../aes256set/nxa5.h"
#include "../../snow5gset/nxa4.h"
#include "../../zuc256set/nxa6.h"
#include "../../common/aead1.h"


static uint32_t S = 0x2026A5u;
static uint32_t rnd(void) { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
static void fill(uint8_t *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static void hexline(FILE *f, char tag, const uint8_t *p, size_t n)
{
    fprintf(f, "%c ", tag);
    for (size_t i = 0; i < n; i++) fprintf(f, "%02x", p[i]);
    fprintf(f, "\n");
}

static void blocks(FILE *f, char tag, const uint8_t *p, uint32_t bits)
{
    uint32_t nb = (bits + 7) / 8;
    for (uint32_t o = 0; o < nb; o += 16) {
        uint8_t b[16] = {0};
        memcpy(b, p + o, nb - o < 16 ? nb - o : 16);
        hexline(f, tag, b, 16);
    }
}

#define MAXB 16400

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int timing = argc > 2;                  /* second argument: write timing_<set>.txt instead */
    const char *names[3] = {"nca5", "nca4", "nca6"};
    static const uint32_t LENS[] = {0, 1, 7, 8, 15, 16, 17, 31, 32, 33, 40, 63, 64, 100, 127, 128, 129, 255, 256,
                                    257, 511, 512, 520, 1000, 1023, 1024, 1500 * 8 / 8};
    int total = 0;
    for (int s = 0; s < 3; s++) {
        char path[512];
        snprintf(path, sizeof path, timing ? "%s/timing_%s.txt" : "%s/vectors_%s.txt", dir, names[s]);
        FILE *f = fopen(path, "w");
        if (!f) { perror(path); return 1; }
        ksg_t k = s == 0 ? ksg_aes256_new() : s == 1 ? ksg_snow5g_new() : ksg_zuc256_new();
        int n = 0;
        static const int TK[7] = {2, 2, 2, 2, 2, 0, 1};               /* timing: kind per packet */
        static const uint32_t TB[7] = {0, 40, 576, 1500, 16384, 16384, 1500};
        for (int t = 0; t < (timing ? 7 : 120); t++) {
            uint8_t key[32], x[6], iv[16], aad[MAXB], in[MAXB], out[MAXB + 16], tag[16];
            fill(key, 32); fill(x, 6); fill(aad, MAXB); fill(in, MAXB);
            int kind = timing ? TK[t] : t % 4;      /* 0 NEA, 1 NIA, 2 NCA enc, 3 NCA dec */
            uint32_t bytes = timing ? TB[t] : t < 4 * 27 ? LENS[(t / 4) % 27] : 1 + rnd() % 1500;
            if (!timing && bytes > 200 && t % 3) bytes = 1 + rnd() % 200;   /* keep simulation time small */
            uint32_t bits = 8 * bytes;
            if (!timing && bits && t % 5 == 1) bits -= 1 + rnd() % 7;
            uint32_t abits = !timing && kind >= 2 && t % 3 ? 8 * (rnd() % 70) - (t % 7 == 0 && t % 3 ? 3 : 0) : 0;
            if ((int32_t)abits < 0) abits = 0;
            uint32_t cnt = rnd(); uint8_t br = rnd() & 31, dir_ = rnd() & 1;
            int mac_en = kind != 0, mode = kind == 3;
            memset(out, 0, sizeof out); memset(tag, 0, 16);
            if (kind == 0) {
                make_5giv(iv, cnt, br, dir_, 0, 0, 0, 0, x);
                aead1(&k, key, iv, 0, 0, NULL, 0, in, bits, out, NULL);
            } else if (kind == 1) {
                make_5giv(iv, cnt, br, dir_, 16, 0, 0, 0, x);
                aead1(&k, key, iv, 0, 16, in, bits, NULL, 0, NULL, tag);
            } else {
                make_5giv(iv, cnt, br, dir_, 16, 1, 0, 0, x);
                aead1(&k, key, iv, mode, 16, aad, abits, in, bits, out, tag);
            }
            uint32_t a_bits = kind == 1 ? bits : abits, m_bits = kind == 1 ? 0 : bits;
            fprintf(f, "P %d %d %u %u\n", mac_en, mode, a_bits, m_bits);
            hexline(f, 'K', key, 32);
            hexline(f, 'I', iv, 16);
            blocks(f, 'A', kind == 1 ? in : aad, a_bits);
            blocks(f, 'M', in, m_bits);
            blocks(f, 'C', out, m_bits);
            if (mac_en) hexline(f, 'T', tag, 16);
            fprintf(f, "E\n");
            n++;
        }
        fprintf(f, "Z\n");
        fclose(f);
        printf("%s: %d packets\n", path, n);
        total += n;
    }
    return total == 0;
}
