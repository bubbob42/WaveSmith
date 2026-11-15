#!/usr/bin/env python3
"""ppm2png.py -- convert DEV:Test/Grab's PPM captures to PNG (optionally
scaled up), with the PNG writer of tools/amiga_icon.py (no PIL needed).

Usage: tools/ppm2png.py IN.ppm OUT.png [SCALE]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amiga_icon import _write_png


def main(argv):
    data = open(argv[1], "rb").read()
    parts = data.split(b"\n", 3)
    w, h = [int(v) for v in parts[1].split()]
    pix = parts[3]
    scale = int(argv[3]) if len(argv) > 3 else 1
    rows = []
    for y in range(h):
        src = pix[y * w * 3:(y + 1) * w * 3]
        line = bytearray()
        for x in range(w):
            line += src[x * 3:x * 3 + 3] * scale
        for _ in range(scale):
            rows.append(bytes(line))
    _write_png(argv[2], rows, w * scale, h * scale)


if __name__ == "__main__":
    main(sys.argv)
