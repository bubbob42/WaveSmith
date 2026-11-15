/*
 * doc.c -- the open file (doc.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "doc.h"
#include "waveplan.h"
#include "levelmap.h"
#include "textutil.h"
#include "beside.h"
#include "envelope.h"
#include "boopsi.h"

#define HEAD_MAX	4096UL		/* the header is read from this much ... */
#define HEAD_BIG	65536UL		/* ... or this, when its data chunk lies beyond
					   (bext, iXML, a long LIST before the samples) */
#define BUF_BYTES	65536UL
#define ZERO_MAX	32768UL		/* frames a zero-crossing search reads */
#define LEAD_MS		500
#define TAIL_MS		2000
#define ENDS_S		90		/* the ends a long file's level is measured over */

static ULONG ends_s = ENDS_S;
static ULONG lead_ms = LEAD_MS, tail_ms = TAIL_MS;
static BOOL keep_backup = FALSE;

void Doc_SetEnds(ULONG seconds)
{
	ends_s = seconds;
}

void Doc_SetMargins(ULONG lead, ULONG tail)
{
	lead_ms = lead;
	tail_ms = tail;
}

void Doc_SetBackup(BOOL keep)
{
	keep_backup = keep;
}

static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

/* the words as loaded (the file's little-endian bytes) in the machine's order */
static void swap_words(UWORD *w, ULONG n)
{
	while (n--) {
		*w = (UWORD) ((*w >> 8) | (*w << 8));
		w++;
	}
}

static void copy_path(struct Doc *d, CONST_STRPTR path)
{
	UWORD i;

	for (i = 0; path[i] && i < DOC_PATHLEN - 1; i++) d->d_Path[i] = path[i];
	d->d_Path[i] = '\0';
}

/* ---- the picture along the timeline ---- */

#define PEAK_STEP_MIN	(1UL << WAVEPEAKS_SHIFT0)	/* the picture's finest */

/* a stretch of the file's picture, its extremes channel after channel
   into mnmx -- a stretch shorter than the picture's finest asked as wide
   (an entry's worth): FALSE where the picture is not there yet */
static BOOL part_peaks(const struct Doc *d, ULONG from, ULONG frames, BYTE *mnmx)
{
	struct WavePeakMsg q;

	q.wpm_First = from;
	q.wpm_Step = frames < PEAK_STEP_MIN ? PEAK_STEP_MIN : frames;
	q.wpm_Count = 1;
	q.wpm_Out = mnmx;
	return (BOOL) (WavePeaks_Query(&d->d_Pic.dp_Peaks, &q) == 1);
}

/* one column a .. a + step - 1 that crosses pieces: each part's extremes
   together, a silence's 0, 0: FALSE where the picture is not there yet */
static BOOL column(const struct Doc *d, ULONG a, ULONG step, BYTE *out)
{
	struct TlAt at;
	BYTE part[2 * WAVEPEAKS_CHANNELS];
	ULONG end = a + step, n;
	UWORD c, rowb = (UWORD) (2 * d->d_Channels);

	if (end < a || end > d->d_Tl.tl_Frames) end = d->d_Tl.tl_Frames;
	for (c = 0; c < rowb; c += 2) {
		out[c] = 127;
		out[c + 1] = -128;
	}
	while (a < end && Tl_At(&d->d_Tl, a, &at)) {
		n = at.ta_Run < end - a ? at.ta_Run : end - a;
		if (at.ta_From == TL_SILENCE) {
			for (c = 0; c < rowb; c += 2) {
				if (out[c] > 0) out[c] = 0;
				if (out[c + 1] < 0) out[c + 1] = 0;
			}
		} else {
			if (!part_peaks(d, at.ta_From, n, part)) return FALSE;
			for (c = 0; c < rowb; c += 2) {
				if (part[c] < out[c]) out[c] = part[c];
				if (part[c + 1] > out[c + 1]) out[c + 1] = part[c + 1];
			}
		}
		a += n;
	}
	return TRUE;
}

/* columns at their level (concept 3.2): each one's extremes times the
   highest level in it -- its first, middle and last frame's -- held at
   the picture's full scale, where the gadget's clip mark shows them; a
   column as recorded left alone */
struct ColRun {
	ULONG cr_First, cr_Step, cr_Count;	/* timeline frames, columns */
	BYTE *cr_Out;
};

static void level_cols(const struct Doc *d, const struct ColRun *cr)
{
	const struct TlLevels *lv = Tl_Levels(&d->d_Levels);
	ULONG j, t, n, end = d->d_Tl.tl_Frames;
	UWORD rowb = (UWORD) (2 * d->d_Channels), c, l, m;
	BYTE *out = cr->cr_Out;
	LONG v;

	for (j = 0; j < cr->cr_Count; j++, out += rowb) {
		t = cr->cr_First + j * cr->cr_Step;
		if (t >= end) break;
		n = cr->cr_Step < end - t ? cr->cr_Step : end - t;
		if (!n || Tl_Untouched(&d->d_Tl, lv, t, n)) continue;
		l = Tl_LevelAt(&d->d_Tl, lv, t);
		if ((m = Tl_LevelAt(&d->d_Tl, lv, t + n / 2)) > l) l = m;
		if ((m = Tl_LevelAt(&d->d_Tl, lv, t + n - 1)) > l) l = m;
		for (c = 0; c < rowb; c++) {
			v = ((LONG) out[c] * (LONG) l + 2048L) >> 12;
			out[c] = (BYTE) (v > 127L ? 127 : v < -128L ? -128 : v);
		}
	}
}

/* the columns of a cut timeline: a run of them inside one piece asked of
   the picture at once (or silence), a column across pieces put together;
   past the end 0, 0 as the picture gives; at their level; the columns
   answered */
static ULONG tl_query(const struct Doc *d, struct WavePeakMsg *q)
{
	struct WavePeakMsg sub;
	struct TlAt at;
	struct ColRun cr;
	ULONG i = 0, a, k, n, step = q->wpm_Step;
	UWORD rowb = (UWORD) (2 * d->d_Channels);
	BYTE *out;

	if (!step) return 0;
	while (i < q->wpm_Count) {
		if (i && step > (0xFFFFFFFFUL - q->wpm_First) / i) break;
		a = q->wpm_First + i * step;
		out = q->wpm_Out + i * rowb;
		if (!Tl_At(&d->d_Tl, a, &at)) {
			for (n = 0; n < rowb; n++) out[n] = 0;
			i++;
			continue;
		}
		if ((k = at.ta_Run / step) > q->wpm_Count - i) k = q->wpm_Count - i;
		if (!k) {
			if (!column(d, a, step, out)) break;
			cr.cr_First = a;
			cr.cr_Step = step;
			cr.cr_Count = 1;
			cr.cr_Out = out;
			level_cols(d, &cr);
			i++;
		} else if (at.ta_From == TL_SILENCE) {
			for (n = 0; n < k * rowb; n++) out[n] = 0;
			i += k;
		} else {
			sub.wpm_First = at.ta_From;
			sub.wpm_Step = step;
			sub.wpm_Count = k;
			sub.wpm_Out = out;
			n = WavePeaks_Query(&d->d_Pic.dp_Peaks, &sub);
			cr.cr_First = a;
			cr.cr_Step = step;
			cr.cr_Count = n;
			cr.cr_Out = out;
			level_cols(d, &cr);
			i += n;
			if (n < k) break;
		}
	}
	return i;
}

/* the gadget's source (A0 the hook, A2 the object, A1 the message): the
   whole file asked of the picture as it is */
