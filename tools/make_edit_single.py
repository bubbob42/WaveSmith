#!/usr/bin/env python3
"""make_edit_single.py -- a 7-inch single's side as WaveSmith gets it,
for its test (test/auto/EditTrim), and the checker of what a save made.

    python3 tools/make_edit_single.py OUTDIR
    python3 tools/make_edit_single.py --check OUTDIR [NAME [--recut] [--trace TRACE]]

OUTDIR/Single.wav: 44.1 kHz stereo 16-bit PCM, 40 s, the concept's single
(Concept-WaveSmith-2026-10.md) in short:

  0.00 s  lead-in: groove noise (-60 dB rms, as make_vinyl_side.py's), the needle's thump at 0.38 s,
          six clicks (0.95, 1.42, 1.77, 2.43, 2.87, 3.12 s)
  4.10 s  music, three chords (-10 dB), a fade-out from 29 s
  34.0 s  run-out: groove noise, the groove's click every 1.8 s
  40.0 s  the end

and after the samples a LIST INFO chunk (INAM "Single A"): a save must
keep it. Beside it OUTDIR/Single.prl, a PrismaRec level map (PRL1, 40 ms
looks, a recognisable byte a look): a save must cut it as it cut the
take. Seeded: the same files each time.

--check reads OUTDIR/Trim.wav (a copy of Single.wav WaveSmith trimmed
and saved; OUTDIR/NAME.wav with --check OUTDIR NAME) and says whether
  - it is a RIFF WAVE with its sizes true, Single.wav's format
  - its samples are Single.wav's from some frame on, byte for byte (cut
    at frames, nothing changed), and how many
  - In lies in the lead-in's last second or the music's first (3.0 to
    4.6 s: dragged from the suggested 3.6 s) and Out after the fade
    (34.0 to 37.0 s: 2 s after the music, suggested)
  - the LIST chunk is still there after the samples
  - Trim.prl (when Single.prl was copied beside Trim.wav) holds Single.prl's
    header and its looks from In's to the last kept frame's, byte for byte

--trace TRACE (WaveSmith's trace of the run): the saves replayed as the
trace has them -- before each, the timeline's pieces ("saveplan", then
"piece from frames" each), after it "saved in out err" -- version after version from
Single.wav: the file must hold the timeline's frames In .. Out - 1, each
piece's frames of the version before (silence zeros), and the map the
looks mapcut.c makes along it (each new look the old look its first
frame came from, a silence's 255). Cuts in the take or not, exact: the
replay follows the old map's grid as WaveSmith does (--recut not needed).
"""
import array
import math
import os
import random
import struct
import sys

RATE = 44100
LENGTH = 40.0
CLICKS = (0.95, 1.42, 1.77, 2.43, 2.87, 3.12)
MUSIC = (4.10, 34.0)
FADE = 29.0
RUNOUT_CLICKS = (34.6, 36.4, 38.2, 39.95)
LIST = b'INFO' + b'INAM' + struct.pack('<I', 9) + b'Single A\x00' + b'\x00'
LOOK_US = 40000
LOOKS = int(LENGTH * 1000000 / LOOK_US)
MAP = b'PRL1' + struct.pack('>I', LOOK_US) + bytes((i * 7) % 90 for i in range(LOOKS))


def look_of(frame):
    """the look a frame lies in, as WaveSmith's mapcut.c works it out"""
    ms = frame * 1000 // RATE
    return ms * 1000 // LOOK_US


