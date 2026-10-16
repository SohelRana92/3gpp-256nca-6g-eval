/*
 * nxa4.c - 256-NEA4 / 256-NIA4 / 256-NCA4 (3GPP TS 35.240, clauses 6.5 and 7).
 */
#include "nxa4.h"
#include "snow5g.h"
#include <stdlib.h>
#include <string.h>

/* ---- Snow 5G KSG interface (clause 6.5) -------------------------------- */
static void s_initialise(void *s, const uint8_t key[32], const uint8_t iv[16])
{
    snow5g_init((snow5g_state *)s, key, iv);                     /* clause 6.5.2 */
}

static void s_generate_hqp(void *s, uint8_t h[16], uint8_t q[16], uint8_t p[16])
{
    snow5g_keystream((snow5g_state *)s, h);                      /* clause 6.5.3: three */
    snow5g_keystream((snow5g_state *)s, q);                      /* consecutive keystream */
    snow5g_keystream((snow5g_state *)s, p);                      /* words, little endian */
}

static void s_keystream(void *s, uint8_t z[16])
{
    snow5g_keystream((snow5g_state *)s, z);                      /* clause 6.5.4 */
}

ksg_t ksg_snow5g_new(void)
{
    ksg_t k = { calloc(1, sizeof(snow5g_state)), s_initialise, s_generate_hqp, s_keystream };
    return k;
}

void ksg_snow5g_free(ksg_t *k)
{
    if (k->state) { memset(k->state, 0, sizeof(snow5g_state)); free(k->state); k->state = NULL; }
}

/* ---- 256-NxA4 wrappers (clause 7) -------------------------------------- */
void nea4_256(uint32_t count_c, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ck[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    ksg_t k = ksg_snow5g_new();
    nxa_nea(&k, count_c, bearer, direction, extra_iv, ck, ibs, length, obs);
    ksg_snow5g_free(&k);
}

void nia4_256(uint32_t count_i, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
              const uint8_t ik[32], const uint8_t *message, uint32_t length,
              uint8_t mac_bytes, uint8_t *mac_i)
{
    ksg_t k = ksg_snow5g_new();
    nxa_nia(&k, count_i, bearer, direction, extra_iv, ik, message, length, mac_bytes, mac_i);
    ksg_snow5g_free(&k);
}

int nca4_256(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
             const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
             const uint8_t *ibs, uint32_t s_length, uint8_t *obs,
             uint8_t mac_bytes, uint8_t *mac)
{
    ksg_t k = ksg_snow5g_new();
    nxa_nca(&k, mode, count, bearer, direction, extra_iv, key, aad, aad_length,
            ibs, s_length, obs, mac_bytes, mac);
    ksg_snow5g_free(&k);
    return 0;
}
