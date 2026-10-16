#!/bin/bash
# run_all.sh - build every R4 FPGA design (about 1-2 hours in total).
#   ./run_all.sh            all designs
#   ONLY="nca5_A ksg6" ./run_all.sh   only the named ones
# Results: reports/summary.csv and one folder of reports per design.
cd "$(dirname "$0")"
command -v vivado >/dev/null || { echo "vivado not in PATH: source <Vivado>/2025.2/Vivado/settings64.sh"; exit 1; }
DESIGNS=(
  "nca5_A nca5_top ARCH=1 D=8"      # AES-256, 1 round/clock,   MAC 8 bits/clock
  "nca5_T nca5_top ARCH=2 D=32"     # AES-256, 2 rounds/clock,  MAC 32 bits/clock
  "nca4_A nca4_top ARCH=1 D=8"      # Snow 5G, 16 S-boxes,      MAC 8 bits/clock
  "nca4_T nca4_top ARCH=2 D=32"     # Snow 5G, 32 S-boxes,      MAC 32 bits/clock
  "nca6_A nca6_top ARCH=1 D=8"      # ZUC-256, 1 word/clock,    MAC 8 bits/clock
  "nca6_T nca6_top ARCH=1 D=32"     # ZUC-256, 1 word/clock,    MAC 32 bits/clock
  "ksg5_1 ksg_aes RPC=1"            # keystream generators alone
  "ksg5_2 ksg_aes RPC=2"
  "ksg4_1 ksg_snow ARCH=1"
  "ksg4_2 ksg_snow ARCH=2"
  "ksg6   ksg_zuc ARCH=1"
)
mkdir -p reports logs
for d in "${DESIGNS[@]}"; do
  set -- $d
  if [ $# -gt 0 ] && [ -n "$ONLY" ] && [[ " $ONLY " != *" $1 "* ]]; then continue; fi
  echo "=== $1 ($(date +%H:%M)) ==="
  rm -f logs/$1.log   # never reuse an old result
  vivado -mode batch -nojournal -log logs/$1.log -source build.tcl -tclargs "$@" > /dev/null 2>&1
  grep "R4 RESULT" logs/$1.log || echo "   failed: see logs/$1.log"
done
echo "done: reports/summary.csv"
