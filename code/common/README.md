# R4 shared building blocks

Level L1 reference code in portable C11. These two primitives are shared by
the 3GPP 256-bit algorithm sets:

| File | What it is | Used by |
|---|---|---|
| `aes256.c/h` | AES-256 forward cipher (FIPS 197) | 256-NEA5/NIA5/NCA5 keystream (TS 35.243) |
| `polyval.c/h` | POLYVAL field multiply and hash (RFC 8452) | Mac5G integrity in all three sets |

## Build and test

```
make check
```

Requires `gcc` and Python 3 with the `cryptography` package. Any version
works (Ubuntu's `python3-cryptography` is fine). Version 42 or later adds an
extra AES-256-GCM-SIV check.

## What the tests prove

1. **Published known-answer tests.**
   - AES-256: FIPS 197 Appendix C.3.
   - POLYVAL: RFC 8452 Appendix A worked example.
2. **Independent cross-checks on random inputs** (`verify_common.py`):
   - The C AES-256 matches the `cryptography` library.
   - A pure-Python POLYVAL is validated against the library's AES-256-GCM:
     GHASH is recovered from the GCM tag and matched to POLYVAL through the
     identity in RFC 8452, Appendix A. With version 42 or later, full
     AES-256-GCM-SIV built on the same POLYVAL is also matched.
   - The C POLYVAL then matches that validated Python POLYVAL.

## Limits (state these in the paper)

- The AES S-box is a lookup table, so this AES is **not constant-time** on
  CPUs with caches. It is a correctness reference only.
- POLYVAL is branch-free but bit-serial and slow. Fast versions come in the
  L2/L3 modules (PCLMULQDQ on Zen 4, PMULL on Pi 5).

## Not yet included

The 3GPP-specific parts (Make_5GIV, the KSG interface, Mac5G masks and
length block, and the 256-NEA/NIA/NCA wrappers) need the text of TS 35.243,
35.240 and 35.246. They are added once those documents are supplied, and are
then checked against the official test data in TS 35.244/245, 35.241/242
and 35.247/248.
