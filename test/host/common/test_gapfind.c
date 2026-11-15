/*
 * test_gapfind.c -- gapfind.c: a record side built from its parts, look by
 * look (40 ms, 25 a second), and the tracks it must give, worked out by
 * hand from how the side is built:
 *
 *   look     0   lead-in, 3 s of groove noise (60 dB down), two clicks
 *           75   A, 40 s of music (12 dB down)
 *         1075   a gap of 2 s, a click of one look and one of two in it
 *         1125   B, 35 s
 *         2000   a gap of 1 s: too short
 *         2025   C, 40 s
 *         3025   a gap of 3 s
 *         3100   D, 31 s
 *         3875   lead-out, 6 s, a click of two looks
 *         4025   the end
 *
 * Defaults: threshold 48 dB, gaps of 1.5 s (38 looks), tracks of 30 s
 * (750), lead 0.5 s (13), tail 2 s (50). So: A from 62 (75 - 13) to 1112
 * (1125 - 13), B+C to 3087, D to 3925 (3875 + 50).
 */
#include <string.h>

#include "check.h"
#include "gapfind.h"
#include "levelmap.h"

#define NOISE	60
#define MUSIC	12

static UBYTE map[8000];
static ULONG n;
static struct GapResult gr;

static void seg(ULONG looks, UBYTE b)
{
	while (looks--) map[n++] = b;
}

/* the side above; d the length of D in looks */
static void side(ULONG d)
{
	n = 0;
	seg(75, NOISE);
	map[12] = 8;				/* the needle */
	map[50] = 9;
	seg(1000, MUSIC);
	seg(50, NOISE);
	map[1090] = 10;				/* crackle in the gap */
	map[1100] = map[1101] = 11;
	seg(875, 14);
	seg(25, NOISE);
	seg(1000, MUSIC);
	seg(75, NOISE);
	seg(d, 15);
	seg(150, NOISE);
	map[n - 40] = map[n - 39] = 9;		/* the run-out groove's click */
}

static void scan(struct GapScan *gs)
{
	GapFind_Defaults(gs, 40);
	gs->gs_Looks = map;
	gs->gs_Count = n;
}

static int track_is(UWORD t, ULONG start, ULONG end)
{
	return gr.gr_Tracks > t && gr.gr_Track[t].gt_Start == start && gr.gr_Track[t].gt_End == end;
}

static void test_side(void)
{
	struct GapScan gs;

	side(775);
	scan(&gs);
	CHECK(gs.gs_Threshold == 48 && gs.gs_MinGapMs == 1500 && gs.gs_MinTrackMs == 30000, "the defaults");
	CHECK(GapFind_Tracks(&gs, &gr) == 3, "the side: three tracks (the 1 s gap too short)");
	CHECK(gr.gr_MusicStart == 75 && gr.gr_MusicEnd == 3875, "  the music from 75 to 3875, clicks before and after not");
	CHECK(track_is(0, 62, 1112), "  A from 0.5 s before its music to 0.5 s before B's");
	CHECK(track_is(1, 1112, 3087), "  B and C, the 1 s gap inside");
	CHECK(track_is(2, 3087, 3925), "  D to 2 s after the last music");

	gs.gs_KeepEnds = TRUE;
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 3 && track_is(0, 0, 1112) && track_is(2, 3087, n), "KeepEnds: the side's own ends");

	side(500);				/* D 20 s: too short */
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 2 && track_is(1, 1112, 3100 + 500 + 50), "a last track of 20 s joins the one before");

	side(775);
	scan(&gs);
	gs.gs_MinGapMs = 1000;			/* the 1 s gap counts now: 25 looks */
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 4 && track_is(1, 1112, 2012) && track_is(2, 2012, 3087), "MinGap 1 s: B and C apart");

	side(775);
	scan(&gs);
	gs.gs_Threshold = 65;			/* the groove noise is loud now */
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 1 && gr.gr_MusicStart == 0, "threshold 65 dB: the noise is music, one track");
}

