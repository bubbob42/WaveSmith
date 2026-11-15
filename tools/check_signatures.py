#!/usr/bin/env python3
"""check_signatures.py -- the register budget of every function.

Under PARAMETERS=REGISTERS / -msasregparm the first two integer arguments
travel in d0/d1 and the first two pointers in a0/a1; anything beyond goes
on the STACK. PrismaRec keeps every internal call fully in registers
(taken from Herald): at most 2 pointer and 2 integer parameters,
else a pointer to an argument struct. Stack-passed arguments are also what
triggers the GCC epilogue miscompile (tools/check_epilogues.py).

Exempt: ASM entry points and DISP_QUAL dispatchers (their registers are
pinned by hand), and main.

USAGE
    tools/check_signatures.py src/*.h src/*.c
"""

import re
import sys

POINTER_TYPES = {"STRPTR", "CONST_STRPTR", "APTR", "Msg", "WrapWidthFunc",
                 "CxObj", "CxMsg"}
EXEMPT = {"main"}

SIG = re.compile(
    r"(?:^|[;}\n])\s*((?:static\s+|extern\s+|const\s+|struct\s+)*"
    r"[A-Za-z_][A-Za-z0-9_\s\*]*?)\b([A-Za-z_][A-Za-z0-9_]*)\s*"
    r"\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?=[;{])", re.S)
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "do", "else"}


def strip(text):
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text = re.sub(r"^\s*#.*$", "", text, flags=re.M)
    return text


def classify(param):
    p = param.strip()
    if not p or p == "void" or p == "...":
        return None
    if "(" in p or "*" in p:
        return "ptr"
    words = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", p)
    if any(w in POINTER_TYPES for w in words):
        return "ptr"
    return "int"


def main(argv):
    bad = []
    seen = set()
    for src in argv[1:]:
        with open(src, encoding="latin-1") as fh:
            text = strip(fh.read())
        for m in SIG.finditer(text):
            prefix, name, params = m.group(1), m.group(2), m.group(3)
            # a dispatcher's registers are pinned by hand (DISP_QUAL REG())
            if name in KEYWORDS or name in EXEMPT or "ASM" in prefix or "DISP_QUAL" in prefix \
                    or "REG(" in params:
                continue
            if not prefix.strip() or prefix.strip() in KEYWORDS:
                continue
            kinds = [classify(p) for p in params.split(",")]
            ptrs = kinds.count("ptr")
            ints = kinds.count("int")
            if ptrs > 2 or ints > 2:
                key = (name, ptrs, ints)
                if key in seen:
                    continue
                seen.add(key)
                line = text.count("\n", 0, m.start(2)) + 1
                bad.append((src, line, name, ptrs, ints))
    if bad:
        print("check_signatures: over the register budget (2 pointers + 2 integers):")
        for src, line, name, ptrs, ints in bad:
            print("  %s:%d: %s -- %d pointer%s, %d integer%s"
                  % (src, line, name, ptrs, "" if ptrs == 1 else "s", ints, "" if ints == 1 else "s"))
        print("  Pass an argument struct by pointer instead.")
        return 1
    print("check_signatures: ok (%d file%s)" % (len(argv) - 1, "" if len(argv) == 2 else "s"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
