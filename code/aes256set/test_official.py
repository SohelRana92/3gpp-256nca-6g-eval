#!/usr/bin/env python3
"""Run the official implementers' test data of 3GPP TS 35.244 through the R4 code.

The vectors are read directly from your copy of the specification, so they are
never copied into this repository:

    python3 test_official.py path/to/35244-k00.docx ./kat_cli

Covers clause 5.2 (POLYVAL dot function) and clause 5.3 (AES-256 in counter
mode as a stream cipher). Requires pandoc.
"""
import re
import subprocess
import sys

spec, cli = sys.argv[1], sys.argv[2]
text = subprocess.run(["pandoc", "-t", "plain", "--wrap=none", spec],
                      capture_output=True, text=True, check=True).stdout


def arr(s):
    """'{ 01 02 ... }' possibly spread over lines -> hex string."""
    return "".join(re.findall(r"\b[0-9a-fA-F]{2}\b", s)).lower()


def run(*a):
    return subprocess.run([cli, *a], capture_output=True, text=True, check=True).stdout.split()


fails = total = 0

# Clause 5.2: POLYVAL dot function tests.
for m in re.finditer(r"=== POLYVAL TEST #(\d+) ===\s*A = \{([^}]*)\}\s*B = \{([^}]*)\}\s*A\*B = \{([^}]*)\}", text):
    n, a, b, want = m.group(1), arr(m.group(2)), arr(m.group(3)), arr(m.group(4))
    got = run("pdot", a, b)[0]
    ok = got == want
    total += 1; fails += not ok
    print(f"POLYVAL test #{n}: {'PASS' if ok else 'FAIL'}")
    if not ok:
        print(f"   got  {got}\n   want {want}")

# Clause 5.3: AES-256 in counter mode.
for m in re.finditer(r"=== AES-256 TEST #(\d+) ===(.*?)(?====|Annex)", text, re.S):
    n, body = m.group(1), m.group(2)
    key = arr(re.search(r"KEY = \{([^}]*)\}", body).group(1))
    iv = arr(re.search(r"IV = \{([^}]*)\}", body).group(1))
    want = [arr(x) for x in re.findall(r"ks\[ctr=\d+\] = \{([^}]*)\}", body)]
    got = run("aesks", key, iv, str(len(want)))
    ok = got == want
    total += 1; fails += not ok
    print(f"AES-256 CTR test #{n} ({len(want)} keystream blocks): {'PASS' if ok else 'FAIL'}")
    if not ok:
        for g, w in zip(got, want):
            print(f"   got  {g}\n   want {w}")

print(f"\n{total - fails}/{total} official TS 35.244 tests pass")
if total == 0:
    print("No vectors found - check that this is the Release 20 document (35244-k00).")
sys.exit(1 if fails or total == 0 else 0)
