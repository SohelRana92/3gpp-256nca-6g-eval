/*
 * aead1.c - 256-AEAD1 (3GPP TS 35.243, clause 5) and shared helpers.
 */
#include "aead1.h"
#include "polyval.h"
#include <string.h>

void make_5giv(uint8_t iv[16], uint32_t count, uint8_t bearer, uint8_t direction,
               uint8_t mac_bytes, uint8_t cf, uint8_t lk, uint8_t ai,
               const uint8_t extra_iv[6])
{
    /* IV[0]: MAC_BYTES in bits 7..3 (lsb at bit 3), CF bit 2, LK bit 1, AI bit 0. */
    iv[0] = (uint8_t)(((mac_bytes & 0x1F) << 3) | ((cf & 1) << 2) | ((lk & 1) << 1) | (ai & 1));
    /* IV[1]: bits 7..6 zero, BEARER in bits 5..1 (lsb at bit 1), DIRECTION bit 0. */
    iv[1] = (uint8_t)(((bearer & 0x1F) << 1) | (direction & 1));
    /* IV[2..7]: EXTRA_IV[0..5]. */
    if (extra_iv) memcpy(iv + 2, extra_iv, 6); else memset(iv + 2, 0, 6);
    /* IV[8..11]: COUNT, big endian. */
    iv[8]  = (uint8_t)(count >> 24);
    iv[9]  = (uint8_t)(count >> 16);
    iv[10] = (uint8_t)(count >> 8);
    iv[11] = (uint8_t)count;
    /* IV[12..15]: zero (reserved for the AES-CTR counter). */
    memset(iv + 12, 0, 4);
}

uint8_t clear_bits(uint8_t b, uint32_t s)
{
    static const uint8_t bitpad[8] = {0xFF, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
    return (uint8_t)(b & bitpad[s % 8]);
}

void mac5g_update(const uint8_t h[16], uint8_t a[16], const uint8_t *data, uint32_t d_bytes)
{
    uint32_t didx = 0;
    while (d_bytes >= 16) {
        for (int i = 0; i < 16; i++) a[i] ^= data[didx + i];
        didx += 16;
        polyval_dot(a, a, h);
        d_bytes -= 16;
    }
    if (d_bytes > 0) {                       /* last partial block: zero padding */
        for (uint32_t i = 0; i < d_bytes; i++) a[i] ^= data[didx + i];
        polyval_dot(a, a, h);
    }
}

void mac5g_final(const uint8_t q[16], uint8_t a[16], const uint8_t p[16],
                 uint32_t a_len_bits, uint32_t s_len_bits)
{
    uint8_t mac_len[16] = {0};
    for (int i = 0; i < 4; i++) {            /* little-endian 32-bit lengths */
        mac_len[i]     = (uint8_t)(s_len_bits >> (8 * i));
        mac_len[8 + i] = (uint8_t)(a_len_bits >> (8 * i));
    }
    for (int i = 0; i < 16; i++) a[i] ^= mac_len[i];
    polyval_dot(a, a, q);
    for (int i = 0; i < 16; i++) a[i] ^= p[i];
}

/* Feed `len_bits` of a stream into Mac5G, masking unused bits of the last
 * byte on a private copy so the caller's buffer is never changed. */
static void mac5g_update_bits(const uint8_t h[16], uint8_t a[16], const uint8_t *data, uint32_t len_bits)
{
    uint32_t nbytes = (len_bits + 7) / 8;
    if (nbytes == 0) return;
    uint32_t whole = (len_bits % 8) ? nbytes - 1 : nbytes;
    uint32_t aligned = whole - (whole % 16);
    mac5g_update(h, a, data, aligned);
    uint8_t tail[16] = {0};
    uint32_t rest = nbytes - aligned;        /* 0..16 bytes */
    memcpy(tail, data + aligned, rest);
    if (len_bits % 8) tail[rest - 1] = clear_bits(tail[rest - 1], len_bits);
    mac5g_update(h, a, tail, rest);
}

void aead1(const ksg_t *ksg, const uint8_t key[32], const uint8_t iv[16], int mode,
           uint8_t mac_bytes, const uint8_t *aad, uint32_t aad_length,
           const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t *mac)
{
    uint8_t h[16], q[16], p[16], ks[16];

    ksg->initialise(ksg->state, key, iv);                       /* step 2 */
    if (mac_bytes > 0) ksg->generate_hqp(ksg->state, h, q, p);  /* step 3 */

    uint32_t s_bytes = (s_length + 7) / 8;                      /* step 5 */
    if (s_length > 0) {                                         /* steps 4-12 */
        for (uint32_t i = 0; i < s_bytes; i++) {
            if (i % 16 == 0) ksg->keystream(ksg->state, ks);
            obs[i] = (uint8_t)(ibs[i] ^ ks[i % 16]);
        }
        obs[s_bytes - 1] = clear_bits(obs[s_bytes - 1], s_length);
    }

    if (mac_bytes > 0) {                                        /* steps 13-30 */
        uint8_t a[16] = {0};
        if (aad_length > 0) mac5g_update_bits(h, a, aad, aad_length);
        /* Pick the stream that holds the ciphertext. In decrypt mode the input
         * is the ciphertext; its unused tail bits are masked as for OBS. */
        if (s_length > 0) mac5g_update_bits(h, a, mode == 0 ? obs : ibs, s_length);
        mac5g_final(q, a, p, aad_length, s_length);
        memcpy(mac, a, mac_bytes);
    }
}

void nxa_nea(const ksg_t *ksg, uint32_t count_c, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t ck[32],
             const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    uint8_t iv[16];
    make_5giv(iv, count_c, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1(ksg, ck, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nxa_nia(const ksg_t *ksg, uint32_t count_i, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t ik[32],
             const uint8_t *message, uint32_t length, uint8_t mac_bytes, uint8_t *mac_i)
{
    uint8_t iv[16];
    make_5giv(iv, count_i, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1(ksg, ik, iv, 0, mac_bytes, message, length, NULL, 0, NULL, mac_i);
}

void nxa_nca(const ksg_t *ksg, int mode, uint32_t count, uint8_t bearer, uint8_t direction,
             const uint8_t extra_iv[6], const uint8_t key[32],
             const uint8_t *aad, uint32_t aad_length, const uint8_t *ibs, uint32_t s_length,
             uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1(ksg, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
}
