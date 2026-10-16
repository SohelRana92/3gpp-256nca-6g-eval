#!/bin/bash
# sim.sh - analyse all sources and run the packet tests in GHDL.
#   ./sim.sh             all sets and variants
set -e
cd "$(dirname "$0")"
W=work_ghdl; mkdir -p $W
A="ghdl -a --std=08 --workdir=$W"
$A rtl/r4_pkg.vhd rtl/polyval_mac.vhd rtl/aead1_ctrl.vhd
for f in rtl/ksg_aes.vhd rtl/ksg_snow.vhd rtl/ksg_zuc.vhd rtl/nca5_top.vhd rtl/nca4_top.vhd rtl/nca6_top.vhd; do
  if [ -f $f ]; then $A $f; fi
done
$A tb/tb_polyval.vhd tb/tb_nca.vhd
ghdl -e --std=08 --workdir=$W tb_nca
run() { ghdl -r --std=08 --workdir=$W tb_nca -gSET=$1 -gARCH=$2 -gD=$3 -gVEC=tb/vectors/vectors_nca$1.txt 2>&1 | grep -E "note|warning|FAIL|error" | sed 's/^.*(report [a-z]*): //'; }
IFS=";" read -ra LIST <<< "${CFGS:-5 1 8;5 2 32;4 1 8;4 2 32;6 1 8;6 1 32}"
for cfg in "${LIST[@]}"; do run $cfg; done
