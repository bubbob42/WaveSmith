#!/usr/bin/env python3
"""check_guide.py -- the manual's own structure, checked on the host.

AmigaGuide forgives almost everything, and that is the trouble: a link to a
node that is not there opens the first page instead, an attribute left on
runs to the end of the node, and a framed example wider than the window is
clipped at the right edge. None of that is an error anybody sees until they
read the page on the machine. This fails the build instead, for every guide
given:

  * every @{" ... " link NODE} names a node of the same guide (matched
    without regard to case, as AmigaGuide does; a link into another file,
    "file/node", is left alone);
  * @node and @endnode pair up;
  * in every node, @{b} and @{ub}, @{i} and @{ui}, @{u} and @{uu} pair up;
  * no line between @{code} and @{body} is wider than 72 columns, which is
    what a 640-wide window shows in Topaz 8 without scrolling.

test/wb/TestGuide promises these checks; this is where they live.

USAGE
    tools/check_guide.py guide/*/Warden.guide
"""

import re
import sys

LINK = re.compile(r'@\{"[^"]*"\s+link\s+("?)([^"\s}]+)\1', re.IGNORECASE)
NODE = re.compile(r'^@node\s+("?)([^"\s]+)\1', re.IGNORECASE)
PAIRS = (("@{b}", "@{ub}"), ("@{i}", "@{ui}"), ("@{u}", "@{uu}"))
WIDTH = 72


def check(path):
    errors = []
    with open(path, "rb") as fh:
        raw = fh.read()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("latin-1")	# a guide kept in the Amiga's own
    lines = text.split("\n")

    nodes = set()
    for line in lines:
        m = NODE.match(line)
        if m:
            nodes.add(m.group(2).lower())

    depth = 0
    node = None
    counts = {}
    incode = False
    for no, line in enumerate(lines, 1):
        low = line.lower()
        if low.startswith("@node"):
            if depth != 0:
                errors.append("%d: @node inside node %s" % (no, node))
            depth += 1
            node = NODE.match(line).group(2) if NODE.match(line) else "?"
            counts = {}
            incode = False
            continue
        if low.startswith("@endnode"):
            if depth != 1:
                errors.append("%d: @endnode without @node" % no)
            for on, off in PAIRS:
                if counts.get(on, 0) != counts.get(off, 0):
                    errors.append("node %s: %s %d times, %s %d times"
                                  % (node, on, counts.get(on, 0),
                                     off, counts.get(off, 0)))
            depth = 0
            continue
        for on, off in PAIRS:
            counts[on] = counts.get(on, 0) + line.count(on)
            counts[off] = counts.get(off, 0) + line.count(off)
        for m in LINK.finditer(line):
            target = m.group(2)
            if "/" in target:
                continue
            if target.lower() not in nodes:
                errors.append("%d: link to a node that is not there: %s"
                              % (no, target))
        if "@{code}" in line:
            incode = True
            continue
        if "@{body}" in line:
            incode = False
            continue
        if incode and len(line) > WIDTH:
            errors.append("%d: %d columns in a framed example (max %d)"
                          % (no, len(line), WIDTH))
    if depth != 0:
        errors.append("end of file inside node %s" % node)
    return errors


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    bad = 0
    for path in argv:
        errors = check(path)
        for e in errors:
            print("check_guide: %s:%s" % (path, e))
        bad += len(errors)
    if bad:
        return 1
    print("check_guide: ok (%d guide%s)" % (len(argv), "" if len(argv) == 1 else "s"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
