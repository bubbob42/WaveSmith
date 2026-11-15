/*
 * wavepeaks.c -- the sound's picture in RAM (wavepeaks.h)
 */

#include "wavepeaks.h"

static UWORD shift_of(UWORD level)
{
	return (UWORD) (WAVEPEAKS_SHIFT0 + level * WAVEPEAKS_SHIFTUP);
}

BYTE WavePeaks_Top(WORD v)
{
	/* (v + 32768) / 256 - 128, in unsigned arithmetic: floor for both
	   signs, where >> of a negative value is the compiler's choice */
	return (BYTE) ((LONG) (((ULONG) ((LONG) v + 32768L)) >> 8) - 128L);
}

/* each level's open entry: as low and as high as can be */
static void open_entry(struct WavePeaks *wp, UWORD level)
{
	UWORD c;

	for (c = 0; c < WAVEPEAKS_CHANNELS; c++) {
		wp->wp_Min[level][c] = 127;
		wp->wp_Max[level][c] = -128;
	}
}

BOOL WavePeaks_Init(struct WavePeaks *wp, ULONG frames, UWORD channels)
{
	UWORD l;
	UWORD sh;

	if (channels < 1 || channels > WAVEPEAKS_CHANNELS) return FALSE;
	wp->wp_Frames = frames;
	wp->wp_Done = wp->wp_Fed = 0;
	wp->wp_Channels = channels;
	wp->wp_Pad = 0;
	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		sh = shift_of(l);
		/* rounded up without frames + (1 << sh) - 1, which would wrap
		   near 4 G frames */
		wp->wp_Entries[l] = (frames >> sh) + ((frames & ((1UL << sh) - 1)) ? 1 : 0);
		wp->wp_Level[l] = NULL;
		open_entry(wp, l);
	}
	return TRUE;
}

ULONG WavePeaks_Bytes(const struct WavePeaks *wp, UWORD level)
{
	if (level >= WAVEPEAKS_LEVELS) return 0;
	/* entries <= 4 M (4 G frames / 1024), x 4: fits */
	return wp->wp_Entries[level] * wp->wp_Channels * 2;
}

/* the open entry of level into its slot, as far as it has come */
static void store_entry(struct WavePeaks *wp, UWORD level, ULONG entry)
{
	BYTE *p;
	UWORD c;

	if (!wp->wp_Level[level] || entry >= wp->wp_Entries[level]) return;
	p = wp->wp_Level[level] + entry * wp->wp_Channels * 2;
	for (c = 0; c < wp->wp_Channels; c++) {
		p[c * 2] = wp->wp_Min[level][c];
		p[c * 2 + 1] = wp->wp_Max[level][c];
	}
}

/* frame f (the one just taken in) closed an entry of level 0 and maybe of
   the levels above: each into its slot, merged into the next, opened anew */
static void close_entries(struct WavePeaks *wp, ULONG f)
{
	UWORD l, c;
	UWORD sh;

	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		sh = shift_of(l);
		if (((f + 1) & ((1UL << sh) - 1)) != 0) break;
		store_entry(wp, l, f >> sh);
		if (l + 1 < WAVEPEAKS_LEVELS) {
			for (c = 0; c < wp->wp_Channels; c++) {
				if (wp->wp_Min[l][c] < wp->wp_Min[l + 1][c]) wp->wp_Min[l + 1][c] = wp->wp_Min[l][c];
				if (wp->wp_Max[l][c] > wp->wp_Max[l + 1][c]) wp->wp_Max[l + 1][c] = wp->wp_Max[l][c];
			}
		}
		open_entry(wp, l);
	}
}

/* level's open entry into its slot: its own minimum and maximum and those
   of the entries still open below it, not merged into it yet */
static void store_open(struct WavePeaks *wp, UWORD level)
{
	BYTE *p;
	BYTE mn, mx;
	UWORD c, k;

	if (!wp->wp_Level[level] || wp->wp_Fed == 0) return;
	p = wp->wp_Level[level] + ((wp->wp_Fed - 1) >> shift_of(level)) * wp->wp_Channels * 2;
	for (c = 0; c < wp->wp_Channels; c++) {
		mn = wp->wp_Min[level][c];
		mx = wp->wp_Max[level][c];
		for (k = 0; k < level; k++) {
			if (wp->wp_Min[k][c] < mn) mn = wp->wp_Min[k][c];
			if (wp->wp_Max[k][c] > mx) mx = wp->wp_Max[k][c];
		}
		p[c * 2] = mn;
		p[c * 2 + 1] = mx;
	}
}

/* the slots' stores kept before wp_Done's (a reader on another task):
   GCC may move stores past one another, SAS/C does not across a call */
#ifdef __GNUC__
#define WP_BARRIER()	__asm__ __volatile__("" ::: "memory")
#else
#define WP_BARRIER()
#endif

/* WavePeaks_Top() inline, for the feed's loop */
#define TOP(v)		((BYTE) ((LONG) (((ULONG) ((LONG) (v) + 32768L)) >> 8) - 128L))
#define ENTRY0		(1UL << WAVEPEAKS_SHIFT0)

