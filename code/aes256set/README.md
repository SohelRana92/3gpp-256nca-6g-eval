# AES-256 set: 256-NEA5, 256-NIA5, 256-NCA5

Level L1 reference in portable C11, written from the normative text of
3GPP TS 35.243 V20.0.0 (clauses 4.3, 5, 6 and 7).

| File | Contents | Specification |
|---|---|---|
| `../common/aead1.c/h` | Make_5GIV, ClearBits, Mac5G.Update/Final, 256-AEAD1 | clauses 4.3, 5.2 |
| `nxa5.c/h` | AES-256 KSG (Initialise, GenerateHQP, Keystream) and the three wrappers | clauses 6.3, 7.1-7.3 |
| `crosscheck_annexb.cpp` | Random-input comparison with the Annex B reference code | Annex B (informative) |
| `extract_annexb.py` | Pulls Annex B code out of your own copy of the specification | - |
| `test_official.py`, `kat_cli.c` | Runs the official implementers' test data, read from your copy of TS 35.244 | TS 35.244 V20.0.0 |

## Build and check

```
make                                          # builds libnxa5.a
make crosscheck SPEC=path/to/35243-k00.docx   # needs pandoc and g++
make official TESTDATA=path/to/35244-k00.docx # official test data
```

The 3GPP Annex B code is **not** stored in this repository. The script
extracts it from your copy of TS 35.243 each time.

## Verification status

| Check | Result |
|---|---|
| AES-256 vs FIPS 197 C.3, POLYVAL vs RFC 8452 App. A | pass (`../common`) |
| R4 vs Annex B, 5000 random trials per algorithm | **match**, 0 mismatches |
| Mutation test (one-bit IV bug injected) | detected: 256-NCA5 fails 200/200 |
| **Official implementers' test data, TS 35.244 V20.0.0** | **9/9 pass**: 6 POLYVAL dot tests (clause 5.2), 3 AES-256 CTR keystream tests of 4 blocks each (clause 5.3) |
| Design conformance test data, TS 35.245 (full NEA5/NIA5/NCA5) | not yet supplied |

The random trials cover bit lengths that are not multiples of 8, AAD-only
calls, empty payloads, tags of 4 to 16 bytes, decryption and tag checks.

The official implementers' test data confirm the two building blocks
(POLYVAL dot and the AES-256 keystream generator) against 3GPP's own values.
The full-mode wrappers (IV mapping, H/Q/P, Mac5G, truncation) are confirmed
by agreement with Annex B on 5000 random trials each. Official vectors for
the full modes are in TS 35.245 (design conformance test data); add them to
reach full official coverage.

## A note on the specification text

In decrypt mode, step 24 of clause 5.2.2 feeds IBS (the received
ciphertext) to Mac5G.Update without ClearBits, while step 11 masks the
unused bits of OBS in encrypt mode. The Annex B code masks the last byte in
both modes. We follow Annex B, so that a ciphertext with stray bits beyond
S_LENGTH produces the same tag as the clean one. For well-formed input
(unused bits already zero) both readings agree.

## Limits

- AES uses a table S-box: **not constant-time**. Correctness reference only.
- POLYVAL is bit-serial and slow. Faster L2/L3 versions follow.
- Only 256-bit keys (LK = 0), as the current specification requires.
