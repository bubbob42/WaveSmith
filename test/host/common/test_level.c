/*
 * test_level.c -- level.c against the exact dB (math.h on the host)
 */
#include <math.h>

#include "check.h"
#include "level.h"

static LONG exact_db(ULONG peak)
{
	return (LONG) floor(20.0 * log10((double) peak / 32767.0) + 0.5);
}

int main(void)
{
	ULONG p;
	LONG db, prev = 0;
	int wrong = 0;

	/* every peak: rounded like the exact value, never rising */
	for (p = 1; p <= LEVEL_FULL; p++) {
		db = Level_Db(p);
		if (db != exact_db(p) && !(exact_db(p) < LEVEL_FLOOR && db == LEVEL_FLOOR)) wrong++;
		if (p > 1 && db < prev) wrong++;
		prev = db;
	}
	CHECK(wrong == 0, "every peak rounds to the exact dB and the dB never falls as the peak rises");

	CHECK(Level_Db(0) == LEVEL_NONE, "0 is nothing at all");
	CHECK(Level_Db(LEVEL_FULL) == 0, "full scale is 0 dB");
	CHECK(Level_Db(1) == -90, "a peak of 1 is -90 dB");
	CHECK(Level_Db(16384) == -6, "half scale is -6 dB");
	CHECK(Level_Db(30935) == 0 && Level_Db(30934) == -1, "the 0 dB boundary at -0.5 dB");
	CHECK(Level_Db(100000) == 0, "above full scale stays 0 dB");

	/* Level_Peak: the inverse at every dB */
	wrong = 0;
	for (db = 0; db >= LEVEL_FLOOR; db--) {
		p = Level_Peak(db);
		if (Level_Db(p) < db) wrong++;
		if (p > 1 && Level_Db(p - 1) >= db) wrong++;
	}
	CHECK(wrong == 0, "Level_Peak gives the smallest peak of each dB");
	CHECK(Level_Peak(1) == LEVEL_FULL + 1, "above 0 dB: never reached");
	CHECK(Level_Peak(-120) == 1, "below the floor: any peak");

	CHECK(Level_Louder(0x10002000UL, FALSE) == 0x2000, "the louder side: right");
	CHECK(Level_Louder(0x30002000UL, FALSE) == 0x3000, "the louder side: left");
	CHECK(Level_Louder(0x10002000UL, TRUE) == 0x1000, "mono: the left one");
	CHECK(Level_Louder(0x00000000UL, FALSE) == 0, "silence");

	/* the gain steps: each half dB as exactly as a whole percent can,
	 * and back */
	wrong = 0;
	for (db = LEVEL_GAIN_MIN; db <= LEVEL_GAIN_MAX; db++) {
		p = Level_GainPercent(db);
		if (fabs(20.0 * log10(p / 100.0) - db / 2.0) > 0.18) wrong++;
		if (Level_GainHalfDb(p) != db) wrong++;
		if (db > LEVEL_GAIN_MIN && p <= Level_GainPercent(db - 1)) wrong++;
	}
	CHECK(wrong == 0, "every half-dB step: its percent within 0.18 dB, and back");
	CHECK(Level_GainPercent(0) == 100 && Level_GainPercent(12) == 200 && Level_GainPercent(72) == 6310,
	      "0 dB = 100 %, +6 dB = 200 %, +36 dB = 6310 %");
	CHECK(Level_GainPercent(-100) == 25 && Level_GainPercent(100) == 6310, "clamped to -12 .. +36 dB");
	wrong = 0;
	for (p = 1; p <= 70000; p++) {
		db = Level_GainHalfDb(p);
		/* the nearest step, as a ratio */
		if (db > LEVEL_GAIN_MIN && db < LEVEL_GAIN_MAX &&
		    (fabs(log10((double) p / Level_GainPercent(db))) > fabs(log10((double) p / Level_GainPercent(db - 1))) + 1e-12 ||
		     fabs(log10((double) p / Level_GainPercent(db))) > fabs(log10((double) p / Level_GainPercent(db + 1))) + 1e-12))
			wrong++;
	}
	CHECK(wrong == 0, "any percent gives the nearest step");
	CHECK(Level_GainHalfDb(1) == LEVEL_GAIN_MIN && Level_GainHalfDb(100000) == LEVEL_GAIN_MAX, "and the ends");

	return CHECK_DONE("test_level");
}
