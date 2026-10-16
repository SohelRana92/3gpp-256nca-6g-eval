/*
 * aes256.h - Portable reference AES-256 (encryption direction only).
 *
 * Level L1 (reference portable C) in the R4 evaluation.
 * The 3GPP 256-NEA5/NIA5/NCA5 algorithms (TS 35.243) use AES-256 as a
 * keystream generator, so only the forward cipher is needed.
 *
 * Note: this version uses a table S-box. Table lookups indexed by secret
 * data are not constant-time on CPUs with caches. It is a correctness
 * reference, not a hardened implementation. The constant-time variant is
 * built and tested separately (Step 3g).
 */
#ifndef R4_AES256_H
#define R4_AES256_H

#include <stdint.h>

#define AES256_ROUNDS 14

typedef struct {
    uint8_t rk[(AES256_ROUNDS + 1) * 16]; /* 15 round keys of 16 bytes */
} aes256_ctx;

/* Expand a 32-byte key into the round-key schedule. */
void aes256_init(aes256_ctx *ctx, const uint8_t key[32]);

/* Encrypt one 16-byte block: out = AES-256_K(in). in and out may alias. */
void aes256_encrypt_block(const aes256_ctx *ctx, const uint8_t in[16], uint8_t out[16]);

/* One AES encryption round with an all-zero round key:
 * SubBytes, ShiftRows, MixColumns (FIPS 197). Used as the 128-bit S-box of
 * the Snow 5G finite state machine (TS 35.240, clause 6.2.3). */
void aes_round_zero_key(uint8_t s[16]);

#endif
