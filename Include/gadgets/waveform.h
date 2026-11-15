#ifndef GADGETS_WAVEFORM_H
#define GADGETS_WAVEFORM_H

/*
 * waveform.h -- waveform.gadget: a sound drawn in a ReAction window, and
 * the mouse's work on it -- zoom and scroll, a cursor, a selection, two
 * trim handles, markers, shaded regions, a play head, a time ruler, an
 * overview mode. Every look and behaviour is a tag (Documentation/
 * Concept-WaveSmith-2026-10.md, section 4; Autodocs/waveform_gc.doc).
 *
 * The class owns no samples and reads no files: its input methods run on
 * input.device's task, where a file or a device is not to be touched. The
 * picture comes from a WaveSource that answers from RAM at once; what the
 * class lacks (samples, zoomed in deeper than the source holds) it asks
 * for (WAVEFORM_Want, the signal), and the program, on its own process,
 * reads and hands in (WAVEFORM_Samples).
 *
 * A frame is one sample of every channel, a column one pixel of the view.
 * WAVEFORM_NONE is "none" for every frame tag. I S G: where a tag works
 * (OM_NEW, OM_SET, OM_GET). A pointer tag is either borrowed (kept until
 * set again, never freed by the class) or copied, as each says.
 *
 * While the class is linked into its programs: Waveform_MakeClass() and
 * Waveform_FreeClass(); as a disk class later, the same tags.
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <intuition/classes.h>
#include <intuition/intuition.h>

#define WAVEFORM_TAGBASE	(TAG_USER + 0x57460000UL)	/* 'WF' */
#define WAVEFORM_NONE		0xFFFFFFFFUL

/* ---- the source: where the picture comes from ---- */

/*
 * The hook fills wpm_Count columns of wpm_Step frames each from wpm_First:
 * for every column, each channel's minimum and maximum (-128..127, the
 * sample's top byte), channel after channel, and returns how many columns
 * it filled, from the first on, writing none beyond them (the class keeps
 * what follows them). It may be called on input.device's task:
 * from RAM only, never a file, never a wait. The columns it leaves are
 * drawn as Missing: a picture still being made -- set WAVEFORM_Source
 * again as it grows. A view finer than ws_Finest frames a column is drawn
 * from samples: the class asks for them (WAVEFORM_Want) and meanwhile
 * stretches the picture's ws_Finest steps over those columns, so a
 * program that cannot hand them in (too many, no memory) still shows a
 * coarse picture.
 *   hook: A0 the hook, A2 the class's object, A1 the WavePeakMsg
 */
struct WavePeakMsg {
	ULONG wpm_First;
	ULONG wpm_Step;			/* frames a column, 1 or more */
	ULONG wpm_Count;		/* columns */
	BYTE *wpm_Out;			/* wpm_Count x channels x 2 */
};

struct WaveSource {
	struct Hook ws_Peaks;
	ULONG ws_Frames;		/* the sound's length */
	ULONG ws_Rate;			/* frames a second */
	UWORD ws_Channels;		/* 1 or 2: more are mixed by the source */
	UWORD ws_Flags;			/* WAVESOURCE_#? */
	ULONG ws_Finest;		/* the finest step it answers, frames;
					   finer views from samples */
};
#define WAVESOURCE_LEVELONLY	0x0001	/* levels without signs (a .prl map):
					   drawn as an envelope or bars */

/* Samples the program hands in (WAVEFORM_Samples), interleaved, the
   machine's order; borrowed until replaced */
struct WaveSamples {
	ULONG wsm_First;		/* the first frame they hold */
	ULONG wsm_Frames;
	UWORD wsm_Channels;
	UWORD wsm_Pad;
	const WORD *wsm_Data;
};

/* What the class would draw from samples (WAVEFORM_Want): at most the
   view's frames, only while it is finer than ws_Finest */
struct WaveWant {
	ULONG ww_First;
	ULONG ww_Frames;
};

/* Where the play head's line goes (WAVEFORM_HeadBox), the gadget's own
   copy: the lanes in the window's coordinates and the view on them -- a
   frame's column is (frame - hb_First) / hb_Zoom, in view below hb_Columns */
struct WaveHeadBox {
	WORD hb_Left, hb_Top;		/* the first column, the lanes' top row */
	UWORD hb_Columns, hb_Rows;
	ULONG hb_First, hb_Zoom;
};

/* A range to show (WAVEFORM_ShowRange) */
struct WaveRange {
	ULONG wr_First;
	ULONG wr_End;			/* the frame after the last */
	UWORD wr_Margin;		/* per cent of the range on each side */
	UWORD wr_Pad;
};

/* ---- markers and regions: the program's arrays, borrowed ---- */

