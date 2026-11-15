#ifndef WAVESMITH_PLAY_H
#define WAVESMITH_PLAY_H

/*
 * play.h -- listening on the card (concept D3, the user's: "AHI is too
 * costly, the card can play wav directly"): prisma.library's memory
 * player fed the WAV's header (its sizes the span's) and exactly the
 * span's frames of the timeline, read from a handle of its own -- so
 * "Listen at Out" stops where Save will cut, and a cut is heard as Save
 * will write it. The timeline is the doc's, borrowed while it plays: no
 * change to it before Play_Stop(). The card is taken only while it plays
 * (as PrismaRec does) and given back at the end.
 *
 * Or through AHI (ahiplay.h; Settings, Play through): Automatic plays on
 * the card when there is one and it is free, else through AHI (the owner
 * kept: Play_Owner() names who has the card); Card only; AHI only.
 */

#include <exec/types.h>

#include "doc.h"

#define PLAY_OK		0
#define PLAY_NOLIB	1	/* no prisma.library 3 */
#define PLAY_BUSY	2	/* another program has the card: Play_Owner() */
#define PLAY_NOCARD	3
#define PLAY_NOPLAYER	4	/* no memory for the player or its buffers */
#define PLAY_REFUSED	5	/* the player said no */
#define PLAY_FILE	6	/* the file cannot be read */
/* and ahiplay.h's AHIPLAY_#? */

/* Settings, Play through */
#define PLAY_WAY_AUTO	0
#define PLAY_WAY_CARD	1
#define PLAY_WAY_AHI	2

/* what it plays through now (Play_Through) */
#define PLAY_THROUGH_CARD	0	/* the card (or the tests' player) */
#define PLAY_THROUGH_AHI	1

struct PlayWay {
	UWORD pw_Way;			/* PLAY_WAY_#? */
	ULONG pw_AhiID, pw_AhiFreq;	/* AHI's mode, its rate (0, 0: AHI's default) */
};

/* the timeline's frames to hear: first .. end - 1 */
struct PlaySpan {
	ULONG ps_First;
	ULONG ps_End;
};

UWORD Play_Open(void);			/* PLAY_OK (prisma.library opened) or PLAY_NOLIB */
void Play_Close(void);
void Play_SetWay(const struct PlayWay *pw);	/* for the plays from now on */
BOOL Play_Any(void);			/* anything to play through at all: the card, AHI */
BOOL Play_There(void);			/* the card's library, or AHI, as the way allows */
UWORD Play_Through(void);		/* PLAY_THROUGH_#? of the play going on (or last) */
ULONG Play_Signals(void);		/* to Wait() for */
UWORD Play_Start(struct Doc *d, const struct PlaySpan *ps);
/* a piece of AHI's next sound (Play_Tick, Play_Feed): in its frames, out
   what came */
struct FeedStep {
	ULONG fs_Piece;			/* in: frames a piece, 0 the most (16384) */
	/* in: samples read already (the gadget's, the read-ahead's; NULL
	   none), of this call only -- what they hold is copied, not read
	   from the file again */
	const struct WaveSamples *fs_Have[2];
	ULONG fs_Fed;			/* out: the frames filled */
	BOOL fs_Behind;			/* out: short of where it should be by now --
					   another piece wanted at once */
};
/* between the player's signals (the play head's beat): AHI's next sound
   filled a piece further; the card's player needs nothing (fs_Fed 0) */
void Play_Feed(struct FeedStep *fs);
/* the player's signals answered: more queued (AHI: a piece, copied from
   fs_Have where they hold it; fs NULL: read), the end seen; TRUE while
   it plays */
BOOL Play_Tick(const struct FeedStep *fs);
ULONG Play_Frame(void);			/* the frame playing about now */
BOOL Play_Playing(void);
/* a level (concept 3.2) left out of what plays: the processor is too slow
   to compute it in time (a 68000 or 68010, F10) -- what is heard is the
   sound as recorded; Save computes every level on any processor */
BOOL Play_AsRecorded(void);
void Play_Stop(void);			/* the card given back */
CONST_STRPTR Play_Owner(void);		/* after PLAY_BUSY, or AHI for a card in use: "" else */

#endif