static void test_gaps(void)
{
	struct GapScan gs;

	/* a burst of five looks (200 ms) in A's gap: more than a click */
	side(775);
	map[1098] = map[1099] = map[1100] = map[1101] = map[1102] = 10;
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 2 && track_is(0, 62, 3087), "a 200 ms burst splits the gap in two too short");

	/* three looks: still a click (away from the gap's own clicks) */
	side(775);
	map[1080] = map[1081] = map[1082] = 10;
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 3 && track_is(0, 62, 1112), "a 120 ms click does not");

	/* digital silence between the tracks (255) */
	side(775);
	memset(map + 1075, LEVELMAP_NONE, 50);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 3 && track_is(0, 62, 1112), "nothing at all (255) is silence");

	/* a short gap counted (MinGap 0.5 s: 13 looks): the split no earlier
	 * than its middle */
	n = 0;
	seg(800, MUSIC);
	seg(15, NOISE);
	seg(800, MUSIC);
	scan(&gs);
	gs.gs_MinGapMs = 500;
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 2 && track_is(0, 0, 807), "a gap of 15 looks: split in its middle, not 13 before the music");

	/* a first track too short: no split there */
	n = 0;
	seg(75, NOISE);
	seg(500, MUSIC);
	seg(50, NOISE);
	seg(1000, MUSIC);
	seg(75, NOISE);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 1 && track_is(0, 62, 1625 + 50), "a first track of 22 s: the music runs on");

	/* nothing but silence; nothing at all */
	n = 0;
	seg(1000, NOISE);
	scan(&gs);
	CHECK(GapFind_Tracks(&gs, &gr) == 0, "silence only: no tracks");
	n = 0;
	seg(1000, LEVELMAP_NONE);
	scan(&gs);
	CHECK(GapFind_Tracks(&gs, &gr) == 0, "nothing at all: no tracks");
	n = 0;
	scan(&gs);
	CHECK(GapFind_Tracks(&gs, &gr) == 0, "no looks: no tracks");
}

static void test_many(void)
{
	struct GapScan gs;
	static UBYTE big[120 * 825 + 10];
	ULONG i;

	/* 120 tracks of 31 s, 2 s apart: 99, the last holding the rest */
	n = 0;
	for (i = 0; i < 120; i++) {
		memset(big + n, MUSIC, 775);
		n += 775;
		memset(big + n, NOISE, 50);
		n += 50;
	}
	GapFind_Defaults(&gs, 40);
	gs.gs_Looks = big;
	gs.gs_Count = n;
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == GAPFIND_MAXTRACKS, "120 tracks: 99 at most");
	CHECK(gr.gr_Track[98].gt_End == n - 50 + 50 && gr.gr_Track[97].gt_End == gr.gr_Track[98].gt_Start,
	      "  the last one holding the rest, every track where the one before ends");
}

static void test_learn(void)
{
	struct GapScan gs;

	side(775);
	scan(&gs);
	/* the lead-in: 62 looks before the guard; the needle's two clicks (8,
	 * 9), two more at 40: 4 of 62, under the tenth left out */
	map[30] = map[31] = 40;
	CHECK(GapFind_Learn(&gs) == NOISE - 6, "learned: 6 dB above the groove's 60 dB");

	map[20] = map[21] = map[22] = 40;	/* 7 of 62 at 40 dB or louder: over a tenth */
	CHECK(GapFind_Learn(&gs) == 0, "more than a tenth at 40 dB: no quiet groove to learn from");
	map[20] = map[21] = map[22] = map[30] = map[31] = 50;
	CHECK(GapFind_Learn(&gs) == 44, "more than a tenth at 50 dB: they set the level, 44");

	side(775);
	memset(map, LEVELMAP_NONE, 75);
	scan(&gs);
	CHECK(GapFind_Learn(&gs) == 0, "a lead-in of digital silence: nothing to learn");

	side(775);
	memset(map, 36, 75);
	scan(&gs);
	CHECK(GapFind_Learn(&gs) == 0, "a loud lead-in (36 dB down): no quiet groove");

	n = 0;
	seg(25, NOISE);				/* 1 s, 12 looks before the guard */
	seg(1000, MUSIC);
	scan(&gs);
	CHECK(GapFind_Learn(&gs) == 0, "a lead-in of 1 s: too short");
	n = 0;
	seg(1000, MUSIC);
	scan(&gs);
	CHECK(GapFind_Learn(&gs) == 0, "no lead-in at all");
}

static void test_quiet(void)
{
	struct GapScan gs;

	side(775);
	scan(&gs);
	CHECK(GapFind_Quiet(&gs) == NOISE - 6, "the quietest 1.5 s: the groove's 60 dB, 54");

	/* no lead-in: the side from A's first look */
	side(775);
	memmove(map, map + 75, n - 75);
	n -= 75;
	scan(&gs);
	CHECK(GapFind_Learn(&gs) == 0 && GapFind_Quiet(&gs) == NOISE - 6, "no lead-in: Learn none, Quiet from the gaps");
	gs.gs_Threshold = GapFind_Quiet(&gs);
	CHECK(GapFind_Tracks(&gs, &gr) == 3 && track_is(0, 0, 1112 - 75), "  and with it the same three tracks");

	/* the gaps digital silence, no noise anywhere */
	n = 0;
	seg(1000, MUSIC);
	seg(50, LEVELMAP_NONE);
	seg(1000, MUSIC);
	scan(&gs);
	CHECK(GapFind_Quiet(&gs) == 0, "digital silence only: nothing to learn");

	/* no gap at all, a quiet passage of 2 s at 40 dB down: not a groove */
	n = 0;
	seg(1000, MUSIC);
	seg(50, 40);
	seg(1000, MUSIC);
	scan(&gs);
	CHECK(GapFind_Quiet(&gs) == 0, "a quiet passage at 40 dB: too loud to be a groove's noise");

	n = 0;
	seg(20, NOISE);
	scan(&gs);
	CHECK(GapFind_Quiet(&gs) == 0, "fewer looks than a gap: nothing");
}

