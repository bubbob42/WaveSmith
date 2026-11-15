#ifndef WAVESMITH_ABOUT_H
#define WAVESMITH_ABOUT_H

/*
 * about.h -- the About window: Mason's picture (art/wavesmith_app.png,
 * about_pic.h) beside the version, what WaveSmith does and the credits.
 * Herald's About window (src/aboutpic.c), with the picture blended onto
 * the window's background: it is glass, not opaque.
 *
 * MODELESS: WaveSmith goes on playing and painting while it is open, so
 * the caller adds About_Signal() to its Wait() and calls
 * About_HandleInput() when it wakes. One at a time: a second About_Open()
 * brings the open one to the front. Failure is silent -- an About box
 * that cannot be made is worth no requester.
 *
 * The picture is no BOOPSI image in the layout (layout.gadget draws only
 * gadgets): an empty layout of its size keeps its place, and it is drawn
 * there by hand once the window is open -- a smart refresh window keeps
 * it -- each of its pixels as wide as the screen's pixels make it square
 * (two on hires without lace). All it holds is allocated (one pointer of
 * near data: WaveSmith's SAS/C near data is all but full).
 */

#include <exec/types.h>
#include <intuition/intuition.h>

/* open it centred on over, on its screen (or bring it forward) */
void About_Open(struct Window *over);
/* the signals to wait for; 0 while it is shut */
ULONG About_Signal(void);
/* what it has had: a button, a key, the close gadget */
void About_HandleInput(void);
/* shut, its pens given back (on every way out of WaveSmith) */
void About_Close(void);

#endif
