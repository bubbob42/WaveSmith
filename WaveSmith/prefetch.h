#ifndef WAVESMITH_PREFETCH_H
#define WAVESMITH_PREFETCH_H

/*
 * prefetch.h -- while playing at a zoom finer than the picture holds, the
 * samples of the page in view and the next one read ahead, a piece at a
 * time on a handle of their own (neither the doc's reading nor the card's
 * moves): handed to the gadget before the page turns, so the next page is
 * drawn from its samples at once instead of stretched from the picture
 * first and drawn again when they come. Frames are the doc's timeline's,
 * borrowed: Prefetch_Close() before it changes.
 *
 * Two buffers: the one the gadget draws from (shown) and the one being
 * filled. A window takes what the samples the gadget draws from hold of
 * it -- this page's, whoever read them -- and what was read of it before
 * it moved, and reads only the rest: the page after this one. Prefetch_
 * Close() frees them: the gadget must have let go of them first
 * (WAVEFORM_Samples).
 */

#include <exec/types.h>
#include <gadgets/waveform.h>

#include "doc.h"

#define PREFETCH_PIECE	16384UL		/* frames read a beat at most: 64 KB of stereo
					   -- the next page's in 7 beats; an A1200's
					   beats come 5 to 10 a second while it plays
					   deep. pf_Piece: what the program's time
					   allows (an A2000 reads 16384 in 150 ms) */
#define PREFETCH_COPY	4UL		/* frames copied a step from the gadget's samples:
					   so many pieces' worth -- a copy costs an
					   A2000 a twelfth of a read */

struct Prefetch {
	BPTR pf_FH;			/* 0: not open */
	const struct Timeline *pf_Tl;	/* the doc's */
	const struct TlLevels *pf_Levels;	/* the doc's end fades and dips */
	ULONG pf_Max;			/* frames a buffer holds */
	ULONG pf_Frames;		/* the sound's (the timeline's) */
	ULONG pf_Start;			/* the samples' first byte in the file */
	UWORD pf_Align, pf_Channels;
	WORD *pf_Buf[2];
	struct WaveSamples pf_WS[2];	/* the buffers as the gadget takes them */
	UWORD pf_Fill;			/* the one being filled */
	ULONG pf_First, pf_Want;	/* the window: first, frames (NONE: none) */
	ULONG pf_Got;			/* of it read so far */
	BOOL pf_Handed;			/* whole and given to the gadget */
	BOOL pf_Failed;			/* the file would not give it: given up */
	ULONG pf_Piece;			/* frames a step, PREFETCH_PIECE at most (Prefetch_Open sets it) */
	ULONG pf_Read;			/* frames the last step read */
	ULONG pf_Copied;		/* ... copied */
	const WORD *pf_From;		/* the gadget's samples copied from (Prefetch_Want) */
	ULONG pf_FromFirst;		/* ... their first frame */
	ULONG pf_Copy;			/* frames of the window copied from them: the first so many */
	struct WaveSamples pf_Part;	/* Prefetch_Part()'s */
};

/* Ready for d's file with buffers of max frames: FALSE (nothing held) when
   the file or the memory is not to be had */
BOOL Prefetch_Open(struct Prefetch *pf, const struct Doc *d, ULONG max);
void Prefetch_Close(struct Prefetch *pf);
/* the window first .. first + frames - 1 wanted (clipped to the sound and
   to a buffer): nothing when it is what is filled or shown already (shown
   but have not it: handed again by the next step). have: the samples the
   gadget draws from (NULL none): their part of it copied by the steps,
   not read again -- told before each step: let go of before the copy is
   through, the rest is read */
void Prefetch_Want(struct Prefetch *pf, ULONG first, ULONG frames, const struct WaveSamples *have);
/* a piece more copied (pf_Copied) or read (pf_Piece, pf_Read); the
   samples of the window once it is whole -- then the buffer is the
   gadget's (shown) -- else NULL */
struct WaveSamples *Prefetch_Step(struct Prefetch *pf);
/* what the buffer being filled holds so far (NULL: nothing) -- valid
   until the next Prefetch_#? call */
const struct WaveSamples *Prefetch_Part(struct Prefetch *pf);

#endif
