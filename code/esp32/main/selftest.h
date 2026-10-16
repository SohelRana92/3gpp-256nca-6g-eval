#ifndef R4_SELFTEST_H
#define R4_SELFTEST_H
#include <stdint.h>
/* Returns the number of failures (0 = all checks pass). Prints one line per check. */
int r4_selftest(int trials);
/* Digest of L1 outputs over the fixed cases, per algorithm (9 values). */
void r4_l1_digests(uint64_t d[9]);
#endif