def single():
    rnd = random.Random(1981)
    n = int(LENGTH * RATE)
    data = array.array('h', [0]) * (2 * n)
    noise = 32767 * 10 ** (-60 / 20)
    chords = ((220.0, 277.2, 329.6), (196.0, 246.9, 293.7), (174.6, 220.0, 261.6))
    for i in range(n):
        t = i / RATE
        for c in (0, 1):
            v = rnd.gauss(0, noise)
            if MUSIC[0] <= t < MUSIC[1]:
                env = min(1.0, (t - MUSIC[0]) / 0.3)
                if t > FADE:
                    env *= max(0.0, (MUSIC[1] - t) / (MUSIC[1] - FADE))
                ch = chords[int((t - MUSIC[0]) / 2.0) % 3]
                s = sum(math.sin(2 * math.pi * f * (1.0 + 0.002 * c) * t) for f in ch) / 3
                v += 32767 * 10 ** (-10 / 20) * env * s * (0.8 + 0.2 * math.sin(2 * math.pi * 0.5 * t))
            data[2 * i + c] = max(-32768, min(32767, int(v)))
    # the needle's thump: 30 Hz, dying in 60 ms
    for k in range(int(0.15 * RATE)):
        i = int(0.38 * RATE) + k
        v = 0.5 * 32767 * math.exp(-k / (0.06 * RATE)) * math.sin(2 * math.pi * 30 * k / RATE)
        for c in (0, 1):
            data[2 * i + c] = max(-32768, min(32767, data[2 * i + c] + int(v)))
    # the clicks: a millisecond each
    for j, t in enumerate(CLICKS + RUNOUT_CLICKS):
        sign = 1 if j % 2 == 0 else -1
        for k in range(44):
            i = int(t * RATE) + k
            if i >= n:
                break
            v = sign * 0.35 * 32767 * (1 - k / 44)
            for c in (0, 1):
                data[2 * i + c] = max(-32768, min(32767, data[2 * i + c] + int(v)))
    return data


def write(path, data):
    if sys.byteorder != 'little':
        data = array.array('h', data)
        data.byteswap()
    raw = data.tobytes()
    fmt = struct.pack('<HHIIHH', 1, 2, RATE, RATE * 4, 4, 16)
    body = (b'WAVE' + b'fmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(raw)) + raw +
            (b'\x00' if len(raw) & 1 else b'') + b'LIST' + struct.pack('<I', len(LIST)) + LIST)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', len(body)) + body)


def chunks(path):
    with open(path, 'rb') as f:
        b = f.read()
    ok = b[:4] == b'RIFF' and b[8:12] == b'WAVE' and struct.unpack('<I', b[4:8])[0] == len(b) - 8
    out, at = {}, 12
    while at + 8 <= len(b):
        cid = b[at:at + 4]
        size = struct.unpack('<I', b[at + 4:at + 8])[0]
        out[cid] = (at + 8, size)
        at += 8 + size + (size & 1)
    return b, ok and at == len(b), out


def recut_ok(got, first, end):
    """the map of a take cut twice: Single.prl's header and its looks from
    first or a look either side to end or a look either side"""
    for f in (first - 1, first, first + 1):
        for e in (end - 1, end, end + 1):
            if 0 <= f < e <= LOOKS and got == MAP[:8] + MAP[8 + f:8 + e]:
                return True
    return False


SILENCE = 0xFFFFFFFF


def ms_of(frames):
    """WavePlan_FramesToMs"""
    return frames // RATE * 1000 + frames % RATE * 1000 // RATE


def frames_of(ms):
    """WavePlan_MsToFrames"""
    return ms // 1000 * RATE + ms % 1000 * RATE // 1000


