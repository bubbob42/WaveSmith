/*
 * waveplan.c -- waveform.gadget's arithmetic (waveplan.h)
 */

#include "waveplan.h"

/* ---- the view ---- */

ULONG WaveView_ZoomAll(const struct WaveView *v)
{
	ULONG cols = v->wv_Columns ? v->wv_Columns : 1, z;

	z = v->wv_Frames / cols + (v->wv_Frames % cols ? 1 : 0);
	return z ? z : 1;
}

static ULONG zoom_max(const struct WaveView *v)
{
	ULONG all = WaveView_ZoomAll(v);

	return v->wv_ZoomMax && v->wv_ZoomMax < all ? v->wv_ZoomMax : all;
}

ULONG WaveView_Visible(const struct WaveView *v)
{
	ULONG cols = v->wv_Columns ? v->wv_Columns : 1;

	/* clamped, zoom <= frames / cols + 1: the product stays near frames */
	if (v->wv_Zoom > 0xFFFFFFFFUL / cols) return 0xFFFFFFFFUL;
	return cols * v->wv_Zoom;
}

void WaveView_Clamp(struct WaveView *v)
{
	ULONG zmin = v->wv_ZoomMin ? v->wv_ZoomMin : 1, zmax = zoom_max(v), vis;

	if (zmin > zmax) zmin = zmax;
	if (v->wv_Zoom < zmin) v->wv_Zoom = zmin;
	if (v->wv_Zoom > zmax) v->wv_Zoom = zmax;
	vis = WaveView_Visible(v);
	if (vis >= v->wv_Frames)
		v->wv_Top = 0;
	else if (v->wv_Top > v->wv_Frames - vis)
		v->wv_Top = v->wv_Frames - vis;
}

void WaveView_Fit(struct WaveView *v)
{
	v->wv_Zoom = zoom_max(v);
	v->wv_Top = 0;
	WaveView_Clamp(v);
}

ULONG WaveView_FrameAt(const struct WaveView *v, LONG x)
{
	LONG cols = (LONG) v->wv_Columns;
	ULONG d;

	if (x < -cols) x = -cols;
	if (x > 2 * cols) x = 2 * cols;
	if (x < 0) {
		d = (ULONG) (-x) * v->wv_Zoom;		/* a view's frames at most */
		return d >= v->wv_Top ? 0 : v->wv_Top - d;
	}
	d = (ULONG) x * v->wv_Zoom;			/* two views' frames at most */
	if (d > v->wv_Frames || v->wv_Top > v->wv_Frames - d) return v->wv_Frames;
	return v->wv_Top + d;
}

LONG WaveView_ColumnOf(const struct WaveView *v, ULONG frame)
{
	ULONG d;

	if (frame == WAVEFORM_NONE || frame < v->wv_Top || v->wv_Zoom == 0) return -1;
	d = (frame - v->wv_Top) / v->wv_Zoom;
	return d < v->wv_Columns ? (LONG) d : -1;
}

void WaveView_Zoom(struct WaveView *v, LONG steps, ULONG centre)
{
	LONG x = WaveView_ColumnOf(v, centre);
	ULONG z = v->wv_Zoom, zmax = zoom_max(v), back;

	if (x < 0) {
		x = v->wv_Columns / 2;
		centre = WaveView_FrameAt(v, x);
	}
	for (; steps > 0 && z > 1; steps--) z = (z + 1) >> 1;
	for (; steps < 0 && z < zmax; steps++) z = z > zmax / 2 ? zmax : z * 2;
	v->wv_Zoom = z;
	back = (ULONG) x * z;				/* a view's frames at most */
	v->wv_Top = centre > back ? centre - back : 0;
	WaveView_Clamp(v);
}

void WaveView_Show(struct WaveView *v, const struct WaveRange *r)
{
	ULONG span, m, total, cols = v->wv_Columns ? v->wv_Columns : 1;
	UWORD margin = r->wr_Margin > 100 ? 100 : r->wr_Margin;

	span = r->wr_End > r->wr_First ? r->wr_End - r->wr_First : 1;
	/* span x margin / 100 without the product: span / 100 x 100 fits */
	m = span / 100 * margin + span % 100 * margin / 100;
	total = span + 2 * m;				/* 3 spans at most */
	if (total < span) total = 0xFFFFFFFFUL;
	v->wv_Zoom = total / cols + (total % cols ? 1 : 0);
	v->wv_Top = r->wr_First > m ? r->wr_First - m : 0;
	WaveView_Clamp(v);
}

