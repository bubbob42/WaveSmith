/*
 * test_waveplan.c -- waveplan.c: the view, time, the ruler, the parts,
 * snapping, zero crossings, samples as columns, the scale, the signature.
 * The numbers are the concept's (Concept-WaveSmith-2026-10.md): a 22:41.3
 * side of 60,033,330 frames, a single of 3:41.6 at 44.1 kHz, its In at
 * 0:03.598 (frame 158,672), a view of 622 columns.
 */
#include <string.h>

#include "check.h"
#include "waveplan.h"

static struct WaveView v;

static void view(ULONG frames, ULONG top, ULONG zoom)
{
	memset(&v, 0, sizeof(v));
	v.wv_Frames = frames;
	v.wv_Top = top;
	v.wv_Zoom = zoom;
	v.wv_ZoomMin = 1;
	v.wv_Columns = 622;
}

static int is(const char *a, const char *b)
{
	return strcmp(a, b) == 0;
}

static void test_view(void)
{
	struct WaveRange r;

	view(60033330UL, 0, 1);
	CHECK(WaveView_ZoomAll(&v) == 96517UL, "the side: 96517 frames a column");
	WaveView_Fit(&v);
	CHECK(v.wv_Zoom == 96517UL && v.wv_Top == 0, "fit: all, from 0");
	CHECK(WaveView_Visible(&v) == 622UL * 96517UL, "visible: columns x zoom");

	view(60033330UL, 5000000UL, 1000);
	CHECK(WaveView_FrameAt(&v, 0) == 5000000UL && WaveView_FrameAt(&v, 621) == 5621000UL, "frame at a column");
	CHECK(WaveView_ColumnOf(&v, 5621999UL) == 621 && WaveView_ColumnOf(&v, 5622000UL) == -1, "column of the view's last frame, and past it");
	CHECK(WaveView_ColumnOf(&v, 4999999UL) == -1 && WaveView_ColumnOf(&v, WAVEFORM_NONE) == -1, "before the view, none");
	CHECK(WaveView_FrameAt(&v, -10) == 4990000UL, "left of the view");
	CHECK(WaveView_FrameAt(&v, -10000) == 5000000UL - 622000UL, "far left: a view's width at most");
	CHECK(WaveView_FrameAt(&v, 2000) == 5000000UL + 1244000UL, "far right: two views' widths at most");
	v.wv_Top = 60033330UL - 622000UL;
	CHECK(WaveView_FrameAt(&v, 700) == 60033330UL, "past the end: the end");
	v.wv_Top = 100;
	CHECK(WaveView_FrameAt(&v, -1) == 0, "before the start: 0");

	/* zoom about a frame: it keeps its column */
	view(60033330UL, 5000000UL, 1000);
	WaveView_Zoom(&v, 1, 5311000UL);
	CHECK(v.wv_Zoom == 500 && v.wv_Top == 5311000UL - 311UL * 500UL, "zoom in: half, the frame in its column");
	CHECK(WaveView_ColumnOf(&v, 5311000UL) == 311, "... column 311");
	WaveView_Zoom(&v, -3, 5311000UL);
	CHECK(v.wv_Zoom == 4000 && v.wv_Top == 5311000UL - 311UL * 4000UL, "zoom out by three");
	WaveView_Zoom(&v, -40, 5311000UL);
	CHECK(v.wv_Zoom == 96517UL && v.wv_Top == 0, "zoom out past all: all");
	view(1000, 0, 1);
	WaveView_Zoom(&v, 3, 500);
	CHECK(v.wv_Zoom == 1, "zoom in past 1: 1");
	view(1000000UL, 0, 1000);
	WaveView_Zoom(&v, 1, WAVEFORM_NONE);
	CHECK(v.wv_Zoom == 500 && WaveView_ColumnOf(&v, 311000UL) == 311, "no centre: the view's middle");

	/* show a range: 3 s around In, 10 % either side */
	view(9772560UL, 0, 15712);
	r.wr_First = 100000UL;
	r.wr_End = 232300UL;
	r.wr_Margin = 10;
	WaveView_Show(&v, &r);
	CHECK(v.wv_Zoom == 256 && v.wv_Top == 86770UL, "show: zoom 256 from 86770");
	CHECK(WaveView_ColumnOf(&v, 100000UL) >= 0 && WaveView_ColumnOf(&v, 232299UL) >= 0, "show: the range in view");

	/* clamp: a top too far */
	view(9772560UL, 9700000UL, 1000);
	WaveView_Clamp(&v);
	CHECK(v.wv_Top == 9772560UL - 622000UL, "clamp: the view ends at the end");
	v.wv_ZoomMax = 500;
	WaveView_Clamp(&v);
	CHECK(v.wv_Zoom == 500, "clamp: the widest zoom");

	/* follow the play head */
	view(9772560UL, 0, 1000);
	CHECK(!WaveView_Follow(&v, 300000UL, WAVEFORM_FOLLOW_PAGE), "in view: no page");
	CHECK(WaveView_Follow(&v, 700000UL, WAVEFORM_FOLLOW_PAGE) && v.wv_Top == 622000UL - 38000UL,
	      "past the edge: a page on, from the edge (the head 116,000 in)");
	CHECK(!WaveView_Follow(&v, 700000UL, WAVEFORM_FOLLOW_PAGE), "then in view");
	CHECK(WaveView_Follow(&v, 584000UL + 622000UL, WAVEFORM_FOLLOW_PAGE) && v.wv_Top == 584000UL + 584000UL,
	      "just past the next edge: the same step again");
	CHECK(WaveView_Follow(&v, 4000000UL, WAVEFORM_FOLLOW_PAGE) && v.wv_Top == 4000000UL - 38000UL,
	      "a jump beyond the next page: from the head");
	CHECK(WaveView_Follow(&v, 100000UL, WAVEFORM_FOLLOW_PAGE) && v.wv_Top == 100000UL - 38000UL, "a jump back: from the head");
	CHECK(WaveView_Follow(&v, 2000000UL, WAVEFORM_FOLLOW_CENTER) && v.wv_Top == 2000000UL - 311000UL, "centred");
	CHECK(!WaveView_Follow(&v, 2000000UL, WAVEFORM_FOLLOW_NONE), "none");
}

