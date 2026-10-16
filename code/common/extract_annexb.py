#!/usr/bin/env python3
"""Extract the informative reference code annex from a copy of a 3GPP 256-bit
algorithm specification (TS 35.243 Annex B, TS 35.240 Annex B, TS 35.246 Annex A).

The 3GPP code is not redistributed in this repository. Run this script on your
own copy of the specification to build the cross-check:

    python3 extract_annexb.py path/to/35243-k00.docx annex_ref.cpp

Requires pandoc. The code is wrapped in `namespace annex` so that its symbol
names cannot clash with the R4 implementation.
"""
import subprocess
import sys

src, dst = sys.argv[1], sys.argv[2]
text = subprocess.run(["pandoc", "-t", "plain", "--wrap=none", src],
                      capture_output=True, text=True, check=True).stdout
lines = text.splitlines()
start = next(i for i, l in enumerate(lines) if l.strip() == "// Common components") - 1
# Include code lines directly above the marker (e.g. a #define in TS 35.246).
while start > 0 and (lines[start - 1].strip() == "" or lines[start - 1].lstrip().startswith(("#", "//"))):
    start -= 1
end = next(i for i in range(start, len(lines)) if lines[i].strip().startswith("Annex "))
code = [l for l in lines[start:end] if l.strip()]
includes = [l for l in code if l.startswith("#include")]
body = [l for l in code if not l.startswith("#include")]
with open(dst, "w") as f:
    f.write(f"// Extracted from {src} (informative reference code annex). Not redistributed.\n")
    f.write("\n".join(includes) + "\n#include <string.h>\nnamespace annex {\n")
    f.write("\n".join(body) + "\n} // namespace annex\n")
print(f"wrote {dst}: {len(body)} lines")