BOOL WaveView_Follow(struct WaveView *v, ULONG pos, UWORD mode)
{
	ULONG old = v->wv_Top, vis = WaveView_Visible(v), lead;

	if (pos == WAVEFORM_NONE || mode == WAVEFORM_FOLLOW_NONE) return FALSE;
	if (mode == WAVEFORM_FOLLOW_CENTER) {
		lead = (ULONG) (v->wv_Columns / 2) * v->wv_Zoom;
		v->wv_Top = pos > lead ? pos - lead : 0;
	} else if (pos < v->wv_Top || pos - v->wv_Top >= vis) {
		/* a page on: the head a sixteenth in from the left. Run off the
		   right edge, the page turns by its width less that sixteenth,
		   wherever the head got to since the last look -- the next page
		   is known before it shows (its samples read ahead); a jump
		   back or beyond it: from the head */
		lead = (ULONG) (v->wv_Columns / 16) * v->wv_Zoom;
		if (pos >= v->wv_Top && vis > lead && pos - v->wv_Top - vis < vis - lead) v->wv_Top += vis - lead;
		else v->wv_Top = pos > lead ? pos - lead : 0;
	}
	WaveView_Clamp(v);
	return (BOOL) (v->wv_Top != old);
}

/* ---- time ---- */

/* n with at least digits digits at p: the end */
static char *put_num(char *p, ULONG n, UWORD digits)
{
	char d[12];
	UWORD k = 0;

	do {
		d[k++] = (char) ('0' + n % 10);
		n /= 10;
	} while (n && k < 11);
	while (k < digits && k < 11) d[k++] = '0';
	while (k) *p++ = d[--k];
	return p;
}

UWORD WavePlan_Time(const struct WaveClock *c, ULONG frame, char *buf)
{
	ULONG rate = c->wc_Rate ? c->wc_Rate : 1, f, s, frac;
	UWORD dec = c->wc_Decimals > 3 ? 3 : c->wc_Decimals;
	char *p = buf;

	if (frame >= c->wc_Origin) {
		f = frame - c->wc_Origin;
	} else {
		f = c->wc_Origin - frame;
		*p++ = '-';
	}
	if (c->wc_Format == WAVEFORM_TIME_FRAMES) {
		p = put_num(p, f, 1);
		*p = '\0';
		return (UWORD) (p - buf);
	}
	s = f / rate;
	/* f % rate < rate: x 1000 fits for any rate below 4 MHz */
	frac = rate < 4000000UL ? f % rate * 1000UL / rate : f % rate / (rate / 1000UL);
	if (c->wc_Format == WAVEFORM_TIME_SECONDS) {
		p = put_num(p, s, 1);
	} else if (s >= 3600 || c->wc_Format == WAVEFORM_TIME_HMS) {
		p = put_num(p, s / 3600, 1);
		*p++ = ':';
		p = put_num(p, s / 60 % 60, 2);
		*p++ = ':';
		p = put_num(p, s % 60, 2);
	} else {
		p = put_num(p, s / 60, 1);
		*p++ = ':';
		p = put_num(p, s % 60, 2);
	}
	if (dec) {
		*p++ = '.';
		p = put_num(p, dec == 1 ? frac / 100 : dec == 2 ? frac / 10 : frac, dec);
	}
	*p = '\0';
	return (UWORD) (p - buf);
}

