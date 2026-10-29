#!/usr/bin/env python3
"""Build every supplementary table (LaTeX) and Fig. S5 from the raw CSV files.

Run from any directory:  python3 paper/supplementary/make_tables.py
Inputs : results/**  (see the table captions for the exact file of each table)
Outputs: paper/supplementary/tables/*.tex
No number is typed by hand: every cell comes from a CSV median.
"""
import os
import re
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "..", "results")
OUT = os.path.join(HERE, "tables")
os.makedirs(OUT, exist_ok=True)
SETS = [("AES-256", "5"), ("SNOW 5G", "4"), ("ZUC-256", "6")]


def load(*p):
    return pd.read_csv(os.path.join(RES, *p), comment="#")


def med(d, level, alg, col="med_cycles"):
    return d[(d.level == level) & (d.alg == alg)].groupby("bytes")[col].median()


def f0(x):
    return "--" if pd.isna(x) else f"{x:,.0f}".replace(",", "{,}")


def fc(x):
    if pd.isna(x):
        return "--"
    return f"{x:.3f}" if x < 10 else f"{x:.1f}"


import re as _re
import sys as _sys
_sys.path.insert(0, HERE)
from boldtab import bold_tables  # bold header row and first column

# Concise captions (column abbreviations and the source file are kept).
CAPTIONS = {
    "s_zen4.tex": r"Cloud tier (Zen~4, 3.8~GHz): all modes at every level; L3 runs L2 code below 512~B. Sources: \texttt{zen4\_final\_all\_levels.csv}, \texttt{zen4\_base\_ref.csv}.",
    "s_pi5.tex": r"Edge tier (Pi~5, 2.4~GHz): all modes at L1 and L2. Sources: \texttt{pi5\_final\_all\_levels.csv}, \texttt{pi5\_base\_ref.csv}.",
    "s_esp32.tex": r"Device tier (ESP32, 240~MHz): all modes at L1, L2 and HW. Sources: \texttt{esp32\_final.csv}, \texttt{esp32\_run4.csv}, \texttt{esp32\_run5.csv}.",
    "s_sizes.tex": r"256-NCA cycles per packet at every size (Zen~4: L2 below 512~B, L3 above; GCM: OpenSSL or mbedTLS). Sources: \texttt{*\_final*.csv}.",
    "s_mc_zen4.tex": r"Multi-core throughput on Zen~4 in Gbit/s (16: both threads of all cores; Eff.: 8-core over 8$\times$ single-core). Source: \texttt{zen4\_mc.csv}.",
    "s_mc_pi5.tex": r"Multi-core throughput on the Pi~5 in Gbit/s (Eff.: 4-core over 4$\times$ single-core). Source: \texttt{pi5\_mc.csv}.",
    "s_mc_fetch.tex": r"OpenSSL on Zen~4: usual call versus a cipher object fetched once per thread, in Gbit/s (Eff.: 8-core over 8$\times$ single-core). Source: \texttt{zen4\_mc\_ossl.csv}.",
    "s_energy.tex": r"Energy on one Pi~5 core at 2.4~GHz (median of three runs; spread: (max $-$ min)/median). Sources: \texttt{pi5\_energy.csv}, \texttt{pi5\_energy\_ref.csv}.",
    "s_fpga.tex": r"All out-of-context builds on the XC7S25-1 ($f_{\max} = 1/(T - \mathrm{WNS})$; power: Vivado estimate, static 0.058~W). Source: \texttt{ooc\_final/reports/summary.csv}.",
    "s_ct.tex": r"All timing-leakage tests (Meas.: millions; medians in cycles; leak: $|t| > 10$; possible: $4.5 < |t| \le 10$). Source: \texttt{<platform>\_<file>.csv}.",
    "s_layout.tex": r"Largest change of the median cycles when the output buffer moves to five offsets in a 4~KiB page. Sources: \texttt{zen4\_layout.csv}, \texttt{pi5\_layout.csv}.",
}


