#!/usr/bin/env python3
"""Fig. 4 (device tier) from the measured ESP32 data.

Input : ../../results/esp32/esp32_final.csv  (run 3: all measured code in IRAM)
Output: fig4_esp32.pdf/.png
Grayscale, 300 dpi. Every plotted value is a median from the CSV.
"""
import os
import matplotlib.pyplot as plt
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
d = pd.read_csv(os.path.join(HERE, "..", "..", "results", "esp32", "esp32_final.csv"))
plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})
SETS = [("256-NCA5", "AES-256"), ("256-NCA4", "Snow 5G"), ("256-NCA6", "ZUC-256")]
MARK = {"256-NCA5": "o", "256-NCA4": "s", "256-NCA6": "^"}


def series(alg, level):
    q = d[(d.alg == alg) & (d.level == level) & (d.bytes > 0)].sort_values("bytes")
    return q.bytes.values, q.cpb.values


fig, (ax, bx) = plt.subplots(1, 2, figsize=(6.6, 2.6), gridspec_kw={"width_ratios": [1.6, 1]})
for alg, name in SETS:
    x, y = series(alg, "L2")
    ax.plot(x, y, color="black", lw=0.9, marker=MARK[alg], ms=3, mfc="black", label=f"{name} L2")
x, y = series("256-NCA5", "HW")
ax.plot(x, y, color="black", lw=0.9, ls=(0, (4, 2)), marker="o", ms=3, mfc="white", label="AES-256 HW")
x, y = series("mbedTLS-AES-256-GCM", "base")
ax.plot(x, y, color="0.55", lw=1.6, label="mbedTLS AES-256-GCM")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xlabel("Packet size (bytes)")
ax.set_ylabel("Cycles per byte")
ax.set_title("(a) Encryption and integrity (256-NCA)", fontsize=8)
ax.grid(True, which="major", color="0.88", lw=0.5)
ax.legend(fontsize=6, frameon=False, loc="upper right")

fills = {"L1": "white", "L2": "0.6", "HW": "black"}
w = 0.26
for i, (alg, name) in enumerate(SETS):
    for j, lev in enumerate(["L1", "L2", "HW"]):
        q = d[(d.alg == alg) & (d.level == lev) & (d.bytes == 16384)]
        if q.empty:
            continue
        bx.bar(i + (j - 1) * w, q.cpb.values[0], w, color=fills[lev], edgecolor="black", lw=0.6,
               label=lev if i == 0 else None)
gcm = d[(d.alg == "mbedTLS-AES-256-GCM") & (d.bytes == 16384)].cpb.values[0]
bx.axhline(gcm, color="0.55", lw=1.2, ls="--")
bx.text(2.45, gcm * 1.15, "GCM", fontsize=6, color="0.35", ha="right")
bx.set_yscale("log")
bx.set_ylim(top=20000)
bx.set_xticks(range(3))
bx.set_xticklabels([n for _, n in SETS])
bx.set_ylabel("Cycles per byte at 16 KiB")
bx.set_title("(b) Optimization levels", fontsize=8)
bx.legend(fontsize=6, frameon=False, loc="upper right", ncol=3)
bx.grid(True, axis="y", which="major", color="0.88", lw=0.5)
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig4_esp32.{ext}"), dpi=300)
print("wrote fig4_esp32")
