#ifndef WAVESMITH_TRIMPLAN_H
#define WAVESMITH_TRIMPLAN_H

/*
 * trimplan.h -- what WaveSmith suggests to cut off a recording's ends,
 * from its looks (40 ms each): where the music starts and ends (gapfind.c,
 * the threshold learned from the lead-in's groove noise), In half a
 * second before it, Out two seconds after (a fade's quiet ends kept), the
 * lead-in and run-out as silence regions, and the clicks in them -- a look
 * whose plain peak stands far above its crackle-proof level (looklevel.c
 * measures the level, a click spoils only a piece of it). The status
 * line's facts too: the lengths, the clicks, the run-out groove's period.
 *
 * Pure: host-tested (test/host/wavesmith/test_trimplan.c).
 */

#include <exec/types.h>
#include <gadgets/waveform.h>

#include "gapfind.h"

#define TRIMPLAN_REGIONS	64	/* two silences, the rest clicks */
#define TRIMPLAN_CLICK_DB	12	/* a click: its peak this much above the look's level */

struct TrimLooks {
	const UBYTE *tl_Level;		/* each look's crackle-proof level (dB down, 255 none) */
	const UBYTE *tl_Peak;		/* each look's plain peak, the same scale */
	ULONG tl_Count;			/* looks */
	ULONG tl_LookFrames;		/* frames a look */
	ULONG tl_Frames;		/* the sound's */
	ULONG tl_Rate;
	ULONG tl_LeadMs;		/* In before the music: 500 */
	ULONG tl_TailMs;		/* Out after it: 2000 */
};

struct TrimPlan {
	BOOL tp_Found;			/* music found: In and Out suggested */
	UWORD tp_Threshold;		/* dB down: silence at or below it */
	ULONG tp_In, tp_Out;		/* frames; the file's ends when nothing found */
	ULONG tp_MusicStart, tp_MusicEnd;	/* frames */
	UWORD tp_LeadClicks, tp_TailClicks;
	ULONG tp_PeriodMs;		/* the run-out's clicks' spacing; 0: none seen */
	UWORD tp_RegionCount;
	struct WaveRegion tp_Region[TRIMPLAN_REGIONS];
	/* the tracks the gap detector found (SplitTracks' tracks): each one's
	   first frame, half a second before its music; the first In's */
	UWORD tp_TrackCount;
	ULONG tp_TrackStart[GAPFIND_MAXTRACKS];
};

/* The plan for the looks; FALSE (tp_Found too) when there is no music:
   In 0, Out the sound's end, no regions */
BOOL TrimPlan_Make(const struct TrimLooks *tl, struct TrimPlan *tp);

#endif