struct WaveMarker {
	ULONG wm_Frame;
	UWORD wm_Kind;			/* WAVEMARKER_CUE, _LABEL, _SPLIT */
	UWORD wm_Flags;			/* WAVEMARKER_FIXED ... */
	LONG wm_Pen;			/* -1: the Marker colour */
	CONST_STRPTR wm_Label;		/* NULL: none; borrowed */
};
#define WAVEMARKER_CUE		0
#define WAVEMARKER_LABEL	1
#define WAVEMARKER_SPLIT	2
#define WAVEMARKER_FIXED	0x0001	/* never dragged */
#define WAVEMARKER_HIDDEN	0x0002
#define WAVEMARKER_SELECTED	0x0004
#define WAVEMARKER_NOLABEL	0x0008

struct WaveRegion {
	ULONG wrg_First;
	ULONG wrg_End;
	UWORD wrg_Kind;			/* WAVEREGION_SILENCE, _CLICK, _GONE, _USER */
	UWORD wrg_Style;		/* WAVEREGION_TINT, _HATCH, _BAR, _TICK */
	LONG wrg_Pen;			/* -1: the kind's colour */
};
#define WAVEREGION_SILENCE	0
#define WAVEREGION_CLICK	1
#define WAVEREGION_GONE		2
#define WAVEREGION_USER		3
#define WAVEREGION_TINT		0
#define WAVEREGION_HATCH	1
#define WAVEREGION_BAR		2
#define WAVEREGION_TICK		3

/* A stretch of the sound at a level of the program's (a gain, a fade, a
   piece of a fade a cut left), the levels in frame order, none over
   another. The picture is the program's to draw at its level; the class
   draws a fade's level as a line in each lane (its gain times its curve,
   as recorded at most), a bar along the lanes' foot under a gain, and a
   tag with wlv_Label on it where the level starts. A stretch of gain
   alone gets no line. */
struct WaveLevel {
	ULONG wlv_First;
	ULONG wlv_End;			/* the frame after the last */
	UWORD wlv_Gain;			/* 4.12: WAVELEVEL_UNITY as recorded */
	UWORD wlv_Fade;			/* WAVELEVEL_NONE, or _IN / _OUT | a shape */
	UWORD wlv_U0, wlv_U1;		/* the stretch of its fade's curve it holds:
					   0 the curve's start, 65535 its end */
	CONST_STRPTR wlv_Label;		/* its tag's words ("+3.0 dB"); NULL: no tag; borrowed */
};
#define WAVELEVEL_UNITY		4096
#define WAVELEVEL_NONE		0x00
#define WAVELEVEL_IN		0x10	/* the curve read from its start */
#define WAVELEVEL_OUT		0x20	/* ... from its end */
#define WAVELEVEL_DIR		0x30
#define WAVELEVEL_SHAPE		0x0F
#define WAVELEVEL_LINEAR	0	/* the amplitude in a straight line */
#define WAVELEVEL_EVEN		1	/* the amplitude squared */
#define WAVELEVEL_SCURVE	2	/* (1 - cos) / 2 */
#define WAVELEVEL_U_MAX		65535

/* ---- source ---- */
#define WAVEFORM_Source		(WAVEFORM_TAGBASE + 1)	/* (struct WaveSource *) IS, borrowed */
#define WAVEFORM_Frames		(WAVEFORM_TAGBASE + 2)	/* (ULONG) G */
#define WAVEFORM_Rate		(WAVEFORM_TAGBASE + 3)	/* (ULONG) G */
#define WAVEFORM_Channels	(WAVEFORM_TAGBASE + 4)	/* (UWORD) G */

/* ---- view ---- */
#define WAVEFORM_Top		(WAVEFORM_TAGBASE + 10)	/* (ULONG) ISG: the first frame in view */
#define WAVEFORM_Visible	(WAVEFORM_TAGBASE + 11)	/* (ULONG) G: frames in view */
#define WAVEFORM_Zoom		(WAVEFORM_TAGBASE + 12)	/* (ULONG) ISG: frames a column, 0 all */
#define WAVEFORM_ZoomMin	(WAVEFORM_TAGBASE + 13)	/* (ULONG) IS: 1 */
#define WAVEFORM_ZoomMax	(WAVEFORM_TAGBASE + 14)	/* (ULONG) IS: 0, the whole source */
#define WAVEFORM_ZoomStep	(WAVEFORM_TAGBASE + 15)	/* (LONG) S: verb, +n in, -n out (halvings) */
#define WAVEFORM_ZoomCenter	(WAVEFORM_TAGBASE + 16)	/* (ULONG) S: the frame a ZoomStep keeps in its column */
#define WAVEFORM_ShowRange	(WAVEFORM_TAGBASE + 17)	/* (struct WaveRange *) S: verb */
#define WAVEFORM_ShowAll	(WAVEFORM_TAGBASE + 18)	/* (BOOL) S: verb */
#define WAVEFORM_Follow		(WAVEFORM_TAGBASE + 19)	/* (ULONG) IS: WAVEFORM_FOLLOW_#? */
#define WAVEFORM_AutoScroll	(WAVEFORM_TAGBASE + 20)	/* (BOOL) IS: TRUE */
#define WAVEFORM_FOLLOW_NONE	0
#define WAVEFORM_FOLLOW_PAGE	1
#define WAVEFORM_FOLLOW_CENTER	2

