/*
 * timeline.c -- the edit list (timeline.h)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "timeline.h"

/* a piece as recorded: no gain, no fade */
static void plain(struct Piece *p)
{
	p->p_Gain = (UWORD) ENV_UNITY;
	p->p_Fade = TL_FADE_NONE;
	p->p_Pad = 0;
	p->p_U0 = p->p_U1 = 0;
}

void Tl_Init(struct Timeline *tl, ULONG frames)
{
	tl->tl_Frames = frames;
	tl->tl_Count = 0;
	tl->tl_Pad = 0;
	if (!frames) return;
	tl->tl_Pieces[0].p_From = 0;
	tl->tl_Pieces[0].p_Frames = frames;
	plain(&tl->tl_Pieces[0]);
	tl->tl_Count = 1;
}

/* as recorded: no level of its own */
static BOOL is_plain(const struct Piece *p)
{
	return (BOOL) (p->p_Gain == ENV_UNITY && !(p->p_Fade & TL_FADE_DIR));
}

BOOL Tl_Whole(const struct Timeline *tl, ULONG frames)
{
	if (!tl->tl_Count) return (BOOL) (frames == 0);
	return (BOOL) (tl->tl_Count == 1 && tl->tl_Pieces[0].p_From == 0 && tl->tl_Pieces[0].p_Frames == frames &&
		       is_plain(&tl->tl_Pieces[0]));
}

BOOL Tl_PieceSame(const struct Piece *a, const struct Piece *b)
{
	if (a->p_From != b->p_From || a->p_Frames != b->p_Frames || a->p_Gain != b->p_Gain || a->p_Fade != b->p_Fade)
		return FALSE;
	return (BOOL) (!(a->p_Fade & TL_FADE_DIR) || (a->p_U0 == b->p_U0 && a->p_U1 == b->p_U1));
}

BOOL Tl_Same(const struct Timeline *a, const struct Timeline *b)
{
	UWORD i;

	if (a->tl_Count != b->tl_Count || a->tl_Frames != b->tl_Frames) return FALSE;
	for (i = 0; i < a->tl_Count; i++) {
		if (!Tl_PieceSame(&a->tl_Pieces[i], &b->tl_Pieces[i])) return FALSE;
	}
	return TRUE;
}

/* a of c as a place 0 .. ENV_U_MAX (a <= c), inside 32 bits: c and a
   halved alike until c fits 16 bits (off by 1/65536 at most) */
static UWORD place(ULONG a, ULONG c)
{
	if (!c) return 0;
	while (c > 65535UL) {
		a >>= 1;
		c >>= 1;
	}
	if (a > c) a = c;
	return (UWORD) (a * ENV_U_MAX / c);
}

/* the place f of the way along a curve's stretch, its first place in
   span's high word, its end's in the low */
static UWORD between(ULONG span, UWORD f)
{
	UWORD u0 = (UWORD) (span >> 16), u1 = (UWORD) span;

	return (UWORD) (u1 >= u0 ? u0 + (ULONG) (u1 - u0) * f / ENV_U_MAX : u0 - (ULONG) (u0 - u1) * f / ENV_U_MAX);
}

BOOL Tl_At(const struct Timeline *tl, ULONG t, struct TlAt *at)
{
	ULONG start = 0;
	UWORD i;
	const struct Piece *p;

	if (t >= tl->tl_Frames) return FALSE;
	for (i = 0; i < tl->tl_Count; i++) {
		p = &tl->tl_Pieces[i];
		if (t - start < p->p_Frames) {
			at->ta_Piece = i;
			at->ta_Start = start;
			at->ta_From = p->p_From == TL_SILENCE ? TL_SILENCE : p->p_From + (t - start);
			at->ta_Run = p->p_Frames - (t - start);
			return TRUE;
		}
		start += p->p_Frames;
	}
	return FALSE;
}

/* q and p one piece: they follow each other in the file with a level
   that runs on -- the same gain, and no fade or the same one whose curve
   goes on where q's ends; or both silence */