def _set_caption(text, cap):
    i = text.find("\\caption{")
    if i < 0:
        return text
    j, depth = i + len("\\caption{"), 1
    while depth:
        depth += {"{": 1, "}": -1}.get(text[j], 0)
        j += 1
    return text[:i] + "\\caption{" + cap + "}" + text[j:]


def write(name, text):
    if name in CAPTIONS:
        text = _set_caption(text, CAPTIONS[name])
    text = bold_tables(text)
    open(os.path.join(OUT, name), "w").write(text)
    print("wrote", name)


# ---------------------------------------------------------------- S1-S3: all modes and levels
def tier_table(name, label, caption, d, levels, refs, long_b, unit, scale):
    rows = []
    for sname, n in SETS:
        first = True
        for lev in levels:
            for mode in ("NEA", "NIA", "NCA"):
                alg = f"256-{mode}{n}"
                c = med(d[lev] if isinstance(d, dict) else d, lev, alg)
                if c.empty:
                    continue
                ns = med(d[lev] if isinstance(d, dict) else d, lev, alg, "med_ns")
                cpb = (c[long_b] / long_b)
                thr = long_b * 8 / ns[long_b] * scale
                rows.append(f"{sname if first else ''} & {lev} & 256-{mode}{n} & {f0(c.get(0))} & {f0(c.get(40))} & "
                            f"{f0(c.get(1500))} & {fc(cpb)} & {thr:.1f} \\\\")
                first = False
        rows.append("\\midrule")
    for label_r, src, lev, alg in refs:
        c = med(src, lev, alg)
        ns = med(src, lev, alg, "med_ns")
        cpb = c[long_b] / long_b
        rows.append(f"\\multicolumn{{3}}{{@{{}}l}}{{{label_r}}} & {f0(c.get(0))} & {f0(c.get(40))} & {f0(c.get(1500))} & "
                    f"{fc(cpb)} & {long_b * 8 / ns[long_b] * scale:.1f} \\\\")
    lb = "64 KiB" if long_b == 65536 else "16 KiB"
    write(name, f"""\\begin{{table}}[!htbp]
\\centering
\\caption{{{caption}}}
\\label{{{label}}}
\\footnotesize
\\begin{{tabular}}{{@{{}}l l l r r r r r@{{}}}}
\\toprule
Set & Level & Algorithm & \\multicolumn{{3}}{{c}}{{Cycles per packet}} & \\multicolumn{{2}}{{c}}{{{lb}}} \\\\
\\cmidrule(lr){{4-6}}\\cmidrule(l){{7-8}}
 & & & Setup & 40 B & 1500 B & c/B & {unit} \\\\
\\midrule
{chr(10).join(rows)}
\\bottomrule
\\end{{tabular}}
\\end{{table}}
""")


z = load("zen4", "zen4_final_all_levels.csv")
zr = load("zen4", "zen4_base_ref.csv")
tier_table("s_zen4.tex", "tab:s-zen4",
           "Cloud tier (Zen~4, one core at 3.8~GHz): all three modes at every level. Setup is an empty packet. "
           "Sources: \\texttt{results/zen4/zen4\\_final\\_all\\_levels.csv}; ChaCha20-Poly1305 and AEGIS-256 from "
           "\\texttt{zen4\\_base\\_ref.csv}. L3 runs L2 code below 512 bytes.",
           z, ["L1", "L2", "L3"],
           [("OpenSSL AES-256-CTR", z, "base", "OpenSSL-AES-256-CTR"), ("OpenSSL AES-256-GCM", z, "base", "OpenSSL-AES-256-GCM"),
            ("OpenSSL ChaCha20-Poly1305", zr, "base", "OpenSSL-ChaCha20-Poly1305"), ("libaegis AEGIS-256", zr, "base", "libaegis-AEGIS-256")],
           65536, "Gbit/s", 1)