static void test_time(void)
{
	struct WaveClock c;
	char b[WAVEPLAN_TIMELEN];

	c.wc_Rate = 44100;
	c.wc_Origin = 0;
	c.wc_Format = WAVEFORM_TIME_AUTO;
	c.wc_Decimals = 3;
	CHECK(WavePlan_Time(&c, 158672UL, b) == 8 && is(b, "0:03.598"), "In: 0:03.598");
	WavePlan_Time(&c, 9560880UL, b);
	CHECK(is(b, "3:36.800"), "Out: 3:36.800");
	WavePlan_Time(&c, 158760000UL, b);
	CHECK(is(b, "1:00:00.000"), "an hour");
	c.wc_Decimals = 0;
	WavePlan_Time(&c, 158672UL, b);
	CHECK(is(b, "0:03"), "no decimals");
	c.wc_Decimals = 1;
	WavePlan_Time(&c, 158672UL, b);
	CHECK(is(b, "0:03.5"), "tenths");
	c.wc_Decimals = 3;
	c.wc_Origin = 44200;
	WavePlan_Time(&c, 100, b);
	CHECK(is(b, "-0:01.000"), "before the origin");
	c.wc_Origin = 0;
	c.wc_Format = WAVEFORM_TIME_FRAMES;
	WavePlan_Time(&c, 158672UL, b);
	CHECK(is(b, "158672"), "frames");
	c.wc_Format = WAVEFORM_TIME_SECONDS;
	WavePlan_Time(&c, 158672UL, b);
	CHECK(is(b, "3.598"), "seconds");
	c.wc_Format = WAVEFORM_TIME_AUTO;
	c.wc_Rate = 192000;
	WavePlan_Time(&c, 0xFFFFFFFFUL, b);
	CHECK(is(b, "6:12:49.621"), "4 G frames at 192 kHz: no wrap");
	/* and back: what the user types */
	c.wc_Rate = 44100;
	c.wc_Origin = 0;
	c.wc_Format = WAVEFORM_TIME_AUTO;
	CHECK(WavePlan_ParseTime(&c, "0:03.598") == 158671UL, "0:03.598");
	CHECK(WavePlan_ParseTime(&c, "3.598") == 158671UL, "3.598 seconds");
	CHECK(WavePlan_ParseTime(&c, "3:36.8") == 9560880UL, "3:36.8");
	CHECK(WavePlan_ParseTime(&c, "1:00:00") == 158760000UL, "an hour");
	CHECK(WavePlan_ParseTime(&c, " 0:01 ") == 44100UL, "blanks around");
	CHECK(WavePlan_ParseTime(&c, "0:0x") == WAVEFORM_NONE && WavePlan_ParseTime(&c, "") == WAVEFORM_NONE &&
	      WavePlan_ParseTime(&c, "1:2:3:4") == WAVEFORM_NONE, "no time");
	CHECK(WavePlan_ParseTime(&c, "-0:01") == WAVEFORM_NONE, "before 0");
	c.wc_Origin = 44200;
	CHECK(WavePlan_ParseTime(&c, "-0:01") == 100, "before the origin");
	c.wc_Origin = 0;
	c.wc_Format = WAVEFORM_TIME_FRAMES;
	CHECK(WavePlan_ParseTime(&c, "158672") == 158672UL && WavePlan_ParseTime(&c, "1:2") == WAVEFORM_NONE, "frames");
	c.wc_Format = WAVEFORM_TIME_AUTO;
	CHECK(WavePlan_ParseTime(&c, "99999999999") == WAVEFORM_NONE, "too long: no wrap");
	CHECK(WavePlan_MsToFrames(1, 44100) == 44 && WavePlan_MsToFrames(3600000UL, 192000) == 691200000UL, "ms to frames");
	CHECK(WavePlan_FramesToMs(9560880UL, 44100) == 216800UL && WavePlan_FramesToMs(5, 0) == 0, "frames to ms");
}

