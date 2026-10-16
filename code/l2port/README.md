# Level L2, portable 32-bit C (device tier: ESP32)

No intrinsics and no 64-bit vector units: the same nine functions as
`../l2x86/nxa_l2.h` (`*_l2`), written for 32-bit microcontrollers.

| File | Content |
|---|---|
| `tab_l2p.c` | AES S-box (computed from its definition) and four 1 KiB T-tables, built in RAM at start-up |
| `nxa5_l2p.c` | AES-256 CTR KSG: T-table rounds on little-endian column words, key schedule on words |
| `nxa4_l2p.c` | Snow 5G: two 16-bit LFSR cells per 32-bit word (8 new cells per clock in 4 words), FSM AES round as one T-table round, sigma as a 4x4 byte transpose, halves renamed by unrolling two clocks |
| `aead1_l2p.c` | 256-AEAD1 with POLYVAL by Shoup's 4-bit tables in the GHASH bit order (RFC 8452 Appendix A), on 32-bit words, in one out-of-line loop for every caller; tables per packet for H and Q |
| `../l2x86/nxa6_l2.c` | ZUC-256 L2, already portable C, shared with x86 and ARM |

`make check` runs the L2-vs-L1 equivalence test (20000 random cases per
algorithm, POLYVAL on 100000) on the host; `make check32` does the same as an
i386 build. Both pass. Planted faults are each caught: the alpha constant, the sigma word order,
one GHASH reduction constant (entry and shift), the block word order of the
MAC, one AES round constant, and the Snow 5G R1 key injection clock.

The table look-ups depend on secret data, as in L1 and in mbedTLS.
