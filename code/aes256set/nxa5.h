/*
 * nxa5.h - The AES-256 based 3GPP algorithm set (TS 35.243):
 *          256-NEA5 (encryption), 256-NIA5 (integrity), 256-NCA5 (AEAD).
 * Level L1 reference, written from the normative text.
 * Key sizes other than 256 bits are not supported (LK = 0, clause 4.5).
 */
#ifndef R4_NXA5_H
#define R4_NXA5_H

#include <stdint.h>
#include "../common/aead1.h"

/* The AES-256 KSG (clause 6.3), exposed so other code can reuse it. */
ksg_t ksg_aes256_new(void);
void  ksg_aes256_free(ksg_t *k);

/* 256-NEA5 (clause 7.1). length = number of bits; obs gets ceil(length/8) bytes. */
void nea5_256(uint32_t count_c, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ck[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);

/* 256-NIA5 (clause 7.2). mac_bytes in 4..16. */
void nia5_256(uint32_t count_i, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ik[32], const uint8_t *message, uint32_t length,
              uint8_t mac_bytes, uint8_t *mac_i);

/* 256-NCA5 (clause 7.3). mode 0 = encrypt, 1 = decrypt; mac_bytes in 4..16.
 * mac receives the computed tag in both modes. Returns 0. */
int nca5_256(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
             const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
             const uint8_t *ibs, uint32_t s_length, uint8_t *obs,
             uint8_t mac_bytes, uint8_t *mac);

/* Constant-time tag comparison for callers that verify a received tag. */
int nxa_tag_equal(const uint8_t *a, const uint8_t *b, uint8_t n);

#endif
