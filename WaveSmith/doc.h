#ifndef WAVESMITH_DOC_H
#define WAVESMITH_DOC_H

/*
 * doc.h -- the file WaveSmith has open: its header, its picture (the
 * peaks in RAM, wavepeaks.h), its looks (looklevel.c's crackle-proof
 * level and each look's plain peak, for trimplan.c), read a piece at a
 * time so that the window stays alive; the samples the gadget wants,
 * a zero crossing near a handle, and the save. The samples stay in the
 * file (concept D1): only the picture is in RAM.
 *
 * This step: 16-bit PCM WAV, mono or stereo (normwav.c); the other
 * formats PrismaEncode reads come with indecode.c.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <gadgets/waveform.h>

#include "wavepeaks.h"
#include "looklevel.h"
#include "normwav.h"
#include "trimplan.h"
#include "timeline.h"

#define DOC_PATHLEN	256

/* how the doc is being read */
#define DOC_READ_ALL	0		/* once through, everything (a short file) */
#define DOC_READ_HEAD	1		/* its head, the level's filter on */
#define DOC_READ_TAIL	2		/* its tail, the level and the plain peaks */
#define DOC_READ_REST	3		/* on from the head, the picture and the peaks
					   (the level too when the ends could not
					   decide: d_PlanReady FALSE) */

/* Doc_Open()'s answers besides DOS's error codes */
#define DOC_OK		0
#define DOC_E_NOTWAV	-1		/* not a WAV file */
#define DOC_E_NOTPCM16	-2		/* not 16-bit PCM, or not mono or stereo */
#define DOC_E_NOMEM	-3
#define DOC_E_EMPTY	-4		/* no samples */
#define DOC_E_KEPT	-5		/* Doc_Save: the original gone, the new file
					   under its temporary name (Doc_LastTemp()) */
#define DOC_E_CANCELLED	-6		/* Doc_Save/SaveAs: called off by d_SaveProgress */
/* Doc_Save's answer when the doc is the file saved (positive: no DOS
   error is this small) */
#define DOC_KEPT	1

/* the picture and each look's plain peak, built as the samples go by (a
   reading, or a save writing the file anew): one whole, so that a save
   can build the new file's in its place and give the old one back */
struct DocPicture {
	struct WavePeaks dp_Peaks;	/* the file's peaks, wavepeaks.h */
	UBYTE *dp_PeakLooks;		/* each look's plain peak (levelmap.h's dB) */
	UWORD *dp_PeakWords;		/* the same, the louder channel's |sample|
					   (0 .. 32767): Doc_PartPeak's */
	ULONG dp_LookMax;		/* looks the arrays hold (d_Looks too) */
	ULONG dp_PeakCount;		/* plain peaks done */
	ULONG dp_PeakIn;		/* frames into the look being measured */
	ULONG dp_PeakL, dp_PeakR;	/* its peaks so far */
	BOOL dp_NoPicture;		/* a long file's tail pass: plain peaks only */
};

/* the samples a fetch reads, two buffers taken in turn: the class draws
   from the one it was handed (on input.device's task) while the next
   fetch fills the other */
#define DOC_FETCHES	2

struct Doc {
	char d_Path[DOC_PATHLEN];
	BPTR d_FH;
	struct NormWav d_Wav;
	UBYTE *d_Head;			/* the header up to the samples */
	ULONG d_HeadMax;		/* the bytes d_Head holds */
	ULONG d_Frames;
	ULONG d_Trailer;		/* bytes after the samples (a LIST chunk ...) */
	UWORD d_Channels;
	ULONG d_Rate;

	/* the picture (d_Pic.dp_Peaks), the gadget's source drawn from it
	   along the timeline */
	struct DocPicture d_Pic;
	struct WaveSource d_Source;

	/* the sound as shown, played and saved (timeline.h): the whole file
	   until a cut. Every frame below but the reading's is the timeline's;
	   d_Frames is the file's. Doc_Changed() after a change. */
	struct Timeline d_Tl;
	/* the level from outside the pieces (timeline.h, concept 3.2): the
	   fades at In and Out, the dips at the cuts -- the window's to set
	   before it plays or saves; all 0 (Doc_Open's): none */
	struct TlLevels d_Levels;
	ULONG d_Clipped;		/* the last save's samples held at full scale */

	/* the looks */
	struct LookLevel d_LL;
	UBYTE *d_Looks;			/* crackle-proof levels (d_Pic.dp_LookMax of them) */

	/* reading */
	ULONG d_Read;			/* frames read */
	LONG d_ReadError;		/* a read or seek that failed (a DOS error), the
					   reading ended with it: the picture is not
					   whole; 0 none */
	BOOL d_Done;
	UBYTE *d_Buf;

	/* the suggestion, once read -- or once the ends are (d_PlanReady) */
	struct TrimPlan d_Plan;
	BOOL d_PlanReady;
	ULONG d_PlanLooks;		/* the looks it was made from (Doc_Replan) */
	BOOL d_LevelWhole;		/* d_Looks measured all through (a long file
					   read ends first: its middle only after
					   Doc_MeasureLevel) */

	/* a long file read ends first (concept D11 b): the crackle-proof
	   level over the first and the last Doc_SetEnds() seconds only, the
	   suggestion made from them, then the rest for the picture. When the
	   music began after the head or ended before the tail (a run-out
	   left running), the ends do not decide: the level is measured on
	   through the rest and the suggestion made at the end, as a short
	   file's */
	UWORD d_Phase;			/* DOC_READ_#? */
	ULONG d_HeadEnd;		/* the head's end, on a look */
	ULONG d_TailFrom;		/* the tail's first frame, a look early (the
					   filter's warm-up), on a look */
	ULONG d_TailAt;			/* the tail read so far, to here */
	struct LookLevel d_TailLL;
	ULONG d_KeepCount, d_KeepIn, d_KeepL, d_KeepR;	/* the plain peaks' place, kept over it */