static void test_ruler(void)
{
	struct WaveRuler r;

	/* the single whole: labels "3:41", 8 pixels a character, 12 apart */
	view(9772560UL, 0, 1);
	WaveView_Fit(&v);
	CHECK(v.wv_Zoom == 15712, "the single: 15712 frames a column");
	r.wru_Rate = 44100;
	r.wru_CharWidth = 8;
	r.wru_Gap = 12;
	CHECK(WavePlan_Ruler(&v, &r) && r.wru_MajorMs == 30000UL && r.wru_Div == 3 && r.wru_Decimals == 0, "whole: every 30 s, 3 small ticks");
	CHECK(r.wru_Major == 1323000UL && r.wru_Minor == 441000UL, "... in frames");
	/* 3 s around In (zoom 213): every 0.5 s, "0:05.0" */
	view(9772560UL, 92600UL, 213);
	CHECK(WavePlan_Ruler(&v, &r) && r.wru_MajorMs == 500 && r.wru_Decimals == 1 && r.wru_Div == 5, "3 s: every 0.5 s, tenths");
	/* one frame a column: 2 ms steps at most */
	view(9772560UL, 92600UL, 1);
	CHECK(WavePlan_Ruler(&v, &r) && r.wru_MajorMs <= 2 && r.wru_Decimals == 3, "a frame a column: ms");
	v.wv_Columns = 10;
	r.wru_CharWidth = 400;
	CHECK(!WavePlan_Ruler(&v, &r), "nothing fits: FALSE");
}

