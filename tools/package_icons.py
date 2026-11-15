#!/usr/bin/env python3
"""The package's icons besides WaveSmith's own (tools/make_edit_icons.py
writes them all into build/edit-icons):

    folder_icon()    WBDRAWER: a drawer inside WaveSmith's (Help and its
                     languages): the same drawer with an empty label
    drawer_icon()    WBDRAWER: the drawer WaveSmith is installed in, and the
                     package's own -- a drawer front with a small waveform
                     on its label. Martin "Mason" Merz's GlowIcon of the
                     drawer goes on top of it (icons/glow/WaveSmithDrawer.info)
    text_icon()      WBPROJECT for the readme (MultiView)
    install_icon()   WBPROJECT for the Installer: an arrow into the case
    guide_icon()     WBPROJECT for a manual (MultiView): a page with a blue
                     question mark

The family's look (the Prisma Megamix software's icons, where these come
from): 52x40, the 4-colour Workbench palette (0 grey, 1 black, 2 white,
3 blue), a white face, a black frame, a one-pixel shadow right and below.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from amiga_icon import Canvas, GREY, BLACK, WHITE, BLUE  # noqa: E402

W, H = 52, 40


def folder_icon():
    """A drawer front with its handle and an empty label."""
    c = Canvas(W, H)
    c.fill_rect(3, 6, 47, 36, WHITE)
    c.rect(3, 6, 47, 36, BLACK)
    c.hline(4, 48, 37, BLACK)                    # drop shadow
    c.vline(48, 7, 37, BLACK)
    c.hline(4, 46, 10, BLACK)                    # the top rail
    c.rect(13, 14, 37, 26, BLACK)                # the label plate
    c.fill_rect(19, 30, 31, 32, BLUE)            # the handle
    c.rect(19, 30, 31, 32, BLACK)
    return c


def drawer_icon():
    """A drawer front with its handle, and a small waveform on the label."""
    c = Canvas(W, H)
    c.fill_rect(3, 6, 47, 36, WHITE)
    c.rect(3, 6, 47, 36, BLACK)
    c.hline(4, 48, 37, BLACK)                    # drop shadow
    c.vline(48, 7, 37, BLACK)
    c.hline(4, 46, 10, BLACK)                    # the top rail
    # the label plate: a waveform, louder in the middle
    c.rect(13, 14, 37, 26, BLACK)
    c.hline(15, 35, 20, BLUE)
    for x, a in zip(range(16, 35, 2), (1, 2, 4, 3, 5, 4, 3, 2, 3, 1)):
        c.vline(x, 20 - a, 20 + a, BLUE)
    # the handle
    c.fill_rect(19, 30, 31, 32, BLUE)
    c.rect(19, 30, 31, 32, BLACK)
    return c


def text_icon():
    """A page of text with a folded corner."""
    c = Canvas(W, H)
    c.fill_rect(14, 3, 37, 36, WHITE)
    c.rect(14, 3, 37, 36, BLACK)
    c.hline(15, 38, 37, BLACK)                   # drop shadow
    c.vline(38, 4, 37, BLACK)
    for i in range(6):                           # the folded corner
        c.hline(32 + i, 37, 3 + i, GREY)
    c.hline(31, 37, 8, BLACK)
    c.vline(31, 3, 8, BLACK)
    for y in range(12, 34, 4):                   # the lines of text
        c.hline(18, 33 if y % 8 else 29, y, BLUE)
    return c


def install_icon():
    """An arrow into the case: the family's installer icon."""
    c = Canvas(W, H)
    c.fill_rect(21, 2, 30, 15, BLACK)            # shaft
    for i in range(13):                          # head, widest at the top
        c.hline(11 + i, 40 - i, 15 + i, BLACK)
    c.fill_rect(23, 3, 28, 15, BLUE)
    for i in range(10):
        c.hline(14 + i, 37 - i, 16 + i, BLUE)
    c.rect(3, 28, 48, 39, BLACK)                 # the case
    c.fill_rect(4, 29, 47, 38, WHITE)
    c.hline(4, 47, 31, BLACK)
    c.fill_rect(29, 33, 45, 34, BLACK)           # drive slots
    c.fill_rect(29, 36, 45, 37, BLACK)
    c.fill_rect(7, 33, 13, 37, BLUE)             # the badge
    c.fill_rect(9, 34, 11, 36, WHITE)
    c.fill_rect(44, 29, 45, 30, BLUE)            # power light
    return c


def guide_icon():
    """A page with a folded corner, lines of text and a blue question mark."""
    c = Canvas(W, H)
    x0, y0, x1, y1, fold = 10, 2, 41, 37, 8
    c.fill_rect(x0, y0, x1, y1, WHITE)
    for i in range(fold):                        # the fold cut back to grey
        c.hline(x1 - fold + 1 + i, x1, y0 + i, GREY)
    c.vline(x0, y0, y1, BLACK)
    c.vline(x1, y0 + fold, y1, BLACK)
    c.hline(x0, x1 - fold, y0, BLACK)
    c.hline(x0, x1, y1, BLACK)
    for i in range(fold + 1):
        c.set(x1 - fold + i, y0 + i, BLACK)
    c.hline(x1 - fold, x1 - 1, y0 + fold, BLACK)
    c.vline(x1 - fold, y0, y0 + fold, BLACK)
    for y in (8, 12, 16, 20):
        c.hline(x0 + 4, x1 - 12, y, BLACK)
    qx, qy = 22, 24
    c.fill_rect(qx, qy, qx + 7, qy + 1, BLUE)
    c.fill_rect(qx + 6, qy + 1, qx + 7, qy + 4, BLUE)
    c.fill_rect(qx + 3, qy + 4, qx + 5, qy + 6, BLUE)
    c.fill_rect(qx + 3, qy + 8, qx + 5, qy + 9, BLUE)
    return c
