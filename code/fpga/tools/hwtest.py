#!/usr/bin/env python3
"""Send test packets to hwtest_top on the Arty S7-25 and check every reply.

usage: python3 hwtest.py <port> <vector file> [<timing file>]
   e.g. python3 hwtest.py /dev/ttyUSB1 ../tb/vectors/vectors_nca5.txt ../tb/vectors/timing_nca5.txt

The vector files come from the verified L1 C code (tools/gen_vectors.c).
Prints one line per timing packet with the clock cycles counted on the FPGA.
"""
import struct
import sys
import time

import serial


def packets(path):
    with open(path) as f:
        lines = [l.split() for l in f if l.strip()]
    i = 0
    while lines[i][0] != "Z":
        p = {"P": list(map(int, lines[i][1:])), "A": [], "M": [], "C": [], "T": None}
        i += 1
        while lines[i][0] != "E":
            t, v = lines[i][0], bytes.fromhex(lines[i][1])
            if t in ("A", "M", "C"):
                p[t].append(v)
            else:
                p[t] = v
            i += 1
        i += 1
        yield p


def run(ser, p):
    mac, mode, abits, mbits = p["P"]
    msg = b"R" + bytes([mac, mode]) + struct.pack("<II", abits, mbits) + p["K"] + p["I"] + b"".join(p["A"] + p["M"])
    ser.write(msg)
    nout = len(p["C"])
    want = 1 + 4 + 16 * nout + 16
    got = b""
    t0 = time.time()
    while len(got) < want and time.time() - t0 < 20:
        got += ser.read(want - len(got))
    if len(got) != want or got[0:1] != b"D":
        return None, False
    cycles = struct.unpack("<I", got[1:5])[0]
    out, tag = got[5:5 + 16 * nout], got[5 + 16 * nout:]
    ok = out == b"".join(p["C"]) and (tag == p["T"] if mac else True)
    return cycles, ok


def main():
    port, files = sys.argv[1], sys.argv[2:]
    ser = serial.Serial(port, 1000000, timeout=0.5)
    time.sleep(0.2)
    ser.reset_input_buffer()
    bad = total = 0
    for k, path in enumerate(files):
        for p in packets(path):
            cycles, ok = run(ser, p)
            total += 1
            if cycles is None:
                print("no reply from the board"); return 1
            if not ok:
                bad += 1
                print("MISMATCH", p["P"])
            if k == 1:
                mac, mode, abits, mbits = p["P"]
                kind = "NEA" if not mac else ("NIA" if mbits == 0 and abits else "NCA")
                print(f"  {kind} {max(abits, mbits) // 8:6d} bytes: {cycles:7d} cycles")
    print(f"{total} packets, {bad} mismatches" + ("  -> ALL MATCH ON THE FPGA" if bad == 0 else ""))
    return bad != 0


if __name__ == "__main__":
    sys.exit(main())
