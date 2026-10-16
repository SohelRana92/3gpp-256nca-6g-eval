/*
 * nxa4_l2a.c - 256-NEA4 / 256-NIA4 / 256-NCA4: Snow 5G on NEON + ARMv8 AES (L2).
 *
 * Same layout as ../l2x86/nxa4_l2.c: each LFSR in two 128-bit registers
 * (cells 0..7 and 8..15, cell i in 16-bit lane i), all eight cells updated
 * per clock. The AES round with a zero key is AESMC(AESE(x, 0)): AESE with a
 * zero key is SubBytes and ShiftRows, AESMC is MixColumns. This equals the
 * AES round of TS 35.240 figure 6.2.3-3, because SubBytes and ShiftRows commute.
 */
#include "../l2x86/aead1_l2.h"
#include "../l2x86/nxa_l2.h"
#include <arm_neon.h>

typedef struct { uint16x8_t alo, ahi, blo, bhi; uint8x16_t r1, r2, r3; } snow_st;

static inline uint16x8_t mulx(uint16x8_t v, uint16x8_t poly)        /* times x in GF(2^16), per lane */
{
    uint16x8_t msb = vreinterpretq_u16_s16(vshrq_n_s16(vreinterpretq_s16_u16(v), 15));
    return veorq_u16(vshlq_n_u16(v, 1), vandq_u16(msb, poly));
}

static inline uint8x16_t aes_r(uint8x16_t x) { return vaesmcq_u8(vaeseq_u8(x, vdupq_n_u8(0))); }

static inline uint8x16_t clock1(snow_st *s)
{
    const uint16x8_t alpha = vdupq_n_u16(0x4a6d), beta = vdupq_n_u16(0xcc87);
    static const uint8_t SIG[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    const uint8x16_t sigma = vld1q_u8(SIG);

    uint8x16_t z = veorq_u8(vreinterpretq_u8_u16(vaddq_u16(vreinterpretq_u16_u8(s->r1), s->bhi)), s->r2);

    uint8x16_t tmp = vreinterpretq_u8_u16(vaddq_u16(vreinterpretq_u16_u8(s->r2),
                         vreinterpretq_u16_u8(veorq_u8(s->r3, vreinterpretq_u8_u16(s->ahi)))));
    s->r3 = aes_r(s->r2);
    s->r2 = aes_r(s->r1);
    s->r1 = vqtbl1q_u8(tmp, sigma);

    uint16x8_t a7 = vreinterpretq_u16_u8(vextq_u8(vreinterpretq_u8_u16(s->alo), vreinterpretq_u8_u16(s->ahi), 14));
    uint16x8_t na = veorq_u16(veorq_u16(s->blo, a7), mulx(s->alo, alpha));
    uint16x8_t nb = veorq_u16(veorq_u16(s->alo, s->bhi), mulx(s->blo, beta));
    s->alo = s->ahi; s->ahi = na;
    s->blo = s->bhi; s->bhi = nb;
    return z;
}

static void snow_init(void *p, const uint8_t key[32], const uint8_t iv[16])
{
    snow_st *s = (snow_st *)p;
    uint8x16_t k0 = vld1q_u8(key), k1 = vld1q_u8(key + 16);
    s->ahi = vreinterpretq_u16_u8(k0);
    s->alo = vreinterpretq_u16_u8(vld1q_u8(iv));
    s->bhi = vreinterpretq_u16_u8(k1);
    s->blo = vdupq_n_u16(0);
    s->r1 = s->r2 = s->r3 = vdupq_n_u8(0);
    for (int t = 1; t <= 16; t++) {
        uint8x16_t z = clock1(s);
        s->ahi = veorq_u16(s->ahi, vreinterpretq_u16_u8(z));
        if (t == 15) s->r1 = veorq_u8(s->r1, k0);
        if (t == 16) s->r1 = veorq_u8(s->r1, k1);
    }
}

static void snow_hqp(void *p, uint8_t hqp[48])
{
    snow_st *s = (snow_st *)p;
    for (int i = 0; i < 3; i++) vst1q_u8(hqp + 16 * i, clock1(s));
}

static void snow_blocks(void *p, uint8_t *out, size_t n)
{
    snow_st s = *(snow_st *)p;
    size_t i = 0;
    /* Four clocks per pass: the LFSR halves change roles between clocks, so
     * unrolling lets the compiler rename registers instead of moving them. */
    for (; i + 4 <= n; i += 4) {
        vst1q_u8(out + 16 * i, clock1(&s));
        vst1q_u8(out + 16 * i + 16, clock1(&s));
        vst1q_u8(out + 16 * i + 32, clock1(&s));
        vst1q_u8(out + 16 * i + 48, clock1(&s));
    }
    for (; i < n; i++) vst1q_u8(out + 16 * i, clock1(&s));
    *(snow_st *)p = s;
}

static const ksg2_ops KSG_SNOW = {snow_init, snow_hqp, snow_blocks};

void nea4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *ibs, uint32_t length, uint8_t *obs)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, 0, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, 0, 0, NULL, 0, ibs, length, obs, NULL);
}

void nia4_256_l2(uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                 const uint8_t key[32], const uint8_t *msg, uint32_t length, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 0, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, 0, mac_bytes, msg, length, NULL, 0, NULL, mac);
}

int nca4_256_l2(int mode, uint32_t count, uint8_t bearer, uint8_t direction, const uint8_t extra_iv[6],
                const uint8_t key[32], const uint8_t *aad, uint32_t aad_length,
                const uint8_t *ibs, uint32_t s_length, uint8_t *obs, uint8_t mac_bytes, uint8_t *mac)
{
    snow_st st; uint8_t iv[16];
    make_5giv(iv, count, bearer, direction, mac_bytes, 1, 0, 0, extra_iv);
    aead1_l2(&KSG_SNOW, &st, key, iv, mode, mac_bytes, aad, aad_length, ibs, s_length, obs, mac);
    return 0;
}