	/* a save's progress, per cent, each time it moves: the window's gauge
	   (NULL: none) -- a save of a side takes minutes on a 68030; FALSE
	   calls the save off (DOC_E_CANCELLED: the file and the doc as they
	   were) */
	BOOL (*d_SaveProgress)(UWORD percent);

	/* samples handed to the gadget (WAVEFORM_Samples): DOC_FETCHES
	   buffers, d_FetchNext the one the next fetch fills */
	WORD *d_Samples[DOC_FETCHES];
	ULONG d_SamplesMax[DOC_FETCHES];	/* frames each holds */
	struct WaveSamples d_WS[DOC_FETCHES];
	UWORD d_FetchNext;
	UWORD d_FetchPad;
};

/* d cleared and the file opened, its header read: DOC_OK, a DOC_E_#?, or
   a DOS error code */
LONG Doc_Open(struct Doc *d, CONST_STRPTR path);
/* the timeline's frames: what the window shows and Save writes */
ULONG Doc_Length(const struct Doc *d);
/* d_Tl changed (a cut, an undo): the source's length with it -- the
   gadgets' WAVEFORM_Source set again after */
void Doc_Changed(struct Doc *d);
/* Another piece read into the picture and the looks: TRUE while there is
   more; the plan is made (d_Plan, d_PlanReady) at the end, or once the
   ends of a long file are read -- the picture comes after */
BOOL Doc_Read(struct Doc *d);
/* The seconds of either end the level is measured over in a long file;
   a file not longer than twice that is measured whole. 0: every file
   whole. For the opens that follow. */
void Doc_SetEnds(ULONG seconds);
/* In's place before the music and Out's after it, ms (trimplan.h): for
   the plans made from now on (Doc_Replan) */
void Doc_SetMargins(ULONG lead, ULONG tail);
/* Doc_Save keeps the file it replaces as <path>.bak (an older one
   replaced); FALSE: it is deleted */
void Doc_SetBackup(BOOL keep);
/* the read doc's suggestion made again (Doc_SetMargins changed): FALSE
   while it is being read */
BOOL Doc_Replan(struct Doc *d);
/* the crackle-proof level over the middle of a long file read ends first
   (Find tracks wants it all through), d_SaveProgress told as a save
   tells it: 0 (d_LevelWhole), DOC_E_CANCELLED or a DOS error -- the doc
   as it was */
LONG Doc_MeasureLevel(struct Doc *d);
/* the tracks of the read doc (d_LevelWhole) as the suggestion's gap
   detector finds them, into tp (In and Out not touched): FALSE without
   a whole level or music */
BOOL Doc_Tracks(struct Doc *d, struct TrimPlan *tp);
/* how far the reading is, per cent */
UWORD Doc_Percent(const struct Doc *d);
/* timeline frames from first into a buffer the gadget does not hold
   (the machine's order), the buffers taken in turn: the samples, or
   NULL when they cannot be read or held (the buffers as they were) */
const struct WaveSamples *Doc_Fetch(struct Doc *d, ULONG first, ULONG frames);
/* the zero crossing nearest frame within so many frames either side;
   frame itself when there is none or the file cannot be read */
ULONG Doc_ZeroCross(struct Doc *d, ULONG frame, ULONG within);
/* (Doc_Save and Doc_SaveAs: once the plan is ready, the picture need not
   be done) */
/* the file written anew with the timeline's frames in..out - 1 only, in place
   (beside, then renamed over; protection, comment and date kept):
   DOC_KEPT, the doc is the file saved -- read: its picture and its plain
   peaks made from the samples as they were written, its crackle-proof
   level carried over along the timeline (as the .prl is cut, a look off
   at a join at most), its suggestion made again from them; nothing is
   read again (a long side took minutes). 0: the same, but without the
   memory for the new picture -- the doc closed, to be opened again. A
   DOS error / DOC_E_#?: the file and the doc as they were (d_FH 0 when
   the file could not be opened again); DOC_E_KEPT, the doc closed */
LONG Doc_Save(struct Doc *d, ULONG in, ULONG out);
/* the timeline's frames ds_In..ds_Out - 1 into a new file ds_Path (beside it under
   a temporary name, then renamed over whatever is there); the doc as it
   was: 0 or a DOS error / DOC_E_#? */
struct DocSave {
	ULONG ds_In, ds_Out;
	CONST_STRPTR ds_Path;
};
LONG Doc_SaveAs(struct Doc *d, const struct DocSave *ds);
/* the loudest |sample| of the timeline's frames first .. end - 1 at their
   level (0 .. 32768; Normalise and the Gain window, concept 3.2): a
   stretch as recorded from its looks' peaks, the looks at its edges read
   again; one at a level read through Tl_Read and measured. The file
   being read: the part it holds so far. */
ULONG Doc_PartPeak(struct Doc *d, ULONG first, ULONG end);
/* The temporary file a save writes first, beside path: the k-th name of
   the few tried ("WaveSmith-<k>.tmp" in path's drawer -- short, so that
   FFS's 30 characters never cut it into another file's name); FALSE when
   it does not fit DOC_PATHLEN */
BOOL Doc_TempName(CONST_STRPTR path, UWORD k, STRPTR out);
/* the last save's temporary file (DOC_E_KEPT: the sound is there) and
   backup ("" before any): what the window's message names */
CONST_STRPTR Doc_LastTemp(void);
CONST_STRPTR Doc_LastBackup(void);
/* everything the doc holds given back; the doc cleared */
void Doc_Close(struct Doc *d);

#endif
