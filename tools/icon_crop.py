#!/usr/bin/env python3
"""icon_crop.py -- a Workbench .info cut to its artwork: the rows and
columns of transparent pixels around the image removed, classic and
GlowIcon alike.

    icon_crop.py IN.info OUT.info [--classic-only]     one icon
    icon_crop.py --check IN.info OUT.info              the cut verified

Classic (planar) images: transparent = pen 0. The normal and the selected
image are cut to ONE box (their union), so they stay aligned; the gadget's
size follows by the same amount. Everything else -- the DiskObject, the
DrawerData, the default tool, the tool types, the tool window, the drawer
extension -- is kept byte for byte.

GlowIcon (the IFF FORM ICON tail, OS 3.5): transparent = an IMAG's
transparent colour, when its flags say it has one (bit 0). All IMAGs are
cut to one box; each is coded again with ITS depth and ITS compression
(RLE as the reader in icon_sheet.py has it: an 8-bit control n <= 127 --
n+1 literals -- or n >= 128 -- one entry 257-n times; MSB first); the
palettes are copied unchanged; FACE takes the new size, its flags and
aspect kept. A GlowIcon with an image that has no transparent colour, or
with chunks this tool does not know (ARGB, PNG), keeps its glow part.

Checked 2026-10-26 over every icon of ~/Developer/AmigaApps (702 distinct,
564 cut, 188 of them GlowIcons): each cut verified by --check, and all of
them loaded by icon.library 47 (OS 3.2, the A1200) and shown by Workbench
(the A4000T).

Pure standard library.
"""

import struct
import sys

WB_DISKMAGIC = 0xE310


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def u32(d, o):
    return struct.unpack(">I", d[o:o + 4])[0]


# ------------------------------------------------------------------ classic

def read_image(d, off):
    """struct Image at off -> dict (pens as rows), next offset"""
    left, top, w, h, depth = struct.unpack(">hhhhh", d[off:off + 10])
    data_ptr = u32(d, off + 10)
    pick, onoff = d[off + 14], d[off + 15]
    nxt = u32(d, off + 16)
    img = {"left": left, "top": top, "w": w, "h": h, "depth": depth, "data": data_ptr,
           "pick": pick, "onoff": onoff, "next": nxt, "pens": None}
    off += 20
    if data_ptr and w > 0 and h > 0:
        rb = ((w + 15) // 16) * 2
        pens = [[0] * w for _ in range(h)]
        for p in range(depth):
            base = off + p * rb * h
            for y in range(h):
                ro = base + y * rb
                for x in range(w):
                    if d[ro + (x >> 3)] & (0x80 >> (x & 7)):
                        pens[y][x] |= 1 << p
        img["pens"] = pens
        off += depth * rb * h
    return img, off


def write_image(img):
    out = struct.pack(">hhhhh", img["left"], img["top"], img["w"], img["h"], img["depth"])
    out += struct.pack(">I", img["data"]) + bytes((img["pick"], img["onoff"])) + struct.pack(">I", img["next"])
    if img["pens"] is not None:
        w, h = img["w"], img["h"]
        rb = ((w + 15) // 16) * 2
        planes = bytearray()
        for p in range(img["depth"]):
            for y in range(h):
                row = bytearray(rb)
                for x in range(w):
                    if img["pens"][y][x] & (1 << p):
                        row[x >> 3] |= 0x80 >> (x & 7)
                planes += row
        out += bytes(planes)
    return out


def bbox(rows, opaque):
    """(x0, y0, x1, y1) of the pixels opaque(v) says show, or None"""
    xs, ys = [], []
    for y, row in enumerate(rows):
        for x, v in enumerate(row):
            if opaque(v):
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def union(a, b):
    if a is None:
        return b
    if b is None:
        return a
    return min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])


def cut(rows, box):
    x0, y0, x1, y1 = box
    return [row[x0:x1 + 1] for row in rows[y0:y1 + 1]]


def glow_start(d):
    """offset of the trailing FORM ICON (its size reaching the end), or None"""
    i = 0
    while True:
        i = d.find(b"FORM", i)
        if i < 0:
            return None
        if i + 12 <= len(d) and d[i + 8:i + 12] == b"ICON":
            end = i + 8 + u32(d, i + 4)
            if end == len(d) or end + (end & 1) == len(d):
                return i
        i += 1


def parse(d):
    if len(d) < 78 or u16(d, 0) != WB_DISKMAGIC:
        raise ValueError("not a classic .info")
    ga_render, ga_select = u32(d, 22), u32(d, 26)
    off = 78 + (56 if u32(d, 66) else 0)
    head = d[:off]
    imgs = []
    for present in (ga_render, ga_select):
        if present:
            img, off = read_image(d, off)
            imgs.append(img)
    g = glow_start(d)
    if g is not None and g < off:
        g = None
    rest = d[off:g] if g is not None else d[off:]
    glow = d[g:] if g is not None else b""
    return {"head": head, "imgs": imgs, "rest": rest, "glow": glow}