p = load("pi5", "pi5_final_all_levels.csv")
pr = load("pi5", "pi5_base_ref.csv")
tier_table("s_pi5.tex", "tab:s-pi5",
           "Edge tier (Raspberry Pi~5, one Cortex-A76 core at 2.4~GHz): all three modes at L1 and L2. "
           "Sources: \\texttt{results/pi5/pi5\\_final\\_all\\_levels.csv}; ChaCha20-Poly1305 and AEGIS-256 from "
           "\\texttt{pi5\\_base\\_ref.csv}.",
           p, ["L1", "L2"],
           [("OpenSSL AES-256-CTR", p, "base", "OpenSSL-AES-256-CTR"), ("OpenSSL AES-256-GCM", p, "base", "OpenSSL-AES-256-GCM"),
            ("OpenSSL ChaCha20-Poly1305", pr, "base", "OpenSSL-ChaCha20-Poly1305"), ("libaegis AEGIS-256", pr, "base", "libaegis-AEGIS-256")],
           65536, "Gbit/s", 1)
e = load("esp32", "esp32_final.csv")
e4 = load("esp32", "esp32_run4.csv")
e5 = load("esp32", "esp32_run5.csv")
tier_table("s_esp32.tex", "tab:s-esp32",
           "Device tier (ESP32, one core at 240~MHz): all three modes at L1, L2 and HW. "
           "Sources: \\texttt{results/esp32/esp32\\_final.csv} (run~3); ChaCha20-Poly1305 and hardened AEGIS-256 from "
           "\\texttt{esp32\\_run4.csv}; AEGIS-256 with plain tables from \\texttt{esp32\\_run5.csv}.",
           e, ["L1", "L2", "HW"],
           [("mbedTLS AES-256-CTR", e, "base", "mbedTLS-AES-256-CTR"), ("mbedTLS AES-256-GCM", e, "base", "mbedTLS-AES-256-GCM"),
            ("mbedTLS ChaCha20-Poly1305", e4, "base", "mbedTLS-ChaCha20-Poly1305"),
            ("AEGIS-256, plain tables", e5, "base", "libaegis-AEGIS-256-ttab"),
            ("AEGIS-256, hardened", e4, "base", "libaegis-AEGIS-256")],
           16384, "Mbit/s", 1000)


# ---------------------------------------------------------------- S4: 256-NCA at every packet size
def best_zen(alg):
    a, b = med(z, "L2", alg), med(z, "L3", alg)
    return pd.concat([a[a.index < 512], b[b.index >= 512]])


cols = [("Zen~4", "AES-256", best_zen("256-NCA5")), ("", "SNOW 5G", best_zen("256-NCA4")), ("", "ZUC-256", best_zen("256-NCA6")),
        ("", "GCM", med(z, "base", "OpenSSL-AES-256-GCM")),
        ("Pi~5", "AES-256", med(p, "L2", "256-NCA5")), ("", "SNOW 5G", med(p, "L2", "256-NCA4")), ("", "ZUC-256", med(p, "L2", "256-NCA6")),
        ("", "GCM", med(p, "base", "OpenSSL-AES-256-GCM")),
        ("ESP32", "AES HW", med(e, "HW", "256-NCA5")), ("", "AES-256", med(e, "L2", "256-NCA5")), ("", "SNOW 5G", med(e, "L2", "256-NCA4")),
        ("", "ZUC-256", med(e, "L2", "256-NCA6")), ("", "GCM", med(e, "base", "mbedTLS-AES-256-GCM"))]
sizes = sorted(set().union(*[set(c.index) for _, _, c in cols]))
lines = []
for s in sizes:
    lab = f"{s // 1024} KiB" if s >= 4096 and s % 1024 == 0 else f"{s}"
    lines.append(lab + " & " + " & ".join(f0(c.get(s)) for _, _, c in cols) + " \\\\")
