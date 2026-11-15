/*
 * test_docends.c -- WaveSmith's doc.c reading a long file's ends first
 * (concept D11 b, Doc_SetEnds): the test single (make_edit_single.py)
 * read whole and ends first must give the same suggestion, the same
 * picture and the same plain peaks; the suggestion must be there before
 * the picture is done; a fetch and a save in the middle of it must not
 * move the reading; ends too short for the music's ends must fall back to
 * the whole file's level, and give its suggestion at the end.
 * And a worn record's side (make_vinyl_side.py: crackle, rumble, three
 * songs, 2 minutes), when given: whole and ends first alike.
 *   test_docends <drawer> <Single.wav> [<side.wav>]
 * The samples are turned big-endian in a copy first: doc.c reads them as
 * the Amiga loads them (the file's bytes as a big-endian word).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>

#include "doc.h"
#include "doshost.h"
#include "check.h"

#define PATH_LEN	512
#define FETCH_AT	(44100UL * 20)	/* the middle: never read by the ends */

static struct Doc doc;

/* what a read gave */
struct Snap {
	struct TrimPlan sn_Plan;
	ULONG sn_Frames;
	ULONG sn_PeakCount;
	UBYTE *sn_PeakLooks;
	UBYTE *sn_Looks;		/* the crackle-proof levels */
	ULONG sn_LookCount;
	ULONG sn_TailFirst;		/* the tail's first look (ends first) */
	BYTE *sn_Picture;		/* every level, one after the other */
	ULONG sn_PictureBytes;
	int sn_Early;			/* the plan ready before the picture was */
	int sn_ReplanEarly;		/* Doc_Replan took it before the doc was read */
	int sn_Phased;			/* read ends first */
	int sn_PercentOk;		/* never back, 100 at the end */
	int sn_FetchOk;			/* the fetch in the tail's pass worked */
	LONG sn_SaveErr;		/* the save in the middle of the reading */
};

/* how to read */
struct How {
	ULONG hw_Ends;			/* seconds; 0: whole */
	int hw_Fetch;			/* a fetch from the middle, in the tail's pass */
	const char *hw_SaveAs;		/* a save once the plan is ready, the rest unread */
};

/* the WAV from into to, its samples' bytes turned */
static int be_copy(const char *from, const char *to)
{
	FILE *f = fopen(from, "rb");
	unsigned char *b;
	unsigned long n, at, size, i;
	unsigned char t;
	int ok = 0;

	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	n = (unsigned long) ftell(f);
	fseek(f, 0, SEEK_SET);
	if (!(b = (unsigned char *) malloc(n)) || fread(b, 1, n, f) != n) {
		fclose(f);
		free(b);
		return 0;
	}
	fclose(f);
	for (at = 12; at + 8 <= n; at += 8 + size + (size & 1)) {
		size = b[at + 4] | ((unsigned long) b[at + 5] << 8) | ((unsigned long) b[at + 6] << 16) | ((unsigned long) b[at + 7] << 24);
		if (memcmp(b + at, "data", 4) == 0) {
			for (i = at + 8; i + 1 < at + 8 + size && i + 1 < n; i += 2) {
				t = b[i];
				b[i] = b[i + 1];
				b[i + 1] = t;
			}
			ok = 1;
			break;
		}
	}
	if (ok && (!(f = fopen(to, "wb")) || fwrite(b, 1, n, f) != n)) ok = 0;
	if (f) fclose(f);
	free(b);
	return ok;
}

/* the picture's levels into one block */
static int keep_picture(struct Snap *sn)
{
	UWORD l;
	ULONG at = 0, n;

	sn->sn_PictureBytes = 0;
	for (l = 0; l < WAVEPEAKS_LEVELS; l++) sn->sn_PictureBytes += WavePeaks_Bytes(&doc.d_Pic.dp_Peaks, l);
	if (!(sn->sn_Picture = (BYTE *) malloc(sn->sn_PictureBytes + 1))) return 0;
	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		n = WavePeaks_Bytes(&doc.d_Pic.dp_Peaks, l);
		memcpy(sn->sn_Picture + at, doc.d_Pic.dp_Peaks.wp_Level[l], n);
		at += n;
	}
	return 1;
}

