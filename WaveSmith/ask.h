#ifndef WAVESMITH_ASK_H
#define WAVESMITH_ASK_H

/*
 * ask.h -- a question in a window of its own, centred on WaveSmith's:
 * EasyRequest() takes no position (it opens at the screen's top left), and
 * requester.class centres on the screen, so the question has to BE a
 * window -- window.class's WPOS_CENTERWINDOW (after AmiMatters' gui_ask.c).
 *
 * Answers as EasyRequest does: 1 the leftmost button, 2 the next, 0 the
 * last one -- Esc and the close gadget answer 0 too, Return the leftmost.
 * An underscore in the gadget string makes the next letter that button's
 * key. Modal: its own loop until answered; without the memory for the
 * window, EasyRequest asks instead.
 */

#include <exec/types.h>
#include <intuition/intuition.h>

#define ASK_LINES	8		/* lines of the body at most */
#define ASK_BUTTONS	4

struct AskArgs {
	struct Window *aa_Over;		/* centred on it, on its screen */
	CONST_STRPTR aa_Title;
	CONST_STRPTR aa_Body;		/* finished text, lines by \n */
	CONST_STRPTR aa_Gadgets;	/* "_Save|_Discard|_Cancel" */
	struct IBox *aa_Opened;		/* NULL, or the window's box the moment
					   it opened (the tests: it must not move);
					   Width 0 when EasyRequest asked */
};

LONG Ask_Window(const struct AskArgs *aa);

#endif
