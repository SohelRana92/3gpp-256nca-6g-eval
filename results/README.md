# Measurement data

Every file here is raw output of the programs in `code/`, as written on the measurement
machine. The only change is in `pi5/pi5_verify_log.txt`, where the local folder path is
replaced by `<repo>`. Lines starting with `#` are headers with the date, platform, compiler
and settings of the run.

Files used in the paper are marked **paper**. The other files are earlier runs, kept so that
every change in the campaign stays traceable (see the notes column).

## File formats

### Single-core benchmark (`bench`): `*_L1*.csv`, `*_L2*.csv`, `*_base*.csv`, `*_final*.csv`, `*_zucct.csv`, `esp32_run*.csv`

One row per level, algorithm and payload size.

| Column | Meaning |
|---|---|
| `platform`, `level`, `set`, `alg` | Platform label; level (`L1`, `L2`, `L3`, `HW`, `L2ct`, `L3ct`, `base` for reference schemes); set name; algorithm |
| `bytes` | Payload size of one call; 0 gives the fixed setup cost |
| `iters`, `reps` | Calls per batch; timed batches (31) |
| `med_ns`, `q1_ns`, `q3_ns`, `min_ns` | Time per call: median, quartiles and minimum over the batches |
| `med_cycles`, `q1_cycles`, `q3_cycles`, `min_cycles` | Core cycles per call (perf counter; CCOUNT on the ESP32) |
| `cpb` | Median cycles per byte (empty for 0 bytes) |
| `eff_ghz` | Cycles divided by nanoseconds; shows that the clock stayed fixed |
| `med_tsc` | Time-stamp counter per call (Zen 4 only) |

Throughput in the paper is `bytes * 8 / med_ns` Gbit/s.

### Buffer placement (`*_layout.csv`)

As above, with a first column `out_offset`: position of the output buffer in a 4 KiB page.

### Multi-core (`*_mc*.csv`)

| Column | Meaning |
|---|---|
| `threads`, `cpus` | Number of threads; logical CPUs used, joined by `+` |
| `rep`, `seconds`, `calls` | Repetition (0 to 2); timed seconds; calls of all threads |
| `gbps_total`, `gbps_per_thread` | Summed and mean throughput |
| `eff_ghz_min`, `eff_ghz_max` | Lowest and highest effective clock over the threads |
| `cycles_per_byte_mean` | Mean cycles per byte over the threads |

### Timing leakage (`*_ct*.csv`)

| Column | Meaning |
|---|---|
| `test` | `key` (fixed vs random key) or `data` (fixed vs random plaintext) |
| `measurements` | Number of timed calls |
| `t_raw`, `t_max` | Welch t over all measurements; largest abs(t) over all measurements and 100 cropped subsets |
| `crop_index` | Subset that gives `t_max` (-1: all measurements) |
| `median_class0`, `median_class1` | Median cycles of the two classes |
| `verdict` | `LEAK` if t_max > 10, `possible` if 4.5 < t_max <= 10, else `pass` |

### Energy (`pi5_energy*.csv`)

| Column | Meaning |
|---|---|
| `rep`, `seconds`, `calls`, `payload_bytes` | Repetition; window length; calls and bytes in the active window |
| `idle_core_w`, `active_core_w` | Mean VDD_CORE power in the idle and active windows |
| `idle_total_w`, `active_total_w` | Mean sum of all PMIC rails |
| `n_idle`, `n_active` | Number of PMIC samples per window |
| `temp_c` | Core temperature after the window |
| `nj_per_byte_core`, `nj_per_byte_total` | (active - idle) power x seconds / payload bytes |

### FPGA

- `fpga/ooc_final/reports/summary.csv`: one row per out-of-context build: area, clock
  period `period_ns`, worst negative slack `wns_ns`, `fmax_mhz = 1000 / (period - wns)`, and
  Vivado power estimates at f_max and at 100 MHz. A build appears more than once when it
  was repeated; the last row is the final one.
- `fpga/ooc_final/reports/<build>/`: Vivado utilization, timing and power reports.
- `fpga/hw_final/<engine>/hwtest.txt`: board test of each engine: 127 packets compared with
  the L1 results, and the cycle counts of the timing packets.
- `fpga/fpga_final.csv`: merged table used by the paper (built by
  `analysis/build_final_csv.py`).

## Files

### Zen 4 (`zen4/`)

