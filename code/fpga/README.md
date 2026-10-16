# R4 hardware tier: 3GPP 256-bit algorithm sets on a Spartan-7 FPGA

VHDL-2008 designs of the full 256-NEA/NIA/NCA engines (256-AEAD1 with POLYVAL)
for the three sets, for the Digilent Arty S7-25 (XC7S25-1CSGA324).

| File | Content |
|---|---|
| `rtl/r4_pkg.vhd` | block layout, AES and ZUC S-boxes, AES round functions, bit masks |
| `rtl/polyval_mac.vhd` | Mac5G accumulator; digit-serial POLYVAL multiplier, D bits per clock (generic) |
| `rtl/aead1_ctrl.vhd` | 256-AEAD1 packet engine (H/Q/P, AAD, data, length block, tag), shared by all sets |
| `rtl/ksg_aes.vhd` | AES-256 CTR KSG, 1 or 2 rounds per clock, key schedule on the fly |
| `rtl/ksg_snow.vhd` | Snow 5G KSG, one Snow clock per 1 or 2 cycles (32 or 16 S-boxes) |
| `rtl/ksg_zuc.vhd` | ZUC-256 KSG (P = 48), one word per cycle |
| `rtl/nca{5,4,6}_top.vhd` | engine = KSG + controller |
| `tb/` | GHDL testbenches and test vectors (from the verified L1 C code) |
| `vivado/` | out-of-context build scripts with area, timing and power reports |

## Design points

| Tag | KSG | MAC | Cycles per 16-byte block (NCA / NEA) |
|---|---|---|---|
| nca5_A | AES, 1 round/clock | 8 bits/clock | 16 / 14 |
| nca5_T | AES, 2 rounds/clock | 32 bits/clock | 7 / 7 |
| nca4_A | Snow 5G, 16 S-boxes | 8 bits/clock | 16 / 2 |
| nca4_T | Snow 5G, 32 S-boxes | 32 bits/clock | 4 / 1 |
| nca6_A | ZUC-256 | 8 bits/clock | 16 / 4 |
| nca6_T | ZUC-256 | 32 bits/clock | 4 / 4 |

Cycle counts are measured in simulation on 16 KiB packets (`tb/vectors/timing_*.txt`).
A full-width MAC (128 bits/clock) works in simulation but does not fit the XC7S25.

## Verification

`./sim.sh` runs 120 packets per set and design point in GHDL: 256-NEA, 256-NIA,
256-NCA encrypt and decrypt, lengths in bits with partial last bytes, with and
without AAD, up to 1500 bytes. Expected values come from the L1 C code, which
passes the official 3GPP test data. All designs match on every packet.
Planted faults (AES Rcon, Snow 5G sigma, ZUC constant d1, ZUC L2 rotation,
POLYVAL reduction polynomial) are each caught.

## Build in Vivado

```
source <Vivado install>/2025.2/Vivado/settings64.sh
cd vivado
./run_all.sh
```

## Hardware test on the board (optional, board on USB)

`rtl_hw/hwtest_top.vhd` wraps one engine with a 1 Mbaud UART and two 32 KiB
block-RAM buffers. The PC sends a packet, the FPGA runs the engine from RAM to
RAM at 50 MHz, counts the clock cycles and returns the output, the tag and the
cycle count. `tools/hwtest.py` sends all 120 test packets and the timing packets
and checks every reply. The design passes the same test in GHDL
(`tb/tb_hwtest.vhd`, with a fast UART).

```
cd vivado
./hw_test_all.sh /dev/ttyUSB1        # build, program and test all six engines
```
