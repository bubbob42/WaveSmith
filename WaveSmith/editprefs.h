/*
 * editprefs.h -- WaveSmith's settings (the Settings menu), and the text
 * they are kept as, WaveSmith.prefs: one "KEY=value" a line, ';' or '#'
 * starts a comment, unknown keys and bad values are passed over (the
 * default stays) -- PrismaRec's way (recprefs.h). Pure: host-tested
 * (test/host/wavesmith/test_editprefs.c); gui.c reads and writes the
 * file.
 */
#ifndef WAVESMITH_EDITPREFS_H
#define WAVESMITH_EDITPREFS_H

#include <exec/types.h>

#define EDITPREFS_NAME		"WaveSmith.prefs"
#define EDITPREFS_TEXTMAX	256	/* EditPrefs_Write(): the most it writes */
#define EDITPREFS_LEAD		500UL	/* ms: the defaults, trimplan.h's */
#define EDITPREFS_TAIL		2000UL
#define EDITPREFS_LEAD_MAX	10000UL	/* ms: In at most so long before the music */
#define EDITPREFS_TAIL_MAX	30000UL	/* ms: Out at most so long after it */
#define EDITPREFS_OVER		10	/* the overview's per cent of the two strips */
#define EDITPREFS_OVER_MIN	5
#define EDITPREFS_OVER_MAX	50
#define EDITPREFS_PLAY_AUTO	0	/* PLAY: as play.h's PLAY_WAY_#? */
#define EDITPREFS_PLAY_CARD	1
#define EDITPREFS_PLAY_AHI	2
#define EDITPREFS_SHAPES	3	/* FADESHAPE: as envelope.h's ENV_SHAPE_#? */
#define EDITPREFS_NORMALISE	10	/* NORMALISE: tenths of a dB below full scale (F7) */
#define EDITPREFS_NORMALISE_MAX	240	/* as low as a gain goes (envelope.h) */

struct EditPrefs {
	BOOL ep_Snap;		/* SNAP: In, Out and the cuts on zero crossings (YES) */
	ULONG ep_LeadMs;	/* LEAD: the suggestion's In before the music (500) */
	ULONG ep_TailMs;	/* TAIL: its Out after the music (2000) */
	BOOL ep_Backup;		/* BACKUP: Save keeps the old file as <name>.bak (NO) */
	BOOL ep_Icons;		/* ICONS: a new file gets the system's icon (YES) */
	UWORD ep_Overview;	/* OVERVIEW: the weight bar's place, per cent (10) */
	UWORD ep_Play;		/* PLAY: AUTO, CARD or AHI (AUTO) */
	ULONG ep_AhiID;		/* AHIMODE: AHI's audio ID (0: its default) */
	ULONG ep_AhiFreq;	/* AHIFREQ: the mode's rate (0: its default) */
	UWORD ep_FadeShape;	/* FADESHAPE: LINEAR, EVEN or SCURVE (EVEN, F2): the
				   fades the user makes (envelope.h's ENV_SHAPE_#?) */
	UWORD ep_Normalise;	/* NORMALISE: the level Normalise brings a part's
				   peak to, tenths of a dB below full scale (10) */
};

void EditPrefs_Defaults(struct EditPrefs *ep);
/* The settings in text (len bytes) over ep; the number taken */
UWORD EditPrefs_Read(struct EditPrefs *ep, CONST_STRPTR text, ULONG len);
/* All of them into buf, size EDITPREFS_TEXTMAX at least (else 0): the
 * length written */
ULONG EditPrefs_Write(const struct EditPrefs *ep, STRPTR buf, ULONG size);

#endif