/* ---- lanes and level ---- */
#define WAVEFORM_Lanes		(WAVEFORM_TAGBASE + 30)	/* (ULONG) IS: WAVEFORM_LANES_#? */
#define WAVEFORM_LaneGap	(WAVEFORM_TAGBASE + 31)	/* (UWORD) IS: 1 */
#define WAVEFORM_LaneLabels	(WAVEFORM_TAGBASE + 32)	/* (CONST_STRPTR *) IS, borrowed */
#define WAVEFORM_Scale		(WAVEFORM_TAGBASE + 33)	/* (ULONG) IS: WAVEFORM_SCALE_#? */
#define WAVEFORM_DBRange	(WAVEFORM_TAGBASE + 34)	/* (UWORD) IS: 60 */
#define WAVEFORM_Gain		(WAVEFORM_TAGBASE + 35)	/* (UWORD) IS: per cent, 100 */
#define WAVEFORM_ClipMark	(WAVEFORM_TAGBASE + 36)	/* (BOOL) IS: TRUE */
#define WAVEFORM_LANES_AUTO	0
#define WAVEFORM_LANES_MIX	1
#define WAVEFORM_LANES_SPLIT	2
#define WAVEFORM_LANES_LEFT	3
#define WAVEFORM_LANES_RIGHT	4
#define WAVEFORM_SCALE_LINEAR	0
#define WAVEFORM_SCALE_DB	1

/* ---- draw style and ruler ---- */
#define WAVEFORM_WaveStyle	(WAVEFORM_TAGBASE + 40)	/* (ULONG) IS: WAVEFORM_WAVE_#? */
#define WAVEFORM_CenterLine	(WAVEFORM_TAGBASE + 41)	/* (BOOL) IS: TRUE */
#define WAVEFORM_Grid		(WAVEFORM_TAGBASE + 42)	/* (ULONG) IS: WAVEFORM_GRID_#? */
#define WAVEFORM_BevelStyle	(WAVEFORM_TAGBASE + 43)	/* (ULONG) IS: BVS_FIELD (images/bevel.h) */
#define WAVEFORM_Patterns	(WAVEFORM_TAGBASE + 44)	/* (ULONG) IS: WAVEFORM_PATTERNS_#? */
#define WAVEFORM_Font		(WAVEFORM_TAGBASE + 45)	/* (struct TextFont *) IS, open, borrowed */
#define WAVEFORM_Ruler		(WAVEFORM_TAGBASE + 46)	/* (ULONG) IS: WAVEFORM_RULER_#? */
#define WAVEFORM_TimeFormat	(WAVEFORM_TAGBASE + 47)	/* (ULONG) IS: WAVEFORM_TIME_#? */
#define WAVEFORM_TimeOrigin	(WAVEFORM_TAGBASE + 48)	/* (ULONG) IS: the frame called 0:00 */
#define WAVEFORM_WAVE_FILLED	0
#define WAVEFORM_WAVE_OUTLINE	1
#define WAVEFORM_WAVE_BARS	2
#define WAVEFORM_WAVE_DOTS	3
#define WAVEFORM_GRID_NONE	0
#define WAVEFORM_GRID_TIME	1
#define WAVEFORM_GRID_LEVEL	2
#define WAVEFORM_GRID_BOTH	3
#define WAVEFORM_PATTERNS_AUTO	0
#define WAVEFORM_PATTERNS_ALWAYS 1
#define WAVEFORM_PATTERNS_NEVER	2
#define WAVEFORM_RULER_NONE	0
#define WAVEFORM_RULER_TOP	1
#define WAVEFORM_RULER_BOTTOM	2
#define WAVEFORM_TIME_AUTO	0
#define WAVEFORM_TIME_HMS	1
#define WAVEFORM_TIME_FRAMES	2
#define WAVEFORM_TIME_SECONDS	3