ULONG WavePlan_ParseTime(const struct WaveClock *c, CONST_STRPTR s)
{
	ULONG part[3], ms = 0, n, f, scale;
	UWORD parts = 0, digits;
	BOOL neg = FALSE;

	while (*s == ' ') s++;
	if (*s == '-') {
		neg = TRUE;
		s++;
	}
	part[0] = part[1] = part[2] = 0;
	/* up to three numbers apart by ':' */
	for (;;) {
		n = 0;
		digits = 0;
		while (*s >= '0' && *s <= '9') {
			if (n > 429496728UL) return WAVEFORM_NONE;		/* the next digit would wrap */
			n = n * 10 + (ULONG) (*s++ - '0');
			digits++;
		}
		if (!digits && *s != '.') return WAVEFORM_NONE;
		if (parts >= 3) return WAVEFORM_NONE;
		part[parts++] = n;
		if (*s != ':') break;
		s++;
	}
	if (c->wc_Format == WAVEFORM_TIME_FRAMES) {
		if (parts != 1 || *s) return WAVEFORM_NONE;
		f = part[0];
	} else {
		/* the fraction: thousandths at most */
		if (*s == '.') {
			s++;
			for (scale = 100; *s >= '0' && *s <= '9'; s++) {
				ms += (ULONG) (*s - '0') * scale;
				scale /= 10;
			}
		}
		while (*s == ' ') s++;
		if (*s) return WAVEFORM_NONE;
		/* seconds: h:m:s, m:s or s; under 50 days in ms */
		if (parts == 3) n = part[0] * 3600UL + part[1] * 60UL + part[2];
		else if (parts == 2) n = part[0] * 60UL + part[1];
		else n = part[0];
		if (n > 4294966UL) return WAVEFORM_NONE;
		f = WavePlan_MsToFrames(n * 1000UL + ms, c->wc_Rate);
	}
	if (neg) {
		if (f > c->wc_Origin) return WAVEFORM_NONE;
		return c->wc_Origin - f;
	} else {
		if (f > 0xFFFFFFFFUL - c->wc_Origin) return WAVEFORM_NONE;
		return c->wc_Origin + f;
	}
}

ULONG WavePlan_MsToFrames(ULONG ms, ULONG rate)
{
	/* ms / 1000 x rate: 4.29 M s x 1000 would not fit, nor would ms x rate */
	return ms / 1000UL * rate + ms % 1000UL * rate / 1000UL;
}

ULONG WavePlan_FramesToMs(ULONG frames, ULONG rate)
{
	if (rate == 0) return 0;
	return frames / rate * 1000UL + frames % rate * 1000UL / rate;
}

/* ---- the ruler ---- */

/* the labelled steps, and how many small ticks between two */
static const ULONG nice_ms[] = {
	1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 30000,
	60000, 120000, 300000, 600000, 900000, 1800000, 3600000, 7200000
};
static const UBYTE minor_div[] = {
	5, 4, 5, 5, 4, 5, 5, 4, 5, 5, 4, 5, 5, 3, 3, 4, 4, 5, 5, 3, 3, 4, 4
};
#define NICE_COUNT	(sizeof(nice_ms) / sizeof(nice_ms[0]))

BOOL WavePlan_Ruler(const struct WaveView *v, struct WaveRuler *r)
{
	struct WaveClock ck;
	char buf[WAVEPLAN_TIMELEN];
	ULONG frames, px, need, last, vis;
	UWORD i;

	if (r->wru_Rate == 0 || v->wv_Zoom == 0) return FALSE;
	vis = WaveView_Visible(v);
	last = vis > v->wv_Frames - (v->wv_Top < v->wv_Frames ? v->wv_Top : v->wv_Frames) ?
	       v->wv_Frames : v->wv_Top + vis;
	ck.wc_Rate = r->wru_Rate;
	ck.wc_Origin = 0;
	ck.wc_Format = WAVEFORM_TIME_AUTO;
	for (i = 0; i < NICE_COUNT; i++) {
		frames = WavePlan_MsToFrames(nice_ms[i], r->wru_Rate);
		if (frames == 0) continue;
		px = frames / v->wv_Zoom;
		ck.wc_Decimals = (UWORD) (nice_ms[i] < 10 ? 3 : nice_ms[i] < 100 ? 2 : nice_ms[i] < 1000 ? 1 : 0);
		/* the longest label: the view's last time's */
		need = (ULONG) WavePlan_Time(&ck, last, buf) * r->wru_CharWidth + r->wru_Gap;
		if (need > v->wv_Columns) continue;		/* no label fits the view */
		if (px >= need) {
			r->wru_Major = frames;
			r->wru_Minor = frames / minor_div[i] ? frames / minor_div[i] : 1;
			r->wru_MajorMs = nice_ms[i];
			r->wru_Div = minor_div[i];
			r->wru_Decimals = ck.wc_Decimals;
			return TRUE;
		}
	}
	return FALSE;
}