static DISP_QUAL ULONG tl_peaks(REG(a0, struct Hook *h), REG(a2, Object *o), REG(a1, struct WavePeakMsg *q))
{
	const struct Doc *d = (const struct Doc *) h->h_Data;

	(void) o;
	if (Tl_Whole(&d->d_Tl, d->d_Frames) && !Tl_Levels(&d->d_Levels)) return WavePeaks_Query(&d->d_Pic.dp_Peaks, q);
	return tl_query(d, q);
}

/* the memory of a picture given back; the picture cleared (so a second
   call gives back nothing) */
static void picture_free(struct DocPicture *pic)
{
	UWORD l;

	if (pic->dp_PeakLooks) FreeVec(pic->dp_PeakLooks);
	if (pic->dp_PeakWords) FreeVec(pic->dp_PeakWords);
	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		if (pic->dp_Peaks.wp_Level[l]) FreeVec(pic->dp_Peaks.wp_Level[l]);
	}
	clear_bytes((UBYTE *) pic, sizeof(*pic));
}

/* d's picture made anew and empty for a sound of frames frames, its looks
   d_LL's: DOC_OK; else a DOC_E_#?, nothing held and d_Pic cleared. Doc_Open
   and a save's new picture alike */
static LONG picture_new(struct Doc *d, ULONG frames)
{
	struct DocPicture *pic = &d->d_Pic;
	BOOL ok;
	UWORD l;
	ULONG n;

	clear_bytes((UBYTE *) pic, sizeof(*pic));
	if (!d->d_LL.ll_Frames || !WavePeaks_Init(&pic->dp_Peaks, frames, d->d_Channels)) return DOC_E_NOTPCM16;
	pic->dp_LookMax = frames / d->d_LL.ll_Frames + 2;
	pic->dp_PeakLooks = (UBYTE *) AllocVec(pic->dp_LookMax, MEMF_ANY | MEMF_CLEAR);
	pic->dp_PeakWords = (UWORD *) AllocVec(pic->dp_LookMax * 2, MEMF_ANY | MEMF_CLEAR);
	ok = (BOOL) (pic->dp_PeakLooks && pic->dp_PeakWords);
	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		n = WavePeaks_Bytes(&pic->dp_Peaks, l);
		if (!(pic->dp_Peaks.wp_Level[l] = (BYTE *) AllocVec(n ? n : 4, MEMF_ANY | MEMF_CLEAR))) ok = FALSE;
	}
	if (ok) return DOC_OK;
	picture_free(pic);
	return DOC_E_NOMEM;
}

/* the gadget's source: the picture drawn along the timeline */
static void source_set(struct Doc *d)
{
	d->d_Source.ws_Peaks.h_Entry = (ULONG (*)()) tl_peaks;
	d->d_Source.ws_Peaks.h_SubEntry = NULL;
	d->d_Source.ws_Peaks.h_Data = d;
	d->d_Source.ws_Frames = d->d_Tl.tl_Frames;
	d->d_Source.ws_Channels = d->d_Channels;
	d->d_Source.ws_Flags = 0;
	d->d_Source.ws_Finest = PEAK_STEP_MIN;
	d->d_Source.ws_Rate = d->d_Rate;
}

ULONG Doc_Length(const struct Doc *d)
{
	return d->d_Tl.tl_Frames;
}

void Doc_Changed(struct Doc *d)
{
	d->d_Source.ws_Frames = d->d_Tl.tl_Frames;
}

/* the looks' measure, the picture, the crackle-proof looks */
static LONG alloc_looks(struct Doc *d)
{
	LONG err;

	if (!LookLevel_Init(&d->d_LL, d->d_Rate, d->d_Channels)) return DOC_E_NOTPCM16;
	if ((err = picture_new(d, d->d_Frames)) != DOC_OK) return err;
	if (!(d->d_Looks = (UBYTE *) AllocVec(d->d_Pic.dp_LookMax, MEMF_ANY | MEMF_CLEAR))) return DOC_E_NOMEM;
	d->d_LL.ll_Out = d->d_Looks;
	d->d_LL.ll_OutMax = d->d_Pic.dp_LookMax;
	return DOC_OK;
}

/* a long file read ends first: the head's end and the tail's start, each
   on a look; a file not longer than both ends and the warm-up look once
   through */
static void plan_phases(struct Doc *d)
{
	ULONG lf = d->d_LL.ll_Frames, ends, first;

	d->d_Phase = DOC_READ_ALL;
	if (!ends_s || !lf || ends_s > 0xFFFFFFFFUL / d->d_Rate) return;
	ends = ends_s * d->d_Rate;
	if (d->d_Frames < 4 * lf || ends > (d->d_Frames - 4 * lf) / 2) return;
	d->d_HeadEnd = ends / lf * lf;
	first = (d->d_Frames - ends + lf - 1) / lf;	/* the tail's first look */
	d->d_TailFrom = (first - 1) * lf;
	if (d->d_TailFrom <= d->d_HeadEnd) return;
	d->d_Phase = DOC_READ_HEAD;
}

LONG Doc_Open(struct Doc *d, CONST_STRPTR path)
{
	LONG n, end, err;
	UWORD r;

	clear_bytes((UBYTE *) d, sizeof(*d));
	copy_path(d, path);
	if (!(d->d_FH = Open((STRPTR) d->d_Path, MODE_OLDFILE))) return IoErr();
	if (!(d->d_Head = (UBYTE *) AllocVec(HEAD_MAX, MEMF_ANY)) || !(d->d_Buf = (UBYTE *) AllocVec(BUF_BYTES, MEMF_ANY)))
		return DOC_E_NOMEM;
	d->d_HeadMax = HEAD_MAX;
	if ((n = Read(d->d_FH, d->d_Head, (LONG) HEAD_MAX)) < 12) return n < 0 ? IoErr() : DOC_E_NOTWAV;
	r = NormWav_Parse(d->d_Head, (ULONG) n, &d->d_Wav);
	if (r == NORMWAV_NODATA && (ULONG) n == HEAD_MAX) {
		/* the samples further in: the header read again, more of it */
		FreeVec(d->d_Head);
		if (!(d->d_Head = (UBYTE *) AllocVec(HEAD_BIG, MEMF_ANY))) return DOC_E_NOMEM;
		d->d_HeadMax = HEAD_BIG;
		if (Seek(d->d_FH, 0, OFFSET_BEGINNING) < 0 || (n = Read(d->d_FH, d->d_Head, (LONG) HEAD_BIG)) < 12)
			return n < 0 ? IoErr() : DOC_E_NOTWAV;
		r = NormWav_Parse(d->d_Head, (ULONG) n, &d->d_Wav);
	}
	if (r != NORMWAV_OK) return r == NORMWAV_NOTWAV ? DOC_E_NOTWAV : DOC_E_NOTPCM16;
	if (d->d_Wav.nw_Channels < 1 || d->d_Wav.nw_Channels > 2) return DOC_E_NOTPCM16;
	d->d_Channels = d->d_Wav.nw_Channels;
	d->d_Rate = d->d_Wav.nw_Rate;
	/* the samples' bytes: the header's, or to the file's end; whole frames */
	/* (a file of 2 GB or more: Seek answers past LONG -- refused, never
	   taken for DOC_OK with nothing set up) */
	if ((end = Seek(d->d_FH, 0, OFFSET_END)) < 0 || (end = Seek(d->d_FH, 0, OFFSET_BEGINNING)) < 0) {
		LONG e = IoErr();

		return e ? e : ERROR_SEEK_ERROR;
	}
	if ((ULONG) end < d->d_Wav.nw_DataStart) return DOC_E_EMPTY;
	if (d->d_Wav.nw_DataBytes == NORMWAV_TOEND || d->d_Wav.nw_DataBytes > (ULONG) end - d->d_Wav.nw_DataStart)
		d->d_Wav.nw_DataBytes = (ULONG) end - d->d_Wav.nw_DataStart;
	d->d_Frames = d->d_Wav.nw_DataBytes / d->d_Wav.nw_Align;
	if (!d->d_Frames) return DOC_E_EMPTY;
	/* what follows the samples (and their pad byte): kept by a save */
	n = (LONG) (d->d_Wav.nw_DataStart + d->d_Wav.nw_DataBytes + (d->d_Wav.nw_DataBytes & 1));
	d->d_Trailer = (ULONG) end > (ULONG) n ? (ULONG) end - (ULONG) n : 0;
	Tl_Init(&d->d_Tl, d->d_Frames);
	if ((err = alloc_looks(d)) != DOC_OK) return err;
	source_set(d);
	plan_phases(d);
	if (Seek(d->d_FH, (LONG) d->d_Wav.nw_DataStart, OFFSET_BEGINNING) < 0) {
		LONG e = IoErr();

		return e ? e : ERROR_SEEK_ERROR;
	}
	return DOC_OK;
}

