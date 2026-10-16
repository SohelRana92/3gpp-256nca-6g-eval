#!/usr/bin/env python3
"""Rebuild the merged result files used by the figures and tables, and check them.

  results/zen4/zen4_final_all_levels.csv  = zen4_L1.csv  +  zen4_L2_L3_base_final.csv
  results/pi5/pi5_final_all_levels.csv    = L1 rows of pi5_L1_L2_base_run1.csv  +  pi5_L2_base.csv
  results/esp32/esp32_final.csv           = esp32_run3.csv (all measured code in IRAM)
  results/fpga/fpga_final.csv             = board cycle counts (hw_final/*/hwtest.txt)
                                            + out-of-context area, f_max and power
                                              (ooc_final/reports/summary.csv, last entry per build)

Usage:  python3 analysis/build_final_csv.py           # rebuild into a temp folder and compare
        python3 analysis/build_final_csv.py --write   # overwrite the merged files
The merge only selects and concatenates rows; it changes no measured value.
"""
import os
import re
import sys
import tempfile
import pandas as pd

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RES = os.path.join(ROOT, "results")
KEY = ["level", "alg", "bytes"]


def load(*p):
    return pd.read_csv(os.path.join(RES, *p), comment="#")


def build():
    out = {}
    out["zen4/zen4_final_all_levels.csv"] = pd.concat([load("zen4", "zen4_L1.csv"), load("zen4", "zen4_L2_L3_base_final.csv")])
    r1 = load("pi5", "pi5_L1_L2_base_run1.csv")
    out["pi5/pi5_final_all_levels.csv"] = pd.concat([r1[r1.level == "L1"], load("pi5", "pi5_L2_base.csv")])
    out["esp32/esp32_final.csv"] = load("esp32", "esp32_run3.csv")
    ooc = pd.read_csv(os.path.join(RES, "fpga", "ooc_final", "reports", "summary.csv")).drop_duplicates("tag", keep="last").set_index("tag")
    rows = []
    for tag in ["nca4_A", "nca4_T", "nca5_A", "nca5_T", "nca6_A", "nca6_T"]:
        c = {}
        for line in open(os.path.join(RES, "fpga", "hw_final", tag, "hwtest.txt")):
            m = re.match(r"\s*(NCA|NEA|NIA)\s+(\d+) bytes:\s+(\d+) cycles", line)
            if m:
                c[(m[1], int(m[2]))] = int(m[3])
        o = ooc.loc[tag]
        f = o.fmax_mhz
        nca = 16384 * 8 * f / c[("NCA", 16384)] / 1000
        nea = 16384 * 8 * f / c[("NEA", 16384)] / 1000
        rows.append(dict(tag=tag, lut=o.lut, ff=o.ff, slice=o.slice, bram=o.bram, fmax=f,
                         c0=c[("NCA", 0)], c40=c[("NCA", 40)], c1500=c[("NCA", 1500)], c16k=c[("NCA", 16384)],
                         nea16k=c[("NEA", 16384)], nia1500=c[("NIA", 1500)],
                         nca_gbps=round(nca, 3), nea_gbps=round(nea, 3), mbps_per_slice=round(nca * 1000 / o.slice, 3),
                         lat40_ns=round(c[("NCA", 40)] / f * 1000, 1), lat1500_ns=round(c[("NCA", 1500)] / f * 1000, 1),
                         dyn_w_fmax=o.dyn_w_fmax, tot_w_fmax=o.total_w_fmax,
                         dyn_pJ_B=round(o.dyn_w_fmax * 8000 / nca, 1), tot_pJ_B=round(o.total_w_fmax * 8000 / nca, 1),
                         dyn_w_100=o.dyn_w_100))
    out["fpga/fpga_final.csv"] = pd.DataFrame(rows)
    return out


def same(a, b, key):
    a = a.sort_values(key).reset_index(drop=True)
    b = b.sort_values(key).reset_index(drop=True)
    if list(a.columns) != list(b.columns) or len(a) != len(b):
        return False
    num = a.select_dtypes("number").columns
    return ((a[num] - b[num]).abs().fillna(0) <= 0.051).all().all() and (a.drop(columns=num).astype(str) == b.drop(columns=num).astype(str)).all().all()


ok = True
for path, df in build().items():
    ref = pd.read_csv(os.path.join(RES, path), comment="#")
    key = ["tag"] if path.startswith("fpga") else KEY
    s = same(df, ref, key)
    ok &= s
    print(f"{path:42s} {len(df):4d} rows  {'matches' if s else 'DIFFERS'}")
    if "--write" in sys.argv:
        df.to_csv(os.path.join(RES, path), index=False)
sys.exit(0 if ok else 1)
