#!/usr/bin/env python3
"""Run the official implementers' test data of 3GPP TS 35.241 (Snow 5G set).

Vectors are read from your copy of the document, never copied here:

    python3 test_official.py path/to/35241-k00.docx ./kat_cli

Checks clause 5.2 (POLYVAL dot) and clause 5.3 (Snow 5G): the full internal
state after the 16 initialisation clocks, and the keystream words.
"""
import re
import subprocess
import sys

spec, cli = sys.argv[1], sys.argv[2]
text = subprocess.run(["pandoc", "-t", "plain", "--wrap=none", spec],
                      capture_output=True, text=True, check=True).stdout


def hexwords(s, width):
    return "".join(re.findall(r"\b[0-9a-fA-F]{%d}\b" % width, s)).lower()


def field(body, name, width=2):
    m = re.search(r"\b%s = \{([^}]*)\}" % re.escape(name), body)
    return hexwords(m.group(1), width) if m else None


def run(*a):
    return subprocess.run([cli, *a], capture_output=True, text=True, check=True).stdout.split()


fails = total = 0
for m in re.finditer(r"=== POLYVAL TEST #(\d+) ===\s*A = \{([^}]*)\}\s*B = \{([^}]*)\}\s*A\*B = \{([^}]*)\}", text):
    n, a, b, want = m.group(1), hexwords(m.group(2), 2), hexwords(m.group(3), 2), hexwords(m.group(4), 2)
    ok = run("pdot", a, b)[0] == want
    total += 1; fails += not ok
    print(f"POLYVAL test #{n}: {'PASS' if ok else 'FAIL'}")

for m in re.finditer(r"=== SNOW-5G TEST #(\d+) ===(.*?)(?==== |Annex A)", text, re.S):
    n, body = m.group(1), m.group(2)
    key, iv = field(body, "KEY"), field(body, "IV")
    after = body.split("State after 16 initialisation clocks", 1)[1].split("Keystream mode", 1)[0]
    want_state = [field(after, "lfsrA", 4), field(after, "lfsrB", 4),
                  field(after, "R1"), field(after, "R2"), field(after, "R3")]
    ks_part = body.split("Keystream mode", 1)[1]
    want_ks = [hexwords(x, 2) for x in re.findall(r"ks_\d+ = \{([^}]*)\}", ks_part)]
    got_state = run("state", key, iv)
    got_ks = run("ks", key, iv, str(len(want_ks)))
    ok_state, ok_ks = got_state == want_state, got_ks == want_ks
    total += 2; fails += (not ok_state) + (not ok_ks)
    print(f"Snow 5G test #{n}: state after initialisation {'PASS' if ok_state else 'FAIL'}; "
          f"{len(want_ks)} keystream words {'PASS' if ok_ks else 'FAIL'}")
    if not ok_state:
        for name, g, w in zip(["lfsrA", "lfsrB", "R1", "R2", "R3"], got_state, want_state):
            if g != w:
                print(f"   {name}: got {g}\n          want {w}")
    if not ok_ks:
        for g, w in zip(got_ks, want_ks):
            print(f"   ks got {g} want {w}")

print(f"\n{total - fails}/{total} official TS 35.241 checks pass")
if total == 0:
    print("No vectors found - check that this is the Release 20 document (35241-k00).")
sys.exit(1 if fails or total == 0 else 0)
