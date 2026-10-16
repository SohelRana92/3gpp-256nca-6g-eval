/*
 * tab_l2p.c - AES S-box and T-tables for the portable L2, built in RAM.
 * The S-box is computed from its definition (FIPS 197, 5.1.1): the
 * multiplicative inverse in GF(2^8), then the affine map with constant 0x63.
 */
#include "tab_l2p.h"

uint32_t L2P_TE[4][256];
uint8_t  L2P_SBOX[256];

static uint8_t xt(uint8_t a) { return (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1B : 0)); }

void l2p_tables_init(void)
{
    static int done;
    if (done) return;
    /* p runs through powers of 3, q through powers of 3^-1, so q = p^-1. */
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t)(p ^ xt(p));                              /* p *= 3 */
        q ^= (uint8_t)(q << 1); q ^= (uint8_t)(q << 2); q ^= (uint8_t)(q << 4);
        if (q & 0x80) q ^= 0x09;                              /* q /= 3 */
        uint8_t x = (uint8_t)(q ^ (uint8_t)((q << 1) | (q >> 7)) ^ (uint8_t)((q << 2) | (q >> 6))
                              ^ (uint8_t)((q << 3) | (q >> 5)) ^ (uint8_t)((q << 4) | (q >> 4)));
        L2P_SBOX[p] = (uint8_t)(x ^ 0x63);
    } while (p != 1);
    L2P_SBOX[0] = 0x63;
    for (int i = 0; i < 256; i++) {
        uint8_t s = L2P_SBOX[i], s2 = xt(s), s3 = (uint8_t)(s2 ^ s);
        uint32_t w = (uint32_t)s2 | ((uint32_t)s << 8) | ((uint32_t)s << 16) | ((uint32_t)s3 << 24);
        for (int r = 0; r < 4; r++) L2P_TE[r][i] = r ? (w << (8 * r)) | (w >> (32 - 8 * r)) : w;
    }
    done = 1;
}

__attribute__((constructor)) static void l2p_tables_ctor(void) { l2p_tables_init(); }
