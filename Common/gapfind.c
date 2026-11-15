/*
 * gapfind.c -- see gapfind.h
 */
#include "gapfind.h"
#include "levelmap.h"

#define LEARN_GUARD_MS	2000UL		/* left out before the first music: a fade-in */
#define LEARN_SHORT_MS	500UL		/* ... or this, in a short lead-in */
#define ONSET_DB	2		/* a fade's start: this much above the noise */
#define LEARN_MIN_MS	1000UL		/* a lead-in shorter than this teaches nothing */
#define LEARN_ABOVE	6		/* the threshold this many dB above the noise */
#define LEARN_LOUDEST	10UL		/* percent of the lead-in left out: clicks */
#define LEARN_MAX_DB	80		/* quieter than this the map cannot tell */
#define HIST_NONE	91		/* the histogram's slot for LEVELMAP_NONE (and below 90 dB) */
#define BURST_MS	500UL		/* a side's ends: a loud burst this short ... */
#define BURST_QUIET_MS	1000UL		/* ... with no music this long after it (at the
					   start; before it, at the end) is a thump */

void GapFind_Defaults(struct GapScan *gs, ULONG look_ms)
{
	gs->gs_LookMs = look_ms ? look_ms : 40UL;
	gs->gs_Threshold = 48;
	gs->gs_KeepEnds = FALSE;
	gs->gs_MinGapMs = 1500UL;
	gs->gs_MinTrackMs = 30000UL;
	gs->gs_LeadMs = 500UL;
	gs->gs_TailMs = 2000UL;
}

/* ms in whole looks, rounded */
static ULONG looks_of(const struct GapScan *gs, ULONG ms)
{
	return (ms + gs->gs_LookMs / 2UL) / gs->gs_LookMs;
}

/* look i louder than the threshold (dB down): not silence */
static BOOL loud(const struct GapScan *gs, ULONG i, UWORD threshold)
{
	UBYTE b = gs->gs_Looks[i];

	return (BOOL) (b != LEVELMAP_NONE && b < threshold);
}

/* the loud looks from i on, at most one more than a click */
static ULONG run_from(const struct GapScan *gs, ULONG i, UWORD threshold)
{
	ULONG n = 0;

	while (i + n < gs->gs_Count && n <= GAPFIND_CLICK_LOOKS && loud(gs, i + n, threshold)) n++;
	return n;
}

/* the loud looks up to i (i included), at most one more than a click */
static ULONG run_to(const struct GapScan *gs, ULONG i, UWORD threshold)
{
	ULONG n = 0;

	while (n <= i && n <= GAPFIND_CLICK_LOOKS && loud(gs, i - n, threshold)) n++;
	return n;
}

/* the first look of music from i on (a loud run longer than a click), or
 * gs_Count */
static ULONG music_from(const struct GapScan *gs, ULONG i, UWORD threshold)
{
	while (i < gs->gs_Count && run_from(gs, i, threshold) <= GAPFIND_CLICK_LOOKS) i++;
	return i;
}

/* the loud looks from i on, at most max (gs_Threshold) */
static ULONG run_up(const struct GapScan *gs, ULONG i, ULONG max)
{
	ULONG n = 0;

	while (i + n < gs->gs_Count && n < max && loud(gs, i + n, gs->gs_Threshold)) n++;
	return n;
}

/* the loud looks up to i (i included), at most max (gs_Threshold) */
static ULONG run_down(const struct GapScan *gs, ULONG i, ULONG max)
{
	ULONG n = 0;

	while (n <= i && n < max && loud(gs, i - n, gs->gs_Threshold)) n++;
	return n;
}

/* A side's first music: a loud run longer than a click -- but a burst no
 * longer than BURST_MS with no such run in the BURST_QUIET_MS after it is
 * the needle's thump, not music (the user's rule, 2026-10-26: a lossy
 * coder smears a WAV's 3-look thump into 4, one more than a click, and a
 * real drop rumbles longer). A burst with nothing loud after it at all
 * stays music. Clicks in that second do not count. */
