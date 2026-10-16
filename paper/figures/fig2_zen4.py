#!/usr/bin/env python3
"""Fig. 2 (cloud tier) from the measured Zen 4 data.

Input : ../../results/zen4/zen4_final_all_levels.csv  (bench output, all levels)
Output: fig2_zen4.pdf/.png  - cycles per byte against packet size, and the
                              64 KiB cost of each set at L1, L2 and L3
Grayscale, 300 dpi. Every plotted value is a median from the CSV.
"""
import os
import matplotlib.pyplot as plt
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, "..", "..", "results", "zen4", "zen4_final_all_levels.csv")
d = pd.read_csv(CSV)
d = d[~((d.level == "L1") & d.alg.str.startswith("OpenSSL"))]   # baselines come from the final run

plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})
SETS = [("256-NCA5", "AES-256"), ("256-NCA4", "Snow 5G"), ("256-NCA6", "ZUC-256")]
MARK = {"256-NCA5": "o", "256-NCA4": "s", "256-NCA6": "^"}
LINE = {"L2": (0, (4, 2)), "L3": "-"}


def series(alg, level):
    q = d[(d.alg == alg) & (d.level == level) & (d.bytes > 0)].sort_values("bytes")
    return q.bytes.values, q.cpb.values


# ---- Fig. 2 ---------------------------------------------------------------
fig, (ax, bx) = plt.subplots(1, 2, figsize=(6.6, 2.6), gridspec_kw={"width_ratios": [1.6, 1]})
for alg, name in SETS:
    for lev in ["L2", "L3"]:
        x, y = series(alg, lev)
        ax.plot(x, y, color="black", lw=0.9, ls=LINE[lev], marker=MARK[alg], ms=3,
                mfc="white" if lev == "L2" else "black", label=f"{name} {lev}")
x, y = series("OpenSSL-AES-256-GCM", "base")
ax.plot(x, y, color="0.55", lw=1.6, label="OpenSSL AES-256-GCM")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xlabel("Packet size (bytes)")
ax.set_ylabel("Cycles per byte")
ax.set_title("(a) Encryption and integrity (256-NCA)", fontsize=8)
ax.grid(True, which="major", color="0.88", lw=0.5)
ax.legend(fontsize=6, ncol=2, frameon=False, loc="upper right")

levels = ["L1", "L2", "L3"]
fills = {"L1": "white", "L2": "0.6", "L3": "black"}
w = 0.26
for i, (alg, name) in enumerate(SETS):
    for j, lev in enumerate(levels):
        cpb = d[(d.alg == alg) & (d.level == lev) & (d.bytes == 65536)].cpb.values[0]
        bx.bar(i + (j - 1) * w, cpb, w, color=fills[lev], edgecolor="black", lw=0.6,
               label=lev if i == 0 else None)
gcm = d[(d.alg == "OpenSSL-AES-256-GCM") & (d.bytes == 65536)].cpb.values[0]
bx.axhline(gcm, color="0.55", lw=1.2, ls="--")
bx.text(2.45, gcm * 1.15, "GCM", fontsize=6, color="0.35", ha="right")
bx.set_yscale("log")
bx.set_xticks(range(3))
bx.set_xticklabels([n for _, n in SETS])
bx.set_ylabel("Cycles per byte at 64 KiB")
bx.set_title("(b) Optimization levels", fontsize=8)
bx.legend(fontsize=6, frameon=False, loc="upper right")
bx.grid(True, axis="y", which="major", color="0.88", lw=0.5)
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig2_zen4.{ext}"), dpi=300)

print("wrote fig2_zen4")
