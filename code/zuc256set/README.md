# ZUC-256 set: 256-NEA6, 256-NIA6, 256-NCA6

Level L1 reference in portable C11, written from the normative text of
3GPP TS 35.246 V20.0.0 (clauses 6 and 7), on top of the shared 256-AEAD1
code in `../common`.

| File | Contents | Specification |
|---|---|---|
| `zuc256.c/h` | ZUC-256 core: LFSR over GF(2^31-1), bit-reorganisation, F, L1, L2, Key/IV-loading, initialisation with **P = 48** | clauses 6.2.1-6.2.8 |
| `zuc_sbox.h` | S-boxes S0 and S1 | figures 6.2.5-1, 6.2.5-2 |
| `nxa6.c/h` | ZUC-256 KSG interface (four 32-bit words per 128-bit block) and the three wrappers | clauses 6.3, 7 |
| `kat_cli.c`, `test_official.py` | Official implementers' test data, read from your copy of TS 35.247 | TS 35.247 V20.0.0 |
| `crosscheck_annex.cpp` | Random-input comparison with the informative reference code | TS 35.246 Annex A |

The S-boxes are images in the specification. `zuc_sbox.h` was generated from
the Annex A code text and checked three ways: both tables are permutations of
0..255; they reproduce the examples of clause 6.2.5 (S0(0x12) = 0xF9,
S1(0x34) = 0xC0, S(0x12345678) = 0xF9C05A4E); and the S0 rows were compared
with figure 6.2.5-1. The official vectors then confirm every entry in use.

## Build and check

```
make                                            # builds libnxa6.a
make official   TESTDATA=path/to/35247-k00.docx # official test data
make crosscheck SPEC=path/to/35246-k00.docx     # needs pandoc and g++
```

## Verification status

| Check | Result |
|---|---|
| **Official implementers' test data, TS 35.247 V20.0.0** | **12/12 pass**: 6 POLYVAL dot tests; 3 ZUC-256 tests, each checking the LFSR and R1, R2 after initialisation (48 rounds plus the final work-mode clock) and 32 keystream words |
| R4 vs Annex A, 5000 random trials per algorithm | **match**, 0 mismatches |
| Mutation test (P = 32, the original ZUC-256 value) | detected: all ZUC state and keystream checks fail, all three algorithms fail the cross-check; POLYVAL tests still pass, as expected |
| Design conformance test data, TS 35.248 (full modes) | not yet supplied |

## Limits

- Table S-boxes indexed by secret data: **not constant-time** on CPUs with
  caches. Correctness reference only.
- The 48 initialisation rounds (instead of 32) raise the fixed cost of every
  packet. The benchmark reports setup time separately for this reason.
