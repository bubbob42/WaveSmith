/*
 * trimplan.c -- the ends WaveSmith suggests to cut off (trimplan.h)
 */

#include "trimplan.h"
#include "gapfind.h"

#define LOOK_NONE	255
#define MAX_PERIODS	32

static void add_region(struct TrimPlan *tp, struct WaveRegion *r)
{
	if (tp->tp_RegionCount < TRIMPLAN_REGIONS) tp->tp_Region[tp->tp_RegionCount++] = *r;
}

/* look k a click: its plain peak loud (above silence) and far above its
   crackle-proof level */
static BOOL is_click(const struct TrimLooks *tl, ULONG k, UWORD threshold)
{
	UBYTE p = tl->tl_Peak[k], l = tl->tl_Level[k];

	if (p == LOOK_NONE || p > threshold) return FALSE;
	return (BOOL) (l == LOOK_NONE || (UWORD) l >= (UWORD) p + TRIMPLAN_CLICK_DB);
}

/* the median of n spacings (n <= MAX_PERIODS), sorted in place */
static ULONG median(ULONG *v, UWORD n)
{
	UWORD i, j;
	ULONG t;

	for (i = 1; i < n; i++) {
		t = v[i];
		for (j = i; j > 0 && v[j - 1] > t; j--) v[j] = v[j - 1];
		v[j] = t;
	}
	return n ? v[n / 2] : 0;
}

/* the clicks of the lead-in (or of the run-out: tail) as regions,
   counted; the run-out's also give their spacing */
static UWORD clicks(const struct TrimLooks *tl, struct TrimPlan *tp, BOOL tail)
{
	struct WaveRegion r;
	ULONG k, last = 0xFFFFFFFFUL, gaps[MAX_PERIODS];
	ULONG lo = tail ? tp->tp_MusicEnd / tl->tl_LookFrames : 0;
	ULONG hi = tail ? tl->tl_Count : tp->tp_MusicStart / tl->tl_LookFrames;
	UWORD n = 0, g = 0;
	BOOL in = FALSE;

	r.wrg_Kind = WAVEREGION_CLICK;
	r.wrg_Style = WAVEREGION_TICK;
	r.wrg_Pen = -1;
	for (k = lo; k < hi && k < tl->tl_Count; k++) {
		if (!is_click(tl, k, tp->tp_Threshold)) {
			in = FALSE;
			continue;
		}
		if (in) continue;		/* one click over two looks: one */
		in = TRUE;
		n++;
		r.wrg_First = k * tl->tl_LookFrames;
		r.wrg_End = r.wrg_First + tl->tl_LookFrames;
		add_region(tp, &r);
		if (last != 0xFFFFFFFFUL && g < MAX_PERIODS) gaps[g++] = k - last;
		last = k;
	}
	if (tail && g >= 2) {
		/* looks to ms: a look's frames x 1000 / rate, times the looks */
		tp->tp_PeriodMs = median(gaps, g) * (tl->tl_LookFrames * 1000UL / (tl->tl_Rate ? tl->tl_Rate : 1));
	}
	return n;
}

BOOL TrimPlan_Make(const struct TrimLooks *tl, struct TrimPlan *tp)
{
	struct GapScan gs;
	static struct GapResult gr;		/* 99 tracks: not on the stack */
	struct WaveRegion r;
	ULONG look_ms, end;
	UWORD learnt, t;

	tp->tp_Found = FALSE;
	tp->tp_In = 0;
	tp->tp_Out = tl->tl_Frames;
	tp->tp_MusicStart = tp->tp_MusicEnd = 0;
	tp->tp_LeadClicks = tp->tp_TailClicks = 0;
	tp->tp_PeriodMs = 0;
	tp->tp_RegionCount = 0;
	tp->tp_TrackCount = 0;
	tp->tp_Threshold = 48;
	if (!tl->tl_Count || !tl->tl_LookFrames || !tl->tl_Rate) return FALSE;
	look_ms = tl->tl_LookFrames * 1000UL / tl->tl_Rate;		/* 1764 x 1000: fits */
	if (!look_ms) look_ms = 1;
	GapFind_Defaults(&gs, look_ms);
	gs.gs_Looks = tl->tl_Level;
	gs.gs_Count = tl->tl_Count;
	gs.gs_KeepEnds = FALSE;
	gs.gs_LeadMs = tl->tl_LeadMs;
	gs.gs_TailMs = tl->tl_TailMs;
	if ((learnt = GapFind_Learn(&gs)) != 0 || (learnt = GapFind_Quiet(&gs)) != 0) gs.gs_Threshold = learnt;
	tp->tp_Threshold = gs.gs_Threshold;
	if ((t = GapFind_Tracks(&gs, &gr)) == 0) return FALSE;
	tp->tp_Found = TRUE;
	for (tp->tp_TrackCount = 0; tp->tp_TrackCount < t && tp->tp_TrackCount < GAPFIND_MAXTRACKS; tp->tp_TrackCount++)
		tp->tp_TrackStart[tp->tp_TrackCount] = gr.gr_Track[tp->tp_TrackCount].gt_Start * tl->tl_LookFrames;
	tp->tp_MusicStart = gr.gr_MusicStart * tl->tl_LookFrames;
	tp->tp_MusicEnd = gr.gr_MusicEnd * tl->tl_LookFrames;
	tp->tp_In = gr.gr_Track[0].gt_Start * tl->tl_LookFrames;
	end = gr.gr_Track[t - 1].gt_End;		/* <= the looks: x a look's frames fits */
	tp->tp_Out = end * tl->tl_LookFrames;
	if (tp->tp_Out > tl->tl_Frames) tp->tp_Out = tl->tl_Frames;
	if (tp->tp_MusicEnd > tl->tl_Frames) tp->tp_MusicEnd = tl->tl_Frames;
	/* the lead-in and the run-out: silence, as found */
	r.wrg_Kind = WAVEREGION_SILENCE;
	r.wrg_Style = WAVEREGION_TINT;
	r.wrg_Pen = -1;
	if (tp->tp_MusicStart > 0) {
		r.wrg_First = 0;
		r.wrg_End = tp->tp_MusicStart;
		add_region(tp, &r);
	}
	if (tp->tp_MusicEnd < tl->tl_Frames) {
		r.wrg_First = tp->tp_MusicEnd;
		r.wrg_End = tl->tl_Frames;
		add_region(tp, &r);
	}
	if (tl->tl_Peak) {
		tp->tp_LeadClicks = clicks(tl, tp, FALSE);
		tp->tp_TailClicks = clicks(tl, tp, TRUE);
	}
	return TRUE;
}