/* the top byte of a sample, as WavePeaks_Top() has it */
#define TOP(v)		((BYTE) ((LONG) (((ULONG) ((LONG) (v) + 32768L)) >> 8) - 128L))
/* a word as it lies in the file (little-endian), in the machine's order */
#define LEWORD(w)	((LONG) (WORD) (UWORD) (((w) << 8) | ((w) >> 8)))

/* a run's extremes into the plain peaks (the look's) and the picture:
   one minimum and maximum a channel serve both -- |v| at most is the
   larger of -min and max, the top byte's extremes those of the extremes */
static void run_done(struct Doc *d, const LONG *mm, ULONG run)
{
	BYTE tb[4];
	ULONG a;
	UWORD c;

	for (c = 0; c < d->d_Channels; c++) {
		a = (ULONG) (-mm[c * 2] > mm[c * 2 + 1] ? -mm[c * 2] : mm[c * 2 + 1]);
		if (a > 32767) a = 32767;		/* -32768 counts as 32767 */
		if (c == 0 && a > d->d_Pic.dp_PeakL) d->d_Pic.dp_PeakL = a;
		if (c == 1 && a > d->d_Pic.dp_PeakR) d->d_Pic.dp_PeakR = a;
		tb[c * 2] = TOP(mm[c * 2]);
		tb[c * 2 + 1] = TOP(mm[c * 2 + 1]);
	}
	if (!d->d_Pic.dp_NoPicture && WavePeaks_RunLeft(&d->d_Pic.dp_Peaks)) WavePeaks_FeedRun(&d->d_Pic.dp_Peaks, run, tb);
	d->d_Pic.dp_PeakIn += run;
	if (d->d_Pic.dp_PeakIn == d->d_LL.ll_Frames) {
		if (d->d_Pic.dp_PeakCount < d->d_Pic.dp_LookMax) {
			d->d_Pic.dp_PeakWords[d->d_Pic.dp_PeakCount] = (UWORD) (d->d_Pic.dp_PeakL > d->d_Pic.dp_PeakR ? d->d_Pic.dp_PeakL : d->d_Pic.dp_PeakR);
			d->d_Pic.dp_PeakLooks[d->d_Pic.dp_PeakCount++] = LevelMap_Byte((d->d_Pic.dp_PeakL << 16) | d->d_Pic.dp_PeakR, (BOOL) (d->d_Channels == 1));
		}
		d->d_Pic.dp_PeakIn = 0;
		d->d_Pic.dp_PeakL = d->d_Pic.dp_PeakR = 0;
	}
}

/* frames once through as they lie in the file, in runs to the next look's
   or picture entry's end: each channel's extremes in registers, the word
   turned on the way (no pass to turn them, none for each measure) */
static void read_runs(struct Doc *d, const UWORD *w, ULONG frames)
{
	ULONG run, k;
	LONG v, mm[4];

	while (frames) {
		run = d->d_LL.ll_Frames - d->d_Pic.dp_PeakIn;
		k = d->d_Pic.dp_NoPicture ? 0 : WavePeaks_RunLeft(&d->d_Pic.dp_Peaks);
		if (k && k < run) run = k;
		if (run > frames) run = frames;
		frames -= run;
		mm[0] = mm[2] = 32767;
		mm[1] = mm[3] = -32768;
		if (d->d_Channels == 2) {
			for (k = run; k; k--, w += 2) {
				v = LEWORD(w[0]);
				if (v < mm[0]) mm[0] = v;
				if (v > mm[1]) mm[1] = v;
				v = LEWORD(w[1]);
				if (v < mm[2]) mm[2] = v;
				if (v > mm[3]) mm[3] = v;
			}
		} else {
			for (k = run; k; k--, w++) {
				v = LEWORD(w[0]);
				if (v < mm[0]) mm[0] = v;
				if (v > mm[1]) mm[1] = v;
			}
		}
		run_done(d, mm, run);
	}
	if (!d->d_Pic.dp_NoPicture) WavePeaks_Flush(&d->d_Pic.dp_Peaks);
}

/* the plan from the looks so far (count of them) */
/* the looks the plan is made from, the margins with them */
static void looks_of(const struct Doc *d, struct TrimLooks *tl, ULONG count)
{
	tl->tl_Level = d->d_Looks;
	tl->tl_Peak = d->d_Pic.dp_PeakLooks;
	tl->tl_Count = count;
	tl->tl_LookFrames = d->d_LL.ll_Frames;
	tl->tl_Frames = d->d_Frames;
	tl->tl_Rate = d->d_Rate;
	tl->tl_LeadMs = lead_ms;
	tl->tl_TailMs = tail_ms;
}

static void plan_now(struct Doc *d, ULONG count)
{
	struct TrimLooks tl;

	looks_of(d, &tl, count);
	TrimPlan_Make(&tl, &d->d_Plan);
	d->d_PlanReady = TRUE;
	d->d_PlanLooks = count;
}

/* a read that failed after the ends: the sound is shorter */
static void plan_fits(struct Doc *d)
{
	if (d->d_Plan.tp_Out > d->d_Frames) d->d_Plan.tp_Out = d->d_Frames;
	if (d->d_Plan.tp_In > d->d_Plan.tp_Out) d->d_Plan.tp_In = d->d_Plan.tp_Out;
}

BOOL Doc_Replan(struct Doc *d)
{
	if (!d->d_Done || !d->d_PlanLooks) return FALSE;
	plan_now(d, d->d_PlanLooks);
	plan_fits(d);
	return TRUE;
}

/* the last plain peak, a look begun */
static void last_peak(struct Doc *d)
{
	if (d->d_Pic.dp_PeakIn && d->d_Pic.dp_PeakCount < d->d_Pic.dp_LookMax) {
		d->d_Pic.dp_PeakWords[d->d_Pic.dp_PeakCount] = (UWORD) (d->d_Pic.dp_PeakL > d->d_Pic.dp_PeakR ? d->d_Pic.dp_PeakL : d->d_Pic.dp_PeakR);
		d->d_Pic.dp_PeakLooks[d->d_Pic.dp_PeakCount++] = LevelMap_Byte((d->d_Pic.dp_PeakL << 16) | d->d_Pic.dp_PeakR, (BOOL) (d->d_Channels == 1));
	}
}

static void finish(struct Doc *d)
{
	LookLevel_Finish(&d->d_LL);
	last_peak(d);
	/* read ends first, the plan was made from them; else now */
	if (!d->d_PlanReady)
		plan_now(d, d->d_LL.ll_OutCount < d->d_Pic.dp_PeakCount ? d->d_LL.ll_OutCount : d->d_Pic.dp_PeakCount);
	plan_fits(d);
	/* measured to the end: read once through, or a head that is the whole
	   (shortened) file -- not after a read that failed */
	if ((d->d_Phase == DOC_READ_ALL || d->d_Phase == DOC_READ_HEAD) && !d->d_ReadError) d->d_LevelWhole = TRUE;
	d->d_Done = TRUE;
}

