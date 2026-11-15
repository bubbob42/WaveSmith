#!/usr/bin/env python3
"""check_helpnodes.py -- every page the program asks for is in every guide.

The editor's context help opens the manual at a node: HELP_NODE("Memory")
for the Help key over the memory limits, and so on (src/prefswin.c,
src/prefsrules.c). AmigaGuide does not complain about a node it does not
have -- it quietly shows the first page instead -- so a renamed or missing
node would turn context help back into "open the manual" without anybody
noticing. This fails the build instead.

Node names are matched without regard to case, as AmigaGuide matches them.
Every guide is checked, because the names are the same in every language
and the German one is the one most likely to fall behind.

USAGE
    tools/check_helpnodes.py --guides guide/*/Warden.guide -- src/*.c
"""

import re
import sys


def main(argv):
    if "--" not in argv or "--guides" not in argv:
        print(__doc__)
        return 2
    sep = argv.index("--")
    guides = argv[argv.index("--guides") + 1:sep]
    sources = argv[sep + 1:]

    wanted = {}
    for path in sources:
        with open(path, encoding="latin-1") as f:
            for no, line in enumerate(f, 1):
                for name in re.findall(r'HELP_NODE\(\s*"([^"]+)"\s*\)', line):
                    wanted.setdefault(name, "%s:%d" % (path, no))

    bad = 0
    for guide in guides:
        with open(guide, encoding="latin-1") as f:
            nodes = {m.lower() for m in
                     re.findall(r'^@node\s+"?([^"\s]+)', f.read(), re.M | re.I)}
        for name, where in sorted(wanted.items()):
            if name.lower() not in nodes:
                print("check_helpnodes: %s asks for node '%s', which %s "
                      "has not got" % (where, name, guide))
                bad += 1
    if bad:
        return 1
    print("check_helpnodes: ok (%d nodes in %d guides)"
          % (len(wanted), len(guides)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
