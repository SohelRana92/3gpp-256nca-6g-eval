// crosscheck_annexb.cpp - Compare the R4 L1 implementation of 256-NEA6,
// 256-NIA6 and 256-NCA6 with the informative Annex A code of TS 35.246 on
// random inputs, including bit lengths that are not multiples of 8.
//
//   ./crosscheck_annexb [trials] [seed]
//
// This is a cross-check between two implementations of the same text. It does
// not replace the official test data in TS 35.247.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "annex_ref.cpp"

extern "C" {
#include "nxa6.h"
}

static uint64_t rng_state;
static uint32_t rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return (uint32_t)rng_state; }
static void fill(uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)rnd(); }

static uint32_t pick_len(uint32_t max_bits)
{
    switch (rnd() % 4) {
    case 0:  return 8 * (1 + rnd() % (max_bits / 8));          // whole bytes
    case 1:  return 128 * (1 + rnd() % (max_bits / 128));      // whole blocks
    default: return 1 + rnd() % max_bits;                      // any bit length
    }
}

int main(int argc, char **argv)
{
    int trials = argc > 1 ? atoi(argv[1]) : 5000;
    rng_state = argc > 2 ? strtoull(argv[2], 0, 10) : 0x9E3779B97F4A7C15ULL;
    static uint8_t key[32], ex[6], aad[2048], ibs[9100], o1[9100], o2[9100], p1[9100], p2[9100];
    uint8_t m1[16], m2[16], m3[16];
    long fails[3] = {0, 0, 0};

    for (int t = 0; t < trials; t++) {
        fill(key, 32); fill(ex, 6);
        uint32_t count = rnd(); uint8_t bearer = rnd() & 31, dir = rnd() & 1;
        uint8_t mb = 4 + rnd() % 13;
        uint32_t s_len = pick_len(9000 * 8), a_len = (rnd() % 3) ? pick_len(2000 * 8) : 0;
        uint32_t s_bytes = (s_len + 7) / 8;
        fill(ibs, s_bytes); fill(aad, (a_len + 7) / 8);

        // 256-NEA6
        memset(o1, 0, sizeof o1); memset(o2, 0, sizeof o2);
        nea6_256(count, bearer, dir, ex, key, ibs, s_len, o1);
        annex::NEA6_256(count, ex, dir, bearer, key, 32, ibs, o2, s_len);
        if (memcmp(o1, o2, s_bytes)) fails[0]++;

        // 256-NIA6 (message = aad buffer)
        uint32_t m_len = a_len ? a_len : 1 + rnd() % 64;
        nia6_256(count, bearer, dir, ex, key, aad, m_len, mb, m1);
        annex::NIA6_256(count, ex, dir, bearer, key, 32, aad, m_len, m2, mb);
        if (memcmp(m1, m2, mb)) fails[1]++;

        // 256-NCA6 encrypt, then decrypt with both implementations
        uint32_t c_len = (rnd() % 8) ? s_len : 0;              // sometimes AAD only
        memset(o1, 0, sizeof o1); memset(o2, 0, sizeof o2);
        nca6_256(0, count, bearer, dir, ex, key, aad, a_len, ibs, c_len, o1, mb, m1);
        annex::NCA6_256(0, count, ex, dir, bearer, key, 32, m2, mb, aad, a_len, ibs, o2, c_len);
        int bad = memcmp(m1, m2, mb) || memcmp(o1, o2, (c_len + 7) / 8);
        nca6_256(1, count, bearer, dir, ex, key, aad, a_len, o1, c_len, p1, mb, m3);
        memcpy(m2, m1, mb);
        int rc = annex::NCA6_256(1, count, ex, dir, bearer, key, 32, m2, mb, aad, a_len, o1, p2, c_len);
        if (c_len) {
            uint8_t want[9100]; memcpy(want, ibs, (c_len + 7) / 8);
            want[(c_len + 7) / 8 - 1] = clear_bits(want[(c_len + 7) / 8 - 1], c_len);
            bad |= memcmp(p1, want, (c_len + 7) / 8) || memcmp(p2, want, (c_len + 7) / 8);
        }
        bad |= rc != 0 || !!memcmp(m1, m3, mb);
        if (bad) fails[2]++;
    }
    printf("random trials per algorithm: %d\n", trials);
    printf("256-NEA6  R4 vs Annex A: %s (%ld mismatches)\n", fails[0] ? "FAIL" : "MATCH", fails[0]);
    printf("256-NIA6  R4 vs Annex A: %s (%ld mismatches)\n", fails[1] ? "FAIL" : "MATCH", fails[1]);
    printf("256-NCA6  R4 vs Annex A: %s (%ld mismatches, incl. decrypt and tag check)\n", fails[2] ? "FAIL" : "MATCH", fails[2]);
    return (fails[0] || fails[1] || fails[2]) ? 1 : 0;
}