static BOOL runs_on(const struct Piece *q, const struct Piece *p)
{
	if (q->p_From == TL_SILENCE || p->p_From == TL_SILENCE) return (BOOL) (q->p_From == p->p_From);
	if (q->p_From + q->p_Frames != p->p_From || q->p_Gain != p->p_Gain || q->p_Fade != p->p_Fade) return FALSE;
	return (BOOL) (!(q->p_Fade & TL_FADE_DIR) || q->p_U1 == p->p_U0);
}

/* pieces that run on made one, empty ones dropped: the one way a
   timeline is written */
static void tidy(struct Timeline *tl)
{
	struct Piece *p = tl->tl_Pieces, *q;
	UWORD i, n = 0;

	for (i = 0; i < tl->tl_Count; i++) {
		if (!p[i].p_Frames) continue;
		if (n) {
			q = &p[n - 1];
			if (runs_on(q, &p[i])) {
				q->p_Frames += p[i].p_Frames;
				q->p_U1 = p[i].p_U1;
				continue;
			}
		}
		p[n++] = p[i];
	}
	tl->tl_Count = n;
}

/* a piece begins at t (made so: the one t lies in split there); its
   index -- tl_Count when t is the end. The caller made room for it. */
static UWORD split_at(struct Timeline *tl, ULONG t)
{
	struct TlAt at;
	struct Piece *p = tl->tl_Pieces;
	ULONG head;
	UWORD i;

	if (!Tl_At(tl, t, &at)) return tl->tl_Count;
	if (at.ta_Start == t) return at.ta_Piece;
	head = t - at.ta_Start;
	for (i = tl->tl_Count; i > at.ta_Piece + 1; i--) p[i] = p[i - 1];
	tl->tl_Count++;
	/* the second half the same level; a fade's curve shared out between
	   the two at the place t lies on it */
	p[at.ta_Piece + 1] = p[at.ta_Piece];
	p[at.ta_Piece + 1].p_From = p[at.ta_Piece].p_From == TL_SILENCE ? TL_SILENCE : p[at.ta_Piece].p_From + head;
	p[at.ta_Piece + 1].p_Frames = p[at.ta_Piece].p_Frames - head;
	if (p[at.ta_Piece].p_Fade & TL_FADE_DIR) {
		UWORD u0 = p[at.ta_Piece].p_U0, u1 = p[at.ta_Piece].p_U1, um;

		um = between(((ULONG) u0 << 16) | u1, place(head, p[at.ta_Piece].p_Frames));
		p[at.ta_Piece].p_U1 = um;
		p[at.ta_Piece + 1].p_U0 = um;
	}
	p[at.ta_Piece].p_Frames = head;
	return (UWORD) (at.ta_Piece + 1);
}

/* tk_First .. tk_End - 1 taken out, a silence of its length put in for
   tk_Silence (one piece more): FALSE, nothing changed, when nothing lies
   there or the pieces would be too many */
struct Take {
	ULONG tk_First, tk_End;
	BOOL tk_Silence;
};

static BOOL take_out(struct Timeline *tl, const struct Take *tk)
{
	struct Piece *p = tl->tl_Pieces;
	ULONG first = tk->tk_First, end = tk->tk_End;
	BOOL silence = tk->tk_Silence;
	UWORD a, b, i, put = (UWORD) (silence ? 1 : 0);

	if (first >= end || end > tl->tl_Frames) return FALSE;
	/* two splits at most and the silence: room for them first */
	if ((ULONG) tl->tl_Count + 2 + put > TL_MAXPIECES) return FALSE;
	a = split_at(tl, first);
	b = split_at(tl, end);
	if (silence) {
		p[a].p_From = TL_SILENCE;
		p[a].p_Frames = end - first;
		plain(&p[a]);
	}
	for (i = 0; b + i < tl->tl_Count; i++) p[a + put + i] = p[b + i];
	tl->tl_Count = (UWORD) (a + put + i);
	if (!silence) tl->tl_Frames -= end - first;
	tidy(tl);
	return TRUE;
}

