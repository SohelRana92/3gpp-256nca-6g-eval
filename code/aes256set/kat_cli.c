/*
 * kat_cli.c - Small command-line front end used by test_official.py to run
 * the official implementers' test data (TS 35.244) through the R4 code.
 *
 *   ./kat_cli pdot A B             -> pdot(A, B)                (16-byte hex in/out)
 *   ./kat_cli aesks KEY IV12 N     -> N keystream blocks of the AES-256 KSG,
 *                                     one per line, counter starting at 0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/polyval.h"
#include "nxa5.h"

static void unhex(const char *s, uint8_t *o, size_t n)
{
    if (strlen(s) != 2 * n) { fprintf(stderr, "bad length for %s\n", s); exit(2); }
    for (size_t i = 0; i < n; i++) { unsigned v; sscanf(s + 2 * i, "%2x", &v); o[i] = (uint8_t)v; }
}
static void phex(const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) printf("%02x", b[i]); printf("\n"); }

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "pdot")) {
        uint8_t a[16], b[16], r[16];
        unhex(argv[2], a, 16); unhex(argv[3], b, 16);
        polyval_dot(r, a, b); phex(r, 16);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "aesks")) {
        uint8_t key[32], iv[16] = {0}, z[16];
        unhex(argv[2], key, 32); unhex(argv[3], iv, 12);
        ksg_t k = ksg_aes256_new();
        k.initialise(k.state, key, iv);       /* the KSG sets bytes 12..15 (CTR) to 0 */
        for (int i = 0; i < atoi(argv[4]); i++) { k.keystream(k.state, z); phex(z, 16); }
        ksg_aes256_free(&k);
        return 0;
    }
    fprintf(stderr, "usage: kat_cli pdot A B | kat_cli aesks KEY IV12 N\n");
    return 2;
}
