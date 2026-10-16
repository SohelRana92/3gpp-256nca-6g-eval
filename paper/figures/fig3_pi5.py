#!/usr/bin/env python3
"""Fig. 3 (edge tier) and Fig. 5 (setup share, all three software tiers) from measured data.

Input : ../../results/pi5/pi5_final_all_levels.csv   (L1 from run 1, L2 and OpenSSL from run 2)
        ../../results/esp32/esp32_final.csv           (run 3)
        ../../results/zen4/zen4_final_all_levels.csv
Output: fig3_pi5.pdf/.png, fig5_setup.pdf/.png
Grayscale, 300 dpi. Every plotted value is a median from the CSV.
"""
import os
import matplotlib.pyplot as plt
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "results")
p = pd.read_csv(os.path.join(RES, "pi5", "pi5_final_all_levels.csv"))
e = pd.read_csv(os.path.join(RES, "esp32", "esp32_final.csv"))
z = pd.read_csv(os.path.join(RES, "zen4", "zen4_final_all_levels.csv"))
z = z[~((z.level == "L1") & z.alg.str.startswith("OpenSSL"))]

plt.rcParams.update({"font.family": "serif", "font.size": 8, "axes.linewidth": 0.6})
SETS = [("256-NCA5", "AES-256"), ("256-NCA4", "Snow 5G"), ("256-NCA6", "ZUC-256")]
MARK = {"256-NCA5": "o", "256-NCA4": "s", "256-NCA6": "^"}


def series(d, alg, level):
    q = d[(d.alg == alg) & (d.level == level) & (d.bytes > 0)].sort_values("bytes")
    return q.bytes.values, q.cpb.values


# ---- Fig. 3 ---------------------------------------------------------------
fig, (ax, bx) = plt.subplots(1, 2, figsize=(6.6, 2.6), gridspec_kw={"width_ratios": [1.6, 1]})
for alg, name in SETS:
    x, y = series(p, alg, "L2")
    ax.plot(x, y, color="black", lw=0.9, marker=MARK[alg], ms=3, mfc="black", label=f"{name} L2")
x, y = series(p, "OpenSSL-AES-256-GCM", "base")
ax.plot(x, y, color="0.55", lw=1.6, label="OpenSSL AES-256-GCM")
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xlabel("Packet size (bytes)")
ax.set_ylabel("Cycles per byte")
ax.set_title("(a) Encryption and integrity (256-NCA)", fontsize=8)
ax.grid(True, which="major", color="0.88", lw=0.5)
ax.legend(fontsize=6, frameon=False, loc="upper right")

fills = {"L1": "white", "L2": "black"}
w = 0.36
for i, (alg, name) in enumerate(SETS):
    for j, lev in enumerate(["L1", "L2"]):
        cpb = p[(p.alg == alg) & (p.level == lev) & (p.bytes == 65536)].cpb.values[0]
        bx.bar(i + (j - 0.5) * w, cpb, w, color=fills[lev], edgecolor="black", lw=0.6,
               label=lev if i == 0 else None)
gcm = p[(p.alg == "OpenSSL-AES-256-GCM") & (p.bytes == 65536)].cpb.values[0]
bx.axhline(gcm, color="0.55", lw=1.2, ls="--")
bx.text(2.45, gcm * 0.62, "GCM", fontsize=6, color="0.35", ha="right")
bx.set_yscale("log")
bx.set_xticks(range(3))
bx.set_xticklabels([n for _, n in SETS])
bx.set_ylabel("Cycles per byte at 64 KiB")
bx.set_ylim(top=2500)
bx.set_title("(b) Optimization levels", fontsize=8)
bx.legend(fontsize=6, frameon=False, loc="upper right")
bx.grid(True, axis="y", which="major", color="0.88", lw=0.5)
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig3_pi5.{ext}"), dpi=300)

# ---- Fig. 5: setup share, device, edge and cloud --------------------------
sizes = [16, 40, 64, 128, 256, 576, 1024, 1500, 4096, 9000]
fig, axes = plt.subplots(1, 3, figsize=(6.9, 2.4), sharey=True)
panels = [(axes[0], e, "(a) Device tier (ESP32)", "mbedTLS-AES-256-GCM", "mbedTLS AES-256-GCM"),
          (axes[1], p, "(b) Edge tier (Pi 5)", "OpenSSL-AES-256-GCM", "OpenSSL AES-256-GCM"),
          (axes[2], z, "(c) Cloud tier (Zen 4)", "OpenSSL-AES-256-GCM", "OpenSSL AES-256-GCM")]
for ax, d, title, ref, refname in panels:
    for alg, name in SETS + [(ref, refname)]:
        lev = "base" if alg == ref else "L2"
        q = d[(d.alg == alg) & (d.level == lev)].set_index("bytes").med_cycles
        share = [100 * q[0] / q[s_] for s_ in sizes]
        if alg == ref:
            ax.plot(sizes, share, color="0.55", lw=1.6, label="AES-256-GCM (library)")
        else:
            ax.plot(sizes, share, color="black", lw=0.9, marker=MARK[alg], ms=3, mfc="white", label=f"{name} L2")
    ax.set_xscale("log", base=2)
    ax.set_ylim(0, 100)
    ax.set_xlabel("Packet size (bytes)")
    ax.set_title(title, fontsize=8)
    ax.grid(True, color="0.88", lw=0.5)
axes[0].set_ylabel("Setup share of total cost (%)")
axes[2].legend(fontsize=6, frameon=False, loc="upper right")
fig.tight_layout()
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(HERE, f"fig5_setup.{ext}"), dpi=300)
print("wrote fig3_pi5 and fig5_setup")
