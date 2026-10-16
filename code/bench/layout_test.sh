#!/usr/bin/env bash
# Does the relative position of the input and output buffers change the results?
# Runs L2 and L3 encryption + integrity at a few sizes with the output buffer
# at 0, 64, 1024, 2048 and 3072 bytes into a page (the input is at 0).
#
#   ./layout_test.sh zen4 2        (after sudo ./clock_lock.sh 3800MHz)
set -euo pipefail
PLAT="${1:?usage: $0 <platform> [cpu]}"; CPU="${2:-2}"
cd "$(dirname "$0")"; mkdir -p results
make -s clean && make -s
OUT="results/${PLAT}_layout.csv"; : > "$OUT"
for off in 0 64 1024 2048 3072; do
  echo "output offset $off ..."
  ./bench --platform "$PLAT" --cpu "$CPU" --levels L2,L3 --algs 256-NEA5,256-NCA5,256-NCA4 \
          --sizes 1500,4096,65536 --reps 21 --min-ms 2 --out-offset $off -o results/tmp.csv 2>/dev/null
  if [ ! -s "$OUT" ]; then grep -v '^#' results/tmp.csv | head -1 | sed 's/^/out_offset,/' >> "$OUT"; fi
  grep -v '^#' results/tmp.csv | tail -n +2 | sed "s/^/$off,/" >> "$OUT"
done
rm -f results/tmp.csv
echo "done: $OUT"