def look_start(k, micros=LOOK_US):
    """mapcut.c's look_start: a file's look k's first frame"""
    ms = k * (micros // 1000) + k // 1000 * (micros % 1000) + k % 1000 * (micros % 1000) // 1000
    return frames_of(ms)


def timeline_of(pending, cur):
    """a timeline's frames (Single.wav's each, -1 silence) from its pieces
    over the version cur"""
    tl = array.array('l')
    for pc in pending:
        frm, n = pc.frm, pc.n
        if frm == SILENCE:
            tl.extend([-1] * n)
        else:
            tl.extend(cur[frm:frm + n])
    return tl


def map_cut(cmap, pending, cur, span, lv=None):
    """the map a save of the timeline's span (i, o) leaves, as mapcut.c
    cuts it: the whole file's looks In's to the last frame's; with cuts
    or a level, each new look the old look its first frame came from (255
    silence), its dB moved by the level at its middle"""
    i, o = span
    if len(pending) == 1 and pending[0].frm == 0 and pending[0].n == len(cur) and pending[0].plain() and lv is None:
        first, end = look_of(i), min(look_of(o - 1) + 1, len(cmap))
        return cmap[first:end] if first < end else b''
    src = array.array('l')
    for pc in pending:
        src.extend([-1] * pc.n if pc.frm == SILENCE else range(pc.frm, pc.frm + pc.n))
    half = frames_of(LOOK_US // 2000)
    out = bytearray()
    for k in range(look_of(o - 1 - i) + 1):
        t = i + look_start(k)
        f = src[t] if t < len(src) else -1
        lk = look_of(f) if f >= 0 else None
        b = cmap[lk] if lk is not None and lk < len(cmap) else 255
        out.append(leveled(b, level_at(pending, lv, t + half)))
    return bytes(out)


# ---- the level (concept 3.2): envelope.c's and timeline.c's sums, the
# same integers, so that a save at a level is replayed sample for sample

UNITY, ENV_MAX, U_MAX, BLOCK = 4096, 32767, 65535, 16
FADE_IN, FADE_OUT, FADE_DIR, FADE_SHAPE = 0x10, 0x20, 0x30, 0x0F
SHAPE_LINEAR, SHAPE_EVEN, SHAPE_SCURVE = 0, 1, 2
SCURVE = [0, 0, 1, 1, 2, 4, 6, 8, 10, 12, 15, 19, 22, 26, 30, 35, 39, 44, 50, 55, 61, 68, 74, 81, 88, 96, 103, 111, 120, 128, 137, 146, 156, 166, 176, 186, 197, 208, 219, 230, 242, 254, 266, 279, 291, 304, 318, 331, 345, 359, 374, 388, 403, 418, 433, 449, 465, 481, 497, 514, 531, 548, 565, 582, 600, 618, 636, 654, 673, 691, 710, 729, 749, 768, 788, 808, 828, 848, 869, 889, 910, 931, 952, 974, 995, 1017, 1039, 1060, 1083, 1105, 1127, 1150, 1172, 1195, 1218, 1241, 1264, 1288, 1311, 1334, 1358, 1382, 1406, 1429, 1453, 1478, 1502, 1526, 1550, 1575, 1599, 1624, 1648, 1673, 1698, 1723, 1747, 1772, 1797, 1822, 1847, 1872, 1897, 1922, 1948, 1973, 1998, 2023, 2048, 2073, 2098, 2123, 2148, 2174, 2199, 2224, 2249, 2274, 2299, 2324, 2349, 2373, 2398, 2423, 2448, 2472, 2497, 2521, 2546, 2570, 2594, 2618, 2643, 2667, 2690, 2714, 2738, 2762, 2785, 2808, 2832, 2855, 2878, 2901, 2924, 2946, 2969, 2991, 3013, 3036, 3057, 3079, 3101, 3122, 3144, 3165, 3186, 3207, 3227, 3248, 3268, 3288, 3308, 3328, 3347, 3367, 3386, 3405, 3423, 3442, 3460, 3478, 3496, 3514, 3531, 3548, 3565, 3582, 3599, 3615, 3631, 3647, 3663, 3678, 3693, 3708, 3722, 3737, 3751, 3765, 3778, 3792, 3805, 3817, 3830, 3842, 3854, 3866, 3877, 3888, 3899, 3910, 3920, 3930, 3940, 3950, 3959, 3968, 3976, 3985, 3993, 4000, 4008, 4015, 4022, 4028, 4035, 4041, 4046, 4052, 4057, 4061, 4066, 4070, 4074, 4077, 4081, 4084, 4086, 4088, 4090, 4092, 4094, 4095, 4095, 4096, 4096]
WHOLE_DB = [258, 290, 325, 365, 410, 460, 516, 579, 649, 728, 817, 917, 1029, 1154, 1295, 1453, 1631, 1830, 2053, 2303, 2584, 2900, 3254, 3651, 4096, 4596, 5157, 5786, 6492, 7284, 8173, 9170, 10289, 11544, 12953, 14533, 16306, 18296, 20529, 23034, 25844, 28997, 32536]
TENTH_UP = [32768, 33147, 33531, 33920, 34312, 34710, 35112, 35518, 35929, 36345]


def env_curve(shape, u):
    """Env_Curve: 0 at u 0, UNITY at U_MAX"""
    if shape == SHAPE_LINEAR:
        return (u * UNITY + U_MAX // 2) // U_MAX
    if shape == SHAPE_SCURVE:
        i, f = u >> 8, u & 255
        return UNITY if i >= 256 else SCURVE[i] + (((SCURVE[i + 1] - SCURVE[i]) * f + 128) >> 8)
    return UNITY if u == U_MAX else (((u * u) >> 1) + (1 << 18)) >> 19


def env_from_tenths(t):
    """Env_FromTenths"""
    t = max(-240, min(180, t))
    d = t // 10 if t >= 0 else -((-t + 9) // 10)
    r = t - d * 10
    return min(ENV_MAX, (WHOLE_DB[d + 24] * TENTH_UP[r] + 16384) >> 15)


def env_tenths(level):
    """Env_Tenths: the nearest tenths of a dB, -241 for nothing"""
    if not level:
        return -241
    lo, hi = -240, 180
    if level <= env_from_tenths(lo):
        return lo
    if level >= env_from_tenths(hi):
        return hi
    while hi - lo > 1:
        mid = int((lo + hi) / 2)
        if env_from_tenths(mid) <= level:
            lo = mid
        else:
            hi = mid
    return lo if level - env_from_tenths(lo) <= env_from_tenths(hi) - level else hi


def env_mul(a, b):
    return min(ENV_MAX, (a * b + 2048) >> 12)


def place(a, c):
    """timeline.c's place(): a of c as 0 .. U_MAX"""
    if not c:
        return 0
    while c > 65535:
        a >>= 1
        c >>= 1
    return min(a, c) * U_MAX // c


def between(u0, u1, f):
    return u0 + (u1 - u0) * f // U_MAX if u1 >= u0 else u0 - (u0 - u1) * f // U_MAX


class Piece:
    def __init__(self, frm, n, gain=UNITY, fade=0, u0=0, u1=0):
        self.frm, self.n, self.gain, self.fade, self.u0, self.u1 = frm, n, gain, fade, u0, u1

    def plain(self):
        return self.gain == UNITY and not (self.fade & FADE_DIR)


class Levels:
    """struct TlLevels, None when it holds nothing (Tl_Levels)"""
    def __init__(self, w):
        self.inn, self.out, self.fin, self.fout, self.dip, self.shape = w

    @staticmethod
    def of(w):
        return Levels(w) if w and (w[2] or w[3] or w[4]) else None


def cut_before(pieces, i):
    if not i or i >= len(pieces):
        return False
    q, p = pieces[i - 1], pieces[i]
    if q.frm == SILENCE or p.frm == SILENCE:
        return q.frm != p.frm
    return q.frm + q.n != p.frm


def level(pieces, k, start, lv, t):
    """timeline.c's level(): piece k (starting at start) at frame t"""
    p, off = pieces[k], t - start
    g = p.gain
    if p.fade & FADE_DIR:
        u = between(p.u0, p.u1, place(off, p.n))
        if (p.fade & FADE_DIR) == FADE_OUT:
            u = U_MAX - u
        g = env_mul(g, env_curve(p.fade & FADE_SHAPE, u))
    if lv is None:
        return g
    if lv.fin and t >= lv.inn and t - lv.inn < lv.fin:
        g = env_mul(g, env_curve(lv.shape, place(t - lv.inn, lv.fin)))
    if lv.fout and t < lv.out and lv.out - t <= lv.fout:
        g = env_mul(g, env_curve(lv.shape, place(lv.out - t, lv.fout)))
    if lv.dip:
        if cut_before(pieces, k) and off < lv.dip:
            g = env_mul(g, env_curve(SHAPE_SCURVE, place(off, lv.dip)))
        d = p.n - off
        if cut_before(pieces, k + 1) and d <= lv.dip:
            g = env_mul(g, env_curve(SHAPE_SCURVE, place(d, lv.dip)))
    return g


def zone_edges(pieces, k, start, lv):
    if lv is None:
        return []
    end = start + pieces[k].n
    e = [lv.inn, lv.inn + lv.fin, lv.out - lv.fout if lv.out > lv.fout else 0, lv.out]
    if lv.dip:
        e += [start + lv.dip, end - lv.dip if end > lv.dip else 0]
    return e


def untouched(pieces, k, start, lv):
    """the whole piece as recorded (Tl_Read asks it of each piece's run)"""
    p = pieces[k]
    if not p.plain():
        return False
    if lv is None:
        return True
    first, end = start, start + p.n
    if lv.fin and first < lv.inn + lv.fin and end > lv.inn:
        return False
    if lv.fout and end > lv.out - min(lv.fout, lv.out) and first < lv.out:
        return False
    if lv.dip and ((cut_before(pieces, k) and 0 < lv.dip) or (cut_before(pieces, k + 1) and 0 < lv.dip)):
        return False
    return True


def render(pieces, cur, lv):
    """the timeline's samples (16-bit stereo, little-endian) over the
    version cur, each piece at its level as Tl_Read sets it: stretches cut
    at every block (16 frames from the timeline's start) and fade or dip
    edge, each at the level of its middle frame"""
    out = bytearray()
    start = 0
    for k, p in enumerate(pieces):
        if p.frm == SILENCE:
            out.extend(bytes(p.n * 4))
        elif untouched(pieces, k, start, lv):
            out.extend(cur[p.frm * 4:(p.frm + p.n) * 4])
        else:
            data = array.array('h', cur[p.frm * 4:(p.frm + p.n) * 4])
            if sys.byteorder != 'little':
                data.byteswap()
            edges = zone_edges(pieces, k, start, lv)
            x, end = start, start + p.n
            while x < end:
                a = x & ~(BLOCK - 1)
                a = max(a, start)
                for c in edges:
                    if a < c <= x:
                        a = c
                b = (a | (BLOCK - 1)) + 1
                for c in edges:
                    if a < c < b:
                        b = c
                b = min(b, end)
                g = level(pieces, k, start, lv, a + (b - a) // 2)
                if g != UNITY:
                    for i in range((x - start) * 2, (b - start) * 2):
                        v = (data[i] * g + 2048) >> 12
                        data[i] = 32767 if v > 32767 else -32768 if v < -32768 else v
                x = b
            if sys.byteorder != 'little':
                data.byteswap()
            out.extend(data.tobytes())
        start += p.n
    return bytes(out)


def level_at(pieces, lv, t):
    """Tl_LevelAt: the level frame t is at, exactly (UNITY outside)"""
    start = 0
    for k, p in enumerate(pieces):
        if t < start + p.n:
            return UNITY if p.frm == SILENCE else level(pieces, k, start, lv, t)
        start += p.n
    return UNITY


def leveled(look, lvl):
    """mapcut.c's leveled(): a look's dB moved by its level"""
    if look == 255 or lvl == UNITY:
        return look
    if not lvl:
        return 255
    t = env_tenths(lvl)
    db = look - int((t + (5 if t >= 0 else -5)) / 10)
    return max(0, min(90, db))


def replay(trace, frames, looks, name=None, src=None):
    """the saves of a trace replayed: name's last version's frames
    (Single.wav's frame each, -1 silence), its map's looks, and the last
    split's two parts (frames, looks) made from it; None when the trace has
    no pieces (an older WaveSmith) or the file was never written. name is
    the file opened -- unless a Save as wrote it ("savedasname NAME.wav",
    then "savedas in out err"): the doc is the new file from then on, and
    the saves after it go there"""
    # each file: its frames (Single.wav's each), its map, the last split,
    # the saves, the last tracks, its samples (a level makes them its own)
    files = {None: [array.array('l', range(frames)), bytes(looks), None, 0, None, src]}
    doc, newname, pending, parts, lv = None, None, None, [], None
    for line in open(trace, encoding='latin-1'):
        w = line.split()
        cur, cmap, samp = files[doc][0], files[doc][1], files[doc][5]
        if line.startswith('saveplan pieces frames'):
            pending, lv = [], None
        elif line.startswith('piece from frames') and pending is not None:
            if len(w) > 3 and w[3] == 'gain':
                pending.append(Piece(*[int(x) for x in w[7:13]]))
            else:
                pending.append(Piece(int(w[3]), int(w[4])))
        elif line.startswith('savelevels in out fadein fadeout dip shape') and pending is not None:
            lv = Levels.of([int(x) for x in w[7:13]])
        elif line.startswith('split in at out err'):
            i, at, o, e = int(w[5]), int(w[6]), int(w[7]), int(w[8])
            if e == 0 and pending is not None:
                tl, rs = timeline_of(pending, cur), render(pending, samp, lv)
                files[doc][2] = ((tl[i:at], map_cut(cmap, pending, cur, (i, at), lv), rs[i * 4:at * 4]),
                                 (tl[at:o], map_cut(cmap, pending, cur, (at, o), lv), rs[at * 4:o * 4]))
                files[doc][3] += 1
            pending = None
        elif line.startswith('saved in out err'):
            i, o, e = int(w[4]), int(w[5]), int(w[6])
            if e == 0 and pending is not None:
                tl, rs = timeline_of(pending, cur), render(pending, samp, lv)
                files[doc][1] = map_cut(cmap, pending, cur, (i, o), lv)
                files[doc][0] = tl[i:o]
                files[doc][5] = rs[i * 4:o * 4]
                files[doc][3] += 1
            pending = None
        elif line.startswith('trackpart n first end') and pending is not None:
            a, b = int(w[5]), int(w[6])
            tl, rs = timeline_of(pending, cur), render(pending, samp, lv)
            parts.append((tl[a:b], map_cut(cmap, pending, cur, (a, b), lv), rs[a * 4:b * 4]))
        elif line.startswith('splitmarks parts err'):
            if int(w[4]) == 0 and pending is not None and len(parts) == int(w[3]):
                files[doc][4] = parts
                files[doc][3] += 1
            pending, parts = None, []
        elif line.startswith('savedasname '):
            newname = w[1][:-4] if w[1].lower().endswith('.wav') else w[1]
        elif line.startswith('savedas in out err'):
            i, o, e = int(w[4]), int(w[5]), int(w[6])
            if e == 0 and pending is not None and newname:
                tl, rs = timeline_of(pending, cur), render(pending, samp, lv)
                files[newname] = [tl[i:o], map_cut(cmap, pending, cur, (i, o), lv), None, 1, None, rs[i * 4:o * 4]]
                doc = newname
            pending = None
    f = files[name] if name in files else files[None]
    return (f[0], f[1], f[2], f[4], f[5]) if f[3] else None


def check(outdir, name='Trim', recut=False, trace=None):
    """recut: the take was trimmed and saved more than once -- its map is
    then cut on the first cut's look grid, which the PRL1 header cannot
    place within the new file: one look more or less at either end"""
    w, m = name + '.wav', name + '.prl'
    orig, ok0, oc = chunks(os.path.join(outdir, 'Single.wav'))
    trim, ok1, tc = chunks(os.path.join(outdir, w))
    fails = []
    if not ok0:
        fails.append('Single.wav is not a whole RIFF WAVE (the generator?)')
    if not ok1:
        fails.append(w + ': the RIFF size or its chunks are not true')
    if b'fmt ' not in tc or b'data' not in tc:
        fails.append(w + ': no fmt or no data chunk')
        print('\n'.join('FAIL ' + f for f in fails))
        return 1
    if trim[tc[b'fmt '][0]:tc[b'fmt '][0] + 16] != orig[oc[b'fmt '][0]:oc[b'fmt '][0] + 16]:
        fails.append(w + "'s format is not Single.wav's")
    d0, n0 = oc[b'data']
    d1, n1 = tc[b'data']
    samples = trim[d1:d1 + n1]
    src = orig[d0:d0 + n0]
    played = replay(trace, n0 // 4, MAP[8:], name, src) if trace else None
    if played:
        return check_replayed(w, m, outdir, (trim, tc, samples, src, played, fails))
    # where the kept samples start: the first 64 bytes found on a frame
    at = -1
    pos = src.find(samples[:64])
    while pos >= 0:
        if pos % 4 == 0 and src[pos:pos + n1] == samples:
            at = pos
            break
        pos = src.find(samples[:64], pos + 1)
    if at < 0:
        fails.append(w + "'s samples are not a stretch of Single.wav's, byte for byte")
    else:
        t_in = at / 4 / RATE
        t_out = (at + n1) / 4 / RATE
        print('kept %d frames: In %.3f s, Out %.3f s' % (n1 // 4, t_in, t_out))
        if not 3.0 <= t_in <= 4.6:
            fails.append('In at %.3f s: not in 3.0 .. 4.6 s' % t_in)
        if not 34.0 <= t_out <= 37.0:
            fails.append('Out at %.3f s: not in 34.0 .. 37.0 s' % t_out)
    if b'LIST' not in tc or trim[tc[b'LIST'][0]:tc[b'LIST'][0] + tc[b'LIST'][1]] != LIST:
        fails.append('the LIST chunk after the samples is gone or changed')
    tprl = os.path.join(outdir, m)
    if at >= 0 and os.path.exists(os.path.join(outdir, 'Single.prl')):
        first, end = look_of(at // 4), min(look_of(at // 4 + n1 // 4 - 1) + 1, LOOKS)
        want = MAP[:8] + MAP[8 + first:8 + end]
        got = open(tprl, 'rb').read() if os.path.exists(tprl) else None
        if got is None:
            fails.append(m + ': no level map beside the trimmed take')
        elif got != want and not (recut and recut_ok(got, first, end)):
            fails.append(m + ': not the looks %d .. %d of Single.prl (%d bytes, %d wanted)' %
                         (first, end - 1, len(got), len(want)))
        else:
            print('the level map cut: looks %d .. %d%s' % (first, end - 1, '' if got == want else ', a look off (cut again)'))
    for f in fails:
        print('FAIL ' + f)
    print('check_edit_single: %s' % ('ok' if not fails else '%d failure(s)' % len(fails)))
    return 1 if fails else 0


def check_part(outdir, name, st):
    """a split's part, NAME.wav and NAME.prl, against the replay"""
    want, frames, looks, fails = st
    path = os.path.join(outdir, name + '.wav')
    if not os.path.exists(path):
        fails.append(name + '.wav: not written')
        return
    b, ok, ch = chunks(path)
    if not ok or b'data' not in ch:
        fails.append(name + '.wav: the RIFF size or its chunks are not true')
        return
    d, n = ch[b'data']
    if b[d:d + n] != bytes(want):
        fails.append(name + ".wav's samples are not the timeline's part (%d bytes, %d wanted)" % (n, len(want)))
    elif b'LIST' not in ch or b[ch[b'LIST'][0]:ch[b'LIST'][0] + ch[b'LIST'][1]] != LIST:
        fails.append(name + '.wav: the LIST chunk is gone or changed')
    else:
        print('%s: %d frames, %d of silence, as replayed' % (name + '.wav', len(frames), sum(1 for f in frames if f < 0)))
    prl = os.path.join(outdir, name + '.prl')
    if os.path.exists(os.path.join(outdir, 'Single.prl')):
        got = open(prl, 'rb').read() if os.path.exists(prl) else None
        if got != MAP[:8] + looks:
            fails.append(name + '.prl: not the looks replayed (%s bytes, %d wanted)' % (len(got) if got else 'no', 8 + len(looks)))


def check_replayed(w, m, outdir, st):
    """--check with --trace: the file and its map against the replay"""
    trim, tc, samples, src, (cur, cmap, split, tracks, want), fails = st
    if split:
        for n, (frames, looks, part) in enumerate(split):
            check_part(outdir, '%s-%d' % (w[:-4], n + 1), (part, frames, looks, fails))
    if tracks:
        for n, (frames, looks, part) in enumerate(tracks):
            check_part(outdir, '%s-%02d' % (w[:-4], n + 1), (part, frames, looks, fails))
    gaps = sum(1 for f in cur if f < 0)
    pieces = sum(1 for k in range(len(cur)) if k == 0 or (cur[k] < 0) != (cur[k - 1] < 0) or
                 (cur[k] >= 0 and cur[k] != cur[k - 1] + 1))
    kept = [f for f in (cur[0], cur[-1]) if f >= 0] if len(cur) else []
    print('kept %d frames in %d piece(s), %d of silence%s' % (
        len(cur), pieces, gaps,
        ': In %.3f s, Out %.3f s' % (kept[0] / RATE, (kept[-1] + 1) / RATE) if len(kept) == 2 else ''))
    if bytes(want) != samples:
        diff = next((k for k in range(min(len(want), len(samples)) // 4) if want[k * 4:k * 4 + 4] != samples[k * 4:k * 4 + 4]),
                    None)
        fails.append(w + "'s samples are not the timeline's (%d bytes, %d wanted; first difference at frame %s)" % (
            len(samples), len(want), diff))
    elif len(kept) == 2:
        if not 3.0 <= kept[0] / RATE <= 4.6:
            fails.append('In at %.3f s: not in 3.0 .. 4.6 s' % (kept[0] / RATE))
        if not 34.0 <= (kept[-1] + 1) / RATE <= 37.0:
            fails.append('Out at %.3f s: not in 34.0 .. 37.0 s' % ((kept[-1] + 1) / RATE))
    if b'LIST' not in tc or trim[tc[b'LIST'][0]:tc[b'LIST'][0] + tc[b'LIST'][1]] != LIST:
        fails.append('the LIST chunk after the samples is gone or changed')
    tprl = os.path.join(outdir, m)
    if os.path.exists(os.path.join(outdir, 'Single.prl')):
        got = open(tprl, 'rb').read() if os.path.exists(tprl) else None
        if got is None:
            fails.append(m + ': no level map beside the take')
        elif got != MAP[:8] + cmap:
            fails.append(m + ': not the looks replayed (%d bytes, %d wanted)' % (len(got), 8 + len(cmap)))
        else:
            print('the level map: %d looks as replayed, %d of silence' % (len(cmap), cmap.count(255)))
    for f in fails:
        print('FAIL ' + f)
    print('check_edit_single: %s' % ('ok' if not fails else '%d failure(s)' % len(fails)))
    return 1 if fails else 0


def main(argv):
    if len(argv) >= 3 and argv[1] == '--check':
        trace = argv[argv.index('--trace') + 1] if '--trace' in argv[:-1] else None
        rest = [a for a in argv[3:] if a != '--trace' and a != trace]
        return check(argv[2], rest[0] if rest and not rest[0].startswith('--') else 'Trim', '--recut' in rest, trace)
    if len(argv) != 2:
        print(__doc__)
        return 2
    os.makedirs(argv[1], exist_ok=True)
    write(os.path.join(argv[1], 'Single.wav'), single())
    with open(os.path.join(argv[1], 'Single.prl'), 'wb') as f:
        f.write(MAP)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