static void test_fades(void)
{
	struct GapScan gs;
	ULONG i;

	/* B fades in over its first 2 s (50 looks) from 59 dB down to 13,
	 * the threshold learned for this noise (60 dB: 54). Under it for its
	 * first 7 looks: music from 1132, which alone would split at 1119.
	 * Walked back over the looks louder than the noise less 2 dB (58):
	 * 57, 56, 55, 54 -- the fade from 1128, split 0.5 s before, 1115 */
	side(775);
	for (i = 0; i < 50; i++) map[1125 + i] = (UBYTE) (59 - (46 * i) / 49);
	scan(&gs);
	gs.gs_Threshold = 54;
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 3 && track_is(0, 62, 1115), "a fade-in from under the threshold: B from 0.5 s before it");

	/* a gap whose second half is a quiet passage above the noise (50 dB
	 * down, between the threshold, 48, and the noise less 2, 52): walked
	 * back over, but only to the gap's middle -- the split there */
	n = 0;
	seg(900, MUSIC);
	seg(20, NOISE);
	seg(30, 50);
	seg(900, MUSIC);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 2 && gr.gr_Track[0].gt_End == 925, "  walked back no further than the gap's middle");

	/* the last song fades out over 3 s into the lead-out, 15 dB down to
	 * 59: under the threshold from look 3856 (music to 3856, tail to
	 * 3906); walked on over the looks louder than 52 to 3863: the tail
	 * after that, 3913 */
	side(775);
	for (i = 0; i < 75; i++) map[3800 + i] = (UBYTE) (15 + (44 * i) / 74);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_Tracks == 3 && gr.gr_MusicEnd == 3856 && gr.gr_Track[2].gt_End == 3913,
	      "a fade-out: the tail after its end");
}

/* A side's ends (the user's rule, 2026-10-26): a burst up to 0.5 s (13
 * looks) with no music in the second (25 looks) after it -- before it, at
 * the end -- is a thump, not music; clicks in that second do not count */
static void thump_side(ULONG burst_at, ULONG burst, ULONG music_at)
{
	ULONG i;

	n = 0;
	seg(music_at, NOISE);
	for (i = 0; i < burst; i++) map[burst_at + i] = 30;
	map[24] = map[37] = map[50] = 9;		/* the lead-in's clicks, a look each */
	seg(1000, MUSIC);
	seg(150, NOISE);
}

static void test_thump(void)
{
	struct GapScan gs;
	ULONG i;

	/* the M4A's: the needle's thump 4 looks (one more than a click) */
	thump_side(10, 4, 105);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicStart == 105, "a 4-look thump with a quiet second after it: not music, the music at 105");
	/* the music half a second after the burst: it starts with the burst */
	thump_side(10, 4, 30);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicStart == 10, "a burst with music 0.4 s after it: the music starts with it");
	/* a burst longer than half a second: music */
	thump_side(10, 15, 105);
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicStart == 10, "a 0.6 s burst: music, though quiet follows");
	/* a burst with nothing loud after it at all stays the music */
	n = 0;
	seg(300, NOISE);
	for (i = 10; i < 14; i++) map[i] = 30;
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicStart == 10 && gr.gr_MusicEnd == 14, "a lone burst, nothing else loud: it is the music");
	/* the end: the needle lifted 4 looks, 2.4 s after the music */
	n = 0;
	seg(50, NOISE);
	seg(1000, MUSIC);
	seg(250, NOISE);
	for (i = 1110; i < 1114; i++) map[i] = 30;
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicStart == 50 && gr.gr_MusicEnd == 1050, "the needle lifted after the run-out: not music, the music ends at 1050");
	/* the same burst 0.4 s after the music: its end */
	for (i = 1110; i < 1114; i++) map[i] = NOISE;
	for (i = 1060; i < 1064; i++) map[i] = 30;
	scan(&gs);
	GapFind_Tracks(&gs, &gr);
	CHECK(gr.gr_MusicEnd == 1064, "a burst 0.4 s after the music: the music ends with it");
}

int main(void)
{
	test_side();
	test_thump();
	test_gaps();
	test_many();
	test_learn();
	test_quiet();
	test_fades();
	return CHECK_DONE("test_gapfind");
}
