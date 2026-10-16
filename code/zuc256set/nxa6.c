/*
 * nxa6.c - 256-NEA6 / 256-NIA6 / 256-NCA6 (3GPP TS 35.246, clauses 6.3 and 7).
 */
#include "nxa6.h"
#include "zuc256.h"
#include <stdlib.h>
#include <string.h>

/* ---- ZUC-256 KSG interface (clause 6.3) -------------------------------- */
static void z_initialise(void *s, const uint8_t key[32], const uint8_t iv[16])
{
    zuc256_init((zuc256_state *)s, key, iv);                    /* clause 6.3.1 */
}

static void z_block(zuc256_state *st, uint8_t out[16])          /* four words, concatenated */
{
    for (int i = 0; i < 4; i++) zuc256_word(st, out + 4 * i);
}

static void z_generate_hqp(void *s, uint8_t h[16], uint8_t q[16], uint8_t p[16])
{
    z_block((zuc256_state *)s, h);                              /* clause 6.3.2: twelve */
    z_block((zuc256_state *)s, q);                              /* consecutive words    */
    z_block((zuc256_state *)s, p);
}

static void z_keystream(void *s, uint8_t z[16])
{
    z_block((zuc256_state *)s, z);                              /* clause 6.3.3 */
}

ksg_t ksg_zuc256_new(void)
{
    ksg_t k = { calloc(1, sizeof(zuc256_state)), z_initialise, z_generate_hqp, z_keystream };
    return k;
}

void ksg_zuc256_free(ksg_t *k)
{
    if (k->state) { memset(k->state, 0, sizeof(zuc256_state)); free(k->state); k->state = NULL; }
}

/* ---- 256-NxA6 wrappers (clause 7) -------------------------------------- */
void nea6_256(uint32_t count_c, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ck[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    ksg_t k = ksg_zuc256_new();
    nxa_nea(&k, count_c, bearer, direction, extra_iv, ck, ibs, length, obs);
    ksg_zuc256_free(&k);
}

void nia6_256(uint32_t count_i, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ik[32], const uint8_t *message, uint32_t length,
              uint8_t mac_bytes, uint8_t *mac_i)
{
    ksg_t k = ksg_zuc256_new();
    nxa_nia(&k, count_i, bearer, direction, extra_iv, ik, message, length, mac_bytes, mac_i);
    ksg_zuc256_free(&k);
}

int nca6_256(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
             const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
             const uint8_t *ibs, uint32_t s_length, uint8_t *obs,
             uint8_t mac_bytes, uint8_t *mac)
{
    ksg_t k = ksg_zuc256_new();
    nxa_nca(&k, mode, count, bearer, direction, extra_iv, key, aad, aad_length,
            ibs, s_length, obs, mac_bytes, mac);
    ksg_zuc256_free(&k);
    return 0;
}