BOOL Tl_Cut(struct Timeline *tl, ULONG first, ULONG end)
{
	struct Take tk;

	tk.tk_First = first;
	tk.tk_End = end;
	tk.tk_Silence = FALSE;
	return take_out(tl, &tk);
}

BOOL Tl_Silence(struct Timeline *tl, ULONG first, ULONG end)
{
	struct Take tk;

	tk.tk_First = first;
	tk.tk_End = end;
	tk.tk_Silence = TRUE;
	return take_out(tl, &tk);
}

/* the part's pieces set by how: their first and their end found by
   splitting there (room for two first), then each file piece's level */
#define LV_GAIN		0
#define LV_FADE		1
#define LV_PLAIN	2

static BOOL set_level(struct Timeline *tl, const struct TlPart *pt, UWORD how)
{
	struct Piece *p = tl->tl_Pieces;
	ULONG first = pt->tp_First, end = pt->tp_End, start, len = end - first;
	UWORD a, b, i;

	if (first >= end || end > tl->tl_Frames) return FALSE;
	if ((ULONG) tl->tl_Count + 2 > TL_MAXPIECES) return FALSE;
	a = split_at(tl, first);
	b = split_at(tl, end);
	for (i = 0, start = 0; i < a; i++) start += p[i].p_Frames;
	for (i = a; i < b; i++) {
		if (p[i].p_From != TL_SILENCE) {
			if (how == LV_GAIN) {
				p[i].p_Gain = Env_Mul(p[i].p_Gain, pt->tp_Gain);
				if (!p[i].p_Gain) p[i].p_Gain = 1;
			} else if (how == LV_FADE) {
				p[i].p_Fade = pt->tp_Fade;
				p[i].p_U0 = place(start - first, len);
				p[i].p_U1 = place(start + p[i].p_Frames - first, len);
			} else {
				plain(&p[i]);
			}
		}
		start += p[i].p_Frames;
	}
	tidy(tl);
	return TRUE;
}

BOOL Tl_Gain(struct Timeline *tl, const struct TlPart *pt)
{
	return set_level(tl, pt, LV_GAIN);
}

BOOL Tl_Fade(struct Timeline *tl, const struct TlPart *pt)
{
	if (!(pt->tp_Fade & TL_FADE_DIR)) return FALSE;
	return set_level(tl, pt, LV_FADE);
}

BOOL Tl_Plain(struct Timeline *tl, const struct TlPart *pt)
{
	return set_level(tl, pt, LV_PLAIN);
}

/* ---- the level, frame by frame ---- */

/* a piece the timeline plays, and what lies around it */
struct LevelCtx {
	const struct Timeline *lc_Tl;
	const struct TlRead *lc_Rd;	/* Tl_Read's: where the samples go */
	const struct TlLevels *lc_Lv;	/* NULL: none from outside */
	const struct Piece *lc_P;
	ULONG lc_Start;			/* its first timeline frame */
	BOOL lc_CutIn, lc_CutOut;	/* a cut (or silence) at its start, at its end */
};

/* a cut before piece i: a silence meets it, or it does not follow the
   piece before it in the file (a level of its own is no cut) */
static BOOL cut_before(const struct Timeline *tl, UWORD i)
{
	const struct Piece *q, *p;

	if (!i || i >= tl->tl_Count) return FALSE;
	q = &tl->tl_Pieces[i - 1];
	p = &tl->tl_Pieces[i];
	if (q->p_From == TL_SILENCE || p->p_From == TL_SILENCE) return (BOOL) (q->p_From != p->p_From);
	return (BOOL) (q->p_From + q->p_Frames != p->p_From);
}

/* lc for the piece at points into (lc_Tl set) */
static void context(struct LevelCtx *lc, const struct TlAt *at)
{
	lc->lc_P = &lc->lc_Tl->tl_Pieces[at->ta_Piece];
	lc->lc_Start = at->ta_Start;
	lc->lc_CutIn = cut_before(lc->lc_Tl, at->ta_Piece);
	lc->lc_CutOut = cut_before(lc->lc_Tl, (UWORD) (at->ta_Piece + 1));
}

