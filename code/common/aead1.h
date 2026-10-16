/*
 * aead1.h - The generic 256-AEAD1 construction (3GPP TS 35.243, clause 5)
 * and the shared helpers used by all nine 256-NxAx algorithms.
 *
 * Level L1 reference: written from the normative text, one step per line
 * where practical, so each line can be traced to the specification.
 */
#ifndef R4_AEAD1_H
#define R4_AEAD1_H

#include <stddef.h>
#include <stdint.h>

/* KeyStream Generator interface (TS 35.243, clause 4.4). */
typedef struct {
    void *state;
    void (*initialise)(void *state, const uint8_t key[32], const uint8_t iv[16]);
    void (*generate_hqp)(void *state, uint8_t h[16], uint8_t q[16], uint8_t p[16]);
    void (*keystream)(void *state, uint8_t z[16]);
} ksg_t;

/* Make_5GIV mapping (TS 35.243, clause 4.3, table 4.3.1-3).
 * extra_iv may be NULL, which means six zero bytes. */
void make_5giv(uint8_t iv[16], uint32_t count, uint8_t bearer, uint8_t direction,
               uint8_t mac_bytes, uint8_t cf, uint8_t lk, uint8_t ai,
               const uint8_t extra_iv[6]);

/* ClearBits (clause 5.2.3.2): keep only the s mod 8 leading bits of b. */
uint8_t clear_bits(uint8_t b, uint32_t s);

/* Mac5G.Update (clause 5.2.3.4) and Mac5G.Final (clause 5.2.3.5). */
void mac5g_update(const uint8_t h[16], uint8_t a[16], const uint8_t *data, uint32_t d_bytes);
void mac5g_final(const uint8_t q[16], uint8_t a[16], const uint8_t p[16],
                 uint32_t a_len_bits, uint32_t s_len_bits);

/* 256-AEAD1 (clause 5.2.2).
 *   mode       0 = encrypt, 1 = decrypt
 *   mac_bytes  0 (no MAC) or 4..16
 *   aad        AAD_LENGTH bits; it is read only (the last byte is masked on a copy)
 *   ibs/obs    S_LENGTH bits; obs receives the output stream
 *   mac        receives MAC_BYTES bytes (the computed tag, also in decrypt mode)
 * The caller compares the tag in decrypt mode (see clause 7.3.0, NOTE). */
void aead1(const ksg_t *ksg, const uint8_t key[32], const uint8_t iv[16], int mode,
           uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
           const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac);

/* Generic 256-NEAx / 256-NIAx / 256-NCAx wrappers (TS 35.24x, clause 7),
 * shared by the three sets. They build the IV with Make_5GIV using the
 * constants of tables 7.1.5-1, 7.2.5-1 and 7.3.5-1, then call 256-AEAD1. */
void nxa_nea(const ksg_t *ksg, uint32_t count_c, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t ck[32],
             const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nxa_nia(const ksg_t *ksg, uint32_t count_i, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t ik[32],
             const uint8_t *message, uint32_t length, uint8_t mac_bytes, uint8_t *mac_i);
void nxa_nca(const ksg_t *ksg, int mode, uint32_t count, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t key[32],
             const uint8_t *aad, uint32_t aad_length, const uint8_t *ibs, uint32_t s_length,
             uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

#endif
