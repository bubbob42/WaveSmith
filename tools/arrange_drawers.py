#!/usr/bin/env python3
"""arrange_drawers.py -- lay out every drawer of the release package.

  tools/arrange_drawers.py <stage>      (build/release: WaveSmith.info and
                                         the WaveSmith drawer it opens)

Two sets of drawers, one set of rules:

  the package's face   <stage>/WaveSmith.info opens the unpacked drawer:
                       Install and the readme side by side, LICENSE and
                       COPYING.LIB under them
  what gets installed  Files/WaveSmith, which the Installer copies whole
                       (the program and Help side by side; Help: a drawer
                       per language side by side; each of those: the
                       guide), the GlowIcon set Files/Icons/Glow/WaveSmith
                       copied over its classic icons when GlowIcons are
                       chosen, and the drawer's icon (Files/WaveSmith.info,
                       or the GlowIcon Files/Icons/Glow/WaveSmithDrawer.info)

The drawer's icon in the user's own drawer (SYS:Tools) and the package's
icon wherever the archive is unpacked keep NO position: those windows are
the user's, and Workbench finds them a free place. Everything inside our
drawers is placed, and every drawer window is boxed and set to "Show only
icons / View by icon" -- a drawer left on "inherit" may open by name and
drop every position.

The rules (a few knobs; the rest derived from the icons themselves):

  - the icons stand in a grid of rows and columns; an icon is placed by the
    size Workbench DRAWS it at (a GlowIcon's own, which is taller than its
    classic part here), centred on its column with its BOTTOM on its row's
    bottom: icons of different heights in a row put their names on one
    line. Each place holds every icon that can stand there (the classic
    icon and the GlowIcon of the set the Installer copies over it): the
    row is as tall as the tallest.
  - the spacing is for BIG icon fonts, not for Topaz 8: a column is as wide
    as its longest name at LETTER points a letter (Topaz 8 is 8, the
    A4000T's XHelvetica 11 about 7; 10 leaves room for a 15-point font),
    names sit GAP apart at least, column centres never closer than PITCH,
    and a name is NAME lines tall.
  - a window is its rows (ROWGAP between a row's names and the next row's
    icons), PAD all round, the title bar and bottom border (CHROME:
    MEASURED 17 + 18 on the A4000T with XHelvetica 13 as the screen font),
    and AIR below: Workbench wants room under the names before it drops
    the vertical scroller, and a bigger screen font's taller title bar
    takes its few lines from there. No scroller is ever needed to see an
    icon. Workbench puts all spare room at the bottom, so CHROME is the
    measured figure, not a worst case: a generous one looks lopsided.
  - windows open below the screen's title bar (TOP: an 18-line bar on the
    A4000T, room for a bigger one) and cascade in steps wide enough to
    leave each parent's title and a strip of it in sight.
  - windows cascade, no two at one origin (a second window exactly on top
    of the first looks like a click that missed), and every one fits a PAL
    640x256 screen.

After the layout it checks the result and fails on a problem: every placed
icon and its name inside its window, no two names overlapping, the windows
inside 640x256 and at distinct origins, the icons for the user's drawers
unplaced.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import set_icon_pos as sip                         # noqa: E402

PAD = 24          # margin round a window's content
GAP = 32          # least air between two names in a row
PITCH = 104       # least distance between two columns' centres
ROWGAP = 16       # from a row's names to the next row's icons
NAME = 16         # the name under an icon: a big icon font's line
LETTER = 10       # points a letter: room for a big icon font
CHROME = 36       # title bar + bottom border (MEASURED 35 on the A4000T)
BORDERS = 22      # left (4) + right with its scroller (18)
AIR = 12          # Workbench's own margin under the names
MIN_CLIENT = 248  # 270 wide outside
SCREEN = (640, 256)
TOP = 24          # the highest a window opens: below the screen's bar

APP = "WaveSmith"


def load(path):
    with open(path, "rb") as fh:
        return bytearray(fh.read())


def save(path, data):
    with open(path, "wb") as fh:
        fh.write(data)


def label(path):
    """The name Workbench shows: the file's name without .info."""
    return os.path.basename(path)[:-len(".info")]


def name_w(path):
    return len(label(path)) * LETTER