write("s_sizes.tex", f"""\\begin{{table*}}[!htbp]
\\centering
\\caption{{256-NCA cycles per packet at every measured size, with a 16-byte tag and no associated data. Zen~4 uses L2 below 512 bytes and L3 from 512 bytes; the Pi~5 uses L2; the ESP32 uses HW and L2. GCM is OpenSSL AES-256-GCM on the servers and mbedTLS AES-256-GCM on the ESP32. Sources: the three \\texttt{{*\\_final*.csv}} files of Table~\\ref{{tab:s-zen4}} to Table~\\ref{{tab:s-esp32}}.}}
\\label{{tab:s-sizes}}
\\scriptsize
\\setlength{{\\tabcolsep}}{{2.6pt}}
\\resizebox{{\\textwidth}}{{!}}{{%
\\begin{{tabular}}{{@{{}}r rrrr rrrr rrrrr@{{}}}}
\\toprule
Bytes & \\multicolumn{{4}}{{c}}{{Cloud: Zen 4}} & \\multicolumn{{4}}{{c}}{{Edge: Pi 5}} & \\multicolumn{{5}}{{c}}{{Device: ESP32}} \\\\
\\cmidrule(lr){{2-5}}\\cmidrule(lr){{6-9}}\\cmidrule(l){{10-14}}
 & {" & ".join(n for _, n, _ in cols)} \\\\
\\midrule
{chr(10).join(lines)}
\\bottomrule
\\end{{tabular}}}}
\\end{{table*}}
""")


# ---------------------------------------------------------------- S5: multi-core
def mc_table(d, threads, plat, label, src):
    g = d.groupby(["alg", "bytes", "threads"]).gbps_total.median()
    rows = []
    names = {"256-NCA5": "256-NCA5 (AES-256)", "256-NCA4": "256-NCA4 (SNOW 5G)", "256-NCA6": "256-NCA6 (ZUC-256)",
             "OpenSSL-AES-256-GCM": "OpenSSL AES-256-GCM", "OpenSSL-AES-256-GCM-fetch": "AES-256-GCM, fetched",
             "OpenSSL-ChaCha20-Poly1305": "OpenSSL ChaCha20-Poly1305",
             "OpenSSL-ChaCha20-Poly1305-fetch": "ChaCha20-Poly1305, fetched", "libaegis-AEGIS-256": "libaegis AEGIS-256"}
    order = [a for a in names if a in d.alg.unique()]
    phys = max(t for t in threads if t <= 8) if plat == "zen4" else max(threads)
    for a in order:
        for b in (40, 1500, 65536):
            v = [g[(a, b, t)] for t in threads]
            eff = g[(a, b, phys)] / (phys * g[(a, b, 1)]) * 100
            rows.append(f"{names[a] if b == 40 else ''} & {b} & " + " & ".join(f"{x:.2f}" if x < 10 else f"{x:.1f}" for x in v)
                        + f" & {eff:.0f} \\\\")
        rows.append("\\addlinespace[2pt]")
    th = " & ".join(str(t) for t in threads)
    return f"""\\begin{{table}}[!htbp]
\\centering
\\caption{{Multi-core throughput on {'Zen~4 (L3 and reference schemes)' if plat == 'zen4' else 'the Pi~5 (L2 and reference schemes)'}: total Gbit/s over all threads, median of three 2-second runs. Threads go to separate physical cores first{', and 16 uses both hardware threads of all eight cores' if plat == 'zen4' else ''}. Eff.\\ is the throughput on {phys} cores divided by {phys} times the single-core value. Source: \\texttt{{{src}}}.}}
\\label{{{label}}}
\\footnotesize
\\begin{{tabular}}{{@{{}}l r {'r ' * len(threads)}r@{{}}}}
\\toprule
Algorithm & Bytes & \\multicolumn{{{len(threads)}}}{{c}}{{Threads}} & Eff. \\\\
\\cmidrule(lr){{3-{2 + len(threads)}}}
 & & {th} & (\\%) \\\\
\\midrule
{chr(10).join(rows[:-1])}
\\bottomrule
\\end{{tabular}}
\\end{{table}}
"""