/* the level at timeline frame t of lc's piece */
static UWORD level(const struct LevelCtx *lc, ULONG t)
{
	const struct Piece *p = lc->lc_P;
	const struct TlLevels *lv = lc->lc_Lv;
	ULONG off = t - lc->lc_Start, u, d;
	UWORD g = p->p_Gain;

	if (p->p_Fade & TL_FADE_DIR) {
		u = between(((ULONG) p->p_U0 << 16) | p->p_U1, place(off, p->p_Frames));
		if ((p->p_Fade & TL_FADE_DIR) == TL_FADE_OUT) u = ENV_U_MAX - u;
		g = Env_Mul(g, Env_Curve((UWORD) (p->p_Fade & TL_FADE_SHAPE), (UWORD) u));
	}
	if (!lv) return g;
	if (lv->lv_FadeIn && t >= lv->lv_In && t - lv->lv_In < lv->lv_FadeIn)
		g = Env_Mul(g, Env_Curve(lv->lv_Shape, place(t - lv->lv_In, lv->lv_FadeIn)));
	if (lv->lv_FadeOut && t < lv->lv_Out && lv->lv_Out - t <= lv->lv_FadeOut)
		g = Env_Mul(g, Env_Curve(lv->lv_Shape, place(lv->lv_Out - t, lv->lv_FadeOut)));
	if (lv->lv_Dip) {
		if (lc->lc_CutIn && off < lv->lv_Dip)
			g = Env_Mul(g, Env_Curve(ENV_SHAPE_SCURVE, place(off, lv->lv_Dip)));
		d = p->p_Frames - off;
		if (lc->lc_CutOut && d <= lv->lv_Dip)
			g = Env_Mul(g, Env_Curve(ENV_SHAPE_SCURVE, place(d, lv->lv_Dip)));
	}
	return g;
}

/* nothing to do over first .. first + n - 1 of lc's piece: as recorded,
   no end fade and no dip reaching into it */
static BOOL untouched(const struct LevelCtx *lc, ULONG first, ULONG n)
{
	const struct TlLevels *lv = lc->lc_Lv;
	ULONG end = first + n, off = first - lc->lc_Start;

	if (!is_plain(lc->lc_P)) return FALSE;
	if (!lv) return TRUE;
	if (lv->lv_FadeIn && first < lv->lv_In + lv->lv_FadeIn && end > lv->lv_In) return FALSE;
	if (lv->lv_FadeOut && end > lv->lv_Out - (lv->lv_FadeOut < lv->lv_Out ? lv->lv_FadeOut : lv->lv_Out) &&
	    first < lv->lv_Out)
		return FALSE;
	if (lv->lv_Dip) {
		if (lc->lc_CutIn && off < lv->lv_Dip) return FALSE;
		if (lc->lc_CutOut && lc->lc_P->p_Frames - (off + n) < lv->lv_Dip) return FALSE;
	}
	return TRUE;
}

UWORD Tl_LevelAt(const struct Timeline *tl, const struct TlLevels *lv, ULONG t)
{
	struct TlAt at;
	struct LevelCtx lc;

	if (!Tl_At(tl, t, &at) || at.ta_From == TL_SILENCE) return (UWORD) ENV_UNITY;
	lc.lc_Tl = tl;
	lc.lc_Rd = NULL;
	lc.lc_Lv = lv;
	context(&lc, &at);
	return level(&lc, t);
}

BOOL Tl_Untouched(const struct Timeline *tl, const struct TlLevels *lv, ULONG first, ULONG n)
{
	struct TlAt at;
	struct LevelCtx lc;
	ULONG run;

	lc.lc_Tl = tl;
	lc.lc_Rd = NULL;
	lc.lc_Lv = lv;
	while (n && Tl_At(tl, first, &at)) {
		run = at.ta_Run < n ? at.ta_Run : n;
		if (at.ta_From != TL_SILENCE) {
			context(&lc, &at);
			if (!untouched(&lc, first, run)) return FALSE;
		}
		first += run;
		n -= run;
	}
	return TRUE;
}

