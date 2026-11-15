#!/usr/bin/env python3
"""make_vinyl_side.py -- a record side as a turntable delivers it, for
SplitTracks' tests (test/auto/SplitRun, and on the host through vamos),
and the checker of what SplitTracks made of it.

    python3 tools/make_vinyl_side.py OUTDIR [NAME] [--clean] [--no-lead-in]
    python3 tools/make_vinyl_side.py --check OUTDIR [NAME]

OUTDIR/side.wav: 44.1 kHz stereo 16-bit PCM, about 2 minutes:

  0.0 s   lead-in: the groove, the needle's thump at 1.0 s
  3.0 s   song 1, 35 s, fading in over 2 s from -45 dB
          a gap of 2.5 s
  40.5 s  song 2, 42 s, a pause of 1.0 s at -46 dB inside it (no gap:
          shorter than 1.5 s), fading out over 4 s into the gap
          a gap of 1.8 s
  84.3 s  song 3, 33 s
  117.3 s lead-out: the run-out groove, its click every 1.8 s, 6 s

and all through it: groove noise (-60 dB rms), crackle (some 30 clicks a
second, gaps too), a pop now and then, rumble (12 Hz, -40 dB), a warp's
sway (0.55 Hz, -35 dB) and a DC offset -- a worn record, its noise's
peaks at about -46 dB. --clean: a good one, the noise 18 dB down, half
the crackle, no rumble. --no-lead-in: the take starts with song 1 (a
recording begun late). NAME (side unless told): the file's name. Seeded:
the same file each time.

--check reads OUTDIR/NAME-NN.wav (three of them) and says whether
  - each is a RIFF WAVE with its sizes true, FROM's format
  - each song lies inside one track from where it is heard (above -45
    dB: a fade-in's start is under the record's noise), each track
    starts at most 1.2 s before that and after the song before it ended
  - there are no more tracks
  - the tracks joined are the side's samples from the first track's start
    on, byte for byte (cut at frames, nothing changed)
"""
import array
import math
import os
import random
import struct
import sys

RATE = 44100
SONGS = [  # start, length, fade-in, fade-out, pause (start, length) or None
    (3.0, 35.0, 2.0, 0.0, None),
    (40.5, 42.0, 0.0, 4.0, (20.0, 1.0)),
    (84.3, 33.0, 0.0, 0.0, None),
]
LENGTH = 117.3 + 6.0


def db(x):
    return 32767.0 * 10 ** (x / 20.0)


def side(clean, lead_in):
    rnd = random.Random(1975)
    noise_db, crackle, rumble = (-78, 15.0, 0.0) if clean else (-60, 30.0, 1.0)
    n = int(LENGTH * RATE)
    left = [0.0] * n
    right = [0.0] * n
    # the songs: chords of three tones and a bass, beat-shaped, -10 dB peaks
    for k, (start, length, fin, fout, pause) in enumerate(SONGS):
        s0 = int(start * RATE)
        s1 = int((start + length) * RATE)
        base = 220.0 * (1.5 ** k)
        freqs = (base, base * 1.25, base * 1.5, base / 4)
        for i in range(s0, s1):
            t = (i - s0) / RATE
            beat = 0.6 + 0.4 * abs(math.sin(math.pi * t * 2))
            env = 1.0
            if fin and t < fin:
                env = 10 ** ((-45 + 45 * t / fin) / 20)
            if fout and t > length - fout:
                env = 10 ** ((-50 * (t - (length - fout)) / fout) / 20)
            if pause and pause[0] <= t < pause[0] + pause[1]:
                env *= 10 ** (-36 / 20.0)
            v = sum(math.sin(2 * math.pi * f * t) for f in freqs) / 4.0
            a = db(-10) * beat * env * v
            left[i] += a
            right[i] += a * 0.8
    # the turntable: noise, rumble, warp, DC
    for i in range(n):
        t = i / RATE
        common = rumble * (db(-40) * math.sin(2 * math.pi * 12 * t) + db(-35) * math.sin(2 * math.pi * 0.55 * t)) + 180
        left[i] += common + rnd.gauss(0, db(noise_db))
        right[i] += common + rnd.gauss(0, db(noise_db))
    # crackle: some 30 clicks a second, three-sample spikes, either side
    i = 0
    while True:
        i += int(rnd.expovariate(crackle) * RATE) + 1
        if i + 3 >= n:
            break
        amp = rnd.uniform(1500, 18000) * rnd.choice((-1, 1))
        ch = left if rnd.random() < 0.5 else right
        ch[i] += amp
        ch[i + 1] -= amp * 0.6
        ch[i + 2] += amp * 0.25
    # pops: a bump of 5 ms now and then (a scratch)
    for t in (12.0, 38.6, 61.2, 83.1, 100.4):
        i0 = int(t * RATE)
        for j in range(int(0.005 * RATE)):
            v = 9000 * math.sin(math.pi * j / (0.005 * RATE)) * math.exp(-j / (0.002 * RATE))
            left[i0 + j] += v
            right[i0 + j] += v * 0.9
    # the needle's thump, the run-out groove's click
    i0 = int(1.0 * RATE)
    for j in range(int(0.03 * RATE)):
        v = 12000 * math.sin(2 * math.pi * 40 * j / RATE) * math.exp(-j / (0.008 * RATE))
        left[i0 + j] += v
        right[i0 + j] += v
    t = 117.3 + 0.9
    while t < LENGTH - 0.1:
        i0 = int(t * RATE)
        for j, v in enumerate((14000, -9000, 4000, -1500)):
            left[i0 + j] += v
            right[i0 + j] += v
        t += 1.8
    first = 0 if lead_in else int(3.0 * RATE)
    left = left[first:]
    right = right[first:]
    n -= first
    out = array.array("h", [0]) * (2 * n)
    for i in range(n):
        out[2 * i] = max(-32768, min(32767, int(round(left[i]))))
        out[2 * i + 1] = max(-32768, min(32767, int(round(right[i]))))
    if sys.byteorder == "big":
        out.byteswap()
    return out.tobytes()


