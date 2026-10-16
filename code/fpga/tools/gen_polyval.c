/* gen_polyval.c - random POLYVAL dot products from the verified L1 code: lines a, b, dot(a,b). */
#include <stdio.h>
#include "../../common/polyval.h"
static unsigned S = 7;
static unsigned rnd(void) { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
int main(void)
{
    for (int t = 0; t < 300; t++) {
        uint8_t a[16], b[16], r[16];
        for (int i = 0; i < 16; i++) { a[i] = (uint8_t)rnd(); b[i] = (uint8_t)rnd(); }
        if (t == 0) for (int i = 0; i < 16; i++) { a[i] = i == 0; b[i] = i == 15 ? 0x80 : 0; }
        polyval_dot(r, a, b);
        for (int i = 0; i < 16; i++) printf("%02x", a[i]); printf("\n");
        for (int i = 0; i < 16; i++) printf("%02x", b[i]); printf("\n");
        for (int i = 0; i < 16; i++) printf("%02x", r[i]); printf("\n");
    }
    return 0;
}