static void test_parts(void)
{
	struct WaveHit h;
	LONG mk[1];

	memset(&h, 0, sizeof(h));
	h.wh_Width = 622;
	h.wh_Height = 134;
	h.wh_In = 100;
	h.wh_Out = 400;
	h.wh_SelStart = h.wh_SelEnd = -1;
	h.wh_BoxLeft = h.wh_BoxRight = -1;
	h.wh_Grab = 3;
	h.wh_RulerTop = 0;
	h.wh_RulerBottom = 10;
	CHECK(WavePlan_Part(&h, 102, 50) == WAVEPART_TRIMIN && WavePlan_Part(&h, 103, 50) == WAVEPART_TRIMIN, "In, within 3");
	CHECK(WavePlan_Part(&h, 104, 50) == WAVEPART_WAVE, "the sound beside it");
	CHECK(WavePlan_Part(&h, 398, 5) == WAVEPART_TRIMOUT, "Out, even in the ruler's lines");
	CHECK(WavePlan_Part(&h, 200, 5) == WAVEPART_RULER, "the ruler");
	CHECK(WavePlan_Part(&h, -1, 50) == WAVEPART_NONE && WavePlan_Part(&h, 10, 134) == WAVEPART_NONE, "outside");
	/* the flags: In's right of its line, Out's left of it, on lines 10..20 */
	h.wh_FlagTop = 10;
	h.wh_FlagBottom = 21;
	h.wh_InFlagX0 = 101;
	h.wh_InFlagX1 = 118;
	h.wh_OutFlagX0 = 374;
	h.wh_OutFlagX1 = 399;
	CHECK(WavePlan_Part(&h, 115, 15) == WAVEPART_TRIMIN, "In's flag grabs In");
	CHECK(WavePlan_Part(&h, 380, 20) == WAVEPART_TRIMOUT, "Out's flag grabs Out");
	CHECK(WavePlan_Part(&h, 115, 21) == WAVEPART_WAVE && WavePlan_Part(&h, 119, 15) == WAVEPART_WAVE,
	      "beside a flag, below it: not the handle");
	h.wh_FlagTop = h.wh_FlagBottom = 0;
	h.wh_In = h.wh_Out = 300;
	CHECK(WavePlan_Part(&h, 299, 50) == WAVEPART_TRIMOUT, "both on one column: left of it Out");
	CHECK(WavePlan_Part(&h, 300, 50) == WAVEPART_TRIMIN && WavePlan_Part(&h, 301, 50) == WAVEPART_TRIMIN, "... on it and right of it In");
	h.wh_In = h.wh_Out = -1;
	mk[0] = 200;
	h.wh_Markers = mk;
	h.wh_MarkerCount = 1;
	CHECK(WavePlan_Part(&h, 201, 50) == (WAVEPART_MARKER | (0UL << 8)), "a marker");
	h.wh_SelStart = 250;
	h.wh_SelEnd = 350;
	CHECK(WavePlan_Part(&h, 251, 50) == WAVEPART_SELSTART && WavePlan_Part(&h, 349, 50) == WAVEPART_SELEND, "the selection's edges");
	CHECK(WavePlan_Part(&h, 300, 50) == WAVEPART_WAVE, "inside the selection");
	h.wh_SelEnd = -1;
	CHECK(WavePlan_Part(&h, 251, 50) == WAVEPART_SELSTART, "an edge while the other is out of view");
	h.wh_SelStart = -1;
	h.wh_SelEnd = 350;
	CHECK(WavePlan_Part(&h, 349, 50) == WAVEPART_SELEND, "... and the other way round");
	h.wh_SelStart = 250;
	h.wh_BoxLeft = 10;
	h.wh_BoxRight = 20;
	CHECK(WavePlan_Part(&h, 15, 5) == WAVEPART_BOX && WavePlan_Part(&h, 30, 5) == WAVEPART_BOXOUT, "the overview's box");
}

static void test_snap(void)
{
	struct WaveSnap s;
	ULONG t[2];

	t[0] = 1000;
	t[1] = 5000;
	s.wsn_Flags = WAVEFORM_SNAP_MARKERS | WAVEFORM_SNAP_STEP;
	s.wsn_TargetCount = 2;
	s.wsn_Targets = t;
	s.wsn_Distance = 100;
	s.wsn_Step = 441;
	CHECK(WavePlan_Snap(&s, 1050) == 1000, "pulled to a target");
	CHECK(WavePlan_Snap(&s, 1200) == 1323 && WavePlan_Snap(&s, 1500) == 1323, "else to the nearest step");
	CHECK(WavePlan_Snap(&s, 0xFFFFFFF0UL) == 0xFFFFFFF0UL - 0xFFFFFFF0UL % 441, "near 4 G: down, no wrap");
	s.wsn_Flags = 0;
	CHECK(WavePlan_Snap(&s, 1050) == 1050, "no flags: as it is");
}