/* ---- what is under the pointer ---- */

static ULONG dist(LONG a, LONG b)
{
	return a > b ? (ULONG) (a - b) : (ULONG) (b - a);
}

ULONG WavePlan_Part(const struct WaveHit *h, LONG x, LONG y)
{
	ULONG best = 0xFFFFFFFFUL, d, part = WAVEPART_NONE;
	UWORD i;

	if (x < 0 || y < 0 || x >= (LONG) h->wh_Width || y >= (LONG) h->wh_Height) return WAVEPART_NONE;
	/* the handles: the nearer; on one column, the side the pointer is on
	   (left of it Out's kept part ends, right of it In's begins) */
	if (h->wh_In >= 0 && (d = dist(x, h->wh_In)) <= h->wh_Grab) {
		best = d;
		part = WAVEPART_TRIMIN;
	}
	if (h->wh_Out >= 0 && (d = dist(x, h->wh_Out)) <= h->wh_Grab && (d < best || (d == best && x < h->wh_Out))) {
		best = d;
		part = WAVEPART_TRIMOUT;
	}
	if (part != WAVEPART_NONE) return part;
	/* a handle's flag grabs it as well as its line */
	if (y >= h->wh_FlagTop && y < h->wh_FlagBottom) {
		if (h->wh_In >= 0 && x >= h->wh_InFlagX0 && x <= h->wh_InFlagX1) return WAVEPART_TRIMIN;
		if (h->wh_Out >= 0 && x >= h->wh_OutFlagX0 && x <= h->wh_OutFlagX1) return WAVEPART_TRIMOUT;
	}
	for (i = 0; i < h->wh_MarkerCount && h->wh_Markers; i++) {
		if (h->wh_Markers[i] < 0) continue;
		if ((d = dist(x, h->wh_Markers[i])) <= h->wh_Grab && d < best) {
			best = d;
			part = WAVEPART_MARKER | ((ULONG) i << 8);
		}
	}
	if (part != WAVEPART_NONE) return part;
	/* each edge on its own: the other may be out of view */
	if (h->wh_SelStart >= 0 && (d = dist(x, h->wh_SelStart)) <= h->wh_Grab) {
		best = d;
		part = WAVEPART_SELSTART;
	}
	if (h->wh_SelEnd >= 0 && (d = dist(x, h->wh_SelEnd)) <= h->wh_Grab &&
	    (d < best || (d == best && x >= h->wh_SelEnd)))
		part = WAVEPART_SELEND;
	if (part != WAVEPART_NONE) return part;
	if (h->wh_BoxLeft >= 0) return (ULONG) (x >= h->wh_BoxLeft && x <= h->wh_BoxRight ? WAVEPART_BOX : WAVEPART_BOXOUT);
	if (y >= h->wh_RulerTop && y < h->wh_RulerBottom) return WAVEPART_RULER;
	return WAVEPART_WAVE;
}

/* ---- snapping ---- */

ULONG WavePlan_Snap(const struct WaveSnap *s, ULONG frame)
{
	ULONG best = WAVEFORM_NONE, bd = 0xFFFFFFFFUL, d, r, t;
	UWORD i;

	if ((s->wsn_Flags & WAVEFORM_SNAP_MARKERS) && s->wsn_Targets) {
		for (i = 0; i < s->wsn_TargetCount; i++) {
			t = s->wsn_Targets[i];
			if (t == WAVEFORM_NONE) continue;
			d = t > frame ? t - frame : frame - t;
			if (d <= s->wsn_Distance && d < bd) {
				bd = d;
				best = t;
			}
		}
		if (best != WAVEFORM_NONE) return best;
	}
	if ((s->wsn_Flags & WAVEFORM_SNAP_STEP) && s->wsn_Step > 1) {
		r = frame % s->wsn_Step;
		if (r < s->wsn_Step - r || frame - r > 0xFFFFFFFFUL - s->wsn_Step) return frame - r;
		return frame - r + s->wsn_Step;
	}
	return frame;
}

