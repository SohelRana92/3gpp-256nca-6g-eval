/*
 * polyval.c - Portable reference POLYVAL (RFC 8452).
 *
 * A field element is held as two 64-bit words: lo holds the coefficients
 * of x^0..x^63 and hi holds x^64..x^127. Bytes are little-endian, so byte 0
 * bit 0 is the coefficient of x^0, as RFC 8452 requires.
 */
#include "polyval.h"
#include <string.h>

typedef struct { uint64_t lo, hi; } fe128;

static fe128 load(const uint8_t b[16])
{
    fe128 v = {0, 0};
    for (int i = 7; i >= 0; i--) { v.lo = (v.lo << 8) | b[i]; v.hi = (v.hi << 8) | b[8 + i]; }
    return v;
}

static void store(uint8_t b[16], fe128 v)
{
    for (int i = 0; i < 8; i++) { b[i] = (uint8_t)(v.lo >> (8 * i)); b[8 + i] = (uint8_t)(v.hi >> (8 * i)); }
}

/* Reduction constant: x^128 = x^127 + x^126 + x^121 + 1 (mod p). */
#define RED_HI 0xC200000000000000ULL   /* bits 127, 126, 121 -> hi bits 63, 62, 57 */
#define RED_LO 0x0000000000000001ULL   /* bit 0 */

/* v = v * x mod p, branch-free. */
static fe128 mul_x(fe128 v)
{
    uint64_t carry = v.hi >> 63;                 /* coefficient of x^127 before shift */
    uint64_t mask = (uint64_t)0 - carry;
    fe128 r;
    r.hi = (v.hi << 1) | (v.lo >> 63);
    r.lo = v.lo << 1;
    r.hi ^= RED_HI & mask;
    r.lo ^= RED_LO & mask;
    return r;
}

/* v = v * x^-1 mod p, branch-free.
 * If the x^0 coefficient is 1, add p first (which clears it), then shift. */
static fe128 mul_xinv(fe128 v)
{
    uint64_t odd = v.lo & 1;
    uint64_t mask = (uint64_t)0 - odd;
    v.lo ^= RED_LO & mask;                       /* clear bit 0 */
    v.hi ^= RED_HI & mask;                       /* add x^127 + x^126 + x^121 */
    fe128 r;
    r.lo = (v.lo >> 1) | (v.hi << 63);
    r.hi = (v.hi >> 1) | (odd << 63);            /* the added x^128 term becomes x^127 */
    return r;
}

void polyval_dot(uint8_t r[16], const uint8_t a[16], const uint8_t b[16])
{
    fe128 A = load(a), B = load(b), acc = {0, 0};
    /* Horner, from the highest coefficient of B: acc = acc*x + b_i*A. */
    for (int i = 127; i >= 0; i--) {
        acc = mul_x(acc);
        uint64_t bit = (i >= 64) ? (B.hi >> (i - 64)) & 1 : (B.lo >> i) & 1;
        uint64_t mask = (uint64_t)0 - bit;
        acc.lo ^= A.lo & mask;
        acc.hi ^= A.hi & mask;
    }
    for (int i = 0; i < 128; i++) acc = mul_xinv(acc);   /* times x^-128 */
    store(r, acc);
}

void polyval(uint8_t out[16], const uint8_t h[16], const uint8_t *x, size_t nblocks)
{
    uint8_t s[16] = {0};
    for (size_t j = 0; j < nblocks; j++) {
        for (int i = 0; i < 16; i++) s[i] ^= x[16 * j + i];
        polyval_dot(s, s, h);
    }
    memcpy(out, s, 16);
}
