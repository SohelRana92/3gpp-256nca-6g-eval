#!/usr/bin/env python3
"""Fig. 8 (cross-tier summary): cost of each 3GPP option relative to the fastest
3GPP option on the same tier and packet size, and the half-performance length n1/2.

Inputs: ../../results/zen4/zen4_final_all_levels.csv   (L2 below 512 B, L3 from 512 B; OpenSSL GCM)
        ../../results/pi5/pi5_final_all_levels.csv    (L2; OpenSSL GCM)
        ../../results/esp32/esp32_final.csv           (L2, HW, mbedTLS AES-256-GCM)
        ../../results/fpga/fpga_final.csv             (f_max of the T engines)
        ../../results/fpga/hw_final/<tag>/hwtest.txt  (board cycle counts)
Output: fig8_summary.pdf/.png
Cost is cycles per packet on the processors and time per packet at f_max on the FPGA.
n1/2 = setup / (cost per byte at 16 KiB): the packet size at which half of the
long-packet speed is reached. Grayscale, 300 dpi.
"""
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "results")
SIZES = [40, 576, 1500, 16384]
plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})


def load(*p):
    return pd.read_csv(os.path.join(RES, *p), comment="#")


def cyc(d, level, alg):
    return d[(d.level == level) & (d.alg == alg)].groupby("bytes").med_cycles.median()


def zen_best(d, alg):
    l2, l3 = cyc(d, "L2", alg), cyc(d, "L3", alg)
    return pd.concat([l2[l2.index < 512], l3[l3.index >= 512]])


def fpga(tag):
    d = pd.read_csv(os.path.join(RES, "fpga", "fpga_final.csv")).set_index("tag")
    c = {}
    for line in open(os.path.join(RES, "fpga", "hw_final", tag, "hwtest.txt")):
        m = re.match(r"\s*NCA\s+(\d+) bytes:\s+(\d+) cycles", line)
        if m:
            c[int(m[1])] = int(m[2])
    return pd.Series(c) / d.loc[tag, "fmax"]          # microseconds per packet


z = load("zen4", "zen4_final_all_levels.csv")
p = load("pi5", "pi5_final_all_levels.csv")
e = load("esp32", "esp32_final.csv")
TIERS = [
    ("Device\n(ESP32)", [("AES-256 HW", cyc(e, "HW", "256-NCA5")), ("AES-256 L2", cyc(e, "L2", "256-NCA5")),
                         ("Snow 5G L2", cyc(e, "L2", "256-NCA4")), ("ZUC-256 L2", cyc(e, "L2", "256-NCA6"))],
     ("AES-256-GCM", cyc(e, "base", "mbedTLS-AES-256-GCM"))),
    ("Edge\n(Pi 5)", [("AES-256", cyc(p, "L2", "256-NCA5")), ("Snow 5G", cyc(p, "L2", "256-NCA4")),
                      ("ZUC-256", cyc(p, "L2", "256-NCA6"))],
     ("AES-256-GCM", cyc(p, "base", "OpenSSL-AES-256-GCM"))),
    ("Cloud\n(Zen 4)", [("AES-256", zen_best(z, "256-NCA5")), ("Snow 5G", zen_best(z, "256-NCA4")),
                        ("ZUC-256", zen_best(z, "256-NCA6"))],
     ("AES-256-GCM", cyc(z, "base", "OpenSSL-AES-256-GCM"))),
    ("Hardware\n(Spartan-7)", [("AES-256 T", fpga("nca5_T")), ("Snow 5G T", fpga("nca4_T")),
                               ("ZUC-256 T", fpga("nca6_T"))], None),
]

rows, vals, nhalf, groups = [], [], [], []
for tier, sets, ref in TIERS:
    best = pd.concat([s[SIZES] for _, s in sets], axis=1).min(axis=1)
    start = len(rows)
    for name, s in sets + ([ref] if ref else []):
        rows.append(name)
        vals.append((s[SIZES] / best).values)
        nhalf.append(s[0] * 16384 / (s[16384] - s[0]))
    groups.append((tier, start, len(rows)))
V = np.array(vals)

fig, ax = plt.subplots(figsize=(3.6, 3.4))
ax.imshow(V, cmap="Greys", norm=LogNorm(vmin=1, vmax=20), aspect="auto")
for i in range(V.shape[0]):
    for j in range(V.shape[1]):
        v = V[i, j]
        ax.text(j, i, f"{v:.2f}" if v < 10 else f"{v:.1f}", ha="center", va="center", fontsize=6.5,
                color="white" if v > 4 else "black")
    ax.text(len(SIZES) - 0.35, i, f"{nhalf[i]:.0f}", ha="left", va="center", fontsize=6.5)
ax.text(len(SIZES) - 0.35, -0.75, "$n_{1/2}$ (B)", ha="left", va="center", fontsize=6.5)
ax.set_xticks(range(len(SIZES)))
ax.set_xticklabels(["40 B", "576 B", "1500 B", "16 KiB"], fontsize=7)
ax.xaxis.tick_top()
ax.set_yticks(range(len(rows)))
ax.set_yticklabels(rows, fontsize=6.5)
ax.tick_params(length=0)
for tier, a, b in groups:
    if a:
        ax.axhline(a - 0.5, color="black", lw=1.2)
    ax.text(-2.55, (a + b - 1) / 2, tier, ha="center", va="center", fontsize=6.5, rotation=90)
for s in ax.spines.values():
    s.set_visible(False)
ax.set_xlim(-0.5, len(SIZES) - 0.5)
fig.subplots_adjust(left=0.36, right=0.86, top=0.92, bottom=0.02)
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig8_summary.{ext}"), dpi=300)
for r, v, n in zip(rows, V, nhalf):
    print(f"{r:14s}", " ".join(f"{x:6.2f}" for x in v), f"n1/2={n:.0f}")
