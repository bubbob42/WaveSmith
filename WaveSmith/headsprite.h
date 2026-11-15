/*
 * headsprite.h -- the play heads as hardware sprites on a native screen,
 * moved 50 times a second by a process of its own
 * (Documentation/Concept-WaveSmith-Transport-2026-11.md 4, 8, 11): the
 * main task, painting, reading ahead and feeding AHI in the same beat,
 * moved the drawn head 8-9 times a second on an A1200, and the overview's
 * stood still for seconds on an A2000 while the next page was read.
 *
 * Two lines: the lanes' head and the overview's, a pair of sprites in the
 * head's orange. The main task stays their owner: it says each beat where
 * the head is and where each line may show (HeadSprite_Place()); the
 * mover carries the frame on by the clock between those words and hides
 * the lines at once while a menu is up, the window is not the active one
 * or its screen is not in front. Without a sprite (RTG, none free)
 * nothing opens and the gadgets draw their heads as before; with one
 * only, the lanes have it. A layer in front of the window over a line (a
 * bubble, another window) hides it while it passes under it: a sprite
 * shows over everything.
 */
#ifndef WAVESMITH_HEADSPRITE_H
#define WAVESMITH_HEADSPRITE_H

#include <exec/types.h>
#include <intuition/intuition.h>

#define HEAD_COVERS	6		/* the layers over a line kept apart */
#define HEAD_LINES	2
#define HEAD_MAIN	0		/* the lanes' head */
#define HEAD_OVER	1		/* the overview's */

/* where one line goes */
struct HeadLine {
	ULONG hl_First, hl_Zoom;	/* the view: column = (frame - first) / zoom */
	WORD hl_Left, hl_Top;		/* the first column, the lanes' top: the SCREEN's pixels */
	UWORD hl_Columns, hl_Rows;
	BOOL hl_On;			/* FALSE: hidden */
	/* the module's own: the layers over the line, their columns */
	UWORD hl_Covers;
	WORD hl_CoverX0[HEAD_COVERS], hl_CoverX1[HEAD_COVERS];
};

struct HeadPlace {
	ULONG hp_Frame;			/* the play frame ... */
	ULONG hp_Ticks;			/* ... at this E-clock reading (Clock_Ticks()) */
	ULONG hp_Rate;			/* frames a second */
	struct HeadLine hp_Line[HEAD_LINES];
};

/* the sprites, their colour (a pen of their own, or past the screen's own
   colours one set for them; else sprite 1 alone, in the pointer's colour)
   and the mover, for this window: FALSE without */
BOOL HeadSprite_Open(struct Window *win);
void HeadSprite_Close(void);
BOOL HeadSprite_There(void);
/* the main task's word, copied (a new height makes a line anew), with
   the layers over the lines as they are -- tried, never waited for: a
   menu holds the screen's layers, and AHI's feeding would wait with it;
   busy, the last ones. The lines a sprite shows (1 << HEAD_#?); the
   others' gadgets draw their heads (no sprite, or more layers over it
   than kept apart) */
UWORD HeadSprite_Place(const struct HeadPlace *hp);

#endif
