#ifndef WAVESMITH_MARGINS_H
#define WAVESMITH_MARGINS_H

/*
 * margins.h -- Settings, Lead and tail...: the suggestion's margins in a
 * window of their own, centred on WaveSmith's (as ask.h's question): In
 * so long before the music, Out so long after it. Each a slider and its
 * milliseconds, typed or slid, the seconds beside them read-only. Use
 * takes them, Cancel (Esc, the close gadget) leaves them. Modal: its own
 * loop until answered. slider.gadget and integer.gadget are opened for
 * the window and closed after it.
 */

#include <exec/types.h>
#include <intuition/intuition.h>

/* the boxes the window reports once open (the tests aim by them):
   the window on the screen, the rest in the window */
#define MARGINS_BOX_WINDOW	0
#define MARGINS_BOX_LEAD	1	/* the lead's milliseconds */
#define MARGINS_BOX_TAIL	2
#define MARGINS_BOX_USE		3
#define MARGINS_BOXES		4

struct MarginArgs {
	struct Window *ma_Over;		/* centred on it, on its screen */
	ULONG ma_Lead, ma_Tail;		/* ms: in, and out when used */
	ULONG ma_LeadMax, ma_TailMax;
	/* NULL, or called once the window is open with its MARGINS_BOXES
	   boxes (the tests' trace) */
	void (*ma_Shown)(const struct IBox *boxes);
	BOOL ma_NoWindow;		/* out: no window (a class missing, no memory) */
};

/* TRUE: Use, ma_Lead and ma_Tail what the window says; FALSE: Cancel,
   or no window could be made (ma_NoWindow): the two as they were */
BOOL Margins_Window(struct MarginArgs *ma);
/* ms as seconds into buf (MARGINS_SECSLEN): "0.5 s", "2.0 s", "0.75 s" */
#define MARGINS_SECSLEN	16
void Margins_Secs(ULONG ms, char *buf);

#endif
