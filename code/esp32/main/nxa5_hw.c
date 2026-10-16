/*
 * nxa5_hw.c - 256-NEA5 / NIA5 / NCA5 on the ESP32 AES accelerator (level HW).
 *
 * The accelerator encrypts one 16-byte block at a time from memory-mapped
 * registers. We use it for the AES-256 counter-mode keystream and keep the
 * rest of 256-AEAD1 (POLYVAL, XOR, framing) in the portable L2 code. The key
 * is written to the engine once per packet. We hold the engine, through the
 * ESP-IDF driver lock, for the whole packet, as mbedTLS does for one call.
 */
#include "nxa5_hw.h"
#include "../../l2x86/aead1_l2.h"
#include "aes/esp_aes.h"
#include "hal/aes_hal.h"
#include "hal/aes_types.h"
#include <string.h>

typedef struct {
    uint32_t blk[4];      /* counter block: IV bytes 0..11, then the big-endian counter */
    uint32_t ctr;
} hw_st;

static void hw_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    hw_st *s = (hw_st *)p;
    memcpy(s->blk, iv, 12);
    s->blk[3] = 0;
    s->ctr = 0;
    aes_hal_setkey(key, 32, ESP_AES_ENCRYPT);
}

static void hw_ctr(hw_st *s, uint8_t *out, size_t n)     /* out is 4-byte aligned */
{
    uint32_t c = s->ctr;
    for (size_t b = 0; b < n; b++, c++, out += 16) {
        s->blk[3] = __builtin_bswap32(c);
        aes_hal_transform_block(s->blk, out);
    }
    s->ctr = c;
}

/* H, Q, P with the AI bit (bit 0 of IV byte 0) set; then counter 0 again with AI = 0. */
static void hw_hqp(void *p, uint8_t hqp[48])
{
    hw_st *s = (hw_st *)p;
    uint32_t save = s->blk[0];
    ((uint8_t *)s->blk)[0] |= 1u;
    hw_ctr(s, hqp, 3);
    s->blk[0] = save;
    s->ctr = 0;
}

static void hw_blocks(void *p, uint8_t *out, size_t n) { hw_ctr((hw_st *)p, out, n); }

static const ksg2_ops KSG_HW = {hw_init, hw_hqp, hw_blocks};

void nea5_256_hw(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    hw_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    esp_aes_acquire_hardware();
    aead1_l2(&KSG_HW, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
    esp_aes_release_hardware();
}

void nia5_256_hw(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    hw_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    esp_aes_acquire_hardware();
    aead1_l2(&KSG_HW, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
    esp_aes_release_hardware();
}

int nca5_256_hw(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    hw_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    esp_aes_acquire_hardware();
    aead1_l2(&KSG_HW, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    esp_aes_release_hardware();
    return 0;
}
