/*
 * gainwin.h -- Effects/Gain... (concept 3.2): a window over WaveSmith's,
 * a slider and a field in tenths of a dB (F6: -24 to +18), the part's
 * peak now and after the gain, Normalise to a level (F7), Listen, Use,
 * Cancel. While it is open the caller is told every move (ga_Tell), so
 * that the waveform behind follows the slider and Listen plays the part
 * at the gain -- the player kept fed from this window's own loop.
 */
#ifndef WAVESMITH_GAINWIN_H
#define WAVESMITH_GAINWIN_H

#include <exec/types.h>
#include <intuition/intuition.h>

#include "gaincalc.h"

/* what the caller is told (GainMsg.gm_What) */
#define GAIN_PREVIEW	1		/* gm_Tenths: the gain now -- show it */
#define GAIN_LISTEN	2		/* Listen: the part played from its start (the
				   gain it plays at follows the previews) */
#define GAIN_SIGNALS	3		/* gm_Sigs: ga_SigMask's signals came */
#define GAIN_SHOWN	4		/* gm_Boxes: the window open, its gadgets' boxes */

/* gm_Boxes: GAIN_BOXES of them */
#define GAIN_BOX_WINDOW		0
#define GAIN_BOX_FIELD		1
#define GAIN_BOX_NORMALISE	2
#define GAIN_BOX_LISTEN		3
#define GAIN_BOX_USE		4
#define GAIN_BOX_CANCEL		5
#define GAIN_BOXES		6

struct GainMsg {
	UWORD gm_What;			/* GAIN_#? */
	WORD gm_Tenths;
	ULONG gm_Sigs;
	const struct IBox *gm_Boxes;
};

struct GainArgs {
	struct Window *ga_Over;		/* the window it is centred over */
	WORD ga_Tenths;			/* in: the gain at first; out: the gain used */
	WORD ga_Target;			/* in and out: Normalise's level, tenths, <= 0 */
	ULONG ga_Peak;			/* the part's loudest |sample| now, 0 .. 32768 */
	void (*ga_Tell)(const struct GainMsg *gm);	/* NULL: nobody */
	ULONG ga_SigMask;		/* signals waited for besides the window's */
	BOOL ga_NoWindow;		/* out: none could be opened */
};

/* The window opened, answered and gone: TRUE for Use, ga_Tenths and
   ga_Target then what it said; FALSE for Cancel, Esc or the close
   gadget (ga_Target kept all the same: the level is a setting) */
BOOL Gain_Window(struct GainArgs *ga);

#endif
