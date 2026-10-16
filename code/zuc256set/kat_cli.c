/*
 * kat_cli.c - Command-line front end used by test_official.py (TS 35.247).
 *
 *   ./kat_cli pdot A B         -> pdot(A, B)
 *   ./kat_cli state KEY IV     -> state after initialisation (P = 48 rounds plus the
 *                                 final work-mode clock): 16 LFSR words, R1, R2
 *   ./kat_cli ks KEY IV N      -> N 32-bit keystream words after initialisation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/polyval.h"
#include "zuc256.h"

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
    if ((argc == 4 && !strcmp(argv[1], "state")) || (argc == 5 && !strcmp(argv[1], "ks"))) {
        uint8_t key[32], iv[16], z[4];
        zuc256_state st;
        unhex(argv[2], key, 32); unhex(argv[3], iv, 16);
        zuc256_init(&st, key, iv);
        if (!strcmp(argv[1], "state")) {
            for (int i = 0; i < 16; i++) printf("%08x", st.s[i]);
            printf("\n%08x\n%08x\n", st.r1, st.r2);
        } else {
            for (int i = 0; i < atoi(argv[4]); i++) { zuc256_word(&st, z); phex(z, 4); }
        }
        return 0;
    }
    fprintf(stderr, "usage: kat_cli pdot A B | state KEY IV | ks KEY IV N\n");
    return 2;
}