static ULONG side_start(const struct GapScan *gs)
{
	ULONG i = 0, r, next, burst = looks_of(gs, BURST_MS), quiet = looks_of(gs, BURST_QUIET_MS);

	for (;;) {
		i = music_from(gs, i, gs->gs_Threshold);
		if (i >= gs->gs_Count) return i;
		if ((r = run_up(gs, i, burst + 1)) > burst) return i;
		next = music_from(gs, i + r, gs->gs_Threshold);
		if (next >= gs->gs_Count || next < i + r + quiet) return i;
		i += r;				/* a thump: on after it */
	}
}

/* The look after a side's last music, from the end back: the same, a
 * burst with no music in the BURST_QUIET_MS before it the needle lifted */
static ULONG side_end(const struct GapScan *gs, ULONG first)
{
	ULONG last = gs->gs_Count, r, p, burst = looks_of(gs, BURST_MS), quiet = looks_of(gs, BURST_QUIET_MS);

	for (;;) {
		while (last > first && run_to(gs, last - 1, gs->gs_Threshold) <= GAPFIND_CLICK_LOOKS) last--;
		if (last <= first) return last;
		if ((r = run_down(gs, last - 1, burst + 1)) > burst) return last;
		/* the music before the burst: back from its first look */
		for (p = last - r; p > first && run_to(gs, p - 1, gs->gs_Threshold) <= GAPFIND_CLICK_LOOKS; p--) ;
		if (p <= first || p + quiet > last - r) return last;
		last -= r;			/* the needle lifted: back before it */
	}
}

/* the end of a silence that starts at i: the first look of music after
 * it (clicks inside it skipped), or end */
static ULONG silence_end(const struct GapScan *gs, ULONG i, ULONG end)
{
	ULONG r;

	while (i < end) {
		if (!loud(gs, i, gs->gs_Threshold)) {
			i++;
			continue;
		}
		r = run_from(gs, i, gs->gs_Threshold);
		if (r > GAPFIND_CLICK_LOOKS) break;	/* music */
		i += r;					/* a click */
	}
	return i;
}

/* a histogram's slot for look byte b */
#define SLOT(b)		((b) > HIST_NONE ? HIST_NONE : (b))

/* the level of total looks counted in hist, their loudest tenth left out
 * (clicks): from the loud end, the first byte past that tenth --
 * HIST_NONE when they are nothing at all */
static UWORD loud_end(const ULONG *hist, ULONG total)
{
	ULONG need = total * LEARN_LOUDEST / 100UL, sum = 0;
	UWORD b;

	for (b = 0; b < HIST_NONE; b++) {
		sum += hist[b];
		if (sum > need) break;
	}
	return b;
}

/* a noise level b as a threshold 6 dB above it: 0 when it is none to
 * learn from -- digital silence, or too loud to be a groove's noise */
static UWORD above_noise(UWORD b)
{
	if (b >= HIST_NONE || b > LEARN_MAX_DB) return 0;
	if (b < GAPFIND_MUSIC_DB + 2 * LEARN_ABOVE) return 0;
	return (UWORD) (b - LEARN_ABOVE);
}

UWORD GapFind_Learn(const struct GapScan *gs)
{
	ULONG hist[HIST_NONE + 1], m, guard, lead, i;
	UWORD b;

	/* the lead-in up to the music for sure, the fade-in before it left
	 * out (a quiet start under the threshold would pass for noise) */
	m = music_from(gs, 0, (UWORD) (GAPFIND_MUSIC_DB + 1));
	if (m >= gs->gs_Count) return 0;
	guard = looks_of(gs, LEARN_GUARD_MS);
	if (m < guard + looks_of(gs, LEARN_MIN_MS)) guard = looks_of(gs, LEARN_SHORT_MS);
	if (m <= guard) return 0;
	lead = m - guard;
	if (lead < looks_of(gs, LEARN_MIN_MS)) return 0;
	for (b = 0; b <= HIST_NONE; b++) hist[b] = 0;
	for (i = 0; i < lead; i++) hist[SLOT(gs->gs_Looks[i])]++;
	return above_noise(loud_end(hist, lead));
}

