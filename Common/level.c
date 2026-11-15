/*
 * level.c -- peaks in dB. A table, no arithmetic: the meters ask 50 times
 * a second, and a 68000 divides slowly.
 */
#include "level.h"

/* thr[k]: the smallest peak that rounds to -k dB or louder, that is
 * ceil(32767 * 10^(-(k + 0.5) / 20)) */
#define THRESHOLDS 91

static const UWORD thr[THRESHOLDS] = {
	30935, 27570, 24572, 21900, 19519, 17396, 15504, 13818, 12316, 10976,
	9783, 8719, 7771, 6926, 6173, 5501, 4903, 4370, 3895, 3471,
	3094, 2757, 2458, 2190, 1952, 1740, 1551, 1382, 1232, 1098,
	979, 872, 778, 693, 618, 551, 491, 437, 390, 348,
	310, 276, 246, 219, 196, 174, 156, 139, 124, 110,
	98, 88, 78, 70, 62, 56, 50, 44, 39, 35,
	31, 28, 25, 22, 20, 18, 16, 14, 13, 11,
	10, 9, 8, 7, 7, 6, 5, 5, 4, 4,
	4, 3, 3, 3, 2, 2, 2, 2, 2, 2,
	1
};

LONG Level_Db(ULONG peak)
{
	UWORD lo = 0, hi = THRESHOLDS - 1, mid;

	if (peak == 0) return LEVEL_NONE;
	/* the first k with peak >= thr[k]; thr falls, thr[last] is 1 */
	while (lo < hi) {
		mid = (UWORD) ((lo + hi) / 2);
		if (peak >= thr[mid]) hi = mid;
		else lo = (UWORD) (mid + 1);
	}
	return -(LONG) lo;
}

ULONG Level_Peak(LONG db)
{
	if (db > 0) return LEVEL_FULL + 1;
	if (db < LEVEL_FLOOR) return 1;
	return thr[-db];
}

ULONG Level_Louder(ULONG look, BOOL mono)
{
	ULONG left = look >> 16, right = look & 0xFFFFUL;

	if (mono || left >= right) return left;
	return right;
}

/* round(100 * 10^(h / 40)) for h = LEVEL_GAIN_MIN .. LEVEL_GAIN_MAX */
#define GAIN_STEPS	(LEVEL_GAIN_MAX - LEVEL_GAIN_MIN + 1)

static const UWORD gain_pct[GAIN_STEPS] = {
	25, 27, 28, 30, 32, 33, 35, 38, 40, 42, 45, 47, 50, 53, 56, 60, 63, 67, 71, 75,
	79, 84, 89, 94, 100, 106, 112, 119, 126, 133, 141, 150, 158, 168, 178, 188, 200,
	211, 224, 237, 251, 266, 282, 299, 316, 335, 355, 376, 398, 422, 447, 473, 501,
	531, 562, 596, 631, 668, 708, 750, 794, 841, 891, 944, 1000, 1059, 1122, 1189,
	1259, 1334, 1413, 1496, 1585, 1679, 1778, 1884, 1995, 2113, 2239, 2371, 2512,
	2661, 2818, 2985, 3162, 3350, 3548, 3758, 3981, 4217, 4467, 4732, 5012, 5309,
	5623, 5957, 6310
};

ULONG Level_GainPercent(LONG halfdb)
{
	if (halfdb < LEVEL_GAIN_MIN) halfdb = LEVEL_GAIN_MIN;
	if (halfdb > LEVEL_GAIN_MAX) halfdb = LEVEL_GAIN_MAX;
	return gain_pct[halfdb - LEVEL_GAIN_MIN];
}

LONG Level_GainHalfDb(ULONG percent)
{
	UWORD i;

	if (percent <= gain_pct[0]) return LEVEL_GAIN_MIN;
	if (percent >= gain_pct[GAIN_STEPS - 1]) return LEVEL_GAIN_MAX;
	/* the step below, then the nearer of it and the next -- nearer as
	 * a ratio: past their geometric mean, p * p > a * b */
	for (i = 0; i + 1 < GAIN_STEPS && gain_pct[i + 1] <= percent; i++) ;
	if (i + 1 < GAIN_STEPS && percent * percent > (ULONG) gain_pct[i] * gain_pct[i + 1]) i++;
	return (LONG) i + LEVEL_GAIN_MIN;
}
