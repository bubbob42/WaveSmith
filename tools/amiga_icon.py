"""
amiga_icon -- generate classic (old-style) AmigaOS Workbench .info icons.

Reusable helper for any AmigaOS 3.x project cloned from this starter.
Writes the pre-OS3.5 "planar" icon format: a DiskObject with one (optionally
two) Image structure(s) whose bitmap is stored as bitplanes. Four colours means
two bitplanes, drawn in the running Workbench's pens 0..3 (grey / black / white
/ blue in the standard 4-colour palette).

Pure standard library -- no third-party modules. Also emits an RGB PNG preview
(hand-rolled via zlib) so the art can be eyeballed on the host before it is
tried on real Workbench.

Authoring an icon:

    from amiga_icon import Canvas, write_info, GREY, BLACK, WHITE, BLUE
    c = Canvas(52, 40, bg=GREY)
    c.fill_rect(6, 3, 45, 36, WHITE)
    c.rect(6, 3, 45, 36, BLACK)
    ...
    write_info("dist/MyApp.info", c)          # a WBTOOL icon
    c.to_png("preview.png", scale=8)

Canvas may also be built from ASCII art (Canvas.from_ascii) when that is
clearer than drawing primitives.

Format references: workbench/workbench.h (struct DiskObject / Gadget / Image),
icon.library autodocs, and the well-known ".info" file layout.
"""

import struct
import zlib

# Classic AmigaOS 3.x 4-colour Workbench pens. Icons are drawn in pen numbers,
# not RGB; these RGB values are only used for the host PNG preview so it looks
# roughly like Workbench.
GREY = 0
BLACK = 1
WHITE = 2
BLUE = 3

PREVIEW_PALETTE = {
    GREY:  (170, 170, 170),
    BLACK: (0, 0, 0),
    WHITE: (255, 255, 255),
    BLUE:  (59, 103, 163),
}

# struct DiskObject
WB_DISKMAGIC = 0xE310
WB_DISKVERSION = 1

# do_Type
WBDISK = 1
WBDRAWER = 2
WBTOOL = 3
WBPROJECT = 4
WBGARBAGE = 5
WBKICK = 6

# Gadget flags / activation / type (intuition/intuition.h)
GFLG_GADGHCOMP = 0x0000     # highlight by complementing the image
GFLG_GADGHIMAGE = 0x0002    # highlight with a second image (SelectRender)
GFLG_GADGIMAGE = 0x0004     # GadgetRender points to an Image
GACT_RELVERIFY = 0x0001
GACT_IMMEDIATE = 0x0002
GTYP_BOOLGADGET = 0x0001

NO_ICON_POSITION = 0x80000000