static void test_zero(void)
{
	static WORD d[2000];
	struct WaveSamples s;
	struct WaveZero z;
	ULONG f;

	/* stereo, the sum (f - 1500) x 20: zero at 1500 */
	for (f = 0; f < 1000; f++) d[f * 2] = d[f * 2 + 1] = (WORD) (((LONG) f + 1000 - 1500) * 10);
	s.wsm_First = 1000;
	s.wsm_Frames = 1000;
	s.wsm_Channels = 2;
	s.wsm_Data = d;
	z.wz_Want = 1480;
	z.wz_Within = 50;
	CHECK(WavePlan_ZeroCrossing(&s, &z) && z.wz_Found == 1500, "the crossing nearby");
	z.wz_Want = 1400;
	CHECK(!WavePlan_ZeroCrossing(&s, &z) && z.wz_Found == 1400, "none within: as it was");
	z.wz_Want = 1600;
	z.wz_Within = 200;
	CHECK(WavePlan_ZeroCrossing(&s, &z) && z.wz_Found == 1500, "from the other side");
	/* a sign change every 7 frames: the nearest */
	for (f = 0; f < 1000; f++) d[f * 2] = d[f * 2 + 1] = (WORD) ((f / 7) % 2 ? -300 : 300);
	z.wz_Want = 1000 + 17;
	z.wz_Within = 10;
	CHECK(WavePlan_ZeroCrossing(&s, &z) && z.wz_Found == 1000 + 14, "the nearest of many");
	z.wz_Want = 500;
	CHECK(!WavePlan_ZeroCrossing(&s, &z), "outside the samples: none");
}

static void test_samples(void)
{
	static WORD d[100];
	static BYTE o[64];
	struct WaveSamples s;
	struct WavePeakMsg q;
	ULONG f;

	for (f = 0; f < 100; f++) d[f] = (WORD) ((LONG) f * 300 - 15000);
	s.wsm_First = 0;
	s.wsm_Frames = 100;
	s.wsm_Channels = 1;
	s.wsm_Data = d;
	q.wpm_First = 0;
	q.wpm_Step = 10;
	q.wpm_Count = 10;
	q.wpm_Out = o;
	CHECK(WavePlan_FromSamples(&s, &q) == 10, "ten columns");
	CHECK(o[0] == -59 && o[1] == -49, "the first column: -15000 .. -12300");
	q.wpm_First = 60;
	q.wpm_Step = 30;
	q.wpm_Count = 3;
	CHECK(WavePlan_FromSamples(&s, &q) == 2, "the last column cut by the end, then none");
	s.wsm_First = 50;
	q.wpm_First = 0;
	CHECK(WavePlan_FromSamples(&s, &q) == 0, "before the samples: none");
}

/* the columns as the per-sample top bytes made them before (the least and
   greatest top byte of every sample): what the class drew must not move */
static ULONG top_bytes(const struct WaveSamples *s, const struct WavePeakMsg *q, BYTE *out)
{
	ULONG i, f = q->wpm_First, n, k, end = s->wsm_First + s->wsm_Frames;
	UWORD c;
	BYTE mn, mx, b;

	for (i = 0; i < q->wpm_Count; i++, f += q->wpm_Step) {
		if (f < s->wsm_First || f >= end) break;
		n = q->wpm_Step > end - f ? end - f : q->wpm_Step;
		for (c = 0; c < s->wsm_Channels; c++) {
			mn = 127;
			mx = -128;
			for (k = 0; k < n; k++) {
				b = (BYTE) ((LONG) (((ULONG) ((LONG) s->wsm_Data[(f - s->wsm_First + k) * s->wsm_Channels + c] + 32768L)) >> 8) -
					    128L);
				if (b < mn) mn = b;
				if (b > mx) mx = b;
			}
			*out++ = mn;
			*out++ = mx;
		}
	}
	return i;
}