/* the channels' sum at frame f of the samples */
static LONG mix(const struct WaveSamples *s, ULONG f)
{
	const WORD *p = s->wsm_Data + (f - s->wsm_First) * s->wsm_Channels;
	LONG sum = 0;
	UWORD c;

	for (c = 0; c < s->wsm_Channels; c++) sum += p[c];
	return sum;
}

/* f a crossing: the sum changes sign from f - 1 to f, or is 0 at f */
static BOOL crossing(const struct WaveSamples *s, ULONG f)
{
	LONG a = mix(s, f - 1), b = mix(s, f);

	return (BOOL) (b == 0 || (a < 0 && b > 0) || (a > 0 && b < 0));
}

BOOL WavePlan_ZeroCrossing(const struct WaveSamples *s, struct WaveZero *z)
{
	ULONG lo, hi, d, want = z->wz_Want;

	z->wz_Found = want;
	if (!s->wsm_Data || s->wsm_Frames < 2 || s->wsm_Channels == 0) return FALSE;
	lo = want > z->wz_Within ? want - z->wz_Within : 0;
	if (lo < s->wsm_First + 1) lo = s->wsm_First + 1;
	hi = z->wz_Within > 0xFFFFFFFFUL - want ? 0xFFFFFFFFUL : want + z->wz_Within;
	if (hi > s->wsm_First + s->wsm_Frames - 1) hi = s->wsm_First + s->wsm_Frames - 1;
	if (lo > hi) return FALSE;
	/* outwards from want: the first found is the nearest (the earlier on a tie) */
	for (d = 0; d <= z->wz_Within; d++) {
		if (want >= d && want - d >= lo && want - d <= hi && crossing(s, want - d)) {
			z->wz_Found = want - d;
			return TRUE;
		}
		if (d && want + d >= lo && want + d <= hi && want + d > want && crossing(s, want + d)) {
			z->wz_Found = want + d;
			return TRUE;
		}
		if ((want < d || want - d < lo) && want + d >= hi) break;
	}
	return FALSE;
}

/* a sample's top byte, floor(v / 256) (as WavePeaks_Top) */
/* a sample's top byte, -128..127: (v + 32768) / 256 - 128 in unsigned
   arithmetic, floor for both signs (>> of a negative value is the
   compiler's choice); a macro, for the per-sample loop below */
#define TOP_BYTE(v)	((BYTE) ((LONG) (((ULONG) ((LONG) (v) + 32768L)) >> 8) - 128L))

ULONG WavePlan_FromSamples(const struct WaveSamples *s, struct WavePeakMsg *q)
{
	ULONG i, f, n, k, end;
	UWORD c, ch = s->wsm_Channels;
	BYTE *out = q->wpm_Out;
	WORD mn, mx, mn2, mx2, v;
	const WORD *col, *p;

	if (!s->wsm_Data || q->wpm_Step == 0 || ch == 0) return 0;
	end = s->wsm_First + s->wsm_Frames;
	f = q->wpm_First;
	for (i = 0; i < q->wpm_Count; i++) {
		/* the column's first frame, a step on from the last; below 4 G */
		if (i) {
			if (q->wpm_Step > 0xFFFFFFFFUL - f) break;
			f += q->wpm_Step;
		}
		if (f < s->wsm_First || f >= end) break;
		n = q->wpm_Step > end - f ? end - f : q->wpm_Step;
		/* one product a column, none a sample (a 68000 calls for each);
		   the samples compared as they are, the top bytes taken of the
		   column's least and greatest only -- TOP_BYTE never turns two
		   samples round, the bytes are the same (a 68030 spent 0.3 s a
		   view of 620 columns on a top byte a sample) */
		col = s->wsm_Data + (f - s->wsm_First) * ch;
		if (ch == 2) {
			/* both channels in one walk */
			mn = mx = col[0];
			mn2 = mx2 = col[1];
			for (p = col + 2, k = n - 1; k; k--, p += 2) {
				if ((v = p[0]) < mn) mn = v;
				else if (v > mx) mx = v;
				if ((v = p[1]) < mn2) mn2 = v;
				else if (v > mx2) mx2 = v;
			}
			*out++ = TOP_BYTE(mn);
			*out++ = TOP_BYTE(mx);
			*out++ = TOP_BYTE(mn2);
			*out++ = TOP_BYTE(mx2);
			continue;
		}
		for (c = 0; c < ch; c++) {
			p = col + c;
			mn = mx = *p;
			for (p += ch, k = n - 1; k; k--, p += ch) {
				if ((v = *p) < mn) mn = v;
				else if (v > mx) mx = v;
			}
			*out++ = TOP_BYTE(mn);
			*out++ = TOP_BYTE(mx);
		}
	}
	return i;
}