class Canvas(object):
    """A rectangular grid of pen numbers (0..3 for a 4-colour icon)."""

    def __init__(self, w, h, bg=GREY):
        self.w = w
        self.h = h
        self.px = [[bg for _ in range(w)] for _ in range(h)]

    def set(self, x, y, pen):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = pen

    def fill_rect(self, x0, y0, x1, y1, pen):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, pen)

    def rect(self, x0, y0, x1, y1, pen):
        """One-pixel outline rectangle (inclusive corners)."""
        self.hline(x0, x1, y0, pen)
        self.hline(x0, x1, y1, pen)
        self.vline(x0, y0, y1, pen)
        self.vline(x1, y0, y1, pen)

    def hline(self, x0, x1, y, pen):
        for x in range(x0, x1 + 1):
            self.set(x, y, pen)

    def vline(self, x, y0, y1, pen):
        for y in range(y0, y1 + 1):
            self.set(x, y, pen)

    def swap_pens(self, a, b):
        """Swap two pen numbers everywhere on the canvas."""
        for row in self.px:
            for x in range(self.w):
                if row[x] == a:
                    row[x] = b
                elif row[x] == b:
                    row[x] = a

    def invert_bw(self):
        """Swap BLACK<->WHITE (pens 1 and 2). The artwork is authored for the
        3.x Workbench palette (BLACK=1, WHITE=2); the Kickstart-1.3 Workbench
        maps those pens the other way (pen 1 = white, pen 2 = black), so an icon
        shown on a 1.3 screen comes out black/white reversed. Calling this before
        write_info renders it correctly on 1.3. Grey (0) and blue (3) are left
        as-is (the 1.3 palette differs there too, but only B/W was asked for)."""
        self.swap_pens(BLACK, WHITE)

    def copy(self):
        c = Canvas(self.w, self.h)
        c.px = [row[:] for row in self.px]
        return c

    def selected(self):
        """A selected-state image for the KICKSTART-1.3 icons.

        Only the ARTWORK inverts: pens 1 and 2 (the light/dark pair in both the
        3.x and the 1.3 palette) swap, while pen 0 -- the surrounding
        background, which the desktop shows through -- and the pen-3 accent are
        left alone.

        Pass this as write_info(select=...) for 1.3 icons ONLY. On 2.x/3.x the
        ordinary GFLG_GADGHCOMP complement is what is wanted, so those icons
        ship a single image and this is not used."""
        c = self.copy()
        c.swap_pens(BLACK, WHITE)
        return c

    def max_pen(self):
        m = 0
        for row in self.px:
            for p in row:
                if p > m:
                    m = p
        return m

    @classmethod
    def from_ascii(cls, art, charmap):
        """Build from a list/multiline string; charmap maps chars to pens.

        Every row is padded to the widest with the first row's leftmost pen's
        char is NOT assumed -- unmapped/short cells become pen 0.
        """
        if isinstance(art, str):
            rows = art.strip("\n").split("\n")
        else:
            rows = list(art)
        w = max(len(r) for r in rows)
        h = len(rows)
        c = cls(w, h, bg=0)
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                c.set(x, y, charmap.get(ch, 0))
        return c

    # -- host preview ---------------------------------------------------------

    def to_png(self, path, scale=8, palette=None):
        pal = palette or PREVIEW_PALETTE
        pw = self.w * scale
        ph = self.h * scale
        rows = []
        for y in range(self.h):
            row = bytearray()
            for x in range(self.w):
                r, g, b = pal.get(self.px[y][x], (255, 0, 255))
                row += bytes((r, g, b)) * scale
            rows.extend([bytes(row)] * scale)
        _write_png(path, rows, pw, ph)


def _write_png(path, rgb_rows, w, h):
    def chunk(typ, data):
        body = typ + data
        return (struct.pack(">I", len(data)) + body
                + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    raw = b"".join(b"\x00" + r for r in rgb_rows)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)   # 8-bit truecolour
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


# -- .info writer -------------------------------------------------------------

def _planes(canvas, depth):
    """Pack a canvas into `depth` bitplanes, plane 0 first, MSB-left rows."""
    wordwidth = (canvas.w + 15) // 16
    rowbytes = wordwidth * 2
    out = bytearray()
    for p in range(depth):
        plane = bytearray(rowbytes * canvas.h)
        for y in range(canvas.h):
            base = y * rowbytes
            for x in range(canvas.w):
                if (canvas.px[y][x] >> p) & 1:
                    plane[base + (x >> 3)] |= 0x80 >> (x & 7)
        out += plane
    return bytes(out)


def read_icon_image(path):
    """Read the FIRST (gadget-render) image out of a classic .info icon back
    into a Canvas -- the inverse of write_info()/_planes(). Used to reuse an
    existing hand-drawn classic icon's art elsewhere (e.g. a scaled-up About
    box picture) instead of re-deriving it from something else."""
    with open(path, "rb") as fh:
        d = fh.read()
    if len(d) < 78 or struct.unpack(">H", d[0:2])[0] != WB_DISKMAGIC:
        raise ValueError("%s: not a classic .info file" % path)
    do_drawer = struct.unpack(">I", d[66:70])[0]
    off = 78
    if do_drawer:
        off += 56
    w, h, depth = struct.unpack(">hhh", d[off + 4:off + 10])
    imgdata_flag = struct.unpack(">I", d[off + 10:off + 14])[0]
    off += 20   # struct Image header size
    if not imgdata_flag:
        raise ValueError("%s: gadget image has no data" % path)
    wordwidth = (w + 15) // 16
    rowbytes = wordwidth * 2
    canvas = Canvas(w, h, bg=0)
    for p in range(depth):
        base = off + p * rowbytes * h
        for y in range(h):
            rowoff = base + y * rowbytes
            for x in range(w):
                byte = d[rowoff + (x >> 3)]
                if byte & (0x80 >> (x & 7)):
                    canvas.px[y][x] |= (1 << p)
    return canvas


