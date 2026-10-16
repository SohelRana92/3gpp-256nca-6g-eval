# R4 device tier: ESP32 benchmark (ESP-IDF)

ESP-IDF project for the classic ESP32 (tested with ESP-IDF v5.4.4). It uses
the shared algorithm sources in `../aes256set`, `../snow5gset`, `../zuc256set`,
`../common`, `../l2port` and `../l2x86/nxa6_l2.c`, so keep the folder tree as it is.

## Levels

| Level | What it is |
|---|---|
| L1 | the reference C code, identical to the PC and Pi builds |
| L2 | portable 32-bit C (`../l2port`): AES with four RAM T-tables, Snow 5G with two 16-bit cells per word, POLYVAL with 4-bit tables; ZUC-256 as on the other tiers |
| HW | the AES-256 set with the AES-256 keystream on the ESP32 AES accelerator; POLYVAL and framing as in L2 |
| base | mbedTLS AES-256-CTR and AES-256-GCM from ESP-IDF (also on the AES accelerator; GHASH in software) |

## Correctness (runs first, on the board)

1. The L1 code must reproduce digests of its own outputs computed on a PC
   (`main/golden.h`), where the same code passes the official 3GPP test data.
   64 fixed cases per set: lengths in bits, AAD, tag sizes 4..16, decryption.
2. L2 and HW must give bit-identical results to L1 on 200 random cases per
   algorithm, and the L2 POLYVAL dot product must match L1.

If any check fails, the program prints `R4-STOP` and does no timing.
`main/golden.h` is regenerated on a PC with
`gcc -O2 -DR4_GEN_GOLDEN -o gen_golden main/selftest.c <L1 sources> && ./gen_golden > main/golden.h`.

## Measurement

Same method as `../bench`: one public-API call per packet, sizes 0 to 16384
bytes, batches of at least 2 ms, 5 warm-up and 31 timed batches, median and
quartiles. Cycles from the CPU cycle counter, time from `esp_timer`. CPU at
240 MHz, `-O2`, the benchmark alone on core 1, Wi-Fi and Bluetooth off.
All measured code runs from internal RAM (`main/linker.lf`): our code, the
mbedTLS AES/GCM/cipher modules and the AES driver. Run 1 (code in flash) and
run 2 (code in RAM) agree within 2% except for ZUC-256 at L2: its unrolled
code (18.7 KiB) missed in the flash cache, which doubled its setup cost.

Runs 1 and 2 also showed that 256-NCA at L2 and HW cost 28,700 cycles more
per 512-byte chunk than 256-NEA plus 256-NIA. The cause was code generation:
the MAC loop inlined into the large 256-AEAD1 function spilled registers on
the windowed Xtensa core (about 52 extra instructions per byte). Run 3 uses
one out-of-line MAC loop on 32-bit words for every caller. In QEMU with
instruction counting, the excess falls from 6,440 to about 10 units per chunk,
and the MAC itself needs 29% fewer instructions.

The emulator (QEMU) build passes all self-tests; its timings mean nothing.

## Reference schemes (added 2026-10-15)

The `base` level now also measures mbedTLS ChaCha20-Poly1305 (enabled in `sdkconfig.defaults`)
and AEGIS-256 from libaegis 0.9.1 (portable C path, code in IRAM, tables in DRAM). Delete an
old `sdkconfig` before building, or the ChaCha20 options stay off. The board checks both schemes
against their RFC test vectors before any timing.
