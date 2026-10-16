/*
 * ksg_l2.h - Internal: the L2 keystream generators, shared with L3.
 */
#ifndef R4_KSG_L2_H
#define R4_KSG_L2_H
#include "aead1_l2.h"
#include <immintrin.h>

typedef struct {
    __m128i rk[15];           /* AES-256 round keys */
    uint8_t iv[16];           /* IV; bytes 12..15 hold the next counter (big-endian) */
} aes_st;

extern const ksg2_ops KSG2_AES, KSG2_SNOW, KSG2_ZUC, KSG2_ZUC_CT;

#define KSG2_STATE_MAX 512    /* bytes; enough for every L2 state */
#endif
