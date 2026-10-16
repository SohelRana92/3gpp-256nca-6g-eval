/*
 * snow5g.h - The Snow 5G stream cipher core (3GPP TS 35.240, clause 6).
 * Level L1 reference, written from the normative text.
 *
 * Representation:
 *   a[i], b[i]   16-bit LFSR cells a_i, b_i (a_0, b_0 leave first)
 *   r1, r2, r3   128-bit FSM registers as 16-byte arrays, byte k = r_k
 *                (r_0 least significant), which is also AES state byte k
 *                (TS 35.240, figure 6.2.3-3).
 */
#ifndef R4_SNOW5G_H
#define R4_SNOW5G_H

#include <stdint.h>

typedef struct {
    uint16_t a[16], b[16];
    uint8_t r1[16], r2[16], r3[16];
} snow5g_state;

/* Initialisation (clause 6.3): load KEY and IV, then 16 clocks. */
void snow5g_init(snow5g_state *st, const uint8_t key[32], const uint8_t iv[16]);

/* Keystream (clause 6.4): one 128-bit word, returned as 16 bytes in little-endian
 * order, as the KSG.Keystream interface requires (clause 6.5.4). */
void snow5g_keystream(snow5g_state *st, uint8_t z[16]);

#endif
