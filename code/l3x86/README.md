# Level L3 for x86-64 (AVX-512, VAES, VPCLMULQDQ)

For CPUs with 512-bit vectors and the vector AES and carry-less multiply
instructions: AMD Zen 4 and later, Intel Ice Lake and later. The code checks
the CPU at run time (`r4_l3_supported()`); the test and the benchmark skip
L3 on other CPUs.

| Part | L2 | L3 |
|---|---|---|
| POLYVAL (Mac5G, all sets) | 128-bit PCLMULQDQ, 8 blocks per reduction | 512-bit VPCLMULQDQ, 16 blocks per reduction (4 blocks per instruction); H^5..H^16 four at a time |
| AES-256 KSG | AES-NI, 8 blocks in flight | 512-bit VAES, 16 blocks per loop; runs under 16 blocks use the L2 code |
| Snow 5G KSG | SSSE3 + AES-NI | same as L2 |
| ZUC-256 KSG | scalar, unrolled | same as L2 |
| Data XOR | 128-bit | 512-bit, masked tail |

Packets under 512 bytes (data + AAD) go to the L2 code. In the first Zen 4
run, L3 lost 6-19% to L2 below 128 bytes and up to 19% at 256-576 bytes
(the cost of computing H^2..H^16), while it won from 1 KiB up (0.41-0.65
of the L2 cycles at 64 KiB). The 16-block MAC path now starts at 32 blocks,
and H^5..H^16 are computed four at a time.

The second run showed that handing short packets to L2 itself cost 10-45
cycles, because the entry function built its large aligned stack frame first.
Each entry point is now a thin front (one compare, then a direct jump to L2)
with the long-packet body in a separate function.

Snow 5G and ZUC-256 each make one keystream word after the previous one,
so a single packet cannot use wider registers. For them L3 speeds up only
the MAC. This is a property of the designs, and the paper reports it as such.

## Correctness
`make check` compares L3 with L1 on 20000 random trials per algorithm, with
lengths up to 9000 bytes and edge cases at 64, 256 and 512 bytes.
Mutation tests (wrong H power in the 16-block POLYVAL, wrong step in the
four-at-a-time power computation, wrong VAES counter order, wrong counter
step, wrong masked tail length) all make it fail.