static int read_doc(const char *path, const struct How *hw, struct Snap *sn)
{
	UWORD pc, last = 0;
	BOOL more = TRUE;

	memset(sn, 0, sizeof(*sn));
	sn->sn_PercentOk = 1;
	sn->sn_FetchOk = -1;
	sn->sn_SaveErr = -1;
	Doc_SetEnds(hw->hw_Ends);
	if (Doc_Open(&doc, (CONST_STRPTR) path) != DOC_OK) {
		Doc_Close(&doc);
		return 0;
	}
	sn->sn_Phased = doc.d_Phase == DOC_READ_HEAD;
	while (more) {
		more = Doc_Read(&doc);
		if (doc.d_PlanReady && !doc.d_Done && !sn->sn_Early) {
			sn->sn_Early = 1;
			/* the suggestion made again only once the doc is read */
			if (Doc_Replan(&doc)) sn->sn_ReplanEarly = 1;
			if (hw->hw_SaveAs) {
				struct DocSave ds;

				ds.ds_In = doc.d_Plan.tp_In;
				ds.ds_Out = doc.d_Plan.tp_Out;
				ds.ds_Path = (CONST_STRPTR) hw->hw_SaveAs;
				sn->sn_SaveErr = Doc_SaveAs(&doc, &ds);
			}
		}
		if (hw->hw_Fetch && sn->sn_FetchOk < 0 && doc.d_Phase == DOC_READ_TAIL && doc.d_TailAt > doc.d_TailFrom)
		{
			const struct WaveSamples *ws = Doc_Fetch(&doc, FETCH_AT, 4096);

			sn->sn_FetchOk = ws && ws->wsm_First == FETCH_AT;
		}
		pc = Doc_Percent(&doc);
		if (pc < last || pc > 100) sn->sn_PercentOk = 0;
		last = pc;
	}
	if (last != 100 || !doc.d_Done) sn->sn_PercentOk = 0;
	sn->sn_Plan = doc.d_Plan;
	sn->sn_Frames = doc.d_Frames;
	sn->sn_PeakCount = doc.d_Pic.dp_PeakCount;
	if (!(sn->sn_PeakLooks = (UBYTE *) malloc(doc.d_Pic.dp_PeakCount + 1)) || !keep_picture(sn)) {
		Doc_Close(&doc);
		return 0;
	}
	memcpy(sn->sn_PeakLooks, doc.d_Pic.dp_PeakLooks, doc.d_Pic.dp_PeakCount);
	sn->sn_LookCount = doc.d_Pic.dp_LookMax;
	/* the tail's first look from the ends alone, not from doc.c's sums */
	sn->sn_TailFirst = sn->sn_Phased ? (doc.d_Frames - hw->hw_Ends * doc.d_Rate + doc.d_LL.ll_Frames - 1) / doc.d_LL.ll_Frames : 0;
	if (!(sn->sn_Looks = (UBYTE *) malloc(doc.d_Pic.dp_LookMax))) {
		Doc_Close(&doc);
		return 0;
	}
	memcpy(sn->sn_Looks, doc.d_Looks, doc.d_Pic.dp_LookMax);
	Doc_Close(&doc);
	return 1;
}

static void free_snap(struct Snap *sn)
{
	free(sn->sn_PeakLooks);
	free(sn->sn_Picture);
	free(sn->sn_Looks);
	sn->sn_Looks = NULL;
	sn->sn_PeakLooks = NULL;
	sn->sn_Picture = NULL;
}

