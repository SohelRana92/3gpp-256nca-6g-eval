#!/usr/bin/env python3
"""Energy per byte on the Raspberry Pi 5 from the on-board PMIC (R4 edge tier).

The Pi 5 power-management chip reports current and voltage for each supply
rail (`vcgencmd pmic_read_adc`). For each case we:
  1. sample all rails for S seconds with the benchmark idle,
  2. run the benchmark on one pinned core for S seconds (bench --run-seconds)
     and sample the rails again,
  3. take energy per byte = (P_active - P_idle) * seconds / payload bytes.
We repeat each case R times, alternating idle and active windows, so slow
drift (temperature, background load) affects both windows alike. The sampler
runs on its own core in both windows, so its cost cancels.

We report the VDD_CORE rail (the CPU cores) and the sum of all reported rails.

usage:  sudo ./clock_lock.sh 2400MHz
        python3 energy_pi.py                   # about 15 minutes
        sudo ./clock_restore.sh
output: results/pi5_energy.csv and results/pi5_energy_summary.txt
"""
import argparse
import os
import re
import statistics
import subprocess
import threading
import time

SETS = {
    "r4":  [("L2", "256-NCA4"), ("L2", "256-NCA5"), ("L2", "256-NCA6"), ("base", "OpenSSL-AES-256-GCM")],
    "ref": [("base", "OpenSSL-ChaCha20-Poly1305"), ("base", "libaegis-AEGIS-256")],
}
SIZES = (40, 1500, 65536)
CASES = []

LINE = re.compile(r"^\s*(\S+)_(A|V)\s+(current|volt)\(\d+\)=([0-9.]+)")


def read_rails():
    out = subprocess.run(["vcgencmd", "pmic_read_adc"], capture_output=True, text=True).stdout
    amps, volts = {}, {}
    for line in out.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        name, kind, val = m.group(1), m.group(3), float(m.group(4))
        (amps if kind == "current" else volts)[name] = val
    core = amps.get("VDD_CORE", 0.0) * volts.get("VDD_CORE", 0.0)
    total = sum(a * volts[n] for n, a in amps.items() if n in volts)
    return core, total


class Sampler(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.samples, self.on, self.stop = [], False, False

    def run(self):
        while not self.stop:
            c, t = read_rails()
            if self.on:
                self.samples.append((time.monotonic(), c, t))

    def window(self, seconds, skip=0.5):
        self.samples = []
        self.on = True
        time.sleep(seconds)
        self.on = False
        t0 = self.samples[0][0] if self.samples else 0
        s = [x for x in self.samples if x[0] - t0 >= skip]
        return s


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def temp():
    out = subprocess.run(["vcgencmd", "measure_temp"], capture_output=True, text=True).stdout
    m = re.search(r"([0-9.]+)", out)
    return float(m.group(1)) if m else float("nan")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpu", type=int, default=3, help="core for the benchmark")
    ap.add_argument("--sampler-cpu", type=int, default=0, help="core for the PMIC sampler")
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--out", default="results/pi5_energy.csv")
    ap.add_argument("--cases", choices=["r4", "ref", "all"], default="r4",
                    help="r4: the three 3GPP sets and GCM (default); ref: ChaCha20-Poly1305 and AEGIS-256; all: both")
    a = ap.parse_args()
    global CASES
    pick = SETS["r4"] + SETS["ref"] if a.cases == "all" else SETS[a.cases]
    CASES = [(lev, alg, n) for lev, alg in pick for n in SIZES]
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    os.sched_setaffinity(0, {a.sampler_cpu})
    if not os.path.exists("./bench"):
        subprocess.run(["make", "-s"], check=True)

    s = Sampler()
    s.start()
    rows = []
    hdr = ("platform,level,alg,bytes,rep,seconds,calls,payload_bytes,idle_core_w,active_core_w,"
           "idle_total_w,active_total_w,n_idle,n_active,temp_c,nj_per_byte_core,nj_per_byte_total")
    with open(a.out, "w") as f:
        f.write(f"# R4 Pi 5 energy, {time.strftime('%Y-%m-%dT%H:%M:%S')}, PMIC rails via vcgencmd pmic_read_adc, "
                f"bench core {a.cpu}, sampler core {a.sampler_cpu}, {a.seconds} s windows\n")
        f.write(hdr + "\n")
        for rep in range(a.reps):
            for lev, alg, n in CASES:
                idle = s.window(a.seconds)
                cmd = ["./bench", "--levels", lev, "--algs", alg, "--sizes", str(n),
                       "--cpu", str(a.cpu), "--run-seconds", str(a.seconds)]
                s.samples, s.on = [], True
                out = subprocess.run(cmd, capture_output=True, text=True)
                s.on = False
                if out.returncode != 0:
                    print("bench failed:", out.stderr)
                    continue
                act = s.samples
                t0 = act[0][0] if act else 0
                act = [x for x in act if x[0] - t0 >= 0.5]
                kv = dict(p.split("=") for p in out.stdout.split())
                sec, calls, pb = float(kv["seconds"]), int(kv["calls"]), int(kv["payload_bytes"])
                ic, it = mean([x[1] for x in idle]), mean([x[2] for x in idle])
                ac, at = mean([x[1] for x in act]), mean([x[2] for x in act])
                ec = (ac - ic) * sec / pb * 1e9
                et = (at - it) * sec / pb * 1e9
                row = (f"pi5,{lev},{alg},{n},{rep},{sec:.4f},{calls},{pb},{ic:.4f},{ac:.4f},{it:.4f},{at:.4f},"
                       f"{len(idle)},{len(act)},{temp():.1f},{ec:.4f},{et:.4f}")
                f.write(row + "\n")
                f.flush()
                rows.append((lev, alg, n, ec, et, ac - ic))
                print(f"rep {rep}  {lev:4s} {alg:20s} {n:6d} B  dP_core {ac - ic:6.3f} W  "
                      f"{ec:9.3f} nJ/B core  {et:9.3f} nJ/B all rails  ({len(act)} samples)", flush=True)
    s.stop = True

    summ = os.path.splitext(a.out)[0] + "_summary.txt"
    with open(summ, "w") as g:
        g.write("level alg bytes  median nJ/B (core)  median nJ/B (all rails)  median dP core (W)\n")
        for lev, alg, n in CASES:
            r = [x for x in rows if x[:3] == (lev, alg, n)]
            if r:
                g.write(f"{lev} {alg} {n}  {statistics.median(x[3] for x in r):.4f}  "
                        f"{statistics.median(x[4] for x in r):.4f}  {statistics.median(x[5] for x in r):.4f}\n")
    print("done:", a.out, "and", summ)


if __name__ == "__main__":
    main()
