/*
 * test_timeline.c -- timeline.c: the edit list's cuts and silences
 * against a model (an array: each timeline frame's file frame, or -1 for
 * silence) -- hand-picked cases first, then a seeded run of random ones;
 * after each, every frame's place (Tl_At), every file frame's (Tl_Find),
 * the joins, the stretches a file range plays as (Tl_Ranges) and the tidy
 * form (no two pieces that could be one). Then Tl_Read through doshost.c:
 * a file whose frames hold their own numbers, read across pieces and
 * silence.
 *
 *   test_timeline <work drawer>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/dos.h>

#include "check.h"
#include "doshost.h"
#include "timeline.h"

#define FILE_FRAMES	600UL
#define RUNS		4000

static long model[FILE_FRAMES * 2];
static ULONG model_n;

static void model_init(void)
{
	ULONG i;

	for (i = 0; i < FILE_FRAMES; i++) model[i] = (long) i;
	model_n = FILE_FRAMES;
}

static void model_take(ULONG a, ULONG b, int silence)
{
	ULONG i;

	if (silence) {
		for (i = a; i < b; i++) model[i] = -1;
		return;
	}
	memmove(&model[a], &model[b], (model_n - b) * sizeof(model[0]));
	model_n -= b - a;
}

/* the timeline as the model says, every way it can be asked */
static int agrees(const struct Timeline *tl, const char *what)
{
	struct TlAt at;
	struct TlRange rg[TL_MAXPIECES];
	struct TlRanges tr;
	ULONG t, f, joins[TL_MAXPIECES], want, k;
	UWORD n, i;
	int bad = 0;

	if (tl->tl_Frames != model_n) bad = 1;
	for (t = 0; !bad && t < model_n; t++) {
		if (!Tl_At(tl, t, &at)) bad = 2;
		else if (model[t] < 0 ? at.ta_From != TL_SILENCE : at.ta_From != (ULONG) model[t]) bad = 3;
		else if (at.ta_Run == 0 || t + at.ta_Run > model_n) bad = 4;
	}
	if (!bad && Tl_At(tl, model_n, &at)) bad = 5;
	/* each file frame: where it plays first */
	for (f = 0; !bad && f < FILE_FRAMES; f++) {
		want = TL_NONE;
		for (t = 0; t < model_n; t++) {
			if (model[t] == (long) f) {
				want = t;
				break;
			}
		}
		if (Tl_Find(tl, f) != want) bad = 6;
	}
	/* the joins: where the next frame is not the one after in the file */
	n = Tl_Joins(tl, joins, TL_MAXPIECES);
	k = 0;
	for (t = 1; !bad && t < model_n; t++) {
		if ((model[t - 1] < 0) == (model[t] < 0) && (model[t] < 0 || model[t] == model[t - 1] + 1)) continue;
		if (k >= n || joins[k] != t) bad = 7;
		k++;
	}
	if (!bad && k != n) bad = 8;
	/* tidy: the joins are the pieces' borders */
	if (!bad && n + 1 != tl->tl_Count && !(tl->tl_Count == 0 && n == 0)) bad = 9;
	/* a file range's stretches: the frames of the model inside it */
	tr.tr_First = FILE_FRAMES / 4;
	tr.tr_End = FILE_FRAMES * 3 / 4;
	tr.tr_Out = rg;
	tr.tr_Max = TL_MAXPIECES;
	n = Tl_Ranges(tl, &tr);
	for (t = 0, i = 0; !bad && t < model_n; t++) {
		int in = model[t] >= (long) tr.tr_First && model[t] < (long) tr.tr_End;
		int inr = i < n && t >= rg[i].tg_First && t < rg[i].tg_End;

		if (in != inr) bad = 10;
		if (i < n && t + 1 == rg[i].tg_End) i++;
	}
	if (bad) printf("  %s: disagrees (%d), %lu frames, %u pieces\n", what, bad, (unsigned long) tl->tl_Frames, tl->tl_Count);
	return !bad;
}