/* the head read: on to the tail, the plain peaks' place kept for the
   rest; its looks from the warm-up look on */
static BOOL start_tail(struct Doc *d)
{
	ULONG first = d->d_TailFrom / d->d_LL.ll_Frames;

	d->d_KeepCount = d->d_Pic.dp_PeakCount;
	d->d_KeepIn = d->d_Pic.dp_PeakIn;
	d->d_KeepL = d->d_Pic.dp_PeakL;
	d->d_KeepR = d->d_Pic.dp_PeakR;
	d->d_Pic.dp_PeakCount = first;
	d->d_Pic.dp_PeakIn = d->d_Pic.dp_PeakL = d->d_Pic.dp_PeakR = 0;
	LookLevel_Init(&d->d_TailLL, d->d_Rate, d->d_Channels);
	d->d_TailLL.ll_Out = d->d_Looks + first;
	d->d_TailLL.ll_OutMax = d->d_Pic.dp_LookMax - first;
	d->d_TailAt = d->d_TailFrom;
	if (Seek(d->d_FH, (LONG) (d->d_Wav.nw_DataStart + d->d_TailFrom * d->d_Wav.nw_Align), OFFSET_BEGINNING) < 0) {
		/* no tail first: the whole file through, as a short one */
		d->d_Pic.dp_PeakCount = d->d_KeepCount;
		d->d_Pic.dp_PeakIn = d->d_KeepIn;
		d->d_Pic.dp_PeakL = d->d_KeepL;
		d->d_Pic.dp_PeakR = d->d_KeepR;
		d->d_Phase = DOC_READ_ALL;
		return TRUE;
	}
	d->d_Phase = DOC_READ_TAIL;
	return TRUE;
}

/* the tail read: the middle, never measured, counts as music (0 dB down,
   the warm-up look with it), the plan made; back to the head's end for
   the rest */
static BOOL end_tail(struct Doc *d)
{
	ULONG k, first = d->d_TailFrom / d->d_LL.ll_Frames + 1, count;

	LookLevel_Finish(&d->d_TailLL);
	last_peak(d);
	count = first - 1 + d->d_TailLL.ll_OutCount;
	if (count > d->d_Pic.dp_PeakCount) count = d->d_Pic.dp_PeakCount;
	for (k = d->d_LL.ll_OutCount; k < first && k < d->d_Pic.dp_LookMax; k++) {
		d->d_Looks[k] = d->d_Pic.dp_PeakLooks[k] = 0;
		d->d_Pic.dp_PeakWords[k] = 0;
	}
	plan_now(d, count);
	/* the music began after the head, or ended before the tail (a run-out
	   left running for minutes): the middle's stand-in decided an end, so
	   the level is measured on through the rest, as a short file's, and
	   the plan made at the end */
	if (!d->d_Plan.tp_Found || d->d_Plan.tp_MusicStart >= d->d_HeadEnd ||
	    d->d_Plan.tp_MusicEnd <= first * d->d_LL.ll_Frames)
		d->d_PlanReady = FALSE;
	/* measured on through the rest, the level is whole at the end; else
	   its middle is not measured (Doc_MeasureLevel) */
	d->d_LevelWhole = (BOOL) !d->d_PlanReady;
	d->d_Pic.dp_PeakCount = d->d_KeepCount;
	d->d_Pic.dp_PeakIn = d->d_KeepIn;
	d->d_Pic.dp_PeakL = d->d_KeepL;
	d->d_Pic.dp_PeakR = d->d_KeepR;
	d->d_Pic.dp_NoPicture = FALSE;
	d->d_Phase = DOC_READ_REST;
	if (Seek(d->d_FH, (LONG) (d->d_Wav.nw_DataStart + d->d_Read * d->d_Wav.nw_Align), OFFSET_BEGINNING) < 0)
		d->d_ReadError = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
	return TRUE;
}

/* a piece of the tail: its level and plain peaks, no picture */
static BOOL read_tail(struct Doc *d)
{
	ULONG align = d->d_Wav.nw_Align, frames;
	LONG n;

	frames = (BUF_BYTES / align < d->d_Frames - d->d_TailAt) ? BUF_BYTES / align : d->d_Frames - d->d_TailAt;
	n = 0;
	if (frames == 0 || (n = Read(d->d_FH, d->d_Buf, (LONG) (frames * align))) <= 0) {
		if (frames && n < 0) d->d_ReadError = IoErr();
		return end_tail(d);
	}
	frames = (ULONG) n / align;
	LookLevel_Feed(&d->d_TailLL, (const UWORD *) d->d_Buf, frames);
	d->d_Pic.dp_NoPicture = TRUE;
	read_runs(d, (const UWORD *) d->d_Buf, frames);
	d->d_TailAt += frames;
	return TRUE;
}

BOOL Doc_Read(struct Doc *d)
{
	ULONG align = d->d_Wav.nw_Align, frames, bytes, limit;
	LONG n;

	if (d->d_Done) return FALSE;
	if (d->d_ReadError) {
		/* the file would not be read on (a seek or a read failed): the
		   picture as far as it came, the sound as long as it is */
		finish(d);
		return FALSE;
	}
	if (d->d_Phase == DOC_READ_TAIL) return read_tail(d);
	limit = d->d_Phase == DOC_READ_HEAD ? d->d_HeadEnd : d->d_Frames;
	frames = (BUF_BYTES / align < limit - d->d_Read) ? BUF_BYTES / align : limit - d->d_Read;
	if (frames == 0 && d->d_Phase == DOC_READ_HEAD) return start_tail(d);
	bytes = frames * align;
	n = 0;
	if (frames == 0 || (n = Read(d->d_FH, d->d_Buf, (LONG) bytes)) <= 0) {
		/* a read that failed: an error, the sound as long as it is (a
		   save copies from the file, the picture is what is missing);
		   the file ending early: what was read is the sound -- the
		   timeline too while it is the whole file */
		if (frames && n < 0) d->d_ReadError = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
		else if (d->d_Read && d->d_Read < d->d_Frames) {
			if (Tl_Whole(&d->d_Tl, d->d_Frames)) Tl_Init(&d->d_Tl, d->d_Read);
			d->d_Frames = d->d_Read;
			Doc_Changed(d);
		}
		finish(d);
		return FALSE;
	}
	frames = (ULONG) n / align;
	/* looklevel takes the words as they lie in the file, and so do the
	   plain peaks and the picture, in one pass; the rest of a long file
	   is read for the picture only (its level came from the ends) */
	if (d->d_Phase != DOC_READ_REST || !d->d_PlanReady) LookLevel_Feed(&d->d_LL, (const UWORD *) d->d_Buf, frames);
	read_runs(d, (const UWORD *) d->d_Buf, frames);
	d->d_Read += frames;
	if (d->d_Read >= d->d_Frames) {
		finish(d);
		return FALSE;
	}
	return TRUE;
}

