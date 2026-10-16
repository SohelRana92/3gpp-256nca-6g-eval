# Snow 5G set: 256-NEA4, 256-NIA4, 256-NCA4

Level L1 reference in portable C11, written from the normative text of
3GPP TS 35.240 V20.1.0 (clauses 6 and 7), on top of the shared 256-AEAD1
code in `../common`.

| File | Contents | Specification |
|---|---|---|
| `snow5g.c/h` | Snow 5G core: alpha, beta, sigma, LFSRs, FSM, initialisation, keystream | clauses 6.2-6.4 |
| `nxa4.c/h` | Snow 5G KSG interface and the three wrappers | clauses 6.5, 7 |
| `kat_cli.c`, `test_official.py` | Official implementers' test data, read from your copy of TS 35.241 | TS 35.241 V20.0.0 |
| `crosscheck_annex.cpp` | Random-input comparison with the informative reference code | TS 35.240 Annex B |

Two details come from figures, not text, in TS 35.240:
Table 6.3-1 (key and IV bytes map to 16-bit words in little-endian order,
k_i = KEY[2i+1] || KEY[2i]) and Figure 6.2.3-3 (byte r_0 of a 128-bit
register is AES state byte 0, the standard AES layout).

## Build and check

```
make                                            # builds libnxa4.a
make official   TESTDATA=path/to/35241-k00.docx # official test data
make crosscheck SPEC=path/to/35240-k10.docx     # needs pandoc and g++
```

Neither the 3GPP test data nor the reference code is stored here; both are
read from your copies of the documents.

## Verification status

| Check | Result |
|---|---|
| **Official implementers' test data, TS 35.241 V20.0.0** | **12/12 pass**: 6 POLYVAL dot tests; 3 Snow 5G tests, each checking the full internal state after initialisation (lfsrA, lfsrB, R1, R2, R3) and 4 keystream words |
| R4 vs Annex B, 5000 random trials per algorithm | **match**, 0 mismatches |
| Mutation test (two sigma entries swapped) | detected by both: official test fails, all three algorithms fail the cross-check |
| Design conformance test data, TS 35.242 (full modes) | not yet supplied |

## Limits

- The FSM uses the table-based AES round from `../common/aes256.c`:
  **not constant-time**. Correctness reference only.
- One LFSR clock at a time, as written in the specification. Faster
  word-parallel L2 and SIMD L3 versions follow.
