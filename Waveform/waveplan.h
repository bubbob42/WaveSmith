#ifndef WAVEFORM_WAVEPLAN_H
#define WAVEFORM_WAVEPLAN_H

/*
 * waveplan.h -- waveform.gadget's arithmetic, without a window: the view
 * (which frames each column shows, zoom and scroll), the one mapping
 * between columns and frames, the ruler's steps and the time's text, what
 * part of the gadget is under the pointer, snapping, zero crossings, the
 * lanes' scale (linear or dB), a column's signature for the repaint. The
 * class (waveform.c) holds the state and draws; this decides.
 *
 * Every function keeps to the register budget (2 pointers + 2 integers):
 * what more it needs comes in a structure. Frames are ULONG and every
 * product is bounded where it is made (the host test's ULONG is 32 bits,
 * as on the Amiga).
 *
 * Pure: host-tested (test/host/waveform/test_waveplan.c).
 */

#include <exec/types.h>
#include <gadgets/waveform.h>

/* ---- the view ---- */

struct WaveView {
	ULONG wv_Frames;		/* the source's length */
	ULONG wv_Top;			/* the first frame in view */
	ULONG wv_Zoom;			/* frames a column, 1 or more */
	ULONG wv_ZoomMin;		/* 1 or more */
	ULONG wv_ZoomMax;		/* 0: the whole source in view */
	UWORD wv_Columns;		/* the view's width in pixels, 1 or more */
	UWORD wv_Pad;
};

/* The zoom that shows the whole source (1 at least) */
ULONG WaveView_ZoomAll(const struct WaveView *v);
/* Zoom and top into their bounds: zoom within ZoomMin..ZoomMax (or all),
   the view no further than the end allows */
void WaveView_Clamp(struct WaveView *v);
/* The whole source */
void WaveView_Fit(struct WaveView *v);
/* Frames in view: columns x zoom (no more than the source plus a column) */
ULONG WaveView_Visible(const struct WaveView *v);
/* The frame at column x (gadget-relative; outside the view too, within a
   view's width either side), within 0..frames */
ULONG WaveView_FrameAt(const struct WaveView *v, LONG x);
/* The column frame is in; -1: not in view */
LONG WaveView_ColumnOf(const struct WaveView *v, ULONG frame);
/* Zoom in (steps > 0) or out by halvings, keeping centre in its column
   (the view's middle when centre is not in view or WAVEFORM_NONE) */
void WaveView_Zoom(struct WaveView *v, LONG steps, ULONG centre);
/* Show a range with its margin */
void WaveView_Show(struct WaveView *v, const struct WaveRange *r);
/* The view after the play head at pos (WAVEFORM_FOLLOW_#?): TRUE when Top
   moved. FOLLOW_PAGE, the head run off the right edge into the next
   page: Top + Visible less a sixteenth of the columns, however far the
   head ran -- the page after this one is known in advance */
BOOL WaveView_Follow(struct WaveView *v, ULONG pos, UWORD mode);

/* ---- time ---- */

struct WaveClock {
	ULONG wc_Rate;			/* frames a second, 1 or more */
	ULONG wc_Origin;		/* the frame called 0:00 */
	UWORD wc_Format;		/* WAVEFORM_TIME_#? */
	UWORD wc_Decimals;		/* 0..3: tenths, hundredths, thousandths */
};
#define WAVEPLAN_TIMELEN	20	/* the longest text and its 0 */

/* frame as text into buf (WAVEPLAN_TIMELEN): "m:ss", "m:ss.mmm",
   "h:mm:ss" from an hour on, "-" before the origin; its length */
UWORD WavePlan_Time(const struct WaveClock *c, ULONG frame, char *buf);

/* ---- the ruler ---- */

struct WaveRuler {
	ULONG wru_Rate;			/* in: frames a second */
	UWORD wru_CharWidth;		/* in: pixels a character of its font */
	UWORD wru_Gap;			/* in: pixels between two labels */
	ULONG wru_Major;		/* out: frames between labelled ticks */
	ULONG wru_Minor;		/* out: frames between small ticks */
	ULONG wru_MajorMs;		/* out: the same in ms */
	UWORD wru_Decimals;		/* out: the labels' */
	UWORD wru_Div;			/* out: small ticks a step (wru_Minor apart) */
};
/* The steps that fit the view: the shortest whose labels do not touch;
   FALSE when none does (a view too narrow) */
BOOL WavePlan_Ruler(const struct WaveView *v, struct WaveRuler *r);

/* A time typed by the user -- "3.598", "0:03.598", "3:36.8", "1:02:03",
   "-0:01" before the origin, a frame number with WAVEFORM_TIME_FRAMES --
   as a frame (the origin added): WAVEFORM_NONE when it is no time, or
   before 0 */
ULONG WavePlan_ParseTime(const struct WaveClock *c, CONST_STRPTR s);

/* ms of time as frames at rate, and back (without the products that wrap) */
ULONG WavePlan_MsToFrames(ULONG ms, ULONG rate);
ULONG WavePlan_FramesToMs(ULONG frames, ULONG rate);

/* ---- what is under the pointer ---- */