zm = load("zen4", "zen4_mc.csv")
pm = load("pi5", "pi5_mc.csv")
zo = load("zen4", "zen4_mc_ossl.csv")
write("s_mc_zen4.tex", mc_table(zm, [1, 2, 4, 8, 16], "zen4", "tab:s-mc-zen4", "results/zen4/zen4\\_mc.csv"))
write("s_mc_pi5.tex", mc_table(pm, [1, 2, 4], "pi5", "tab:s-mc-pi5", "results/pi5/pi5\\_mc.csv"))
write("s_mc_fetch.tex", mc_table(zo, [1, 2, 4, 8, 16], "zen4", "tab:s-mc-fetch", "results/zen4/zen4\\_mc\\_ossl.csv")
      .replace("Multi-core throughput on Zen~4 (L3 and reference schemes)",
               "OpenSSL on Zen~4 with the usual call (\\texttt{EVP\\_aes\\_256\\_gcm()} per packet) and with a cipher object fetched once per thread (\\texttt{EVP\\_CIPHER\\_fetch}); the key is set up for every packet in both cases"))


# ---------------------------------------------------------------- S6: energy
en = pd.concat([load("pi5", "pi5_energy.csv"), load("pi5", "pi5_energy_ref.csv")])
en["dp_core"] = en.active_core_w - en.idle_core_w
g = en.groupby(["alg", "bytes"]).agg(core=("nj_per_byte_core", "median"), total=("nj_per_byte_total", "median"),
                                     dp=("dp_core", "median"), idle=("idle_core_w", "median"),
                                     spread=("nj_per_byte_core", lambda s: (s.max() - s.min()) / s.median() * 100),
                                     temp=("temp_c", "max"))
names = {"256-NCA4": "SNOW 5G (NCA4), L2", "256-NCA5": "AES-256 (NCA5), L2", "256-NCA6": "ZUC-256 (NCA6), L2",
         "OpenSSL-AES-256-GCM": "AES-256-GCM, OpenSSL", "OpenSSL-ChaCha20-Poly1305": "ChaCha20-Poly1305, OpenSSL",
         "libaegis-AEGIS-256": "AEGIS-256, libaegis"}
rows = []
for a, nm in names.items():
    for b in (40, 1500, 65536):
        r = g.loc[(a, b)]
        rows.append(f"{nm if b == 40 else ''} & {b} & {r.core:.2f} & {r.total:.2f} & {r.core * b / 1000:.3f} & {r.idle:.2f} & {r.dp:.2f} & {r.spread:.1f} & {r.temp:.1f} \\\\")
    rows.append("\\addlinespace[2pt]")
write("s_energy.tex", f"""\\begin{{table}}[!htbp]
\\centering
\\caption{{Energy on one Pi~5 core at 2.4~GHz, from the power-management chip. nJ/B: rise in power over idle times run time, per byte, for the core rail and for the sum of all rails. $\\mu$J/pkt: core-rail energy per packet. Idle and $\\Delta P$: core rail. Spread: (max $-$ min)/median of the three repetitions. Medians of three 10-second runs. Sources: \\texttt{{results/pi5/pi5\\_energy.csv}} and \\texttt{{pi5\\_energy\\_ref.csv}}.}}
\\label{{tab:s-energy}}
\\footnotesize
\\begin{{tabular}}{{@{{}}l r r r r r r r r@{{}}}}
\\toprule
Algorithm & Bytes & nJ/B & nJ/B & $\\mu$J/pkt & Idle & $\\Delta P$ & Spread & Max \\\\
 & & core & all rails & core & (W) & (W) & (\\%) & $^\\circ$C \\\\
\\midrule
{chr(10).join(rows[:-1])}
\\bottomrule
\\end{{tabular}}
\\end{{table}}
""")

# ---------------------------------------------------------------- S7: FPGA
oc = pd.read_csv(os.path.join(RES, "fpga", "ooc_final", "reports", "summary.csv")).drop_duplicates("tag", keep="last").set_index("tag")
desc = {"ksg5_1": "AES-256 generator, 1 round/clk", "ksg5_2": "AES-256 generator, 2 rounds/clk",
        "ksg4_1": "SNOW 5G generator, 1 AES round/clk", "ksg4_2": "SNOW 5G generator, 2 AES rounds/clk",
        "ksg6": "ZUC-256 generator", "nca5_A": "256-NCA5 design A", "nca5_T": "256-NCA5 design T",
        "nca4_A": "256-NCA4 design A", "nca4_T": "256-NCA4 design T", "nca6_A": "256-NCA6 design A", "nca6_T": "256-NCA6 design T"}
