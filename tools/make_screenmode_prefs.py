#!/usr/bin/env python3
"""make_screenmode_prefs.py -- a ScreenMode.prefs for a given display mode.

Used to build Workbench-mode variants of the autotest system disk (the RTG
rows of the display matrix). Layout: FORM PREF, PRHD, SCRM with
struct ScreenModePrefs { ULONG sm_Reserved[4]; ULONG sm_DisplayID;
UWORD sm_Width, sm_Height, sm_Depth, sm_Control; }.

Usage: tools/make_screenmode_prefs.py OUT MODEID WIDTH HEIGHT DEPTH
       (MODEID in hex, e.g. 50031100 = uaegfx 640x480 16 bit)
"""

import struct
import sys


def chunk(cid, data):
    pad = b"\0" if len(data) & 1 else b""
    return cid + struct.pack(">I", len(data)) + data + pad


def main(argv):
    out, mode, w, h, d = argv[1], int(argv[2], 16), int(argv[3]), int(argv[4]), int(argv[5])
    scrm = struct.pack(">IIIIIHHHH", 0, 0, 0, 0, mode, w, h, d, 1)
    prhd = struct.pack(">BBI", 0, 0, 0)
    with open(out, "wb") as fh:
        fh.write(chunk(b"FORM", b"PREF" + chunk(b"PRHD", prhd) + chunk(b"SCRM", scrm)))


if __name__ == "__main__":
    main(sys.argv)