| File | Paper | Notes |
|---|---|---|
| `zen4_final_all_levels.csv` | **paper** | `zen4_L1.csv` + `zen4_L2_L3_base_final.csv` |
| `zen4_L1.csv` | **paper** | L1, 0 B to 64 KiB |
| `zen4_L2_L3_base_final.csv` | **paper** | L2, L3 and OpenSSL, 0 B to 1 MiB, fixed buffer layout |
| `zen4_base_ref.csv` | **paper** | Reference schemes incl. ChaCha20-Poly1305 and AEGIS-256 |
| `zen4_mc.csv`, `zen4_mc_info.txt` | **paper** | Multi-core scaling |
| `zen4_mc_ossl.csv`, `zen4_mc_ossl_info.txt` | **paper** | OpenSSL usual call vs fetched cipher object |
| `zen4_ct.csv`, `zen4_ct_info.txt` | **paper** | Timing-leakage tests, all implementations |
| `zen4_ct_rerun.csv` | **paper** | Repeats with 20 million measurements: L1 Snow 5G, and the L3 data tests |
| `zen4_ct_l3long.csv` | **paper** | L3 AES-256 and Snow 5G with 100 million measurements |
| `zen4_ct_zuc.csv`, `zen4_zucct.csv` | **paper** | Constant-time ZUC-256: leakage tests and speed |
| `zen4_layout.csv` | **paper** | Buffer-placement test |
| `zen4_sysinfo*.txt` | | System information of each run |
| `zen4_L1_run1.csv`, `zen4_L2_base*.csv`, `zen4_L2_L3_base_run1.csv`, `zen4_L2_L3_base_v3.csv`, `zen4_L2_L3_base_1MB.csv`, `zen4_L2_L3_base.csv` | | Earlier runs during development of L2 and L3, superseded by the final files |

### Raspberry Pi 5 (`pi5/`)

| File | Paper | Notes |
|---|---|---|
| `pi5_final_all_levels.csv` | **paper** | L1 rows of `pi5_L1_L2_base_run1.csv` + `pi5_L2_base.csv` |
| `pi5_L1_L2_base_run1.csv` | **paper** (L1) | First run; its L2 rows are superseded |
| `pi5_L2_base.csv` | **paper** | L2 and OpenSSL after the ARM key-schedule and Snow 5G changes |
| `pi5_base_ref.csv` | **paper** | Reference schemes incl. ChaCha20-Poly1305 and AEGIS-256 |
| `pi5_energy.csv`, `pi5_energy_summary.txt` | **paper** | Energy, 3GPP sets and AES-256-GCM |
| `pi5_energy_ref.csv`, `pi5_energy_ref_summary.txt` | **paper** | Energy, ChaCha20-Poly1305 and AEGIS-256 |
| `pi5_mc.csv`, `pi5_mc_info.txt` | **paper** | Multi-core scaling |
| `pi5_ct.csv`, `pi5_ct_info.txt` | **paper** | Timing-leakage tests |
| `pi5_ct_zuc_run1.csv`, `pi5_ct_zuc.csv`, `pi5_zucct.csv` | **paper** | ZUC-256 leakage reruns and the constant-time version |
| `pi5_layout.csv` | **paper** | Buffer-placement test |
| `pi5_verify_log.txt` | **paper** | Full correctness check (`make verify`) |
| `pi5_sysinfo*.txt` | | System information of each run |

`pi5_zucct.csv`: the header line of the original run read `platform=zen4` by mistake; the
data rows always read `pi5`, and the header is corrected with a note line.

### ESP32 (`esp32/`)

| File | Paper | Notes |
|---|---|---|
| `esp32_final.csv` | **paper** | Identical to `esp32_run3.csv` |
| `esp32_run3.csv`, `esp32_log_run3.txt` | **paper** | Main run: all measured code in IRAM, shared 32-bit MAC loop |
| `esp32_run4.csv`, `esp32_run4_log.txt` | **paper** | Adds ChaCha20-Poly1305 and hardened AEGIS-256; repeats run 3 within 0.01% |
| `esp32_run5.csv`, `esp32_run5_log.txt` | **paper** | AEGIS-256 with plain T-tables, reference schemes only |
| `esp32_run1.csv`, `esp32_log_run1.txt` | | Code in flash (ZUC-256 loop missed in the flash cache) |
| `esp32_run2_iram.csv`, `esp32_log_run2_iram.txt` | | Code in IRAM, MAC loop inlined (register spills) |

### FPGA (`fpga/`)

| Folder | Paper | Notes |
|---|---|---|
| `fpga_final.csv` | **paper** | Merged area, f_max, cycles and power |
| `ooc_final/` | **paper** | Final out-of-context builds of all 11 designs (`*.backup.log`: earlier attempts of the same build) |
| `hw_final/` | **paper** | Final board test of all six engines, 50 MHz, ZUC-256 engine version 3 |
| `hw_run1/`, `hw_run1_logs/` | | First board test (ZUC-256 engine version 1) |
| `hw_run2_zucv2/`, `hw_run3_zucv3/` | | Board tests of the ZUC-256 engine versions 2 and 3 |
| `ooc_run_all_console.txt` | | Console output of the out-of-context script |