LONG Doc_MeasureLevel(struct Doc *d)
{
	struct LookLevel ll;
	ULONG lf = d->d_LL.ll_Frames, align = d->d_Wav.nw_Align, k0, f, end, frames, k;
	UBYTE *meas;
	LONG n, err = 0;
	UWORD pc, said = 0xFFFF;

	if (!d->d_Done || !d->d_FH || !lf) return ERROR_OBJECT_IN_USE;
	if (d->d_LevelWhole) return 0;
	/* from the head's end, a look early for the filters' warm-up (that
	   look's own value kept), up to the tail's warm-up look -- the tail
	   measured its looks from the one after */
	k0 = d->d_HeadEnd / lf;
	if (k0 == 0) k0 = 1;
	end = (d->d_TailFrom / lf + 1) * lf;
	if (end > d->d_Frames) end = d->d_Frames;
	f = (k0 - 1) * lf;
	if (end <= f) {
		d->d_LevelWhole = TRUE;		/* nothing in the middle left to measure */
		return 0;
	}
	if (!LookLevel_Init(&ll, d->d_Rate, d->d_Channels)) return 0;
	/* measured apart, taken only when whole: called off or failed, the
	   looks as they were (doc.h) */
	if (!(meas = (UBYTE *) AllocVec(d->d_Pic.dp_LookMax, MEMF_ANY | MEMF_CLEAR))) return ERROR_NO_FREE_STORE;
	ll.ll_Out = meas + (k0 - 1);
	ll.ll_OutMax = d->d_Pic.dp_LookMax - (k0 - 1);
	if (Seek(d->d_FH, (LONG) (d->d_Wav.nw_DataStart + f * align), OFFSET_BEGINNING) < 0) {
		err = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
		FreeVec(meas);
		return err;
	}
	while (f < end) {
		frames = BUF_BYTES / align < end - f ? BUF_BYTES / align : end - f;
		if ((n = Read(d->d_FH, d->d_Buf, (LONG) (frames * align))) <= 0) {
			err = n < 0 ? IoErr() : ERROR_SEEK_ERROR;
			break;
		}
		frames = (ULONG) n / align;
		LookLevel_Feed(&ll, (const UWORD *) d->d_Buf, frames);
		f += frames;
		pc = (UWORD) ((f - (k0 - 1) * lf) / ((end - (k0 - 1) * lf) / 100 + 1));
		if (pc > 100) pc = 100;
		if (pc != said && d->d_SaveProgress) {
			said = pc;
			if (!d->d_SaveProgress(pc)) {
				err = DOC_E_CANCELLED;
				break;
			}
		}
	}
	if (!err) {
		/* the warm-up look's own value kept (k0 - 1), the rest measured */
		for (k = k0; k < k0 - 1 + ll.ll_OutCount && k < d->d_Pic.dp_LookMax; k++) d->d_Looks[k] = meas[k];
		d->d_LevelWhole = TRUE;
	}
	FreeVec(meas);
	return err;
}

BOOL Doc_Tracks(struct Doc *d, struct TrimPlan *tp)
{
	struct TrimLooks tl;

	tp->tp_TrackCount = 0;
	if (!d->d_Done || !d->d_LevelWhole || !d->d_PlanLooks) return FALSE;
	looks_of(d, &tl, d->d_PlanLooks);
	return (BOOL) (TrimPlan_Make(&tl, tp) && tp->tp_TrackCount);
}

UWORD Doc_Percent(const struct Doc *d)
{
	ULONG done = d->d_Read, total = d->d_Frames;

	if (d->d_Done) return 100;
	/* a long file's tail is read twice: its first pass counted in */
	if (d->d_Phase != DOC_READ_ALL) {
		total += d->d_Frames - d->d_TailFrom;
		done += d->d_Phase == DOC_READ_HEAD ? 0 :
			d->d_Phase == DOC_READ_TAIL ? d->d_TailAt - d->d_TailFrom : d->d_Frames - d->d_TailFrom;
	}
	if (!total) return 100;
	/* done / (total / 100) without a product that wraps */
	return (UWORD) (total >= 100 ? (done / (total / 100) > 100 ? 100 : done / (total / 100)) : done * 100 / total);
}

/* a stretch of frames to read, and where to */
struct Span {
	ULONG sp_First;
	ULONG sp_Frames;
	WORD *sp_Buf;
};

/* sp's timeline frames into its buffer (the machine's order): how many */
static ULONG read_frames(struct Doc *d, struct Span *sp)
{
	struct TlRead rd;
	ULONG frames;

	Tl_ReadInit(&rd);
	rd.rd_FH = d->d_FH;
	rd.rd_Start = d->d_Wav.nw_DataStart;
	rd.rd_Align = (UWORD) d->d_Wav.nw_Align;
	rd.rd_Pad = 0;
	rd.rd_First = sp->sp_First;
	rd.rd_Frames = sp->sp_Frames;
	rd.rd_Buf = (UBYTE *) sp->sp_Buf;
	rd.rd_Levels = Tl_Levels(&d->d_Levels);
	frames = Tl_Read(&d->d_Tl, &rd);
	swap_words((UWORD *) sp->sp_Buf, frames * d->d_Channels);
	return frames;
}

/* the reading back where it was (a fetch moves the file) */
static void back_to_reading(struct Doc *d)
{
	if (!d->d_Done &&
	    Seek(d->d_FH, (LONG) (d->d_Wav.nw_DataStart + (d->d_Phase == DOC_READ_TAIL ? d->d_TailAt : d->d_Read) * d->d_Wav.nw_Align),
		 OFFSET_BEGINNING) < 0 && !d->d_ReadError)
		d->d_ReadError = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
}

const struct WaveSamples *Doc_Fetch(struct Doc *d, ULONG first, ULONG frames)
{
	struct WaveSamples *ws;
	struct Span sp;
	UWORD k = d->d_FetchNext;
	WORD *buf;
	ULONG got;

	if (!frames || first >= d->d_Tl.tl_Frames) return NULL;
	/* the buffer the gadget was not handed last; a bigger one made before
	   the old goes: no memory, both as they were */
	if (frames > d->d_SamplesMax[k]) {
		if (!(buf = (WORD *) AllocVec(frames * d->d_Wav.nw_Align, MEMF_ANY))) return NULL;
		if (d->d_Samples[k]) FreeVec(d->d_Samples[k]);
		d->d_Samples[k] = buf;
		d->d_SamplesMax[k] = frames;
	}
	sp.sp_First = first;
	sp.sp_Frames = frames;
	sp.sp_Buf = d->d_Samples[k];
	got = read_frames(d, &sp);
	back_to_reading(d);
	if (!got) return NULL;
	ws = &d->d_WS[k];
	ws->wsm_First = first;
	ws->wsm_Frames = got;
	ws->wsm_Channels = d->d_Channels;
	ws->wsm_Data = d->d_Samples[k];
	d->d_FetchNext = (UWORD) (k ^ 1);
	return ws;
}

ULONG Doc_ZeroCross(struct Doc *d, ULONG frame, ULONG within)
{
	struct WaveSamples s;
	struct WaveZero z;
	struct Span sp;

	if (within > ZERO_MAX / 2) within = ZERO_MAX / 2;
	if (!within) within = 1;
	sp.sp_First = frame > within ? frame - within : 0;
	sp.sp_Frames = 2 * within + 1;
	if (!(sp.sp_Buf = (WORD *) AllocVec(sp.sp_Frames * d->d_Wav.nw_Align, MEMF_ANY))) return frame;
	s.wsm_Frames = read_frames(d, &sp);
	back_to_reading(d);
	s.wsm_First = sp.sp_First;
	s.wsm_Channels = d->d_Channels;
	s.wsm_Data = sp.sp_Buf;
	z.wz_Want = frame;
	z.wz_Within = within;
	WavePlan_ZeroCrossing(&s, &z);
	FreeVec(sp.sp_Buf);
	return z.wz_Found;
}

/* name (DOC_PATHLEN, holding a path) with suffix after it: FALSE when
   it does not fit */
/* ---- the names of the files a save makes beside the file (beside.h) ---- */

static char last_temp[DOC_PATHLEN], last_backup[DOC_PATHLEN];

BOOL Doc_TempName(CONST_STRPTR path, UWORD k, STRPTR out)
{
	return Beside_TempName(path, k, out);
}

/* a temporary file's name beside path, none of that name there: FALSE
   (IoErr() set) when none is free */