static void test_samples_same(void)
{
	static WORD d[3 * 5000];
	static BYTE o[2 * 3 * 600], r[2 * 3 * 600];
	struct WaveSamples s;
	struct WavePeakMsg q;
	ULONG seed = 12345, f, got, want, steps[5] = { 1, 2, 7, 178, 1023 };
	UWORD ch, k;
	int same = 1;

	for (f = 0; f < 3 * 5000; f++) {
		seed = seed * 1103515245UL + 12345UL;
		/* the edges too: -32768, 32767 and around 0 */
		d[f] = (WORD) (f % 97 == 0 ? -32768 : f % 89 == 0 ? 32767 : (LONG) ((seed >> 8) & 0xFFFF) - 32768L);
	}
	for (ch = 1; ch <= 3; ch++) {
		for (k = 0; k < 5; k++) {
			s.wsm_First = 1000;
			s.wsm_Frames = 5000 - (ULONG) k * 3;
			s.wsm_Channels = ch;
			s.wsm_Data = d;
			q.wpm_First = 1000 + k;
			q.wpm_Step = steps[k];
			q.wpm_Count = 600;
			q.wpm_Out = o;
			memset(o, 0x55, sizeof(o));
			memset(r, 0x55, sizeof(r));
			got = WavePlan_FromSamples(&s, &q);
			want = top_bytes(&s, &q, r);
			if (got != want || memcmp(o, r, sizeof(o)) != 0) same = 0;
		}
	}
	CHECK(same, "samples compared as words: the columns the per-sample top bytes gave (1-3 channels, cut columns)");
}

static void test_scale(void)
{
	struct WaveScale s;

	s.wsc_Half = 30;
	s.wsc_Gain = 100;
	s.wsc_Mode = WAVEFORM_SCALE_LINEAR;
	s.wsc_DBRange = 60;
	CHECK(WavePlan_Scale(&s, 127) == 30 && WavePlan_Scale(&s, -127) == -30 && WavePlan_Scale(&s, -128) == -30, "full scale: the edge");
	CHECK(WavePlan_Scale(&s, 64) == 15 && WavePlan_Scale(&s, 0) == 0, "half, and zero");
	s.wsc_Gain = 200;
	CHECK(WavePlan_Scale(&s, 64) == 30, "gain 200: half is full");
	s.wsc_Gain = 100;
	s.wsc_Mode = WAVEFORM_SCALE_DB;
	CHECK(WavePlan_Scale(&s, 127) == 30 && WavePlan_Scale(&s, 0) == 0, "dB: 0 dB at the edge");
	CHECK(WavePlan_Scale(&s, 1) == 8 && WavePlan_Scale(&s, -64) == -27, "dB: -42 dB and -6 dB");
	s.wsc_DBRange = 60000;
	CHECK(WavePlan_Scale(&s, 64) >= 0 && WavePlan_Scale(&s, 64) <= 30, "a huge range: capped, no wrap");
}