rows = []
for t, nm in desc.items():
    r = oc.loc[t]
    rows.append(f"{nm} & {r.lut} & {r.ff} & {r.slice} & {r.bram:g} & {r.period_ns:.2f} & {r.wns_ns:.3f} & {r.fmax_mhz:.1f} & "
                f"{r.dyn_w_fmax:.3f} & {r.total_w_fmax:.3f} & {r.dyn_w_100:.3f} \\\\")
    if t in ("ksg6",):
        rows.append("\\midrule")
cyc = []
for t in ("nca5_A", "nca5_T", "nca4_A", "nca4_T", "nca6_A", "nca6_T"):
    c = {}
    for line in open(os.path.join(RES, "fpga", "hw_final", t, "hwtest.txt")):
        m = re.match(r"\s*(NCA|NEA|NIA)\s+(\d+) bytes:\s+(\d+) cycles", line)
        if m:
            c[(m[1], int(m[2]))] = int(m[3])
    f = oc.loc[t].fmax_mhz
    cyc.append(f"{desc[t]} & " + " & ".join(str(c[("NCA", b)]) for b in (0, 40, 576, 1500, 16384))
               + f" & {c[('NEA', 16384)]} & {c[('NIA', 1500)]} & {c[('NCA', 40)] / f * 1000:.0f} & {c[('NCA', 1500)] / f * 1000:.0f} \\\\")
write("s_fpga.tex", f"""\\begin{{table*}}[!htbp]
\\centering
\\caption{{Hardware tier, all out-of-context builds on the XC7S25-1 (Vivado 2025.2, second pass). $T$: clock period of the second pass; WNS: worst negative slack; $f_{{\\max}} = 1/(T - \\mathrm{{WNS}})$. Power: Vivado estimates with default switching activity; the static part is 0.058~W in every build. Source: \\texttt{{results/fpga/ooc\\_final/reports/summary.csv}} (last entry per build).}}
\\label{{tab:s-fpga}}
\\footnotesize
\\resizebox{{\\textwidth}}{{!}}{{%
\\begin{{tabular}}{{@{{}}l r r r r r r r r r r@{{}}}}
\\toprule
Design & LUT & FF & Slice & BRAM & $T$ (ns) & WNS (ns) & $f_{{\\max}}$ (MHz) & Dyn.\\ W & Total W & Dyn.\\ W \\\\
 & & & & & & & & at $f_{{\\max}}$ & at $f_{{\\max}}$ & at 100 MHz \\\\
\\midrule
{chr(10).join(rows)}
\\bottomrule
\\end{{tabular}}}}

\\vspace{{6pt}}
\\resizebox{{\\textwidth}}{{!}}{{%
\\begin{{tabular}}{{@{{}}l r r r r r r r r r@{{}}}}
\\toprule
Engine & \\multicolumn{{5}}{{c}}{{256-NCA cycles on the board}} & NEA & NIA & \\multicolumn{{2}}{{c}}{{Latency at $f_{{\\max}}$ (ns)}} \\\\
\\cmidrule(lr){{2-6}}\\cmidrule(l){{9-10}}
 & 0 B & 40 B & 576 B & 1500 B & 16 KiB & 16 KiB & 1500 B & 40 B & 1500 B \\\\
\\midrule
{chr(10).join(cyc)}
\\bottomrule
\\end{{tabular}}}}
\\\\[2pt]{{\\footnotesize Board cycle counts from \\texttt{{results/fpga/hw\\_final/<design>/hwtest.txt}} (127 of 127 packets match, 50~MHz).}}
\\end{{table*}}
""")

