#ifndef PRISMA_NORMWAV_H
#define PRISMA_NORMWAV_H

/*
 * normwav.h -- Normalise's pure part: a 16-bit PCM WAV file's header, the
 * decibels in tenths without floating point, and the gain as a table of
 * every sample value.
 *
 * A sample is handled as the Amiga loads it: its two bytes, as they lie in
 * the file (little-endian), read as one big-endian UWORD -- the file's
 * first byte in the high half. The table maps such a word to the word to
 * store, so the samples are never turned round on the way through.
 *
 * No AmigaOS calls in here: the module builds on the host for its tests
 * (test/host/test_normwav.c).
 */

#ifdef PRISMA_HOST
/* 32 bits, as on the Amiga (a host long has 64) */
typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef short WORD;
typedef unsigned int ULONG;
typedef int LONG;
typedef const char *CONST_STRPTR;
#else
#include <exec/types.h>
#endif

/* NormWav_Parse()'s answers */
#define NORMWAV_OK		0
#define NORMWAV_NOTWAV		1	/* no RIFF WAVE header */
#define NORMWAV_NOTPCM16	2	/* not 16-bit PCM (or a block that is not channels x 2) */
#define NORMWAV_NODATA		3	/* no "data" chunk in the bytes given */

/* nw_DataBytes when the header does not say: the chip writes $FFFFFFFF
 * until the recording is closed (PrismaLib/riff.h), so a take that never
 * was has it -- the samples then run to the file's end */
#define NORMWAV_TOEND		0xFFFFFFFFUL

/* the deepest level in tenths of a dB that the arithmetic knows */
#define NORMWAV_MAXTENTHS	999

struct NormWav {
	ULONG nw_DataStart;		/* the first sample's byte */
	ULONG nw_DataBytes;		/* as the header says, or NORMWAV_TOEND */
	ULONG nw_Rate;			/* frames a second */
	UWORD nw_Channels;
	UWORD nw_Align;			/* bytes a frame: channels x 2 */
};

/* The header in the file's first len bytes: NORMWAV_#? */
UWORD NormWav_Parse(const UBYTE *head, ULONG len, struct NormWav *nw);

/* little-endian fields */
ULONG NormWav_LE32(const UBYTE *p);
void NormWav_PutLE32(UBYTE *p, ULONG v);

/* The header's two sizes made true for databytes of samples and nothing
 * after them: the data chunk's, and the RIFF size (with the pad byte an
 * odd data chunk is followed by) */
void NormWav_FixSizes(UBYTE *head, const struct NormWav *nw, ULONG databytes);

/* 32767 (full scale) tenths x 0.1 dB down, rounded; tenths at most
 * NORMWAV_MAXTENTHS */
ULONG NormWav_Amplitude(UWORD tenths);

/* How far below full scale a peak lies, in tenths of a dB, the nearer of
 * the two neighbours: 0 at 32767 and above, NORMWAV_MAXTENTHS for 0 */
UWORD NormWav_PeakTenths(ULONG peak);

/* A level like "-1", "-1.5", "-0.1" or "0" (decibels, at most 0, one
 * decimal) into tenths below full scale: 1, or 0 when it is none */
LONG NormWav_ParseTenths(CONST_STRPTR s, UWORD *tenths);

/* The highest magnitude among n samples (words as loaded, see above) and
 * peak, the run so far: -32768 counts as 32768 */
ULONG NormWav_Peak(const UWORD *w, ULONG n, ULONG peak);

/* table[65536]: every sample word to the word that stores it times
 * target / peak, rounded half away from zero; peak 1..32768, target at
 * most 32767 -- a sample of magnitude peak becomes target exactly */
void NormWav_Table(UWORD *table, ULONG target, ULONG peak);

/* n samples through the table, in place */
void NormWav_Apply(UWORD *w, ULONG n, const UWORD *table);

#endif
