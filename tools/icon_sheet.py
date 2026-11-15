#!/usr/bin/env python3
"""icon_sheet.py -- contact-sheet PNG previews of AmigaOS Workbench icons.

Renders every `.info` in a directory (or the icons named on the command line)
into one RGB PNG: per icon the classic planar imagery -- normal and selected --
and, when the file carries an appended OS3.5/V44 GlowIcon `FORM ICON` tail, the
colour imagery too, with each icon's type, stack, default tool and Tool Types.
For eyeballing a whole icon set at once (our own art, or a downloaded set) the
way `amiga_icon.Canvas.to_png()` shows a single icon being authored.

    python3 tools/icon_sheet.py icons/ --out /tmp/icons.png
    python3 tools/icon_sheet.py icons/ --out /tmp/icons.png --scale 3 \\
        --detail-dir /tmp/icons --detail-scale 8

Transparent GlowIcon pixels show as a dark checkerboard (drawn in destination
pixels, and darker than every Workbench pen, so it can never be mistaken for
the icon's own dithering); everything else sits on Workbench grey.

Pure Python 3 standard library, like the rest of tools/. Format references:
`workbench/workbench.h`, the `icon.library` autodocs, and the published
GlowIcon `FORM ICON` layout -- all public.
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amiga_icon import PREVIEW_PALETTE, _write_png      # noqa: E402

WB_DISKMAGIC = 0xE310

# Icons store pen numbers, not RGB. Pens 0..3 are the shared preview palette
# (grey / black / white / blue); 4..7 extend it with the AmigaOS 3.1 Workbench
# defaults so a deeper (8-colour) icon still previews sensibly.
WB_PENS = [PREVIEW_PALETTE[0], PREVIEW_PALETTE[1],
           PREVIEW_PALETTE[2], PREVIEW_PALETTE[3],
           (123, 123, 123), (175, 175, 175),
           (170, 144, 124), (255, 169, 151)]

BG = (32, 36, 44)            # sheet background
INK = (232, 232, 232)        # heading text
DIM = (150, 158, 172)        # caption text
RULE = (64, 70, 84)          # separators and cell frames
GREY = (170, 170, 170)       # Workbench grey inside a cell
CHECK_A = (86, 92, 104)      # transparency checkerboard
CHECK_B = (66, 72, 84)

TYPE_NAME = {1: "Disk", 2: "Drawer", 3: "Tool", 4: "Project",
             5: "Garbage", 6: "Device", 7: "Kick", 8: "AppIcon"}

# A 5x9 ASCII matrix for the captions -- no font library on the build host.
# The artwork is the 9-pin matrix from the DotMatrixFont project (rows 0..6 the
# main block, 7..8 the descender); trailing blank rows are dropped here and
# padded back when the glyph is drawn.
FONT5X9 = {
    ' ':  '',
    '!':  '..X../..X../..X../..X../..X../...../..X..',
    '"':  '.X.X./.X.X.',
    '#':  '.X.X./.X.X./XXXXX/.X.X./XXXXX/.X.X./.X.X.',
    '$':  '..X../.XXXX/X.X../.XXX./..X.X/XXXX./..X..',
    '%':  'XX.../XX..X/...X./..X../.X.../X..XX/...XX',
    '&':  '.XX../X..X./X.X../.X.../X.X.X/X..X./.XX.X',
    '\'': '..X../..X..',
    '(':  '...X./..X../.X.../.X.../.X.../..X../...X.',
    ')':  '.X.../..X../...X./...X./...X./..X../.X...',
    '*':  '..X../X.X.X/.XXX./X.X.X/..X..',
    '+':  '...../..X../..X../XXXXX/..X../..X..',
    ',':  '...../...../...../...../...../.XX../.XX../..X../.X...',
    '-':  '...../...../...../XXXXX',
    '.':  '...../...../...../...../...../.XX../.XX..',
    '/':  '....X/....X/...X./..X../.X.../X..../X....',
    '0':  '.XXX./X...X/X..XX/X.X.X/XX..X/X...X/.XXX.',
    '1':  '..X../.XX../..X../..X../..X../..X../.XXX.',
    '2':  '.XXX./X...X/....X/...X./..X../.X.../XXXXX',
    '3':  'XXXXX/...X./..X../...X./....X/X...X/.XXX.',
    '4':  '...X./..XX./.X.X./X..X./XXXXX/...X./...X.',
    '5':  'XXXXX/X..../XXXX./....X/....X/X...X/.XXX.',
    '6':  '..XX./.X.../X..../XXXX./X...X/X...X/.XXX.',
    '7':  'XXXXX/....X/...X./..X../.X.../.X.../.X...',
    '8':  '.XXX./X...X/X...X/.XXX./X...X/X...X/.XXX.',
    '9':  '.XXX./X...X/X...X/.XXXX/....X/...X./.XX..',
    ':':  '...../...../.XX../.XX../...../.XX../.XX..',
    ';':  '...../...../.XX../.XX../...../.XX../.XX../..X../.X...',
    '<':  '...X./..X../.X.../X..../.X.../..X../...X.',
    '=':  '...../...../XXXXX/...../XXXXX',
    '>':  '.X.../..X../...X./....X/...X./..X../.X...',
    '?':  '.XXX./X...X/....X/...X./..X../...../..X..',
    '@':  '.XXX./X...X/....X/.XX.X/X.X.X/X.X.X/.XXX.',
    'A':  '..X../.X.X./X...X/X...X/XXXXX/X...X/X...X',
    'B':  'XXXX./X...X/X...X/XXXX./X...X/X...X/XXXX.',
    'C':  '.XXX./X...X/X..../X..../X..../X...X/.XXX.',
    'D':  'XXXX./X...X/X...X/X...X/X...X/X...X/XXXX.',
    'E':  'XXXXX/X..../X..../XXXX./X..../X..../XXXXX',
    'F':  'XXXXX/X..../X..../XXXX./X..../X..../X....',
    'G':  '.XXX./X...X/X..../X.XXX/X...X/X...X/.XXXX',
    'H':  'X...X/X...X/X...X/XXXXX/X...X/X...X/X...X',
    'I':  '.XXX./..X../..X../..X../..X../..X../.XXX.',
    'J':  '..XXX/...X./...X./...X./...X./X..X./.XX..',
    'K':  'X...X/X..X./X.X../XX.../X.X../X..X./X...X',
    'L':  'X..../X..../X..../X..../X..../X..../XXXXX',
    'M':  'X...X/XX.XX/X.X.X/X.X.X/X...X/X...X/X...X',
    'N':  'X...X/XX..X/X.X.X/X..XX/X...X/X...X/X...X',
    'O':  '.XXX./X...X/X...X/X...X/X...X/X...X/.XXX.',
    'P':  'XXXX./X...X/X...X/XXXX./X..../X..../X....',
    'Q':  '.XXX./X...X/X...X/X...X/X.X.X/X..X./.XX.X',
    'R':  'XXXX./X...X/X...X/XXXX./X.X../X..X./X...X',
    'S':  '.XXXX/X..../X..../.XXX./....X/....X/XXXX.',
    'T':  'XXXXX/..X../..X../..X../..X../..X../..X..',
    'U':  'X...X/X...X/X...X/X...X/X...X/X...X/.XXX.',
    'V':  'X...X/X...X/X...X/X...X/X...X/.X.X./..X..',
    'W':  'X...X/X...X/X...X/X.X.X/X.X.X/XX.XX/X...X',
    'X':  'X...X/X...X/.X.X./..X../.X.X./X...X/X...X',
    'Y':  'X...X/X...X/.X.X./..X../..X../..X../..X..',
    'Z':  'XXXXX/....X/...X./..X../.X.../X..../XXXXX',
    '[':  '.XXX./.X.../.X.../.X.../.X.../.X.../.XXX.',
    '\\': 'X..../X..../.X.../..X../...X./....X/....X',
    ']':  '.XXX./...X./...X./...X./...X./...X./.XXX.',
    '^':  '..X../.X.X./X...X',
    '_':  '...../...../...../...../...../...../...../XXXXX',
    '`':  '.X.../..X..',
    'a':  '...../...../.XXX./....X/.XXXX/X...X/.XXXX',
    'b':  'X..../X..../XXXX./X...X/X...X/X...X/XXXX.',
    'c':  '...../...../.XXX./X..../X..../X...X/.XXX.',
    'd':  '....X/....X/.XXXX/X...X/X...X/X...X/.XXXX',
    'e':  '...../...../.XXX./X...X/XXXXX/X..../.XXX.',
    'f':  '..XX./.X.../XXXX./.X.../.X.../.X.../.X...',
    'g':  '...../...../.XXXX/X...X/X...X/X...X/.XXXX/....X/.XXX.',
    'h':  'X..../X..../XXXX./X...X/X...X/X...X/X...X',
    'i':  '..X../...../.XX../..X../..X../..X../.XXX.',
    'j':  '...X./...../..XX./...X./...X./...X./...X./X..X./.XX..',
    'k':  'X..../X..../X..X./X.X../XX.../X.X../X..X.',
    'l':  '.XX../..X../..X../..X../..X../..X../.XXX.',
    'm':  '...../...../XX.X./X.X.X/X.X.X/X.X.X/X.X.X',
    'n':  '...../...../XXXX./X...X/X...X/X...X/X...X',
    'o':  '...../...../.XXX./X...X/X...X/X...X/.XXX.',
    'p':  '...../...../XXXX./X...X/X...X/X...X/XXXX./X..../X....',
    'q':  '...../...../.XXXX/X...X/X...X/X...X/.XXXX/....X/....X',
    'r':  '...../...../X.XX./XX..X/X..../X..../X....',
    's':  '...../...../.XXXX/X..../.XXX./....X/XXXX.',
    't':  '.X.../.X.../XXXX./.X.../.X.../.X..X/..XX.',
    'u':  '...../...../X...X/X...X/X...X/X..XX/.XX.X',
    'v':  '...../...../X...X/X...X/X...X/.X.X./..X..',
    'w':  '...../...../X...X/X...X/X.X.X/X.X.X/.X.X.',
    'x':  '...../...../X...X/.X.X./..X../.X.X./X...X',
    'y':  '...../...../X...X/X...X/X...X/X...X/.XXXX/....X/.XXX.',
    'z':  '...../...../XXXXX/...X./..X../.X.../XXXXX',
    '{':  '...XX/..X../..X../.XX../..X../..X../...XX',
    '|':  '..X../..X../..X../..X../..X../..X../..X..',
    '}':  'XX.../..X../..X../..XX./..X../..X../XX...',
    '~':  '...../...../.X.../X.X.X/...X.',
}

CELL_W = 6          # 5 columns of glyph + 1 of spacing
CELL_H = 10


def text_width(s, scale):
    return len(s) * CELL_W * scale


# ------------------------------------------------------------- RGB canvas

class Image(object):
    """A plain RGB raster with the few primitives the sheet needs."""

    def __init__(self, w, h, bg):
        self.w = w
        self.h = h
        self.px = [bytearray(bytes(bg) * w) for _ in range(h)]

    def put(self, x, y, colour):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x * 3:x * 3 + 3] = bytes(colour)

    def rect(self, x, y, w, h, colour):
        for yy in range(y, y + h):
            if 0 <= yy < self.h:
                for xx in range(x, x + w):
                    self.put(xx, yy, colour)

    def frame(self, x, y, w, h, colour):
        self.rect(x, y, w, 1, colour)
        self.rect(x, y + h - 1, w, 1, colour)
        self.rect(x, y, 1, h, colour)
        self.rect(x + w - 1, y, 1, h, colour)

    def text(self, x, y, s, colour, scale=1):
        for ch in s:
            rows = FONT5X9[ch] if ch in FONT5X9 else FONT5X9['?']
            for ry, row in enumerate(rows.split('/') if rows else []):
                for rx, cell in enumerate(row):
                    if cell == 'X':
                        self.rect(x + rx * scale, y + ry * scale,
                                  scale, scale, colour)
            x += CELL_W * scale

    def write_png(self, path):
        _write_png(path, [bytes(r) for r in self.px], self.w, self.h)
        return path


# --------------------------------------------------------- classic .info

def _u16(d, off):
    return struct.unpack(">H", d[off:off + 2])[0]


def _u32(d, off):
    return struct.unpack(">I", d[off:off + 4])[0]


class Bitmap(object):
    """Decoded icon imagery: rows of (r, g, b), or None where transparent."""

    def __init__(self, w, h, rows):
        self.w = w
        self.h = h
        self.rows = rows


def _planar_image(d, off):
    """Decode one struct Image + its bitplanes -> (Bitmap, next_off, depth)."""
    w, h, depth = struct.unpack(">hhh", d[off + 4:off + 10])
    has_data = _u32(d, off + 10)
    off += 20                       # sizeof(struct Image)
    if not has_data or w <= 0 or h <= 0:
        return None, off, depth
    rowbytes = ((w + 15) // 16) * 2
    pens = [[0] * w for _ in range(h)]
    for p in range(depth):
        base = off + p * rowbytes * h
        for y in range(h):
            rowoff = base + y * rowbytes
            for x in range(w):
                if d[rowoff + (x >> 3)] & (0x80 >> (x & 7)):
                    pens[y][x] |= 1 << p
    off += depth * rowbytes * h
    rows = [[WB_PENS[pen % len(WB_PENS)] for pen in row] for row in pens]
    return Bitmap(w, h, rows), off, depth


def read_classic(path):
    """Parse a .info: type, metadata and the normal/selected planar imagery."""
    with open(path, "rb") as fh:
        d = fh.read()
    if len(d) < 78 or _u16(d, 0) != WB_DISKMAGIC:
        raise ValueError("%s: not a classic .info file" % path)
    ga_render = _u32(d, 22)         # struct Gadget at 4: GadgetRender at +18
    ga_select = _u32(d, 26)         #                     SelectRender at +22
    do_default = _u32(d, 50)
    do_tooltypes = _u32(d, 54)
    do_drawer = _u32(d, 66)
    stack = _u32(d, 74)

    off = 78
    if do_drawer:
        off += 56                   # sizeof(struct DrawerData)
    images = []
    depths = []
    for present in (ga_render, ga_select):
        if not present:
            continue
        img, off, depth = _planar_image(d, off)
        images.append(img)
        depths.append(depth)

    def read_string(o):
        n = _u32(d, o)
        return (d[o + 4:o + 4 + n].split(b"\x00", 1)[0].decode("latin-1"),
                o + 4 + n)

    default_tool = None
    if do_default:
        default_tool, off = read_string(off)
    tooltypes = []
    if do_tooltypes:
        count = _u32(d, off) // 4 - 1
        off += 4
        for _ in range(count):
            s, off = read_string(off)
            tooltypes.append(s)
    return {"data": d, "type": d[48], "stack": stack, "tooltypes": tooltypes,
            "default_tool": default_tool, "images": images, "depths": depths}


# ------------------------------------------------------------ GlowIcon tail

def _glow_tail(d):
    """The trailing IFF FORM ICON, validated by its declared size.

    A plain substring search is unsafe -- Tool Type text can contain "FORM" --
    so accept only the FORM whose size reaches exactly the end of the file
    (one pad byte allowed), the same test tools/merge_glowicon.py makes.
    """
    i = 0
    while True:
        i = d.find(b"FORM", i)
        if i < 0:
            return None
        if i + 12 <= len(d) and d[i + 8:i + 12] == b"ICON":
            end = i + 8 + _u32(d, i + 4)
            if end == len(d) or end + (end & 1) == len(d):
                return d[i:]
        i += 1


class _Bits(object):
    """MSB-first bit reader over a byte string."""

    def __init__(self, data):
        self.d = data
        self.pos = 0
        self.limit = len(data) * 8

    def read(self, nbits):
        v = 0
        for _ in range(nbits):
            v = (v << 1) | ((self.d[self.pos >> 3] >> (7 - (self.pos & 7))) & 1)
            self.pos += 1
        return v


def _unrle(data, entry_bits, count):
    """GlowIcon RLE: 8-bit control codes over `entry_bits`-wide entries --
    n <= 127 copies n+1 entries verbatim, n >= 128 repeats one 257-n times."""
    bits = _Bits(data)
    out = []
    while len(out) < count and bits.pos + 8 <= bits.limit:
        n = bits.read(8)
        if n <= 127:
            for _ in range(n + 1):
                if bits.pos + entry_bits > bits.limit:
                    break
                out.append(bits.read(entry_bits))
        else:
            if bits.pos + entry_bits > bits.limit:
                break
            out.extend([bits.read(entry_bits)] * (257 - n))
    return out[:count]


def _unpacked(data, entry_bits, count):
    bits = _Bits(data)
    out = []
    while len(out) < count and bits.pos + entry_bits <= bits.limit:
        out.append(bits.read(entry_bits))
    return out[:count]


def read_glow(d):
    """Decode the appended GlowIcon imagery -> [Bitmap normal, Bitmap sel]."""
    tail = _glow_tail(d)
    if tail is None:
        return []
    off = 12                        # past "FORM" <size> "ICON"
    end = min(len(tail), 8 + _u32(tail, 4))
    width = height = None
    palette = None
    out = []
    while off + 8 <= end:
        cid = tail[off:off + 4]
        csize = _u32(tail, off + 4)
        body = tail[off + 8:off + 8 + csize]
        off += 8 + csize + (csize & 1)
        if cid == b"FACE" and len(body) >= 6:
            width = body[0] + 1     # every FACE/IMAG count is stored as n-1
            height = body[1] + 1
        elif cid == b"IMAG" and width and len(body) >= 10:
            transparent, ncolours, flags, icomp, pcomp, depth = body[:6]
            isize = _u16(body, 6) + 1
            psize = _u16(body, 8) + 1
            ncolours += 1
            data = body[10:10 + isize]
            pdata = body[10 + isize:10 + isize + psize]
            npix = width * height
            pixels = (_unrle(data, depth, npix) if icomp == 1
                      else _unpacked(data, depth, npix))
            if flags & 2:           # bit 1: this image carries a palette
                praw = (_unrle(pdata, 8, ncolours * 3) if pcomp == 1
                        else list(pdata[:ncolours * 3]))
                palette = [tuple(praw[i * 3:i * 3 + 3])
                           for i in range(ncolours) if len(praw) >= i * 3 + 3]
            if not palette:         # a second IMAG may reuse the first palette
                continue
            rows = []
            for y in range(height):
                row = []
                for x in range(width):
                    i = y * width + x
                    idx = pixels[i] if i < len(pixels) else 0
                    if (flags & 1) and idx == transparent:
                        row.append(None)
                    else:
                        row.append(palette[idx] if idx < len(palette)
                                   else (255, 0, 255))
                rows.append(row)
            out.append(Bitmap(width, height, rows))
    return out


# ------------------------------------------------------------------ layout

PAD = 8             # Workbench grey around the art inside a cell
GAP = 14            # between cells
CAP_H = 14          # caption strip under a cell
HEAD_H = 34         # the two heading lines above a row of cells
MARGIN = 20


def blit(img, bm, x, y, scale):
    """Draw a Bitmap; transparent pixels expose the checkerboard.

    The checkerboard is laid out in DESTINATION pixels and is darker than every
    Workbench pen, so it cannot be confused with the icon's own dithering.
    """
    for sy in range(bm.h):
        for sx in range(bm.w):
            colour = bm.rows[sy][sx]
            if colour is not None:
                img.rect(x + sx * scale, y + sy * scale, scale, scale, colour)
                continue
            for dy in range(scale):
                for dx in range(scale):
                    px = x + sx * scale + dx
                    py = y + sy * scale + dy
                    img.put(px, py,
                            CHECK_A if ((px // 6) + (py // 6)) & 1 else CHECK_B)


def shots_for(info, glow):
    """[(caption, Bitmap)] for one icon, classic first then glow."""
    shots = []
    for i, bm in enumerate(info["images"]):
        if bm is None:
            continue
        what = ("classic normal", "classic selected")[i] if i < 2 \
            else "classic %d" % i
        shots.append(("%s %dx%d %d-plane" % (what, bm.w, bm.h,
                                             info["depths"][i]), bm))
    for i, bm in enumerate(glow):
        what = ("glow normal", "glow selected")[i] if i < 2 else "glow %d" % i
        shots.append(("%s %dx%d" % (what, bm.w, bm.h), bm))
    return shots


def build_sheet(entries, scale, title):
    """entries: [(path, info, [(caption, Bitmap), ...])] -> a rendered Image."""
    blocks = []
    for path, info, shots in entries:
        widths = []
        heights = []
        for caption, bm in shots:
            widths.append(max(bm.w * scale + PAD * 2,
                              text_width(caption, 1) + 4))
            heights.append(bm.h * scale + PAD * 2)
        meta = "%s   type: %s   stack: %d   glow: %s" % (
            os.path.basename(path), TYPE_NAME.get(info["type"], info["type"]),
            info["stack"], "yes" if info["glow"] else "no")
        sub = "tool: %s   tooltypes: %s" % (
            info["default_tool"] or "-",
            ", ".join(info["tooltypes"]) if info["tooltypes"] else "none")
        if len(sub) > 110:
            sub = sub[:107] + "..."
        body_w = sum(widths) + GAP * (len(widths) - 1) if widths else 0
        blocks.append({
            "meta": meta, "sub": sub, "shots": shots, "widths": widths,
            "w": max(body_w, text_width(meta, 1), text_width(sub, 1)),
            "h": HEAD_H + (max(heights) if heights else 0) + CAP_H,
        })

    title_h = 30
    sheet_w = max([b["w"] for b in blocks] + [text_width(title, 2)]) \
        + MARGIN * 2
    sheet_h = title_h + sum(b["h"] + 18 for b in blocks) + MARGIN * 2
    img = Image(sheet_w, sheet_h, BG)
    img.text(MARGIN, MARGIN, title, INK, 2)

    y = MARGIN + title_h + 6
    for i, b in enumerate(blocks):
        if i:
            img.rect(MARGIN, y - 10, sheet_w - MARGIN * 2, 1, RULE)
        img.text(MARGIN, y, b["meta"], INK, 1)
        img.text(MARGIN, y + 13, b["sub"], DIM, 1)
        x = MARGIN
        top = y + HEAD_H
        for n, (caption, bm) in enumerate(b["shots"]):
            cw = bm.w * scale + PAD * 2
            ch = bm.h * scale + PAD * 2
            img.rect(x, top, cw, ch, GREY)
            img.frame(x, top, cw, ch, RULE)
            blit(img, bm, x + PAD, top + PAD, scale)
            img.text(x, top + ch + 4, caption, DIM, 1)
            x += b["widths"][n] + GAP
        y += b["h"] + 18
    return img


def main(argv):
    ap = argparse.ArgumentParser(description="contact-sheet PNG previews of "
                                             "AmigaOS .info icons")
    ap.add_argument("paths", nargs="+", help="directories and/or .info files")
    ap.add_argument("--out", required=True, help="the contact sheet PNG")
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--detail-dir", help="also write one PNG per icon here")
    ap.add_argument("--detail-scale", type=int, default=8)
    ap.add_argument("--title")
    args = ap.parse_args(argv)

    files = []
    for p in args.paths:
        if os.path.isdir(p):
            files.extend(os.path.join(p, n) for n in sorted(os.listdir(p))
                         if n.lower().endswith(".info"))
        else:
            files.append(p)
    if not files:
        sys.exit("icon_sheet: no .info files found")

    entries = []
    for f in files:
        try:
            info = read_classic(f)
        except (ValueError, IndexError, struct.error) as err:
            print("skipped %s: %s" % (f, err))
            continue
        glow = read_glow(info["data"])
        info["glow"] = glow
        entries.append((f, info, shots_for(info, glow)))
        print("%-32s %-8s classic:%d glow:%d"
              % (os.path.basename(f), TYPE_NAME.get(info["type"], "?"),
                 len([i for i in info["images"] if i]), len(glow)))
    if not entries:
        sys.exit("icon_sheet: nothing could be decoded")

    title = args.title or os.path.basename(os.path.abspath(
        args.paths[0] if os.path.isdir(args.paths[0])
        else os.path.dirname(os.path.abspath(args.paths[0]))))
    build_sheet(entries, args.scale, title).write_png(args.out)
    print("wrote %s" % args.out)

    if args.detail_dir:
        if not os.path.isdir(args.detail_dir):
            os.makedirs(args.detail_dir)
        for path, info, shots in entries:
            base = os.path.basename(path)
            if base.lower().endswith(".info"):
                base = base[:-5]
            out = os.path.join(args.detail_dir, base + "-detail.png")
            build_sheet([(path, info, shots)], args.detail_scale,
                        base).write_png(out)
            print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
