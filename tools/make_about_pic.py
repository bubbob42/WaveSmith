#!/usr/bin/env python3
"""make_about_pic.py -- WaveSmith's About picture, from Mason's art.

Reads art/wavesmith_app.png (64x64, Martin "Mason" Merz's picture for the
About window) and writes WaveSmith/about_pic.h, which is COMMITTED: an
ordinary build does not regenerate it, so the artwork cannot change under a
release by accident.

After Herald's tool of the same name, with two differences the artwork and
WaveSmith force:

  The picture is see-through: its panels are glass (alpha about 170 of
  255), only the waveforms are solid, and a faint rim (alpha 6 to 21)
  frames them. So the palette keeps an ALPHA per entry, and
  WaveSmith/about.c blends each entry with the window's own background
  colour at run time before it turns it into a pen -- the picture sits on
  whatever grey the Workbench has. Only alpha 0 is left out
  (ABOUT_PIC_NONE: not drawn); without the rim the panels lost their edge.

  WaveSmith's SAS/C near data is all but full (a 2.5 KB table breaks the
  link). So the palette and the runs are STRING LITERALS, which SAS/C puts
  in the code hunk under STRMERGE (measured: a 4000-byte literal, no data
  hunk at all) and GCC in .rodata -- not arrays, which would be near data
  -- each returned by a function of its own (about_rgba(), about_runs()).

The colours go to ABOUT_PIC_COLOURS entries by median cut in (r, g, b, a),
no dithering. RUN-LENGTH ENCODED as (count, index) byte pairs, left to
right and top to bottom, count 1..255, no run across a row: each run is a
span WaveSmith/about.c hands straight to RectFill.

Usage:
    tools/make_about_pic.py     (writes WaveSmith/about_pic.h and previews
                                 on two backgrounds in build/)
"""

import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from png_read import read_png  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_PNG = os.path.join(ROOT, "art", "wavesmith_app.png")
OUT_H = os.path.join(ROOT, "WaveSmith", "about_pic.h")
PREVIEW = os.path.join(ROOT, "build", "about-preview-%s.ppm")

COLOURS = 16                # palette entries: close to the art (24 barely closer), pens a 64-colour Workbench has
CLEAR = 1                   # alpha below this: not drawn
NONE = 255                  # "do not draw this pixel"
GROUNDS = {"grey": (170, 170, 170), "dark": (90, 100, 120)}   # the previews' backgrounds


def median_cut(counts, colours):
    """{rgba: count} -> (palette, {rgba: index}), as tools/png2ilbm.py cuts
    (r, g, b) but over four channels: boxes split at the weighted median of
    their widest channel, the busiest and widest first"""
    boxes = [list(counts.items())]

    def spread(box):
        ranges = [max(c[0][ch] for c in box) - min(c[0][ch] for c in box) for ch in range(4)]
        ch = max(range(4), key=lambda i: ranges[i])
        return ranges[ch], ch

    while len(boxes) < colours:
        best, best_score = None, -1
        for i, box in enumerate(boxes):
            if len(box) < 2:
                continue
            width, _ = spread(box)
            score = width * sum(n for _, n in box)
            if score > best_score:
                best, best_score = i, score
        if best is None:
            break
        box = boxes.pop(best)
        _, ch = spread(box)
        box.sort(key=lambda c: c[0][ch])
        half, run, cut = sum(n for _, n in box) / 2.0, 0, 1
        for i, (_, n) in enumerate(box):
            run += n
            if run >= half:
                cut = min(max(i + 1, 1), len(box) - 1)
                break
        boxes += [box[:cut], box[cut:]]
    palette, index = [], {}
    for box in boxes:
        total = sum(n for _, n in box)
        palette.append(tuple((sum(c[ch] * n for c, n in box) + total // 2) // total for ch in range(4)))
        for c, _ in box:
            index[c] = len(palette) - 1
    return palette, index


def encode(data, width):
    """(count, value) pairs, count 1..255, no run across a row."""
    out = []
    for start in range(0, len(data), width):
        row = data[start:start + width]
        i = 0
        while i < len(row):
            v = row[i]
            n = 1
            while i + n < len(row) and row[i + n] == v and n < 255:
                n += 1
            out.append(n)
            out.append(v)
            i += n
    return out


def blend(rgba, ground):
    """an entry over a background, as WaveSmith/about.c blends it"""
    a = rgba[3]
    return tuple((rgba[ch] * a + ground[ch] * (255 - a) + 127) // 255 for ch in range(3))


def literal(name, data):
    """a function returning the bytes as a string literal, 32 a line --
    adjacent literals on lines of their own: a macro's continued lines are
    ONE line to SAS/C, and its line buffer overflows ("Error 6")"""
    lines = ["static const UBYTE *%s(void)" % name, "{", "\treturn (const UBYTE *)"]
    for i in range(0, len(data), 32):
        lines.append("\t\t\"" + "".join("\\x%02x" % b for b in data[i:i + 32]) + "\"")
    lines[-1] += ";"
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    w, h, px = read_png(SRC_PNG)
    counts = Counter(p for row in px for p in row if p[3] >= CLEAR)
    palette, index = median_cut(counts, COLOURS)
    pixels = [NONE if p[3] < CLEAR else index[p] for row in px for p in row]
    rle = encode(pixels, w)

    with open(OUT_H, "w") as f:
        f.write("#ifndef WAVESMITH_ABOUT_PIC_H\n#define WAVESMITH_ABOUT_PIC_H\n\n")
        f.write("/*\n * GENERATED by tools/make_about_pic.py from art/wavesmith_app.png,\n")
        f.write(" * artwork by Martin \"Mason\" Merz. Do not hand-edit: re-run the tool and\n")
        f.write(" * commit the result.\n *\n")
        f.write(" * about_rgba(): r, g, b, a per entry (a: 255 solid; blended with the\n")
        f.write(" * window's background at run time). about_runs(): (count, entry) pairs,\n")
        f.write(" * row by row; ABOUT_PIC_NONE is not drawn. String literals, so that they\n")
        f.write(" * sit in the code hunk, not in SAS/C's near data. For about.c alone.\n */\n\n")
        f.write("#define ABOUT_PIC_W\t\t%d\n" % w)
        f.write("#define ABOUT_PIC_H\t\t%d\n" % h)
        f.write("#define ABOUT_PIC_NONE\t\t%d\n" % NONE)
        f.write("#define ABOUT_PIC_COLOURS\t%d\n" % len(palette))
        f.write("#define ABOUT_PIC_PAIRS\t\t%d\n\n" % (len(rle) // 2))
        f.write(literal("about_rgba", [v for e in palette for v in e]))
        f.write("\n")
        f.write(literal("about_runs", rle))
        f.write("\n#endif\n")

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    for name, ground in GROUNDS.items():
        with open(PREVIEW % name, "wb") as f:
            f.write(("P6\n%d %d\n255\n" % (w, h)).encode("ascii"))
            for i in pixels:
                f.write(bytes(ground if i == NONE else blend(palette[i], ground)))

    print("about_pic.h: %dx%d, %d colours; %d pixels -> %d bytes of runs" % (w, h, len(palette), w * h, len(rle)))


if __name__ == "__main__":
    main()