# ------------------------------------------------------------------ GlowIcon

class BitReader(object):
    def __init__(self, data):
        self.d, self.pos, self.limit = data, 0, len(data) * 8

    def read(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((self.d[self.pos >> 3] >> (7 - (self.pos & 7))) & 1)
            self.pos += 1
        return v


class BitWriter(object):
    def __init__(self):
        self.out, self.acc, self.n = bytearray(), 0, 0

    def write(self, v, bits):
        for i in range(bits - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.acc)
                self.acc, self.n = 0, 0

    def done(self):
        if self.n:
            self.out.append(self.acc << (8 - self.n))
            self.acc, self.n = 0, 0
        return bytes(self.out)


def unrle(data, bits, count):
    r, out = BitReader(data), []
    while len(out) < count and r.pos + 8 <= r.limit:
        n = r.read(8)
        if n <= 127:
            for _ in range(n + 1):
                if r.pos + bits > r.limit:
                    break
                out.append(r.read(bits))
        else:
            if r.pos + bits > r.limit:
                break
            out.extend([r.read(bits)] * (257 - n))
    return out[:count]


def unpacked(data, bits, count):
    r, out = BitReader(data), []
    while len(out) < count and r.pos + bits <= r.limit:
        out.append(r.read(bits))
    return out[:count]


def rle(pixels, bits):
    """runs of 3 or more as one repeated entry (3..128), the rest as
    literals (1..128). Never the control byte 128 (a run of 129): no
    original GlowIcon has one, and ByteRun1 reads that byte as nothing"""
    w, i, n = BitWriter(), 0, len(pixels)
    lit = []

    def flush():
        while lit:
            chunk = lit[:128]
            del lit[:128]
            w.write(len(chunk) - 1, 8)
            for v in chunk:
                w.write(v, bits)

    while i < n:
        j = i
        while j < n and pixels[j] == pixels[i] and j - i < 128:
            j += 1
        run = j - i
        if run >= 3:
            flush()
            w.write(257 - run, 8)
            w.write(pixels[i], bits)
            i = j
        else:
            lit.append(pixels[i])
            i += 1
    flush()
    return w.done()


def packed(pixels, bits):
    w = BitWriter()
    for v in pixels:
        w.write(v, bits)
    return w.done()


def chunks(form):
    """[(id, body)] of a FORM ICON"""
    out, off, end = [], 12, min(len(form), 8 + u32(form, 4))
    while off + 8 <= end:
        cid, size = form[off:off + 4], u32(form, off + 4)
        out.append((cid, form[off + 8:off + 8 + size]))
        off += 8 + size + (size & 1)
    return out


def glow_images(form):
    """(face, [imag dicts with pixel rows]) or (None, reason)"""
    face, imags = None, []
    for cid, body in chunks(form):
        if cid == b"FACE":
            face = body
        elif cid == b"IMAG":
            if face is None or len(body) < 10:
                return None, "IMAG before FACE"
            w, h = face[0] + 1, face[1] + 1
            tr, ncol, flags, icomp, pcomp, depth = body[:6]
            isize, psize = u16(body, 6) + 1, u16(body, 8) + 1
            data = body[10:10 + isize]
            pal = body[10 + isize:10 + isize + psize] if flags & 2 else b""
            px = unrle(data, depth, w * h) if icomp == 1 else unpacked(data, depth, w * h)
            if len(px) < w * h:
                return None, "an IMAG decodes short"
            imags.append({"tr": tr, "ncol": ncol, "flags": flags, "icomp": icomp, "pcomp": pcomp,
                          "depth": depth, "pal": pal, "psize": psize,
                          "rows": [px[y * w:(y + 1) * w] for y in range(h)], "raw": body})
        elif cid not in (b"FACE", b"IMAG"):
            return None, "a chunk this tool does not know: %s" % cid.decode("latin-1", "replace")
    if face is None:
        return None, "no FACE"
    return (face, imags), None


def glow_box(imags):
    box = None
    for im in imags:
        if not im["flags"] & 1:
            return None, "an image without a transparent colour"
        tr = im["tr"]
        box = union(box, bbox(im["rows"], lambda v, tr=tr: v != tr))
    return box, None


def write_glow(face, imags, box):
    x0, y0, x1, y1 = box
    w, h = x1 - x0 + 1, y1 - y0 + 1
    nface = bytes((w - 1, h - 1)) + face[2:]
    body = b"ICON" + chunk(b"FACE", nface)
    for im in imags:
        px = [v for row in cut(im["rows"], box) for v in row]
        data = rle(px, im["depth"]) if im["icomp"] == 1 else packed(px, im["depth"])
        head = bytes((im["tr"], im["ncol"], im["flags"], im["icomp"], im["pcomp"], im["depth"]))
        head += struct.pack(">HH", len(data) - 1, im["psize"] - 1)
        body += chunk(b"IMAG", head + data + im["pal"])
    return b"FORM" + struct.pack(">I", len(body)) + body


def chunk(cid, body):
    return cid + struct.pack(">I", len(body)) + body + (b"\x00" if len(body) & 1 else b"")


# ------------------------------------------------------------------ the cut

def crop(d, classic_only=False):
    """the icon cut -> (bytes, report dict)"""
    ic = parse(d)
    rep = {"classic": None, "glow": None, "glow_note": None}
    head = bytearray(ic["head"])
    imgs = ic["imgs"]
    box = None
    for im in imgs:
        if im["pens"] is not None:
            box = union(box, bbox(im["pens"], lambda v: v != 0))
    same = len(set((im["w"], im["h"]) for im in imgs)) <= 1
    if box is not None and same and imgs:
        w0, h0 = imgs[0]["w"], imgs[0]["h"]
        nw, nh = box[2] - box[0] + 1, box[3] - box[1] + 1
        if (nw, nh) != (w0, h0):
            for im in imgs:
                if im["pens"] is not None:
                    im["pens"] = cut(im["pens"], box)
                im["w"], im["h"] = nw, nh
            gw, gh = struct.unpack(">hh", bytes(head[12:16]))
            struct.pack_into(">hh", head, 12, max(1, gw - (w0 - nw)), max(1, gh - (h0 - nh)))
        rep["classic"] = ((w0, h0), (nw, nh))
    body = bytes(head) + b"".join(write_image(im) for im in imgs) + ic["rest"]
    glow = ic["glow"]
    if glow and not classic_only:
        got, why = glow_images(glow)
        if got is None:
            rep["glow_note"] = why
        else:
            face, imags = got
            gbox, why = glow_box(imags)
            fw, fh = face[0] + 1, face[1] + 1
            if gbox is None:
                rep["glow_note"] = why or "nothing shows"
            else:
                nw, nh = gbox[2] - gbox[0] + 1, gbox[3] - gbox[1] + 1
                if (nw, nh) != (fw, fh):
                    glow = write_glow(face, imags, gbox)
                rep["glow"] = ((fw, fh), (nw, nh))
        body += glow
    return body, rep


def check(a, b):
    """b is a's cut: everything but the images and the gadget's size the
    same, each image the original's pixels in the one box all its shown
    pixels fit, palettes and IMAG settings unchanged"""
    ia, ib = parse(a), parse(b)
    problems = []
    if ia["head"][:12] != ib["head"][:12] or ia["head"][16:] != ib["head"][16:] or ia["rest"] != ib["rest"]:
        problems.append("the classic structure changed besides the image and the gadget's size")
    if len(ia["imgs"]) != len(ib["imgs"]):
        problems.append("the number of classic images changed")
    box = None
    for im in ia["imgs"]:
        if im["pens"] is not None:
            box = union(box, bbox(im["pens"], lambda v: v != 0))
    for x, y in zip(ia["imgs"], ib["imgs"]):
        if x["pens"] is None:
            continue
        want = cut(x["pens"], box) if box else x["pens"]
        if y["pens"] != want or (y["w"], y["h"]) != (len(want[0]), len(want)):
            problems.append("a classic image is not the original's art in its box")
        for k in ("left", "top", "depth", "pick", "onoff"):
            if x[k] != y[k]:
                problems.append("a classic image's %s changed" % k)
    if bool(ia["glow"]) != bool(ib["glow"]):
        problems.append("the glow part came or went")
    elif ia["glow"]:
        ga, _ = glow_images(ia["glow"])
        gb, why = glow_images(ib["glow"])
        if ga is None:
            if ia["glow"] != ib["glow"]:
                problems.append("a glow part this tool cannot read was changed")
        elif gb is None:
            problems.append("the new glow part does not decode: %s" % why)
        else:
            gbox, _ = glow_box(ga[1])
            if gb[0][2:] != ga[0][2:]:
                problems.append("FACE's flags or aspect changed")
            for x, y in zip(ga[1], gb[1]):
                want = cut(x["rows"], gbox) if gbox else x["rows"]
                if y["rows"] != want:
                    problems.append("a glow image is not the original's pixels in its box")
                for k in ("tr", "ncol", "flags", "icomp", "pcomp", "depth", "pal"):
                    if x[k] != y[k]:
                        problems.append("a glow image's %s changed" % k)
    return problems


def main(argv):
    if len(argv) >= 4 and argv[1] == "--check":
        p = check(open(argv[2], "rb").read(), open(argv[3], "rb").read())
        for x in p:
            print("PROBLEM " + x)
        return 1 if p else 0
    data = open(argv[1], "rb").read()
    out, rep = crop(data, "--classic-only" in argv)
    open(argv[2], "wb").write(out)
    print(argv[1], rep)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
