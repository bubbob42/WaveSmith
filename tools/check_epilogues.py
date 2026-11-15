#!/usr/bin/env python3
"""check_epilogues.py -- catch a silent m68k-amigaos-gcc miscompilation.

WHAT GOES WRONG (found in GSheets, 2026-09-15)

    movel d2,sp@-        <- saves d2
    movel d3,sp@-        <- saves d3
    ...
    movel arg,sp@-       <- pushes an ARGUMENT for the call
    movel arg,sp@-       <- pushes an ARGUMENT for the call
    jsr _something
    movel sp@+,d2        <- pops an ARGUMENT, not the saved d2
    movel sp@+,d3        <- pops an ARGUMENT, not the saved d3
    addql #8,sp          <- discards the SAVED d2/d3
    rts

The stack pointer comes out balanced, so nothing looks wrong -- but d2/d3
come back holding the last two arguments instead of the caller's values.

WHAT IT TAKES

The miscompilation needs a call whose ARGUMENTS GO ON THE STACK: without
those pushes there is nothing for the epilogue to pop by mistake. Herald
never makes one -- every function stays within 2 pointers + 2 integers
(tools/check_signatures.py) and the OS is called in registers -- so this
check looks for the precondition itself, in the built objects.

A push of a callee-saved register (d2-d7, a2-a6) is a SAVE and expected.
Anything else pushed on the stack (a scratch register, a memory operand,
pea, an immediate) is an argument, and a function that has one AND ends
by popping registers before a stack cleanup is reported. Shape alone is
not enough to report: a function with a local struct saves a register
too, and its epilogue pops that register and then frees the local --
the same instructions, in the same order, and perfectly correct.

This version reads OBJECTS (objdump -d), so the Makefile can hand it only
the objects that changed since the last clean run.

USAGE
    tools/check_epilogues.py build/gcc/foo.o [...]

Exits 1 and names the function when the shape AND the precondition meet.
"""

import os
import re
import subprocess
import sys

OBJDUMP = os.environ.get("GCC_OBJDUMP", "m68k-amigaos-objdump")

LABEL = re.compile(r"^[0-9a-f]+ <(_[A-Za-z_0-9.$]+)>:")
INSN = re.compile(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{4} )+\s*(.*)$")
POP = re.compile(r"^movel sp@\+,[ad]\d$")
CLEAN = re.compile(r"^(addql #\d+,sp|addl #\d+,sp|addaw #\d+,sp|lea sp@\(\d+\),sp)$")
SAVE = re.compile(r"^movel ([ad][2-6]),sp@-$")
SAVE_MANY = re.compile(r"^moveml [ad][2-6][^,]*,sp@-$")
PUSH = re.compile(r"sp@-")


def scan(obj, bad):
    out = subprocess.run([OBJDUMP, "-d", obj], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, universal_newlines=True)
    if out.returncode != 0:
        print("check_epilogues: objdump failed on %s" % obj)
        return False
    fn = None
    body = []
    funcs = []
    for line in out.stdout.splitlines():
        m = LABEL.match(line)
        if m:
            if fn is not None:
                funcs.append((fn, body))
            fn, body = m.group(1), []
            continue
        m = INSN.match(line)
        if m and fn is not None:
            body.append(m.group(1).strip())
    if fn is not None:
        funcs.append((fn, body))

    for fn, insns in funcs:
        args = [t for t in insns
                if PUSH.search(t) and not SAVE.match(t) and not SAVE_MANY.match(t)]
        if not args:
            continue                    # nothing on the stack to pop by mistake
        for i, text in enumerate(insns):
            if not CLEAN.match(text):
                continue
            if i + 1 >= len(insns) or insns[i + 1] != "rts":
                continue
            j = i - 1
            pops = 0
            while j >= 0 and POP.match(insns[j]):
                pops += 1
                j -= 1
            if pops:
                bad.append((obj, fn, "%d pop%s before '%s', and the call '%s'"
                                     " puts arguments on the stack"
                                     % (pops, "" if pops == 1 else "s", text,
                                        args[0])))
                break
    return True


def main(argv):
    objs = [a for a in argv[1:] if a.endswith(".o")]
    bad = []
    ok = True
    for obj in objs:
        ok = scan(obj, bad) and ok
    if bad:
        print("check_epilogues: MISCOMPILED EPILOGUE in %d function(s):" % len(bad))
        for obj, fn, why in bad:
            print("  %s  %s  (%s)" % (obj, fn, why))
        print("  d2/d3 come back holding arguments. Fix: pass a struct by"
              " pointer, stay within 2 pointers + 2 integers.")
        return 1
    if not ok:
        return 2
    print("check_epilogues: ok (%d object%s)" % (len(objs), "" if len(objs) == 1 else "s"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
