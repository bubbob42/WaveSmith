#!/usr/bin/env python3
"""check_level_mirror.py DRAWER -- the level's sums in Python
(make_edit_single.py's render(), level_at()) against timeline.c's: every
case test/host/wavesmith/level_mirror wrote, rendered the Python way and
compared with what Tl_Read made, byte for byte, and the exact levels.
make_edit_single.py checks WaveSmith's saves with these sums: they must
be the C's, or a right save reads as wrong."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_edit_single as m  # noqa: E402


def main(argv):
    d = argv[1]
    src = open(os.path.join(d, 'mirror.src'), 'rb').read()
    cases, fails = [], 0
    for line in open(os.path.join(d, 'mirror.txt')):
        w = line.split()
        if w[0] == 'case':
            cases.append({'n': int(w[1]), 'frames': int(w[3]), 'pieces': [], 'at': []})
        elif w[0] == 'piece':
            cases[-1]['pieces'].append(m.Piece(*[int(x) for x in w[7:13]]))
        elif w[0] == 'savelevels':
            cases[-1]['lv'] = m.Levels.of([int(x) for x in w[7:13]])
        elif w[0] == 'levelat':
            cases[-1]['at'].append((int(w[1]), int(w[2])))
    for c in cases:
        got = open(os.path.join(d, 'mirror-%d.raw' % c['n']), 'rb').read()
        want = m.render(c['pieces'], src, c['lv'])
        if got != want:
            k = next((i for i in range(min(len(got), len(want)) // 4) if got[i * 4:i * 4 + 4] != want[i * 4:i * 4 + 4]), None)
            print('FAIL case %d: the samples differ (%d bytes, %d made here; first at frame %s)' % (c['n'], len(got), len(want), k))
            fails += 1
        for t, v in c['at']:
            if m.level_at(c['pieces'], c['lv'], t) != v:
                print('FAIL case %d: the level at %d is %d, %d here' % (c['n'], t, v, m.level_at(c['pieces'], c['lv'], t)))
                fails += 1
                break
    print('check_level_mirror: %d cases, %s' % (len(cases), 'ok' if not fails else '%d failure(s)' % fails))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