static BOOL temp_name(CONST_STRPTR path, char *tmp)
{
	if (!Beside_Temp(path, (STRPTR) tmp)) return FALSE;
	Text_Copy((STRPTR) last_temp, (CONST_STRPTR) tmp, DOC_PATHLEN);
	return TRUE;
}

/* the backup's name beside path, cut to fit: FALSE when there is none */
static BOOL bak_name(CONST_STRPTR path, char *bak)
{
	if (!Beside_BackupName(path, (STRPTR) bak)) return FALSE;
	Text_Copy((STRPTR) last_backup, (CONST_STRPTR) bak, DOC_PATHLEN);
	return TRUE;
}

CONST_STRPTR Doc_LastTemp(void)
{
	return (CONST_STRPTR) last_temp;
}

CONST_STRPTR Doc_LastBackup(void)
{
	return (CONST_STRPTR) last_backup;
}

/* the original out of the new file's way: deleted, or kept as
   <name>.bak (Doc_SetBackup), an older backup replaced: FALSE with
   IoErr() */
static BOOL set_aside(CONST_STRPTR path)
{
	char bak[DOC_PATHLEN];

	if (!keep_backup) return (BOOL) (DeleteFile((STRPTR) path) != 0);
	if (!bak_name(path, bak)) {
		SetIoErr(ERROR_OBJECT_EXISTS);
		return FALSE;
	}
	if (!DeleteFile((STRPTR) bak) && IoErr() != ERROR_OBJECT_NOT_FOUND) return FALSE;
	return (BOOL) (Rename((STRPTR) path, (STRPTR) bak) != 0);
}

/* a save's work: into out, the kept frames in .. outf - 1 */
struct Save {
	BPTR sv_Out;
	ULONG sv_In, sv_OutFrame;
	ULONG sv_Offset, sv_Bytes;	/* a copy: from the file's offset, so many bytes */
	ULONG sv_First;			/* a stretch at its level: its first timeline frame */
	ULONG sv_Done, sv_Total;	/* bytes copied of all a save copies: the progress */
	UWORD sv_Said;			/* the per cent last told */
	BOOL sv_Feed;			/* the samples written made into the picture
					   (Doc_Save keeping the doc) */
};

/* the samples just written from d_Buf into the new file's picture and
   plain peaks, as a reading makes them of the samples it reads */
static void fed(struct Doc *d, const struct Save *sv, ULONG bytes)
{
	if (sv->sv_Feed) read_runs(d, (const UWORD *) d->d_Buf, bytes / d->d_Wav.nw_Align);
}

/* the save's progress told when its per cent moved (d_SaveProgress):
   FALSE when it is called off */
static BOOL saved_some(struct Doc *d, struct Save *sv)
{
	UWORD pc;

	if (!d->d_SaveProgress || !sv->sv_Total) return TRUE;
	/* done / (total / 100) without a product that wraps */
	pc = (UWORD) (sv->sv_Total >= 100 ? sv->sv_Done / (sv->sv_Total / 100) : sv->sv_Done * 100 / sv->sv_Total);
	if (pc > 100) pc = 100;
	if (pc == sv->sv_Said) return TRUE;
	sv->sv_Said = pc;
	return d->d_SaveProgress(pc);
}

/* sv_Bytes bytes of the file from sv_Offset into sv_Out, through d_Buf */
static LONG copy_out(struct Doc *d, struct Save *sv)
{
	ULONG left = sv->sv_Bytes, want;
	LONG n;

	if (Seek(d->d_FH, (LONG) sv->sv_Offset, OFFSET_BEGINNING) < 0) return IoErr();
	while (left) {
		want = left < BUF_BYTES ? left : BUF_BYTES;
		if ((n = Read(d->d_FH, d->d_Buf, (LONG) want)) != (LONG) want) return n < 0 ? IoErr() : ERROR_SEEK_ERROR;
		if (Write(sv->sv_Out, d->d_Buf, n) != n) return IoErr();
		fed(d, sv, want);
		left -= want;
		sv->sv_Done += want;
		if (!saved_some(d, sv)) return DOC_E_CANCELLED;
	}
	return 0;
}

/* sv_Bytes bytes of zeros into sv_Out (a silence), d_Buf cleared for it */
static LONG zeros_out(struct Doc *d, struct Save *sv)
{
	ULONG left = sv->sv_Bytes, want, i;

	for (i = 0; i < BUF_BYTES && i < left; i++) d->d_Buf[i] = 0;
	while (left) {
		want = left < BUF_BYTES ? left : BUF_BYTES;
		if (Write(sv->sv_Out, d->d_Buf, (LONG) want) != (LONG) want) return IoErr();
		fed(d, sv, want);
		left -= want;
		sv->sv_Done += want;
		if (!saved_some(d, sv)) return DOC_E_CANCELLED;
	}
	return 0;
}

/* ---- a part's peak (Doc_PartPeak) ---- */

/* the loudest |sample| of the bytes in d_Buf, peak the run's so far */
static ULONG buf_peak(const struct Doc *d, ULONG bytes, ULONG peak)
{
	const UBYTE *p = d->d_Buf;
	ULONG i, m;
	LONG s;

	for (i = 0; i + 1 < bytes; i += 2) {
		s = (LONG) (WORD) (UWORD) (p[i] | ((UWORD) p[i + 1] << 8));
		m = (ULONG) (s < 0 ? -s : s);
		if (m > peak) peak = m;
	}
	return peak;
}

/* the loudest of n file frames from f as they lie in the file */
static ULONG raw_peak(struct Doc *d, ULONG f, ULONG n)
{
	ULONG align = d->d_Wav.nw_Align, want, peak = 0;

	if (!n || Seek(d->d_FH, (LONG) (d->d_Wav.nw_DataStart + f * align), OFFSET_BEGINNING) < 0) return 0;
	while (n) {
		want = n < BUF_BYTES / align ? n : BUF_BYTES / align;
		if (Read(d->d_FH, d->d_Buf, (LONG) (want * align)) != (LONG) (want * align)) break;
		peak = buf_peak(d, want * align, peak);
		n -= want;
	}
	return peak;
}

/* the loudest of n timeline frames from t at their level (Tl_Read) */
static ULONG level_peak(struct Doc *d, ULONG t, ULONG n)
{
	struct TlRead rd;
	ULONG align = d->d_Wav.nw_Align, want, got, peak = 0;

	Tl_ReadInit(&rd);
	rd.rd_FH = d->d_FH;
	rd.rd_Start = d->d_Wav.nw_DataStart;
	rd.rd_Align = (UWORD) align;
	rd.rd_Buf = d->d_Buf;
	rd.rd_Levels = Tl_Levels(&d->d_Levels);
	while (n) {
		want = n < BUF_BYTES / align ? n : BUF_BYTES / align;
		rd.rd_First = t;
		rd.rd_Frames = want;
		got = Tl_Read(&d->d_Tl, &rd);
		peak = buf_peak(d, got * align, peak);
		if (got != want) break;
		t += want;
		n -= want;
	}
	return peak;
}

/* n file frames from f as recorded: the looks wholly inside from the
   peaks the reading kept, the looks the stretch begins and ends in read
   again (a look the reading has not reached yet read as well) */
static ULONG file_peak(struct Doc *d, ULONG f, ULONG n)
{
	ULONG lf = d->d_LL.ll_Frames, k0, k1, k, peak, m;

	/* the looks' peaks only once read through: while reading, a long
	   file's middle has none yet (and its tail's are counted apart) */
	if (!lf || !d->d_Done) return raw_peak(d, f, n);
	k0 = (f + lf - 1) / lf;			/* the first whole look */
	k1 = (f + n) / lf;			/* the end of the whole looks */
	if (k1 > d->d_Pic.dp_PeakCount) k1 = d->d_Pic.dp_PeakCount;
	if (k0 >= k1) return raw_peak(d, f, n);
	peak = raw_peak(d, f, k0 * lf - f);
	for (k = k0; k < k1; k++) if (d->d_Pic.dp_PeakWords[k] > peak) peak = d->d_Pic.dp_PeakWords[k];
	m = raw_peak(d, k1 * lf, f + n - k1 * lf);
	return m > peak ? m : peak;
}