# ---------------------------------------------------------------- S8: dudect
ct_files = [("zen4", "zen4_ct.csv"), ("zen4", "zen4_ct_rerun.csv"), ("zen4", "zen4_ct_l3long.csv"), ("zen4", "zen4_ct_zuc.csv"),
            ("pi5", "pi5_ct.csv"), ("pi5", "pi5_ct_zuc_run1.csv"), ("pi5", "pi5_ct_zuc.csv")]
rows = []
for plat, fn in ct_files:
    d = load(plat, fn)
    for _, r in d.iterrows():
        alg = r.alg.replace("leaky-early-exit", "early exit").replace("ct-branchfree", "branch-free")
        v = {"LEAK": "\\textbf{leak}", "possible": "\\emph{possible}"}.get(r.verdict, r.verdict)
        rows.append(f"{'Zen 4' if r.platform == 'zen4' else 'Pi 5'} & {r.level} & {alg} & {r.bytes} & {r.test} & "
                    f"{r.measurements / 1e6:g} & {r.t_max:.2f} & {r.median_class0:.1f} & {r.median_class1:.1f} & {v} & "
                    f"\\texttt{{{fn[len(plat)+1:-4].replace('_', chr(92) + '_')}}} \\\\")
    rows.append("\\midrule")
write("s_ct.tex", f"""{{\\scriptsize\\setlength{{\\tabcolsep}}{{3.5pt}}
\\begin{{longtable}}{{@{{}}l l l r l r r r r l l@{{}}}}
\\caption{{Every timing-leakage test. Meas.: measurements in millions. $|t|_{{\\max}}$: largest $|t|$ over all measurements and 100 cropped subsets. Medians in cycles (Zen~4: time-stamp counter; Pi~5: user-mode cycle counter). Verdict: leak if $|t| > 10$, possible if $4.5 < |t| \\le 10$. File: \\texttt{{results/<platform>/<platform>\\_<file>.csv}}.}}
\\label{{tab:s-ct}}\\\\
\\toprule
Platform & Level & Algorithm & Bytes & Test & Meas. & $|t|_{{\\max}}$ & Median 0 & Median 1 & Verdict & File \\\\
\\midrule
\\endfirsthead
\\toprule
Platform & Level & Algorithm & Bytes & Test & Meas. & $|t|_{{\\max}}$ & Median 0 & Median 1 & Verdict & File \\\\
\\midrule
\\endhead
{chr(10).join(rows[:-1])}
\\bottomrule
\\end{{longtable}}}}
""")

# ---------------------------------------------------------------- S9: buffer placement
rows = []
for plat, fn, nm in [("zen4", "zen4_layout.csv", "Zen 4"), ("pi5", "pi5_layout.csv", "Pi 5")]:
    d = load(plat, fn)
    for lev in sorted(d.level.unique()):
        for alg in sorted(d[d.level == lev].alg.unique()):
            q = d[(d.level == lev) & (d.alg == alg)]
            dev = q.groupby("bytes").med_cycles.agg(lambda s: (s.max() - s.min()) / s.min() * 100)
            rows.append(f"{nm} & {lev} & {alg} & {dev.max():.1f} & {dev.idxmax()} \\\\")
write("s_layout.tex", f"""\\begin{{table}}[!htbp]
\\centering
\\caption{{Buffer-placement test: the output buffer is placed at five offsets within a 4~KiB page (0, 64, 1024, 2048 and 3072 bytes). The table gives the largest relative change of the median cycles over the five offsets, and the packet size where it occurs. Sources: \\texttt{{results/zen4/zen4\\_layout.csv}} and \\texttt{{results/pi5/pi5\\_layout.csv}}.}}
\\label{{tab:s-layout}}
\\footnotesize
\\begin{{tabular}}{{@{{}}l l l r r@{{}}}}
\\toprule
Platform & Level & Algorithm & Max change (\\%) & At bytes \\\\
\\midrule
{chr(10).join(rows)}
\\bottomrule
\\end{{tabular}}
\\end{{table}}
""")