void WavePeaks_Feed(struct WavePeaks *wp, const WORD *frames, ULONG count)
{
	ULONG run;
	BYTE b, mn0, mx0, mn1, mx1;

	if (count > wp->wp_Frames - wp->wp_Fed) count = wp->wp_Frames - wp->wp_Fed;
	while (count) {
		/* up to the end of level 0's entry, minima and maxima only: no
		   product, no call a frame (a 68000 pays dearly for both) */
		run = ENTRY0 - (wp->wp_Fed & (ENTRY0 - 1));
		if (run > count) run = count;
		count -= run;
		wp->wp_Fed += run;
		mn0 = wp->wp_Min[0][0];
		mx0 = wp->wp_Max[0][0];
		if (wp->wp_Channels == 2) {
			mn1 = wp->wp_Min[0][1];
			mx1 = wp->wp_Max[0][1];
			while (run--) {
				b = TOP(frames[0]);
				if (b < mn0) mn0 = b;
				if (b > mx0) mx0 = b;
				b = TOP(frames[1]);
				if (b < mn1) mn1 = b;
				if (b > mx1) mx1 = b;
				frames += 2;
			}
			wp->wp_Min[0][1] = mn1;
			wp->wp_Max[0][1] = mx1;
		} else {
			while (run--) {
				b = TOP(*frames);
				if (b < mn0) mn0 = b;
				if (b > mx0) mx0 = b;
				frames++;
			}
		}
		wp->wp_Min[0][0] = mn0;
		wp->wp_Max[0][0] = mx0;
		/* the entry's last frame taken: it and those above it closed */
		if (!(wp->wp_Fed & (ENTRY0 - 1))) close_entries(wp, wp->wp_Fed - 1);
	}
	WavePeaks_Flush(wp);
}

ULONG WavePeaks_RunLeft(const struct WavePeaks *wp)
{
	ULONG run;

	if (wp->wp_Fed >= wp->wp_Frames) return 0;
	run = ENTRY0 - (wp->wp_Fed & (ENTRY0 - 1));
	return run < wp->wp_Frames - wp->wp_Fed ? run : wp->wp_Frames - wp->wp_Fed;
}

void WavePeaks_FeedRun(struct WavePeaks *wp, ULONG count, const BYTE *mnmx)
{
	UWORD c;

	if (!count || count > WavePeaks_RunLeft(wp)) return;
	for (c = 0; c < wp->wp_Channels; c++) {
		if (mnmx[c * 2] < wp->wp_Min[0][c]) wp->wp_Min[0][c] = mnmx[c * 2];
		if (mnmx[c * 2 + 1] > wp->wp_Max[0][c]) wp->wp_Max[0][c] = mnmx[c * 2 + 1];
	}
	wp->wp_Fed += count;
	if (!(wp->wp_Fed & (ENTRY0 - 1))) close_entries(wp, wp->wp_Fed - 1);
}

/* each entry still open, as far as it has come, so that a query sees
   what was read (one closed on this frame is in its slot already) */
void WavePeaks_Flush(struct WavePeaks *wp)
{
	UWORD l;

	for (l = 0; l < WAVEPEAKS_LEVELS; l++) {
		if (wp->wp_Fed & ((1UL << shift_of(l)) - 1)) store_open(wp, l);
	}
	/* the slots written first, then what a query may read of them */
	WP_BARRIER();
	wp->wp_Done = wp->wp_Fed;
}

/* the coarsest level whose entries are no longer than step; -1: none */
static LONG level_for(ULONG step)
{
	LONG l;

	for (l = WAVEPEAKS_LEVELS - 1; l >= 0; l--) {
		if (step >= (1UL << shift_of((UWORD) l))) return l;
	}
	return -1;
}

ULONG WavePeaks_Query(const struct WavePeaks *wp, struct WavePeakMsg *q)
{
	ULONG i, first, last, e, e1, step = q->wpm_Step;
	LONG level;
	UWORD sh, c, ch = wp->wp_Channels, rsh = (UWORD) (ch == 2 ? 2 : 1), rowb = (UWORD) (ch * 2);
	BYTE *out = q->wpm_Out, mn, mx;
	const BYTE *p;

	if (step == 0 || (level = level_for(step)) < 0) return 0;
	if (!wp->wp_Level[level]) return 0;
	sh = shift_of((UWORD) level);
	first = q->wpm_First;
	for (i = 0; i < q->wpm_Count; i++) {
		/* the column's first frame a step on from the last, below 4 G: no
		   division, no product a column (the class asks every column of
		   two gadgets at every per cent of a read) */
		if (i) {
			if (step > 0xFFFFFFFFUL - first) break;
			first += step;
		}
		if (first >= wp->wp_Frames) {
			for (c = 0; c < rowb; c++) *out++ = 0;
			continue;
		}
		if (first >= wp->wp_Done) break;
		last = step - 1 > 0xFFFFFFFFUL - first ? 0xFFFFFFFFUL : first + step - 1;
		if (last >= wp->wp_Done) last = wp->wp_Done - 1;
		e1 = last >> sh;
		for (c = 0; c < ch; c++) {
			mn = 127;
			mx = -128;
			/* an entry's row: channels x (min, max), 2 or 4 bytes */
			for (e = first >> sh, p = wp->wp_Level[level] + (e << rsh) + c * 2; e <= e1; e++, p += rowb) {
				if (p[0] < mn) mn = p[0];
				if (p[1] > mx) mx = p[1];
			}
			*out++ = mn;
			*out++ = mx;
		}
	}
	return i;
}
