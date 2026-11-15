/*
 * looklevel.h -- a look's level for the gap detector (gapfind.h), made
 * from the samples of a 16-bit PCM take so that a record's noise does not
 * pass for music. The bytes are the .prl map's (levelmap.h: dB below full
 * scale, LEVELMAP_NONE for nothing); only how they are measured differs:
 *
 *   rumble    a one-pole high-pass at about 100 Hz on the measuring path
 *             (the take itself is not touched): a turntable's rumble, a
 *             warp's sway and a DC offset do not raise the floor
 *   crackle   a look (40 ms) is cut into LOOKLEVEL_PIECES pieces of 5 ms;
 *             each piece's peak (the louder channel) is taken, and the
 *             look's level is the LOWER MIDDLE of them -- a click (about
 *             a millisecond) spoils one piece, and up to half the pieces
 *             may be spoiled (some 100 clicks a second) before the level
 *             moves; music fills every piece
 *
 * The chip's own looks (the .prl beside a take) are plain peaks: on a
 * crackling record they read louder in the gaps.
 *
 * A sample is handled as the Amiga loads it: its two bytes as they lie in
 * the file (little-endian), read as one big-endian UWORD.
 *
 * Pure: host-tested (test/host/prismarec/test_looklevel.c).
 */
#ifndef PRISMAREC_LOOKLEVEL_H
#define PRISMAREC_LOOKLEVEL_H

#include <exec/types.h>

#define LOOKLEVEL_MS		40	/* a look */
#define LOOKLEVEL_PIECES	8	/* 5 ms each */
#define LOOKLEVEL_CHANNELS	8	/* at most */
#define LOOKLEVEL_CUTOFF_HZ	100

struct LookLevel {
	UBYTE *ll_Out;			/* the looks' bytes go here ... */
	ULONG ll_OutMax;		/* ... this many at most ... */
	ULONG ll_OutCount;		/* ... this many so far */
	/* the rest: LookLevel_Init()'s */
	UWORD ll_Channels;
	WORD ll_Pole;			/* the high-pass's, Q15 */
	ULONG ll_Frames;		/* a look's frames */
	ULONG ll_PieceFrames;		/* a piece's (the last takes the rest) */
	ULONG ll_Done;			/* frames into this look */
	UWORD ll_Piece;			/* pieces done */
	ULONG ll_PiecePeak;
	UWORD ll_Peaks[LOOKLEVEL_PIECES];
	LONG ll_Prev[LOOKLEVEL_CHANNELS];	/* the filter: the last input ... */
	LONG ll_Last[LOOKLEVEL_CHANNELS];	/* ... and output, per channel */
};

/* Set up for a take of rate frames a second and channels channels
 * (1 .. LOOKLEVEL_CHANNELS); ll_Out, ll_OutMax are the caller's to set,
 * before or after. FALSE for a rate or a channel count it cannot take. */
BOOL LookLevel_Init(struct LookLevel *ll, ULONG rate, UWORD channels);

/* frames frames of samples (channels words each, as loaded): every look
 * they complete goes into ll_Out */
void LookLevel_Feed(struct LookLevel *ll, const UWORD *w, ULONG frames);

/* the take's end: a look begun goes out as it is */
void LookLevel_Finish(struct LookLevel *ll);

/* a look's length in microseconds, as the frames make it */
ULONG LookLevel_Micros(const struct LookLevel *ll, ULONG rate);

#endif