static int same_plan(const struct TrimPlan *a, const struct TrimPlan *b)
{
	UWORD i;

	if (a->tp_Found != b->tp_Found || a->tp_Threshold != b->tp_Threshold || a->tp_In != b->tp_In || a->tp_Out != b->tp_Out ||
	    a->tp_MusicStart != b->tp_MusicStart || a->tp_MusicEnd != b->tp_MusicEnd || a->tp_LeadClicks != b->tp_LeadClicks ||
	    a->tp_TailClicks != b->tp_TailClicks || a->tp_PeriodMs != b->tp_PeriodMs || a->tp_RegionCount != b->tp_RegionCount)
		return 0;
	for (i = 0; i < a->tp_RegionCount; i++) {
		if (a->tp_Region[i].wrg_First != b->tp_Region[i].wrg_First || a->tp_Region[i].wrg_End != b->tp_Region[i].wrg_End ||
		    a->tp_Region[i].wrg_Kind != b->tp_Region[i].wrg_Kind)
			return 0;
	}
	return 1;
}

static int same_picture(const struct Snap *a, const struct Snap *b)
{
	return a->sn_PictureBytes == b->sn_PictureBytes && memcmp(a->sn_Picture, b->sn_Picture, a->sn_PictureBytes) == 0;
}

static int same_peaks(const struct Snap *a, const struct Snap *b)
{
	return a->sn_PeakCount == b->sn_PeakCount && memcmp(a->sn_PeakLooks, b->sn_PeakLooks, a->sn_PeakCount) == 0;
}

/* the tail's levels, measured in its own pass, as the whole read's: the
   filter warmed up a look early */
static int same_tail(const struct Snap *whole, const struct Snap *ends)
{
	ULONG k, n = ends->sn_PeakCount < whole->sn_PeakCount ? ends->sn_PeakCount : whole->sn_PeakCount;

	if (!ends->sn_TailFirst || ends->sn_TailFirst >= n || whole->sn_LookCount != ends->sn_LookCount) return 0;
	for (k = ends->sn_TailFirst; k < n; k++) {
		if (whole->sn_Looks[k] != ends->sn_Looks[k]) {
			printf("  look %lu: %u whole, %u ends first\n", (unsigned long) k, (unsigned) whole->sn_Looks[k],
			       (unsigned) ends->sn_Looks[k]);
			return 0;
		}
	}
	return 1;
}

static void say_plan(const char *what, const struct TrimPlan *tp)
{
	printf("  %s: found %d in %lu out %lu music %lu..%lu clicks %u/%u period %lu ms, %u regions\n", what, (int) tp->tp_Found,
	       (unsigned long) tp->tp_In, (unsigned long) tp->tp_Out, (unsigned long) tp->tp_MusicStart,
	       (unsigned long) tp->tp_MusicEnd, (unsigned) tp->tp_LeadClicks, (unsigned) tp->tp_TailClicks,
	       (unsigned long) tp->tp_PeriodMs, (unsigned) tp->tp_RegionCount);
}

/* the file at path holds the same bytes as the one at other */
static int same_file(const char *path, const char *other)
{
	FILE *a = fopen(path, "rb"), *b = fopen(other, "rb");
	int ca, cb, same = a && b;

	while (same) {
		ca = getc(a);
		cb = getc(b);
		if (ca != cb) same = 0;
		if (ca == EOF) break;
	}
	if (a) fclose(a);
	if (b) fclose(b);
	return same;
}

/* the worn side: whole, then 20 and 30 s ends first, alike */
/* Doc_MeasureLevel's progress: called off at prog_stop per cent */
static int prog_stop = 101;

static BOOL progress(UWORD pc)
{
	return (BOOL) (pc < prog_stop);
}

