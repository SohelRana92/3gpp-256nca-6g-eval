/*
 * nxa_l3.h - Level L3 of the 3GPP 256-bit algorithm sets for x86-64 with
 * AVX-512, VAES and VPCLMULQDQ (e.g. AMD Zen 4, Intel Ice Lake and later).
 *
 * Same arguments and results as L1 and L2 (lengths in bits).
 *   - POLYVAL (all sets): 512-bit VPCLMULQDQ, 16 blocks per reduction.
 *   - AES-256 KSG: 512-bit VAES, 16 counter blocks per loop.
 *   - Snow 5G and ZUC-256 KSG: as in L2. Both produce one keystream word
 *     after another, so a single packet cannot use wider vectors.
 * Call r4_l3_supported() first; it checks the CPU at run time.
 */
#ifndef R4_NXA_L3_H
#define R4_NXA_L3_H
#include <stdint.h>

int r4_l3_supported(void);

void nea5_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia5_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca5_256_l3(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

void nea4_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia4_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca4_256_l3(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

void nea6_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia6_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca6_256_l3(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);
/* ZUC-256 with the constant-time S-box layer of nxa6_l2ct.c */
void nea6ct_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia6ct_256_l3(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca6ct_256_l3(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                   const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                   const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);

#endif
