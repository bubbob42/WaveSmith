/*
 * marks.h -- the markers: where the tracks of a side start, timeline
 * frames, rising, each once, never at the start (the first track's is
 * the file's). Put down at the play marker (Return), dragged, taken
 * away (Del), found (Find tracks), cut with the sound; the cue sheet
 * keeps them (cuesheet.h). Pure: host-tested
 * (test/host/wavesmith/test_marks.c).
 */
#ifndef WAVESMITH_MARKS_H
#define WAVESMITH_MARKS_H

#include <exec/types.h>

#include "cuesheet.h"

#define MARKS_MAX	CUE_MARKS_MAX
#define MARKS_NONE	0xFFFF

struct Marks {
	ULONG mk_Frame[MARKS_MAX];
	UWORD mk_Count;
	UWORD mk_Pad;
};

void Marks_Clear(struct Marks *mk);
/* a marker at f, in its place: its index; MARKS_NONE when f is the start
   or past end, or there are MARKS_MAX -- one there already: its index */
UWORD Marks_Add(struct Marks *mk, ULONG f, ULONG end);
/* the i-th taken away: FALSE when there is none */
BOOL Marks_Remove(struct Marks *mk, UWORD i);
/* the i-th moved to f (dragged), as Marks_Add puts it: its new index,
   MARKS_NONE when it went (f the start or past end: taken away) */
struct MarkMove {
	UWORD mm_Index;
	ULONG mm_Frame, mm_End;
};
UWORD Marks_Move(struct Marks *mk, const struct MarkMove *mm);
/* first .. end - 1 cut out of the timeline (joined): the markers in it
   gone, the ones after it earlier by its length */
void Marks_Cut(struct Marks *mk, ULONG first, ULONG end);
/* count frames (any order) as the markers, as Marks_Add puts each */
struct MarkSet {
	const ULONG *ms_Frames;
	UWORD ms_Count;
	ULONG ms_End;
};
void Marks_Set(struct Marks *mk, const struct MarkSet *ms);
/* how many lie inside first .. end (both left out): the parts a split
   at them makes, less one */
UWORD Marks_Inside(const struct Marks *mk, ULONG first, ULONG end);

#endif