static void side_checks(const char *from, const char *drawer)
{
	static const ULONG ends[] = { 20, 30 };
	char side[PATH_LEN];
	struct Snap whole, sn;
	struct How hw;
	UWORD i;

	sprintf(side, "%s/Side.wav", drawer);
	CHECK(be_copy(from, side), "side: copied, its samples big-endian");
	hw.hw_Ends = 0;
	hw.hw_Fetch = 0;
	hw.hw_SaveAs = NULL;
	CHECK(read_doc(side, &hw, &whole), "side whole: read");
	CHECK(whole.sn_Plan.tp_Found, "side whole: music found");
	say_plan("side whole", &whole.sn_Plan);
	/* the tracks (Find tracks): three, each half a second before its
	   song -- song 1 fades in from -45 dB, heard a little later */
	{
		const struct TrimPlan *tp = &whole.sn_Plan;
		ULONG r = 44100;

		CHECK(tp->tp_TrackCount == 3, "side whole: three tracks");
		CHECK(tp->tp_TrackStart[0] == tp->tp_In, "side whole: the first track at In");
		CHECK(tp->tp_TrackStart[1] + r / 2 >= r * 405 / 10 - r / 5 && tp->tp_TrackStart[1] + r / 2 <= r * 405 / 10 + r / 5,
		      "side whole: track 2 half a second before 40.5 s");
		CHECK(tp->tp_TrackStart[2] + r / 2 >= r * 843 / 10 - r / 5 && tp->tp_TrackStart[2] + r / 2 <= r * 843 / 10 + r / 5,
		      "side whole: track 3 half a second before 84.3 s");
		printf("  side whole: tracks at %lu %lu %lu\n", (unsigned long) tp->tp_TrackStart[0],
		       (unsigned long) tp->tp_TrackStart[1], (unsigned long) tp->tp_TrackStart[2]);
	}
	/* read ends first: the middle's level not measured, no tracks; then
	   measured (Doc_MeasureLevel): the levels and the tracks as whole */
	{
		static struct TrimPlan tp;
		ULONG k, diff = 0;
		LONG err;

		Doc_SetEnds(20);
		CHECK(Doc_Open(&doc, (CONST_STRPTR) side) == DOC_OK, "side middle: opened");
		while (Doc_Read(&doc)) ;
		CHECK(doc.d_Done && !doc.d_LevelWhole, "side middle: read ends first, the level not whole");
		CHECK(!Doc_Tracks(&doc, &tp), "side middle: no tracks before the middle is measured");
		prog_stop = 40;
		doc.d_SaveProgress = progress;
		err = Doc_MeasureLevel(&doc);
		CHECK(err == DOC_E_CANCELLED && !doc.d_LevelWhole, "side middle: measuring called off at 40 per cent");
		prog_stop = 101;
		err = Doc_MeasureLevel(&doc);
		CHECK(err == 0 && doc.d_LevelWhole, "side middle: measured");
		for (k = 0; k < doc.d_PlanLooks && k < whole.sn_LookCount; k++) if (doc.d_Looks[k] != whole.sn_Looks[k]) diff++;
		CHECK(diff == 0, "side middle: every look's level as whole");
		if (diff) printf("  side middle: %lu looks differ\n", (unsigned long) diff);
		CHECK(Doc_Tracks(&doc, &tp) && tp.tp_TrackCount == 3 && tp.tp_TrackStart[1] == whole.sn_Plan.tp_TrackStart[1] &&
		      tp.tp_TrackStart[2] == whole.sn_Plan.tp_TrackStart[2], "side middle: the tracks as whole");
		CHECK(Doc_MeasureLevel(&doc) == 0, "side middle: measured again, nothing to do");
		Doc_Close(&doc);
	}
	for (i = 0; i < 2; i++) {
		hw.hw_Ends = ends[i];
		CHECK(read_doc(side, &hw, &sn), "side ends: read");
		CHECK(sn.sn_Phased && sn.sn_Early, "side ends: ends first, the plan before the picture");
		CHECK(same_plan(&whole.sn_Plan, &sn.sn_Plan), "side ends: the suggestion as whole");
		CHECK(same_picture(&whole, &sn) && same_peaks(&whole, &sn), "side ends: the picture and the plain peaks as whole");
		CHECK(same_tail(&whole, &sn), "side ends: the tail's levels as whole");
		if (!same_plan(&whole.sn_Plan, &sn.sn_Plan)) say_plan(ends[i] == 20 ? "side ends 20" : "side ends 30", &sn.sn_Plan);
		free_snap(&sn);
	}
	free_snap(&whole);
	remove(side);
}

