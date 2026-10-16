# R4 benchmark harness, Linux

Times one call of each public API, the way a protocol stack sends one packet.
Each call includes the IV construction, the KSG initialisation, the H/Q/P
generation, the keystream and the MAC. Size 0 gives the fixed setup cost
per packet.

| Item | Setting |
|---|---|
| Algorithms | 256-NEA/NIA/NCA 4, 5 and 6 at level L1 (portable C) and L2 (x86-64 crypto instructions, see `../l2x86`); plus OpenSSL AES-256-CTR and AES-256-GCM as production baselines (when `libssl-dev` is installed) |
| Sizes (bytes) | 0, 16, 40, 64, 128, 256, 576, 1024, 1500, 4096, 9000, 16384, 65536, 262144 (256 KiB), 1048576 (1 MiB) |
| MAC / AAD | 16-byte tag, no AAD |
| Statistics | 31 timed batches per point, each at least 2 ms, after 5 warm-up batches; median, quartiles and minimum |
| Buffers | fixed layout in every build: input on a page boundary, output 2048 bytes into a page (`--out-offset` changes it; `layout_test.sh` checks the effect) |
| Clocks | core cycles from the Linux perf counter; ns from CLOCK_MONOTONIC_RAW; x86 TSC. `eff_ghz` = cycles / ns shows whether the clock stayed fixed |

## Zen 4 (Ryzen 7 7700) - one-time setup
```
sudo apt install linux-tools-common linux-tools-$(uname -r) libssl-dev
```

## Run
```
cd r4/code/bench
sudo ./clock_lock.sh 3800MHz     # boost off, clock fixed at nominal 3.8 GHz, perf counter enabled
./run_linux.sh zen4 2 L2,base    # pins to logical CPU 2; L2 + baselines, a few minutes
                                 # (leave out L2,base to run L1 as well)
sudo ./clock_restore.sh          # back to normal
```
Close other programs first, and leave the machine alone during the run.

Output: `results/<platform>_L2_base.csv` and `results/<platform>_sysinfo.txt`.

## Checks after a run
- The summary printed at the end says **cycles/byte**, not ns/byte. If it
  says ns/byte, the perf counter was blocked.
- In the CSV, `eff_ghz` should sit close to 3.80 on every row.

## Raspberry Pi 5
The same scripts work: `sudo ./clock_lock.sh 2400MHz`, then `./run_linux.sh pi5 3`.

## What these numbers are
Level L1 is the portable reference code: table AES and a bit-serial POLYVAL.
It is a correctness baseline, not a speed claim. Level L2 uses AES-NI,
PCLMULQDQ and SSSE3 and is checked bit-for-bit against L1.

## Energy (Raspberry Pi 5)

`energy_pi.py` measures energy per byte from the Pi 5 power-management chip
(`vcgencmd pmic_read_adc`): idle window, then a steady-load window with
`./bench --run-seconds`, energy = (P_active - P_idle) x time / bytes. It reports
the VDD_CORE rail and the sum of all rails. Run it after `sudo ./clock_lock.sh 2400MHz`.

## Reference schemes (added 2026-10-15)

`--levels base` now also measures ChaCha20-Poly1305 (OpenSSL `EVP_chacha20_poly1305`) and
AEGIS-256 (libaegis 0.9.1, bundled in `../third_party/libaegis`, MIT licence, built with its
own release flags `-O3`; `make AEGIS=0` leaves it out). Before any timing, `bench` checks both
against the RFC test vectors (RFC 8439 Section 2.8.2, RFC 10032 Appendix A.3.2 #1) and stops
if either fails. `energy_pi.py --cases ref` measures their energy on the Pi 5.

## Multi-core scaling (added 2026-10-15)

`./run_mc.sh zen4` (or `pi5`) builds `mc` and runs every 256-NCA set at its best level
(L3 on x86-64, L2 on ARM) plus AES-256-GCM, ChaCha20-Poly1305 and AEGIS-256 on 1, 2, 4, 8
threads on separate physical cores, then on all logical CPUs (SMT). Each thread is pinned,
has its own buffers, key context and counter, and must reproduce the tag of the main thread
before timing. Each point runs 0.3 s of warm-up and 2 s of timing, three times.
Output: `results/<platform>_mc.csv` (summed Gbit/s, per-thread clock) and `_mc_info.txt`.

## Timing-leakage tests (added 2026-10-16)

`./run_ct.sh <platform> <cpu>` builds `ct` and runs a dudect-style fixed-vs-random test
(Welch's t-test with 100 cropping percentiles) on every 256-NCA level and on the reference
schemes, for a fixed all-zero key against random keys ("key") and an all-zero plaintext
against random plaintexts ("data"). Two controls check the harness: a key loop with an
early exit (must report LEAK) and a branch-free loop (must pass). |t| > 4.5: possible
leak; |t| > 10: leak. Timer: rdtsc on x86-64, user-mode cycle counter via perf on aarch64.