/* a fade's line: the curves, the program's levels walked, the end fades */
static void test_level(void)
{
	struct WaveLevel lv[3];
	struct WaveLevelWalk w;
	UWORD a, b;

	CHECK(WavePlan_Curve(WAVELEVEL_LINEAR, 0) == 0 && WavePlan_Curve(WAVELEVEL_LINEAR, 32768U) == 2048 &&
	      WavePlan_Curve(WAVELEVEL_LINEAR, WAVELEVEL_U_MAX) == WAVELEVEL_UNITY, "linear: 0, half, whole");
	CHECK(WavePlan_Curve(WAVELEVEL_EVEN, 32768U) == 1024 && WavePlan_Curve(WAVELEVEL_EVEN, WAVELEVEL_U_MAX) == WAVELEVEL_UNITY,
	      "even: a quarter at half way");
	CHECK(WavePlan_Curve(WAVELEVEL_SCURVE, 32768U) == 2048 && WavePlan_Curve(WAVELEVEL_SCURVE, 0) == 0 &&
	      WavePlan_Curve(WAVELEVEL_SCURVE, WAVELEVEL_U_MAX) == WAVELEVEL_UNITY, "S-curve: half at half way, its ends");
	a = WavePlan_Curve(WAVELEVEL_SCURVE, 6000);
	b = WavePlan_Curve(WAVELEVEL_SCURVE, 59535U);
	CHECK(a < WavePlan_Curve(WAVELEVEL_LINEAR, 6000) && a + b >= 4095 && a + b <= 4097, "S-curve: gentle at the start, symmetric");

	memset(&w, 0, sizeof(w));
	memset(lv, 0, sizeof(lv));
	CHECK(WavePlan_LevelAt(&w, 500) == WAVEPLAN_NOLINE, "nothing: no line");
	/* a gain alone; a fade out with a gain of 2; a fade in's second half */
	lv[0].wlv_First = 0;
	lv[0].wlv_End = 1000;
	lv[0].wlv_Gain = 8192;
	lv[1].wlv_First = 1000;
	lv[1].wlv_End = 2000;
	lv[1].wlv_Gain = 8192;
	lv[1].wlv_Fade = WAVELEVEL_OUT | WAVELEVEL_EVEN;
	lv[1].wlv_U1 = WAVELEVEL_U_MAX;
	lv[2].wlv_First = 3000;
	lv[2].wlv_End = 3100;
	lv[2].wlv_Gain = WAVELEVEL_UNITY;
	lv[2].wlv_Fade = WAVELEVEL_IN | WAVELEVEL_LINEAR;
	lv[2].wlv_U0 = 32768U;
	lv[2].wlv_U1 = WAVELEVEL_U_MAX;
	w.lw_Level = lv;
	w.lw_Count = 3;
	CHECK(WavePlan_LevelAt(&w, 500) == WAVEPLAN_NOLINE && w.lw_Gain == 8192, "a gain alone: no line, its gain said");
	CHECK(WavePlan_LevelAt(&w, 1000) == WAVELEVEL_UNITY, "a fade out's start at a gain of 2: as recorded at most");
	CHECK(WavePlan_LevelAt(&w, 1500) == 2048, "half way: a quarter, times 2");
	CHECK(WavePlan_LevelAt(&w, 1999) < 8, "its end: next to nothing");
	CHECK(WavePlan_LevelAt(&w, 2500) == WAVEPLAN_NOLINE && w.lw_Gain == WAVELEVEL_UNITY, "between: no line, no gain");
	CHECK(WavePlan_LevelAt(&w, 3000) == 2048, "the second half of a fade in: from half");
	CHECK(WavePlan_LevelAt(&w, 3200) == WAVEPLAN_NOLINE && w.lw_Next == 3, "past them all");
	w.lw_Next = 0;
	CHECK(WavePlan_LevelAt(&w, 1500) == 2048, "walked again from the start");

	/* the end fades, a gain under one of them; frames past 16 bits */
	w.lw_In = 1000;
	w.lw_FadeIn = 1000;
	w.lw_Out = 30000000UL;
	w.lw_FadeOut = 10000000UL;
	w.lw_Shape = WAVELEVEL_EVEN;
	w.lw_Next = 0;
	CHECK(WavePlan_LevelAt(&w, 999) == WAVEPLAN_NOLINE, "before In's fade: the gain alone, no line");
	CHECK(WavePlan_LevelAt(&w, 1000) == 0, "In: nothing");
	CHECK(WavePlan_LevelAt(&w, 1500) == 512, "In's fade half way over the fade out's 2048: a quarter of it");
	CHECK(WavePlan_LevelAt(&w, 20000000UL) == WAVELEVEL_UNITY, "Out's fade, its inner end: as recorded");
	a = WavePlan_LevelAt(&w, 25000000UL);
	CHECK(a >= 1020 && a <= 1028, "Out's fade half way, 10 million frames long: a quarter");
	CHECK(WavePlan_LevelAt(&w, 29999999UL) == 0, "the frame before Out: nothing");
	CHECK(WavePlan_LevelAt(&w, 30000000UL) == WAVEPLAN_NOLINE, "Out and past: no line");
}

int main(void)
{
	test_view();
	test_time();
	test_ruler();
	test_parts();
	test_snap();
	test_zero();
	test_samples();
	test_samples_same();
	test_scale();
	test_level();
	CHECK(WavePlan_Sig(WAVEPLAN_SIG_SEED, "a", 1) == 0xE40C292CUL, "FNV-1a of \"a\"");
	CHECK(WavePlan_Sig(WAVEPLAN_SIG_SEED, "", 0) == WAVEPLAN_SIG_SEED, "nothing: the seed");
	return CHECK_DONE("test_waveplan");
}
