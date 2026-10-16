/*
 * nxa4.h - The Snow 5G based 3GPP algorithm set (TS 35.240):
 *          256-NEA4 (encryption), 256-NIA4 (integrity), 256-NCA4 (AEAD).
 * Level L1 reference. Same calling conventions as nxa5.h.
 */
#ifndef R4_NXA4_H
#define R4_NXA4_H

#include <stdint.h>
#include "../common/aead1.h"

ksg_t ksg_snow5g_new(void);
void  ksg_snow5g_free(ksg_t *k);

void nea4_256(uint32_t count_c, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ck[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia4_256(uint32_t count_i, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ik[32], const uint8_t *message, uint32_t length,
              uint8_t mac_bytes, uint8_t *mac_i);
int  nca4_256(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
              const uint8_t *ibs, uint32_t s_length, uint8_t *obs,
              uint8_t mac_bytes, uint8_t *mac);

#endif
