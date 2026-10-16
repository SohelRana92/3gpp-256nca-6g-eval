#!/usr/bin/env python3
"""Fig. 1 - Structure of the 3GPP 256-AEAD1 framework (TS 35.243, clause 5).

Conceptual block diagram; no measured data. Grayscale.
Run:  python3 fig1_aead1.py    ->  fig1_aead1.pdf, fig1_aead1.png (300 dpi)
"""
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

plt.rcParams.update({"font.family": "serif", "font.size": 8})
fig, ax = plt.subplots(figsize=(6.5, 2.9))
ax.set_xlim(0, 100)
ax.set_ylim(0, 46)
ax.axis("off")


def box(x, y, w, h, text, fill="white", bold=False, fs=8):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.4,rounding_size=1.2",
                                fc=fill, ec="black", lw=0.9))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center",
            fontweight="bold" if bold else "normal", fontsize=fs)


def arrow(x1, y1, x2, y2, label=None, lx=0, ly=1.4):
    ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle="-|>", lw=0.8, color="black", shrinkA=0, shrinkB=0))
    if label:
        ax.text((x1 + x2) / 2 + lx, (y1 + y2) / 2 + ly, label, ha="center", va="bottom", fontsize=7)


def xor(x, y):
    ax.add_patch(plt.Circle((x, y), 1.6, fc="white", ec="black", lw=0.9))
    ax.plot([x - 1.6, x + 1.6], [y, y], color="black", lw=0.8)
    ax.plot([x, x], [y - 1.6, y + 1.6], color="black", lw=0.8)


# Inputs
box(0.8, 35.5, 20, 8, "COUNT, BEARER,\nDIRECTION, EXTRA_IV,\nMAC_BYTES, CF", fs=7)
box(24.5, 36.5, 11, 6, "Make_5GIV")
arrow(21.2, 39.5, 24.1, 39.5)
box(0.8, 25, 20, 6, "256-bit KEY")

# Keystream generator
box(44, 25, 19, 18, "KSG\n(Snow 5G, AES-256\nor ZUC-256)", fill="0.88", bold=True)
arrow(35.9, 39.5, 43.6, 39.5)
ax.text(39.7, 37.6, "IV", ha="center", va="top", fontsize=7)
arrow(21.2, 28, 43.6, 28)

# Encryption path
xor(78, 34)
box(84.5, 30.8, 14, 6.4, "Ciphertext")
arrow(63.4, 34, 76.4, 34, "keystream z", ly=0.9)
box(70.5, 40.5, 15, 4.5, "Plaintext")
arrow(78, 40.1, 78, 35.6)
arrow(79.6, 34, 84.1, 34)

# H, Q, P and Mac5G
box(40, 3, 46, 13, "", fill="0.95")
ax.text(63, 14, "Mac5G", ha="center", va="center", fontweight="bold")
box(42, 4.5, 13, 6.5, "AAD, then\nciphertext blocks", fs=7)
box(58.5, 4.5, 11.5, 6.5, "pdot with H\n(POLYVAL)", fs=7)
box(73, 4.5, 11.5, 6.5, "+ lengths,\npdot with Q,\nXOR P", fs=7)
arrow(55.4, 7.75, 58.1, 7.75)
arrow(70.4, 7.75, 72.6, 7.75)
arrow(53.5, 24.6, 53.5, 16.4, "H, Q, P", lx=4.5, ly=-1.5)
ax.plot([91.5, 91.5], [30.4, 13], color="black", lw=0.8)
arrow(91.5, 13, 86.6, 13)
ax.text(93, 21, "ciphertext\nto MAC", ha="left", va="center", fontsize=7)
box(88.5, 1.5, 10.5, 5.5, "Tag\n4-16 bytes")
arrow(84.9, 6.0, 88.1, 4.6)

fig.tight_layout(pad=0.2)
fig.savefig("fig1_aead1.pdf")
fig.savefig("fig1_aead1.png", dpi=300)
print("saved fig1_aead1.pdf and fig1_aead1.png")