def scale_canvas(canvas, factor):
    """Nearest-neighbour integer upscale (each source pixel -> factor x
    factor block). Used to enlarge small hand-drawn icon art for a bigger
    context (an About-box picture) without softening/blurring it."""
    out = Canvas(canvas.w * factor, canvas.h * factor, bg=0)
    for y in range(canvas.h):
        for x in range(canvas.w):
            pen = canvas.px[y][x]
            for dy in range(factor):
                for dx in range(factor):
                    out.set(x * factor + dx, y * factor + dy, pen)
    return out


def _image_struct(canvas, depth):
    # struct Image: LeftEdge, TopEdge, Width, Height, Depth, ImageData(ptr),
    # PlanePick, PlaneOnOff, NextImage(ptr).
    return struct.pack(
        ">hhhhhIBBI",
        0, 0, canvas.w, canvas.h, depth,
        1,                       # ImageData: non-NULL placeholder
        (1 << depth) - 1,        # PlanePick: all planes carry data
        0,                       # PlaneOnOff
        0,                       # NextImage
    )


def _string_chunk(s):
    b = s.encode("latin-1") + b"\x00"
    return struct.pack(">I", len(b)) + b


def crop_to_art(canvas, select=None):
    """canvas -- and select, to the SAME box, so the two stay aligned -- cut
    to the artwork: the rows and columns all of pen 0 around it gone. Pen 0
    is the background Workbench shows through; a border of it is empty space
    that nonetheless is the icon's box (what a click, a drag and the selected
    state take). Returns (canvas, select); unchanged when nothing is drawn."""
    xs, ys = [], []
    for c in (canvas, select):
        if c is None:
            continue
        for y in range(c.h):
            for x in range(c.w):
                if c.px[y][x] != GREY:
                    xs.append(x)
                    ys.append(y)
    if not xs:
        return canvas, select
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)

    def cut(c):
        out = Canvas(x1 - x0 + 1, y1 - y0 + 1)
        out.px = [row[x0:x1 + 1] for row in c.px[y0:y1 + 1]]
        return out

    return cut(canvas), (cut(select) if select is not None else None)


