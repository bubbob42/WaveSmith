#!/usr/bin/env python3
"""Generate WaveSmith's icon.

    WaveSmith.info    WBTOOL: WaveSmith's window in small -- a waveform
                       in two lanes (stereo), its trimmed ends shaded (the
                       lead-in with a click, the run-out with the groove's),
                       In and Out as blue lines with their flags, In's right
                       of its line, Out's left, as the gadget draws them. No
                       tool types: WaveSmith reads none.

The family's look (tools/make_rec_icons.py): a white panel, a black frame,
a one-pixel shadow right and below, the 4-colour Workbench palette (0 grey,
1 black, 2 white, 3 blue). But no border of pen 0 around it: the image is
cropped to its artwork, so the icon's box on Workbench -- what a click and
a drag take, what the selected state fills -- is the art and nothing more
(the user's: no empty space around an icon that does not show).

Beside it the package's other icons (tools/package_icons.py):

    Drawer.info       WBDRAWER: the drawer WaveSmith is installed in, and the
                      package's own
    Folder.info       WBDRAWER: Help and its languages inside it
    Install.info      WBPROJECT for the Installer: APPNAME WaveSmith. Its
                      art is AmigaOS 3.2's standard installer icon -- the
                      classic image of icons/install/Install-classic.info
                      here; the GNUmakefile appends the GlowIcon of
                      icons/install/Install-glow.info -- with our default
                      tool, tooltypes and the stock icon's stack
    Readme.info       WBPROJECT for the readme (MultiView)
    Guide.info        WBPROJECT for a manual (MultiView)

Built into build/edit-icons (never committed).

Usage:
    python3 tools/make_edit_icons.py <outdir> [--preview]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amiga_icon import Canvas, write_info, read_icon_image, WBTOOL, WBDRAWER, WBPROJECT, GREY, BLACK, WHITE, BLUE
from package_icons import drawer_icon, folder_icon, text_icon, guide_icon

# AmigaOS 3.2's standard installer icon, its classic (planar) part
INSTALL_CLASSIC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "icons", "install",
                               "Install-classic.info")

# the panel's inside: x 0..44, y 0..32; the shadow at x 45 and y 33
PW, PH = 45, 33

# the lanes' centres and the waveform's columns
LANES = (11, 23)
X0, X1 = 3, 41
IN_X, OUT_X = 9, 35

# a column's half height in a lane (0: the centre line only): a lead-in
# of groove noise with a click, three chords fading out, a run-out with
# the groove's click
AMPS = {
    3: 0, 4: 1, 5: 4, 6: 0, 7: 1, 8: 0,
    10: 2, 11: 3, 12: 4, 13: 4, 14: 3, 15: 2, 16: 3, 17: 4, 18: 4, 19: 3,
    20: 4, 21: 3, 22: 2, 23: 3, 24: 4, 25: 3, 26: 3, 27: 2, 28: 3, 29: 2,
    30: 2, 31: 1, 32: 1, 33: 1, 34: 0,
    36: 0, 37: 1, 38: 0, 39: 3, 40: 0, 41: 1,
}


def crop(c):
    """c cut to its artwork: the rows and columns all of pen 0 around it gone"""
    xs = [x for y in range(c.h) for x in range(c.w) if c.px[y][x] != GREY]
    ys = [y for y in range(c.h) for x in range(c.w) if c.px[y][x] != GREY]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    out = Canvas(x1 - x0 + 1, y1 - y0 + 1)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            out.px[y - y0][x - x0] = c.px[y][x]
    return out


def edit_icon():
    c = Canvas(PW + 1, PH + 1)
    # the panel and its shadow
    c.fill_rect(0, 0, PW - 1, PH - 1, WHITE)
    c.rect(0, 0, PW - 1, PH - 1, BLACK)
    c.hline(1, PW, PH, BLACK)
    c.vline(PW, 1, PH, BLACK)
    # the trimmed ends, shaded, lanes high
    c.fill_rect(X0, 6, IN_X - 1, 28, GREY)
    c.fill_rect(OUT_X + 1, 6, X1, 28, GREY)
    # the waveform, both lanes
    for x in range(X0, X1 + 1):
        a = AMPS.get(x)
        if a is None:
            continue
        for cy in LANES:
            c.vline(x, cy - a, cy + a, BLACK)
    # the lanes' parting line
    c.hline(X0, X1, 17, GREY)
    # In and Out: blue lines, the flags at the top -- In's right of its
    # line, Out's left of it
    c.vline(IN_X, 3, 29, BLUE)
    c.fill_rect(IN_X + 1, 3, IN_X + 5, 5, BLUE)
    c.vline(OUT_X, 3, 29, BLUE)
    c.fill_rect(OUT_X - 5, 3, OUT_X - 1, 5, BLUE)
    return crop(c)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/edit-icons"
    os.makedirs(out, exist_ok=True)
    icon = edit_icon()
    write_info(os.path.join(out, "WaveSmith.info"), icon, WBTOOL, stack=16384)
    write_info(os.path.join(out, "Drawer.info"), drawer_icon(), WBDRAWER, stack=4096)
    write_info(os.path.join(out, "Folder.info"), folder_icon(), WBDRAWER, stack=4096)
    write_info(os.path.join(out, "Install.info"), read_icon_image(INSTALL_CLASSIC), WBPROJECT,
               default_tool="Installer", tooltypes=["APPNAME=WaveSmith", "MINUSER=NOVICE"], stack=10000)
    write_info(os.path.join(out, "Readme.info"), text_icon(), WBPROJECT,
               default_tool="SYS:Utilities/MultiView", stack=4096)
    write_info(os.path.join(out, "Guide.info"), guide_icon(), WBPROJECT,
               default_tool="SYS:Utilities/MultiView", stack=4096)
    if "--preview" in sys.argv:
        icon.to_png(os.path.join(out, "WaveSmith_preview.png"), scale=8)


if __name__ == "__main__":
    main()