int main(int argc, char **argv)
{
	char single[PATH_LEN], early[PATH_LEN], late[PATH_LEN];
	struct Snap whole, ends, mid, short_ends, tail_only;
	struct How hw;
	struct DocSave ds;
	LONG err;

	if (argc < 3) return 2;
	dos_host.dh_WriteLeft = -1;		/* the disk never full */
	sprintf(single, "%s/Ends.wav", argv[1]);
	sprintf(early, "%s/Ends-early.wav", argv[1]);
	sprintf(late, "%s/Ends-late.wav", argv[1]);
	CHECK(be_copy(argv[2], single), "the single copied, its samples big-endian");

	/* the whole file, as before: the reference */
	hw.hw_Ends = 0;
	hw.hw_Fetch = 0;
	hw.hw_SaveAs = NULL;
	CHECK(read_doc(single, &hw, &whole), "whole: read");
	CHECK(!whole.sn_Phased && !whole.sn_Early, "whole: once through, the plan at the end");
	CHECK(whole.sn_PercentOk, "whole: the per cent never back, 100 at the end");
	CHECK(whole.sn_Plan.tp_Found && whole.sn_Plan.tp_In == 158760 && whole.sn_Plan.tp_Out == 1582308,
	      "whole: In 3.6 s, Out 35.88 s (the single's)");
	CHECK(whole.sn_Plan.tp_LeadClicks == 8 && whole.sn_Plan.tp_TailClicks == 4, "whole: 8 clicks in, 4 out");
	say_plan("whole", &whole.sn_Plan);

	/* the default ends: the single is too short for them */
	hw.hw_Ends = 90;
	CHECK(read_doc(single, &hw, &ends), "ends 90: read");
	CHECK(!ends.sn_Phased, "ends 90: a 40 s file read whole");
	CHECK(same_plan(&whole.sn_Plan, &ends.sn_Plan) && same_picture(&whole, &ends) && same_peaks(&whole, &ends),
	      "ends 90: as whole");
	free_snap(&ends);

	/* 10 s each end: the head, the tail, then the rest */
	hw.hw_Ends = 10;
	CHECK(read_doc(single, &hw, &ends), "ends 10: read");
	CHECK(ends.sn_Phased, "ends 10: ends first");
	CHECK(ends.sn_Early, "ends 10: the plan ready before the picture");
	CHECK(!ends.sn_ReplanEarly, "ends 10: no replan while it is read");
	CHECK(ends.sn_PercentOk, "ends 10: the per cent never back, 100 at the end");
	CHECK(same_plan(&whole.sn_Plan, &ends.sn_Plan), "ends 10: the suggestion as whole");
	CHECK(same_picture(&whole, &ends), "ends 10: the picture as whole");
	CHECK(same_peaks(&whole, &ends), "ends 10: the plain peaks as whole");
	CHECK(same_tail(&whole, &ends), "ends 10: the tail's levels as whole (its filter warmed up)");
	CHECK(ends.sn_Frames == whole.sn_Frames, "ends 10: the frames as whole");
	say_plan("ends 10", &ends.sn_Plan);

	/* a fetch in the tail's pass, a save once the plan is ready: the
	   reading goes on where it was */
	hw.hw_Fetch = 1;
	hw.hw_SaveAs = early;
	CHECK(read_doc(single, &hw, &mid), "mid: read");
	CHECK(mid.sn_FetchOk == 1, "mid: the middle fetched in the tail's pass");
	CHECK(mid.sn_SaveErr == 0, "mid: saved before the picture was done");
	CHECK(same_plan(&whole.sn_Plan, &mid.sn_Plan) && same_picture(&whole, &mid) && same_peaks(&whole, &mid),
	      "mid: the suggestion, the picture, the plain peaks as whole");
	/* the same save made from the whole read: the same bytes */
	hw.hw_Ends = 0;
	hw.hw_Fetch = 0;
	hw.hw_SaveAs = NULL;
	Doc_SetEnds(0);
	CHECK(Doc_Open(&doc, (CONST_STRPTR) single) == DOC_OK, "late: opened");
	while (Doc_Read(&doc)) ;
	ds.ds_In = doc.d_Plan.tp_In;
	ds.ds_Out = doc.d_Plan.tp_Out;
	ds.ds_Path = (CONST_STRPTR) late;
	err = Doc_SaveAs(&doc, &ds);
	CHECK(err == 0, "late: saved");
	if (err) printf("  late: error %ld\n", (long) err);
	/* the margins changed (Settings, Lead and tail): the suggestion made
	   again from the same looks -- the music where it was, In a second
	   earlier, Out two later (a look either way: 40 ms); back to 500 and
	   2000, the suggestion as it was */
	{
		struct TrimPlan was;
		ULONG look = doc.d_LL.ll_Frames, rate = doc.d_Rate;

		was = doc.d_Plan;
		Doc_SetMargins(1500, 4000);
		CHECK(Doc_Replan(&doc), "replan: made again");
		CHECK(doc.d_Plan.tp_Found && doc.d_Plan.tp_MusicStart == was.tp_MusicStart && doc.d_Plan.tp_MusicEnd == was.tp_MusicEnd,
		      "replan: the music where it was");
		CHECK(doc.d_Plan.tp_In + rate <= was.tp_In + look && doc.d_Plan.tp_In + rate + look >= was.tp_In,
		      "replan: In a second earlier");
		CHECK(doc.d_Plan.tp_Out <= was.tp_Out + 2 * rate + look && doc.d_Plan.tp_Out + look >= was.tp_Out + 2 * rate,
		      "replan: Out two seconds later");
		printf("  replan 1500/4000: in %lu out %lu (was %lu %lu)\n", (unsigned long) doc.d_Plan.tp_In,
		       (unsigned long) doc.d_Plan.tp_Out, (unsigned long) was.tp_In, (unsigned long) was.tp_Out);
		Doc_SetMargins(0, 30000);
		CHECK(Doc_Replan(&doc) && doc.d_Plan.tp_Out == doc.d_Frames && doc.d_Plan.tp_In <= doc.d_Plan.tp_MusicStart,
		      "replan: a tail past the end ends at the end");
		Doc_SetMargins(500, 2000);
		CHECK(Doc_Replan(&doc) && same_plan(&was, &doc.d_Plan), "replan: 500 and 2000 again, the suggestion as it was");
	}
	Doc_Close(&doc);
	/* not before the reading is done */
	CHECK(Doc_Open(&doc, (CONST_STRPTR) single) == DOC_OK && !Doc_Replan(&doc), "replan: none while it is read");
	Doc_Close(&doc);
	CHECK(same_file(early, late), "mid: the early save byte for byte the late one");
	free_snap(&mid);

	/* 3 s each end: the music begins after the head and ends before the
	   tail -- the level measured on, the plan at the end, as whole */
	hw.hw_Ends = 3;
	CHECK(read_doc(single, &hw, &short_ends), "ends 3: read");
	CHECK(short_ends.sn_Phased && !short_ends.sn_Early, "ends 3: ends first, but the plan at the end");
	CHECK(same_plan(&whole.sn_Plan, &short_ends.sn_Plan) && same_picture(&whole, &short_ends) && same_peaks(&whole, &short_ends),
	      "ends 3: as whole");
	free_snap(&short_ends);

	/* 5 s: the head holds the music's start, the tail (35..40 s) only
	   run-out -- the run-out left running: the plan at the end */
	hw.hw_Ends = 5;
	CHECK(read_doc(single, &hw, &tail_only), "ends 5: read");
	CHECK(tail_only.sn_Phased && !tail_only.sn_Early, "ends 5: ends first, but the plan at the end");
	CHECK(same_plan(&whole.sn_Plan, &tail_only.sn_Plan) && same_picture(&whole, &tail_only) && same_peaks(&whole, &tail_only),
	      "ends 5: as whole");
	free_snap(&tail_only);

	free_snap(&ends);
	free_snap(&whole);
	if (argc > 3) side_checks(argv[3], argv[1]);
	Doc_SetEnds(90);
	CHECK(!dos_host.dh_Vecs && !dos_host.dh_Handles && !dos_host.dh_Locks && !dos_host.dh_Objs, "nothing held");
	remove(single);
	remove(early);
	remove(late);
	return CHECK_DONE("test_docends");
}