const struct TlLevels *Tl_Levels(const struct TlLevels *lv)
{
	return (lv && (lv->lv_FadeIn || lv->lv_FadeOut || lv->lv_Dip)) ? lv : NULL;
}

ULONG Tl_Find(const struct Timeline *tl, ULONG f)
{
	ULONG start = 0;
	UWORD i;
	const struct Piece *p;

	for (i = 0; i < tl->tl_Count; i++) {
		p = &tl->tl_Pieces[i];
		if (p->p_From != TL_SILENCE && f >= p->p_From && f - p->p_From < p->p_Frames) return start + (f - p->p_From);
		start += p->p_Frames;
	}
	return TL_NONE;
}

UWORD Tl_Joins(const struct Timeline *tl, ULONG *out, UWORD max)
{
	ULONG start = 0;
	UWORD i, n = 0;

	/* a cut, or silence's either end -- not where a stretch's level
	   changes (tidy keeps those pieces apart, but nothing was cut there) */
	for (i = 0; i + 1 < tl->tl_Count && n < max; i++) {
		start += tl->tl_Pieces[i].p_Frames;
		if (cut_before(tl, (UWORD) (i + 1))) out[n++] = start;
	}
	return n;
}

BOOL Tl_Look(const struct Timeline *tl, struct TlLook *tk)
{
	struct TlAt at;

	if (!Tl_At(tl, tk->tk_First, &at)) return FALSE;
	tk->tk_From = at.ta_From;
	tk->tk_Level = at.ta_From == TL_SILENCE ? 0 : Tl_LevelAt(tl, tk->tk_Levels, tk->tk_Mid);
	return TRUE;
}

UWORD Tl_Ranges(const struct Timeline *tl, struct TlRanges *tr)
{
	ULONG start = 0, a, b;
	UWORD i, n = 0;
	const struct Piece *p;

	for (i = 0; i < tl->tl_Count && n < tr->tr_Max; i++) {
		p = &tl->tl_Pieces[i];
		if (p->p_From != TL_SILENCE) {
			a = tr->tr_First > p->p_From ? tr->tr_First : p->p_From;
			b = tr->tr_End < p->p_From + p->p_Frames ? tr->tr_End : p->p_From + p->p_Frames;
			if (a < b) {
				a = start + (a - p->p_From);
				b = start + (b - p->p_From);
				/* one stretch where two meet on the timeline */
				if (n && tr->tr_Out[n - 1].tg_End == a) {
					tr->tr_Out[n - 1].tg_End = b;
				} else {
					tr->tr_Out[n].tg_First = a;
					tr->tr_Out[n].tg_End = b;
					n++;
				}
			}
		}
		start += p->p_Frames;
	}
	return n;
}

/* the next frame after x where a level may change its rule: a block's
   start (ENV_BLOCK frames, counted from the timeline's start), an end
   fade's or a dip's edge -- so that a level is the same whoever reads,
   in whatever pieces (Save, the card, the Mac's replay) */
/* the end fades' and the dips' edges around lc's piece into c: how many */
static UWORD zone_edges(const struct LevelCtx *lc, ULONG *c)
{
	const struct TlLevels *lv = lc->lc_Lv;
	ULONG end = lc->lc_Start + lc->lc_P->p_Frames;
	UWORD n = 0;

	if (!lv) return 0;
	c[n++] = lv->lv_In;
	c[n++] = lv->lv_In + lv->lv_FadeIn;
	c[n++] = lv->lv_Out > lv->lv_FadeOut ? lv->lv_Out - lv->lv_FadeOut : 0;
	c[n++] = lv->lv_Out;
	if (lv->lv_Dip) {
		c[n++] = lc->lc_Start + lv->lv_Dip;
		c[n++] = end > lv->lv_Dip ? end - lv->lv_Dip : 0;
	}
	return n;
}