/* ---- cursor and selection ---- */
#define WAVEFORM_Cursor		(WAVEFORM_TAGBASE + 60)	/* (ULONG) ISG */
#define WAVEFORM_CursorStyle	(WAVEFORM_TAGBASE + 61)	/* (ULONG) IS: WAVEFORM_CURSOR_#? */
#define WAVEFORM_ClickAction	(WAVEFORM_TAGBASE + 62)	/* (ULONG) IS: WAVEFORM_CLICK_#? */
#define WAVEFORM_SelStart	(WAVEFORM_TAGBASE + 63)	/* (ULONG) ISG */
#define WAVEFORM_SelEnd		(WAVEFORM_TAGBASE + 64)	/* (ULONG) ISG: the frame after the last */
#define WAVEFORM_SelectMode	(WAVEFORM_TAGBASE + 65)	/* (ULONG) IS: WAVEFORM_SELECT_NONE, _RANGE */
#define WAVEFORM_SelectStyle	(WAVEFORM_TAGBASE + 66)	/* (ULONG) IS: WAVEFORM_SELSTYLE_#? */
#define WAVEFORM_ExtendQualifier (WAVEFORM_TAGBASE + 67) /* (UWORD) IS: either Shift */
#define WAVEFORM_DoubleClick	(WAVEFORM_TAGBASE + 68)	/* (ULONG) IS: WAVEFORM_DOUBLE_#? */
#define WAVEFORM_DragThreshold	(WAVEFORM_TAGBASE + 69)	/* (UWORD) IS: 3 pixels */
#define WAVEFORM_DragEdges	(WAVEFORM_TAGBASE + 70)	/* (BOOL) IS: TRUE */
#define WAVEFORM_CURSOR_LINE	0
#define WAVEFORM_CURSOR_DASHED	1
#define WAVEFORM_CLICK_CURSOR	0
#define WAVEFORM_CLICK_SEEK	1
#define WAVEFORM_CLICK_NONE	2
#define WAVEFORM_SELECT_NONE	0
#define WAVEFORM_SELECT_RANGE	1
#define WAVEFORM_SELSTYLE_FILL	0
#define WAVEFORM_SELSTYLE_INVERT 1
#define WAVEFORM_SELSTYLE_FRAME	2
#define WAVEFORM_DOUBLE_NONE	0
#define WAVEFORM_DOUBLE_REGION	1
#define WAVEFORM_DOUBLE_ALL	2

/* ---- trim handles ---- */
#define WAVEFORM_TrimIn		(WAVEFORM_TAGBASE + 80)	/* (ULONG) ISG: the kept part's first frame */
#define WAVEFORM_TrimOut	(WAVEFORM_TAGBASE + 81)	/* (ULONG) ISG: the frame after its last */
#define WAVEFORM_TrimHandles	(WAVEFORM_TAGBASE + 82)	/* (ULONG) IS: WAVEFORM_HANDLES_#? */
#define WAVEFORM_HandleStyle	(WAVEFORM_TAGBASE + 83)	/* (ULONG) IS: WAVEFORM_HANDLE_#? */
#define WAVEFORM_HandleSize	(WAVEFORM_TAGBASE + 84)	/* (UWORD) IS: lines, 0 the font's */
#define WAVEFORM_HandleLabels	(WAVEFORM_TAGBASE + 85)	/* (CONST_STRPTR *) IS: two, borrowed */
#define WAVEFORM_HandleGrab	(WAVEFORM_TAGBASE + 86)	/* (UWORD) IS: 3 pixels */
#define WAVEFORM_TrimShade	(WAVEFORM_TAGBASE + 87)	/* (ULONG) IS: WAVEFORM_SHADE_#? */
#define WAVEFORM_MinLength	(WAVEFORM_TAGBASE + 88)	/* (ULONG) IS: frames, 1 */
#define WAVEFORM_HANDLES_NONE	0
#define WAVEFORM_HANDLES_SHOW	1
#define WAVEFORM_HANDLES_DRAG	2
#define WAVEFORM_HANDLE_FLAG	0
#define WAVEFORM_HANDLE_BRACKET	1
#define WAVEFORM_HANDLE_TRIANGLE 2
#define WAVEFORM_HANDLE_LINE	3
#define WAVEFORM_SHADE_NONE	0
#define WAVEFORM_SHADE_DARKEN	1
#define WAVEFORM_SHADE_HATCH	2
#define WAVEFORM_SHADE_FLAT	3

/* ---- markers, regions, snapping ---- */
#define WAVEFORM_Markers	(WAVEFORM_TAGBASE + 100) /* (struct WaveMarker *) IS, borrowed */
#define WAVEFORM_MarkerCount	(WAVEFORM_TAGBASE + 101) /* (UWORD) IS */
#define WAVEFORM_DragMarkers	(WAVEFORM_TAGBASE + 102) /* (BOOL) IS: FALSE */
#define WAVEFORM_MarkerStyle	(WAVEFORM_TAGBASE + 103) /* (ULONG) IS: WAVEFORM_HANDLE_#?, TRIANGLE */
#define WAVEFORM_Regions	(WAVEFORM_TAGBASE + 104) /* (struct WaveRegion *) IS, borrowed */
#define WAVEFORM_RegionCount	(WAVEFORM_TAGBASE + 105) /* (UWORD) IS */
#define WAVEFORM_Snap		(WAVEFORM_TAGBASE + 106) /* (ULONG) IS: WAVEFORM_SNAP_#? flags */
#define WAVEFORM_SnapStep	(WAVEFORM_TAGBASE + 107) /* (ULONG) IS: frames */
#define WAVEFORM_SnapDistance	(WAVEFORM_TAGBASE + 108) /* (UWORD) IS: 4 pixels */
#define WAVEFORM_SnapHook	(WAVEFORM_TAGBASE + 109) /* (struct Hook *) IS: input.device's task! */
/* the SnapHook: A1 a ULONG * to the frame let go at, returns the frame to
   use; it runs on input.device's task -- RAM only, no DOS, no waiting */
