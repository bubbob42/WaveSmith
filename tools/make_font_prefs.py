#!/usr/bin/env python3
"""make_font_prefs.py -- a Font.prefs naming one font for all three uses.

Used to build the PAL Hires variant of the autotest system disk (Topaz 8,
the font a 640x256 Workbench is designed for). The test disk's own
Font.prefs names Helvetica 11/13 and Courier 13, so without this no test
ever saw Herald in Topaz 8.

Layout: FORM PREF, PRHD, then one FONT chunk per type -- FP_WBFONT (0, the
icons), FP_SYSFONT (1, the default text font), FP_SCREENFONT (2) -- each a
struct FontPrefs { LONG fp_Reserved[3]; UWORD fp_Reserved2; UWORD fp_Type;
UBYTE fp_FrontPen, fp_BackPen, fp_DrawMode, pad; struct TextAttr (ta_Name,
ta_YSize, ta_Style, ta_Flags); BYTE fp_Name[128]; } -- 156 bytes, as the
disk's own file has them.

Usage: tools/make_font_prefs.py OUT NAME SIZE [FLAGS]
       (FLAGS in hex, default 41 = FPF_ROMFONT | FPF_DESIGNED, for topaz)
"""

import struct
import sys


def chunk(cid, data):
    pad = b"\0" if len(data) & 1 else b""
    return cid + struct.pack(">I", len(data)) + data + pad


def font(kind, name, size, flags):
    # the pens and draw mode the disk's own Font.prefs uses per type
    draw = 0 if kind == 0 else 1
    body = struct.pack(">IIIHHBBBBIHBB", 0, 0, 0, 0, kind, 1, 0, draw, 0,
                       0, size, 0, flags)
    body += name.encode("latin-1").ljust(128, b"\0")
    assert len(body) == 156
    return chunk(b"FONT", body)


def main(argv):
    out, name, size = argv[1], argv[2], int(argv[3])
    flags = int(argv[4], 16) if len(argv) > 4 else 0x41
    prhd = struct.pack(">BBI", 0, 0, 0)
    body = b"PREF" + chunk(b"PRHD", prhd)
    for kind in (0, 1, 2):
        body += font(kind, name, size, flags)
    with open(out, "wb") as fh:
        fh.write(chunk(b"FORM", body))


if __name__ == "__main__":
    main(sys.argv)
