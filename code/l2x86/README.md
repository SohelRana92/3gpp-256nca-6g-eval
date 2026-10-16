# Level L2 for x86-64

The same nine algorithms as L1, with the same arguments and results (lengths
in bits), built on the CPU's crypto instructions.

| Part | L1 (reference) | L2 (this folder) |
|---|---|---|
| AES-256 KSG | table AES, one block at a time | AES-NI, 8 counter blocks in flight; tails as fixed 4-, 2- and 1-block groups |
| Snow 5G | byte loops, 8 single LFSR clocks | each LFSR in two 128-bit registers; all 8 cells per clock (SSSE3); FSM AES rounds with AESENC |
| ZUC-256 (P = 48) | shifting LFSR | ring of 16 cells, 16-step unrolled loop, 64-bit mod 2^31-1 sum, 32-bit S-box tables |
| POLYVAL (Mac5G) | bit-serial | PCLMULQDQ; 8 blocks per reduction with H^1..H^8 |
| 256-AEAD1 | byte loop, KSG called per 16 bytes | 512-byte chunks: bulk keystream, 128-bit XOR, MAC on the chunk while in cache |
| Memory | calloc/free per packet | state on the stack |

Needs AES-NI and PCLMULQDQ. Compiled with `-maes -mpclmul -mssse3 -msse4.1`.

## Correctness
`make check` compares L2 with L1 on random inputs for all nine algorithms
(20000 trials each). The trials cover partial last bytes, the 16-, 128- and
512-byte boundaries of this code, AAD, tag sizes 4 to 16, and decryption.
L1 itself passes the official 3GPP test data, so L2 inherits that result.

Mutation tests: each of these faults makes the check fail, and only for
the algorithms that use the faulty part:
Snow 5G alpha polynomial, Snow 5G LFSR tap, ZUC-256 with P = 32, wrong
H power in the aggregated POLYVAL, wrong AES counter for H/Q/P, and
swapped lengths in the final MAC block.

## Limits
- ZUC-256 still indexes S-box tables with secret data; Snow 5G and AES-256
  use AES-NI and are free of secret-dependent table lookups.
- ZUC-256 is a serial 32-bit design. Its per-packet speed is bounded by the
  F-function chain; wider vectors help only with many packets in parallel.
