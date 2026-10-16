/*
 * tab_l2p.h - Internal: AES tables for the portable L2 (32-bit CPUs).
 *
 * State words are little-endian columns: byte r of word c is AES state byte
 * s[r][c] = in[4c + r]. TE[0][x] is the MixColumns column of S(x) in row 0,
 * as the little-endian word (2s, s, s, 3s); TE[r] is TE[0] rotated by 8r bits.
 * The tables live in RAM and are built once by l2p_tables_init().
 */
#ifndef R4_TAB_L2P_H
#define R4_TAB_L2P_H
#include <stdint.h>

extern uint32_t L2P_TE[4][256];
extern uint8_t  L2P_SBOX[256];
void l2p_tables_init(void);

/* One AES round on four column words: SubBytes, ShiftRows, MixColumns. */
#define L2P_ROUND(o0, o1, o2, o3, i0, i1, i2, i3) do {                                              \
    o0 = L2P_TE[0][(i0) & 0xFF] ^ L2P_TE[1][((i1) >> 8) & 0xFF] ^ L2P_TE[2][((i2) >> 16) & 0xFF] ^ L2P_TE[3][(i3) >> 24]; \
    o1 = L2P_TE[0][(i1) & 0xFF] ^ L2P_TE[1][((i2) >> 8) & 0xFF] ^ L2P_TE[2][((i3) >> 16) & 0xFF] ^ L2P_TE[3][(i0) >> 24]; \
    o2 = L2P_TE[0][(i2) & 0xFF] ^ L2P_TE[1][((i3) >> 8) & 0xFF] ^ L2P_TE[2][((i0) >> 16) & 0xFF] ^ L2P_TE[3][(i1) >> 24]; \
    o3 = L2P_TE[0][(i3) & 0xFF] ^ L2P_TE[1][((i0) >> 8) & 0xFF] ^ L2P_TE[2][((i1) >> 16) & 0xFF] ^ L2P_TE[3][(i2) >> 24]; \
} while (0)

/* Little-endian word load and store (byte by byte: no alignment needed). */
static inline uint32_t l2p_ld(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline void l2p_st(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
#endif
