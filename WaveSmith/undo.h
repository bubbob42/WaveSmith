#ifndef WAVESMITH_UNDO_H
#define WAVESMITH_UNDO_H

/*
 * undo.h -- the user's changes, each a step to take back and do again
 * (concept 3.1; E6, the user's: a handle moved -- a drag, a nudge, a
 * typed time -- is a step, and so is a cut). Each step is the state
 * after it, kept whole: In, Out and the timeline's pieces (a copy); the
 * first is the state the file was opened in; the markers are part of a
 * state as the handles are (their frames, a copy), and so are the fades
 * at In and Out (concept 3.2: two lengths). A change after an Undo
 * drops the steps undone; past UNDO_MAX steps the oldest go.
 *
 * The saved state is the one the file holds: back at it, nothing is
 * unsaved (the title's * goes); a change made after undoing past it
 * leaves no step that is (the * stays until Save).
 *
 * Pure but for the memory (exec): host-tested
 * (test/host/wavesmith/test_undo.c).
 */

#include <exec/types.h>

#include "timeline.h"

#define UNDO_MAX	500
#define UNDO_NONE	0xFFFF

/* a state: kept by the history (us_Pieces and us_Marks its own copies) */
struct UndoState {
	ULONG us_In, us_Out;		/* the handles (WAVEFORM_NONE: none) */
	ULONG us_FadeIn, us_FadeOut;	/* the end fades' frames (0: none) */
	struct Piece *us_Pieces;
	UWORD us_Count;
	UWORD us_MarkCount;
	ULONG *us_Marks;		/* the markers' timeline frames, rising */
};

/* a state given to the history: copied */
struct UndoMark {
	ULONG um_In, um_Out;
	ULONG um_FadeIn, um_FadeOut;
	const struct Timeline *um_Tl;
	const ULONG *um_Marks;		/* NULL with um_MarkCount 0: none */
	UWORD um_MarkCount;
	UWORD um_Pad;
};

struct Undo {
	struct UndoState u_Steps[UNDO_MAX];
	UWORD u_Count;			/* steps held */
	UWORD u_At;			/* the one shown */
	UWORD u_Saved;			/* the one the file holds, UNDO_NONE */
	UWORD u_Pad;
};

/* Undo_Push()'s answers */
#define UNDO_PUSHED	0
#define UNDO_SAME	1		/* the state shown already: no step */
#define UNDO_NOMEM	2		/* no memory: the history gone, unsaved */
#define UNDO_DROPPED	3		/* Undo_Amend: back at the step before, the
					   amended one taken away */

/* empty: no step, nothing saved */
void Undo_Init(struct Undo *u);
/* everything given back: empty again */
void Undo_Free(struct Undo *u);
/* the history anew from a file just opened: its one step, saved; FALSE
   (empty) without the memory */
BOOL Undo_Start(struct Undo *u, const struct UndoMark *m);
/* a change made: its state the next step: UNDO_#? */
UWORD Undo_Push(struct Undo *u, const struct UndoMark *m);
/* a change that goes on the step shown (the arrow keys held on a handle:
   one step, not one a press): its state m instead -- only for the last
   step, neither the first nor the saved one (else as Undo_Push); m the
   state before it: the step taken away (UNDO_DROPPED). The caller amends
   only a step its own Undo_Push or Undo_Amend made (UNDO_PUSHED) */
UWORD Undo_Amend(struct Undo *u, const struct UndoMark *m);
/* Undo: the state before the one shown, now shown -- NULL: none */
const struct UndoState *Undo_Back(struct Undo *u);
/* Redo: the state after, now shown -- NULL: none */
const struct UndoState *Undo_Forward(struct Undo *u);
BOOL Undo_CanBack(const struct Undo *u);
BOOL Undo_CanForward(const struct Undo *u);
/* the state shown is not the one the file holds */
BOOL Undo_Dirty(const struct Undo *u);
/* the state shown is now the file's (a save beside it, the doc kept) */
void Undo_SetSaved(struct Undo *u);
/* a state's pieces into a timeline */
void Undo_Apply(const struct UndoState *s, struct Timeline *tl);

#endif