int main(int argc, char **argv)
{
	static struct Timeline tl, keep;
	static UBYTE file[FILE_FRAMES * 4], buf[FILE_FRAMES * 8];
	struct TlRead rd;
	char path[512];
	ULONG a, b, i, got;
	int r, ok = 1, full = 0;
	FILE *f;
	BPTR fh;

	if (argc < 2) {
		printf("usage: test_timeline <work drawer>\n");
		return 2;
	}

	/* a fresh file: one piece, the whole */
	Tl_Init(&tl, FILE_FRAMES);
	model_init();
	CHECK(tl.tl_Count == 1 && Tl_Whole(&tl, FILE_FRAMES), "a fresh timeline: one piece, the whole file");
	CHECK(!Tl_Whole(&tl, FILE_FRAMES - 1), "not the whole of a longer file");
	CHECK(agrees(&tl, "fresh"), "fresh: as the model");
	Tl_Init(&keep, 0);
	CHECK(keep.tl_Count == 0 && Tl_Whole(&keep, 0), "an empty file: no piece");

	/* by hand: a cut in the middle, joined */
	CHECK(Tl_Cut(&tl, 100, 200), "cut 100..199");
	model_take(100, 200, 0);
	CHECK(tl.tl_Count == 2 && tl.tl_Frames == FILE_FRAMES - 100, "two pieces, 100 frames fewer");
	CHECK(agrees(&tl, "cut"), "cut: as the model");
	CHECK(Tl_Find(&tl, 150) == TL_NONE && Tl_Find(&tl, 200) == 100, "the cut frames gone, 200 plays at 100");
	CHECK(!Tl_Whole(&tl, FILE_FRAMES), "no longer the whole file");
	/* silence over the join and beyond */
	CHECK(Tl_Silence(&tl, 90, 120), "silence 90..119");
	model_take(90, 120, 1);
	CHECK(tl.tl_Frames == FILE_FRAMES - 100 && agrees(&tl, "silence"), "silence: same length, as the model");
	/* a cut touching the silence: the silence shorter */
	CHECK(Tl_Cut(&tl, 110, 130), "cut 110..129");
	model_take(110, 130, 0);
	CHECK(agrees(&tl, "cut into the silence"), "cut into the silence: as the model");
	/* silence next to silence: one */
	CHECK(Tl_Silence(&tl, 80, 90), "silence 80..89 before the silence");
	model_take(80, 90, 1);
	CHECK(agrees(&tl, "silence beside silence"), "silence beside silence: one piece");
	/* the cut that undoes nothing: refused, nothing changed */
	keep = tl;
	CHECK(!Tl_Cut(&tl, 50, 50) && Tl_Same(&tl, &keep), "an empty cut refused");
	CHECK(!Tl_Cut(&tl, 10, tl.tl_Frames + 1) && Tl_Same(&tl, &keep), "a cut past the end refused");
	CHECK(!Tl_Silence(&tl, 30, 20) && Tl_Same(&tl, &keep), "a backwards silence refused");
	/* the ends */
	CHECK(Tl_Cut(&tl, 0, 10), "cut the first 10");
	model_take(0, 10, 0);
	CHECK(Tl_Cut(&tl, tl.tl_Frames - 10, tl.tl_Frames), "cut the last 10");
	model_take(model_n - 10, model_n, 0);
	CHECK(agrees(&tl, "ends"), "ends: as the model");
	/* all of it */
	CHECK(Tl_Cut(&tl, 0, tl.tl_Frames), "cut everything");
	model_take(0, model_n, 0);
	CHECK(tl.tl_Frames == 0 && tl.tl_Count == 0 && agrees(&tl, "empty"), "nothing left: no piece");

	/* random runs against the model */
	srand(42);
	Tl_Init(&tl, FILE_FRAMES);
	model_init();
	for (i = 0; i < RUNS && ok; i++) {
		if (model_n < 20) {
			Tl_Init(&tl, FILE_FRAMES);
			model_init();
		}
		a = (ULONG) rand() % model_n;
		b = a + 1 + (ULONG) rand() % (model_n - a < 40 ? model_n - a : 40);
		keep = tl;
		r = rand() % 3;
		if (r == 0 ? Tl_Silence(&tl, a, b) : Tl_Cut(&tl, a, b)) {
			model_take(a, b, r == 0);
		} else {
			/* only for too many pieces, and then nothing changed */
			full++;
			ok = tl.tl_Count + 3 > TL_MAXPIECES && Tl_Same(&tl, &keep);
		}
		if (ok) ok = agrees(&tl, "random");
	}
	CHECK(ok, "4000 random cuts and silences: as the model, every one");
	printf("  random: %d refused (too many pieces)\n", full);

	/* too many pieces: refused, the timeline as it was */
	Tl_Init(&tl, 100000UL);
	for (i = 0; i < TL_MAXPIECES; i++) {
		if (!Tl_Silence(&tl, i * 10 + 1, i * 10 + 2)) break;
	}
	keep = tl;
	CHECK(i < TL_MAXPIECES && tl.tl_Count + 3 > TL_MAXPIECES, "silences until no room");
	CHECK(!Tl_Silence(&tl, 99000UL, 99001UL) && Tl_Same(&tl, &keep), "the next refused, nothing changed");

	/* Tl_Read: a file whose frame k holds k (two 16-bit words, k and ~k) */
	for (i = 0; i < FILE_FRAMES; i++) {
		file[i * 4] = (UBYTE) (i & 0xFF);
		file[i * 4 + 1] = (UBYTE) (i >> 8);
		file[i * 4 + 2] = (UBYTE) (~i & 0xFF);
		file[i * 4 + 3] = (UBYTE) ((~i >> 8) & 0xFF);
	}
	snprintf(path, sizeof(path), "%s/timeline.raw", argv[1]);
	f = fopen(path, "wb");
	CHECK(f != NULL, "the test file written");
	if (!f) return CHECK_DONE("test_timeline");
	fwrite("HEAD", 1, 4, f);		/* the samples from byte 4 on */
	fwrite(file, 1, sizeof(file), f);
	fclose(f);
	fh = Open((STRPTR) path, MODE_OLDFILE);
	CHECK(fh != 0, "the test file opened");
	Tl_Init(&tl, FILE_FRAMES);
	model_init();
	Tl_Cut(&tl, 50, 150);
	model_take(50, 150, 0);
	Tl_Silence(&tl, 40, 70);
	model_take(40, 70, 1);
	Tl_Cut(&tl, 300, 310);
	model_take(300, 310, 0);
	Tl_ReadInit(&rd);
	rd.rd_FH = fh;
	rd.rd_Start = 4;
	rd.rd_Align = 4;
	rd.rd_Pad = 0;
	rd.rd_First = 20;
	rd.rd_Frames = model_n;			/* more than there is from 20 on */
	rd.rd_Buf = buf;
	/* not zeros: a silence must write its own */
	memset(buf, 0xAA, sizeof(buf));
	got = Tl_Read(&tl, &rd);
	CHECK(got == model_n - 20, "read to the timeline's end, no further");
	ok = 1;
	for (i = 0; i < got && ok; i++) {
		long m = model[20 + i];
		UBYTE *p = buf + i * 4;

		if (m < 0) ok = p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 0;
		else ok = p[0] == file[m * 4] && p[1] == file[m * 4 + 1] && p[2] == file[m * 4 + 2] && p[3] == file[m * 4 + 3];
		if (!ok) printf("  frame %lu: model %ld, read %02x%02x\n", (unsigned long) (20 + i), m, p[1], p[0]);
	}
	CHECK(ok, "every frame read is the model's (silence zeros)");
	/* the whole file: one read as before */
	Tl_Init(&tl, FILE_FRAMES);
	rd.rd_First = 0;
	rd.rd_Frames = FILE_FRAMES;
	memset(buf, 0xAA, sizeof(buf));
	CHECK(Tl_Read(&tl, &rd) == FILE_FRAMES && memcmp(buf, file, sizeof(file)) == 0, "the whole file read as it is");

	/* the level (concept 3.2): a gain on a part splits there, the rest as
	   recorded; the same gain on the parts beside it joins them up */
	{
		struct TlPart pt;
		struct TlLevels lv;
		UWORD before[FILE_FRAMES], g6 = Env_FromTenths(60);
		ULONG t;

		Tl_Init(&tl, FILE_FRAMES);
		pt.tp_First = 300;
		pt.tp_End = 400;
		pt.tp_Gain = g6;
		pt.tp_Fade = TL_FADE_NONE;
		pt.tp_Pad = 0;
		CHECK(Tl_Gain(&tl, &pt) && tl.tl_Count == 3 && !Tl_Whole(&tl, FILE_FRAMES), "a gain on 300..399: three pieces");
		CHECK(Tl_LevelAt(&tl, NULL, 299) == ENV_UNITY && Tl_LevelAt(&tl, NULL, 300) == g6 &&
		      Tl_LevelAt(&tl, NULL, 399) == g6 && Tl_LevelAt(&tl, NULL, 400) == ENV_UNITY, "+6 dB from 300 to 399 only");
		keep = tl;
		pt.tp_First = 0;
		pt.tp_End = 300;
		Tl_Gain(&tl, &pt);
		pt.tp_First = 400;
		pt.tp_End = FILE_FRAMES;
		Tl_Gain(&tl, &pt);
		CHECK(tl.tl_Count == 1 && tl.tl_Pieces[0].p_Gain == g6 && !Tl_Whole(&tl, FILE_FRAMES),
		      "the same gain beside it: one piece again, not as recorded");
		CHECK(!Tl_Same(&tl, &keep), "a level of its own is another timeline (an Undo step)");
		pt.tp_First = 0;
		pt.tp_End = FILE_FRAMES;
		CHECK(Tl_Plain(&tl, &pt) && Tl_Whole(&tl, FILE_FRAMES), "made plain: the file as it is");

		/* a fade in over 100..199, Even: nothing at its start, the
		   curve on, as recorded from its end */
		pt.tp_First = 100;
		pt.tp_End = 200;
		pt.tp_Fade = TL_FADE_IN | ENV_SHAPE_EVEN;
		CHECK(Tl_Fade(&tl, &pt) && tl.tl_Count == 3, "a fade in on 100..199: three pieces");
		CHECK(Tl_LevelAt(&tl, NULL, 99) == ENV_UNITY && Tl_LevelAt(&tl, NULL, 100) == 0 &&
		      Tl_LevelAt(&tl, NULL, 200) == ENV_UNITY, "nothing at its start, as recorded after it");
		CHECK(abs((int) Tl_LevelAt(&tl, NULL, 150) - 1024) <= 2, "half way through an Even fade: a quarter");
		for (t = 100, ok = 1; t < 199; t++) {
			if (Tl_LevelAt(&tl, NULL, t + 1) < Tl_LevelAt(&tl, NULL, t)) ok = 0;
		}
		CHECK(ok, "a fade in never falls");
		/* cut through it: what is left of the fade keeps its curve --
		   every frame at the level it had in the music */
		for (t = 0; t < FILE_FRAMES; t++) before[t] = Tl_LevelAt(&tl, NULL, t);
		CHECK(Tl_Cut(&tl, 130, 170), "a cut through the fade");
		for (t = 0, ok = 1; t < tl.tl_Frames; t++) {
			ULONG f = t < 130 ? t : t + 40;

			if (abs((int) Tl_LevelAt(&tl, NULL, t) - (int) before[f]) > 1) {
				if (ok) printf("  frame %lu (file %lu): %u, was %u\n", (unsigned long) t, (unsigned long) f,
					       Tl_LevelAt(&tl, NULL, t), before[f]);
				ok = 0;
			}
		}
		CHECK(ok, "after the cut every frame at its old level (within 1)");
		/* a fade over a fade: replaced */
		pt.tp_First = 100;
		pt.tp_End = 160;
		pt.tp_Fade = TL_FADE_OUT | ENV_SHAPE_LINEAR;
		CHECK(Tl_Fade(&tl, &pt) && Tl_LevelAt(&tl, NULL, 100) == ENV_UNITY &&
		      abs((int) Tl_LevelAt(&tl, NULL, 130) - 2048) <= 2, "a fade out over the fade in: replaced");

		/* the end fades and the dips, from outside the pieces */
		Tl_Init(&tl, FILE_FRAMES);
		lv.lv_In = 100;
		lv.lv_Out = 500;
		lv.lv_FadeIn = 50;
		lv.lv_FadeOut = 100;
		lv.lv_Dip = 0;
		lv.lv_Shape = ENV_SHAPE_LINEAR;
		lv.lv_Pad = 0;
		CHECK(Tl_LevelAt(&tl, &lv, 99) == ENV_UNITY && Tl_LevelAt(&tl, &lv, 100) == 0 &&
		      Tl_LevelAt(&tl, &lv, 150) == ENV_UNITY, "faded in from In over 50 frames");
		CHECK(Tl_LevelAt(&tl, &lv, 399) == ENV_UNITY && abs((int) Tl_LevelAt(&tl, &lv, 450) - 2048) <= 2 &&
		      Tl_LevelAt(&tl, &lv, 499) < 50 && Tl_LevelAt(&tl, &lv, 500) == ENV_UNITY, "faded out to Out over 100 frames");
		lv.lv_FadeIn = lv.lv_FadeOut = 0;
		lv.lv_Dip = 8;
		Tl_Cut(&tl, 300, 310);
		pt.tp_First = 200;
		pt.tp_End = 250;
		pt.tp_Gain = Env_FromTenths(10);
		Tl_Gain(&tl, &pt);
		CHECK(Tl_LevelAt(&tl, &lv, 291) == ENV_UNITY && Tl_LevelAt(&tl, &lv, 299) < 200 &&
		      Tl_LevelAt(&tl, &lv, 300) == 0 && Tl_LevelAt(&tl, &lv, 308) == ENV_UNITY, "a dip of 8 frames either side of the cut");
		CHECK(Tl_LevelAt(&tl, &lv, 200) == pt.tp_Gain && Tl_LevelAt(&tl, &lv, 250) == ENV_UNITY,
		      "no dip where only the level changes");

		/* Tl_Read at a level: the pieces as recorded untouched, the
		   gained one every sample at its level */
		Tl_Init(&tl, FILE_FRAMES);
		pt.tp_First = 200;
		pt.tp_End = 260;
		pt.tp_Gain = g6;
		Tl_Gain(&tl, &pt);
		rd.rd_First = 0;
		rd.rd_Frames = FILE_FRAMES;
		rd.rd_Levels = NULL;
		memset(buf, 0xAA, sizeof(buf));
		CHECK(Tl_Read(&tl, &rd) == FILE_FRAMES, "read at a level");
		CHECK(!memcmp(buf, file, 200 * 4) && !memcmp(buf + 260 * 4, file + 260 * 4, (FILE_FRAMES - 260) * 4),
		      "the pieces as recorded: the file's bytes");
		for (t = 200, ok = 1; t < 260; t++) {
			short s0 = (short) (file[t * 4] | (file[t * 4 + 1] << 8));
			short r0 = (short) (buf[t * 4] | (buf[t * 4 + 1] << 8));

			if (r0 != (short) (((long) s0 * g6 + 2048) >> 12)) ok = 0;
		}
		CHECK(ok, "the gained piece: each sample times +6 dB");
		/* a fade in at In: its first block silence */
		lv.lv_In = 0;
		lv.lv_Out = FILE_FRAMES;
		lv.lv_FadeIn = 100;
		lv.lv_FadeOut = 0;
		lv.lv_Dip = 0;
		Tl_Init(&tl, FILE_FRAMES);
		rd.rd_Levels = &lv;
		memset(buf, 0xAA, sizeof(buf));
		Tl_Read(&tl, &rd);
		/* the first block at the level of its middle: (8 / 100) squared,
		   a samples' 1/156 -- frame k holds k, so 0 there */
		for (t = 0, ok = 1; t < ENV_BLOCK; t++) {
			short r0 = (short) (buf[t * 4] | (buf[t * 4 + 1] << 8));

			if (r0 != (short) (((long) t * Tl_LevelAt(&tl, &lv, 8) + 2048) >> 12)) ok = 0;
		}
		CHECK(ok, "faded in from In: its first block at its middle's level");
		CHECK(!memcmp(buf + 100 * 4, file + 100 * 4, (FILE_FRAMES - 100) * 4), "after the fade: the file's bytes");
		/* read in odd pieces, the same bytes: a level is the timeline's,
		   not the reader's */
		{
			static UBYTE whole[FILE_FRAMES * 4];
			ULONG at = 0, step = 7;

			memcpy(whole, buf, sizeof(whole));
			memset(buf, 0x55, sizeof(buf));
			while (at < FILE_FRAMES) {
				rd.rd_First = at;
				rd.rd_Frames = step < FILE_FRAMES - at ? step : FILE_FRAMES - at;
				rd.rd_Buf = buf + at * 4;
				Tl_Read(&tl, &rd);
				at += rd.rd_Frames;
				step = step % 23 + 5;
			}
			rd.rd_Buf = buf;
			CHECK(!memcmp(buf, whole, sizeof(whole)), "read in odd pieces: the same samples");
		}
		rd.rd_Levels = NULL;
	}
	Close(fh);
	CHECK(dos_host.dh_Handles == 0, "no handle left open");
	return CHECK_DONE("test_timeline");
}
