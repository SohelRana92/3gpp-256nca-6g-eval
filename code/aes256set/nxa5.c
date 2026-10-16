/*
 * nxa5.c - 256-NEA5 / 256-NIA5 / 256-NCA5 (3GPP TS 35.243, clauses 6 and 7).
 */
#include "nxa5.h"
#include "../common/aes256.h"
#include <stdlib.h>
#include <string.h>

/* ---- AES-256 KSG (clause 6.3) ------------------------------------------ */
typedef struct {
    aes256_ctx aes;           /* expanded round keys (clause 6.3.2, option 4b) */
    uint8_t state[16];        /* IV with the 32-bit counter in bytes 12..15   */
} ksg_aes_state;

static void increment_ctr(uint8_t st[16])          /* big-endian 32-bit CTR */
{
    uint32_t ctr = ((uint32_t)st[12] << 24) | ((uint32_t)st[13] << 16) |
                   ((uint32_t)st[14] << 8) | st[15];
    ctr++;
    st[12] = (uint8_t)(ctr >> 24); st[13] = (uint8_t)(ctr >> 16);
    st[14] = (uint8_t)(ctr >> 8);  st[15] = (uint8_t)ctr;
}

static void aes_initialise(void *s, const uint8_t key[32], const uint8_t iv[16])
{
    ksg_aes_state *k = (ksg_aes_state *)s;
    memcpy(k->state, iv, 16);
    memset(k->state + 12, 0, 4);                  /* reset the CTR */
    aes256_init(&k->aes, key);
}

static void aes_generate_hqp(void *s, uint8_t h[16], uint8_t q[16], uint8_t p[16])
{
    ksg_aes_state *k = (ksg_aes_state *)s;
    k->state[0] |= 0x01;                          /* AI = 1 */
    aes256_encrypt_block(&k->aes, k->state, h);
    increment_ctr(k->state);
    aes256_encrypt_block(&k->aes, k->state, q);
    increment_ctr(k->state);
    aes256_encrypt_block(&k->aes, k->state, p);
    k->state[0] &= 0xFE;                          /* AI = 0 */
    memset(k->state + 12, 0, 4);                  /* reset the CTR */
}

static void aes_keystream(void *s, uint8_t z[16])
{
    ksg_aes_state *k = (ksg_aes_state *)s;
    aes256_encrypt_block(&k->aes, k->state, z);
    increment_ctr(k->state);
}

ksg_t ksg_aes256_new(void)
{
    ksg_t k = { calloc(1, sizeof(ksg_aes_state)), aes_initialise, aes_generate_hqp, aes_keystream };
    return k;
}

void ksg_aes256_free(ksg_t *k)
{
    if (k->state) { memset(k->state, 0, sizeof(ksg_aes_state)); free(k->state); k->state = NULL; }
}

/* ---- 256-NxA5 wrappers (clause 7) -------------------------------------- */
void nea5_256(uint32_t count_c, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ck[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    uint8_t iv[16];
    make_5giv(iv, count_c, bearer, direction, 0, 0, 0, 0, extra_iv);      /* table 7.1.5-1 */
    ksg_t k = ksg_aes256_new();
    aead1(&k, ck, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);              /* table 7.1.3-1 */
    ksg_aes256_free(&k);
}

void nia5_256(uint32_t count_i, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ik[32], const uint8_t *message, uint32_t length,
              uint8_t mac_bytes, uint8_t *mac_i)
{
    uint8_t iv[16];
    make_5giv(iv, count_i, bearer, direction, mac_bytes, 0, 0, 0, extra_iv); /* table 7.2.5-1 */
    ksg_t k = ksg_aes256_new();
    aead1(&k, ik, iv, 0, mac_bytes, message, length, NULL, 0, NULL, mac_i); /* table 7.2.3-1 */
    ksg_aes256_free(&k);
}

int nca5_256(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
             const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
             const uint8_t *ibs, uint32_t s_length, uint8_t *obs,
             uint8_t mac_bytes, uint8_t *mac)
{
    uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv); /* table 7.3.5-1 */
    ksg_t k = ksg_aes256_new();
    aead1(&k, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    ksg_aes256_free(&k);
    return 0;
}

int nxa_tag_equal(const uint8_t *a, const uint8_t *b, uint8_t n)
{
    uint8_t d = 0;
    for (uint8_t i = 0; i < n; i++) d |= (uint8_t)(a[i] ^ b[i]);
    return d == 0;
}
