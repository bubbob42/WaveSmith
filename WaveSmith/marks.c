/*
 * marks.c -- see marks.h
 */
#include <exec/types.h>

#include "marks.h"

void Marks_Clear(struct Marks *mk)
{
	mk->mk_Count = 0;
	mk->mk_Pad = 0;
}

UWORD Marks_Add(struct Marks *mk, ULONG f, ULONG end)
{
	UWORD i, k;

	if (f == 0 || f >= end) return MARKS_NONE;
	for (i = 0; i < mk->mk_Count && mk->mk_Frame[i] < f; i++) ;
	if (i < mk->mk_Count && mk->mk_Frame[i] == f) return i;
	if (mk->mk_Count >= MARKS_MAX) return MARKS_NONE;
	for (k = mk->mk_Count; k > i; k--) mk->mk_Frame[k] = mk->mk_Frame[k - 1];
	mk->mk_Frame[i] = f;
	mk->mk_Count++;
	return i;
}

BOOL Marks_Remove(struct Marks *mk, UWORD i)
{
	if (i >= mk->mk_Count) return FALSE;
	for (mk->mk_Count--; i < mk->mk_Count; i++) mk->mk_Frame[i] = mk->mk_Frame[i + 1];
	return TRUE;
}

UWORD Marks_Move(struct Marks *mk, const struct MarkMove *mm)
{
	if (!Marks_Remove(mk, mm->mm_Index)) return MARKS_NONE;
	/* one less now: there is room for it again */
	return Marks_Add(mk, mm->mm_Frame, mm->mm_End);
}

void Marks_Cut(struct Marks *mk, ULONG first, ULONG end)
{
	UWORD i, n = 0;

	if (end <= first) return;
	for (i = 0; i < mk->mk_Count; i++) {
		if (mk->mk_Frame[i] >= first && mk->mk_Frame[i] < end) continue;
		mk->mk_Frame[n++] = mk->mk_Frame[i] >= end ? mk->mk_Frame[i] - (end - first) : mk->mk_Frame[i];
	}
	mk->mk_Count = n;
	/* one that came to lie on the cut's place where another was: once */
	for (i = 1, n = 1; i < mk->mk_Count; i++) {
		if (mk->mk_Frame[i] != mk->mk_Frame[n - 1]) mk->mk_Frame[n++] = mk->mk_Frame[i];
	}
	if (mk->mk_Count) mk->mk_Count = n;
	/* a marker that came to the start is none */
	if (mk->mk_Count && mk->mk_Frame[0] == 0) Marks_Remove(mk, 0);
}

void Marks_Set(struct Marks *mk, const struct MarkSet *ms)
{
	UWORD i;

	Marks_Clear(mk);
	for (i = 0; i < ms->ms_Count; i++) Marks_Add(mk, ms->ms_Frames[i], ms->ms_End);
}

UWORD Marks_Inside(const struct Marks *mk, ULONG first, ULONG end)
{
	UWORD i, n = 0;

	for (i = 0; i < mk->mk_Count; i++) {
		if (mk->mk_Frame[i] > first && mk->mk_Frame[i] < end) n++;
	}
	return n;
}
