# Reproducing the measurements

This guide repeats every run of the paper. Each step names the output file it writes; the
same file name sits in `results/` with our data. Close other programs before each run and
leave the machine alone while it runs.

Contents: [1 Correctness](#1-correctness) · [2 Zen 4 and Pi 5](#2-zen-4-and-raspberry-pi-5) ·
[3 ESP32](#3-esp32-device-tier) · [4 FPGA](#4-fpga-hardware-tier) ·
[5 Supplementary tables](#5-supplementary-tables)

## 1. Correctness

Without the 3GPP documents (any Linux machine):

```
cd code
make -C common check          # FIPS 197 and RFC 8452 vectors, plus random cross-checks
make -C l2x86 check           # x86-64: ends with "ALL L2 CHECKS PASS"
make -C l3x86 check           # x86-64 with AVX-512 VAES: ends with "ALL L3 CHECKS PASS"
make -C l2arm check           # aarch64: ends with "ALL L2 CHECKS PASS"
```

With the 3GPP documents (see [3GPP_REFERENCE_CODE.md](3GPP_REFERENCE_CODE.md)):

```
make verify DOCS=path/to/folder     # ends with "ALL R4 CORRECTNESS CHECKS PASSED"
```

Our logs: `results/pi5/pi5_verify_log.txt` (Pi 5); the Zen 4 run gives the same result.

## 2. Zen 4 and Raspberry Pi 5

One-time setup:

```
sudo apt install build-essential libssl-dev linux-tools-common linux-tools-$(uname -r) python3-pandas
cd code/bench
```

Every timed run starts with a fixed clock and ends by restoring it:

```
sudo ./clock_lock.sh 3800000      # Zen 4: boost off, 3.8 GHz fixed, perf counter enabled
sudo ./clock_lock.sh 2400000      # Pi 5: 2.4 GHz fixed
...                               # runs below
sudo ./clock_restore.sh
```

We pin to logical CPU 2 on Zen 4 and core 3 on the Pi 5. Change the second argument for
another machine.

### 2.1 Single core, all levels and reference schemes

| Run | Command | Output |
|---|---|---|
| Zen 4, L1 | `./run_linux.sh zen4 2 L1` | `results/zen4_L1.csv` |
| Zen 4, L2, L3 and OpenSSL | `./run_linux.sh zen4 2 L2,L3,base` | `results/zen4_L2_L3_base.csv` |
| Pi 5, L1, L2 and OpenSSL | `./run_linux.sh pi5 3` | `results/pi5_L1_L2_base.csv` |
| Reference schemes only | `./run_linux.sh zen4 2 base`, `./run_linux.sh pi5 3 base` | `results/<platform>_base.csv` |

Each run also writes `results/<platform>_sysinfo.txt`. After a run, `eff_ghz` in the CSV
must stay at the fixed clock (3.73 to 3.74 on Zen 4, 2.37 to 2.40 on the Pi 5), and the
summary must report cycles per byte. If it reports ns per byte, the perf counter is blocked
(`perf_event_paranoid`).

The files in `results/` keep the run names of our campaign. `analysis/build_final_csv.py`
shows which of them form the merged files used in the paper:
`zen4_L1.csv + zen4_L2_L3_base_final.csv`, `pi5_L1_L2_base_run1.csv (L1) + pi5_L2_base.csv`.
The reference schemes come from `zen4_base_ref.csv` and `pi5_base_ref.csv`.

### 2.2 Buffer placement

```
./layout_test.sh zen4 2           # output: results/zen4_layout.csv
./layout_test.sh pi5 3            # output: results/pi5_layout.csv
```

### 2.3 Multi-core scaling

```
./run_mc.sh zen4                  # 1, 2, 4, 8 cores and 16 logical CPUs; about 12 minutes
./run_mc.sh pi5                   # 1, 2, 4 cores
MC_TAG=ossl MC_ALGS=OpenSSL-AES-256-GCM,OpenSSL-AES-256-GCM-fetch,OpenSSL-ChaCha20-Poly1305,OpenSSL-ChaCha20-Poly1305-fetch ./run_mc.sh zen4
```

Outputs: `results/<platform>_mc.csv`, `results/<platform>_mc_info.txt`, and
`results/zen4_mc_ossl.csv` for the OpenSSL fetch test.

### 2.4 Energy on the Pi 5

```
python3 energy_pi.py                                            # 3GPP sets and AES-256-GCM, about 15 minutes
python3 energy_pi.py --cases ref --out results/pi5_energy_ref.csv   # ChaCha20-Poly1305 and AEGIS-256
```

Outputs: `results/pi5_energy.csv`, `results/pi5_energy_summary.txt`,
`results/pi5_energy_ref.csv`, `results/pi5_energy_ref_summary.txt`.

### 2.5 Timing-leakage tests (dudect method)

```
make ct
./run_ct.sh zen4 2                # all implementations, 20 s or 20 million measurements each
./run_ct.sh pi5 3
./ct --platform zen4 --cpu 2 --seconds 300 --max 100000000 --only L3:256-NCA5,L3:256-NCA4 -o results/zen4_ct_l3long.csv
./ct --platform zen4 --cpu 2 --seconds 120 --only L1:256-NCA4,L3:256-NCA5,L3:256-NCA4 -o results/zen4_ct_rerun.csv
```

Outputs: `results/<platform>_ct.csv` and `results/<platform>_ct_info.txt`, plus the files
named after `-o`. The controls must show `LEAK` for `early exit` in the key test and `pass`
for `branch-free`.

### 2.6 Constant-time ZUC-256

The constant-time levels need AVX-512 VBMI on x86 (Zen 4) or NEON on aarch64.

```
./bench --platform zen4 --cpu 2 --levels L2,L2ct,L3,L3ct --algs 256-NEA6,256-NIA6,256-NCA6 -o results/zen4_zucct.csv
./ct --platform zen4 --cpu 2 --seconds 120 --only L2:256-NCA6,L2ct:256-NCA6,L3:256-NCA6,L3ct:256-NCA6 -o results/zen4_ct_zuc.csv
./bench --platform pi5 --cpu 3 --levels L2,L2ct --algs 256-NEA6,256-NIA6,256-NCA6 -o results/pi5_zucct.csv
./ct --platform pi5 --cpu 3 --seconds 120 --only L2:256-NCA6,L2ct:256-NCA6 -o results/pi5_ct_zuc.csv
```

## 3. ESP32 (device tier)

Needs ESP-IDF 5.4.4 and an ESP32 board (we use an ESP32-D0WD-V3, revision 3.1). On Windows,
use the ESP-IDF 5.4 PowerShell and a project path without spaces.

```
cd code/esp32
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash
python get_results.py COM3 run3        # self-tests first, then about 3 minutes of timing
```

The log must contain `# ALL ESP32 SELF-TESTS PASS` and end with `R4-DONE`. If it shows
`R4-STOP`, a self-test failed and nothing is timed. Output: `results/esp32_run3.csv` and
`results/esp32_run3_log.txt`.

The second AEGIS-256 run with plain T-tables uses two build options:

```
del sdkconfig
idf.py fullclean
idf.py set-target esp32
idf.py -DR4_AEGIS_TTAB=1 -DR4_ONLY_BASE=1 build
idf.py -p COM3 flash
python get_results.py COM3 run5
```

The log must show `# libaegis soft AES: plain T-tables (FAVOR_PERFORMANCE)`.
Runs 1 and 2 in `results/esp32/` are earlier runs (code in flash, then first IRAM build);
run 3 is the main run, run 4 adds the reference schemes, run 5 the plain-table AEGIS-256.

## 4. FPGA (hardware tier)

Needs Vivado 2025.2 and, for part 2, a Digilent Arty S7-25.

Part 1, out of context (no board), about 1 to 2 hours:

```
source <Vivado install>/2025.2/Vivado/settings64.sh
cd code/fpga/vivado
./run_all.sh                       # 11 builds: six engines and five keystream generators
```

Outputs: `reports/<build>/` and `reports/summary.csv` (ours: `results/fpga/ooc_final/`).

Part 2, on the board, about 1 hour:

```
sudo apt install python3-serial
./hw_test_all.sh /dev/ttyUSB1      # all six engines
```

Each engine must end with `127 packets, 0 mismatches -> ALL MATCH ON THE FPGA`. Outputs:
`hw/<engine>/hwtest.txt` and logs (ours: `results/fpga/hw_final/`). Simulation of the
testbenches: `code/fpga/sim.sh` (GHDL).

## 5. Supplementary tables

```
python3 analysis/build_final_csv.py               # must print "matches" for all four files
python3 paper/supplementary/make_tables.py
cd paper/supplementary && latexmk -pdf supplementary.tex
```

Needs Python 3 with pandas, and a LaTeX installation for the supplement.
Table S13 of the supplement names the data file behind every figure and table.