ULONG Doc_PartPeak(struct Doc *d, ULONG first, ULONG end)
{
	const struct TlLevels *lv = Tl_Levels(&d->d_Levels);
	struct TlAt at;
	ULONG t = first, n, peak = 0, m;

	if (!d->d_FH || !d->d_Buf || !d->d_Pic.dp_PeakWords) return 0;
	if (end > d->d_Tl.tl_Frames) end = d->d_Tl.tl_Frames;
	while (t < end && Tl_At(&d->d_Tl, t, &at)) {
		n = at.ta_Run < end - t ? at.ta_Run : end - t;
		if (at.ta_From != TL_SILENCE) {
			m = Tl_Untouched(&d->d_Tl, lv, t, n) ? file_peak(d, at.ta_From, n) : level_peak(d, t, n);
			if (m > peak) peak = m;
		}
		t += n;
	}
	back_to_reading(d);		/* the file moved under a reading */
	return peak;
}

/* sv_Bytes bytes of the timeline from frame sv_First into sv_Out at
   their level (Tl_Read, concept 3.2), through d_Buf; the samples held at
   full scale counted into d_Clipped */
static LONG level_out(struct Doc *d, struct Save *sv)
{
	struct TlRead rd;
	ULONG align = d->d_Wav.nw_Align, left = sv->sv_Bytes / align, want;
	LONG err;

	Tl_ReadInit(&rd);
	rd.rd_FH = d->d_FH;
	rd.rd_Start = d->d_Wav.nw_DataStart;
	rd.rd_Align = (UWORD) align;
	rd.rd_Buf = d->d_Buf;
	rd.rd_Levels = Tl_Levels(&d->d_Levels);
	rd.rd_Clipped = &d->d_Clipped;
	rd.rd_First = sv->sv_First;
	while (left) {
		want = left < BUF_BYTES / align ? left : BUF_BYTES / align;
		rd.rd_Frames = want;
		if (Tl_Read(&d->d_Tl, &rd) != want) return (err = IoErr()) ? err : ERROR_SEEK_ERROR;
		if (Write(sv->sv_Out, d->d_Buf, (LONG) (want * align)) != (LONG) (want * align)) return IoErr();
		fed(d, sv, want * align);
		rd.rd_First += want;
		left -= want;
		sv->sv_Done += want * align;
		if (!saved_some(d, sv)) return DOC_E_CANCELLED;
	}
	return 0;
}

/* the timeline's frames sv_In .. sv_OutFrame - 1 into sv_Out: each
   piece's stretch of the file copied, a silence's zeros written, a
   stretch at a level of its own computed (the whole file as recorded:
   one copy, as it was) */
static LONG copy_timeline(struct Doc *d, struct Save *sv)
{
	struct TlAt at;
	ULONG t = sv->sv_In, n;
	LONG err;

	d->d_Clipped = 0;
	while (t < sv->sv_OutFrame && Tl_At(&d->d_Tl, t, &at)) {
		n = at.ta_Run < sv->sv_OutFrame - t ? at.ta_Run : sv->sv_OutFrame - t;
		sv->sv_Bytes = n * d->d_Wav.nw_Align;
		if (at.ta_From == TL_SILENCE) {
			err = zeros_out(d, sv);
		} else if (!Tl_Untouched(&d->d_Tl, Tl_Levels(&d->d_Levels), t, n)) {
			sv->sv_First = t;
			err = level_out(d, sv);
		} else {
			sv->sv_Offset = d->d_Wav.nw_DataStart + at.ta_From * d->d_Wav.nw_Align;
			err = copy_out(d, sv);
		}
		if (err) return err;
		t += n;
	}
	return 0;
}

/* the new file: the header with its sizes made true, the kept samples,
   a pad byte for an odd chunk, what followed the samples */
static LONG write_new(struct Doc *d, struct Save *sv)
{
	ULONG bytes = (sv->sv_OutFrame - sv->sv_In) * d->d_Wav.nw_Align;
	LONG err;
	UBYTE pad = 0;

	sv->sv_Done = 0;
	sv->sv_Total = bytes + d->d_Trailer;
	sv->sv_Said = 0xFFFF;
	NormWav_FixSizes(d->d_Head, &d->d_Wav, bytes);
	/* the RIFF size with what followed the samples */
	NormWav_PutLE32(d->d_Head + 4, NormWav_LE32(d->d_Head + 4) + d->d_Trailer);
	if (Write(sv->sv_Out, d->d_Head, (LONG) d->d_Wav.nw_DataStart) != (LONG) d->d_Wav.nw_DataStart) return IoErr();
	err = copy_timeline(d, sv);
	if (!err && (bytes & 1) && Write(sv->sv_Out, &pad, 1) != 1) err = IoErr();
	if (!err && d->d_Trailer) {
		sv->sv_Offset = d->d_Wav.nw_DataStart + d->d_Wav.nw_DataBytes + (d->d_Wav.nw_DataBytes & 1);
		sv->sv_Bytes = d->d_Trailer;
		sv->sv_Feed = FALSE;		/* what follows the samples is none */
		err = copy_out(d, sv);
	}
	return err;
}

/* ---- a save that keeps the doc (Doc_Save) ---- */

/* The doc's picture while a save builds the new file's in its place --
   put back whole when the save fails. struct DocPicture holds all of it:
   a field added there is set aside and put back with the rest. */
struct Aside {
	struct DocPicture as_Pic;
};

/* the save failed: the new picture given back, d's own in its place */
static void picture_back(struct Doc *d, struct Aside *as)
{
	picture_free(&d->d_Pic);
	d->d_Pic = as->as_Pic;
}

/* d's picture into as, a new one for frames frames in its place,
   counted from nothing: FALSE (d as it was) without the memory */
static BOOL picture_aside(struct Doc *d, struct Aside *as, ULONG frames)
{
	as->as_Pic = d->d_Pic;
	if (picture_new(d, frames) != DOC_OK) {
		d->d_Pic = as->as_Pic;
		return FALSE;
	}
	return TRUE;
}

/* the memory of the picture set aside given back (once: it is cleared) */
static void aside_free(struct Aside *as)
{
	picture_free(&as->as_Pic);
}

/* the new file's crackle-proof looks from the old ones along the
   timeline (in its first kept frame): each look the old look its first
   frame came from, a silence nothing, at the level at its middle -- as
   mapcut.c cuts the .prl; NULL without the memory */
static UBYTE *looks_along(const struct Doc *d, ULONG in)
{
	ULONG lf = d->d_LL.ll_Frames, k, src, oldmax = d->d_LL.ll_OutMax;
	struct TlLook tk;
	UBYTE *looks;

	if (!(looks = (UBYTE *) AllocVec(d->d_Pic.dp_LookMax, MEMF_ANY | MEMF_CLEAR))) return NULL;
	tk.tk_Levels = Tl_Levels(&d->d_Levels);
	for (k = 0; k < d->d_Pic.dp_PeakCount && k < d->d_Pic.dp_LookMax; k++) {
		looks[k] = ENV_LOOK_NONE;
		tk.tk_First = in + k * lf;
		tk.tk_Mid = tk.tk_First + lf / 2;
		if (!Tl_Look(&d->d_Tl, &tk) || tk.tk_From == TL_SILENCE) continue;
		src = tk.tk_From / lf;
		if (src < oldmax) looks[k] = Env_LookDb(d->d_Looks[src], tk.tk_Level);
	}
	return looks;
}

