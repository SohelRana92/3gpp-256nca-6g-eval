#!/usr/bin/env python3
"""Capture the R4 ESP32 benchmark output from the serial port into files.

usage (in the ESP-IDF terminal, after `idf.py -p COM3 flash`):
    python get_results.py COM3

It resets the board, prints everything it receives, and saves:
    results/esp32_run3_log.txt   the full serial output
    results/esp32_run3.csv       the measurements (between R4-CSV-BEGIN and R4-CSV-END)
It stops at R4-DONE, or at R4-STOP if a self-test fails. Uses pyserial, which
the ESP-IDF Python environment already has.
"""
import os
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
tag = sys.argv[2] if len(sys.argv) > 2 else "run3"
os.makedirs("results", exist_ok=True)
ser = serial.Serial(port, 115200, timeout=1)
try:                                 # reset the board (EN pulse via RTS), boot normally
    ser.dtr = False
    ser.rts = True
    time.sleep(0.2)
    ser.rts = False
except OSError:
    print("note: could not reset the board; press its EN/RST button now")

log, csv, in_csv = [], [], False
start = time.time()
while time.time() - start < 1800:
    raw = ser.readline()
    if not raw:
        continue
    line = raw.decode("utf-8", "replace").rstrip("\r\n")
    print(line, flush=True)
    log.append(line)
    if line.startswith("R4-CSV-BEGIN"):
        in_csv, csv = True, []
    elif line.startswith("R4-CSV-END"):
        in_csv = False
    elif in_csv:
        csv.append(line)
    if line.startswith("R4-DONE") or line.startswith("R4-STOP"):
        break
ser.close()

with open(f"results/esp32_{tag}_log.txt", "w") as f:
    f.write("\n".join(log) + "\n")
if csv:
    with open(f"results/esp32_{tag}.csv", "w") as f:
        f.write("\n".join(csv) + "\n")
print(f"\nsaved results/esp32_{tag}_log.txt" + (f" and results/esp32_{tag}.csv" if csv else " (no CSV: check the log)"))
