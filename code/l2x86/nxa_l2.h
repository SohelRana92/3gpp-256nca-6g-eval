/*
 * nxa_l2.h - Level L2 of the 3GPP 256-bit algorithm sets for x86-64.
 *
 * Same arguments and results as the L1 reference (lengths in bits), but
 * built on AES-NI, PCLMULQDQ and SSSE3, with bulk keystream generation,
 * an 8-block aggregated POLYVAL, and all state on the stack.
 * Needs a CPU with AES-NI and PCLMULQDQ (every x86-64 CPU since about 2011).
 */
#ifndef R4_NXA_L2_H
#define R4_NXA_L2_H
#include <stdint.h>

/* AES-256 set (TS 35.243) */
void nea5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia5_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca5_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

/* Snow 5G set (TS 35.240) */
void nea4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca4_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

/* ZUC-256 set (TS 35.246) */
void nea6_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia6_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca6_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

/* ZUC-256 set with a constant-time S-box layer (nxa6_l2ct.c; AVX-512 VBMI on x86-64, NEON on AArch64) */
void nea6ct_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia6ct_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca6ct_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                   const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

/* Building blocks, exposed for the tests. */
void polyval_dot_l2(uint8_t r[16], const uint8_t a[16], const uint8_t b[16]);

#endif
