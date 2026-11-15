/*
 * looklevel.c -- see looklevel.h
 */
#include "looklevel.h"
#include "levelmap.h"

#define MAX_RATE	96000UL		/* a look's frames x 10^6 stays in 32 bits */
#define MIN_RATE	1000UL
#define TWO_PI_FC_Q15	20588852UL	/* 2 pi x LOOKLEVEL_CUTOFF_HZ x 32768 */

/* the sample a word as loaded stands for: its bytes the other way round */
#define SAMPLE(w)	((WORD) (((w) << 8) | ((w) >> 8)))

BOOL LookLevel_Init(struct LookLevel *ll, ULONG rate, UWORD channels)
{
	UWORD c;

	if (rate < MIN_RATE || rate > MAX_RATE || channels == 0 || channels > LOOKLEVEL_CHANNELS) return FALSE;
	ll->ll_Channels = channels;
	/* the pole 1 - 2 pi fc / fs: 32302 at 44.1 kHz */
	ll->ll_Pole = (WORD) (32768L - (LONG) (TWO_PI_FC_Q15 / rate));
	ll->ll_Frames = rate * LOOKLEVEL_MS / 1000UL;
	ll->ll_PieceFrames = ll->ll_Frames / LOOKLEVEL_PIECES;
	if (ll->ll_PieceFrames == 0) return FALSE;
	ll->ll_Done = 0;
	ll->ll_Piece = 0;
	ll->ll_PiecePeak = 0;
	ll->ll_OutCount = 0;
	for (c = 0; c < LOOKLEVEL_CHANNELS; c++) ll->ll_Prev[c] = ll->ll_Last[c] = 0;
	return TRUE;
}

ULONG LookLevel_Micros(const struct LookLevel *ll, ULONG rate)
{
	return rate ? (ll->ll_Frames * 1000000UL + rate / 2UL) / rate : 0;
}

/* the lower middle of the first n pieces' peaks as the look's byte */
static void emit(struct LookLevel *ll, UWORD n)
{
	UWORD v[LOOKLEVEL_PIECES], i, j, t;

	for (i = 0; i < n; i++) {
		t = ll->ll_Peaks[i];
		for (j = i; j > 0 && v[j - 1] > t; j--) v[j] = v[j - 1];
		v[j] = t;
	}
	if (n > 0 && ll->ll_OutCount < ll->ll_OutMax)
		ll->ll_Out[ll->ll_OutCount++] = LevelMap_Byte((ULONG) v[(n - 1) / 2] << 16, TRUE);
	ll->ll_Done = 0;
	ll->ll_Piece = 0;
}

/* where the piece being measured ends, in frames into the look */
static ULONG piece_limit(const struct LookLevel *ll)
{
	return ll->ll_Piece >= LOOKLEVEL_PIECES - 1 ? ll->ll_Frames : (ULONG) (ll->ll_Piece + 1) * ll->ll_PieceFrames;
}

/* a piece measured: its peak kept, the look out when it was the last */
static void piece_end(struct LookLevel *ll)
{
	ll->ll_Peaks[ll->ll_Piece++] = (UWORD) ll->ll_PiecePeak;
	ll->ll_PiecePeak = 0;
	if (ll->ll_Piece == LOOKLEVEL_PIECES) emit(ll, LOOKLEVEL_PIECES);
}

/* one channel's sample through the filter, its peak taken: half the
 * sample, so that the filter's output stays in 16 bits (a 16 x 16
 * multiply); its peak doubled back. The pole's product cut toward 0: a
 * shift alone rounds down, and with a pole this near 1 that drifted the
 * output some 35 steps below 0 (noise read 6 dB loud); rounded to the
 * nearest, a DC offset stalled 35 steps above 0 */
#define FILTER(prev, last) \
	do { \
		x = (LONG) SAMPLE(*w) / 2L; \
		w++; \
		p = (LONG) pole * (LONG) (WORD) (last); \
		y = x - (prev) + (p >= 0 ? p >> 15 : -((-p) >> 15)); \
		if (y > 32767L) y = 32767L; \
		else if (y < -32767L) y = -32767L; \
		(prev) = x; \
		(last) = y; \
		a = (ULONG) (y < 0 ? -y : y) * 2UL; \
		if (a > peak) peak = a; \
	} while (0)

void LookLevel_Feed(struct LookLevel *ll, const UWORD *w, ULONG frames)
{
	LONG x, y, p, prev0, last0, prev1, last1;
	ULONG a, run, peak = ll->ll_PiecePeak, limit = piece_limit(ll);
	WORD pole = ll->ll_Pole;
	UWORD c, ch = ll->ll_Channels;

	while (frames) {
		/* to the piece's end; mono and stereo with the filter in
		   registers, more channels through the arrays */
		run = ll->ll_Done < limit ? limit - ll->ll_Done : 1;
		if (run > frames) run = frames;
		frames -= run;
		ll->ll_Done += run;
		if (ch == 2) {
			prev0 = ll->ll_Prev[0];
			last0 = ll->ll_Last[0];
			prev1 = ll->ll_Prev[1];
			last1 = ll->ll_Last[1];
			while (run--) {
				FILTER(prev0, last0);
				FILTER(prev1, last1);
			}
			ll->ll_Prev[0] = prev0;
			ll->ll_Last[0] = last0;
			ll->ll_Prev[1] = prev1;
			ll->ll_Last[1] = last1;
		} else if (ch == 1) {
			prev0 = ll->ll_Prev[0];
			last0 = ll->ll_Last[0];
			while (run--) FILTER(prev0, last0);
			ll->ll_Prev[0] = prev0;
			ll->ll_Last[0] = last0;
		} else {
			while (run--) {
				for (c = 0; c < ch; c++) FILTER(ll->ll_Prev[c], ll->ll_Last[c]);
			}
		}
		if (ll->ll_Done >= limit) {
			ll->ll_PiecePeak = peak > 32767UL ? 32767UL : peak;
			piece_end(ll);
			peak = 0;
			limit = piece_limit(ll);
		}
	}
	ll->ll_PiecePeak = peak > 32767UL ? 32767UL : peak;
}

void LookLevel_Finish(struct LookLevel *ll)
{
	UWORD n = ll->ll_Piece;

	if (ll->ll_Done == 0) return;
	/* the piece begun counts when it has frames */
	if (ll->ll_Done > (ULONG) ll->ll_Piece * ll->ll_PieceFrames) {
		ll->ll_Peaks[n] = (UWORD) ll->ll_PiecePeak;
		n++;
	}
	ll->ll_PiecePeak = 0;
	emit(ll, n);
}