/* ---- a lane's scale ---- */

/* 20 log10(v / 127) in tenths of a dB, v 1..127 (0: below anything) */
static const WORD db_tenths[128] = {
	-9999, -421, -361, -325, -300, -281, -265, -252, -240, -230, -221, -212,
	-205, -198, -192, -186, -180, -175, -170, -165, -161, -156, -152, -148,
	-145, -141, -138, -134, -131, -128, -125, -122, -120, -117, -114, -112,
	-110, -107, -105, -103, -100, -98, -96, -94, -92, -90, -88, -86,
	-85, -83, -81, -79, -78, -76, -74, -73, -71, -70, -68, -67,
	-65, -64, -62, -61, -60, -58, -57, -56, -54, -53, -52, -51,
	-49, -48, -47, -46, -45, -43, -42, -41, -40, -39, -38, -37,
	-36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25,
	-24, -23, -23, -22, -21, -20, -19, -18, -17, -17, -16, -15,
	-14, -13, -12, -12, -11, -10, -9, -9, -8, -7, -6, -6,
	-5, -4, -3, -3, -2, -1, -1, 0
};

WORD WavePlan_Scale(const struct WaveScale *s, LONG v)
{
	LONG a = v < 0 ? -v : v, px, range;
	LONG half = s->wsc_Half, gain = s->wsc_Gain ? s->wsc_Gain : 100;

	/* the gain first: a x gain <= 128 x 65535, fits */
	a = a * gain / 100;
	if (a > 127) a = 127;
	if (s->wsc_Mode == WAVEFORM_SCALE_DB) {
		/* the top byte reaches 42 dB down at most: a range beyond that
		   leaves the quietest lines empty */
		range = s->wsc_DBRange ? (LONG) (s->wsc_DBRange > 120 ? 120 : s->wsc_DBRange) * 10 : 600;
		if (a == 0) return 0;
		px = (range + db_tenths[a]) * half / range;	/* <= 1200 x 32767: fits */
		if (px < 0) px = 0;
	} else {
		px = (a * half + 63) / 127;			/* <= 127 x 32767 */
	}
	if (px > half) px = half;
	return (WORD) (v < 0 ? -px : px);
}

/* ---- the level a fade's line shows ---- */

/* (1 - cos(pi i / 32)) / 2 in 4.12, i 0 .. 32 */
static const UWORD scurve[33] = {
	0, 10, 39, 88, 156, 242, 345, 465, 600, 749, 910, 1083, 1264, 1453, 1648, 1847, 2048,
	2249, 2448, 2643, 2832, 3013, 3186, 3347, 3496, 3631, 3751, 3854, 3940, 4008, 4057, 4086, 4096
};

UWORD WavePlan_Curve(UWORD shape, UWORD u)
{
	ULONG i, f;

	switch (shape) {
	case WAVELEVEL_LINEAR:
		return (UWORD) (((ULONG) u * WAVELEVEL_UNITY + WAVELEVEL_U_MAX / 2) / WAVELEVEL_U_MAX);
	case WAVELEVEL_SCURVE:
		/* the table's 32 steps, the place between two by straight line */
		if (u == WAVELEVEL_U_MAX) return (UWORD) WAVELEVEL_UNITY;
		i = (ULONG) u >> 11;
		f = (ULONG) u & 2047UL;
		return (UWORD) (scurve[i] + (((ULONG) (scurve[i + 1] - scurve[i]) * f + 1024UL) >> 11));
	default:
		/* Even: u squared, halved first (the rounding's half would pass
		   32 bits near the top) -- as the program's */
		if (u == WAVELEVEL_U_MAX) return (UWORD) WAVELEVEL_UNITY;
		return (UWORD) (((((ULONG) u * (ULONG) u) >> 1) + (1UL << 18)) >> 19);
	}
}

