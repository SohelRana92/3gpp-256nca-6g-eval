"""Make table header rows and first columns bold in LaTeX source.

bold_tables(text) -> text: in every tabular/longtable, wraps each cell of the
rows between \\toprule and the first \\midrule in \\textbf{} and prefixes the
first column type with >{\\bfseries}.  Idempotent.
"""
import re


def _split_cells(row):
    cells, depth, cur = [], 0, ""
    for ch in row:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        if ch == "&" and depth == 0:
            cells.append(cur); cur = ""
        else:
            cur += ch
    cells.append(cur)
    return cells


def _bold_cell(c):
    s = c.strip()
    if not s or s.startswith("\\textbf") or s == "--":
        return c
    m = re.match(r"(\\multicolumn\{\d+\}\{[^}]*\}\{)(.*)\}$", s, re.S)
    if m:
        inner = m.group(2)
        return " " + m.group(1) + (inner if inner.startswith("\\textbf") or not inner.strip() else "\\textbf{" + inner + "}") + "} "
    return " \\textbf{" + s + "} "


def _bold_header(block):
    out = []
    for line in block.split("\n"):
        st = line.strip()
        if not st or st.startswith("\\cmidrule") or st.startswith("\\toprule") or st.startswith("%"):
            out.append(line); continue
        tail = ""
        mm = re.search(r"\\\\\s*(\\cmidrule.*)?$", line)
        body = line
        if mm:
            tail = line[mm.start():]; body = line[:mm.start()]
        out.append("&".join(_bold_cell(c) for c in _split_cells(body)).strip() + " " + tail.strip())
    return "\n".join(out)


def _first_col(spec):
    s = spec
    pre = ""
    m = re.match(r"(@\{\})", s)
    if m:
        pre, s = m.group(1), s[m.end():]
    if s.startswith(">{\\bfseries}"):
        return spec
    if s.startswith(">{"):
        return pre + ">{\\bfseries" + s[2:]  # merge into existing >{...}
    return pre + ">{\\bfseries}" + s


def bold_tables(text):
    pat = re.compile(r"(\\begin\{(tabular\*?|longtable)\}(\{[^}]*\})?\{)((?:[^{}]|\{(?:[^{}]|\{[^{}]*\})*\})*)(\})", re.S)
    def fix_spec(m):
        return m.group(1) + _first_col(m.group(4)) + m.group(5)
    text = pat.sub(fix_spec, text)
    def fix_head(m):
        return m.group(1) + _bold_header(m.group(2)) + m.group(3)
    text = re.sub(r"(\\toprule\s*\n)(.*?)(\n\s*\\midrule)", fix_head, text, flags=re.S)
    return text
