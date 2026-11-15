#!/usr/bin/env python3
"""check_normpeak.py WAV TRACE TARGET_TENTHS -- WaveSmith's Normalise
(concept 3.2, F7) held to its promise in a saved file: the part the
trace's last "normalise" line names, in the saved WAV (its frames moved by
the save's In), peaks on its louder channel at or under the target and
within 0.1 dB of it. TARGET_TENTHS: -20 for -2.0 dB (test/auto/EditGain).

    python3 tools/check_normpeak.py dist/Test/Edit/Gain.wav dist/Test/gain.trace -20
"""
import sys, struct, math

wav, trace, target = sys.argv[1], sys.argv[2], int(sys.argv[3])
norm = saved = None
for line in open(trace, encoding='latin-1'):
    w = line.split()
    if w and w[0] == 'normalise': norm = [int(x) for x in w[-4:]]
    if w and w[0] == 'saved': saved = [int(x) for x in w[-3:]]
data = open(wav, 'rb').read()
pos, fmt, samples = 12, None, None
while pos + 8 <= len(data):
    cid, size = data[pos:pos+4], struct.unpack('<I', data[pos+4:pos+8])[0]
    if cid == b'fmt ': fmt = struct.unpack('<HHIIHH', data[pos+8:pos+24])
    if cid == b'data': samples = data[pos+8:pos+8+size]; break
    pos += 8 + size + (size & 1)
ch = fmt[1]
first, end, peak_before, tenths = norm
si = saved[0]
a, b = (first - si) * ch * 2, (end - si) * ch * 2
part = struct.unpack('<%dh' % ((b - a) // 2), samples[a:b])
peak = max(abs(x) for x in part)
lim = 32767 * 10 ** (target / 200.0)
low = 32767 * 10 ** ((target - 1) / 200.0)
db = 20 * math.log10(peak / 32767.0)
print('part %d..%d (file %d..%d): peak %d (%.2f dB), before %d, gain %+.1f dB; target %.1f dB = %.0f'
      % (first, end, first - si, end - si, peak, db, peak_before, (tenths - 1000) / 10.0, target / 10.0, lim))
ok = low <= peak <= lim + 0.5
print('ok' if ok else 'FAILED: not within 0.1 dB under the target')
sys.exit(0 if ok else 1)
