/*
 * undo.c -- the user's changes, step by step (undo.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "undo.h"

static void drop(struct UndoState *s)
{
	if (s->us_Pieces) FreeVec(s->us_Pieces);
	if (s->us_Marks) FreeVec(s->us_Marks);
	s->us_Pieces = NULL;
	s->us_Marks = NULL;
	s->us_Count = s->us_MarkCount = 0;
}

/* m into s, the pieces copied: FALSE without the memory */
static BOOL keep(struct UndoState *s, const struct UndoMark *m)
{
	UWORD i, n = m->um_Tl->tl_Count;

	s->us_In = m->um_In;
	s->us_Out = m->um_Out;
	s->us_FadeIn = m->um_FadeIn;
	s->us_FadeOut = m->um_FadeOut;
	s->us_Count = n;
	s->us_MarkCount = 0;
	s->us_Pieces = NULL;
	s->us_Marks = NULL;
	if (n) {
		if (!(s->us_Pieces = (struct Piece *) AllocVec((ULONG) n * sizeof(struct Piece), MEMF_ANY))) {
			s->us_Count = 0;
			return FALSE;
		}
		for (i = 0; i < n; i++) s->us_Pieces[i] = m->um_Tl->tl_Pieces[i];
	}
	if (m->um_MarkCount && m->um_Marks) {
		if (!(s->us_Marks = (ULONG *) AllocVec((ULONG) m->um_MarkCount * sizeof(ULONG), MEMF_ANY))) {
			drop(s);
			return FALSE;
		}
		for (i = 0; i < m->um_MarkCount; i++) s->us_Marks[i] = m->um_Marks[i];
		s->us_MarkCount = m->um_MarkCount;
	}
	return TRUE;
}

static BOOL same(const struct UndoState *s, const struct UndoMark *m)
{
	UWORD i;

	if (s->us_In != m->um_In || s->us_Out != m->um_Out || s->us_Count != m->um_Tl->tl_Count) return FALSE;
	if (s->us_FadeIn != m->um_FadeIn || s->us_FadeOut != m->um_FadeOut) return FALSE;
	if (s->us_MarkCount != (m->um_Marks ? m->um_MarkCount : 0)) return FALSE;
	for (i = 0; i < s->us_MarkCount; i++) {
		if (s->us_Marks[i] != m->um_Marks[i]) return FALSE;
	}
	for (i = 0; i < s->us_Count; i++) {
		if (!Tl_PieceSame(&s->us_Pieces[i], &m->um_Tl->tl_Pieces[i]))
			return FALSE;
	}
	return TRUE;
}

void Undo_Init(struct Undo *u)
{
	UWORD i;

	for (i = 0; i < UNDO_MAX; i++) {
		u->u_Steps[i].us_Pieces = NULL;
		u->u_Steps[i].us_Marks = NULL;
		u->u_Steps[i].us_Count = u->u_Steps[i].us_MarkCount = 0;
	}
	u->u_Count = u->u_At = 0;
	u->u_Saved = UNDO_NONE;
	u->u_Pad = 0;
}

void Undo_Free(struct Undo *u)
{
	while (u->u_Count) drop(&u->u_Steps[--u->u_Count]);
	u->u_At = 0;
	u->u_Saved = UNDO_NONE;
}

BOOL Undo_Start(struct Undo *u, const struct UndoMark *m)
{
	Undo_Free(u);
	if (!keep(&u->u_Steps[0], m)) return FALSE;
	u->u_Count = 1;
	u->u_At = 0;
	u->u_Saved = 0;
	return TRUE;
}

UWORD Undo_Push(struct Undo *u, const struct UndoMark *m)
{
	UWORD i;

	if (u->u_Count && same(&u->u_Steps[u->u_At], m)) return UNDO_SAME;
	/* the steps undone go -- and with them the saved one, if it was there */
	while (u->u_Count > u->u_At + 1) drop(&u->u_Steps[--u->u_Count]);
	if (u->u_Saved != UNDO_NONE && u->u_Saved >= u->u_Count) u->u_Saved = UNDO_NONE;
	/* full: the oldest goes */
	if (u->u_Count == UNDO_MAX) {
		drop(&u->u_Steps[0]);
		for (i = 1; i < UNDO_MAX; i++) u->u_Steps[i - 1] = u->u_Steps[i];
		u->u_Steps[UNDO_MAX - 1].us_Pieces = NULL;
		u->u_Steps[UNDO_MAX - 1].us_Marks = NULL;
		u->u_Steps[UNDO_MAX - 1].us_Count = u->u_Steps[UNDO_MAX - 1].us_MarkCount = 0;
		u->u_Count--;
		if (u->u_Saved == 0) u->u_Saved = UNDO_NONE;
		else if (u->u_Saved != UNDO_NONE) u->u_Saved--;
	}
	if (!keep(&u->u_Steps[u->u_Count], m)) {
		Undo_Free(u);
		return UNDO_NOMEM;
	}
	u->u_At = u->u_Count++;
	return UNDO_PUSHED;
}

UWORD Undo_Amend(struct Undo *u, const struct UndoMark *m)
{
	struct UndoState s;

	if (!u->u_Count || u->u_At == 0 || u->u_At + 1 != u->u_Count || u->u_At == u->u_Saved) return Undo_Push(u, m);
	if (same(&u->u_Steps[u->u_At], m)) return UNDO_SAME;
	/* back where the step began: no step at all */
	if (same(&u->u_Steps[u->u_At - 1], m)) {
		drop(&u->u_Steps[u->u_At]);
		u->u_Count--;
		u->u_At--;
		return UNDO_DROPPED;
	}
	/* the new state kept before the old goes: no memory leaves the old */
	s.us_Pieces = NULL;
	s.us_Marks = NULL;
	s.us_Count = s.us_MarkCount = 0;
	if (!keep(&s, m)) {
		Undo_Free(u);
		return UNDO_NOMEM;
	}
	drop(&u->u_Steps[u->u_At]);
	u->u_Steps[u->u_At] = s;
	return UNDO_PUSHED;
}

const struct UndoState *Undo_Back(struct Undo *u)
{
	if (!Undo_CanBack(u)) return NULL;
	return &u->u_Steps[--u->u_At];
}

const struct UndoState *Undo_Forward(struct Undo *u)
{
	if (!Undo_CanForward(u)) return NULL;
	return &u->u_Steps[++u->u_At];
}

BOOL Undo_CanBack(const struct Undo *u)
{
	return (BOOL) (u->u_Count && u->u_At > 0);
}

BOOL Undo_CanForward(const struct Undo *u)
{
	return (BOOL) (u->u_At + 1 < u->u_Count);
}

BOOL Undo_Dirty(const struct Undo *u)
{
	return (BOOL) (u->u_Saved != u->u_At || !u->u_Count);
}

void Undo_SetSaved(struct Undo *u)
{
	if (u->u_Count) u->u_Saved = u->u_At;
}

void Undo_Apply(const struct UndoState *s, struct Timeline *tl)
{
	UWORD i;

	tl->tl_Frames = 0;
	tl->tl_Count = s->us_Count;
	for (i = 0; i < s->us_Count; i++) {
		tl->tl_Pieces[i] = s->us_Pieces[i];
		tl->tl_Frames += s->us_Pieces[i].p_Frames;
	}
}