#define WAVEPART_NONE		0
#define WAVEPART_WAVE		1
#define WAVEPART_RULER		2
#define WAVEPART_TRIMIN		3
#define WAVEPART_TRIMOUT	4
#define WAVEPART_SELSTART	5
#define WAVEPART_SELEND		6
#define WAVEPART_MARKER		7	/* the index in bits 8 up */
#define WAVEPART_BOX		8
#define WAVEPART_BOXOUT		9	/* the overview outside its box */
#define WAVEPART_KIND(p)	((p) & 0xFF)
#define WAVEPART_INDEX(p)	((p) >> 8)

struct WaveHit {
	LONG wh_In, wh_Out;		/* the handles' columns; -1: none to grab */
	LONG wh_SelStart, wh_SelEnd;	/* the selection's edges; -1: none or out of view */
	const LONG *wh_Markers;		/* the draggable markers' columns, -1 not */
	UWORD wh_MarkerCount;
	UWORD wh_Grab;			/* pixels either side that still grab */
	/* the handles' flags grab as their lines do: columns X0..X1 on the
	   lines FlagTop..FlagBottom - 1 (equal: no flags) */
	LONG wh_InFlagX0, wh_InFlagX1, wh_OutFlagX0, wh_OutFlagX1;
	WORD wh_FlagTop, wh_FlagBottom;
	WORD wh_RulerTop;		/* the ruler's lines, top..bottom - 1 */
	WORD wh_RulerBottom;		/* (equal: no ruler) */
	LONG wh_BoxLeft, wh_BoxRight;	/* the overview's box; -1: no box */
	UWORD wh_Width, wh_Height;	/* the gadget's: outside it, NONE */
};
/* The part at x, y (gadget-relative): the handles first, then the
   markers, the selection's edges, the box, the ruler, the sound */
ULONG WavePlan_Part(const struct WaveHit *h, LONG x, LONG y);

/* ---- snapping ---- */

struct WaveSnap {
	UWORD wsn_Flags;		/* WAVEFORM_SNAP_STEP, _MARKERS */
	UWORD wsn_TargetCount;
	ULONG wsn_Step;			/* frames */
	const ULONG *wsn_Targets;	/* frames to snap to (handles, markers, edges) */
	ULONG wsn_Distance;		/* frames within which a target pulls */
};
ULONG WavePlan_Snap(const struct WaveSnap *s, ULONG frame);

struct WaveZero {
	ULONG wz_Want;			/* in: the frame to start from */
	ULONG wz_Within;		/* in: frames either side to look */
	ULONG wz_Found;			/* out: the zero crossing nearest */
};
/* The nearest frame where the channels' sum changes sign (or is 0), within
   the samples given; FALSE: none, wz_Found = wz_Want */
BOOL WavePlan_ZeroCrossing(const struct WaveSamples *s, struct WaveZero *z);

/* The columns of a WavePeakMsg from samples (zoomed in deeper than the
   source holds): those s covers, from the first on -- the last one also
   when s ends inside it, which at the sound's end is the column; short
   of it, the caller leaves that column out (it knows the sound's end) */
ULONG WavePlan_FromSamples(const struct WaveSamples *s, struct WavePeakMsg *q);

/* ---- a lane's scale ---- */

struct WaveScale {
	UWORD wsc_Half;			/* pixels from the centre line to the edge */
	UWORD wsc_Gain;			/* per cent, 100 full scale at the edge */
	UWORD wsc_Mode;			/* WAVEFORM_SCALE_#? */
	UWORD wsc_DBRange;		/* dB from the edge to the centre (DB) */
};
/* A peak byte (-128..127) as pixels from the centre line, up positive:
   -wsc_Half .. wsc_Half */
WORD WavePlan_Scale(const struct WaveScale *s, LONG v);

/* ---- the level a fade's line shows ---- */

/* A fade's curve (WAVELEVEL_#? shape) rising from 0 at u 0 to
   WAVELEVEL_UNITY at WAVELEVEL_U_MAX: a fade in reads it forwards, a fade
   out from its end. The program's own arithmetic, nearly (its S-curve has
   finer steps): a line, not a sample, is drawn from it */
UWORD WavePlan_Curve(UWORD shape, UWORD u);

/* The levels walked frame by frame, the frames asked in rising order
   (lw_Next 0 to start again): the program's levels in frame order, the
   fades at In and Out */
struct WaveLevelWalk {
	const struct WaveLevel *lw_Level;
	UWORD lw_Count;
	UWORD lw_Next;			/* the first level not ended before the last frame asked */
	ULONG lw_In, lw_FadeIn;		/* In and its fade's frames, 0 none */
	ULONG lw_Out, lw_FadeOut;	/* Out and its fade's frames, 0 none */
	UWORD lw_Shape;			/* the end fades' shape */
	UWORD lw_Gain;			/* out: the gain of the program's level at the
					   frame asked, WAVELEVEL_UNITY outside them */
};
#define WAVEPLAN_NOLINE	0xFFFF
/* The level frame f plays at, as a lane's line shows it (WAVELEVEL_UNITY
   at most), or WAVEPLAN_NOLINE where no fade reaches; lw_Gain set */
UWORD WavePlan_LevelAt(struct WaveLevelWalk *w, ULONG f);

/* ---- the repaint ---- */

/* FNV-1a over n bytes, from seed (WAVEPLAN_SIG_SEED to start) */
#define WAVEPLAN_SIG_SEED	0x811C9DC5UL
ULONG WavePlan_Sig(ULONG seed, const void *p, ULONG n);

#endif