def write(path, data):
    fmt = struct.pack("<HHIIHH", 1, 2, RATE, RATE * 4, 4, 16)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt +
                b"data" + struct.pack("<I", len(data)) + data)


def read(path):
    d = open(path, "rb").read()
    assert d[:4] == b"RIFF" and d[8:12] == b"WAVE", path + ": no RIFF WAVE"
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        if cid == b"fmt ":
            fmt = d[pos + 8:pos + 8 + 16]
        elif cid == b"data":
            data = d[pos + 8:pos + 8 + size]
            assert pos + 8 + size == len(d), path + ": the data size is not the file's"
            assert struct.unpack("<I", d[4:8])[0] == len(d) - 8, path + ": the RIFF size is not true"
            break
        pos += 8 + size + (size & 1)
    return fmt, data


def audible(start, fin):
    """where a song is heard: above -45 dB (a fade-in from -55 dB)"""
    return start + (fin * 10.0 / 45.0 if fin else 0.0)


def check(outdir, name):
    fails = 0
    tracks = len(SONGS)
    fmt0, src = read(os.path.join(outdir, name + ".wav"))
    shift = float(open(os.path.join(outdir, name + ".txt")).read().split()[0])
    starts = []
    joined = b""
    for t in range(1, tracks + 1):
        p = os.path.join(outdir, "%s-%02d.wav" % (name, t))
        if not os.path.exists(p):
            print("FAILED  no", os.path.basename(p))
            return 1
        fmt, data = read(p)
        if fmt != fmt0:
            print("FAILED  %s: not FROM's format" % os.path.basename(p))
            fails += 1
        starts.append(len(joined) // 4 / RATE)
        joined += data
    extra = os.path.join(outdir, "%s-%02d.wav" % (name, tracks + 1))
    if os.path.exists(extra):
        print("FAILED  more tracks than", tracks)
        fails += 1
    # where the first track starts in the side: find its first frames
    first = read(os.path.join(outdir, name + "-01.wav"))[1]
    off = src.find(first[:4096])
    if off < 0 or off % 4:
        print("FAILED  the first track is not the side's samples")
        return fails + 1
    if src[off:off + len(joined)] == joined:
        print("ok      the tracks joined: the side's samples from %.2f s, %d bytes, unchanged" % (off / 4 / RATE, len(joined)))
    else:
        print("FAILED  the tracks joined differ from the side's samples")
        fails += 1
    abs_starts = [shift + off / 4 / RATE + s for s in starts]
    ends = abs_starts[1:] + [shift + off / 4 / RATE + len(joined) / 4 / RATE]
    for k, (start, length, fin, fout, pause) in enumerate(SONGS):
        if k >= len(abs_starts):
            break
        a, b = abs_starts[k], ends[k]
        heard = audible(start, fin)
        inside = a <= heard + 0.001 and start + length <= b + 0.001
        lead = heard - a
        prev_end = SONGS[k - 1][0] + SONGS[k - 1][1] if k else 0.0
        good = inside and lead <= 1.2 and a >= prev_end - 0.001
        print("%s  track %d: %.2f - %.2f s, song %.1f - %.1f s (%.2f s before it)" %
              ("ok     " if good else "FAILED ", k + 1, a, b, start, start + length, lead))
        fails += 0 if good else 1
    return fails


def main(argv):
    words = [a for a in argv[1:] if not a.startswith("--")]
    if "--check" in argv:
        sys.exit(1 if check(words[0], words[1] if len(words) > 1 else "side") else 0)
    if not words:
        sys.exit(__doc__)
    name = words[1] if len(words) > 1 else "side"
    lead_in = "--no-lead-in" not in argv
    os.makedirs(words[0], exist_ok=True)
    write(os.path.join(words[0], name + ".wav"), side("--clean" in argv, lead_in))
    # the first line: where the take starts on the side, in seconds
    with open(os.path.join(words[0], name + ".txt"), "w") as f:
        f.write("%.1f\n" % (0.0 if lead_in else 3.0))
        for s in SONGS:
            f.write("%.1f %.1f\n" % (s[0], s[0] + s[1]))


if __name__ == "__main__":
    main(sys.argv)
