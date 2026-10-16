/*
 * test_common.c - Known-answer tests for the shared building blocks.
 *
 *   ./test_common            run the published known-answer tests
 *   ./test_common aes  K P   print AES-256_K(P)      (hex in, hex out)
 *   ./test_common pv   H X   print POLYVAL(H, X)     (X = n whole blocks)
 *
 * The CLI modes let verify_common.py cross-check against independent
 * implementations on random inputs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aes256.h"
#include "polyval.h"

static size_t unhex(const char *s, uint8_t *out, size_t max)
{
    size_t n = strlen(s) / 2;
    if (n > max) { fprintf(stderr, "input too long\n"); exit(2); }
    for (size_t i = 0; i < n; i++) { unsigned v; sscanf(s + 2 * i, "%2x", &v); out[i] = (uint8_t)v; }
    return n;
}

static void phex(const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) printf("%02x", b[i]); printf("\n"); }

static int check(const char *name, const uint8_t *got, const char *want_hex)
{
    uint8_t want[16];
    unhex(want_hex, want, 16);
    int ok = memcmp(got, want, 16) == 0;
    printf("%-44s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) { printf("  got  "); phex(got, 16); printf("  want %s\n", want_hex); }
    return ok;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "aes") == 0) {
        uint8_t k[32], p[16], c[16]; aes256_ctx ctx;
        unhex(argv[2], k, 32); unhex(argv[3], p, 16);
        aes256_init(&ctx, k); aes256_encrypt_block(&ctx, p, c); phex(c, 16);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "pv") == 0) {
        static uint8_t x[1 << 16]; uint8_t h[16], r[16];
        unhex(argv[2], h, 16);
        size_t n = unhex(argv[3], x, sizeof x);
        polyval(r, h, x, n / 16); phex(r, 16);
        return 0;
    }

    int ok = 1;

    /* FIPS 197, Appendix C.3 (AES-256). */
    {
        uint8_t k[32], p[16], c[16]; aes256_ctx ctx;
        unhex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", k, 32);
        unhex("00112233445566778899aabbccddeeff", p, 16);
        aes256_init(&ctx, k); aes256_encrypt_block(&ctx, p, c);
        ok &= check("AES-256  FIPS 197 Appendix C.3", c, "8ea2b7ca516745bfeafc49904b496089");
    }

    /* RFC 8452, Appendix A (POLYVAL worked example). */
    {
        uint8_t h[16], x[32], r[16];
        unhex("25629347589242761d31f826ba4b757b", h, 16);
        unhex("4f4f95668c83dfb6401762bb2d01a262", x, 16);
        unhex("d1a24ddd2721d006bbe45f20d3c9f362", x + 16, 16);
        polyval(r, h, x, 2);
        ok &= check("POLYVAL  RFC 8452 Appendix A", r, "f7a3b47b846119fae5b7866cf5e5b77e");
    }

    printf("%s\n", ok ? "ALL KNOWN-ANSWER TESTS PASS" : "SOME TESTS FAILED");
    return ok ? 0 : 1;
}
