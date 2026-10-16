/*
 * zuc256.h - The ZUC-256 stream cipher core as specified by 3GPP
 *            (TS 35.246, clause 6), with P = 48 initialisation rounds.
 * Level L1 reference, written from the normative text.
 */
#ifndef R4_ZUC256_H
#define R4_ZUC256_H

#include <stdint.h>

#define ZUC256_P 48   /* initialisation rounds; 3GPP sets P = 48 (clause 6.1.1) */

typedef struct {
    uint32_t s[16];   /* LFSR cells s_0..s_15, each a 31-bit value in 1..2^31-1 */
    uint32_t r1, r2;  /* memory cells of the nonlinear function F */
} zuc256_state;

/* Key/IV-loading (clause 6.2.7) and initialisation (clause 6.2.8). */
void zuc256_init(zuc256_state *st, const uint8_t key[32], const uint8_t iv[16]);

/* One 32-bit keystream word (clause 6.2.8), as 4 bytes in big-endian order. */
void zuc256_word(zuc256_state *st, uint8_t z[4]);

#endif
