/*
 * nxa5_hw.h - 256-NEA5 / NIA5 / NCA5 with the ESP32 AES accelerator (level HW).
 * Same arguments and results as L1 (lengths in bits).
 */
#ifndef R4_NXA5_HW_H
#define R4_NXA5_HW_H
#include <stdint.h>

void nea5_256_hw(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs);
void nia5_256_hw(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac);
int  nca5_256_hw(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                 const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac);
#endif
