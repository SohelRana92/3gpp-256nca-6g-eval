#!/usr/bin/env bash
# Run the full R4 sweep on a Linux platform and save all outputs.
#
#   ./run_linux.sh zen4 2              # platform label, logical CPU; all levels
#   ./run_linux.sh zen4 2 L2,base      # only L2 and the OpenSSL baselines
#   ./run_linux.sh pi5 3               # Raspberry Pi 5: L1, L2 and OpenSSL on core 3
#
# Run `sudo ./clock_lock.sh <freq>` first. Output goes to results/:
#   <platform>_<levels>.csv     the measurements
#   <platform>_sysinfo.txt      CPU, clock, compiler, kernel, OpenSSL
set -euo pipefail
PLAT="${1:?usage: $0 <platform> [cpu]}"
CPU="${2:-2}"
LEVELS="${3:-L1,L2,base}"
cd "$(dirname "$0")"
mkdir -p results

make -s clean && make -s

INFO="results/${PLAT}_sysinfo.txt"
{
  echo "== date";      date -Is
  echo "== lscpu";     lscpu
  echo "== os";        (lsb_release -ds 2>/dev/null || cat /etc/os-release); uname -a
  echo "== gcc";       gcc --version | head -1
  echo "== openssl";   openssl version 2>/dev/null || true
  echo "== cpufreq cpu$CPU"
  for f in scaling_driver scaling_governor scaling_min_freq scaling_max_freq scaling_cur_freq energy_performance_preference; do
    p=/sys/devices/system/cpu/cpu$CPU/cpufreq/$f; [ -f "$p" ] && echo "$f=$(cat "$p")"
  done
  [ -f /sys/devices/system/cpu/cpufreq/boost ] && echo "boost=$(cat /sys/devices/system/cpu/cpufreq/boost)"
  echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
  [ -f /sys/devices/system/cpu/smt/control ] && echo "smt=$(cat /sys/devices/system/cpu/smt/control)"
  command -v vcgencmd >/dev/null && { vcgencmd measure_clock arm; vcgencmd measure_temp; vcgencmd get_throttled; } || true
  echo "== load";      uptime
} > "$INFO" 2>&1

# Warn (but continue) if the clock is not locked.
B=/sys/devices/system/cpu/cpufreq/boost
if [ -f "$B" ] && [ "$(cat $B)" != "0" ]; then
  echo "WARNING: boost is on, results will not be repeatable. Run: sudo ./clock_lock.sh <freq>"
fi
if [ "$(cat /proc/sys/kernel/perf_event_paranoid)" -gt 2 ]; then
  echo "WARNING: perf counter blocked, only ns will be recorded. clock_lock.sh fixes this."
fi

OUT="results/${PLAT}_$(echo "$LEVELS" | tr ',' '_').csv"
echo "running sweep on CPU $CPU (about 5-10 minutes) ..."
./bench --platform "$PLAT" --cpu "$CPU" --levels "$LEVELS" --reps 31 --min-ms 2 -o "$OUT"
command -v vcgencmd >/dev/null && { echo "== after run" >> "$INFO"; vcgencmd measure_temp >> "$INFO"; vcgencmd get_throttled >> "$INFO"; } || true
echo
echo "done: $OUT and $INFO"
