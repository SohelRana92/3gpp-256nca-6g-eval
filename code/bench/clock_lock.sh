#!/usr/bin/env bash
# Lock the CPU clock for repeatable timing (Linux). Works through the kernel's
# cpufreq files, so cpupower is not needed (Raspberry Pi OS has none).
#
#   sudo ./clock_lock.sh 3800MHz     # Zen 4 (Ryzen 7 7700): nominal clock, boost off
#   sudo ./clock_lock.sh 2400MHz     # Raspberry Pi 5 (Cortex-A76): maximum clock
#   sudo ./clock_restore.sh          # undo everything afterwards
#
# Saves the current settings to /var/tmp/r4_clock_state first.
set -uo pipefail
ARG="${1:?usage: sudo $0 <freq, e.g. 3800MHz or 2400MHz>}"
[ "$(id -u)" -eq 0 ] || { echo "run with sudo"; exit 1; }

case "$ARG" in
  *[Mm][Hh][Zz]) KHZ=$(( ${ARG%[Mm][Hh][Zz]} * 1000 )) ;;
  *[Gg][Hh][Zz]) KHZ=$(awk "BEGIN{printf \"%d\", ${ARG%[Gg][Hh][Zz]} * 1000000}") ;;
  *) KHZ="$ARG" ;;
esac

CPU0=/sys/devices/system/cpu/cpu0/cpufreq
BOOST=/sys/devices/system/cpu/cpufreq/boost
STATE=/var/tmp/r4_clock_state
{
  echo "GOV=$(cat $CPU0/scaling_governor)"
  echo "MIN=$(cat $CPU0/scaling_min_freq)"
  echo "MAX=$(cat $CPU0/scaling_max_freq)"
  [ -f $BOOST ] && echo "BOOST=$(cat $BOOST)"
  [ -f $CPU0/energy_performance_preference ] && echo "EPP=$(cat $CPU0/energy_performance_preference)"
  echo "PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid)"
} > "$STATE"
echo "saved current settings to $STATE"

HWMAX=$(cat $CPU0/cpuinfo_max_freq)
[ -f $BOOST ] && echo 0 > $BOOST                       # boost off (x86)
[ -f $BOOST ] && HWMAX=$(cat $CPU0/cpuinfo_max_freq)
if [ "$KHZ" -gt "$HWMAX" ]; then
  echo "requested $KHZ kHz is above the maximum $HWMAX kHz; using the maximum"
  KHZ=$HWMAX
fi

for d in /sys/devices/system/cpu/cpu[0-9]*/cpufreq; do
  echo performance > $d/scaling_governor 2>/dev/null
  [ -f $d/energy_performance_preference ] && echo performance > $d/energy_performance_preference 2>/dev/null
  echo "$KHZ" > $d/scaling_max_freq 2>/dev/null
  echo "$KHZ" > $d/scaling_min_freq 2>/dev/null
done
echo 1 > /proc/sys/kernel/perf_event_paranoid          # let user programs read the cycle counter

echo "clock locked:"
for f in scaling_governor scaling_min_freq scaling_max_freq scaling_cur_freq; do echo "  $f=$(cat $CPU0/$f)"; done
[ -f $BOOST ] && echo "  boost=$(cat $BOOST)"
echo "  perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
command -v vcgencmd >/dev/null && echo "  $(vcgencmd measure_clock arm)  $(vcgencmd measure_temp)"
true
