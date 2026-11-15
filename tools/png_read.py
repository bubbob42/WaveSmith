"""png_read.py -- minimal pure-stdlib PNG reader (RGBA/RGB/palette, 8-bit).

Just enough to decode the small art assets this project embeds (About-box
picture, icon previews): parses IHDR/PLTE/tRNS/IDAT/IEND, concatenates and
zlib-inflates the IDAT stream, and reverses the PNG per-scanline filters
(None/Sub/Up/Average/Paeth). No interlacing support (Adam7) -- none of our
source art uses it, and every producer (sips, browsers, image editors)
defaults to non-interlaced.

No third-party modules, matching the rest of tools/ (see amiga_icon.py).

Usage:
    from png_read import read_png
    w, h, pixels = read_png("art/foo.png")   # pixels[y][x] = (r, g, b, a)
"""

import struct
import zlib


def _paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter(raw, width, height, bpp):
    """raw: filter-byte + bytes-per-scanline, repeated `height` times."""
    stride = width * bpp
    out = bytearray(stride * height)
    pos = 0
    prev = bytearray(stride)
    for y in range(height):
        ftype = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ftype == 0:
            pass
        elif ftype == 1:   # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:   # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:   # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
        elif ftype == 4:   # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(a, b, c)) & 0xFF
        else:
            raise ValueError("unknown PNG filter type %d" % ftype)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return bytes(out)


def read_png(path):
    """Returns (width, height, pixels) -- pixels[y][x] = (r, g, b, a) ints."""
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s: not a PNG file" % path)

    pos = 8
    idat = bytearray()
    width = height = bitdepth = colortype = None
    palette = None
    trns = None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length   # + 4-byte CRC

        if ctype == b"IHDR":
            (width, height, bitdepth, colortype,
             comp, filt, interlace) = struct.unpack(">IIBBBBB", body)
            if comp != 0 or filt != 0:
                raise ValueError("%s: unsupported PNG compression" % path)
            if interlace != 0:
                raise ValueError("%s: interlaced PNG not supported" % path)
            if bitdepth != 8:
                raise ValueError("%s: only 8-bit PNG supported" % path)
        elif ctype == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif ctype == b"tRNS":
            trns = body
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break

    if width is None:
        raise ValueError("%s: no IHDR chunk" % path)

    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colortype]
    bpp = channels   # 8-bit depth only, so bytes-per-pixel == channels
    raw = zlib.decompress(bytes(idat))
    unfiltered = _unfilter(raw, width, height, bpp)

    pixels = []
    for y in range(height):
        row = []
        base = y * width * bpp
        for x in range(width):
            off = base + x * bpp
            if colortype == 6:      # RGBA
                r, g, b, a = unfiltered[off:off + 4]
            elif colortype == 2:    # RGB
                r, g, b = unfiltered[off:off + 3]
                a = 255
            elif colortype == 3:    # palette (+ optional tRNS alpha)
                idx = unfiltered[off]
                r, g, b = palette[idx]
                a = trns[idx] if (trns is not None and idx < len(trns)) else 255
            elif colortype == 0:    # grey
                g0 = unfiltered[off]
                r = g = b = g0
                a = 255
            elif colortype == 4:    # grey + alpha
                g0, a = unfiltered[off:off + 2]
                r = g = b = g0
            else:
                raise ValueError("%s: unsupported PNG colour type %d"
                                 % (path, colortype))
            row.append((r, g, b, a))
        pixels.append(row)
    return width, height, pixels
