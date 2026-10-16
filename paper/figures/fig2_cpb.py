#!/usr/bin/env python3
"""Fig. 2: cycles per byte of 256-NCA against packet size on the three processor tiers.

Inputs: ../../results/esp32/esp32_final.csv          (L2, HW, mbedTLS AES-256-GCM)
        ../../results/pi5/pi5_final_all_levels.csv   (L2, OpenSSL AES-256-GCM)
        ../../results/zen4/zen4_final_all_levels.csv (L2, L3, OpenSSL AES-256-GCM)
Output: fig2_cpb.pdf/.png. Grayscale, 300 dpi. Every plotted value is a median from the CSV.
Replaces the per-tier figures fig2_zen4, fig3_pi5 and fig4_esp32 (panel b of those
figures repeated the 64 KiB values of the result tables).
"""
import os
import matplotlib.pyplot as plt
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "results")
plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})
SETS = [("256-NCA5", "AES-256"), ("256-NCA4", "Snow 5G"), ("256-NCA6", "ZUC-256")]
MARK = {"256-NCA5": "o", "256-NCA4": "s", "256-NCA6": "^"}


def load(*p):
    d = pd.read_csv(os.path.join(RES, *p), comment="#")
    return d[~((d.level == "L1") & ~d.alg.str.startswith("256-"))]


def series(d, alg, level):
    q = d[(d.alg == alg) & (d.level == level) & (d.bytes > 0) & (d.bytes <= 65536)].groupby("bytes").cpb.median()
    if level == "L3":                                   # L3 hands packets below 512 B to L2
        q = q[q.index >= 512]
    return q.index.values, q.values


PANELS = [
    ("(a) Device tier (ESP32)", load("esp32", "esp32_final.csv"), ["L2", "HW"], "mbedTLS-AES-256-GCM"),
    ("(b) Edge tier (Pi 5)", load("pi5", "pi5_final_all_levels.csv"), ["L2"], "OpenSSL-AES-256-GCM"),
    ("(c) Cloud tier (Zen 4)", load("zen4", "zen4_final_all_levels.csv"), ["L2", "L3"], "OpenSSL-AES-256-GCM"),
]
STYLE = {"L2": dict(ls="-", mfc="white"), "L3": dict(ls="-", mfc="black"), "HW": dict(ls=(0, (4, 2)), mfc="black")}

fig, axes = plt.subplots(1, 3, figsize=(6.6, 2.55), sharey=False)
for ax, (title, d, levels, gcm) in zip(axes, PANELS):
    for alg, name in SETS:
        for lev in levels:
            x, y = series(d, alg, lev)
            if len(x) == 0:
                continue
            ax.plot(x, y, color="black", lw=0.8, marker=MARK[alg], ms=2.8, label=f"{name} {lev}", **STYLE[lev])
    x, y = series(d, gcm, "base")
    ax.plot(x, y, color="0.55", lw=1.6, label="AES-256-GCM")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xticks([16, 256, 4096, 65536])
    ax.set_xlim(11, 95000)
    ax.set_xticklabels(["16", "256", "4K", "64K"])
    ax.set_xlabel("Packet size (bytes)")
    ax.set_title(title, fontsize=8)
    ax.grid(True, which="major", color="0.88", lw=0.5)
axes[0].set_ylabel("Cycles per byte")
from matplotlib.lines import Line2D
H = [Line2D([], [], color="black", lw=0.8, marker=MARK[a], ms=3, mfc="white", label=n) for a, n in SETS]
H += [Line2D([], [], color="black", lw=0.8, marker="o", ms=3, mfc="white", label="open: L2"),
      Line2D([], [], color="black", lw=0.8, marker="o", ms=3, mfc="black", label="filled: L3 (Zen 4)"),
      Line2D([], [], color="black", lw=0.8, ls=(0, (4, 2)), marker="o", ms=3, mfc="black", label="dashed: HW (ESP32)"),
      Line2D([], [], color="0.55", lw=1.6, label="AES-256-GCM")]
fig.legend(handles=H, loc="upper center", ncol=7, fontsize=6, frameon=False, handlelength=2.0, columnspacing=1.0)
fig.tight_layout(w_pad=0.6, rect=(0, 0, 1, 0.92))
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig2_cpb.{ext}"), dpi=300)
print("wrote fig2_cpb")
