/*
 * polyval.h - Portable reference POLYVAL (RFC 8452, Section 3).
 *
 * POLYVAL works in GF(2^128) defined by x^128 + x^127 + x^126 + x^121 + 1,
 * with little-endian byte and bit order. dot(a, b) = a * b * x^-128.
 * The 3GPP Mac5G integrity function (TS 35.243, clause 5.2.3) builds on
 * this field; the exact Mac5G wrapping (masks, truncation, length block)
 * is added in the 256-AEAD1 module once the specification text is supplied.
 *
 * This reference is branch-free on secret data, but it is a slow
 * bit-serial multiplier meant for correctness, not speed.
 */
#ifndef R4_POLYVAL_H
#define R4_POLYVAL_H

#include <stddef.h>
#include <stdint.h>

/* r = dot(a, b) = a * b * x^-128 in the POLYVAL field. r may alias a or b. */
void polyval_dot(uint8_t r[16], const uint8_t a[16], const uint8_t b[16]);

/* Full POLYVAL(H, X_1..X_n) over n complete 16-byte blocks (RFC 8452). */
void polyval(uint8_t out[16], const uint8_t h[16], const uint8_t *x, size_t nblocks);

#endif