UWORD GapFind_Quiet(const struct GapScan *gs)
{
	ULONG hist[HIST_NONE + 1], w, i;
	UWORD b, level, quiet = 0;

	w = looks_of(gs, gs->gs_MinGapMs);
	if (w == 0 || gs->gs_Count < w) return 0;
	for (b = 0; b <= HIST_NONE; b++) hist[b] = 0;
	for (i = 0; i < w; i++) hist[SLOT(gs->gs_Looks[i])]++;
	/* a window of a gap's length slid over the side: its level each
	 * step, the quietest kept */
	for (i = w;; i++) {
		level = loud_end(hist, w);
		if (level > quiet && level < HIST_NONE) quiet = level;
		if (i >= gs->gs_Count) break;
		hist[SLOT(gs->gs_Looks[i])]++;
		hist[SLOT(gs->gs_Looks[i - w])]--;
	}
	return above_noise(quiet);
}

/* the noise's level the threshold stands for (6 dB below it), less
 * ONSET_DB: a look louder than that is music fading in or out */
static UWORD onset_level(const struct GapScan *gs)
{
	UWORD noise = (UWORD) (gs->gs_Threshold + LEARN_ABOVE);

	return (UWORD) (noise > 90 ? 90 - ONSET_DB : noise - ONSET_DB);
}

/* music detected from g on: where it really starts, the looks before g
 * still louder than the noise walked back over, but not past lo */
static ULONG onset(const struct GapScan *gs, ULONG g, ULONG lo)
{
	UWORD level = onset_level(gs);

	while (g > lo && loud(gs, g - 1, level)) g--;
	return g;
}

/* music ending before e: where it really ends, the fade-out after it
 * walked over */
static ULONG offset(const struct GapScan *gs, ULONG e)
{
	UWORD level = onset_level(gs);

	while (e < gs->gs_Count && loud(gs, e, level)) e++;
	return e;
}

/* a track [start, end) into gr: FALSE when it is full */
static BOOL add_track(struct GapResult *gr, ULONG start, ULONG end)
{
	if (gr->gr_Tracks >= GAPFIND_MAXTRACKS) return FALSE;
	gr->gr_Track[gr->gr_Tracks].gt_Start = start;
	gr->gr_Track[gr->gr_Tracks].gt_End = end;
	gr->gr_Tracks++;
	return TRUE;
}

UWORD GapFind_Tracks(const struct GapScan *gs, struct GapResult *gr)
{
	ULONG first, last, i, g, lead, min_gap, min_track, start, split, middle, end;

	gr->gr_Tracks = 0;
	gr->gr_MusicStart = gr->gr_MusicEnd = 0;
	first = side_start(gs);
	if (first >= gs->gs_Count) return 0;
	/* the last music: from the end back to a loud run longer than a click
	   (a needle lifted no music) */
	last = side_end(gs, first);
	gr->gr_MusicStart = first;
	gr->gr_MusicEnd = last;

	lead = looks_of(gs, gs->gs_LeadMs);
	min_gap = looks_of(gs, gs->gs_MinGapMs);
	min_track = looks_of(gs, gs->gs_MinTrackMs);
	g = onset(gs, first, 0);
	start = gs->gs_KeepEnds || g < lead ? 0 : g - lead;
	end = gs->gs_KeepEnds ? gs->gs_Count : offset(gs, last) + looks_of(gs, gs->gs_TailMs);
	if (end > gs->gs_Count) end = gs->gs_Count;

	/* the gaps between first and last: each long enough a split, where
	 * the track before it is long enough (else the music runs on) */
	i = first;
	while (i < last) {
		if (loud(gs, i, gs->gs_Threshold)) {
			i++;
			continue;
		}
		g = silence_end(gs, i, last);
		if (g < last && g - i >= min_gap) {
			/* half a second before the music -- before its fade-in,
			 * which the threshold missed -- but in the gap's second half */
			middle = i + (g - i) / 2UL;
			split = onset(gs, g, middle);
			split = split >= lead ? split - lead : 0;
			if (split < middle) split = middle;
			if (split - start >= min_track && gr->gr_Tracks < GAPFIND_MAXTRACKS - 1) {
				add_track(gr, start, split);
				start = split;
			}
		}
		i = g;
	}
	add_track(gr, start, end);
	/* a last track too short joins the one before */
	if (gr->gr_Tracks >= 2 && end - start < min_track) {
		gr->gr_Tracks--;
		gr->gr_Track[gr->gr_Tracks - 1].gt_End = end;
	}
	return gr->gr_Tracks;
}
