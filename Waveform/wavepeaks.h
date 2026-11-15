#ifndef WAVEFORM_WAVEPEAKS_H
#define WAVEFORM_WAVEPEAKS_H

/*
 * wavepeaks.h -- a sound's picture for waveform.gadget, kept in RAM: each
 * channel's minimum and maximum (the sample's top byte) over every 1024
 * frames, and again over 16 and 256 of those -- for a 22-minute stereo
 * side 229 + 14 + 1 KB. Built while the file is read (WavePeaks_Feed),
 * asked from any task (WavePeaks_Query: no I/O, no waiting -- the class's
 * input methods run on input.device's task). Finer than 1024 frames a
 * column it has no answer: the program reads the samples then
 * (WAVEFORM_Want, WAVEFORM_Samples).
 *
 * The memory is the caller's: WavePeaks_Init() says how much each level
 * takes (WavePeaks_Bytes()), the caller points wp_Level[] at it.
 *
 * Pure: host-tested (test/host/waveform/test_wavepeaks.c).
 */

#include <exec/types.h>
#include <gadgets/waveform.h>

#define WAVEPEAKS_LEVELS	3
#define WAVEPEAKS_SHIFT0	10	/* level 0: 1024 frames an entry */
#define WAVEPEAKS_SHIFTUP	4	/* each level 16 entries of the one below */
#define WAVEPEAKS_CHANNELS	2	/* the reader mixes more down first */

struct WavePeaks {
	BYTE *wp_Level[WAVEPEAKS_LEVELS];	/* entries x channels x (min, max) */
	ULONG wp_Entries[WAVEPEAKS_LEVELS];
	ULONG wp_Frames;			/* the sound's length */
	ULONG wp_Done;				/* frames a query sees: fed and in
						   their slots (WavePeaks_Flush() moves
						   it, as the feed's last step -- a
						   picture drawn meanwhile on another
						   task reads no slot not written) */
	ULONG wp_Fed;				/* frames fed so far */
	UWORD wp_Channels;
	UWORD wp_Pad;
	/* each level's entry being built: its minimum and maximum so far */
	BYTE wp_Min[WAVEPEAKS_LEVELS][WAVEPEAKS_CHANNELS];
	BYTE wp_Max[WAVEPEAKS_LEVELS][WAVEPEAKS_CHANNELS];
};

/* For a sound of frames frames in channels channels (1 or 2): the levels'
   sizes; wp_Level[] NULL until the caller sets them. FALSE for channels
   out of range. */
BOOL WavePeaks_Init(struct WavePeaks *wp, ULONG frames, UWORD channels);

/* The bytes level takes */
ULONG WavePeaks_Bytes(const struct WavePeaks *wp, UWORD level);

/* count frames more, interleaved, signed 16 bits in the machine's order */
void WavePeaks_Feed(struct WavePeaks *wp, const WORD *frames, ULONG count);

/* The same from a reader that has the frames' extremes already (one pass
   over its buffer for several measures): WavePeaks_RunLeft() frames at
   most -- to the open entry's end -- given as each channel's minimum and
   maximum top byte (WavePeaks_Top(): mnmx channel after channel, minimum
   first); WavePeaks_Flush() after the last run of a piece, so that a
   query sees it. Runs and a flush leave what one WavePeaks_Feed() of the
   same frames leaves. */
ULONG WavePeaks_RunLeft(const struct WavePeaks *wp);
void WavePeaks_FeedRun(struct WavePeaks *wp, ULONG count, const BYTE *mnmx);
void WavePeaks_Flush(struct WavePeaks *wp);

/* wpm_Count columns of wpm_Step frames from wpm_First into wpm_Out (as the
   WaveSource hook): the columns answered, from the first on -- 0 for a
   step under 1024 frames; a column that starts beyond what has been fed
   ends the answer. A column beyond the sound gives 0, 0. */
ULONG WavePeaks_Query(const struct WavePeaks *wp, struct WavePeakMsg *q);

/* A sample's top byte, -128..127 (floor(v / 256)) */
BYTE WavePeaks_Top(WORD v);

#endif
