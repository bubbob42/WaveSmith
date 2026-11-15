/*
 * test_marks.c -- marks.c against a model: random adds, removes, moves
 * and cuts, the list rising and each once after every one, never at the
 * start or past the end, no more than MARKS_MAX; and the cases by hand.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "check.h"
#include "marks.h"

static struct Marks mk;

static int rising(const struct Marks *m, ULONG end)
{
	UWORD i;

	if (m->mk_Count > MARKS_MAX) return 0;
	for (i = 0; i < m->mk_Count; i++) {
		if (m->mk_Frame[i] == 0 || m->mk_Frame[i] >= end) return 0;
		if (i && m->mk_Frame[i] <= m->mk_Frame[i - 1]) return 0;
	}
	return 1;
}

/* the model: a frame's marker kept as a flag in a bitmap of a short sound */
#define MODEL_END	4000
static unsigned char model[MODEL_END];

static int same_as_model(const struct Marks *m, ULONG end)
{
	ULONG f;
	UWORD i = 0;

	for (f = 0; f < end; f++) {
		if (!model[f]) continue;
		if (i >= m->mk_Count || m->mk_Frame[i] != f) return 0;
		i++;
	}
	return i == m->mk_Count;
}

int main(void)
{
	struct MarkMove mm;
	struct MarkSet ms;
	static const ULONG set[] = { 900, 100, 500, 100, 0, 5000, 300 };
	ULONG end, f, a, b, k, n;
	UWORD i, r;
	int ok = 1, step;

	/* by hand */
	Marks_Clear(&mk);
	CHECK(Marks_Add(&mk, 0, 1000) == MARKS_NONE && Marks_Add(&mk, 1000, 1000) == MARKS_NONE && mk.mk_Count == 0,
	      "none at the start or past the end");
	CHECK(Marks_Add(&mk, 500, 1000) == 0 && Marks_Add(&mk, 200, 1000) == 0 && Marks_Add(&mk, 800, 1000) == 2, "added in place");
	CHECK(Marks_Add(&mk, 500, 1000) == 1 && mk.mk_Count == 3, "one there already: its index, no second");
	CHECK(Marks_Remove(&mk, 1) && mk.mk_Count == 2 && mk.mk_Frame[1] == 800 && !Marks_Remove(&mk, 2), "the middle one taken away");
	mm.mm_Index = 0;
	mm.mm_Frame = 900;
	mm.mm_End = 1000;
	CHECK(Marks_Move(&mk, &mm) == 1 && mk.mk_Frame[0] == 800 && mk.mk_Frame[1] == 900, "moved past the other: its new index");
	mm.mm_Index = 1;
	mm.mm_Frame = 800;
	CHECK(Marks_Move(&mk, &mm) == 0 && mk.mk_Count == 1, "moved onto the other: one");
	mm.mm_Index = 0;
	mm.mm_Frame = 0;
	CHECK(Marks_Move(&mk, &mm) == MARKS_NONE && mk.mk_Count == 0, "moved to the start: gone");
	ms.ms_Frames = set;
	ms.ms_Count = 7;
	ms.ms_End = 1000;
	Marks_Set(&mk, &ms);
	CHECK(mk.mk_Count == 4 && mk.mk_Frame[0] == 100 && mk.mk_Frame[1] == 300 && mk.mk_Frame[2] == 500 && mk.mk_Frame[3] == 900,
	      "set from frames out of order, twice, at 0, past the end");
	CHECK(Marks_Inside(&mk, 100, 900) == 2 && Marks_Inside(&mk, 0, 1000) == 4, "inside: both ends left out");
	Marks_Cut(&mk, 250, 600);
	CHECK(mk.mk_Count == 2 && mk.mk_Frame[0] == 100 && mk.mk_Frame[1] == 550,
	      "a cut: the ones in it gone, the ones after it earlier");
	Marks_Cut(&mk, 0, 100);
	CHECK(mk.mk_Count == 1 && mk.mk_Frame[0] == 450, "a cut from the start: the marker at its end, now the start, gone");
	Marks_Clear(&mk);
	Marks_Add(&mk, 100, 1000);
	Marks_Add(&mk, 600, 1000);
	Marks_Cut(&mk, 250, 600);
	CHECK(mk.mk_Count == 2 && mk.mk_Frame[1] == 250, "a marker at the cut's end: at the join after it");
	Marks_Clear(&mk);
	for (f = 1; f <= MARKS_MAX + 5; f++) Marks_Add(&mk, f * 10, 100000);
	CHECK(mk.mk_Count == MARKS_MAX && Marks_Add(&mk, 5, 100000) == MARKS_NONE, "no more than MARKS_MAX");
	CHECK(Marks_Add(&mk, 10, 100000) == 0, "full: one there already still found");

	/* against the model: 20000 random steps on a sound of MODEL_END */
	srand(1981);
	Marks_Clear(&mk);
	memset(model, 0, sizeof(model));
	end = MODEL_END;
	for (step = 0; step < 20000 && ok; step++) {
		k = (ULONG) rand() % 10;
		f = (ULONG) rand() % (end + 2);
		if (k < 5) {
			for (n = 0, a = 0; a < end; a++) n += model[a];
			r = Marks_Add(&mk, f, end);
			if (f > 0 && f < end && (model[f] || n < MARKS_MAX)) model[f] = 1;
			ok = (r == MARKS_NONE) == !(f > 0 && f < end && model[f]);
		} else if (k < 7 && mk.mk_Count) {
			i = (UWORD) ((ULONG) rand() % mk.mk_Count);
			model[mk.mk_Frame[i]] = 0;
			ok = Marks_Remove(&mk, i);
		} else if (k < 9 && mk.mk_Count) {
			mm.mm_Index = (UWORD) ((ULONG) rand() % mk.mk_Count);
			mm.mm_Frame = f;
			mm.mm_End = end;
			model[mk.mk_Frame[mm.mm_Index]] = 0;
			Marks_Move(&mk, &mm);
			if (f > 0 && f < end) model[f] = 1;
		} else if (end > 200) {
			/* a cut: the model shifted the same way; the sound shorter */
			a = (ULONG) rand() % end;
			b = a + (ULONG) rand() % 100;
			if (b > end) b = end;
			Marks_Cut(&mk, a, b);
			for (f = a; f < end; f++) model[f] = (unsigned char) (f + (b - a) < end ? model[f + (b - a)] : 0);
			model[0] = 0;
			end -= b - a;
			if (end < 400) {
				end = MODEL_END;
			}
		}
		if (ok) ok = rising(&mk, end) && same_as_model(&mk, end);
	}
	CHECK(ok, "20000 random steps: as the model, rising, each once");
	if (!ok) printf("  at step %d\n", step);
	return CHECK_DONE("test_marks");
}