static ULONG next_edge(const struct LevelCtx *lc, ULONG x)
{
	ULONG e = (x | (ENV_BLOCK - 1)) + 1, c[6];
	UWORD i, n = zone_edges(lc, c);

	for (i = 0; i < n; i++) if (c[i] > x && c[i] < e) e = c[i];
	return e;
}

/* the stretch x lies in begins here: the last edge at or before x, the
   piece's start at the earliest -- the frame its level is taken at */
static ULONG edge_at(const struct LevelCtx *lc, ULONG x)
{
	ULONG e = x & ~(ENV_BLOCK - 1), c[6];
	UWORD i, n = zone_edges(lc, c);

	if (e < lc->lc_Start) e = lc->lc_Start;
	for (i = 0; i < n; i++) if (c[i] <= x && c[i] > e) e = c[i];
	return e;
}

/* n frames of a piece's at timeline frame t, in buf, set to their level:
   the stretches cut at every edge (next_edge), each at the level of its
   middle frame -- a fade in or out off by half a block at most either
   way, the stretch the same whoever reads (edge_at) */
struct Stretch {
	ULONG st_First, st_Frames;	/* timeline frames */
	UBYTE *st_Buf;			/* their samples */
};

static void apply(const struct LevelCtx *lc, const struct Stretch *st)
{
	const struct TlRead *rd = lc->lc_Rd;
	struct EnvApply ea;
	ULONG t = st->st_First, n = st->st_Frames, x = t, end = t + n, e;
	UBYTE *buf = st->st_Buf;

	if (untouched(lc, t, n)) return;
	ea.ea_Channels = (UWORD) (rd->rd_Align / 2);
	ea.ea_Clipped = 0;
	while (x < end) {
		ULONG a = edge_at(lc, x), b = next_edge(lc, a), pend = lc->lc_Start + lc->lc_P->p_Frames;

		if (b > pend) b = pend;
		e = b < end ? b : end;
		ea.ea_Buf = buf + (x - t) * rd->rd_Align;
		ea.ea_Frames = e - x;
		ea.ea_Level = level(lc, a + (b - a) / 2);
		Env_Apply(&ea);
		x = e;
	}
	if (rd->rd_Clipped) *rd->rd_Clipped += ea.ea_Clipped;
}

void Tl_ReadInit(struct TlRead *rd)
{
	UBYTE *p = (UBYTE *) rd;
	ULONG i;

	for (i = 0; i < sizeof(*rd); i++) p[i] = 0;
}

ULONG Tl_Read(const struct Timeline *tl, const struct TlRead *rd)
{
	struct TlAt at;
	struct LevelCtx lc;
	struct Stretch st;
	ULONG t = rd->rd_First, left = rd->rd_Frames, n, done = 0, bytes, i;
	UBYTE *buf = rd->rd_Buf;
	LONG got;

	lc.lc_Tl = tl;
	lc.lc_Rd = rd;
	lc.lc_Lv = rd->rd_Levels;
	while (left && Tl_At(tl, t, &at)) {
		n = at.ta_Run < left ? at.ta_Run : left;
		bytes = n * rd->rd_Align;
		if (at.ta_From == TL_SILENCE) {
			for (i = 0; i < bytes; i++) buf[i] = 0;
		} else {
			if (Seek(rd->rd_FH, (LONG) (rd->rd_Start + at.ta_From * rd->rd_Align), OFFSET_BEGINNING) < 0) break;
			if ((got = Read(rd->rd_FH, buf, (LONG) bytes)) <= 0) break;
			context(&lc, &at);
			st.st_First = t;
			st.st_Buf = buf;
			if ((ULONG) got < bytes) {
				st.st_Frames = (ULONG) got / rd->rd_Align;
				if (!rd->rd_AsRecorded) apply(&lc, &st);
				done += st.st_Frames;
				break;
			}
			st.st_Frames = n;
			if (!rd->rd_AsRecorded) apply(&lc, &st);
		}
		buf += bytes;
		t += n;
		left -= n;
		done += n;
	}
	return done;
}
