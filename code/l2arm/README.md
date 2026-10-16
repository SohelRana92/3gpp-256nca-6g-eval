# Level L2 for ARMv8-A (Raspberry Pi 5, Cortex-A76)

The same nine algorithms as L1 and as the x86 L2, with the same arguments and
results, built on the ARMv8 cryptography extension and NEON.

| Part | ARM L2 |
|---|---|
| AES-256 KSG | AESE/AESMC, 8 counter blocks in flight; tails as fixed 4-, 2- and 1-block groups; key schedule with AESE for SubWord (no table look-ups) |
| Snow 5G | each LFSR in two 128-bit NEON registers, all 8 cells per clock; FSM AES rounds as AESMC(AESE(x, 0)); byte permutation with TBL; keystream loop unrolled by four |
| ZUC-256 (P = 48) | portable C shared with x86 (`../l2x86/nxa6_l2.c`) |
| POLYVAL (Mac5G) | PMULL (64x64 carry-less multiply); 8 blocks per reduction with H^1..H^8 |
| 256-AEAD1 | 512-byte chunks, 128-bit XOR, MAC on the chunk while in cache, state on the stack |

The Cortex-A76 has 128-bit NEON only (no SVE), so there is no wider L3 on this tier.

## Changes after the first Pi 5 run
- The first version took the AES-256 key schedule from the portable L1 code.
  On the Pi it cost about 1450 of the 1525 cycles of setup per packet. The key
  schedule now uses AESE for SubWord.
- The Snow 5G keystream loop needed about 38 NEON instructions per clock, 7 of
  them register moves for the LFSR halves. Unrolling by four removes the moves
  (about 25 instructions per clock in the compiled code).

## Correctness
`make check` runs the same test as x86 (`../l2x86/test_l2.c`): L2 against L1
on 20000 random trials per algorithm, plus 100000 POLYVAL products. It was
first run under QEMU (aarch64, `-cpu max`). Mutation tests (wrong POLYVAL
reduction constant, Snow 5G AES round without MixColumns, wrong Snow 5G LFSR
tap, MixColumns in the last AES round, swapped length block, wrong round
constant in the key schedule, wrong block order in the unrolled Snow 5G loop)
all make it fail.

Cross-build from x86: `make CC=aarch64-linux-gnu-gcc RUN="qemu-aarch64 -cpu max -L /usr/aarch64-linux-gnu" check`