def write_info(path, canvas, do_type=WBTOOL, default_tool=None,
               tooltypes=None, stack=32768, select=None, crop=True):
    """Write a classic .info icon for `canvas`.

    do_type      WBTOOL for a program, WBPROJECT for a data/settings file.
    default_tool project icons: the program that opens the file (absolute path).
    tooltypes    list of "KEY=VALUE" strings, or None.
    select       Canvas for the selected/highlighted image, or None (the
                 default) to ship a single image and let Workbench highlight by
                 COMPLEMENTING it.

                 Complementing is the normal, wanted behaviour on 2.x/3.x --
                 pass None there. It is only wrong for the Kickstart-1.3 icons
                 (and for GlowIcons), where inverting the whole gadget
                 rectangle flips the surrounding blue background along with the
                 artwork. Those generators pass select=canvas.selected().
    crop         True (the default): the image cut to its artwork first
                 (crop_to_art), so the icon has no empty border.
    """
    if crop:
        canvas, select = crop_to_art(canvas, select)
    depth = max(1, (max(canvas.max_pen(), 1)).bit_length())

    has_select = select is not None
    flags = GFLG_GADGIMAGE | (GFLG_GADGHIMAGE if has_select else GFLG_GADGHCOMP)

    gadget = struct.pack(
        ">IhhhhHHHIIIiIHI",
        0,                       # ga_Next
        0, 0,                    # ga_LeftEdge, ga_TopEdge
        canvas.w, canvas.h,      # ga_Width, ga_Height
        flags,                   # ga_Flags
        GACT_RELVERIFY | GACT_IMMEDIATE,   # ga_Activation
        GTYP_BOOLGADGET,         # ga_GadgetType
        1,                       # ga_GadgetRender: non-NULL placeholder
        1 if has_select else 0,  # ga_SelectRender
        0,                       # ga_GadgetText
        0,                       # ga_MutualExclude
        0,                       # ga_SpecialInfo
        0,                       # ga_GadgetID
        0,                       # ga_UserData
    )

    # Disk / drawer / trashcan icons carry a DrawerData block (the drawer
    # window box); tool/project icons do not. Emitting it lets the release
    # tooling snapshot the window with set_icon_pos.py --window.
    is_drawer = do_type in (WBDISK, WBDRAWER, WBGARBAGE)

    disk = struct.pack(">HH", WB_DISKMAGIC, WB_DISKVERSION)
    disk += gadget
    disk += struct.pack(
        ">BBIIIIIII",
        do_type, 0,              # do_Type, pad
        1 if default_tool else 0,          # do_DefaultTool ptr flag
        1 if tooltypes else 0,             # do_ToolTypes ptr flag
        NO_ICON_POSITION,        # do_CurrentX
        NO_ICON_POSITION,        # do_CurrentY
        1 if is_drawer else 0,   # do_DrawerData ptr flag
        0,                       # do_ToolWindow
        stack,                   # do_StackSize
    )

    body = disk
    if is_drawer:
        # struct DrawerData: NewWindow(48) + dd_CurrentX(4) + dd_CurrentY(4).
        # The box (LeftEdge,TopEdge,Width,Height) and view origin are usually
        # overwritten later by set_icon_pos.py --window; these are sane
        # defaults. Pens -1/-1 use the screen defaults; Type 1 = WBENCHSCREEN.
        body += struct.pack(
            ">hhhhBBIIIIIIIhhHHHll",
            50, 40, 350, 200,    # LeftEdge, TopEdge, Width, Height
            255, 255,            # DetailPen, BlockPen (-1 -> screen defaults)
            0,                   # IDCMPFlags
            0,                   # Flags
            0, 0, 0, 0, 0,       # FirstGadget/CheckMark/Title/Screen/BitMap
            90, 40,              # MinWidth, MinHeight
            0xFFFF, 0xFFFF,      # MaxWidth, MaxHeight
            1,                   # Type = WBENCHSCREEN
            0, 0,                # dd_CurrentX, dd_CurrentY (view origin)
        )
    body += _image_struct(canvas, depth) + _planes(canvas, depth)
    if has_select:
        sdepth = max(1, (max(select.max_pen(), 1)).bit_length())
        body += _image_struct(select, sdepth) + _planes(select, sdepth)
    if default_tool:
        body += _string_chunk(default_tool)
    if tooltypes:
        body += struct.pack(">I", (len(tooltypes) + 1) * 4)
        for t in tooltypes:
            body += _string_chunk(t)

    with open(path, "wb") as f:
        f.write(body)


# -- .info reader / tool-type editor ------------------------------------------
#
# A classic .info is a DiskObject header (78 bytes) followed by a fixed sequence
# of optional sections, each present only when its pointer field in the header
# is non-zero:
#
#     [DrawerData 56b]  [image1]  [image2]  [DefaultTool str]
#     [ToolTypes]  [ToolWindow str]  [trailer: OS3.5 GlowIcon IFF, etc.]
#
# To retag an icon we parse up to the ToolTypes slot, keep everything before it
# and everything after it verbatim (so image data and any appended colour-icon
# survive untouched), and splice a freshly serialised ToolTypes list in between.


def _u16(d, off):
    return struct.unpack(">H", d[off:off + 2])[0]


def _u32(d, off):
    return struct.unpack(">I", d[off:off + 4])[0]