def place(path, cx, bottom):
    data = load(path)
    w, h = sip.shown_size(data)
    struct.pack_into(">ll", data, 58, cx - w // 2, bottom - h)
    save(path, data)


def free(path):
    data = load(path)
    struct.pack_into(">ll", data, 58, sip.NO_POS, sip.NO_POS)
    save(path, data)


def box(path, left, top, width, height):
    data = load(path)
    if struct.unpack(">L", data[66:70])[0] == 0:
        sys.exit("%s: no DrawerData (not a drawer icon)" % path)
    struct.pack_into(">hhhh", data, 78, left, top, width, height)
    struct.pack_into(">ll", data, 78 + 48, 0, 0)   # view origin: top-left
    sip.set_show_mode(data, sip.DDFLAGS_SHOWICONS, sip.DDVM_BYICON)
    save(path, data)


def width_of(alts):
    """A place's width: its widest icon or name."""
    return max(max(name_w(p), sip.shown_size(load(p))[0]) for p in alts)


def layout(rows):
    """Rows of places (each place a list of the icons that can stand there)
    -> the column centres, each row's bottom, the window's client width and
    outer height. Columns are shared by all rows: a column is as wide as
    its widest place in any row."""
    ncol = max(len(r) for r in rows)
    widths = [0] * ncol
    for r in rows:
        for i, alts in enumerate(r):
            widths[i] = max(widths[i], width_of(alts))
    pitch = PITCH
    for a, b in zip(widths, widths[1:]):
        need = (a + b) // 2 + GAP
        pitch = max(pitch, (need + 9) // 10 * 10)
    # what is centred is the grid's whole extent, names included: with
    # names of unequal width, centring the columns' centres would push the
    # wider name out of the window
    span = (ncol - 1) * pitch + widths[0] // 2 + widths[-1] // 2
    client = max(MIN_CLIENT, span + 2 * PAD)
    first = (client - span) // 2 + widths[0] // 2
    centres = [first + i * pitch for i in range(ncol)]
    bottoms = []
    top = PAD
    for r in rows:
        height = max(sip.shown_size(load(p))[1] for alts in r for p in alts)
        bottoms.append(top + height)
        top = top + height + NAME + ROWGAP
    outer_h = bottoms[-1] + NAME + PAD + AIR + CHROME
    return centres, bottoms, client + BORDERS, outer_h


def arrange(window_icons, origin, rows):
    """Box the window (every icon that opens it) and place its rows."""
    centres, bottoms, w, h = layout(rows)
    for p in window_icons:
        box(p, origin[0], origin[1], w, h)
    for bottom, r in zip(bottoms, rows):
        for cx, alts in zip(centres, r):
            for p in alts:
                place(p, cx, bottom)
    return (origin[0], origin[1], w, h)


def languages(help_dir):
    """The language drawers, English first (the manual's own language)."""
    langs = sorted(d for d in os.listdir(help_dir)
                   if os.path.isdir(os.path.join(help_dir, d)))
    if "english" in langs:
        langs.remove("english")
        langs.insert(0, "english")
    return langs


def plan(stage):
    """[(window icons, origin, rows)] for the whole package. Every icon in
    Files/WaveSmith has its twin in the GlowIcon set
    (Files/Icons/Glow/WaveSmith, the same path below it): both stand in
    the same place, and a drawer's twin opens the same window."""
    root = os.path.join(stage, APP)
    files = os.path.join(root, "Files")
    prog = os.path.join(files, APP)
    glow = os.path.join(files, "Icons", "Glow")
    gprog = os.path.join(glow, APP)
    helpdir = os.path.join(prog, "Help")
    langs = languages(helpdir)

    def both(rel):
        return [os.path.join(prog, rel), os.path.join(gprog, rel)]

    p = [
        # the package's face: what to DO, then what to read; the licence
        # under them
        ([os.path.join(stage, APP + ".info")], (100, TOP),
         [[[os.path.join(root, "Install.info")],
           [os.path.join(root, APP + ".readme.info")]],
          [[os.path.join(root, "LICENSE.info")],
           [os.path.join(root, "COPYING.LIB.info")]]]),
        # the installed drawer: the program, then its manual
        ([os.path.join(files, APP + ".info"), os.path.join(glow, APP + "Drawer.info")], (60, TOP),
         [[both(APP + ".info"), both("Help.info")]]),
        (both("Help.info"), (150, TOP + 24),
         [[both(os.path.join("Help", l + ".info")) for l in langs]]),
    ]
    for i, l in enumerate(langs):
        p.append((both(os.path.join("Help", l + ".info")), (240 + 24 * i, TOP + 48 + 12 * i),
                  [[both(os.path.join("Help", l, APP + ".guide.info"))]]))
    return p


def check(stage, windows):
    bad = []
    origins = {}
    for icons, origin, rows in plan(stage):
        for p in icons:
            # a revision-0 icon's show mode is never read, and a GlowIcon
            # drawer then loses its colour image (set_icon_pos.py)
            if struct.unpack(">I", load(p)[44:48])[0] < 1:
                bad.append("%s: revision 0 -- its show mode would be ignored" % p)
        l, t, w, h = windows[icons[0]]
        if t < TOP:
            bad.append("%s: window at %d, under the screen's title bar" % (label(icons[0]), t))
        if l + w > SCREEN[0] or t + h > SCREEN[1]:
            bad.append("%s: window %d,%d %dx%d leaves %dx%d" % (label(icons[0]), l, t, w, h, *SCREEN))
        if (l, t) in origins:
            bad.append("%s opens on top of %s" % (label(icons[0]), origins[(l, t)]))
        origins[(l, t)] = label(icons[0])
        for columns in rows:
            bad.extend(check_row(columns, w, h))
        if len(rows) > 1:
            bad.extend(check_rows(rows))
    for icons, _, _ in plan(stage):
        for p in icons:
            if p.endswith(os.path.join("Files", APP + ".info")) or p.endswith(APP + "Drawer.info") \
               or p == os.path.join(stage, APP + ".info"):
                x = struct.unpack(">l", load(p)[58:62])[0]
                if x != sip.NO_POS:
                    bad.append("%s: placed, but it lands in the user's own drawer" % p)
    return bad


def check_rows(rows):
    """No row's names reach into the next row's icons."""
    bad = []
    for upper, lower in zip(rows, rows[1:]):
        names = max(struct.unpack(">l", load(p)[62:66])[0] + sip.shown_size(load(p))[1] + NAME
                    for alts in upper for p in alts)
        icons = min(struct.unpack(">l", load(p)[62:66])[0] for alts in lower for p in alts)
        if names + ROWGAP // 2 > icons:
            bad.append("%s: its names reach the next row" % label(upper[0][0]))
    return bad


def check_row(columns, w, h):
    """One row: every icon placed, inside the window, no names overlapping."""
    bad = []
    spans = []
    for alts in columns:
        left = right = 0
        for p in alts:
            data = load(p)
            x, y = struct.unpack(">ll", data[58:66])
            iw, ih = sip.shown_size(data)
            if x == sip.NO_POS:
                bad.append("%s: no position" % p)
                continue
            nw = name_w(p)
            left, right = min(x, x + iw // 2 - nw // 2), max(x + iw, x + iw // 2 + nw // 2)
            if left < 0 or right > w - BORDERS or y < 0 or y + ih + NAME > h - CHROME:
                bad.append("%s: %d..%d x %d..%d outside the window" % (p, left, right, y, y + ih + NAME))
        spans.append((left, right, label(alts[0])))
    for (al, ar, an), (bl, br, bn) in zip(spans, spans[1:]):
        if ar + GAP // 2 > bl:
            bad.append("%s and %s: their names overlap" % (an, bn))
    return bad


def main():
    args = sys.argv[1:]
    if len(args) != 1:
        sys.exit(__doc__)
    stage = args[0]
    windows = {}
    for icons, origin, rows in plan(stage):
        for p in icons + [q for r in rows for alts in r for q in alts]:
            if not os.path.isfile(p):
                sys.exit("arrange_drawers: missing %s" % p)
        box_ = arrange(icons, origin, rows)
        for p in icons:
            windows[p] = box_
        print("%-28s window %d,%d %dx%d, %d row(s) of %d" % (label(icons[0]), *box_, len(rows),
                                                          max(len(r) for r in rows)))
    # the icons that land in the user's own drawers: Workbench places them
    free(os.path.join(stage, APP + ".info"))
    free(os.path.join(stage, APP, "Files", APP + ".info"))
    free(os.path.join(stage, APP, "Files", "Icons", "Glow", APP + "Drawer.info"))
    bad = check(stage, windows)
    for b in bad:
        print("  " + b)
    if bad:
        sys.exit("arrange_drawers: %d problem(s)" % len(bad))


if __name__ == "__main__":
    main()
