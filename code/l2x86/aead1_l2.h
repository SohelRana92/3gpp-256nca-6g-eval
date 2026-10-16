/*
 * aead1_l2.h - Internal: 256-AEAD1 (TS 35.243 clause 5) on a bulk KSG.
 */
#ifndef R4_AEAD1_L2_H
#define R4_AEAD1_L2_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*init)(void *st, const uint8_t key[32], const uint8_t iv[16]);
    void (*hqp)(void *st, uint8_t hqp[48]);                       /* H || Q || P */
    void (*blocks)(void *st, uint8_t *out, size_t nblocks);       /* keystream blocks */
} ksg2_ops;

/* Lengths in bits, as in the specification. */
void aead1_l2(const ksg2_ops *k, void *st, const uint8_t key[32], const uint8_t iv[16], int mode,
              uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
              const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac);

/* From ../common/aead1.c */
void make_5giv(uint8_t iv[16], uint32_t count, uint8_t bearer, uint8_t direction,
               uint8_t mac_bytes, uint8_t cf, uint8_t lk, uint8_t ai, const uint8_t extra_iv[6]);
#endif