#define WAVEFORM_SNAP_STEP	0x0001
#define WAVEFORM_SNAP_MARKERS	0x0002
#define WAVEFORM_SNAP_HOOK	0x0004

/* ---- the level: fades and gains (struct WaveLevel), the end fades ---- */
#define WAVEFORM_Levels		(WAVEFORM_TAGBASE + 110) /* (struct WaveLevel *) IS, borrowed */
#define WAVEFORM_LevelCount	(WAVEFORM_TAGBASE + 111) /* (UWORD) IS */
/* the fades at In and Out: they belong to the handles -- a handle moved,
   its fade goes with it, its length kept (never past the other handle) */
#define WAVEFORM_FadeIn		(WAVEFORM_TAGBASE + 112) /* (ULONG) ISG: frames from In, 0 none */
#define WAVEFORM_FadeOut	(WAVEFORM_TAGBASE + 113) /* (ULONG) ISG: frames up to Out, 0 none */
#define WAVEFORM_FadeShape	(WAVEFORM_TAGBASE + 114) /* (UWORD) IS: WAVELEVEL_#? shape, EVEN */
#define WAVEFORM_ShowLevels	(WAVEFORM_TAGBASE + 115) /* (BOOL) IS: TRUE, the lines, bars and tags drawn */

/* ---- play head, overview ---- */
#define WAVEFORM_Position	(WAVEFORM_TAGBASE + 120) /* (ULONG) S: or Waveform_ShowPosition() */
#define WAVEFORM_PositionStyle	(WAVEFORM_TAGBASE + 121) /* (ULONG) IS: WAVEFORM_POSITION_#? */
#define WAVEFORM_Overview	(WAVEFORM_TAGBASE + 122) /* (BOOL) I: no ruler, handles shown
							    as lines (TrimHandles DRAG: lines to
							    drag; a HandleStyle after it: knobs) */
#define WAVEFORM_BoxTop		(WAVEFORM_TAGBASE + 123) /* (ULONG) ISG */
#define WAVEFORM_BoxVisible	(WAVEFORM_TAGBASE + 124) /* (ULONG) ISG */
#define WAVEFORM_BoxStyle	(WAVEFORM_TAGBASE + 125) /* (ULONG) IS: WAVEFORM_BOX_#? */
#define WAVEFORM_POSITION_LINE	0
#define WAVEFORM_POSITION_TRIANGLE 1
#define WAVEFORM_POSITION_BLOCK	2
#define WAVEFORM_BOX_FRAME	0
#define WAVEFORM_BOX_DIM	1

/* ---- input ---- */
#define WAVEFORM_ReadOnly	(WAVEFORM_TAGBASE + 140) /* (BOOL) IS: FALSE */
#define WAVEFORM_Keys		(WAVEFORM_TAGBASE + 141) /* (BOOL) IS: FALSE */
#define WAVEFORM_Help		(WAVEFORM_TAGBASE + 142) /* (BOOL) IS: TRUE, a bubble for what is under
							    the pointer; FALSE: the program's hint */
#define WAVEFORM_Pointers	(WAVEFORM_TAGBASE + 143) /* (BOOL) IS: TRUE */
#define WAVEFORM_HandlePointer	(WAVEFORM_TAGBASE + 144) /* (ULONG) IS: POINTERTYPE_EASTWESTRESIZE */
#define WAVEFORM_EdgePointer	(WAVEFORM_TAGBASE + 145) /* (ULONG) IS: POINTERTYPE_EASTWESTRESIZE */
#define WAVEFORM_WavePointer	(WAVEFORM_TAGBASE + 146) /* (ULONG) IS: POINTERTYPE_TEXT */
#define WAVEFORM_BoxPointer	(WAVEFORM_TAGBASE + 147) /* (ULONG) IS: POINTERTYPE_HAND */
#define WAVEFORM_CustomPointers	(WAVEFORM_TAGBASE + 148) /* (Object **) IS: handle, edge, wave, box; borrowed */
#define WAVEFORM_HelpTexts	(WAVEFORM_TAGBASE + 149) /* (CONST_STRPTR *) IS: WAVEFORM_HELP_COUNT formats,
							    NULL ones the class's; borrowed */
/* the bubbles' formats (WAVEFORM_HelpTexts), each two %s at most */
#define WAVEFORM_HELP_SOUND	0	/* the time under the pointer */
#define WAVEFORM_HELP_HANDLE	1	/* a handle's label, its time */
#define WAVEFORM_HELP_SELECTION	2	/* the selection's start, its end */
#define WAVEFORM_HELP_MARKER	3	/* a marker's label, its time */
#define WAVEFORM_HELP_BOX	4	/* the overview's box: the view's start, end */
#define WAVEFORM_HELP_COUNT	5
#define WAVEFORM_HELPLEN	80	/* a bubble's text and its 0 at most */

