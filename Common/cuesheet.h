/*
 * cuesheet.h -- the markers as a CUE sheet beside the sound, <name>.cue:
 *
 *   REM COMMENT "WaveSmith"
 *   FILE "Side A.wav" WAVE
 *     TRACK 01 AUDIO
 *       INDEX 01 00:00:00
 *     TRACK 02 AUDIO
 *       REM FRAME 1764000
 *       INDEX 01 00:40:00
 *
 * a TRACK for the start and one for each marker, its INDEX 01 at
 * mm:ss:ff (ff 75ths of a second, as players read it) and the exact
 * frame in a REM beside it (players pass REMs over; WaveSmith and
 * SplitTracks cut at it). Read back, the first TRACK's start is where the
 * first track begins (what lies before it, a pregap: a record's lead-in),
 * each later one's a marker: the REM's frame, else INDEX 01's time.
 * Pure: host-tested
 * (test/host/wavesmith/test_cuesheet.c).
 */
#ifndef WAVESMITH_CUESHEET_H
#define WAVESMITH_CUESHEET_H

#include <exec/types.h>

#define CUE_MARKS_MAX	98		/* a CUE sheet's 99 tracks, the first at the start */
#define CUE_TEXTMAX	8192		/* Cue_Write(): the most it writes (99 tracks: 6.7 KB) */
#define CUE_NAMELEN	108		/* a file name's room */

struct CueSheet {
	CONST_STRPTR cs_File;		/* the sound's name, no drawer */
	ULONG cs_Rate;
	const ULONG *cs_Marks;		/* frames, rising, past 0 */
	UWORD cs_Count;
};

/* The sheet into buf, size CUE_TEXTMAX at least (else 0): the length */
ULONG Cue_Write(const struct CueSheet *cs, STRPTR buf, ULONG size);

struct CueRead {
	ULONG cr_Rate;			/* in: the sound's */
	ULONG cr_Frames;		/* in: its length -- a mark at or past it dropped */
	ULONG cr_First;			/* out: the first track's start (0: the file's) */
	ULONG cr_Marks[CUE_MARKS_MAX];	/* out: the later tracks' starts, past cr_First,
					   rising, each once */
	UWORD cr_Count;
};

/* The markers of the sheet in text (len bytes): cr_Count */
UWORD Cue_Read(struct CueRead *cr, CONST_STRPTR text, ULONG len);

#endif
