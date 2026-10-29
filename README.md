# Full-mode implementation and evaluation of the 3GPP 256-NCA4, 256-NCA5 and 256-NCA6 algorithms for 6G

Code, raw data and analysis scripts for the paper

> S. Rana and M. R. H. Mondal, "Full-Mode Implementation of the 3GPP 256-NCA4, 256-NCA5
> and 256-NCA6 Algorithms for 6G: Packet-Size, Energy and Constant-Time Evaluation on
> Microcontroller, ARM, x86 and FPGA", submitted to *Cyber Security and Applications*, 2026.

The package implements the three 3GPP 256-bit algorithm sets in full: 256-NEA, 256-NIA
and 256-NCA, built on Snow 5G (set 4), AES-256 (set 5) and ZUC-256 (set 6). All three
use the shared 256-AEAD1 framework with POLYVAL. It measures them on four 6G tiers with
one method:

| Tier | Platform | Levels |
|---|---|---|
| Device | ESP32-D0WD-V3 (Xtensa LX6, 240 MHz) | L1, L2 (portable 32-bit C), HW (AES engine) |
| Edge | Raspberry Pi 5 (Cortex-A76, 2.4 GHz) | L1, L2 (ARMv8 AES, PMULL, NEON) |
| Cloud | AMD Ryzen 7 7700 (Zen 4, 3.8 GHz) | L1, L2 (AES-NI, PCLMULQDQ), L3 (AVX-512, VAES, VPCLMULQDQ) |
| Hardware | Spartan-7 XC7S25-1 (Arty S7-25) | Compact (A) and throughput (T) VHDL engines |

The reference schemes are AES-256-GCM (OpenSSL or mbedTLS), ChaCha20-Poly1305 and
AEGIS-256 (libaegis 0.9.1). Every number in the paper and its supplement comes from a
file in `results/`.

## Quick start (Linux, x86-64 or aarch64)

```
sudo apt install build-essential libssl-dev python3-pandas
cd code
make -C common check                  # AES-256 and POLYVAL against FIPS 197 and RFC 8452
make -C l2x86 check                   # x86-64: L2 against L1, 20000 random inputs per algorithm
make -C l3x86 check                   # x86-64 with AVX-512 VAES (skips itself otherwise)
make -C l2arm check                   # aarch64: L2 against L1
make -C bench                         # benchmark, then see docs/REPRODUCE.md
```

The checks against the official 3GPP test data need your own copies of the 3GPP
specifications. See [docs/3GPP_REFERENCE_CODE.md](docs/3GPP_REFERENCE_CODE.md).

To rebuild the merged result files and the supplementary tables from the raw data:

```
python3 analysis/build_final_csv.py           # rebuilds the merged CSV files and checks them
python3 paper/supplementary/make_tables.py
```

## Layout

| Folder | Contents |
|---|---|
| `code/common` | AES-256, POLYVAL and the shared 256-AEAD1 layer (L1) |
| `code/aes256set`, `code/snow5gset`, `code/zuc256set` | The three sets at L1, written from TS 35.243, 35.240 and 35.246 |
| `code/l2x86`, `code/l3x86` | L2 and L3 for x86-64, including the constant-time ZUC-256 (`zuc_sbox_ct.h`) |
| `code/l2arm` | L2 for ARMv8-A, including the constant-time ZUC-256 |
| `code/l2port` | Portable 32-bit L2, used on the ESP32 |
| `code/bench` | Single-core benchmark, multi-core benchmark, timing-leakage test (dudect method), Pi 5 energy |
| `code/esp32` | ESP-IDF project for the device tier |
| `code/fpga` | VHDL designs, testbenches, vectors, board wrapper and Vivado scripts |
| `code/third_party/libaegis` | libaegis 0.9.1, unchanged (MIT) |
| `results/` | All raw measurement files (see [results/README.md](results/README.md)) |
| `analysis/` | Rebuilds the merged result files from the individual runs |
| `paper/supplementary` | Supplementary material (PDF, LaTeX source and table generator) |
| `docs/` | Reproduction guide and notes on the 3GPP reference code |

## Reproducing the measurements

[docs/REPRODUCE.md](docs/REPRODUCE.md) gives the steps for each platform: correctness
checks, clock settings, single-core runs, multi-core runs, timing-leakage tests, energy on
the Pi 5, the ESP32 runs and the FPGA flow.

## Licence

- Our code: MIT licence ([LICENSE](LICENSE)).
- Our data and documentation: CC BY 4.0 ([LICENSE-DATA.md](LICENSE-DATA.md)).
- `code/third_party/libaegis`: MIT licence of its authors (see its own LICENSE file).
- The 3GPP specifications, their reference code and their test data are not part of this
  package.

## Citation

Please cite the paper and this package. [CITATION.cff](CITATION.cff) holds the metadata.
Repository: https://github.com/SohelRana92/3gpp-256nca-6g-eval  
Archived versions (Zenodo, always resolves to the latest): https://doi.org/10.5281/zenodo.23415233

## Contact

Sohel Rana, Institute of Information and Communication Technology (IICT), Bangladesh
University of Engineering and Technology (BUET), Dhaka, Bangladesh.