/* ---- talking back ---- */
#define WAVEFORM_SignalTask	(WAVEFORM_TAGBASE + 160) /* (struct Task *) I */
#define WAVEFORM_SignalMask	(WAVEFORM_TAGBASE + 161) /* (ULONG) I: a MASK, 1UL << bit */
#define WAVEFORM_Want		(WAVEFORM_TAGBASE + 162) /* (struct WaveWant *) G */
#define WAVEFORM_Samples	(WAVEFORM_TAGBASE + 163) /* (struct WaveSamples *) SG: verb, borrowed;
							   G: the ones it draws from, NULL none */
#define WAVEFORM_PointerFrame	(WAVEFORM_TAGBASE + 164) /* (ULONG) G */
#define WAVEFORM_ActiveMarker	(WAVEFORM_TAGBASE + 165) /* (LONG) G: -1 none */
#define WAVEFORM_ActiveFrame	(WAVEFORM_TAGBASE + 166) /* (ULONG) G */
#define WAVEFORM_SamplesMore	(WAVEFORM_TAGBASE + 167) /* (BOOL) S, beside WAVEFORM_Samples: the
							   same sound as the last ones (the frames
							   both hold are equal), more of it -- the
							   columns drawn from those stay as they are */
#define WAVEFORM_HeadOff	(WAVEFORM_TAGBASE + 168) /* (BOOL) S: the play head not drawn -- the
							   application shows it otherwise (a sprite);
							   followed and paged as before */
#define WAVEFORM_HeadBox	(WAVEFORM_TAGBASE + 169) /* (struct WaveHeadBox *) G */
#define WAVEFORM_AheadColumns	(WAVEFORM_TAGBASE + 170) /* (UWORD) IS: the next page's columns
							   painted a Waveform_ShowPosition(), 8 at
							   least; 160. 0: none there -- the program
							   paints ahead itself (Waveform_PaintAhead())
							   in pieces of the time it has */
#define WAVEFORM_AheadLeft	(WAVEFORM_TAGBASE + 171) /* (ULONG) G: the next page's columns not
							   painted yet (all before it began; 0 once
							   painted, or with none to paint) */
#define WAVEFORM_PaintColumns	(WAVEFORM_TAGBASE + 172) /* (UWORD) IS: 0; else a paint of the
							   lanes (not a full one: GM_RENDER's redraw)
							   paints that many columns at most, from the
							   left -- a new view's others shown as not
							   there yet -- and Waveform_PaintAhead()
							   goes on with the rest: a slow machine's
							   page turn in pieces. Setting paints nothing */
#define WAVEFORM_PaintLeft	(WAVEFORM_TAGBASE + 173) /* (ULONG) G: the view's columns a paint
							   over its budget left (0: none) */
#define WAVEFORM_FillNew	(WAVEFORM_TAGBASE + 174) /* (BOOL) IS: a new view not turned to (a
							   zoom, a scroll, new samples, a change of
							   the sound) paints nothing at once but the
							   ruler; Waveform_PaintAhead() fills it in
							   from the left, the old picture kept until
							   each piece replaces it (WAVEFORM_PaintLeft
							   counts them) -- for a program that paints
							   the pieces between its input. FALSE: such
							   a view painted whole. Setting paints
							   nothing */
#define WAVEFORM_SamplesNone	(WAVEFORM_TAGBASE + 175) /* (BOOL) S: the samples wanted
							   (WAVEFORM_Want) will not come: a view
							   filling in shows those columns stretched
							   from the source, as a whole paint does;
							   cleared by a new view and by samples */

/* Waveform_Changes(): what changed since the last call */
#define WAVEFORM_CHANGED_CURSOR		0x0001
#define WAVEFORM_CHANGED_SELECTION	0x0002
#define WAVEFORM_CHANGED_TRIM		0x0004
#define WAVEFORM_CHANGED_MARKER		0x0008
#define WAVEFORM_CHANGED_VIEW		0x0010
#define WAVEFORM_CHANGED_BOX		0x0020
#define WAVEFORM_CHANGED_WANT		0x0040
#define WAVEFORM_CHANGED_SEEK		0x0080

/* GADGETUP: the IntuiMessage's Code */
#define WAVEFORM_UP_CURSOR	1
#define WAVEFORM_UP_SELECTION	2
#define WAVEFORM_UP_TRIMIN	3
#define WAVEFORM_UP_TRIMOUT	4
#define WAVEFORM_UP_MARKER	5	/* which: WAVEFORM_ActiveMarker */
#define WAVEFORM_UP_SEEK	6	/* where: WAVEFORM_PointerFrame */
#define WAVEFORM_UP_BOX		7

/* ---- colours: a Pen (LONG, -1 the default) and an RGB ($00RRGGBB,
   obtained by the class) for each element; WAVEFORM_Colors all at once ---- */
