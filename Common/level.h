/*
 * level.h -- a peak as the chip reports it (0..32767), in dB below full
 * scale. Pure: host-tested (test/host/test_level.c).
 */
#ifndef PRISMAREC_LEVEL_H
#define PRISMAREC_LEVEL_H

#include <exec/types.h>

#define LEVEL_FULL	32767UL		/* the largest peak: 0 dB */
#define LEVEL_FLOOR	-90		/* the lowest dB a peak of 1 gives */
#define LEVEL_NONE	-100		/* a peak of 0: nothing at all */
#define LEVEL_OVER	32391UL		/* -0.1 dB and up: the meters' OVER */

/* The peak in whole dB, rounded: 0 .. LEVEL_FLOOR, LEVEL_NONE for 0 */
LONG Level_Db(ULONG peak);

/* The smallest peak that reads db or more (0 .. LEVEL_FLOOR); db above
 * 0 gives LEVEL_FULL + 1 (never reached), below the floor 1 */
ULONG Level_Peak(LONG db);

/* A look (left << 16 | right) to one peak: the louder side, or the left
 * one alone for a mono recording */
ULONG Level_Louder(ULONG look, BOOL mono);

/* The input gain in half-dB steps, as the Level fader sets it, and the
 * percent the settings keep it in (100 = 0 dB; the library's 1024 is
 * percent * 1024 / 100). -12 dB to +36 dB, the chip's 64x near the top. */
#define LEVEL_GAIN_MIN	-24		/* half-dB: -12 dB */
#define LEVEL_GAIN_MAX	72		/* +36 dB */

ULONG Level_GainPercent(LONG halfdb);	/* clamped to the range */
LONG Level_GainHalfDb(ULONG percent);	/* the nearest step */

#endif
