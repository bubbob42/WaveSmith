/*
 * test_trimplan.c -- trimplan.c: the concept's single, look by look (40 ms,
 * 1764 frames at 44.1 kHz), and what WaveSmith must suggest for it:
 *
 *   look     0   lead-in: groove noise (level 60 dB down, peak 58), clicks
 *                (peak 20 down) at looks 20, 35 and 50 (50 over two looks)
 *          103   music (level 12, peak 10) to look 5370 (214.8 s)
 *         5370   run-out: noise, the groove's click every 45 looks (1.8 s)
 *                at 5390, 5435, 5480, 5525
 *         5540   the end (221.6 s)
 *
 * gapfind's rules: In 13 looks (0.5 s) before the music, look 90 = frame
 * 158,760 (3.6 s); Out 50 looks (2 s) after it, look 5420 = frame
 * 9,560,880 (3:36.8); the threshold learned 6 dB above the noise: 54.
 */
#include <string.h>

#include "check.h"
#include "trimplan.h"

#define LOOKS	5540UL
#define LF	1764UL

static UBYTE level[LOOKS], peak[LOOKS];

static void seg(ULONG a, ULONG b, UBYTE l, UBYTE p)
{
	ULONG k;

	for (k = a; k < b; k++) {
		level[k] = l;
		peak[k] = p;
	}
}

int main(void)
{
	static struct TrimPlan tp;
	struct TrimLooks tl;
	UWORD i, ticks = 0, silences = 0;

	seg(0, 103, 60, 58);
	seg(103, 5370, 12, 10);
	seg(5370, LOOKS, 60, 58);
	peak[20] = peak[35] = peak[50] = peak[51] = 20;
	peak[5390] = peak[5435] = peak[5480] = peak[5525] = 22;
	tl.tl_Level = level;
	tl.tl_Peak = peak;
	tl.tl_Count = LOOKS;
	tl.tl_LookFrames = LF;
	tl.tl_Frames = LOOKS * LF;
	tl.tl_Rate = 44100;
	tl.tl_LeadMs = 500;
	tl.tl_TailMs = 2000;
	CHECK(TrimPlan_Make(&tl, &tp), "music found");
	CHECK(tp.tp_Threshold == 54, "threshold 6 dB above the groove's noise");
	CHECK(tp.tp_MusicStart == 103 * LF && tp.tp_MusicEnd == 5370 * LF, "the music's ends");
	CHECK(tp.tp_In == 158760UL, "In: 0.5 s before the music (3.6 s)");
	CHECK(tp.tp_Out == 9560880UL, "Out: 2 s after it (3:36.8)");
	CHECK(tp.tp_LeadClicks == 3, "three clicks in the lead-in (one over two looks)");
	CHECK(tp.tp_TailClicks == 4 && tp.tp_PeriodMs == 1800, "four in the run-out, every 1.8 s");
	for (i = 0; i < tp.tp_RegionCount; i++) {
		if (tp.tp_Region[i].wrg_Kind == WAVEREGION_CLICK && tp.tp_Region[i].wrg_Style == WAVEREGION_TICK) ticks++;
		if (tp.tp_Region[i].wrg_Kind == WAVEREGION_SILENCE) silences++;
	}
	CHECK(ticks == 7 && silences == 2, "regions: two silences, seven click ticks");
	CHECK(tp.tp_Region[0].wrg_First == 0 && tp.tp_Region[0].wrg_End == 103 * LF, "the lead-in's silence");

	/* no lead-in at all: music from the first look, Out still 2 s after */
	seg(0, 103, 12, 10);
	CHECK(TrimPlan_Make(&tl, &tp) && tp.tp_In == 0 && tp.tp_LeadClicks == 0, "no lead-in: In at 0");
	/* no music */
	seg(0, LOOKS, 60, 58);
	CHECK(!TrimPlan_Make(&tl, &tp) && tp.tp_In == 0 && tp.tp_Out == LOOKS * LF && tp.tp_RegionCount == 0, "no music: the whole file");
	/* nothing to measure */
	tl.tl_Count = 0;
	CHECK(!TrimPlan_Make(&tl, &tp), "no looks");
	return CHECK_DONE("test_trimplan");
}