#define WAVEFORM_COLOR_BACKGROUND	0
#define WAVEFORM_COLOR_WAVE		1
#define WAVEFORM_COLOR_WAVESELECTED	2
#define WAVEFORM_COLOR_SELECTION	3
#define WAVEFORM_COLOR_TRIMMED		4
#define WAVEFORM_COLOR_TRIMMEDWAVE	5
#define WAVEFORM_COLOR_CENTERLINE	6
#define WAVEFORM_COLOR_GRID		7
#define WAVEFORM_COLOR_CLIP		8
#define WAVEFORM_COLOR_INHANDLE		9
#define WAVEFORM_COLOR_OUTHANDLE	10
#define WAVEFORM_COLOR_HANDLETEXT	11
#define WAVEFORM_COLOR_MARKER		12
#define WAVEFORM_COLOR_POSITION		13
#define WAVEFORM_COLOR_CURSOR		14
#define WAVEFORM_COLOR_RULERBACK	15
#define WAVEFORM_COLOR_RULERTEXT	16
#define WAVEFORM_COLOR_SILENCE		17
#define WAVEFORM_COLOR_CLICK		18
#define WAVEFORM_COLOR_GONE		19
#define WAVEFORM_COLOR_BOX		20
#define WAVEFORM_COLOR_MISSING		21
#define WAVEFORM_COLOR_LEVEL		22	/* a fade's line, a gain's tag */
#define WAVEFORM_COLOR_COUNT		23
#define WAVEFORM_COLOR_DEFAULT		0xFFFFFFFFUL	/* in WaveColors: keep */

#define WAVEFORM_PENTAG(c)	(WAVEFORM_TAGBASE + 200 + 2 * (c))
#define WAVEFORM_RGBTAG(c)	(WAVEFORM_TAGBASE + 201 + 2 * (c))
#define WAVEFORM_BackgroundPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_BACKGROUND)
#define WAVEFORM_BackgroundRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_BACKGROUND)
#define WAVEFORM_WavePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_WAVE)
#define WAVEFORM_WaveRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_WAVE)
#define WAVEFORM_WaveSelectedPen	WAVEFORM_PENTAG(WAVEFORM_COLOR_WAVESELECTED)
#define WAVEFORM_WaveSelectedRGB	WAVEFORM_RGBTAG(WAVEFORM_COLOR_WAVESELECTED)
#define WAVEFORM_SelectionPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_SELECTION)
#define WAVEFORM_SelectionRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_SELECTION)
#define WAVEFORM_TrimmedPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_TRIMMED)
#define WAVEFORM_TrimmedRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_TRIMMED)
#define WAVEFORM_TrimmedWavePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_TRIMMEDWAVE)
#define WAVEFORM_TrimmedWaveRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_TRIMMEDWAVE)
#define WAVEFORM_CenterLinePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_CENTERLINE)
#define WAVEFORM_CenterLineRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_CENTERLINE)
#define WAVEFORM_GridPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_GRID)
#define WAVEFORM_GridRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_GRID)
#define WAVEFORM_ClipPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_CLIP)
#define WAVEFORM_ClipRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_CLIP)
#define WAVEFORM_InHandlePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_INHANDLE)
#define WAVEFORM_InHandleRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_INHANDLE)
#define WAVEFORM_OutHandlePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_OUTHANDLE)
#define WAVEFORM_OutHandleRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_OUTHANDLE)
#define WAVEFORM_HandleTextPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_HANDLETEXT)
#define WAVEFORM_HandleTextRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_HANDLETEXT)
#define WAVEFORM_MarkerPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_MARKER)
#define WAVEFORM_MarkerRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_MARKER)
#define WAVEFORM_PositionPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_POSITION)
#define WAVEFORM_PositionRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_POSITION)
#define WAVEFORM_CursorPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_CURSOR)
#define WAVEFORM_CursorRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_CURSOR)
#define WAVEFORM_RulerBackPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_RULERBACK)
#define WAVEFORM_RulerBackRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_RULERBACK)
#define WAVEFORM_RulerTextPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_RULERTEXT)
#define WAVEFORM_RulerTextRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_RULERTEXT)
#define WAVEFORM_SilencePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_SILENCE)
#define WAVEFORM_SilenceRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_SILENCE)
#define WAVEFORM_ClickPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_CLICK)
#define WAVEFORM_ClickRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_CLICK)
#define WAVEFORM_GonePen		WAVEFORM_PENTAG(WAVEFORM_COLOR_GONE)
#define WAVEFORM_GoneRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_GONE)
#define WAVEFORM_BoxPen			WAVEFORM_PENTAG(WAVEFORM_COLOR_BOX)
#define WAVEFORM_BoxRGB			WAVEFORM_RGBTAG(WAVEFORM_COLOR_BOX)
#define WAVEFORM_MissingPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_MISSING)
#define WAVEFORM_MissingRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_MISSING)
#define WAVEFORM_LevelPen		WAVEFORM_PENTAG(WAVEFORM_COLOR_LEVEL)
#define WAVEFORM_LevelRGB		WAVEFORM_RGBTAG(WAVEFORM_COLOR_LEVEL)

