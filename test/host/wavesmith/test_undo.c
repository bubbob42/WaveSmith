/*
 * test_undo.c -- undo.c: the steps, the saved state and the * it decides,
 * a change after an Undo dropping the steps undone (and the saved one
 * with them), a cut timeline given back piece for piece, the oldest
 * steps going past UNDO_MAX, and nothing held after (doshost.c counts
 * AllocVec's blocks).
 *
 *   test_undo <work drawer>   (not used: the test writes no files)
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "doshost.h"
#include "undo.h"

static struct Undo undo;
static struct Timeline tl, back;

static struct UndoMark mark(ULONG in, ULONG out)
{
	struct UndoMark m;

	memset(&m, 0, sizeof(m));
	m.um_In = in;
	m.um_Out = out;
	m.um_Tl = &tl;
	return m;
}

int main(void)
{
	struct UndoMark m;
	const struct UndoState *s;
	UWORD i, n;
	int ok;

	Undo_Init(&undo);
	CHECK(!Undo_CanBack(&undo) && !Undo_CanForward(&undo) && Undo_Back(&undo) == NULL, "empty: nothing to undo or redo");

	/* opened: one step, saved */
	Tl_Init(&tl, 1000);
	m = mark(100, 900);
	CHECK(Undo_Start(&undo, &m) && !Undo_Dirty(&undo) && !Undo_CanBack(&undo), "opened: saved, nothing to undo");
	CHECK(Undo_Push(&undo, &m) == UNDO_SAME && undo.u_Count == 1, "the same state again: no step");

	/* In moved: a step, unsaved; back: saved again; forward: unsaved */
	m = mark(200, 900);
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && Undo_Dirty(&undo) && Undo_CanBack(&undo), "In moved: a step, unsaved");
	s = Undo_Back(&undo);
	CHECK(s && s->us_In == 100 && s->us_Out == 900 && !Undo_Dirty(&undo), "Undo: In back at 100, saved again");
	CHECK(Undo_CanForward(&undo) && !Undo_CanBack(&undo), "Undo: Redo possible, no further Undo");
	s = Undo_Forward(&undo);
	CHECK(s && s->us_In == 200 && Undo_Dirty(&undo) && !Undo_CanForward(&undo), "Redo: In at 200 again, unsaved");

	/* undone, then another change: the step undone is gone */
	Undo_Back(&undo);
	m = mark(100, 800);
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && !Undo_CanForward(&undo) && undo.u_Count == 2,
	      "a change after Undo: the step undone dropped");
	s = Undo_Back(&undo);
	CHECK(s && s->us_In == 100 && s->us_Out == 900 && !Undo_Dirty(&undo), "Undo: back to the opened state");

	/* saved later; undone past the save, then a change: no saved step left */
	m = mark(100, 900);
	Undo_Start(&undo, &m);
	m = mark(150, 900);
	Undo_Push(&undo, &m);
	m = mark(150, 850);
	Undo_Push(&undo, &m);
	Undo_SetSaved(&undo);
	CHECK(!Undo_Dirty(&undo), "saved beside: the shown state the file's");
	Undo_Back(&undo);
	CHECK(Undo_Dirty(&undo), "Undo past the save: unsaved");
	Undo_Forward(&undo);
	CHECK(!Undo_Dirty(&undo), "Redo to the save: saved again");
	Undo_Back(&undo);
	Undo_Back(&undo);
	m = mark(120, 900);
	Undo_Push(&undo, &m);
	CHECK(Undo_Dirty(&undo) && undo.u_Saved == UNDO_NONE, "a change past the save: the saved step gone, unsaved");
	Undo_Back(&undo);
	CHECK(Undo_Dirty(&undo), "and back: still unsaved (the file holds none of these)");

	/* a cut: the pieces given back as they were */
	Tl_Init(&tl, 1000);
	m = mark(0, 1000);
	Undo_Start(&undo, &m);
	Tl_Cut(&tl, 300, 400);
	Tl_Silence(&tl, 500, 600);
	back = tl;
	m = mark(0, 900);
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED, "a cut and a silence: a step");
	s = Undo_Back(&undo);
	Undo_Apply(s, &tl);
	CHECK(Tl_Whole(&tl, 1000) && s->us_Out == 1000, "Undo: the whole file again");
	s = Undo_Forward(&undo);
	Undo_Apply(s, &tl);
	CHECK(Tl_Same(&tl, &back) && s->us_Out == 900, "Redo: the cut's pieces, every one");
	/* pieces equal, handles not: a step */
	m = mark(10, 900);
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED, "the same pieces, In moved: a step");

	/* Undo_Amend: the arrows held on In -- one step for the run, amended;
	   the first and the saved step never amended; after an Undo a step
	   anew; back where the run began, the step taken away */
	Tl_Init(&tl, 1000);
	m = mark(100, 900);
	Undo_Start(&undo, &m);
	m = mark(101, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 2, "amend on the first step: a step pushed");
	m = mark(102, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 2 && undo.u_Steps[1].us_In == 102, "amended: still two steps, In 102");
	m = mark(105, 900);
	Undo_Amend(&undo, &m);
	CHECK(Undo_Amend(&undo, &m) == UNDO_SAME && undo.u_Count == 2, "the same state amended: no change");
	s = Undo_Back(&undo);
	CHECK(s && s->us_In == 100 && !Undo_Dirty(&undo), "one Undo takes the whole run back");
	Undo_Forward(&undo);
	m = mark(100, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_DROPPED && undo.u_Count == 1 && undo.u_At == 0 && !Undo_Dirty(&undo),
	      "back where the run began: the step dropped, saved again");
	m = mark(110, 900);
	Undo_Push(&undo, &m);
	Undo_SetSaved(&undo);
	m = mark(111, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 3 && undo.u_Steps[1].us_In == 110,
	      "the saved step not amended: a step after it");
	Undo_Back(&undo);
	m = mark(120, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 3 && !Undo_CanForward(&undo) && undo.u_Steps[2].us_In == 120,
	      "after an Undo (the saved step shown): a step anew, the undone one gone");
	m = mark(130, 900);
	Undo_Amend(&undo, &m);
	s = Undo_Back(&undo);
	CHECK(s && s->us_In == 110 && !Undo_Dirty(&undo), "Undo after the run: the saved state");
	/* a cut amended too (the pieces copied anew) */
	Undo_Forward(&undo);
	CHECK(Tl_Cut(&tl, 300, 400), "amend: a cut");
	m = mark(130, 800);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Steps[2].us_Count == tl.tl_Count, "amend: the cut's pieces kept");
	/* after an Undo to a step that is not the saved one: a step anew */
	m = mark(100, 900);
	Undo_Start(&undo, &m);
	m = mark(200, 900);
	Undo_Push(&undo, &m);
	m = mark(300, 900);
	Undo_Push(&undo, &m);
	Undo_Back(&undo);
	m = mark(250, 900);
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 3 && undo.u_Steps[1].us_In == 200 &&
	      undo.u_Steps[2].us_In == 250, "after an Undo, not on the saved step: step 1 kept, a step anew");
	/* a history begun without a saved step (no memory for the opened one):
	   its first step never amended */
	Undo_Free(&undo);
	m = mark(100, 900);
	Undo_Push(&undo, &m);
	m = mark(101, 900);
	CHECK(undo.u_Saved == UNDO_NONE && Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Count == 2 &&
	      undo.u_Steps[0].us_In == 100, "the first step, unsaved: not amended, a step after it");
	Undo_Free(&undo);
	CHECK(dos_host.dh_Vecs == 0, "amend: everything given back");

	/* markers: part of a state -- a marker added is a step, Undo gives the
	   markers before it back (a copy, the caller's array free to change);
	   no memory for the markers: the history gone, nothing held */
	{
		static ULONG marks[3] = { 1000, 5000, 9000 };
		const struct UndoState *ms;

		Tl_Init(&tl, 10000);
		m = mark(0, 10000);
		Undo_Start(&undo, &m);
		m.um_Marks = marks;
		m.um_MarkCount = 2;
		CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && Undo_Dirty(&undo), "a marker added: a step, unsaved");
		marks[1] = 5100;
		CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && undo.u_Count == 3, "a marker moved: a step");
		CHECK(Undo_Push(&undo, &m) == UNDO_SAME, "the same markers: no step");
		m.um_MarkCount = 3;
		CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED, "a third marker: a step");
		marks[0] = 1;
		ms = Undo_Back(&undo);
		CHECK(ms && ms->us_MarkCount == 2 && ms->us_Marks[0] == 1000 && ms->us_Marks[1] == 5100,
		      "Undo: two markers, as they were (a copy: the array changed since)");
		ms = Undo_Back(&undo);
		CHECK(ms && ms->us_Marks[1] == 5000, "Undo: the marker where it was before the move");
		ms = Undo_Back(&undo);
		CHECK(ms && ms->us_MarkCount == 0 && ms->us_Marks == NULL && !Undo_Dirty(&undo), "Undo: no markers, saved");
		ms = Undo_Forward(&undo);
		CHECK(ms && ms->us_MarkCount == 2, "Redo: the markers back");
		Undo_Forward(&undo);
		Undo_Forward(&undo);
		m.um_MarkCount = 3;
		CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Steps[undo.u_At].us_Marks[0] == 1, "amend: the markers copied anew");
		/* no memory for a state's markers (its pieces got theirs) */
		CHECK(Tl_Cut(&tl, 100, 200), "markers: a cut, pieces to copy");
		dos_host.dh_FailVecIn = 2;
		m.um_In = 5;
		CHECK(Undo_Push(&undo, &m) == UNDO_NOMEM && undo.u_Count == 0 && dos_host.dh_Vecs == 0,
		      "no memory for the markers: the history gone, nothing held");
		dos_host.dh_FailVecIn = 0;
		Undo_Free(&undo);
	}

	/* the end fades: part of a state -- one made is a step, its length
	   changed another, Undo gives the lengths before them back */
	Tl_Init(&tl, 10000);
	m = mark(1000, 9000);
	Undo_Start(&undo, &m);
	m.um_FadeIn = 500;
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && Undo_Dirty(&undo), "an end fade made: a step, unsaved");
	CHECK(Undo_Push(&undo, &m) == UNDO_SAME, "the same end fades: no step");
	m.um_FadeOut = 2000;
	CHECK(Undo_Push(&undo, &m) == UNDO_PUSHED && undo.u_Count == 3, "the fade at Out: a step");
	s = Undo_Back(&undo);
	CHECK(s && s->us_FadeIn == 500 && s->us_FadeOut == 0, "Undo: the fade at In alone");
	s = Undo_Back(&undo);
	CHECK(s && s->us_FadeIn == 0 && s->us_FadeOut == 0 && !Undo_Dirty(&undo), "Undo: no end fade, saved");
	s = Undo_Forward(&undo);
	s = Undo_Forward(&undo);
	CHECK(s && s->us_FadeIn == 500 && s->us_FadeOut == 2000, "Redo: both again");
	m.um_FadeOut = 2500;
	CHECK(Undo_Amend(&undo, &m) == UNDO_PUSHED && undo.u_Steps[undo.u_At].us_FadeOut == 2500, "amend: the length taken");
	m.um_FadeOut = 0;
	CHECK(Undo_Amend(&undo, &m) == UNDO_DROPPED && undo.u_Count == 2, "amend back to the step before: dropped");
	Undo_Free(&undo);

	/* past UNDO_MAX: the oldest go, the saved first of them */
	Tl_Init(&tl, 100000UL);
	m = mark(0, 100000UL);
	Undo_Start(&undo, &m);
	for (i = 1, ok = 1; i < UNDO_MAX + 100 && ok; i++) {
		m = mark(i, 100000UL);
		ok = Undo_Push(&undo, &m) == UNDO_PUSHED;
	}
	CHECK(ok && undo.u_Count == UNDO_MAX && undo.u_Saved == UNDO_NONE, "past the limit: as many steps as kept, the saved one gone");
	for (n = 0; Undo_Back(&undo); n++) ;
	CHECK(n == UNDO_MAX - 1 && undo.u_Steps[undo.u_At].us_In == 100, "Undo to the oldest kept: In 100");

	Undo_Free(&undo);
	CHECK(dos_host.dh_Vecs == 0, "everything given back");
	return CHECK_DONE("test_undo");
}
