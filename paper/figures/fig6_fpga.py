#!/usr/bin/env python3
"""Fig. 6 (hardware tier) from the Vivado reports and the board test.

Inputs: ../../results/fpga/fpga_final.csv                      (area, Fmax, board cycles, power)
        ../../results/fpga/ooc_final/reports/<tag>/utilization_hier.rpt   (LUTs per block)
Output: fig6_fpga.pdf/.png
Grayscale, 300 dpi.
"""
import os
import re
import matplotlib.pyplot as plt
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "results", "fpga")
d = pd.read_csv(os.path.join(RES, "fpga_final.csv")).set_index("tag")
plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})
SETS = [("nca5", "AES-256"), ("nca4", "Snow 5G"), ("nca6", "ZUC-256")]
MARK = {"nca5": "o", "nca4": "s", "nca6": "^"}


def blocks(tag):
    """LUTs of keystream generator, POLYVAL MAC and the rest of the control."""
    v = {}
    for line in open(os.path.join(RES, "ooc_final", "reports", tag, "utilization_hier.rpt")):
        m = re.match(r"\|\s+(ctl|mac|ksg)\s+\|[^|]*\|\s*(\d+)", line)
        if m:
            v[m[1]] = int(m[2])
    return v["ksg"], v["mac"], v["ctl"] - v["mac"]


fig, (ax, bx) = plt.subplots(1, 2, figsize=(6.6, 2.6), gridspec_kw={"width_ratios": [1.2, 1]})
for s, name in SETS:
    a, t = d.loc[f"{s}_A"], d.loc[f"{s}_T"]
    ax.plot([a.slice, t.slice], [a.nca_gbps, t.nca_gbps], color="black", lw=0.8,
            marker=MARK[s], ms=4, mfc="black", label=name)
    ax.annotate("A", (a.slice, a.nca_gbps), xytext=(4, -8), textcoords="offset points", fontsize=6)
    ax.annotate("T", (t.slice, t.nca_gbps), xytext=(4, 2), textcoords="offset points", fontsize=6)
ax.set_xlabel("Slices")
ax.set_ylabel("256-NCA throughput at $f_{\\max}$ (Gbps)")
ax.set_title("(a) Area and throughput", fontsize=8)
ax.set_xlim(600, 1900)
ax.set_ylim(0, 5.6)
ax.grid(True, color="0.88", lw=0.5)
ax.legend(fontsize=6, frameon=False, loc="upper left")

tags, labels = [], []
for s, name in SETS:
    for v in "AT":
        tags.append(f"{s}_{v}")
        labels.append(f"{name.split('-')[0].split(' ')[0]}\n{v}")
k, m, c = zip(*[blocks(t) for t in tags])
x = range(len(tags))
bx.bar(x, k, 0.6, color="white", edgecolor="black", lw=0.6, label="Keystream")
bx.bar(x, m, 0.6, bottom=k, color="0.55", edgecolor="black", lw=0.6, label="POLYVAL MAC")
bx.bar(x, c, 0.6, bottom=[a + b for a, b in zip(k, m)], color="black", edgecolor="black", lw=0.6,
       label="Control")
bx.set_xticks(list(x))
bx.set_xticklabels(labels, fontsize=6)
bx.set_ylabel("LUTs")
bx.set_title("(b) LUTs per block", fontsize=8)
bx.set_ylim(0, 6800)
bx.legend(fontsize=6, frameon=False, loc="upper left", ncol=3, columnspacing=0.8, handlelength=1.2, handletextpad=0.4)
bx.grid(True, axis="y", color="0.88", lw=0.5)
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig6_fpga.{ext}"), dpi=300)
