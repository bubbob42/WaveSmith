#ifndef WAVESMITH_AHIPLAY_H
#define WAVESMITH_AHIPLAY_H

/*
 * ahiplay.h -- listening through AHI, play.h's other way (Settings, Play
 * through: Automatic falls back to it without the card, or with the card
 * in another program's hands): the span along the timeline streamed as
 * two sounds of AHI's, one playing while the other is filled. AHI's sound
 * hook (its mixing interrupt) notes which sound began and signals; the
 * task fills the buffer that finished, a piece a beat, and queues it
 * behind the one playing (one not queued in time: AHI plays the current
 * one again). The mode is the user's (AHI's mode requester, Settings, AHI
 * mode...), else AHI's default. ahi.device is opened while it plays, and
 * for the requester -- not held otherwise.
 */

#include <exec/types.h>
#include <intuition/intuition.h>

#include "doc.h"
#include "play.h"

/* Ahi_Start()'s answers besides play.h's PLAY_OK and PLAY_FILE */
#define AHIPLAY_NODEVICE	20	/* no ahi.device 4 */
#define AHIPLAY_NOMODE		21	/* the mode is not there, or in use */
#define AHIPLAY_NOMEMORY	22

/* the mode: an AHI audio ID (0: AHI's default) and its mixing rate (0:
   the mode's default) */
struct AhiMode {
	ULONG am_ID;
	ULONG am_Freq;
};

/* where it streams from: the doc's timeline, first .. end - 1 */
struct AhiSpan {
	ULONG as_First, as_End;
	const struct AhiMode *as_Mode;
};

/* the signal the hook sends, taken at the start: the window's Wait()
   mask is made once */
BOOL Ahi_Open(void);
BOOL Ahi_There(void);			/* ahi.device opens (asked once) */
ULONG Ahi_Signal(void);			/* to Wait() for; 0 without Ahi_Open */
UWORD Ahi_Start(struct Doc *d, const struct AhiSpan *as);
/* the signal answered: a piece of the free buffer filled (queued once
   full), the end seen: TRUE while it plays */
BOOL Ahi_Tick(const struct FeedStep *give);
/* between the signals (the play head's beat): a piece more of the free
   buffer (fs_Piece frames, kept for the signals' pieces too), queued once
   full; the rest at once when its time runs short (the sound first) */
void Ahi_Feed(struct FeedStep *fs);
/* the frames the play has filled so far: copied from fs_Have, read */
struct AhiCounts {
	ULONG ac_Copied, ac_Read;
	ULONG ac_Differ;		/* copied frames not as read (Ahi_CheckCopies()) */
};
void Ahi_Counts(struct AhiCounts *ac);
/* each copied piece read from the file as well and compared (the tests':
   twice the reading) -- from the next Ahi_Start() on */
void Ahi_CheckCopies(BOOL on);
ULONG Ahi_Frame(void);			/* the frame playing about now */
BOOL Ahi_Playing(void);
void Ahi_Stop(void);			/* AHI and its device given back */
void Ahi_Close(void);			/* the signal too */
/* AHI's mode requester over w: TRUE and am filled when one was chosen */
BOOL Ahi_Request(struct Window *w, struct AhiMode *am);
/* the mode's name into buf (size): FALSE without one */
struct AhiName {
	const struct AhiMode *an_Mode;
	STRPTR an_Buf;
	ULONG an_Size;
};
BOOL Ahi_ModeName(const struct AhiName *an);

#endif