class IconFile(object):
    """A parsed classic Workbench .info, editable in its ToolTypes only.

        icon = IconFile.load("Foo.info")
        icon.tooltypes = ["FORMAT=3475", "UNITS=MM"]
        icon.save()                      # in place, everything else preserved

    Only the ToolTypes are rewritten; the gadget, images, default tool and any
    trailing OS3.5 colour-icon data are kept byte-for-byte.
    """

    def __init__(self, data):
        self.data = data
        self._parse()

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            obj = cls(f.read())
        obj.path = path
        return obj

    def _skip_image(self, off):
        d = self.data
        w = struct.unpack(">h", d[off + 4:off + 6])[0]
        h = struct.unpack(">h", d[off + 6:off + 8])[0]
        depth = struct.unpack(">h", d[off + 8:off + 10])[0]
        has_data = _u32(d, off + 10)
        off += 20
        if has_data:
            rowbytes = ((w + 15) // 16) * 2
            off += depth * h * rowbytes
        return off

    def _read_string(self, off):
        d = self.data
        n = _u32(d, off)
        raw = d[off + 4:off + 4 + n]
        return raw.split(b"\x00", 1)[0].decode("latin-1"), off + 4 + n

    def _parse(self):
        d = self.data
        self.path = None
        if len(d) < 78:
            raise ValueError("not a .info file (too short)")
        if _u16(d, 0) != WB_DISKMAGIC:
            raise ValueError("not a classic .info file (bad magic 0x%04X)"
                             % _u16(d, 0))
        self.do_type = d[48]
        ga_render = _u32(d, 22)
        ga_select = _u32(d, 26)
        do_default = _u32(d, 50)
        do_tooltypes = _u32(d, 54)
        do_drawer = _u32(d, 66)

        off = 78
        if do_drawer:
            off += 56
        if ga_render:
            off = self._skip_image(off)
        if ga_select:
            off = self._skip_image(off)
        if do_default:
            self.default_tool, off = self._read_string(off)
        else:
            self.default_tool = None

        # The ToolTypes slot begins here whether or not any are stored.
        self._tt_start = off
        self.tooltypes = []
        if do_tooltypes:
            arrsize = _u32(d, off)
            off += 4
            count = arrsize // 4 - 1
            for _ in range(count):
                s, off = self._read_string(off)
                self.tooltypes.append(s)
        self._tt_end = off
        self._head = d[:self._tt_start]      # up to (not incl.) ToolTypes
        self._trailer = d[self._tt_end:]      # ToolWindow + colour-icon, verbatim

    def serialize(self):
        head = bytearray(self._head)
        struct.pack_into(">I", head, 54, 1 if self.tooltypes else 0)
        body = bytes(head)
        if self.tooltypes:
            body += struct.pack(">I", (len(self.tooltypes) + 1) * 4)
            for t in self.tooltypes:
                body += _string_chunk(t)
        body += self._trailer
        return body

    def save(self, path=None):
        out = path or self.path
        if out is None:
            raise ValueError("no path to save to")
        with open(out, "wb") as f:
            f.write(self.serialize())
        return out


def tooltype_key(entry):
    """The KEY of a "KEY=VALUE" (or bare "KEY") tool type, upper-cased.

    A leading '(' marks a Workbench-disabled entry; it is part of the key for
    matching so an enabled and a disabled copy of the same key are distinct.
    """
    return entry.split("=", 1)[0].strip().upper()


def merge_tooltypes(existing, assignments, removes=None, replace=False):
    """Return a new ToolTypes list.

    assignments  list of "KEY=VALUE" (or "KEY") strings to set. In merge mode an
                 assignment replaces the first entry with the same key, else it
                 is appended; order is otherwise preserved.
    removes      list of keys to drop (case-insensitive).
    replace      when True, ignore `existing` and use `assignments` verbatim.
    """
    removes = [k.upper() for k in (removes or [])]
    if replace:
        result = list(assignments)
    else:
        result = list(existing)
        for a in assignments:
            key = tooltype_key(a)
            for i in range(len(result)):
                if tooltype_key(result[i]) == key:
                    result[i] = a
                    break
            else:
                result.append(a)
    if removes:
        result = [e for e in result if tooltype_key(e) not in removes]
    return result
