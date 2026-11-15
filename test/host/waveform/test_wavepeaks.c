/*
 * test_wavepeaks.c -- wavepeaks.c: a stereo sound of three 16384-frame
 * blocks and 700 frames more, fed whole and in odd pieces, its picture
 * asked at every level and held against the frames themselves.
 *
 *   left   a saw: (f % 3000) x 20 - 30000, -30000 .. 29980
 *   right  1000, a spike of 32767 at frame 20000 and of -32768 at 40001
 *
 * The answer is entry-exact: a column's minimum and maximum are those of
 * every entry it touches, so the expectation is worked out over the
 * entries' frames (whole for columns on entry boundaries).
 */
#include <string.h>

#include "check.h"
#include "wavepeaks.h"

#define FRAMES	(3UL * 16384UL + 700UL)

static WORD snd[FRAMES * 2];
static BYTE lev0[FRAMES / 1024 * 4 + 8], lev1[FRAMES / 16384 * 4 + 8], lev2[16];
static BYTE pie0[sizeof(lev0)], pie1[sizeof(lev1)], pie2[sizeof(lev2)];
static BYTE out[4096];

static void make(void)
{
	ULONG f;

	for (f = 0; f < FRAMES; f++) {
		snd[f * 2] = (WORD) ((LONG) (f % 3000) * 20 - 30000);
		snd[f * 2 + 1] = 1000;
	}
	snd[20000 * 2 + 1] = 32767;
	snd[40001 * 2 + 1] = -32768;
}

static void init(struct WavePeaks *wp, BYTE *l0, BYTE *l1, BYTE *l2)
{
	CHECK(WavePeaks_Init(wp, FRAMES, 2), "init stereo");
	wp->wp_Level[0] = l0;
	wp->wp_Level[1] = l1;
	wp->wp_Level[2] = l2;
}

/* the frames' minimum and maximum of channel c over [a, b) */
static void brute(ULONG a, ULONG b, UWORD c, BYTE *mn, BYTE *mx)
{
	ULONG f;
	BYTE t;

	*mn = 127;
	*mx = -128;
	if (b > FRAMES) b = FRAMES;
	for (f = a; f < b; f++) {
		t = WavePeaks_Top(snd[f * 2 + c]);
		if (t < *mn) *mn = t;
		if (t > *mx) *mx = t;
	}
}

/* every column of a query against the entries it touches; done: frames fed */
static void check_query(const struct WavePeaks *wp, ULONG first, ULONG step, ULONG count, ULONG done)
{
	struct WavePeakMsg q;
	ULONG got, i, a, b, sh, want;
	UWORD c;
	BYTE mn, mx;
	int bad = 0;

	q.wpm_First = first;
	q.wpm_Step = step;
	q.wpm_Count = count;
	q.wpm_Out = out;
	got = WavePeaks_Query(wp, &q);
	/* the columns that start inside what was fed, or beyond the sound */
	for (want = 0; want < count; want++) {
		a = first + want * step;
		if (a >= done && a < FRAMES) break;
	}
	CHECK(got == want, "the columns answered");
	sh = step >= 262144UL ? 18 : step >= 16384UL ? 14 : 10;
	for (i = 0; i < got && !bad; i++) {
		a = first + i * step;
		if (a >= FRAMES) {
			if (out[i * 4] != 0 || out[i * 4 + 1] != 0) bad = 1;
			continue;
		}
		b = a + step - 1;
		if (b >= done) b = done - 1;
		a = a >> sh << sh;
		b = ((b >> sh) + 1) << sh;
		if (b > done) b = done;
		for (c = 0; c < 2; c++) {
			brute(a, b, c, &mn, &mx);
			if (out[(i * 2 + c) * 2] != mn || out[(i * 2 + c) * 2 + 1] != mx) bad = 1;
		}
	}
	CHECK(!bad, "each column's minimum and maximum");
}