/* the doc made the file just saved under its name, its picture and plain
   peaks built while it was written (as's the old ones, given back here):
   the header read again, the timeline the whole file, the looks carried
   over, the suggestion made -- FALSE when the file cannot be opened again
   or the looks have no memory (the doc then to be closed) */
static BOOL become_saved(struct Doc *d, struct Aside *as, ULONG in)
{
	ULONG frames = d->d_Pic.dp_Peaks.wp_Frames, count;
	UBYTE *looks;
	LONG n;

	aside_free(as);
	last_peak(d);
	count = d->d_Pic.dp_PeakCount;
	if (!(looks = looks_along(d, in))) return FALSE;
	FreeVec(d->d_Looks);
	d->d_Looks = looks;
	d->d_LL.ll_Out = looks;
	d->d_LL.ll_OutMax = d->d_Pic.dp_LookMax;
	d->d_LL.ll_OutCount = count;
	if (!(d->d_FH = Open((STRPTR) d->d_Path, MODE_OLDFILE))) return FALSE;
	if ((n = Read(d->d_FH, d->d_Head, (LONG) d->d_HeadMax)) < 12 || NormWav_Parse(d->d_Head, (ULONG) n, &d->d_Wav) != NORMWAV_OK ||
	    d->d_Wav.nw_DataBytes / d->d_Wav.nw_Align != frames)
		return FALSE;
	if (Seek(d->d_FH, (LONG) d->d_Wav.nw_DataStart, OFFSET_BEGINNING) < 0) return FALSE;
	d->d_Frames = frames;
	Tl_Init(&d->d_Tl, frames);
	clear_bytes((UBYTE *) &d->d_Levels, sizeof(d->d_Levels));
	Doc_Changed(d);
	d->d_Read = frames;
	d->d_Phase = DOC_READ_ALL;
	d->d_Pic.dp_NoPicture = FALSE;
	/* a long file's level not measured through its middle: Find tracks
	   measures the saved one whole (Doc_MeasureLevel) */
	if (!d->d_LevelWhole) {
		d->d_HeadEnd = 0;
		d->d_TailFrom = frames;
	}
	plan_now(d, count);
	plan_fits(d);
	d->d_Done = TRUE;
	return TRUE;
}

LONG Doc_Save(struct Doc *d, ULONG in, ULONG out)
{
	char tmp[DOC_PATHLEN], comment[80];
	struct FileInfoBlock *fib;
	struct DateStamp date;
	struct Aside as;
	struct Save sv;
	BPTR lock;
	LONG err, prot = 0;
	UWORD i;
	BOOL keep;

	if (!d->d_Done && !d->d_PlanReady) return ERROR_OBJECT_IN_USE;
	if (out > d->d_Tl.tl_Frames) out = d->d_Tl.tl_Frames;
	if (in >= out) return DOC_E_EMPTY;
	if (!temp_name((CONST_STRPTR) d->d_Path, tmp)) return IoErr();
	comment[0] = '\0';
	date.ds_Days = date.ds_Minute = date.ds_Tick = 0;
	/* what the file is besides its bytes: kept */
	if ((fib = AllocDosObject(DOS_FIB, NULL))) {
		if ((lock = Lock((STRPTR) d->d_Path, SHARED_LOCK))) {
			if (Examine(lock, fib)) {
				prot = fib->fib_Protection;
				date = fib->fib_Date;
				for (i = 0; fib->fib_Comment[i] && i < sizeof(comment) - 1; i++) comment[i] = fib->fib_Comment[i];
				comment[i] = '\0';
			}
			UnLock(lock);
		}
		FreeDosObject(DOS_FIB, fib);
	}
	if (!(sv.sv_Out = Open((STRPTR) tmp, MODE_NEWFILE))) return IoErr();
	sv.sv_In = in;
	sv.sv_OutFrame = out;
	/* the new file's picture made as it is written: then nothing is read
	   again after the save (no memory for it: it is, as it always was) */
	sv.sv_Feed = keep = picture_aside(d, &as, out - in);
	err = write_new(d, &sv);
	if (!Close(sv.sv_Out) && !err) err = IoErr();
	if (err) {
		if (keep) picture_back(d, &as);
		DeleteFile((STRPTR) tmp);
		back_to_reading(d);
		return err;
	}
	/* the original's handle let go (an open file cannot be deleted); the
	   picture stays: the file is unchanged until it is gone */
	Close(d->d_FH);
	d->d_FH = 0;
	if (!set_aside((CONST_STRPTR) d->d_Path)) {
		err = IoErr();
		if (keep) picture_back(d, &as);
		DeleteFile((STRPTR) tmp);
		if ((d->d_FH = Open((STRPTR) d->d_Path, MODE_OLDFILE))) back_to_reading(d);
		return err;
	}
	/* the new one takes the name; when it cannot, it stays under its own */
	if (!Rename((STRPTR) tmp, (STRPTR) d->d_Path)) {
		if (keep) aside_free(&as);
		Doc_Close(d);
		return DOC_E_KEPT;
	}
	SetProtection((STRPTR) d->d_Path, prot);
	if (comment[0]) SetComment((STRPTR) d->d_Path, (STRPTR) comment);
	if (date.ds_Days || date.ds_Minute || date.ds_Tick) SetFileDate((STRPTR) d->d_Path, &date);
	if (keep && become_saved(d, &as, in)) return DOC_KEPT;
	if (keep) aside_free(&as);
	Doc_Close(d);
	return 0;
}

LONG Doc_SaveAs(struct Doc *d, const struct DocSave *ds)
{
	char tmp[DOC_PATHLEN], old[DOC_PATHLEN];
	struct Save sv;
	ULONG out = ds->ds_Out > d->d_Tl.tl_Frames ? d->d_Tl.tl_Frames : ds->ds_Out;
	BOOL aside = FALSE;
	BPTR lock;
	LONG err;

	if (!d->d_Done && !d->d_PlanReady) return ERROR_OBJECT_IN_USE;
	if (ds->ds_In >= out) return DOC_E_EMPTY;
	if (!temp_name(ds->ds_Path, tmp)) return IoErr();
	if (!(sv.sv_Out = Open((STRPTR) tmp, MODE_NEWFILE))) return IoErr();
	sv.sv_In = ds->ds_In;
	sv.sv_OutFrame = out;
	sv.sv_Feed = FALSE;
	err = write_new(d, &sv);
	if (!Close(sv.sv_Out) && !err) err = IoErr();
	/* a file there moved out of the way under a temporary name first,
	   gone only once the new one has its name -- else back where it was */
	if (!err && (lock = Lock((STRPTR) ds->ds_Path, SHARED_LOCK))) {
		UnLock(lock);
		if (!temp_name(ds->ds_Path, old) || !Rename((STRPTR) ds->ds_Path, (STRPTR) old)) err = IoErr();
		else aside = TRUE;
	}
	if (!err && !Rename((STRPTR) tmp, (STRPTR) ds->ds_Path)) {
		err = IoErr();
		if (aside) Rename((STRPTR) old, (STRPTR) ds->ds_Path);
	} else if (!err && aside) {
		DeleteFile((STRPTR) old);
	}
	if (err) DeleteFile((STRPTR) tmp);
	back_to_reading(d);
	return err;
}

void Doc_Close(struct Doc *d)
{
	UWORD l;

	if (d->d_FH) Close(d->d_FH);
	if (d->d_Head) FreeVec(d->d_Head);
	if (d->d_Buf) FreeVec(d->d_Buf);
	if (d->d_Looks) FreeVec(d->d_Looks);
	picture_free(&d->d_Pic);
	for (l = 0; l < DOC_FETCHES; l++) {
		if (d->d_Samples[l]) FreeVec(d->d_Samples[l]);
	}
	clear_bytes((UBYTE *) d, sizeof(*d));
}
