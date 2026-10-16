#!/usr/bin/env bash
# Multi-core scaling sweep (Linux). Run `sudo ./clock_lock.sh <freq>` first, as for run_linux.sh.
#
#   ./run_mc.sh zen4        # Zen 4: 1, 2, 4, 8 threads on separate cores, then 16 threads (both SMT threads of every core)
#   ./run_mc.sh pi5         # Pi 5: 1, 2, 4 threads
#
# Threads go to separate physical cores first, starting with core 1 (core 0 takes most
# interrupts), so core 0 is used only from 8 threads on. Output in results/:
#   <platform>_mc.csv        one row per algorithm, size, thread count and repetition
#   <platform>_mc_info.txt   CPU topology, clock settings and temperatures
set -euo pipefail
PLAT="${1:?usage: $0 <platform>}"
cd "$(dirname "$0")"
mkdir -p results
make -s mc

# One logical CPU per physical core, ordered by core id, core 0 last.
mapfile -t FIRST < <(for c in /sys/devices/system/cpu/cpu[0-9]*; do
    n=${c##*cpu}; s=$(cut -d, -f1 "$c/topology/thread_siblings_list" | cut -d- -f1)
    [ "$n" = "$s" ] && echo "$(cat "$c/topology/core_id") $n"; done | sort -n | awk '{print $2}')
PHYS=("${FIRST[@]:1}" "${FIRST[0]}")
NPHYS=${#PHYS[@]}
NLOG=$(nproc --all)

lists=()
for n in 1 2 4 8 16 32; do
  [ "$n" -le "$NPHYS" ] && lists+=("$(IFS=,; echo "${PHYS[*]:0:$n}")")
done
if [ "$NLOG" -gt "$NPHYS" ]; then                     # SMT: add the second thread of every core
  ALL=("${PHYS[@]}")
  for p in "${PHYS[@]}"; do
    for s in $(tr ',' ' ' < /sys/devices/system/cpu/cpu$p/topology/thread_siblings_list | sed 's/-/ /'); do
      [ "$s" != "$p" ] && ALL+=("$s")
    done
  done
  lists+=("$(IFS=,; echo "${ALL[*]}")")
fi

OUT="results/${PLAT}_mc${MC_TAG:+_$MC_TAG}.csv"
INFO="results/${PLAT}_mc${MC_TAG:+_$MC_TAG}_info.txt"
rm -f "$OUT"
{
  echo "== date"; date -Is
  echo "== lscpu"; lscpu
  echo "== cpu lists"; printf '%s\n' "${lists[@]}"
  echo "== cpufreq"; for f in scaling_governor scaling_min_freq scaling_max_freq; do echo "$f=$(cat /sys/devices/system/cpu/cpu0/cpufreq/$f)"; done
  [ -f /sys/devices/system/cpu/cpufreq/boost ] && echo "boost=$(cat /sys/devices/system/cpu/cpufreq/boost)"
  echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
  echo "== temperature before"; (sensors 2>/dev/null || vcgencmd measure_temp 2>/dev/null || true)
} > "$INFO" 2>&1

echo "CPU lists: ${lists[*]}"
echo "about 10-12 minutes ..."
for L in "${lists[@]}"; do
  ./mc --platform "$PLAT" --cpus "$L" --sizes 40,1500,65536 --seconds ${MC_SECONDS:-2} --reps ${MC_REPS:-3} ${MC_ALGS:+--algs "$MC_ALGS"} -o "$OUT"
done
{
  echo "== temperature after"; (sensors 2>/dev/null || vcgencmd measure_temp 2>/dev/null || true)
  command -v vcgencmd >/dev/null && vcgencmd get_throttled || true
} >> "$INFO" 2>&1
echo
echo "done: $OUT and $INFO"