int main(void)
{
	static struct WavePeaks wp, wq;
	struct WavePeakMsg q;
	ULONG pieces[] = { 1, 7, 1023, 1025, 5000, 16384, 3 }, at, n;
	UWORD k;

	/* the top byte: floor(v / 256) for both signs */
	CHECK(WavePeaks_Top(0) == 0 && WavePeaks_Top(255) == 0 && WavePeaks_Top(256) == 1, "top byte, positive");
	CHECK(WavePeaks_Top(-1) == -1 && WavePeaks_Top(-256) == -1 && WavePeaks_Top(-257) == -2, "top byte, negative");
	CHECK(WavePeaks_Top(32767) == 127 && WavePeaks_Top(-32768) == -128, "top byte, the ends");

	make();
	CHECK(!WavePeaks_Init(&wp, FRAMES, 0) && !WavePeaks_Init(&wp, FRAMES, 3), "channels 0 and 3 refused");
	init(&wp, lev0, lev1, lev2);
	CHECK(wp.wp_Entries[0] == 49 && wp.wp_Entries[1] == 4 && wp.wp_Entries[2] == 1, "entries per level");
	CHECK(WavePeaks_Bytes(&wp, 0) == 49 * 4 && WavePeaks_Bytes(&wp, 2) == 4 && WavePeaks_Bytes(&wp, 3) == 0, "bytes per level");

	/* fed whole */
	WavePeaks_Feed(&wp, snd, FRAMES);
	CHECK(wp.wp_Done == FRAMES, "all fed");
	check_query(&wp, 0, 1024, 49, FRAMES);
	check_query(&wp, 0, 2048, 25, FRAMES);
	check_query(&wp, 500, 5000, 10, FRAMES);
	check_query(&wp, 0, 16384, 4, FRAMES);
	check_query(&wp, 19000, 20000, 3, FRAMES);
	check_query(&wp, 0, 262144, 1, FRAMES);
	check_query(&wp, 40000, 1024, 20, FRAMES);	/* beyond the end: 0, 0 */

	/* the spikes are seen at every level */
	q.wpm_First = 0;
	q.wpm_Step = 262144;
	q.wpm_Count = 1;
	q.wpm_Out = out;
	CHECK(WavePeaks_Query(&wp, &q) == 1 && out[2] == -128 && out[3] == 127, "the spikes in the whole sound");
	q.wpm_Step = 1000;
	CHECK(WavePeaks_Query(&wp, &q) == 0, "finer than 1024: no answer");
	q.wpm_Step = 0;
	CHECK(WavePeaks_Query(&wp, &q) == 0, "step 0: no answer");
	WavePeaks_Feed(&wp, snd, 10);
	CHECK(wp.wp_Done == FRAMES, "feeding past the end takes nothing");

	/* fed in odd pieces: the same picture, and part-way answers on the way */
	init(&wq, pie0, pie1, pie2);
	for (at = 0, k = 0; at < FRAMES; k++) {
		n = pieces[k % 7];
		if (n > FRAMES - at) n = FRAMES - at;
		WavePeaks_Feed(&wq, snd + at * 2, n);
		at += n;
		if (at > 20000 && at < 24000) {
			check_query(&wq, 0, 16384, 4, at);
			check_query(&wq, 0, 1024, 30, at);
			check_query(&wq, 0, 262144, 1, at);
		}
	}
	CHECK(memcmp(lev0, pie0, 49 * 4) == 0, "level 0 the same, whole or in pieces");
	CHECK(memcmp(lev1, pie1, 4 * 4) == 0 && memcmp(lev2, pie2, 4) == 0, "levels 1 and 2 the same");

	/* mono */
	CHECK(WavePeaks_Init(&wq, 3000, 1), "init mono");
	wq.wp_Level[0] = pie0;
	wq.wp_Level[1] = pie1;
	wq.wp_Level[2] = pie2;
	{
		static WORD m[3000];
		ULONG f;

		for (f = 0; f < 3000; f++) m[f] = (WORD) (f == 2500 ? -20000 : 300);
		WavePeaks_Feed(&wq, m, 3000);
	}
	q.wpm_First = 0;
	q.wpm_Step = 2048;
	q.wpm_Count = 2;
	q.wpm_Out = out;
	CHECK(WavePeaks_Query(&wq, &q) == 2, "mono: two columns");
	CHECK(out[0] == 1 && out[1] == 1 && out[2] == WavePeaks_Top(-20000) && out[3] == 1, "mono: the values");

	/* a reader's runs (WavePeaks_FeedRun, the extremes worked out here)
	   and a flush a piece: what WavePeaks_Feed leaves, part-way and at
	   the end, in pieces of every size */
	{
		static BYTE r0[sizeof(lev0)], r1[sizeof(lev1)], r2[sizeof(lev2)];
		struct WavePeaks wf, wr;
		ULONG done = 0, piece, left, run, f, k, sizes[5] = { 1, 700, 1023, 4097, 20011 };
		BYTE mm[4];
		UWORD c, n = 0, same = 1;

		init(&wf, pie0, pie1, pie2);
		init(&wr, r0, r1, r2);
		memset(pie0, 0, sizeof(pie0)); memset(pie1, 0, sizeof(pie1)); memset(pie2, 0, sizeof(pie2));
		memset(r0, 0, sizeof(r0)); memset(r1, 0, sizeof(r1)); memset(r2, 0, sizeof(r2));
		while (done < FRAMES) {
			piece = sizes[n++ % 5];
			if (piece > FRAMES - done) piece = FRAMES - done;
			WavePeaks_Feed(&wf, snd + done * 2, piece);
			for (left = piece, f = done; left; left -= run, f += run) {
				run = WavePeaks_RunLeft(&wr);
				if (run > left) run = left;
				for (c = 0; c < 2; c++) {
					mm[c * 2] = 127;
					mm[c * 2 + 1] = -128;
					for (k = f; k < f + run; k++) {
						if (WavePeaks_Top(snd[k * 2 + c]) < mm[c * 2]) mm[c * 2] = WavePeaks_Top(snd[k * 2 + c]);
						if (WavePeaks_Top(snd[k * 2 + c]) > mm[c * 2 + 1]) mm[c * 2 + 1] = WavePeaks_Top(snd[k * 2 + c]);
					}
				}
				WavePeaks_FeedRun(&wr, run, mm);
			}
			WavePeaks_Flush(&wr);
			done += piece;
			if (memcmp(pie0, r0, sizeof(r0)) || memcmp(pie1, r1, sizeof(r1)) || memcmp(pie2, r2, sizeof(r2)) ||
			    wf.wp_Done != wr.wp_Done)
				same = 0;
		}
		CHECK(same, "runs and a flush a piece: the same picture as Feed, part-way and at the end");
		CHECK(WavePeaks_RunLeft(&wr) == 0, "all fed: no run left");
	}

	return CHECK_DONE("test_wavepeaks");
}
