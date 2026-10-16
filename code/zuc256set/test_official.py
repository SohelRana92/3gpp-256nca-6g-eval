#!/usr/bin/env python3
"""Run the official implementers' test data of 3GPP TS 35.247 (ZUC-256 set).

Vectors are read from your copy of the document, never copied here:

    python3 test_official.py path/to/35247-k00.docx ./kat_cli

Checks clause 5.2 (POLYVAL dot) and clause 5.3 (ZUC-256): the LFSR and the
F registers after the 48 initialisation rounds plus the final work-mode clock
("State after 49 initialisation clocks"), and the keystream words.
"""
import re
import subprocess
import sys

spec, cli = sys.argv[1], sys.argv[2]
text = subprocess.run(["pandoc", "-t", "plain", "--wrap=none", spec],
                      capture_output=True, text=True, check=True).stdout


def hexwords(s, width):
    return "".join(re.findall(r"\b[0-9a-fA-F]{%d}\b" % width, s)).lower()


def run(*a):
    return subprocess.run([cli, *a], capture_output=True, text=True, check=True).stdout.split()


fails = total = 0
for m in re.finditer(r"=== POLYVAL TEST #(\d+) ===\s*A = \{([^}]*)\}\s*B = \{([^}]*)\}\s*A\*B = \{([^}]*)\}", text):
    n, a, b, want = m.group(1), hexwords(m.group(2), 2), hexwords(m.group(3), 2), hexwords(m.group(4), 2)
    ok = run("pdot", a, b)[0] == want
    total += 1; fails += not ok
    print(f"POLYVAL test #{n}: {'PASS' if ok else 'FAIL'}")

for m in re.finditer(r"=== ZUC-256 TEST #(\d+) ===(.*?)(?==== |Annex A)", text, re.S):
    n, body = m.group(1), m.group(2)
    key = hexwords(re.search(r"KEY = \{([^}]*)\}", body).group(1), 2)
    iv = hexwords(re.search(r"IV = \{([^}]*)\}", body).group(1), 2)
    after = body.split("State after 49 initialisation clocks", 1)[1].split("Keystream mode", 1)[0]
    want_state = [hexwords(re.search(r"LFSR = \{([^}]*)\}", after).group(1), 8),
                  re.search(r"FSM_R1 = 0x([0-9a-fA-F]{8})", after).group(1).lower(),
                  re.search(r"FSM_R2 = 0x([0-9a-fA-F]{8})", after).group(1).lower()]
    ks_hex = hexwords(re.search(r"[Kk]s = \{([^}]*)\}", body.split("Keystream mode", 1)[1]).group(1), 8)
    want_ks = [ks_hex[i:i + 8] for i in range(0, len(ks_hex), 8)]
    got_state = run("state", key, iv)
    got_ks = run("ks", key, iv, str(len(want_ks)))
    ok_state, ok_ks = got_state == want_state, got_ks == want_ks
    total += 2; fails += (not ok_state) + (not ok_ks)
    print(f"ZUC-256 test #{n}: state after initialisation {'PASS' if ok_state else 'FAIL'}; "
          f"{len(want_ks)} keystream words {'PASS' if ok_ks else 'FAIL'}")
    if not ok_state:
        for name, g, w in zip(["LFSR", "R1", "R2"], got_state, want_state):
            if g != w:
                print(f"   {name}: got {g}\n        want {w}")
    if not ok_ks:
        print(f"   first ks got {got_ks[:2]} want {want_ks[:2]}")

print(f"\n{total - fails}/{total} official TS 35.247 checks pass")
if total == 0:
    print("No vectors found - check that this is the Release 20 document (35247-k00).")
sys.exit(1 if fails or total == 0 else 0)
