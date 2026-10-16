#!/bin/bash
# hw_test_all.sh - build, program and test every engine on the Arty S7-25 (board on USB).
#   ./hw_test_all.sh [/dev/ttyUSB1]          (about 10 minutes per design)
# Needs Vivado in PATH and python3 with pyserial (sudo apt install python3-serial).
# Your user must be allowed to open the serial port (sudo usermod -aG dialout $USER, then log in again).
cd "$(dirname "$0")"
PORT=${1:-/dev/ttyUSB1}
DESIGNS=("nca5_A SET=5 ARCH=1 D=8" "nca5_T SET=5 ARCH=2 D=32" "nca4_A SET=4 ARCH=1 D=8"
         "nca4_T SET=4 ARCH=2 D=32" "nca6_A SET=6 ARCH=1 D=8" "nca6_T SET=6 ARCH=1 D=32")
command -v vivado >/dev/null || { echo "vivado not in PATH: source <Vivado>/2025.2/Vivado/settings64.sh"; exit 1; }
mkdir -p hw logs
for d in "${DESIGNS[@]}"; do
  set -- $d; tag=$1; s=${2#SET=}
  if [ -n "$ONLY" ] && [[ " $ONLY " != *" $tag "* ]]; then continue; fi
  echo "=== $tag ($(date +%H:%M)) ==="
  rm -f logs/hw_$tag.log logs/prog_$tag.log hw/$tag/$tag.bit hw/$tag/hwtest.txt   # never reuse an old result
  vivado -mode batch -nojournal -log logs/hw_$tag.log -source hw_build.tcl -tclargs "$@" > /dev/null 2>&1
  grep "R4 HWBUILD" logs/hw_$tag.log || { echo "   build failed: see logs/hw_$tag.log"; tail -5 logs/hw_$tag.log 2>/dev/null; continue; }
  vivado -mode batch -nojournal -log logs/prog_$tag.log -source hw_program.tcl -tclargs hw/$tag/$tag.bit > /dev/null 2>&1
  grep "R4 PROGRAMMED" logs/prog_$tag.log || { echo "   programming failed: see logs/prog_$tag.log"; tail -5 logs/prog_$tag.log 2>/dev/null; continue; }
  sleep 1
  python3 ../tools/hwtest.py $PORT ../tb/vectors/vectors_nca$s.txt ../tb/vectors/timing_nca$s.txt | tee hw/$tag/hwtest.txt
done