struct WaveColors {
	ULONG wc_RGB[WAVEFORM_COLOR_COUNT];	/* WAVEFORM_COLOR_DEFAULT: keep */
};
#define WAVEFORM_Colors		(WAVEFORM_TAGBASE + 190) /* (struct WaveColors *) IS, copied */
#define WAVEFORM_PenPrecision	(WAVEFORM_TAGBASE + 191) /* (ULONG) I: PRECISION_IMAGE */

/* ---- the functions ---- */

Class *Waveform_MakeClass(void);		/* NULL: no memory */
void Waveform_FreeClass(Class *cl);		/* after every object is disposed */

/* What changed since the last call (WAVEFORM_CHANGED_#?), cleared */
ULONG Waveform_Changes(Object *wave);

/* The one mapping between a gadget-relative column and a frame: the
   class's clicks, drags, handle grabs and help bubbles use it, and so
   does the program's wheel hook */
ULONG Waveform_FrameAt(Object *wave, LONG x);
LONG Waveform_ColumnOf(Object *wave, ULONG frame);	/* -1: not in view */

/* The play head, drawn by the calling task: the layer is tried, a busy one
   skips this frame; only the columns concerned are drawn. And a step on
   the next page, painted ahead while it plays (WAVEFORM_AheadColumns):
   the columns painted, 0 none */
ULONG Waveform_ShowPosition(Object *wave, struct Window *win, ULONG frame);

/* A step of painting, cols columns (8 at least): the view's columns a
   paint left (WAVEFORM_PaintLeft) into the window -- its layer tried, a
   busy one paints nothing -- else the next page's into its own buffer.
   The columns painted, 0 none (none left, none to paint, or the samples
   it needs not there yet: WAVEFORM_Samples) */
ULONG Waveform_PaintAhead(Object *wave, struct Window *win, ULONG cols);

/* The pen an element is drawn in now (WAVEFORM_COLOR_#?): the one the
   class obtained, given or fell back on -- for the program's own marks to
   match (WaveSmith's handle flags beside the sound); -1 before the gadget
   first laid out its pens */
LONG Waveform_PenOf(Object *wave, ULONG element);

/* The bubble's text, one buffer for every object of the class: for the
   window's HintInfo array ({ id, -1, text, 0 } per gadget -- window.class
   builds its hints only when it has one, and an older one reads the
   text from there); its address never changes */
CONST_STRPTR Waveform_HelpText(void);

/* ---- diagnostics: every paint logged, for a program's trace ----
   Off until Waveform_DiagOn(). Each paint of the lanes adds an event: who
   asked (Intuition's GM_RENDER, the program's OM_SET, the gadget's own
   input, the play head), from which task, onto which state of the
   window's layer. The events live in a ring of WAVEDIAG_EVENTS in memory
   (GM_RENDER may run on input.device's task: no file there); the program
   reads them out with Waveform_DiagRead() and writes them where it likes. */
#define WAVEDIAG_RENDER_REDRAW	1	/* GM_RENDER, GREDRAW_REDRAW */
#define WAVEDIAG_RENDER_UPDATE	2	/* GM_RENDER, GREDRAW_UPDATE or _TOGGLE */
#define WAVEDIAG_SET		3	/* OM_SET or OM_UPDATE, through ObtainGIRPort() */
#define WAVEDIAG_INPUT		4	/* the gadget's own input (GM_GOACTIVE ...), the same */
#define WAVEDIAG_NORPORT	5	/* ObtainGIRPort() gave none: nothing painted */
#define WAVEDIAG_HEAD		6	/* Waveform_ShowPosition() painted columns */
#define WAVEDIAG_EVENTS		256

struct WaveDiagEvent {
	ULONG wde_Seq;			/* 1, 2, ...: the event's number */
	UBYTE wde_Kind;			/* WAVEDIAG_#? */
	UBYTE wde_Other;		/* TRUE: not the task that made the gadget */
	UBYTE wde_Full;			/* every column painted, not only the changed */
	UBYTE wde_Pad;
	UWORD wde_LayerFlags;		/* the layer's Flags (LAYERREFRESH, LAYERUPDATING) */
	UWORD wde_Hidden;		/* the layer's clip rects another window covers */
	WORD wde_First, wde_Last;	/* the columns painted (-1: none) */
	UWORD wde_Drawn;		/* the columns with a picture then; the rest Missing */
	WORD wde_DamageX0, wde_DamageX1;	/* the layer's damage, window x (-1: none) */
};

struct WaveDiagRead {
	ULONG wdr_After;		/* in: the last event read (0: none yet) */
	struct WaveDiagEvent *wdr_Out;	/* in: room for wdr_Max events */
	UWORD wdr_Max;
	UWORD wdr_Pad;
	ULONG wdr_Lost;			/* out: events gone before they were read */
};

/* The log on (FALSE: no memory); it goes with the object */
BOOL Waveform_DiagOn(Object *wave);
/* The events after wdr_After, oldest first: how many (0 with the log off) */
UWORD Waveform_DiagRead(Object *wave, struct WaveDiagRead *rd);

#endif
