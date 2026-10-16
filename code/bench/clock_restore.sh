#!/usr/bin/env bash
# Undo clock_lock.sh using the settings it saved (no cpupower needed).
set -uo pipefail
[ "$(id -u)" -eq 0 ] || { echo "run with sudo"; exit 1; }
STATE=/var/tmp/r4_clock_state
[ -f "$STATE" ] || { echo "no saved state in $STATE"; exit 1; }
# shellcheck disable=SC1090
. "$STATE"
BOOST_F=/sys/devices/system/cpu/cpufreq/boost
[ -n "${BOOST:-}" ] && [ -f $BOOST_F ] && echo "$BOOST" > $BOOST_F     # boost first: allows the old maximum
for d in /sys/devices/system/cpu/cpu[0-9]*/cpufreq; do
  echo "$MIN" > $d/scaling_min_freq 2>/dev/null
  echo "$MAX" > $d/scaling_max_freq 2>/dev/null
  echo "$MIN" > $d/scaling_min_freq 2>/dev/null                       # again, in case MAX had to come first
  echo "$GOV" > $d/scaling_governor 2>/dev/null
  [ -n "${EPP:-}" ] && [ -f $d/energy_performance_preference ] && echo "$EPP" > $d/energy_performance_preference 2>/dev/null
done
echo "$PARANOID" > /proc/sys/kernel/perf_event_paranoid
echo "restored: governor=$GOV min=${MIN}kHz max=${MAX}kHz boost=${BOOST:-n/a} epp=${EPP:-n/a} paranoid=$PARANOID"
