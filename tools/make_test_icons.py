#!/usr/bin/env python3
"""Generate the Workbench icons for the IconX test scripts (test/amiga/).

Every script gets a WBPROJECT icon whose default tool is C:IconX, with a
WINDOW tooltype so its output stays on screen until closed. `gmake stage`
writes them beside the scripts in dist/, whose .backdrop puts them on the
Workbench desktop when dist/ is mounted as DEV: (FS-UAE); on real hardware
the copied dist drawer shows them as ordinary icons.

Glyphs (4-colour, family style of the starter's run icons): an arrow into a
box (install), a magnifier (diag), a sine wave (tone), a play triangle
(play), a record dot (record).

Usage:
    python3 tools/make_test_icons.py <outdir> <Script> [<Script> ...]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amiga_icon import Canvas, write_info, WBPROJECT, GREY, BLACK, WHITE, BLUE

W, H = 52, 40


def window(c):
    """An application window: frame, title bar, close + depth gadgets."""
    c.fill_rect(4, 4, 47, 35, WHITE)
    c.rect(4, 4, 47, 35, BLACK)
    c.hline(4, 47, 10, BLACK)
    c.rect(6, 6, 10, 8, BLACK)
    c.rect(40, 6, 44, 8, BLACK)


def glyph_install(c):
    c.rect(17, 24, 34, 32, BLACK)            # the box (LIBS:)
    c.fill_rect(24, 13, 27, 22, BLUE)        # arrow shaft
    for i in range(5):                       # arrow head
        c.hline(21 + i, 30 - i, 22 + i, BLUE)


def glyph_diag(c):
    c.rect(18, 14, 27, 23, BLUE)             # lens
    c.rect(19, 15, 26, 22, BLUE)
    for i in range(6):                       # handle
        c.fill_rect(27 + i, 23 + i, 28 + i, 24 + i, BLACK)


def glyph_tone(c):
    ys = [22, 19, 17, 16, 17, 19, 22, 25, 27, 28, 27, 25]
    for i in range(24):                      # two periods of a sine
        y = ys[i % 12]
        c.vline(14 + i, y, y + 1, BLUE)
    c.hline(13, 38, 22, BLACK)


def glyph_play(c):
    for i in range(13):
        top = 15 + (i * 3) // 4
        bot = 32 - (i * 3) // 4
        if top <= bot:
            c.vline(20 + i, top, bot, BLUE)


def glyph_record(c):
    for dy in range(-7, 8):                  # a filled dot, radius 7
        half = int((49 - dy * dy) ** 0.5 + 0.5)
        c.hline(26 - half, 26 + half, 22 + dy, BLUE)
    c.hline(22, 30, 22, BLACK)


def glyph_drawers(c):
    c.rect(14, 19, 37, 32, BLACK)            # a drawer, its front
    c.hline(14, 37, 23, BLACK)
    c.hline(23, 28, 27, BLACK)               # the handle
    for i in range(7):                       # and the music, playing on
        top = 11 + (i * 3) // 4
        bot = 17 - (i * 3) // 4
        if top <= bot:
            c.vline(28 + i, top, bot, BLUE)


def glyph_bus(c):
    c.rect(13, 13, 38, 26, BLACK)            # a card
    c.fill_rect(17, 16, 24, 21, BLACK)       # its chip
    for i in range(8):                       # its contact fingers, the bus
        c.vline(15 + i * 3, 27, 31, BLUE)


GLYPHS = {
    "Install": glyph_install, "InstallGCC": glyph_install, "InstallDebug": glyph_install, "InstallAHI": glyph_install, "InstallAHIDebug": glyph_install,
    "Diag": glyph_diag, "State": glyph_diag,
    "Tone44": glyph_tone, "Tone22": glyph_tone, "ToneULaw": glyph_tone, "BenchSeries": glyph_tone, "DeckBench": glyph_tone, "SkinBench": glyph_tone, "AHISeries": glyph_tone, "AHILatency": glyph_tone, "PaulaSides": glyph_tone, "CodecTest": glyph_tone, "ChannelCheck": glyph_tone,
    "PlayFile": glyph_play, "SoundTest": glyph_play, "M4aRestart": glyph_play, "MasterTest": glyph_play, "MixerTest": glyph_play, "PrismaPlay": glyph_play, "PrismaPlayGCC": glyph_play,
    "PrismaRexx": glyph_diag, "PlayerState": glyph_diag,
    "Record": glyph_record, "RecordPlay": glyph_record, "AHIRecord": glyph_record, "PopTest": glyph_record, "ExtIn": glyph_record, "FlacTest": glyph_record, "FlacProbe": glyph_record, "MonitorCheck": glyph_record, "Levels": glyph_record, "Standby": glyph_record, "CardTakes": glyph_record, "Deck": glyph_record, "DeckPlay": glyph_play, "RecTour": glyph_record,
    "EditTry": glyph_play,
    # the tester's package (test/tester/, gmake release-tester)
    "Bench": glyph_tone, "Channels": glyph_tone, "AHI": glyph_tone, "Modes": glyph_tone, "Master": glyph_play, "MHI": glyph_diag,
    "Drawers": glyph_drawers, "BusCheck": glyph_bus,
}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    outdir = sys.argv[1]
    for name in sys.argv[2:]:
        c = Canvas(W, H, bg=GREY)
        window(c)
        GLYPHS.get(name, glyph_play)(c)
        dest = os.path.join(outdir, name + ".info")
        write_info(dest, c, do_type=WBPROJECT, default_tool="C:IconX",
                   tooltypes=["WINDOW=CON:0/20/640/200/WaveSmith %s/CLOSE/WAIT" % name],
                   stack=16384)
        print("wrote %s" % dest)


if __name__ == "__main__":
    main()