/* a of c as a place 0 .. WAVELEVEL_U_MAX (a <= c), inside 32 bits: both
   halved alike until c fits 16 bits */
static UWORD level_place(ULONG a, ULONG c)
{
	if (!c) return 0;
	while (c > 65535UL) {
		a >>= 1;
		c >>= 1;
	}
	if (a > c) a = c;
	return (UWORD) (a * WAVELEVEL_U_MAX / c);
}

/* a times b, both 4.12, rounded (a gain at most 32767: fits) */
static UWORD level_mul(UWORD a, UWORD b)
{
	ULONG v = ((ULONG) a * (ULONG) b + 2048UL) >> 12;

	return (UWORD) (v > 0xFFFEUL ? 0xFFFEUL : v);
}

/* a level's own at f (inside it): its gain times its stretch of its curve */
static UWORD level_own(const struct WaveLevel *lv, ULONG f)
{
	UWORD p = level_place(f - lv->wlv_First, lv->wlv_End - lv->wlv_First), u0 = lv->wlv_U0, u1 = lv->wlv_U1, u;

	u = (UWORD) (u1 >= u0 ? u0 + (ULONG) (u1 - u0) * p / WAVELEVEL_U_MAX : u0 - (ULONG) (u0 - u1) * p / WAVELEVEL_U_MAX);
	if ((lv->wlv_Fade & WAVELEVEL_DIR) == WAVELEVEL_OUT) u = (UWORD) (WAVELEVEL_U_MAX - u);
	return level_mul(lv->wlv_Gain, WavePlan_Curve((UWORD) (lv->wlv_Fade & WAVELEVEL_SHAPE), u));
}

UWORD WavePlan_LevelAt(struct WaveLevelWalk *w, ULONG f)
{
	const struct WaveLevel *lv;
	UWORD v = WAVELEVEL_UNITY;
	BOOL line = FALSE;

	/* the program's: past those ended at f, then the one f lies in */
	w->lw_Gain = WAVELEVEL_UNITY;
	while (w->lw_Next < w->lw_Count && w->lw_Level[w->lw_Next].wlv_End <= f) w->lw_Next++;
	if (w->lw_Next < w->lw_Count && (lv = &w->lw_Level[w->lw_Next])->wlv_First <= f) {
		w->lw_Gain = lv->wlv_Gain;
		if (lv->wlv_Fade & WAVELEVEL_DIR) {
			v = level_own(lv, f);
			line = TRUE;
		} else {
			v = lv->wlv_Gain;
		}
	}
	/* the ends' */
	if (w->lw_FadeIn && f >= w->lw_In && f - w->lw_In < w->lw_FadeIn) {
		v = level_mul(v, WavePlan_Curve(w->lw_Shape, level_place(f - w->lw_In, w->lw_FadeIn)));
		line = TRUE;
	}
	if (w->lw_FadeOut && f < w->lw_Out && w->lw_Out - f <= w->lw_FadeOut) {
		v = level_mul(v, WavePlan_Curve(w->lw_Shape, level_place(w->lw_Out - f, w->lw_FadeOut)));
		line = TRUE;
	}
	if (!line) return WAVEPLAN_NOLINE;
	return (UWORD) (v > WAVELEVEL_UNITY ? WAVELEVEL_UNITY : v);
}

/* ---- the repaint ---- */

ULONG WavePlan_Sig(ULONG seed, const void *p, ULONG n)
{
	const UBYTE *b = (const UBYTE *) p;
	ULONG h = seed;

	/* the FNV prime 2^24 + 2^8 + 2^7 + 2^4 + 2 + 1 as shifts: the same
	   value, no 32-bit product (a call for each on a 68000) */
	while (n--) {
		h ^= *b++;
		h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
	}
	return h;
}
