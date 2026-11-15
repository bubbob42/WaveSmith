#!/usr/bin/env python3
"""merge_glowicon.py -- embed a classic planar icon into a GlowIcon file.

The OS 3.5 "GlowIcon" .info format is the classic icon (struct DiskObject
with its planar Image data, tooltypes, default tool, ...) followed by an
appended IFF `FORM <size> ICON` holding the palette-mapped glow imagery
(FACE/IMAG chunks). icon.library V44+ (OS 3.5/3.9 and OS 3.2's V47) renders
the IFF imagery; every older icon.library reads only the classic part and
ignores the trailing bytes.

This tool takes OUR classic icon (the whole file: planar images, tooltypes
and all) and the artist's GlowIcon, and writes classic-bytes + glow-IFF-tail.
The result shows our 4-colour planar icon on old systems and the glow
imagery on V44+, while tooltypes/default tool/stack come from OUR icon (that
metadata lives only in the classic part).

The result is cut to its artwork (tools/icon_crop.py): the classic image's
border of pen 0 and the glow images' border of their transparent colour
go -- an artist's GlowIcon often has one. --no-crop keeps both as they are.

Usage:
    tools/merge_glowicon.py <classic.info> <glow.info> <out.info> [--no-crop]
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import icon_crop  # noqa: E402

WB_DISKMAGIC = 0xE310


def read_file(path):
    with open(path, "rb") as fh:
        return fh.read()


def check_classic(data, path):
    if len(data) < 78 or struct.unpack(">H", data[0:2])[0] != WB_DISKMAGIC:
        sys.exit("%s: not an Amiga .info file" % path)


def icon_type(data):
    return data[48]


def glow_tail(data, path):
    """The validated trailing IFF FORM ICON of a GlowIcon file.

    Scans every FORM occurrence and accepts the one whose declared size
    reaches exactly the end of the file (an even-padding byte allowed) --
    a plain substring search alone is unsafe, tooltype text can contain
    the word FORM.
    """
    i = 0
    while True:
        i = data.find(b"FORM", i)
        if i < 0:
            sys.exit("%s: no trailing FORM ICON (not a GlowIcon?)" % path)
        if i + 12 <= len(data) and data[i + 8:i + 12] == b"ICON":
            size = struct.unpack(">I", data[i + 4:i + 8])[0]
            end = i + 8 + size
            if end == len(data) or end + (end & 1) == len(data):
                return data[i:]
        i += 1


def main():
    args = [a for a in sys.argv[1:] if a != "--no-crop"]
    if len(args) != 3:
        sys.exit(__doc__)
    classic_path, glow_path, out_path = args

    classic = read_file(classic_path)
    glow = read_file(glow_path)
    check_classic(classic, classic_path)
    check_classic(glow, glow_path)

    if icon_type(classic) != icon_type(glow):
        sys.exit("%s (type %d) and %s (type %d): icon types differ"
                 % (classic_path, icon_type(classic),
                    glow_path, icon_type(glow)))

    tail = glow_tail(glow, glow_path)

    # Append the glow IFF at the EXACT end of the classic DiskObject -- do NOT
    # word-align it. icon.library (V44+) locates the appended colour icon at
    # the DiskObject's computed end, not at the next even offset; an alignment
    # pad byte leaves a one-byte gap it cannot see, so it silently shows only
    # the 4-colour planar image. This bites only classic parts of odd length
    # (e.g. the WBTOOL program icon at 1061 bytes) -- even-length ones never
    # needed a pad, which masked the bug. (Field report 2026-07-21: one
    # icon came out classic despite the GlowIcons having been installed.)
    merged = classic + tail
    if "--no-crop" not in sys.argv:
        merged, _ = icon_crop.crop(merged)

    with open(out_path, "wb") as fh:
        fh.write(merged)
    print("merged %s + %s -> %s (%d bytes)"
          % (classic_path, glow_path, out_path, len(merged)))


if __name__ == "__main__":
    main()
