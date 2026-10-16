#!/usr/bin/env bash
# Timing-leakage tests (dudect method). Run `sudo ./clock_lock.sh <freq>` first.
#   ./run_ct.sh zen4 2        # platform label, logical CPU (about 12 minutes)
#   ./run_ct.sh pi5 3         # about 12 minutes
#   CT_SECONDS=60 ./run_ct.sh zen4 2   # longer runs (more measurements per test)
# Output: results/<platform>_ct.csv and results/<platform>_ct_info.txt
set -euo pipefail
PLAT="${1:?usage: $0 <platform> [cpu]}"
CPU="${2:-2}"
cd "$(dirname "$0")"
mkdir -p results
make -s ct
INFO="results/${PLAT}_ct_info.txt"
{
  echo "== date"; date -Is
  echo "== cpu"; lscpu | grep -E "Model name|^CPU\(s\)|Thread|MHz"
  echo "== os"; uname -a
  echo "== gcc"; gcc --version | head -1
  echo "== openssl"; openssl version 2>/dev/null || true
  echo "== cpufreq cpu$CPU"; for f in scaling_governor scaling_min_freq scaling_max_freq; do echo "$f=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/$f)"; done
  [ -f /sys/devices/system/cpu/cpufreq/boost ] && echo "boost=$(cat /sys/devices/system/cpu/cpufreq/boost)"
  echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
} > "$INFO" 2>&1
echo "running on CPU $CPU, ${CT_SECONDS:-20} s per test ..."
./ct --platform "$PLAT" --cpu "$CPU" --seconds "${CT_SECONDS:-20}" -o "results/${PLAT}_ct.csv"
echo
echo "done: results/${PLAT}_ct.csv and $INFO"
echo "The two control rows must read LEAK (leaky-early-exit, key) and pass (branch-free)."
