/*
 * gapfind.h -- the gaps between the tracks of a record side, found in its
 * level looks: the bytes of the take's .prl map (levelmap.h), or a WAV's
 * looks made the same way (LevelMap_Byte). The auto-split after the side
 * (concept T2, decision D3); SplitTracks (PrismaTools) cuts with it today.
 * Pure: host-tested (test/host/prismarec/test_gapfind.c).
 *
 * A look is a dB below full scale (0 .. 90), LEVELMAP_NONE (255) for
 * nothing at all. Silence: a look at or below the threshold (48 dB down by
 * default, or learned from the lead-in: 6 dB above the groove's noise). A
 * click -- loud for at most GAPFIND_CLICK_LOOKS -- does not end a silence.
 * A gap: a silence of at least gs_MinGapMs between music; a track at least
 * gs_MinTrackMs long (a shorter one joins its neighbour). A track starts
 * gs_LeadMs before its music -- where the music rises above the noise (6
 * dB below the threshold), so that a fade-in's quiet start, under the
 * threshold, is kept; no earlier than the gap's middle. The first starts
 * so long before the first music, the last ends gs_TailMs after the last
 * (its fade-out kept the same way) -- or, with gs_KeepEnds, the side's own
 * ends.
 */
#ifndef PRISMAREC_GAPFIND_H
#define PRISMAREC_GAPFIND_H

#include <exec/types.h>

#define GAPFIND_MAXTRACKS	99
#define GAPFIND_CLICK_LOOKS	3	/* 120 ms at 40 ms a look: a click, not music */
#define GAPFIND_MUSIC_DB	30	/* 30 dB down or louder: music for sure (Learn) */

struct GapScan {
	const UBYTE *gs_Looks;
	ULONG gs_Count;			/* looks */
	ULONG gs_LookMs;		/* a look's length: 40 */
	UWORD gs_Threshold;		/* dB down from which a look is silence */
	BOOL gs_KeepEnds;
	ULONG gs_MinGapMs;		/* 1500 */
	ULONG gs_MinTrackMs;		/* 30000 */
	ULONG gs_LeadMs;		/* 500 */
	ULONG gs_TailMs;		/* 2000 */
};

struct GapTrack {
	ULONG gt_Start;			/* its first look */
	ULONG gt_End;			/* the look after its last */
};

struct GapResult {
	UWORD gr_Tracks;
	ULONG gr_MusicStart;		/* the first look of music, the look */
	ULONG gr_MusicEnd;		/* after the last (0, 0: none) */
	struct GapTrack gr_Track[GAPFIND_MAXTRACKS];
};

/* The defaults above for looks of look_ms each; gs_Looks and gs_Count are
 * the caller's to set */
void GapFind_Defaults(struct GapScan *gs, ULONG look_ms);

/* The threshold 6 dB above the groove's noise in the lead-in -- the looks
 * before the first music, the last 2 s before it left out (a fade-in's
 * quiet start; 0.5 s of a short lead-in), at least a second of them, its
 * loudest 10 % left out (the needle's clicks). 0 when there is none to
 * learn from. */
UWORD GapFind_Learn(const struct GapScan *gs);

/* For a side with no lead-in to learn from: the threshold 6 dB above its
 * quietest stretch -- gs_MinGapMs long, its loudest tenth left out: a
 * gap, or the run-out groove. 0 when there is none (digital silence, or
 * nothing quiet enough to be a groove's noise). On a side without any
 * gap a quiet passage of music is the quietest stretch: INFO shows it. */
UWORD GapFind_Quiet(const struct GapScan *gs);

/* The tracks: gr_Tracks of them, 0 when there is no music at all */
UWORD GapFind_Tracks(const struct GapScan *gs, struct GapResult *gr);

#endif
