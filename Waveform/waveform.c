/*
 * waveform.c -- waveform.gadget (gadgets/waveform.h): a private
 * gadgetclass that draws a sound and lets the mouse work on it.
 *
 * Where the work happens:
 *   - Every column's picture comes from the source's hook, which answers
 *     from RAM (wavepeaks.c, a .prl), or from samples the program handed
 *     in. Zoomed finer than the source's ws_Finest, the samples are asked
 *     for (Want, the signal) and the picture at ws_Finest is stretched
 *     over those columns meanwhile; what the source has not yet (a picture
 *     still being made) is drawn as Missing, never asked for. No file, no
 *     device, no wait in any method: GM_HANDLEINPUT runs on input.device's
 *     task, and so may GM_LAYOUT and GM_RENDER.
 *   - The columns are cached (each lane's minimum and maximum) and signed
 *     (FNV over what a column's drawing reads, the overlays on it
 *     included): a change repaints the columns whose signature moved and
 *     the overlays over them, nothing else.
 *   - The play head moves from the program's task (Waveform_ShowPosition):
 *     SetGadgetAttrs 25 times a second would hold Intuition's lock while
 *     the gadget waits for its layer (PrismaControl's meter froze the
 *     pointer that way); the layer is tried, a busy one skips the frame.
 *   - The pointer's shape comes from Intuition's hover (V47): GM_HELPTEST
 *     says where the pointer is, GM_QUERYHOVERED is answered by handing
 *     gadgetclass GA_PointerType -- no SetWindowPointer, which hangs the
 *     machine from inside a render (GSheets' sheetgrid.c does the same).
 *   - The arithmetic is waveplan.c's, host-tested.
 *
 *   - Help bubbles through GM_HELPTEST (what is under the pointer, as the
 *     pointer's shape has it) and GM_QUERYHOVERED (the text); a serial as
 *     the help code, so that a bubble changes when its text does.
 *
 * Not yet: the keyboard (WAVEFORM_Keys), an off-screen compose for the
 * full paint (risk R1 of the concept: measured first).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/cghooks.h>
#include <intuition/pointerclass.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include <devices/inputevent.h>
#include <images/bevel.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>

#include <gadgets/waveform.h>
#include "waveplan.h"
#include "boopsi.h"

#define WF_MAXLANES	2
#define HEAD_SPAN	64		/* the columns a head's composed move spans:
					   its old place and its new one together */
#define WF_HATCH_LINES	2		/* 2^2 lines of the area patterns */

/* the grounds a column can have */
#define G_PLAIN		0
#define G_TRIM		1
#define G_SEL		2
#define G_SILENCE	3
#define G_MISSING	4
#define G_DIM		5		/* the overview outside its box */
#define G_GONE		6
#define G_NONE		7		/* past the sound's end */

/* the overlays, for the signatures */
#define OV_CURSOR	1
#define OV_IN		2
#define OV_OUT		3
#define OV_PLAY		4
#define OV_MARKER	5
#define OV_SELFRAME	6
#define OV_BOX		7
#define OV_TICK		8
#define OV_TAG		9		/* a level's label at the lanes' foot */

/* what a drag moves */
#define DRAG_NONE	0
#define DRAG_PENDING	1		/* pressed on the sound, not moved yet */
#define DRAG_SELECT	2
#define DRAG_IN		3
#define DRAG_OUT	4
#define DRAG_SELSTART	5
#define DRAG_SELEND	6
#define DRAG_MARKER	7
#define DRAG_BOX	8
#define DRAG_SEEK	9		/* read-only: a click to report */

/* the next page's stages (wd_NxStage) and its piece: a ShowPosition() a
   step, a piece filled from the samples and painted at once -- an A1200
   does 160 columns in about 130 ms; a page of 620 in four steps, which
   it gets between the read-ahead's samples and the turn (filled whole
   first, then painted, it needed ten: never through) */
#define NX_NONE		0
#define NX_WORK		1
#define NX_DONE		2
#define NX_COLS		160
#define NX_COLS_MIN	8

/* how much to repaint */
#define PAINT_NONE	0
#define PAINT_HEAD	1		/* the floating head only, shown or taken away */
#define PAINT_UPDATE	2		/* the columns whose signature moved */
#define PAINT_REFILL	3		/* the view moved: the cache anew, then update */
#define PAINT_FULL	4		/* everything, frame included */

/* an overlay: what it is, its anchor column, the columns it covers */
struct Overlay {
	UWORD ov_Kind;
	UWORD ov_Index;
	LONG ov_X;			/* inner column */
	LONG ov_X0, ov_X1;		/* covered, inner columns */
};
#define MAX_OVERLAYS	48
#define OV_RESERVED	6		/* kept for the selection's frame, the
					   handles, the cursor, the box and the
					   head: ticks and markers take the rest */
#define MAX_HITMARKERS	32

struct WaveData {
	Class *wd_Class;
	Object *wd_Obj;
	struct WaveSource *wd_Source;
	struct WaveView wd_View;

	/* looks */
	UWORD wd_Lanes, wd_LaneGap, wd_Scale, wd_DBRange, wd_Gain;
	UWORD wd_WaveStyle, wd_Grid, wd_Patterns, wd_Ruler, wd_TimeFormat;
	UWORD wd_CursorStyle, wd_ClickAction, wd_SelectMode, wd_SelectStyle;
	UWORD wd_DoubleClick, wd_DragThreshold, wd_TrimHandles, wd_HandleStyle;
	UWORD wd_HandleSize, wd_HandleGrab, wd_TrimShade, wd_MarkerStyle;
	UWORD wd_Snap, wd_SnapDistance, wd_PositionStyle, wd_BoxStyle, wd_Follow;
	UWORD wd_ExtendQual, wd_MarkerCount, wd_RegionCount, wd_LevelCount, wd_FadeShape;
	ULONG wd_BevelStyle, wd_TimeOrigin, wd_MinLength, wd_SnapStep;
	BOOL wd_ClipMark, wd_CenterLine, wd_DragEdges, wd_DragMarkers, wd_Overview;
	BOOL wd_ReadOnly, wd_Keys, wd_Help, wd_Pointers, wd_AutoScroll, wd_LevelLine;
	CONST_STRPTR *wd_LaneLabels;
	CONST_STRPTR *wd_HandleLabels;
	CONST_STRPTR *wd_HelpTexts;		/* WAVEFORM_HelpTexts: NULL the class's */
	struct TextFont *wd_Font;		/* given, or the screen's */
	BOOL wd_FontGiven;
	struct WaveMarker *wd_Markers;
	struct WaveRegion *wd_Regions;
	struct WaveLevel *wd_Levels;
	struct Hook *wd_SnapHook;
	ULONG wd_PointerType[4];		/* handle, edge, wave, box */
	Object **wd_CustomPointers;

	/* state */
	ULONG wd_Cursor, wd_SelStart, wd_SelEnd, wd_TrimIn, wd_TrimOut;
	ULONG wd_Position, wd_BoxTop, wd_BoxVisible;
	ULONG wd_FadeIn, wd_FadeOut;		/* the end fades' frames: they go with the handles */
	ULONG wd_Changes;
	struct Task *wd_SigTask;
	ULONG wd_SigMask;
	struct WaveWant wd_Want;
	struct WaveSamples *wd_Samples;
	ULONG wd_PointerFrame;
	LONG wd_ActiveMarker;
	ULONG wd_ActiveFrame;

	/* the paint log (Waveform_DiagOn): NULL while off */
	struct DiagRing *wd_Diag;
	struct Task *wd_Owner;			/* the task that made the gadget */
	UBYTE wd_DiagCtx;			/* what repaint() is called for: WAVEDIAG_#? */
	BOOL wd_DiagFull;			/* the last paint: every column */
	WORD wd_DiagFirst, wd_DiagLast;		/* the last paint's columns, -1 none */

	/* colours: what was asked, what is used */
	ULONG wd_RGB[WAVEFORM_COLOR_COUNT];
	LONG wd_PenTag[WAVEFORM_COLOR_COUNT];	/* -1: none given */
	LONG wd_Pen[WAVEFORM_COLOR_COUNT];
	BOOL wd_Obtained[WAVEFORM_COLOR_COUNT];
	struct ColorMap *wd_CM;
	ULONG wd_Precision;
	BOOL wd_PensReady;
	BOOL wd_UsePatterns;

	/* geometry, from GM_LAYOUT */
	BOOL wd_Laid;
	WORD wd_BoxL, wd_BoxT, wd_BoxW, wd_BoxH;	/* the gadget's box the layout was made for */
	struct Gadget *wd_Gad;
	struct DrawInfo *wd_Dri;
	Object *wd_Bevel;
	WORD wd_FrameW, wd_FrameH;
	WORD wd_IX, wd_IY, wd_IW, wd_IH;	/* inside the frame, the window's coordinates */
	WORD wd_RulerY, wd_RulerH;		/* RulerH 0: no ruler */
	WORD wd_LaneY, wd_LaneH, wd_LaneCount;	/* LaneH: one lane's */
	WORD wd_LanesH;				/* all of them, the gaps included */
	struct RastPort wd_MRP;			/* measuring only, never drawn in */

	/* the lanes painted off the window, blitted span by span: nothing
	   shows half drawn (a column filled anew blinked -- the play head) */
	struct RastPort wd_BufRP;		/* its BitMap NULL: none, straight in */
	WORD wd_BufX, wd_BufY;			/* the window's point its (0, 0) holds */
	WORD wd_BufW, wd_BufH;
	/* with the buffer the play head floats over the lanes: drawn into the
	   window only, so a move is the buffer's columns back and a line */
	BOOL wd_HeadUp;				/* drawn, over the window's columns ... */
	BOOL wd_HeadOff;			/* WAVEFORM_HeadOff: never drawn (a sprite has it) */
	UWORD wd_AheadCols;			/* WAVEFORM_AheadColumns */
	UWORD wd_PaintCols;			/* WAVEFORM_PaintColumns: a paint's columns at most, 0 all */
	UWORD wd_PaintLeft;			/* WAVEFORM_PaintLeft: the view's columns still to paint */
	BOOL wd_Turned;				/* ShowPosition() turned the page: its paint may be lazy */
	BOOL wd_Lazy;				/* a turned page filled, planned and painted a piece at a time */
	UWORD wd_LazyCol, wd_LazyIn;		/* ... its first column not yet; the columns inside the sound */
	BOOL wd_FillNew;			/* WAVEFORM_FillNew: a new view lazy too, the old picture kept */
	BOOL wd_LazyKeep;			/* ... this lazy view keeps it (not a turned page) */
	BOOL wd_NoSamples;			/* WAVEFORM_SamplesNone: a lazy view's missing columns stretched */
	struct WaveHeadBox wd_HeadBox;		/* WAVEFORM_HeadBox's answer */
	WORD wd_HeadX0, wd_HeadX1;		/* ... these (the window's coordinates) */
	/* the head composed off the window -- the buffer's columns under its
	   old place and its new one, the line over them -- then put in with
	   one blit: on a native screen the window never shows it gone (a
	   blit back and a line drawn after it flickered) */
	struct RastPort wd_HeadRP;		/* its BitMap NULL: none */
	WORD wd_HeadW;				/* the columns it holds */
	/* a planar buffer in chip RAM takes the wave through a mask, a pen at
	   a time (paint_waves): its bits set in fast RAM, copied for the
	   blitter -- NULL: a RectFill a column (a graphics card's buffer) */
	UWORD *wd_Mask, *wd_MaskChip;
	UWORD wd_MaskWPR;			/* words a row they hold (wd_BufH rows) */
	/* the next page painted ahead while pages are followed (the program
	   showing the play head): a copy of this data turned a page on, with
	   its own buffer, mask and cache, painted a piece at each Waveform_
	   ShowPosition() and swapped in when the page turns -- the turn one
	   blit (a paint of the page took 0.6 s on an A1200). Only ever on
	   the program's task, under the window's layer. NULL: none */
	struct WaveData *wd_Next;
	ULONG wd_Gen;				/* bumped by what changes the drawing */
	/* the copy's own: how far it got, from which page and Gen */
	UWORD wd_NxStage, wd_NxCol;
	UWORD wd_NxIn;				/* its columns inside the sound */
	ULONG wd_NxFrom, wd_NxGen;
	/* Waveform_ShowPosition() could not draw (the layer busy, a drag, not
	   laid out): the next call draws, at the same frame too */
	BOOL wd_Owed;

	/* the column cache */
	UWORD wd_CacheMax;			/* columns the arrays hold */
	UWORD wd_CacheCols;			/* columns filled */
	BYTE *wd_Cache;				/* columns x channels x 2 */
	UBYTE *wd_Ground;			/* columns */
	ULONG *wd_Sig;				/* columns: as drawn */
	ULONG *wd_NewSig;			/* columns: as now */
	BOOL wd_CacheValid;
	BOOL wd_SigValid;
	ULONG wd_RulerSig;
	ULONG wd_Answered;			/* columns the source or samples gave */
	/* the columns the samples gave last, kept while what made them stays:
	   a program that sets its source again as its picture grows (every
	   per cent of a read) would have them computed anew each time */
	BOOL wd_SampKept;
	ULONG wd_SKFirst, wd_SKStep, wd_SKCount, wd_SKGot, wd_SKMore;
	ULONG wd_Drawn;				/* those, then the ones stretched from ws_Finest */

	/* input */
	UWORD wd_Drag;
	LONG wd_GrabOffset;			/* columns between the pointer and what it holds */
	LONG wd_PressX, wd_PressY;
	ULONG wd_Anchor;			/* a selection drag's fixed end */
	ULONG wd_ClickSecs, wd_ClickMicros;
	LONG wd_ClickX;
	UWORD wd_ClickCount;
	LONG wd_LastX;				/* the pointer at the last event, inner columns */
	BOOL wd_HoverOK;
	LONG wd_HoverX, wd_HoverY;		/* gadget-relative */
	ULONG wd_PtrShown;			/* the shape handed to gadgetclass */
	APTR wd_PtrObjShown;
	BOOL wd_Activated;

	/* a paint's and a hit test's lists: here, not on input.device's
	   small stack (the paints are one at a time: the layer's lock) */
	struct Overlay wd_Ov[MAX_OVERLAYS + 1];
	UWORD wd_OvCount;
	LONG wd_HitMarkers[MAX_HITMARKERS];
};

static Class *wf_class = NULL;			/* for the functions' INST_DATA */
static struct Library *wf_bevel_lib = NULL;	/* images/bevel.image, the class's own */

static const ULONG default_rgb[WAVEFORM_COLOR_COUNT] = {
	0xD4D4CCUL, 0x2B4A7EUL, 0x10274DUL, 0xFFFFFFUL, 0x8E8E88UL, 0x585856UL,
	0x9E9E98UL, 0xBDBDB5UL, 0xD02020UL, 0x2E8B3AUL, 0xC0392BUL, 0xFFFFFFUL,
	0x1A62A0UL, 0xFF8A00UL, 0x000000UL, WAVEFORM_COLOR_DEFAULT, WAVEFORM_COLOR_DEFAULT,
	0xC2CFDCUL, 0xC0392BUL, 0x707070UL, 0xFFFFFFUL, 0x9A9A94UL, 0x7A2FB0UL
};
/* the DrawInfo pen each element falls back on (and uses on few colours) */
static const UBYTE fallback_pen[WAVEFORM_COLOR_COUNT] = {
	BACKGROUNDPEN, TEXTPEN, TEXTPEN, SHINEPEN, BACKGROUNDPEN, SHADOWPEN,
	SHADOWPEN, SHADOWPEN, FILLPEN, SHINEPEN, TEXTPEN, TEXTPEN,
	FILLPEN, FILLPEN, TEXTPEN, BACKGROUNDPEN, TEXTPEN,
	BACKGROUNDPEN, TEXTPEN, SHADOWPEN, SHINEPEN, SHADOWPEN, FILLPEN
};

static UWORD hatch_pattern[4] = { 0x8888, 0x4444, 0x2222, 0x1111 };
static UWORD dots_pattern[4] = { 0x8888, 0x0000, 0x2222, 0x0000 };
static UWORD dash_pattern[4] = { 0xFFFF, 0x0000, 0xFFFF, 0x0000 };
static UWORD ghost_pattern[2] = { 0x8888, 0x2222 };	/* Intuition's, for a disabled gadget */

/* ---- small helpers ---- */

static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

static void changed(struct WaveData *wd, ULONG what)
{
	wd->wd_Changes |= what;
	if (wd->wd_SigTask && wd->wd_SigMask) Signal(wd->wd_SigTask, wd->wd_SigMask);
}

static UWORD channels_of(struct WaveData *wd)
{
	return (UWORD) (wd->wd_Source && wd->wd_Source->ws_Channels >= 1 && wd->wd_Source->ws_Channels <= 2 ?
			wd->wd_Source->ws_Channels : 1);
}

static UWORD lanes_of(struct WaveData *wd)
{
	if (wd->wd_Overview) return 1;
	if (wd->wd_Lanes == WAVEFORM_LANES_SPLIT || wd->wd_Lanes == WAVEFORM_LANES_AUTO)
		return channels_of(wd);
	return 1;
}

static LONG pen(struct WaveData *wd, UWORD element)
{
	return wd->wd_Pen[element];
}

/* the frames of column c (inner, 0-based): first, and the frame after */
static ULONG col_first(struct WaveData *wd, LONG c)
{
	return WaveView_FrameAt(&wd->wd_View, c);
}

static LONG col_of(struct WaveData *wd, ULONG frame)
{
	return WaveView_ColumnOf(&wd->wd_View, frame);
}

/* ---- pens ---- */

static void release_pens(struct WaveData *wd)
{
	UWORD i;

	for (i = 0; i < WAVEFORM_COLOR_COUNT; i++) {
		if (wd->wd_Obtained[i] && wd->wd_CM) ReleasePen(wd->wd_CM, (ULONG) wd->wd_Pen[i]);
		wd->wd_Obtained[i] = FALSE;
	}
	wd->wd_PensReady = FALSE;
}

/* every element's pen: the one given, else the RGB's nearest (the ones
   that must stand apart first: handles, play head, clip), else DrawInfo's */
static void obtain_pens(struct WaveData *wd, struct GadgetInfo *gi)
{
	static const UBYTE order[WAVEFORM_COLOR_COUNT] = {
		WAVEFORM_COLOR_INHANDLE, WAVEFORM_COLOR_OUTHANDLE, WAVEFORM_COLOR_POSITION, WAVEFORM_COLOR_CLIP,
		WAVEFORM_COLOR_WAVE, WAVEFORM_COLOR_BACKGROUND, WAVEFORM_COLOR_SELECTION, WAVEFORM_COLOR_WAVESELECTED, WAVEFORM_COLOR_LEVEL,
		WAVEFORM_COLOR_TRIMMED, WAVEFORM_COLOR_TRIMMEDWAVE, WAVEFORM_COLOR_MARKER, WAVEFORM_COLOR_CURSOR,
		WAVEFORM_COLOR_HANDLETEXT, WAVEFORM_COLOR_CENTERLINE, WAVEFORM_COLOR_GRID, WAVEFORM_COLOR_SILENCE,
		WAVEFORM_COLOR_CLICK, WAVEFORM_COLOR_GONE, WAVEFORM_COLOR_BOX, WAVEFORM_COLOR_MISSING,
		WAVEFORM_COLOR_RULERBACK, WAVEFORM_COLOR_RULERTEXT
	};
	struct DrawInfo *dri = gi->gi_DrInfo;
	struct Screen *scr = gi->gi_Screen;
	UWORD i, e, depth = 8;
	ULONG rgb;
	LONG p;

	if (wd->wd_PensReady || !dri) return;
	if (scr) {
		wd->wd_CM = scr->ViewPort.ColorMap;
		depth = (UWORD) GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH);
	}
	wd->wd_UsePatterns = (BOOL) (wd->wd_Patterns == WAVEFORM_PATTERNS_ALWAYS ||
				     (wd->wd_Patterns == WAVEFORM_PATTERNS_AUTO && depth < 4));
	for (i = 0; i < WAVEFORM_COLOR_COUNT; i++) {
		e = order[i];
		wd->wd_Obtained[e] = FALSE;
		p = -1;
		if (wd->wd_PenTag[e] >= 0) {
			p = wd->wd_PenTag[e];
		} else if (!wd->wd_UsePatterns && wd->wd_CM && (rgb = wd->wd_RGB[e]) != WAVEFORM_COLOR_DEFAULT) {
			p = ObtainBestPen(wd->wd_CM, ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL,
					  (rgb & 0xFF) * 0x01010101UL, OBP_Precision, wd->wd_Precision, OBP_FailIfBad, TRUE, TAG_DONE);
			if (p >= 0) wd->wd_Obtained[e] = TRUE;
		}
		if (p < 0) p = dri->dri_Pens[fallback_pen[e]];
		wd->wd_Pen[e] = p;
	}
	/* on few colours the handles are told apart by their flags' shades */
	if (wd->wd_UsePatterns) {
		if (wd->wd_PenTag[WAVEFORM_COLOR_INHANDLE] < 0) wd->wd_Pen[WAVEFORM_COLOR_INHANDLE] = dri->dri_Pens[SHINEPEN];
		if (wd->wd_PenTag[WAVEFORM_COLOR_OUTHANDLE] < 0) wd->wd_Pen[WAVEFORM_COLOR_OUTHANDLE] = dri->dri_Pens[TEXTPEN];
	}
	wd->wd_PensReady = TRUE;
}

/* the text pen on a handle's flag: on a flag of DrawInfo's pens (few
   colours, or none free) the one that stands out from it */
static LONG flag_text_pen(struct WaveData *wd, BOOL in)
{
	UWORD e = (UWORD) (in ? WAVEFORM_COLOR_INHANDLE : WAVEFORM_COLOR_OUTHANDLE);

	if (wd->wd_PenTag[WAVEFORM_COLOR_HANDLETEXT] < 0 && wd->wd_Dri && wd->wd_PenTag[e] < 0 &&
	    (wd->wd_UsePatterns || !wd->wd_Obtained[e]))
		return wd->wd_Dri->dri_Pens[in ? TEXTPEN : SHINEPEN];
	return pen(wd, WAVEFORM_COLOR_HANDLETEXT);
}

/* the text pen on a level's tag: on DrawInfo's fill pen (few colours,
   or none free) the text DrawInfo puts on it */
static LONG tag_text_pen(struct WaveData *wd)
{
	if (wd->wd_PenTag[WAVEFORM_COLOR_HANDLETEXT] < 0 && wd->wd_Dri && wd->wd_PenTag[WAVEFORM_COLOR_LEVEL] < 0 &&
	    (wd->wd_UsePatterns || !wd->wd_Obtained[WAVEFORM_COLOR_LEVEL]))
		return wd->wd_Dri->dri_Pens[FILLTEXTPEN];
	return pen(wd, WAVEFORM_COLOR_HANDLETEXT);
}

/* a shade that would not show: its pen the ground's (few colours, or no
   pen free) -- drawn as a pattern then */
static BOOL flat_shade(struct WaveData *wd, UWORD element)
{
	return (BOOL) (wd->wd_UsePatterns || pen(wd, element) == pen(wd, WAVEFORM_COLOR_BACKGROUND));
}

/* ---- the cache ---- */

static BOOL ensure_cache(struct WaveData *wd, UWORD cols)
{
	ULONG bytes;
	UBYTE *mem;

	if (cols <= wd->wd_CacheMax && wd->wd_Cache) return TRUE;
	/* one block: the minima and maxima, the grounds, two signatures */
	bytes = (ULONG) cols * (WF_MAXLANES * 2 + 1 + 2 * sizeof(ULONG)) + 4;
	if (!(mem = (UBYTE *) AllocVec(bytes, MEMF_ANY | MEMF_CLEAR))) return FALSE;
	if (wd->wd_Cache) FreeVec(wd->wd_Sig);
	wd->wd_SampKept = FALSE;
	wd->wd_Sig = (ULONG *) mem;
	wd->wd_NewSig = wd->wd_Sig + cols;
	wd->wd_Cache = (BYTE *) (wd->wd_NewSig + cols);
	wd->wd_Ground = (UBYTE *) wd->wd_Cache + (ULONG) cols * WF_MAXLANES * 2;
	wd->wd_CacheMax = cols;
	wd->wd_SigValid = FALSE;
	wd->wd_CacheValid = FALSE;
	return TRUE;
}

static void free_cache(struct WaveData *wd)
{
	if (wd->wd_Cache) FreeVec(wd->wd_Sig);
	wd->wd_Sig = wd->wd_NewSig = NULL;
	wd->wd_Cache = NULL;
	wd->wd_Ground = NULL;
	wd->wd_CacheMax = 0;
}

/* columns from..cols - 1, finer than the source answers: each the
   ws_Finest step its first frame lies in, while the samples are not
   there: how many, from from on */
static ULONG stretch(struct WaveData *wd, ULONG from, ULONG cols)
{
	struct WavePeakMsg q;
	ULONG fin = wd->wd_Source->ws_Finest, c, f, at = WAVEFORM_NONE;
	UWORD ch = channels_of(wd), k;
	BYTE *col, *prev;

	for (c = from; c < cols; c++) {
		if ((f = col_first(wd, (LONG) c)) >= wd->wd_View.wv_Frames) break;
		col = wd->wd_Cache + c * ch * 2;
		/* the step's start: a mask for a power of two (wavepeaks.c's 1024),
		   no 32-bit remainder a column */
		f = (fin & (fin - 1)) ? f - f % fin : f & ~(fin - 1);
		if (f == at) {
			/* the step the column before showed */
			prev = col - ch * 2;
			for (k = 0; k < ch * 2; k++) col[k] = prev[k];
			continue;
		}
		q.wpm_First = f;
		q.wpm_Step = fin;
		q.wpm_Count = 1;
		q.wpm_Out = col;
		if (CallHookPkt(&wd->wd_Source->ws_Peaks, wd->wd_Obj, &q) != 1) break;
		at = f;
	}
	return c - from;
}

/* the view's columns from the source, then the samples; finer than the
   source answers, the rest stretched from it and the samples wanted */
static void fill_cache(struct WaveData *wd)
{
	struct WavePeakMsg q;
	UWORD cols = (UWORD) (wd->wd_IW > 0 ? wd->wd_IW : 0), ch = channels_of(wd);
	ULONG got, more, first, frames;
	BOOL fine;

	if (cols > wd->wd_CacheMax) cols = wd->wd_CacheMax;
	wd->wd_CacheCols = cols;
	wd->wd_CacheValid = TRUE;
	wd->wd_Answered = wd->wd_Drawn = 0;
	if (!wd->wd_Source || cols == 0) return;
	fine = (BOOL) (wd->wd_Source->ws_Finest > 1 && wd->wd_View.wv_Zoom < wd->wd_Source->ws_Finest);
	q.wpm_First = wd->wd_View.wv_Top;
	q.wpm_Step = wd->wd_View.wv_Zoom;
	q.wpm_Count = cols;
	q.wpm_Out = wd->wd_Cache;
	got = CallHookPkt(&wd->wd_Source->ws_Peaks, wd->wd_Obj, &q);
	if (got > cols) got = cols;
	if (got < cols && wd->wd_Samples && wd->wd_Samples->wsm_Channels == ch) {
		q.wpm_First = col_first(wd, (LONG) got);
		q.wpm_Count = cols - got;
		q.wpm_Out = wd->wd_Cache + got * ch * 2;
		if (wd->wd_SampKept && wd->wd_SKFirst == q.wpm_First && wd->wd_SKStep == q.wpm_Step &&
		    wd->wd_SKCount == q.wpm_Count && wd->wd_SKGot == got) {
			more = wd->wd_SKMore;	/* those columns as they are */
		} else {
			more = WavePlan_FromSamples(wd->wd_Samples, &q);
			/* a last column the samples end inside, short of the sound's
			   end: only part of it -- left to the next samples */
			frames = wd->wd_Samples->wsm_First + wd->wd_Samples->wsm_Frames;
			if (more && frames < wd->wd_View.wv_Frames && frames < col_first(wd, (LONG) (got + more))) more--;
			wd->wd_SKFirst = q.wpm_First;
			wd->wd_SKStep = q.wpm_Step;
			wd->wd_SKCount = q.wpm_Count;
			wd->wd_SKGot = got;
			wd->wd_SKMore = more;
			wd->wd_SampKept = TRUE;
		}
		got += more;
	} else {
		/* those columns the source's now (a zoom out and back in would
		   find them kept, but no longer there) */
		wd->wd_SampKept = FALSE;
	}
	wd->wd_Answered = wd->wd_Drawn = got;
	if (got < cols && fine) wd->wd_Drawn = got + stretch(wd, got, cols);
	/* what only samples give: asked for, once per change; a picture not
	   made yet is the program's to finish */
	first = col_first(wd, (LONG) got);
	if (fine && got < cols && first < wd->wd_View.wv_Frames) {
		frames = col_first(wd, (LONG) cols) - first;
		if (wd->wd_Want.ww_First != first || wd->wd_Want.ww_Frames != frames) {
			wd->wd_Want.ww_First = first;
			wd->wd_Want.ww_Frames = frames;
			changed(wd, WAVEFORM_CHANGED_WANT);
		}
	} else {
		wd->wd_Want.ww_First = WAVEFORM_NONE;
		wd->wd_Want.ww_Frames = 0;
	}
}

/* ---- regions and grounds ---- */

/* the region kind (as a ground) a column's frames fall in; G_PLAIN none */
static UBYTE region_ground(struct WaveData *wd, ULONG f)
{
	UWORD i;
	struct WaveRegion *r;

	for (i = 0; i < wd->wd_RegionCount && wd->wd_Regions; i++) {
		r = &wd->wd_Regions[i];
		if (r->wrg_Style != WAVEREGION_TINT && r->wrg_Style != WAVEREGION_HATCH) continue;
		if (f < r->wrg_First || f >= r->wrg_End) continue;
		if (r->wrg_Kind == WAVEREGION_GONE) return G_GONE;
		return G_SILENCE;
	}
	return G_PLAIN;
}

static BOOL in_selection(struct WaveData *wd, ULONG f)
{
	return (BOOL) (wd->wd_SelStart != WAVEFORM_NONE && wd->wd_SelEnd != WAVEFORM_NONE &&
		       wd->wd_SelEnd > wd->wd_SelStart && f >= wd->wd_SelStart && f < wd->wd_SelEnd);
}

/* the columns' grounds, c0 .. c1 (span), from the frame in each one's
   middle */
static void plan_grounds_span(struct WaveData *wd, ULONG span)
{
	UWORD c = (UWORD) (span >> 16), c1 = (UWORD) (span & 0xFFFF);
	ULONG f, a, b, end = wd->wd_View.wv_Frames, zoom = wd->wd_View.wv_Zoom;
	UBYTE g;

	for (a = col_first(wd, (LONG) c); c <= c1 && c < wd->wd_CacheCols; c++, a = b) {
		/* the next column's first frame a step on, the sound's end at
		   most: col_first(c + 1) without its product */
		b = (a >= end || zoom > end - a) ? end : a + zoom;
		f = a + (b - a) / 2;
		if (a >= wd->wd_View.wv_Frames || !wd->wd_Source) {
			g = G_NONE;
		} else if (c >= wd->wd_Drawn) {
			g = G_MISSING;
		} else if (wd->wd_Overview && wd->wd_BoxStyle == WAVEFORM_BOX_DIM && wd->wd_BoxVisible &&
			   (f < wd->wd_BoxTop || f - wd->wd_BoxTop >= wd->wd_BoxVisible)) {
			g = G_DIM;
		} else if (wd->wd_SelectStyle != WAVEFORM_SELSTYLE_FRAME && wd->wd_SelStart != WAVEFORM_NONE && in_selection(wd, f)) {
			g = G_SEL;
		} else if (wd->wd_TrimShade != WAVEFORM_SHADE_NONE &&
			   ((wd->wd_TrimIn != WAVEFORM_NONE && f < wd->wd_TrimIn) ||
			    (wd->wd_TrimOut != WAVEFORM_NONE && f >= wd->wd_TrimOut))) {
			g = G_TRIM;
		} else {
			g = (wd->wd_RegionCount && wd->wd_Regions) ? region_ground(wd, f) : G_PLAIN;
		}
		wd->wd_Ground[c] = g;
	}
}

/* every column's ground */
static void plan_grounds(struct WaveData *wd)
{
	if (wd->wd_CacheCols) plan_grounds_span(wd, (ULONG) (wd->wd_CacheCols - 1));
}

/* ---- the level: a fade's line, a gain's bar ---- */

/* anything of the level to draw: the program's levels, an end fade */
static BOOL has_levels(struct WaveData *wd)
{
	return (BOOL) (wd->wd_LevelLine && ((wd->wd_Levels && wd->wd_LevelCount) || wd->wd_FadeIn || wd->wd_FadeOut));
}

/* a walk of the levels from the first: the program's, and the end fades
   where the handles are now -- never past the other handle */
static void level_walk(struct WaveData *wd, struct WaveLevelWalk *w)
{
	ULONG in = wd->wd_TrimIn == WAVEFORM_NONE ? 0 : wd->wd_TrimIn;
	ULONG out = wd->wd_TrimOut == WAVEFORM_NONE ? wd->wd_View.wv_Frames : wd->wd_TrimOut;
	ULONG room = out > in ? out - in : 0;

	w->lw_Level = wd->wd_Levels;
	w->lw_Count = (UWORD) (wd->wd_Levels ? wd->wd_LevelCount : 0);
	w->lw_Next = 0;
	w->lw_In = in;
	w->lw_FadeIn = wd->wd_FadeIn < room ? wd->wd_FadeIn : room;
	w->lw_Out = out;
	w->lw_FadeOut = wd->wd_FadeOut < room ? wd->wd_FadeOut : room;
	w->lw_Shape = wd->wd_FadeShape;
	w->lw_Gain = WAVELEVEL_UNITY;
}

/* column c's line (WAVEPLAN_NOLINE: none) and gain (in w), at the frame
   in its middle as its ground; the columns asked in rising order. None
   where the sound is not drawn as it plays: trimmed, dimmed, gone,
   missing, past its end */
static UWORD col_line(struct WaveData *wd, struct WaveLevelWalk *w, LONG c)
{
	ULONG a, b, end = wd->wd_View.wv_Frames;
	UBYTE g = wd->wd_Ground[c];

	w->lw_Gain = WAVELEVEL_UNITY;
	if (g == G_NONE || g == G_MISSING || g == G_TRIM || g == G_DIM || g == G_GONE) return WAVEPLAN_NOLINE;
	a = col_first(wd, c);
	b = (a >= end || wd->wd_View.wv_Zoom > end - a) ? end : a + wd->wd_View.wv_Zoom;
	return WavePlan_LevelAt(w, a + (b - a) / 2);
}

/* ---- the paint ---- */

struct Paint {
	struct RastPort *pa_RP;
	struct WaveData *pa_WD;
	/* the RastPort's (0, 0) from the window's: a buffer's offset. The
	   gadget's own geometry never moves for a paint -- input.device's hit
	   tests read it while the program's task paints the head */
	WORD pa_DX, pa_DY;
};

/* the inner box's left column and the lanes' top line in pa's RastPort */
#define PIX(pa)		((LONG) (pa)->pa_WD->wd_IX + (LONG) (pa)->pa_DX)
#define PLY(pa)		((LONG) (pa)->pa_WD->wd_LaneY + (LONG) (pa)->pa_DY)

/* r filled where it lies on the lanes: what an overlay would draw past
   the inner box -- into the frame, or past a layerless buffer's edge,
   where nothing clips -- left out */
static void lane_fill(struct Paint *pa, const struct Rectangle *r)
{
	struct WaveData *wd = pa->pa_WD;
	LONG ix = PIX(pa), ly = PLY(pa), x0 = r->MinX, x1 = r->MaxX, y0 = r->MinY, y1 = r->MaxY;

	if (x0 < ix) x0 = ix;
	if (x1 > ix + wd->wd_IW - 1) x1 = ix + wd->wd_IW - 1;
	if (y0 < ly) y0 = ly;
	if (y1 > ly + wd->wd_LanesH - 1) y1 = ly + wd->wd_LanesH - 1;
	if (x1 >= x0 && y1 >= y0) RectFill(pa->pa_RP, x0, y0, x1, y1);
}

static void fill(struct Paint *pa, struct IBox *b)
{
	if (b->Width <= 0 || b->Height <= 0) return;
	RectFill(pa->pa_RP, b->Left, b->Top, b->Left + b->Width - 1, b->Top + b->Height - 1);
}

static void box_fill(struct Paint *pa, struct IBox *b)
{
	fill(pa, b);
}

/* an area pattern over a box, JAM1 (the 0 bits keep what is there) */
static void pattern_fill(struct Paint *pa, struct IBox *b)
{
	SetDrMd(pa->pa_RP, JAM1);
	fill(pa, b);
	SetAfPt(pa->pa_RP, NULL, 0);
}

/* a lane's top and the frames-to-pixels scale */
struct Lane {
	WORD ln_Y, ln_H, ln_Mid;
	struct WaveScale ln_Scale;
};

static void lane_box(struct Paint *pa, UWORD l, struct Lane *ln)
{
	struct WaveData *wd = pa->pa_WD;
	WORD half;

	ln->ln_Y = (WORD) (PLY(pa) + l * (wd->wd_LaneH + wd->wd_LaneGap));
	ln->ln_H = wd->wd_LaneH;
	ln->ln_Mid = (WORD) (ln->ln_Y + ln->ln_H / 2);
	/* a lane of a line or two: no swing (not 0xFFFF lines of one) */
	half = (WORD) (wd->wd_WaveStyle == WAVEFORM_WAVE_BARS ? ln->ln_H - 1 : ln->ln_H / 2 - 1);
	ln->ln_Scale.wsc_Half = (UWORD) (half > 0 ? half : 0);
	ln->ln_Scale.wsc_Gain = wd->wd_Gain;
	ln->ln_Scale.wsc_Mode = wd->wd_Scale;
	ln->ln_Scale.wsc_DBRange = wd->wd_DBRange;
}

/* a lane's minimum and maximum at column c (lanes mixed or picked) */
static void lane_values(struct WaveData *wd, ULONG cl, BYTE *mm)
{
	UWORD c = (UWORD) (cl & 0xFFFF), lane = (UWORD) (cl >> 16), ch = channels_of(wd);
	const BYTE *p = wd->wd_Cache + (ULONG) c * ch * 2;
	BYTE mn = p[0], mx = p[1];

	if (ch == 2) {
		if (lanes_of(wd) == 2) {
			mn = p[lane * 2];
			mx = p[lane * 2 + 1];
		} else if (wd->wd_Lanes == WAVEFORM_LANES_RIGHT) {
			mn = p[2];
			mx = p[3];
		} else if (wd->wd_Lanes != WAVEFORM_LANES_LEFT) {
			if (p[2] < mn) mn = p[2];
			if (p[3] > mx) mx = p[3];
		}
	}
	mm[0] = mn;
	mm[1] = mx;
}

static UWORD ground_element(UBYTE g)
{
	switch (g) {
	case G_TRIM: return WAVEFORM_COLOR_TRIMMED;
	case G_SEL: return WAVEFORM_COLOR_SELECTION;
	case G_SILENCE: return WAVEFORM_COLOR_SILENCE;
	case G_MISSING: return WAVEFORM_COLOR_BACKGROUND;
	case G_DIM: return WAVEFORM_COLOR_TRIMMED;
	case G_GONE: return WAVEFORM_COLOR_GONE;
	case G_NONE: return WAVEFORM_COLOR_RULERBACK;
	default: return WAVEFORM_COLOR_BACKGROUND;
	}
}

static UWORD wave_element(UBYTE g)
{
	switch (g) {
	case G_TRIM: case G_DIM: case G_GONE: return WAVEFORM_COLOR_TRIMMEDWAVE;
	case G_SEL: return WAVEFORM_COLOR_WAVESELECTED;
	default: return WAVEFORM_COLOR_WAVE;
	}
}

/* the grounds of columns from..to (inner), run by run, lane by lane, with
   their patterns and the centre line */
static void paint_grounds(struct Paint *pa, LONG from, LONG to)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct IBox b;
	struct Lane ln;
	LONG c, run;
	UBYTE g;
	UWORD l, lanes = lanes_of(wd);
	BOOL invert;

	for (c = from; c <= to; c = run) {
		g = wd->wd_Ground[c];
		for (run = c + 1; run <= to && wd->wd_Ground[run] == g; run++) {
		}
		invert = (BOOL) (g == G_SEL && wd->wd_SelectStyle == WAVEFORM_SELSTYLE_INVERT);
		b.Left = (WORD) (PIX(pa) + c);
		b.Width = (WORD) (run - c);
		for (l = 0; l < lanes; l++) {
			lane_box(pa, l, &ln);
			b.Top = ln.ln_Y;
			b.Height = ln.ln_H;
			SetDrMd(rp, JAM1);
			SetAPen(rp, (ULONG) pen(wd, invert ? WAVEFORM_COLOR_WAVE : ground_element(g)));
			box_fill(pa, &b);
			if (g == G_MISSING ||
			    (g == G_TRIM && (wd->wd_TrimShade == WAVEFORM_SHADE_HATCH || flat_shade(wd, WAVEFORM_COLOR_TRIMMED))) ||
			    (g == G_DIM && flat_shade(wd, WAVEFORM_COLOR_TRIMMED))) {
				SetAPen(rp, (ULONG) pen(wd, g == G_MISSING ? WAVEFORM_COLOR_MISSING : WAVEFORM_COLOR_TRIMMEDWAVE));
				SetAfPt(rp, hatch_pattern, WF_HATCH_LINES);
				pattern_fill(pa, &b);
			} else if (g == G_SILENCE && flat_shade(wd, WAVEFORM_COLOR_SILENCE)) {
				SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_TRIMMEDWAVE));
				SetAfPt(rp, dots_pattern, WF_HATCH_LINES);
				pattern_fill(pa, &b);
			}
			if (wd->wd_CenterLine && g != G_NONE && g != G_MISSING && wd->wd_WaveStyle != WAVEFORM_WAVE_BARS) {
				SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_CENTERLINE));
				RectFill(rp, b.Left, ln.ln_Mid, b.Left + b.Width - 1, ln.ln_Mid);
			}
		}
	}
	/* the lanes' gaps */
	if (lanes > 1 && wd->wd_LaneGap) {
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_CENTERLINE));
		lane_box(pa, 0, &ln);
		b.Left = (WORD) (PIX(pa) + from);
		b.Width = (WORD) (to - from + 1);
		b.Top = (WORD) (ln.ln_Y + ln.ln_H);
		b.Height = (WORD) wd->wd_LaneGap;
		box_fill(pa, &b);
	}
}

/* the time grid's dotted lines at the major ticks between from and to */
static void paint_grid(struct Paint *pa, LONG from, LONG to)
{
	struct WaveData *wd = pa->pa_WD;
	struct WaveRuler r;
	struct IBox b;
	ULONG ms, f, last_ms;
	LONG x;

	if (!(wd->wd_Grid & WAVEFORM_GRID_TIME) || !wd->wd_Source || !wd->wd_MRP.Font) return;
	r.wru_Rate = wd->wd_Source->ws_Rate;
	r.wru_CharWidth = (UWORD) TextLength(&wd->wd_MRP, (STRPTR) "0", 1);
	r.wru_Gap = 12;
	if (!WavePlan_Ruler(&wd->wd_View, &r)) return;
	last_ms = WavePlan_FramesToMs(col_first(wd, to + 1), r.wru_Rate);
	ms = WavePlan_FramesToMs(col_first(wd, from), r.wru_Rate) / r.wru_MajorMs * r.wru_MajorMs;
	SetAPen(pa->pa_RP, (ULONG) pen(wd, WAVEFORM_COLOR_GRID));
	SetAfPt(pa->pa_RP, dash_pattern, WF_HATCH_LINES);
	for (; ms <= last_ms; ms += r.wru_MajorMs) {
		f = WavePlan_MsToFrames(ms, r.wru_Rate);
		if ((x = col_of(wd, f)) < from || x > to) continue;
		b.Left = (WORD) (PIX(pa) + x);
		b.Width = 1;
		b.Top = (WORD) PLY(pa);
		b.Height = wd->wd_LanesH;
		SetDrMd(pa->pa_RP, JAM1);
		fill(pa, &b);
	}
	SetAfPt(pa->pa_RP, NULL, 0);
}

/* the wave's spans going out: straight into the RastPort, a RectFill
   each, or into the mask -- only the element's being put in (the others
   noted in wo_Used for a walk of their own). A RectFill a column writes a
   whole word of every plane down the span: on a native screen (the
   A1200's) 0.6 to 0.9 s a view of 620 columns, the mask's blit a pen
   less than a tenth of it */
struct WaveOut {
	struct Paint *wo_Pa;
	UWORD *wo_Mask;			/* NULL: straight in */
	LONG wo_MX, wo_MY;		/* the mask's first column (on a word) and row */
	UWORD wo_WPR;			/* its words a row */
	UWORD wo_El;			/* the element being put in (the mask) */
	ULONG wo_Used;			/* the elements met, a bit each */
	LONG wo_X;			/* the column being walked (the RastPort's) ... */
	UWORD wo_ColEl;			/* ... and its element */
	LONG wo_Col;			/* ... the inner column ... */
	UWORD wo_Lane;			/* ... in this lane */
	UWORD wo_Prev;			/* the column before's line, WAVEPLAN_NOLINE none */
	LONG wo_PenSet;			/* the RastPort's pen (straight in): -1 none yet */
};

static void wave_span(struct WaveOut *wo, LONG y0, LONG y1)
{
	struct WaveData *wd = wo->wo_Pa->pa_WD;
	UWORD *p, bit, wpr = wo->wo_WPR;
	LONG n, pn;

	if (!wo->wo_Mask) {
		if ((pn = pen(wd, wo->wo_ColEl)) != wo->wo_PenSet) SetAPen(wo->wo_Pa->pa_RP, (ULONG) (wo->wo_PenSet = pn));
		RectFill(wo->wo_Pa->pa_RP, wo->wo_X, y0, wo->wo_X, y1);
		return;
	}
	wo->wo_Used |= 1UL << wo->wo_ColEl;
	if (wo->wo_ColEl != wo->wo_El) return;
	/* the lanes' rows only: the mask is as high as the buffer */
	if (y0 < wo->wo_MY) y0 = wo->wo_MY;
	if (y1 > wo->wo_MY + wd->wd_LanesH - 1) y1 = wo->wo_MY + wd->wd_LanesH - 1;
	if (y1 < y0) return;
	n = wo->wo_X - wo->wo_MX;
	p = wo->wo_Mask + (ULONG) (y0 - wo->wo_MY) * wpr + (ULONG) (n >> 4);
	bit = (UWORD) (0x8000U >> (n & 15));
	n = y1 - y0 + 1;
	do {
		*p |= bit;
		p += wpr;
	} while (--n);
}

/* the wave of column wo_Col in lane wo_Lane (ln): a span (an outline, a
   bar, as the style says) */
static void wave_column(struct WaveOut *wo, const struct Lane *ln)
{
	struct WaveData *wd = wo->wo_Pa->pa_WD;
	BYTE mm[2], pm[2];
	LONG c = wo->wo_Col, top, bot, ptop, pbot, foot, h;
	ULONG l = (ULONG) wo->wo_Lane << 16;
	UBYTE g = wd->wd_Ground[c];
	BOOL clip;

	lane_values(wd, l | (ULONG) c, mm);
	clip = (BOOL) (wd->wd_ClipMark && (mm[1] >= 127 || mm[0] <= -128));
	if (g == G_TRIM && wd->wd_TrimShade == WAVEFORM_SHADE_FLAT) {
		wo->wo_ColEl = WAVEFORM_COLOR_TRIMMEDWAVE;
		wave_span(wo, ln->ln_Mid, ln->ln_Mid);
		return;
	}
	if (g == G_SEL && wd->wd_SelectStyle == WAVEFORM_SELSTYLE_INVERT)
		wo->wo_ColEl = WAVEFORM_COLOR_SELECTION;
	else
		wo->wo_ColEl = clip ? WAVEFORM_COLOR_CLIP : wave_element(g);
	if (wd->wd_WaveStyle == WAVEFORM_WAVE_BARS) {
		h = WavePlan_Scale(&ln->ln_Scale, mm[1] > -mm[0] ? mm[1] : -mm[0]);
		if (h < 1) h = 1;
		foot = ln->ln_Y + ln->ln_H - 1;
		wave_span(wo, foot - h + 1, foot);
		return;
	}
	top = ln->ln_Mid - WavePlan_Scale(&ln->ln_Scale, mm[1]);
	bot = ln->ln_Mid - WavePlan_Scale(&ln->ln_Scale, mm[0]);
	if (bot < top) bot = top;
	if (wd->wd_WaveStyle == WAVEFORM_WAVE_OUTLINE) {
		if (c > 0 && c - 1 < (LONG) wd->wd_CacheCols && wd->wd_Ground[c - 1] != G_NONE &&
		    wd->wd_Ground[c - 1] != G_MISSING) {
			lane_values(wd, l | (ULONG) (c - 1), pm);
			ptop = ln->ln_Mid - WavePlan_Scale(&ln->ln_Scale, pm[1]);
			pbot = ln->ln_Mid - WavePlan_Scale(&ln->ln_Scale, pm[0]);
		} else {
			ptop = top;
			pbot = bot;
		}
		wave_span(wo, ptop < top ? ptop : top, ptop < top ? top : ptop);
		wave_span(wo, pbot < bot ? pbot : bot, pbot < bot ? bot : pbot);
	} else if (wd->wd_WaveStyle == WAVEFORM_WAVE_DOTS && bot > top) {
		wave_span(wo, top, top);
		wave_span(wo, bot, bot);
	} else {
		wave_span(wo, top, bot);
	}
}

/* a level as pixels from the lane's centre: as far as a sample at that
   level reaches */
static LONG line_offset(const struct Lane *ln, UWORD v)
{
	return WavePlan_Scale(&ln->ln_Scale, ((LONG) v * 127L + 2048L) >> 12);
}

/* a line's point at y, joined to the column before's at py (its own
   point not drawn again): no gaps where the line is steep */
static void line_point(struct WaveOut *wo, LONG y, LONG py)
{
	if (py < y - 1) wave_span(wo, py + 1, y);
	else if (py > y + 1) wave_span(wo, y, py - 1);
	else wave_span(wo, y, y);
}

/* over column wo_Col's wave in lane ln: a fade's line, level v (as far
   from the centre as a sample at that level reaches, either side; a
   bar's top), joined to the column before's, and in the last lane a
   gain's bar along the foot */
static void level_column(struct WaveOut *wo, const struct Lane *ln, UWORD v, UWORD gain)
{
	struct WaveData *wd = wo->wo_Pa->pa_WD;
	LONG d, p, foot = ln->ln_Y + ln->ln_H - 1;

	wo->wo_ColEl = WAVEFORM_COLOR_LEVEL;
	if (v != WAVEPLAN_NOLINE) {
		d = line_offset(ln, v);
		p = wo->wo_Prev != WAVEPLAN_NOLINE ? line_offset(ln, wo->wo_Prev) : d;
		if (wd->wd_WaveStyle == WAVEFORM_WAVE_BARS) {
			line_point(wo, foot - (d > 0 ? d - 1 : 0), foot - (p > 0 ? p - 1 : 0));
		} else {
			line_point(wo, ln->ln_Mid - d, ln->ln_Mid - p);
			if (d > 0 || p > 0) line_point(wo, ln->ln_Mid + d, ln->ln_Mid + p);
		}
	}
	if (gain != WAVELEVEL_UNITY && wo->wo_Lane == lanes_of(wd) - 1 && ln->ln_H >= 4)
		wave_span(wo, ln->ln_Y + ln->ln_H - 2, ln->ln_Y + ln->ln_H - 1);
}

/* the sound in columns from..to (span: from << 16 | to): a column and
   lane at a time, the level over the wave */
static void walk_waves(struct WaveOut *wo, ULONG span)
{
	struct Paint *pa = wo->wo_Pa;
	struct WaveData *wd = pa->pa_WD;
	struct Lane ln;
	struct WaveLevelWalk w;
	LONG c, from = (LONG) (span >> 16), to = (LONG) (span & 0xFFFF);
	UWORD l, v, lanes = lanes_of(wd);
	UBYTE g;
	BOOL levels = has_levels(wd);

	for (l = 0; l < lanes; l++) {
		lane_box(pa, l, &ln);
		wo->wo_Lane = l;
		wo->wo_Prev = WAVEPLAN_NOLINE;
		if (levels) {
			level_walk(wd, &w);
			/* the line joins the column before the span's */
			if (from > 0) wo->wo_Prev = col_line(wd, &w, from - 1);
		}
		for (c = from; c <= to; c++) {
			g = wd->wd_Ground[c];
			if (g == G_NONE || g == G_MISSING) {
				wo->wo_Prev = WAVEPLAN_NOLINE;
				continue;
			}
			wo->wo_X = PIX(pa) + c;
			wo->wo_Col = c;
			wave_column(wo, &ln);
			if (!levels) continue;
			v = col_line(wd, &w, c);
			if (v != WAVEPLAN_NOLINE || w.lw_Gain != WAVELEVEL_UNITY) level_column(wo, &ln, v, w.lw_Gain);
			wo->wo_Prev = v;
		}
	}
}

/* the mask cleared for a walk: the span's words, the lanes' rows */
static void mask_clear(struct WaveOut *wo, UWORD words)
{
	UWORD *p = wo->wo_Mask, wpr = wo->wo_WPR, k;
	WORD r = wo->wo_Pa->pa_WD->wd_LanesH;

	while (r-- > 0) {
		for (k = 0; k < words; k++) p[k] = 0;
		p += wpr;
	}
}

/* the sound in columns from..to: into the buffer through the mask when
   it has one, an element a walk -- the bits drawn, copied into chip RAM
   and cut into the planes in the element's pen at once -- else straight */
static void paint_waves(struct Paint *pa, LONG from, LONG to)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct WaveOut wo;
	ULONG span = ((ULONG) from << 16) | (ULONG) to, left;
	UWORD words, el, r;
	LONG x0;

	SetDrMd(rp, JAM1);
	wo.wo_Pa = pa;
	wo.wo_Mask = NULL;
	wo.wo_Used = 0;
	wo.wo_PenSet = -1;
	x0 = PIX(pa) + from;
	wo.wo_MX = x0 & ~15L;
	wo.wo_MY = PLY(pa);
	words = (UWORD) (((PIX(pa) + to - wo.wo_MX) >> 4) + 1);
	if (!wd->wd_Mask || rp != &wd->wd_BufRP || from > to || words > wd->wd_MaskWPR || wd->wd_LanesH > wd->wd_BufH) {
		walk_waves(&wo, span);
		return;
	}
	wo.wo_Mask = wd->wd_Mask;
	wo.wo_WPR = wd->wd_MaskWPR;
	/* the wave's own pen first, the others it met after it */
	left = 1UL << WAVEFORM_COLOR_WAVE;
	for (el = WAVEFORM_COLOR_WAVE; left; el = (UWORD) ((el + 1) % WAVEFORM_COLOR_COUNT)) {
		if (!(left & (1UL << el))) continue;
		left &= ~(1UL << el);
		wo.wo_El = el;
		mask_clear(&wo, words);
		wo.wo_Used = 0;
		walk_waves(&wo, span);
		if (el == WAVEFORM_COLOR_WAVE) left = wo.wo_Used & ~(1UL << WAVEFORM_COLOR_WAVE);
		if (!(wo.wo_Used & (1UL << el))) continue;
		/* the blitter may still read the last element's */
		WaitBlit();
		for (r = 0; r < (UWORD) wd->wd_LanesH; r++)
			CopyMem((APTR) (wd->wd_Mask + (ULONG) r * wo.wo_WPR), (APTR) (wd->wd_MaskChip + (ULONG) r * wo.wo_WPR),
				(ULONG) words * 2);
		SetAPen(rp, (ULONG) pen(wd, el));
		BltTemplate((PLANEPTR) wd->wd_MaskChip, x0 - wo.wo_MX, (LONG) wo.wo_WPR * 2, rp, x0, wo.wo_MY, to - from + 1,
			    (LONG) wd->wd_LanesH);
	}
}

/* ---- the overlays ---- */

static WORD font_height(struct WaveData *wd)
{
	return (WORD) (wd->wd_Font ? wd->wd_Font->tf_YSize : 8);
}

static WORD text_width(struct WaveData *wd, CONST_STRPTR s)
{
	UWORD n = 0;

	if (!s || !wd->wd_MRP.Font) return 0;
	while (s[n]) n++;
	return (WORD) TextLength(&wd->wd_MRP, (STRPTR) s, n);
}

static WORD flag_width(struct WaveData *wd, BOOL in)
{
	CONST_STRPTR label = wd->wd_HandleLabels ? wd->wd_HandleLabels[in ? 0 : 1] : (CONST_STRPTR) (in ? "In" : "Out");

	return (WORD) (text_width(wd, label) + 6);
}

static void add_overlay(struct WaveData *wd, struct Overlay *o)
{
	if (wd->wd_OvCount >= MAX_OVERLAYS || o->ov_X1 < o->ov_X0) return;
	wd->wd_Ov[wd->wd_OvCount++] = *o;
}

/* every overlay of the view into wd_Ov, in drawing order: how many */
static UWORD plan_overlays(struct WaveData *wd)
{
	struct Overlay o, *ov = wd->wd_Ov;
	UWORD n, i;
	LONG x, w;
	ULONG f;

	wd->wd_OvCount = 0;
	/* region ticks (clicks found), under everything else; ticks and
	   markers leave OV_RESERVED places for what must always show */
	for (i = 0; i < wd->wd_RegionCount && wd->wd_Regions && wd->wd_OvCount < MAX_OVERLAYS - OV_RESERVED; i++) {
		struct WaveRegion *rg = &wd->wd_Regions[i];

		if (rg->wrg_Style != WAVEREGION_TICK && rg->wrg_Style != WAVEREGION_BAR) continue;
		x = col_of(wd, rg->wrg_First);
		/* a bar begun before the view and going on into it: from its
		   first column */
		if (x < 0 && rg->wrg_Style == WAVEREGION_BAR && rg->wrg_First < wd->wd_View.wv_Top && rg->wrg_End > wd->wd_View.wv_Top) x = 0;
		if (x < 0) continue;
		o.ov_Kind = OV_TICK;
		o.ov_Index = i;
		o.ov_X = x;
		o.ov_X0 = x > 0 ? x - 1 : 0;
		o.ov_X1 = x + 1;
		if (rg->wrg_Style == WAVEREGION_BAR) {
			/* to its last frame's column (past the view: its edge); an
			   empty one its first column only */
			if (rg->wrg_End <= rg->wrg_First) w = x;
			else if ((w = col_of(wd, rg->wrg_End - 1)) < 0) w = wd->wd_IW - 1;
			o.ov_X1 = w < x ? x : w;
		}
		add_overlay(wd, &o);
	}
	/* the levels' tags where each starts (begun before the view: at its
	   left edge), over the gain's bar */
	for (i = 0; wd->wd_LevelLine && i < wd->wd_LevelCount && wd->wd_Levels && wd->wd_OvCount < MAX_OVERLAYS - OV_RESERVED; i++) {
		struct WaveLevel *lv = &wd->wd_Levels[i];

		if (!lv->wlv_Label || lv->wlv_End <= lv->wlv_First) continue;
		x = col_of(wd, lv->wlv_First);
		if (x < 0 && lv->wlv_First < wd->wd_View.wv_Top && lv->wlv_End > wd->wd_View.wv_Top) x = 0;
		if (x < 0) continue;
		o.ov_Kind = OV_TAG;
		o.ov_Index = i;
		o.ov_X = o.ov_X0 = x;
		o.ov_X1 = x + text_width(wd, lv->wlv_Label) + 5;
		add_overlay(wd, &o);
	}
	if (wd->wd_SelectStyle == WAVEFORM_SELSTYLE_FRAME && wd->wd_SelEnd > wd->wd_SelStart && wd->wd_SelEnd != WAVEFORM_NONE) {
		LONG a = col_of(wd, wd->wd_SelStart), b = col_of(wd, wd->wd_SelEnd - 1);

		if (a < 0 && wd->wd_SelStart < wd->wd_View.wv_Top) a = 0;
		if (b < 0 && wd->wd_SelEnd > wd->wd_View.wv_Top) b = wd->wd_IW - 1;
		if (a >= 0 && b >= a) {
			o.ov_Kind = OV_SELFRAME;
			o.ov_Index = 0;
			o.ov_X = a;
			o.ov_X0 = a;
			o.ov_X1 = b;
			add_overlay(wd, &o);
		}
	}
	for (i = 0; i < wd->wd_MarkerCount && wd->wd_Markers && wd->wd_OvCount < MAX_OVERLAYS - OV_RESERVED; i++) {
		if (wd->wd_Markers[i].wm_Flags & WAVEMARKER_HIDDEN) continue;
		f = (wd->wd_Drag == DRAG_MARKER && wd->wd_ActiveMarker == (LONG) i) ? wd->wd_ActiveFrame : wd->wd_Markers[i].wm_Frame;
		if ((x = col_of(wd, f)) < 0) continue;
		o.ov_Kind = OV_MARKER;
		o.ov_Index = i;
		o.ov_X = x;
		o.ov_X0 = x > 3 ? x - 3 : 0;
		w = (wd->wd_Markers[i].wm_Label && !(wd->wd_Markers[i].wm_Flags & WAVEMARKER_NOLABEL)) ?
		    text_width(wd, wd->wd_Markers[i].wm_Label) + 5 : 3;
		o.ov_X1 = x + w;
		add_overlay(wd, &o);
	}
	if (wd->wd_TrimHandles != WAVEFORM_HANDLES_NONE) {
		for (i = 0; i < 2; i++) {
			f = i ? wd->wd_TrimOut : wd->wd_TrimIn;
			if (f == WAVEFORM_NONE) continue;
			/* Out sits on the last kept frame's column */
			x = col_of(wd, i && f > 0 ? f - 1 : f);
			if (x < 0) continue;
			o.ov_Kind = i ? OV_OUT : OV_IN;
			o.ov_Index = i;
			o.ov_X = x;
			w = wd->wd_HandleStyle == WAVEFORM_HANDLE_FLAG ? flag_width(wd, (BOOL) !i) :
			    wd->wd_HandleStyle == WAVEFORM_HANDLE_BRACKET ? 6 : wd->wd_HandleStyle == WAVEFORM_HANDLE_TRIANGLE ? 3 : 0;
			if (wd->wd_HandleStyle == WAVEFORM_HANDLE_TRIANGLE) {
				o.ov_X0 = x - 3;
				o.ov_X1 = x + 3;
			} else if (i == 0) {
				o.ov_X0 = x;
				o.ov_X1 = x + w;
			} else {
				o.ov_X0 = x - w;
				o.ov_X1 = x;
			}
			if (o.ov_X0 < 0) o.ov_X0 = 0;
			add_overlay(wd, &o);
		}
	}
	if (wd->wd_Cursor != WAVEFORM_NONE && (x = col_of(wd, wd->wd_Cursor)) >= 0) {
		o.ov_Kind = OV_CURSOR;
		o.ov_Index = 0;
		o.ov_X = o.ov_X0 = o.ov_X1 = x;
		add_overlay(wd, &o);
	}
	if (wd->wd_Overview && wd->wd_BoxVisible) {
		LONG a = col_of(wd, wd->wd_BoxTop), b;

		f = wd->wd_BoxTop + wd->wd_BoxVisible;
		b = col_of(wd, f > 0 ? f - 1 : 0);
		if (a < 0) a = 0;
		if (b < 0) b = wd->wd_IW - 1;
		if (b < a + 2) b = a + 2;
		o.ov_Kind = OV_BOX;
		o.ov_Index = 0;
		o.ov_X = a;
		o.ov_X0 = a > 0 ? a - 1 : 0;
		o.ov_X1 = b + 1;
		add_overlay(wd, &o);
	}
	/* the play head: an overlay of its columns only without the buffer
	   (with it, it floats: head_show) */
	if (!wd->wd_BufRP.BitMap && !wd->wd_HeadOff && wd->wd_Position != WAVEFORM_NONE && (x = col_of(wd, wd->wd_Position)) >= 0) {
		o.ov_Kind = OV_PLAY;
		o.ov_Index = 0;
		o.ov_X = x;
		o.ov_X0 = x > 3 ? x - 3 : 0;
		o.ov_X1 = x + 3;
		add_overlay(wd, &o);
	}
	/* nothing beyond the view (the box's ov_X1 is the column after its
	   right edge: the view's width at most) */
	n = wd->wd_OvCount;
	for (i = 0; i < n; i++) {
		if (ov[i].ov_Kind == OV_BOX) {
			if (ov[i].ov_X1 > wd->wd_IW) ov[i].ov_X1 = wd->wd_IW;
		} else if (ov[i].ov_X1 >= wd->wd_IW) {
			ov[i].ov_X1 = wd->wd_IW - 1;
		}
	}
	return n;
}

/* a small triangle, 4 lines, pointing down (top) or up (foot) */
static void triangle(struct Paint *pa, LONG x, LONG y)
{
	struct WaveData *wd = pa->pa_WD;
	WORD i, w;
	BOOL down = (BOOL) (y >= 0);
	LONG yy, ix = PIX(pa);

	if (!down) y = -y;
	for (i = 0; i < 4; i++) {
		w = (WORD) (3 - i);
		yy = down ? y + i : y + 3 - i;
		if (x - w < ix || x + w >= ix + wd->wd_IW) {
			LONG a = x - w < ix ? ix : x - w, b = x + w >= ix + wd->wd_IW ? ix + wd->wd_IW - 1 : x + w;

			if (b >= a) RectFill(pa->pa_RP, a, yy, b, yy);
		} else {
			RectFill(pa->pa_RP, x - w, yy, x + w, yy);
		}
	}
}

static void text_at(struct Paint *pa, LONG x, LONG y, CONST_STRPTR s)
{
	UWORD n = 0;

	if (!s) return;
	while (s[n]) n++;
	Move(pa->pa_RP, x, y + pa->pa_RP->TxBaseline);
	Text(pa->pa_RP, (STRPTR) s, n);
}

static void paint_handle(struct Paint *pa, struct Overlay *o)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	BOOL in = (BOOL) (o->ov_Kind == OV_IN);
	LONG ix = PIX(pa), x = ix + o->ov_X, y0 = PLY(pa), y1 = y0 + wd->wd_LanesH - 1, fw, fx, fh;
	CONST_STRPTR label;
	struct Rectangle r;

	SetDrMd(rp, JAM1);
	SetAPen(rp, (ULONG) pen(wd, in ? WAVEFORM_COLOR_INHANDLE : WAVEFORM_COLOR_OUTHANDLE));
	switch (wd->wd_HandleStyle) {
	case WAVEFORM_HANDLE_BRACKET:
		/* its arms clipped: a handle at the view's edge reaches past it */
		r.MinX = (WORD) (in ? x : x - 1);
		r.MaxX = (WORD) (in ? x + 1 : x);
		r.MinY = (WORD) y0;
		r.MaxY = (WORD) y1;
		lane_fill(pa, &r);
		r.MinX = (WORD) (in ? x : x - 5);
		r.MaxX = (WORD) (in ? x + 5 : x);
		r.MaxY = (WORD) (y0 + 1);
		lane_fill(pa, &r);
		r.MinY = (WORD) (y1 - 1);
		r.MaxY = (WORD) y1;
		lane_fill(pa, &r);
		break;
	case WAVEFORM_HANDLE_TRIANGLE:
		RectFill(rp, x, y0, x, y1);
		triangle(pa, x, y0);
		triangle(pa, x, -(y1 - 3));
		break;
	case WAVEFORM_HANDLE_LINE:
		RectFill(rp, x, y0, x, y1);
		break;
	default:
		RectFill(rp, x, y0, x, y1);
		label = wd->wd_HandleLabels ? wd->wd_HandleLabels[in ? 0 : 1] : (CONST_STRPTR) (in ? "In" : "Out");
		fw = flag_width(wd, in);
		fh = wd->wd_HandleSize ? wd->wd_HandleSize : font_height(wd) + 2;
		if (fh > wd->wd_LanesH) fh = wd->wd_LanesH;
		fx = in ? x + 1 : x - fw;
		/* cut at the view's edges: Out's flag at the left one ends at
		   its line (the columns its overlay covers), not past it */
		if (fx < ix) {
			fw -= ix - fx;
			fx = ix;
		}
		if (fx + fw > ix + wd->wd_IW) fw = ix + wd->wd_IW - fx;
		if (fw <= 0) break;
		RectFill(rp, fx, y0, fx + fw - 1, y0 + fh - 1);
		if (wd->wd_UsePatterns) {
			SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_CURSOR));
			RectFill(rp, fx, y0 + fh - 1, fx + fw - 1, y0 + fh - 1);
			RectFill(rp, in ? fx + fw - 1 : fx, y0, in ? fx + fw - 1 : fx, y0 + fh - 1);
		}
		SetAPen(rp, (ULONG) flag_text_pen(wd, in));
		/* the label where it fits the flag both ways: a text past the
		   lanes' foot would go past a buffer's last line */
		if (fw >= text_width(wd, label) + 3 && font_height(wd) + 1 <= wd->wd_LanesH) text_at(pa, fx + 3, y0 + 1, label);
		break;
	}
}

static void paint_marker(struct Paint *pa, struct Overlay *o)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct WaveMarker *m = &wd->wd_Markers[o->ov_Index];
	struct IBox b;
	LONG x = PIX(pa) + o->ov_X, ly = PLY(pa);

	SetDrMd(rp, JAM1);
	SetAPen(rp, (ULONG) (m->wm_Pen >= 0 ? m->wm_Pen : pen(wd, WAVEFORM_COLOR_MARKER)));
	b.Left = (WORD) x;
	b.Width = 1;
	b.Top = (WORD) ly;
	b.Height = wd->wd_LanesH;
	SetAfPt(rp, dash_pattern, WF_HATCH_LINES);
	pattern_fill(pa, &b);
	if (wd->wd_MarkerStyle == WAVEFORM_HANDLE_TRIANGLE || wd->wd_MarkerStyle == WAVEFORM_HANDLE_FLAG) {
		triangle(pa, x, ly);
		triangle(pa, x, -(ly + wd->wd_LanesH - 4));
	}
	/* the label where it fits the view both ways */
	if (m->wm_Label && !(m->wm_Flags & WAVEMARKER_NOLABEL) &&
	    x + 4 + text_width(wd, m->wm_Label) < PIX(pa) + wd->wd_IW && font_height(wd) + 1 <= wd->wd_LanesH)
		text_at(pa, x + 5, ly + 1, m->wm_Label);
}

/* a level's tag: its label in a box of the level's colour on the gain's
   bar, where it fits the lanes and the view whole */
static void paint_tag(struct Paint *pa, struct Overlay *o)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	CONST_STRPTR label = wd->wd_Levels[o->ov_Index].wlv_Label;
	struct Rectangle r;
	LONG x = PIX(pa) + o->ov_X, y1 = PLY(pa) + wd->wd_LanesH - 1, fh = font_height(wd) + 2;
	LONG tw = text_width(wd, label) + 6;

	if (fh + 2 > wd->wd_LanesH || o->ov_X + tw > wd->wd_IW) return;
	SetDrMd(rp, JAM1);
	SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_LEVEL));
	r.MinX = (WORD) x;
	r.MaxX = (WORD) (x + tw - 1);
	r.MinY = (WORD) (y1 - fh);
	r.MaxY = (WORD) (y1 - 1);
	lane_fill(pa, &r);
	SetAPen(rp, (ULONG) tag_text_pen(wd));
	text_at(pa, x + 3, y1 - fh + 1, label);
}

static void paint_overlay(struct Paint *pa, struct Overlay *o)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct Lane ln;
	struct IBox b;
	struct Rectangle r;
	LONG ix = PIX(pa), x = ix + o->ov_X, y0 = PLY(pa), y1 = y0 + wd->wd_LanesH - 1;
	UWORD l;

	SetDrMd(rp, JAM1);
	switch (o->ov_Kind) {
	case OV_TICK:
		SetAPen(rp, (ULONG) (wd->wd_Regions[o->ov_Index].wrg_Pen >= 0 ? wd->wd_Regions[o->ov_Index].wrg_Pen :
				     pen(wd, WAVEFORM_COLOR_CLICK)));
		for (l = 0; l < lanes_of(wd); l++) {
			lane_box(pa, l, &ln);
			r.MaxY = (WORD) (ln.ln_Y + ln.ln_H - 1);
			if (wd->wd_Regions[o->ov_Index].wrg_Style == WAVEREGION_BAR) {
				r.MinX = (WORD) (ix + o->ov_X0 + (o->ov_X0 < o->ov_X ? 1 : 0));
				r.MaxX = (WORD) (ix + o->ov_X1);
				r.MinY = (WORD) (r.MaxY - 1);
				lane_fill(pa, &r);
			} else {
				r.MinX = r.MaxX = (WORD) x;
				r.MinY = (WORD) (r.MaxY - 2);
				lane_fill(pa, &r);
				r.MinX = (WORD) (ix + o->ov_X0);
				r.MaxX = (WORD) (ix + o->ov_X1);
				r.MinY = r.MaxY;
				lane_fill(pa, &r);
			}
		}
		break;
	case OV_SELFRAME:
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_CURSOR));
		RectFill(rp, x, y0, ix + o->ov_X1, y0);
		RectFill(rp, x, y1, ix + o->ov_X1, y1);
		break;
	case OV_MARKER:
		paint_marker(pa, o);
		break;
	case OV_TAG:
		paint_tag(pa, o);
		break;
	case OV_IN:
	case OV_OUT:
		paint_handle(pa, o);
		break;
	case OV_CURSOR:
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_CURSOR));
		if (wd->wd_CursorStyle == WAVEFORM_CURSOR_DASHED) {
			b.Left = (WORD) x;
			b.Width = 1;
			b.Top = (WORD) y0;
			b.Height = wd->wd_LanesH;
			SetAfPt(rp, dash_pattern, WF_HATCH_LINES);
			pattern_fill(pa, &b);
		} else {
			RectFill(rp, x, y0, x, y1);
		}
		break;
	case OV_BOX:
		/* its right edge the column before ov_X1 (ov_X1 at most the
		   view's width for the box): a box in the last column is a line,
		   never a fill from right to left */
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_BOX));
		r.MinX = (WORD) x;
		r.MaxX = (WORD) (ix + o->ov_X1 - 1 > x ? ix + o->ov_X1 - 1 : x);
		r.MinY = r.MaxY = (WORD) y0;
		lane_fill(pa, &r);
		r.MinY = r.MaxY = (WORD) y1;
		lane_fill(pa, &r);
		r.MinY = (WORD) y0;
		r.MaxX = r.MinX;
		lane_fill(pa, &r);
		r.MinX = r.MaxX = (WORD) (ix + o->ov_X1 - 1 > x ? ix + o->ov_X1 - 1 : x);
		lane_fill(pa, &r);
		break;
	case OV_PLAY:
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_POSITION));
		RectFill(rp, x, y0, wd->wd_PositionStyle == WAVEFORM_POSITION_BLOCK && x + 1 < ix + wd->wd_IW ? x + 1 : x, y1);
		if (wd->wd_PositionStyle == WAVEFORM_POSITION_TRIANGLE) triangle(pa, x, y0);
		break;
	}
}

/* ---- the ruler and the frame ---- */

static ULONG ruler_sig(struct WaveData *wd)
{
	ULONG s = WAVEPLAN_SIG_SEED;

	s = WavePlan_Sig(s, &wd->wd_View.wv_Top, sizeof(ULONG));
	s = WavePlan_Sig(s, &wd->wd_View.wv_Zoom, sizeof(ULONG));
	s = WavePlan_Sig(s, &wd->wd_TimeOrigin, sizeof(ULONG));
	s = WavePlan_Sig(s, &wd->wd_IW, sizeof(WORD));
	/* the rate places the ticks: another source, another ruler */
	if (wd->wd_Source) s = WavePlan_Sig(s, &wd->wd_Source->ws_Rate, sizeof(ULONG));
	return s;
}

static void paint_ruler(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct WaveRuler r;
	struct WaveClock ck;
	struct IBox b;
	char buf[WAVEPLAN_TIMELEN];
	ULONG ms, f, last_ms, mf;
	LONG x, label_end = -1, y, line, tick;
	UWORD j, n;
	BOOL top = (BOOL) (wd->wd_Ruler == WAVEFORM_RULER_TOP);

	if (!wd->wd_RulerH) return;
	SetDrMd(rp, JAM1);
	SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_RULERBACK));
	b.Left = wd->wd_IX;
	b.Top = wd->wd_RulerY;
	b.Width = wd->wd_IW;
	b.Height = wd->wd_RulerH;
	box_fill(pa, &b);
	SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_RULERTEXT));
	line = top ? wd->wd_RulerY + wd->wd_RulerH - 1 : wd->wd_RulerY;
	RectFill(rp, wd->wd_IX, line, wd->wd_IX + wd->wd_IW - 1, line);
	wd->wd_RulerSig = ruler_sig(wd);
	if (!wd->wd_Source || !wd->wd_Source->ws_Rate || !wd->wd_MRP.Font) return;
	r.wru_Rate = wd->wd_Source->ws_Rate;
	r.wru_CharWidth = (UWORD) TextLength(&wd->wd_MRP, (STRPTR) "0", 1);
	if (TextLength(&wd->wd_MRP, (STRPTR) ":", 1) > r.wru_CharWidth) r.wru_CharWidth = (UWORD) TextLength(&wd->wd_MRP, (STRPTR) ":", 1);
	r.wru_Gap = 12;
	if (!WavePlan_Ruler(&wd->wd_View, &r)) return;
	ck.wc_Rate = r.wru_Rate;
	ck.wc_Origin = wd->wd_TimeOrigin;
	ck.wc_Format = wd->wd_TimeFormat;
	ck.wc_Decimals = r.wru_Decimals;
	last_ms = WavePlan_FramesToMs(col_first(wd, wd->wd_IW), r.wru_Rate);
	ms = WavePlan_FramesToMs(wd->wd_View.wv_Top, r.wru_Rate) / r.wru_MajorMs * r.wru_MajorMs;
	tick = wd->wd_RulerH > 4 ? 3 : 1;
	y = top ? wd->wd_RulerY : wd->wd_RulerY + 1;
	for (; ms <= last_ms; ms += r.wru_MajorMs) {
		f = WavePlan_MsToFrames(ms, r.wru_Rate);
		for (j = 0; j < r.wru_Div; j++) {
			mf = f + (ULONG) j * r.wru_Minor;
			if ((x = col_of(wd, mf)) < 0) continue;
			x += wd->wd_IX;
			if (j == 0) {
				RectFill(rp, x, wd->wd_RulerY, x, wd->wd_RulerY + wd->wd_RulerH - 1);
				n = WavePlan_Time(&ck, mf, buf);
				if (x + 3 + TextLength(&wd->wd_MRP, (STRPTR) buf, n) < wd->wd_IX + wd->wd_IW) {
					Move(rp, x + 3, y + rp->TxBaseline);
					Text(rp, (STRPTR) buf, n);
					label_end = x + 3 + TextLength(&wd->wd_MRP, (STRPTR) buf, n);
				}
			} else if (x > label_end + 1) {
				RectFill(rp, x, top ? line - tick : line + 1, x, top ? line - 1 : line + tick);
			}
		}
	}
}

static void paint_frame(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	struct impDraw imp;

	if (!wd->wd_Bevel || !wd->wd_Gad) return;
	imp.MethodID = IM_DRAWFRAME;
	imp.imp_RPort = pa->pa_RP;
	imp.imp_Offset.X = wd->wd_Gad->LeftEdge;
	imp.imp_Offset.Y = wd->wd_Gad->TopEdge;
	imp.imp_State = IDS_NORMAL;
	imp.imp_DrInfo = wd->wd_Dri;
	imp.imp_Dimensions.Width = wd->wd_Gad->Width;
	imp.imp_Dimensions.Height = wd->wd_Gad->Height;
	DoMethodA(wd->wd_Bevel, (Msg) &imp);
}

/* ---- signatures and the paint itself ---- */

/* one byte into a signature as WavePlan_Sig() takes it, inline: a read
   signs every column at every per cent */
#define SIG_BYTE(h, v)	((h) ^= (UBYTE) (v), (h) += ((h) << 1) + ((h) << 4) + ((h) << 7) + ((h) << 8) + ((h) << 24))

/* the time grid's columns into their signatures: the grid moves with
   the rate, not only with the values under it */
static void sig_grid(struct WaveData *wd, ULONG span)
{
	struct WaveRuler r;
	ULONG ms, last_ms, key = 0x47524944UL;	/* "GRID" */
	LONG x, c0 = (LONG) (span >> 16), c1 = (LONG) (span & 0xFFFF);

	if (!(wd->wd_Grid & WAVEFORM_GRID_TIME) || !wd->wd_Source || !wd->wd_MRP.Font || !wd->wd_CacheCols) return;
	r.wru_Rate = wd->wd_Source->ws_Rate;
	r.wru_CharWidth = (UWORD) TextLength(&wd->wd_MRP, (STRPTR) "0", 1);
	r.wru_Gap = 12;
	if (!WavePlan_Ruler(&wd->wd_View, &r)) return;
	last_ms = WavePlan_FramesToMs(col_first(wd, (LONG) wd->wd_CacheCols), r.wru_Rate);
	for (ms = WavePlan_FramesToMs(col_first(wd, 0), r.wru_Rate) / r.wru_MajorMs * r.wru_MajorMs; ms <= last_ms; ms += r.wru_MajorMs) {
		x = col_of(wd, WavePlan_MsToFrames(ms, r.wru_Rate));
		if (x >= c0 && x <= c1 && x < (LONG) wd->wd_CacheCols) wd->wd_NewSig[x] = WavePlan_Sig(wd->wd_NewSig[x], &key, sizeof(key));
	}
}

/* what an overlay looks like besides its place: a marker's or a tick's
   pen, a marker's label (its text, which the program may change in the
   same buffer) */
static ULONG overlay_key(struct WaveData *wd, const struct Overlay *o)
{
	ULONG key = ((ULONG) o->ov_Kind << 24) ^ ((ULONG) o->ov_Index << 16) ^ (ULONG) o->ov_X;
	const struct WaveMarker *m;
	CONST_STRPTR t;

	if (o->ov_Kind == OV_MARKER && wd->wd_Markers) {
		m = &wd->wd_Markers[o->ov_Index];
		key = WavePlan_Sig(key, &m->wm_Pen, sizeof(m->wm_Pen));
		key = WavePlan_Sig(key, &m->wm_Flags, sizeof(m->wm_Flags));
		for (t = m->wm_Label; t && *t; t++) SIG_BYTE(key, *t);
	} else if (o->ov_Kind == OV_TICK && wd->wd_Regions) {
		key = WavePlan_Sig(key, &wd->wd_Regions[o->ov_Index].wrg_Pen, sizeof(wd->wd_Regions[o->ov_Index].wrg_Pen));
	} else if (o->ov_Kind == OV_TAG && wd->wd_Levels) {
		for (t = wd->wd_Levels[o->ov_Index].wlv_Label; t && *t; t++) SIG_BYTE(key, *t);
	}
	return key;
}

/* the columns' signatures as they are now, c0 .. c1 (span): their
   values, ground, grid, overlays -- as the whole view's would be (the
   column before's line taken as it is) */
static void plan_sigs_span(struct WaveData *wd, struct Overlay *ov, UWORD n, ULONG span)
{
	struct WaveLevelWalk w;
	UWORD c = (UWORD) (span >> 16), c1 = (UWORD) (span & 0xFFFF), i, k, v, pv = WAVEPLAN_NOLINE, ch = channels_of(wd),
	      bytes = (UWORD) (ch * 2);
	LONG x;
	ULONG s, key;
	const BYTE *p = wd->wd_Cache + (ULONG) c * bytes;
	BOOL levels = has_levels(wd);

	if (c1 >= wd->wd_CacheCols) c1 = (UWORD) (wd->wd_CacheCols - 1);
	if (!wd->wd_CacheCols || c > c1) return;
	if (levels) {
		level_walk(wd, &w);
		if (c) pv = col_line(wd, &w, (LONG) c - 1);
	}
	for (; c <= c1; c++, p += bytes) {
		s = WAVEPLAN_SIG_SEED;
		for (k = 0; k < bytes; k++) SIG_BYTE(s, p[k]);
		SIG_BYTE(s, wd->wd_Ground[c]);
		if (levels) {
			/* the column before's line too: the join is drawn in this one */
			SIG_BYTE(s, pv);
			SIG_BYTE(s, pv >> 8);
			v = col_line(wd, &w, (LONG) c);
			SIG_BYTE(s, v);
			SIG_BYTE(s, v >> 8);
			SIG_BYTE(s, w.lw_Gain != WAVELEVEL_UNITY);
			pv = v;
		}
		wd->wd_NewSig[c] = s;
	}
	sig_grid(wd, span);
	for (i = 0; i < n; i++) {
		key = overlay_key(wd, &ov[i]);
		for (x = ov[i].ov_X0; x <= ov[i].ov_X1 && x <= (LONG) c1; x++) {
			if (x >= (LONG) (span >> 16)) wd->wd_NewSig[x] = WavePlan_Sig(wd->wd_NewSig[x], &key, sizeof(key));
		}
	}
}

/* every column's signature */
static void plan_sigs(struct WaveData *wd, struct Overlay *ov, UWORD n)
{
	if (wd->wd_CacheCols) plan_sigs_span(wd, ov, n, (ULONG) (wd->wd_CacheCols - 1));
}

/* columns from..to, and every overlay over them */
static void paint_span(struct Paint *pa, struct Overlay *ov, ULONG span)
{
	LONG from = (LONG) (span >> 16), to = (LONG) (span & 0xFFFF);
	UWORD i;

	paint_grounds(pa, from, to);
	paint_grid(pa, from, to);
	paint_waves(pa, from, to);
	for (i = 0; i < MAX_OVERLAYS && ov[i].ov_Kind != 0; i++) {
		if (ov[i].ov_X1 < from || ov[i].ov_X0 > to) continue;
		paint_overlay(pa, &ov[i]);
	}
}

/* the next page's copy and what it holds gone (the blitter done first) */
static void free_next(struct WaveData *wd)
{
	struct WaveData *nx = wd->wd_Next;

	if (!nx) return;
	WaitBlit();
	if (nx->wd_BufRP.BitMap) FreeBitMap(nx->wd_BufRP.BitMap);
	if (nx->wd_Mask) FreeVec(nx->wd_Mask);
	if (nx->wd_MaskChip) FreeVec(nx->wd_MaskChip);
	if (nx->wd_Cache) FreeVec(nx->wd_Sig);
	FreeVec(nx);
	wd->wd_Next = NULL;
}

/* the lanes' buffer and the head's gone (the blitter done with them
   first), the next page's with them */
static void free_buffer(struct WaveData *wd)
{
	free_next(wd);
	if (!wd->wd_BufRP.BitMap && !wd->wd_HeadRP.BitMap && !wd->wd_Mask) return;
	WaitBlit();
	if (wd->wd_BufRP.BitMap) FreeBitMap(wd->wd_BufRP.BitMap);
	wd->wd_BufRP.BitMap = NULL;
	if (wd->wd_HeadRP.BitMap) FreeBitMap(wd->wd_HeadRP.BitMap);
	wd->wd_HeadRP.BitMap = NULL;
	if (wd->wd_Mask) FreeVec(wd->wd_Mask);
	if (wd->wd_MaskChip) FreeVec(wd->wd_MaskChip);
	wd->wd_Mask = wd->wd_MaskChip = NULL;
	wd->wd_MaskWPR = 0;
}

/* the lanes' buffer for rp's bitmap (its friend: the same format), from a
   point on a 16-pixel grid -- an area pattern keeps its phase -- to the
   lanes' far corner: FALSE when there is none, the spans then go straight
   into the window */
static BOOL lane_buffer(struct WaveData *wd, struct RastPort *rp)
{
	WORD x = (WORD) (wd->wd_IX & ~15), y = (WORD) (wd->wd_LaneY & ~15);
	WORD w = (WORD) (wd->wd_IX + wd->wd_IW - x), h = (WORD) (wd->wd_LaneY + wd->wd_LanesH - y);

	ULONG flags;

	if (!rp->BitMap || w <= 0 || h <= 0) return FALSE;
	if (w > wd->wd_BufW || h > wd->wd_BufH) free_buffer(wd);
	if (!wd->wd_BufRP.BitMap) {
		/* interleaved as the window's is: a blit between the two is one
		   pass of the blitter, not one a plane -- the beam never meets
		   some planes new and the others old */
		flags = GetBitMapAttr(rp->BitMap, BMA_FLAGS) & BMF_INTERLEAVED;
		InitRastPort(&wd->wd_BufRP);
		wd->wd_BufRP.BitMap = AllocBitMap((ULONG) w, (ULONG) h, GetBitMapAttr(rp->BitMap, BMA_DEPTH), flags, rp->BitMap);
		if (!wd->wd_BufRP.BitMap) {
			wd->wd_BufW = wd->wd_BufH = 0;
			return FALSE;
		}
		wd->wd_BufW = w;
		wd->wd_BufH = h;
		wd->wd_SigValid = FALSE;	/* a new buffer holds nothing yet */
		wd->wd_HeadUp = FALSE;
		/* the head's, as high as the lanes, 16 columns more for the
		   alignment it keeps; none: the head is drawn into the window */
		InitRastPort(&wd->wd_HeadRP);
		wd->wd_HeadW = HEAD_SPAN + 16;
		wd->wd_HeadRP.BitMap = AllocBitMap((ULONG) wd->wd_HeadW, (ULONG) h, GetBitMapAttr(rp->BitMap, BMA_DEPTH), flags,
						   rp->BitMap);
		/* planes in chip RAM: the wave's mask, a word more a row for a
		   span that starts inside one; none: RectFills */
		if (GetBitMapAttr(wd->wd_BufRP.BitMap, BMA_FLAGS) & BMF_STANDARD) {
			wd->wd_MaskWPR = (UWORD) ((w + 15) / 16 + 1);
			wd->wd_Mask = (UWORD *) AllocVec((ULONG) wd->wd_MaskWPR * 2 * (ULONG) h, MEMF_ANY);
			wd->wd_MaskChip = (UWORD *) AllocVec((ULONG) wd->wd_MaskWPR * 2 * (ULONG) h, MEMF_CHIP);
			if (!wd->wd_Mask || !wd->wd_MaskChip) {
				if (wd->wd_Mask) FreeVec(wd->wd_Mask);
				if (wd->wd_MaskChip) FreeVec(wd->wd_MaskChip);
				wd->wd_Mask = wd->wd_MaskChip = NULL;
				wd->wd_MaskWPR = 0;
			}
		}
	}
	wd->wd_BufX = x;
	wd->wd_BufY = y;
	if (rp->Font) SetFont(&wd->wd_BufRP, rp->Font);
	return TRUE;
}

/* a span painted into the buffer, its columns of the lanes then blitted
   into the window at once; straight in without a buffer */
static void span_out(struct Paint *pa, struct Overlay *ov, ULONG span)
{
	struct WaveData *wd = pa->pa_WD;
	struct Paint bp;
	WORD from = (WORD) (span >> 16), to = (WORD) (span & 0xFFFF), bx = wd->wd_BufX, by = wd->wd_BufY;

	if (!wd->wd_BufRP.BitMap) {
		paint_span(pa, ov, span);
		return;
	}
	bp.pa_RP = &wd->wd_BufRP;
	bp.pa_WD = wd;
	bp.pa_DX = (WORD) (pa->pa_DX - bx);
	bp.pa_DY = (WORD) (pa->pa_DY - by);
	paint_span(&bp, ov, span);
	BltBitMapRastPort(wd->wd_BufRP.BitMap, (WORD) (wd->wd_IX + from - bx), (WORD) (wd->wd_LaneY - by), pa->pa_RP,
			  (WORD) (wd->wd_IX + from), wd->wd_LaneY, (WORD) (to - from + 1), wd->wd_LanesH, 0xC0);
}

/* the columns under the floating head given back from the buffer */
static void head_hide(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	WORD x0 = wd->wd_HeadX0, x1 = wd->wd_HeadX1;

	if (!wd->wd_HeadUp) return;
	wd->wd_HeadUp = FALSE;
	if (x0 < wd->wd_IX) x0 = wd->wd_IX;
	if (x1 > wd->wd_IX + wd->wd_IW - 1) x1 = (WORD) (wd->wd_IX + wd->wd_IW - 1);
	if (x1 < x0 || !wd->wd_BufRP.BitMap) return;
	BltBitMapRastPort(wd->wd_BufRP.BitMap, (WORD) (x0 - wd->wd_BufX), (WORD) (wd->wd_LaneY - wd->wd_BufY), pa->pa_RP,
			  x0, wd->wd_LaneY, (WORD) (x1 - x0 + 1), wd->wd_LanesH, 0xC0);
}

/* the floating head drawn where wd_Position is, when it is in view (and
   in the cache's columns: beyond them nothing is in the buffer) */
static void head_show(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	struct Overlay o;
	LONG x;

	if (wd->wd_HeadOff || wd->wd_Position == WAVEFORM_NONE || (x = col_of(wd, wd->wd_Position)) < 0 ||
	    x >= wd->wd_CacheCols)
		return;
	o.ov_Kind = OV_PLAY;
	o.ov_Index = 0;
	o.ov_X = x;
	o.ov_X0 = x > 3 ? x - 3 : 0;
	o.ov_X1 = x + 3 < wd->wd_IW ? x + 3 : wd->wd_IW - 1;
	paint_overlay(pa, &o);
	wd->wd_HeadUp = TRUE;
	wd->wd_HeadX0 = (WORD) (wd->wd_IX + o.ov_X0);
	wd->wd_HeadX1 = (WORD) (wd->wd_IX + o.ov_X1);
}

/* the floating head to where wd_Position is, composed (wd_HeadRP): the
   buffer's columns under its old place and its new one copied over, the
   head drawn on them, the lot blitted into the window at once. Its old
   place far from the new one: given back by a blit of its own first.
   Without the head's bitmap: given back, then drawn into the window */
static void head_move(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	struct Paint hp;
	struct Overlay o;
	LONG x, n0, n1, u0, u1;
	WORD sx;

	/* the head elsewhere (a sprite): only what was drawn of it goes */
	if (wd->wd_HeadOff) {
		head_hide(pa);
		return;
	}
	x = wd->wd_Position == WAVEFORM_NONE ? -1 : col_of(wd, wd->wd_Position);
	if (!wd->wd_HeadRP.BitMap || x < 0 || x >= wd->wd_CacheCols || wd->wd_LanesH > wd->wd_BufH) {
		head_hide(pa);
		head_show(pa);
		return;
	}
	o.ov_Kind = OV_PLAY;
	o.ov_Index = 0;
	o.ov_X = x;
	o.ov_X0 = x > 3 ? x - 3 : 0;
	o.ov_X1 = x + 3 < wd->wd_IW ? x + 3 : wd->wd_IW - 1;
	u0 = n0 = wd->wd_IX + o.ov_X0;
	u1 = n1 = wd->wd_IX + o.ov_X1;
	if (wd->wd_HeadUp) {
		LONG a = wd->wd_HeadX0 < u0 ? wd->wd_HeadX0 : u0, b = wd->wd_HeadX1 > u1 ? wd->wd_HeadX1 : u1;

		if (a < wd->wd_IX) a = wd->wd_IX;
		if (b > wd->wd_IX + wd->wd_IW - 1) b = wd->wd_IX + wd->wd_IW - 1;
		if (b - a + 1 + (a & 15) <= wd->wd_HeadW) {
			u0 = a;
			u1 = b;
		} else {
			head_hide(pa);
		}
	}
	/* the head's bitmap keeps the window's alignment on 16 columns */
	sx = (WORD) (u0 & 15);
	BltBitMap(wd->wd_BufRP.BitMap, (WORD) (u0 - wd->wd_BufX), (WORD) (wd->wd_LaneY - wd->wd_BufY), wd->wd_HeadRP.BitMap, sx, 0,
		  (WORD) (u1 - u0 + 1), wd->wd_LanesH, 0xC0, 0xFF, NULL);
	hp.pa_RP = &wd->wd_HeadRP;
	hp.pa_WD = wd;
	hp.pa_DX = (WORD) (pa->pa_DX + sx - u0);
	hp.pa_DY = (WORD) (pa->pa_DY - wd->wd_LaneY);
	paint_overlay(&hp, &o);
	BltBitMapRastPort(wd->wd_HeadRP.BitMap, sx, 0, pa->pa_RP, (WORD) u0, wd->wd_LaneY, (WORD) (u1 - u0 + 1), wd->wd_LanesH, 0xC0);
	wd->wd_HeadUp = TRUE;
	wd->wd_HeadX0 = (WORD) n0;
	wd->wd_HeadX1 = (WORD) n1;
}

/* ---- the next page, painted ahead ---- */

/* what the next page's copy owns, as the gadget's: its buffer (the same
   size and format), its mask, its cache: FALSE when one is not to be had */
static BOOL next_alloc(struct WaveData *wd, struct WaveData *nx)
{
	ULONG bytes;
	UBYTE *mem;

	if (!nx->wd_BufRP.BitMap)
		nx->wd_BufRP.BitMap = AllocBitMap((ULONG) wd->wd_BufW, (ULONG) wd->wd_BufH, GetBitMapAttr(wd->wd_BufRP.BitMap, BMA_DEPTH),
						  GetBitMapAttr(wd->wd_BufRP.BitMap, BMA_FLAGS) & BMF_INTERLEAVED, wd->wd_BufRP.BitMap);
	if (wd->wd_Mask && !nx->wd_Mask) {
		bytes = (ULONG) wd->wd_MaskWPR * 2 * (ULONG) wd->wd_BufH;
		nx->wd_Mask = (UWORD *) AllocVec(bytes, MEMF_ANY);
		nx->wd_MaskChip = (UWORD *) AllocVec(bytes, MEMF_CHIP);
		nx->wd_MaskWPR = wd->wd_MaskWPR;
	}
	if (nx->wd_Cache && nx->wd_CacheMax != wd->wd_CacheMax) {
		FreeVec(nx->wd_Sig);
		nx->wd_Cache = NULL;
	}
	if (!nx->wd_Cache) {
		/* as ensure_cache() lays it out: the swap trades the blocks */
		bytes = (ULONG) wd->wd_CacheMax * (WF_MAXLANES * 2 + 1 + 2 * sizeof(ULONG)) + 4;
		if ((mem = (UBYTE *) AllocVec(bytes, MEMF_ANY | MEMF_CLEAR))) {
			nx->wd_Sig = (ULONG *) mem;
			nx->wd_NewSig = nx->wd_Sig + wd->wd_CacheMax;
			nx->wd_Cache = (BYTE *) (nx->wd_NewSig + wd->wd_CacheMax);
			nx->wd_Ground = (UBYTE *) nx->wd_Cache + (ULONG) wd->wd_CacheMax * WF_MAXLANES * 2;
			nx->wd_CacheMax = wd->wd_CacheMax;
		}
	}
	return (BOOL) (nx->wd_BufRP.BitMap && nx->wd_Cache && (!wd->wd_Mask || (nx->wd_Mask && nx->wd_MaskChip)));
}

/* the copy made anew from the gadget as it is, turned a page on: FALSE
   when there is no next page or no memory for one */
static BOOL next_start(struct WaveData *wd)
{
	struct WaveData *nx = wd->wd_Next;
	struct BitMap *bm;
	UWORD *mask, *mchip, wpr, cmax;
	ULONG *sig, *nsig;
	BYTE *cache;
	UBYTE *ground;
	ULONG vis = WaveView_Visible(&wd->wd_View);

	if (!nx) {
		if (!(nx = (struct WaveData *) AllocVec(sizeof(struct WaveData), MEMF_ANY | MEMF_CLEAR))) return FALSE;
		wd->wd_Next = nx;
	}
	if (!next_alloc(wd, nx)) {
		free_next(wd);
		return FALSE;
	}
	/* its own kept over the copy (locals: the struct is too big for a
	   task's stack) */
	bm = nx->wd_BufRP.BitMap;
	mask = nx->wd_Mask;
	mchip = nx->wd_MaskChip;
	wpr = nx->wd_MaskWPR;
	sig = nx->wd_Sig;
	nsig = nx->wd_NewSig;
	cache = nx->wd_Cache;
	ground = nx->wd_Ground;
	cmax = nx->wd_CacheMax;
	CopyMem((APTR) wd, (APTR) nx, sizeof(struct WaveData));
	nx->wd_BufRP.BitMap = bm;
	nx->wd_Mask = mask;
	nx->wd_MaskChip = mchip;
	nx->wd_MaskWPR = wpr;
	nx->wd_Sig = sig;
	nx->wd_NewSig = nsig;
	nx->wd_Cache = cache;
	nx->wd_Ground = ground;
	nx->wd_CacheMax = cmax;
	/* the gadget's, never the copy's to touch */
	nx->wd_HeadRP.BitMap = NULL;
	nx->wd_Diag = NULL;
	nx->wd_Next = NULL;
	nx->wd_HeadUp = nx->wd_CacheValid = nx->wd_SigValid = nx->wd_SampKept = FALSE;
	nx->wd_NxFrom = wd->wd_View.wv_Top;
	nx->wd_NxGen = wd->wd_Gen;
	nx->wd_NxCol = 0;
	nx->wd_NxStage = NX_NONE;
	/* the page after this one as the follow turns it, the head run just
	   off the right edge (waveplan.c: the same page, however far) */
	if (vis == 0 || !WaveView_Follow(&nx->wd_View, wd->wd_View.wv_Top + vis, WAVEFORM_FOLLOW_PAGE) ||
	    nx->wd_View.wv_Top <= wd->wd_View.wv_Top)
		return FALSE;
	/* planned now -- grounds and overlays need no column's values: the
	   columns inside the sound all to be drawn, those past its end none */
	for (cmax = nx->wd_CacheCols; cmax && col_first(nx, (LONG) cmax - 1) >= nx->wd_View.wv_Frames; cmax--) {
	}
	nx->wd_NxIn = cmax;
	nx->wd_Answered = nx->wd_Drawn = cmax;
	nx->wd_Want.ww_First = WAVEFORM_NONE;
	nx->wd_Want.ww_Frames = 0;
	nx->wd_CacheValid = TRUE;
	plan_grounds(nx);
	nx->wd_OvCount = plan_overlays(nx);
	nx->wd_Ov[nx->wd_OvCount].ov_Kind = 0;
	nx->wd_NxStage = NX_WORK;
	return TRUE;
}

/* the next page's columns c0 .. c1 - 1 (inside the sound): the source's,
   then the samples' -- TRUE when all there; FALSE, nothing tried, while
   the samples the gadget draws from do not hold them (the read-ahead's
   come later: the next step tries again) */
static BOOL next_fill(struct WaveData *nx, ULONG span)
{
	struct WavePeakMsg q;
	const struct WaveSamples *sm = nx->wd_Samples;
	UWORD ch = channels_of(nx), c0 = (UWORD) (span >> 16), c1 = (UWORD) (span & 0xFFFF);
	ULONG got = 0, more, end, need;

	if (c0 >= c1) return TRUE;
	q.wpm_First = col_first(nx, (LONG) c0);
	q.wpm_Step = nx->wd_View.wv_Zoom;
	q.wpm_Count = (ULONG) (c1 - c0);
	q.wpm_Out = nx->wd_Cache + (ULONG) c0 * ch * 2;
	if (nx->wd_Source) got = CallHookPkt(&nx->wd_Source->ws_Peaks, nx->wd_Obj, &q);
	if (got >= q.wpm_Count) return TRUE;
	if (!sm || !sm->wsm_Data || sm->wsm_Channels != ch) return FALSE;
	q.wpm_First = col_first(nx, (LONG) (c0 + got));
	need = col_first(nx, (LONG) c1);
	if (need > nx->wd_View.wv_Frames) need = nx->wd_View.wv_Frames;
	end = sm->wsm_First + sm->wsm_Frames;
	if (q.wpm_First < sm->wsm_First || end < need) return FALSE;
	q.wpm_Count = (ULONG) (c1 - c0) - got;
	q.wpm_Out = nx->wd_Cache + (ULONG) (c0 + got) * ch * 2;
	more = WavePlan_FromSamples(sm, &q);
	return (BOOL) (got + more >= (ULONG) (c1 - c0));
}

/* a next page to paint ahead at all: paging, the page in view whole in
   its buffer, a head */
static BOOL ahead_able(const struct WaveData *wd)
{
	return (BOOL) (!wd->wd_Overview && wd->wd_Follow == WAVEFORM_FOLLOW_PAGE && wd->wd_BufRP.BitMap && wd->wd_CacheValid &&
		       wd->wd_SigValid && wd->wd_CacheCols >= (UWORD) wd->wd_IW && wd->wd_LanesH <= wd->wd_BufH &&
		       wd->wd_Position != WAVEFORM_NONE);
}

/* the next page's columns not painted yet (WAVEFORM_AheadLeft): all of
   them before it began, 0 once painted -- or with none to paint (not
   paging, the last page) */
static ULONG ahead_left(const struct WaveData *wd)
{
	const struct WaveData *nx = wd->wd_Next;

	if (!ahead_able(wd)) return 0;
	if (!nx || nx->wd_NxGen != wd->wd_Gen || nx->wd_NxFrom != wd->wd_View.wv_Top || nx->wd_View.wv_Zoom != wd->wd_View.wv_Zoom)
		return wd->wd_CacheCols;
	return nx->wd_NxStage == NX_WORK ? (ULONG) (nx->wd_CacheCols - nx->wd_NxCol) : 0;
}

/* a step on the next page: started (anew when the gadget changed or the
   page turned) and planned, then cols at a time filled and painted into
   its buffer -- never the window -- its signatures at the end; the
   columns painted (0: none -- painted, none to paint, or the samples it
   needs not there yet) */
static UWORD next_step(struct WaveData *wd, UWORD cols)
{
	struct WaveData *nx = wd->wd_Next;
	struct Paint bp;
	UWORD c0, c1, i;

	if (!ahead_able(wd)) return 0;
	if (!nx || nx->wd_NxStage == NX_NONE || nx->wd_NxGen != wd->wd_Gen || nx->wd_NxFrom != wd->wd_View.wv_Top ||
	    nx->wd_View.wv_Zoom != wd->wd_View.wv_Zoom) {
		if (nx && nx->wd_NxStage == NX_NONE && nx->wd_NxGen == wd->wd_Gen && nx->wd_NxFrom == wd->wd_View.wv_Top) return 0;
		/* started, and on at once with its first piece */
		if (!next_start(wd)) return 0;
		nx = wd->wd_Next;
	}
	if (nx->wd_NxStage != NX_WORK) return 0;
	/* a piece, never ending inside an overlay: a later piece's grounds
	   would cut what it drew past its columns */
	c0 = nx->wd_NxCol;
	c1 = (UWORD) (c0 + cols < nx->wd_CacheCols ? c0 + cols : nx->wd_CacheCols);
	for (i = 0; i < MAX_OVERLAYS && nx->wd_Ov[i].ov_Kind != 0; i++) {
		if (nx->wd_Ov[i].ov_X0 < (LONG) c1 && nx->wd_Ov[i].ov_X1 >= (LONG) c1)
			c1 = (UWORD) (nx->wd_Ov[i].ov_X1 + 1 < (LONG) nx->wd_CacheCols ? nx->wd_Ov[i].ov_X1 + 1 : nx->wd_CacheCols);
	}
	/* the samples the gadget draws from now (a read-ahead's buffer is
	   filled again once the gadget has the next) */
	nx->wd_Samples = wd->wd_Samples;
	if (!next_fill(nx, ((ULONG) c0 << 16) | (ULONG) (c1 < nx->wd_NxIn ? c1 : nx->wd_NxIn))) return 0;
	bp.pa_RP = &nx->wd_BufRP;
	bp.pa_WD = nx;
	bp.pa_DX = (WORD) -nx->wd_BufX;
	bp.pa_DY = (WORD) -nx->wd_BufY;
	paint_span(&bp, nx->wd_Ov, ((ULONG) c0 << 16) | (ULONG) (c1 - 1));
	nx->wd_NxCol = c1;
	if (c1 < nx->wd_CacheCols) return (UWORD) (c1 - c0);
	plan_sigs(nx, nx->wd_Ov, nx->wd_OvCount);
	for (i = 0; i < nx->wd_CacheCols; i++) nx->wd_Sig[i] = nx->wd_NewSig[i];
	nx->wd_SigValid = TRUE;
	nx->wd_NxStage = NX_DONE;
	return (UWORD) (c1 - c0);
}

/* the page turned onto the one painted ahead (the same view, nothing
   changed since): its buffer and cache traded in, put into the window
   with one blit, the ruler and the head after it -- FALSE: paint it */
static BOOL next_swap(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD, *nx = wd->wd_Next;
	struct BitMap *bm;
	ULONG *sig, *nsig;
	BYTE *cache;
	UBYTE *ground;

	if (!nx || nx->wd_NxStage != NX_DONE || nx->wd_NxGen != wd->wd_Gen || nx->wd_View.wv_Top != wd->wd_View.wv_Top ||
	    nx->wd_View.wv_Zoom != wd->wd_View.wv_Zoom || nx->wd_View.wv_Frames != wd->wd_View.wv_Frames ||
	    nx->wd_CacheMax != wd->wd_CacheMax || nx->wd_CacheCols != wd->wd_CacheCols || !wd->wd_BufRP.BitMap ||
	    nx->wd_BufX != wd->wd_BufX || nx->wd_BufY != wd->wd_BufY)
		return FALSE;
	bm = wd->wd_BufRP.BitMap;
	wd->wd_BufRP.BitMap = nx->wd_BufRP.BitMap;
	nx->wd_BufRP.BitMap = bm;
	sig = wd->wd_Sig;
	nsig = wd->wd_NewSig;
	cache = wd->wd_Cache;
	ground = wd->wd_Ground;
	wd->wd_Sig = nx->wd_Sig;
	wd->wd_NewSig = nx->wd_NewSig;
	wd->wd_Cache = nx->wd_Cache;
	wd->wd_Ground = nx->wd_Ground;
	nx->wd_Sig = sig;
	nx->wd_NewSig = nsig;
	nx->wd_Cache = cache;
	nx->wd_Ground = ground;
	wd->wd_Answered = nx->wd_Answered;
	wd->wd_Drawn = nx->wd_Drawn;
	wd->wd_PaintLeft = 0;
	wd->wd_Lazy = wd->wd_Turned = FALSE;
	wd->wd_Want = nx->wd_Want;
	wd->wd_SampKept = FALSE;
	wd->wd_CacheValid = wd->wd_SigValid = TRUE;
	nx->wd_NxStage = NX_NONE;
	nx->wd_NxFrom = 0;
	/* what a paint would have done, from the buffer: the ruler, the lanes */
	wd->wd_DiagFull = TRUE;
	wd->wd_DiagFirst = 0;
	wd->wd_DiagLast = (WORD) (wd->wd_CacheCols - 1);
	if (wd->wd_RulerH) paint_ruler(pa);
	BltBitMapRastPort(wd->wd_BufRP.BitMap, (WORD) (wd->wd_IX - wd->wd_BufX), (WORD) (wd->wd_LaneY - wd->wd_BufY), pa->pa_RP,
			  wd->wd_IX, wd->wd_LaneY, (WORD) wd->wd_CacheCols, wd->wd_LanesH, 0xC0);
	/* the blit took the old head with it */
	wd->wd_HeadUp = FALSE;
	head_move(pa);
	wd->wd_Owed = FALSE;
	return TRUE;
}

/* ---- the paint log ---- */

struct DiagRing {
	ULONG dr_Next;				/* the next event's number */
	struct WaveDiagEvent dr_Ev[WAVEDIAG_EVENTS];
};

/* an event for the paint just done (or not done: WAVEDIAG_NORPORT) onto
   rp's layer; any task, so the slot is taken with interrupts off and the
   number written last */
static void diag_note(struct WaveData *wd, UBYTE kind, struct RastPort *rp)
{
	struct DiagRing *r = wd->wd_Diag;
	struct WaveDiagEvent *e;
	struct Layer *l;
	struct ClipRect *cr;
	ULONG seq;
	UWORD n = 0;

	if (!r) return;
	Disable();
	seq = r->dr_Next++;
	Enable();
	e = &r->dr_Ev[seq % WAVEDIAG_EVENTS];
	e->wde_Seq = 0;
	e->wde_Kind = kind;
	e->wde_Other = (UBYTE) (FindTask(NULL) != wd->wd_Owner);
	e->wde_Full = (UBYTE) (kind != WAVEDIAG_NORPORT && wd->wd_DiagFull);
	e->wde_Pad = 0;
	e->wde_First = kind == WAVEDIAG_NORPORT ? -1 : wd->wd_DiagFirst;
	e->wde_Last = kind == WAVEDIAG_NORPORT ? -1 : wd->wd_DiagLast;
	e->wde_Drawn = (UWORD) wd->wd_Drawn;
	e->wde_LayerFlags = 0;
	e->wde_Hidden = 0;
	e->wde_DamageX0 = e->wde_DamageX1 = -1;
	if (rp && (l = rp->Layer)) {
		e->wde_LayerFlags = l->Flags;
		for (cr = l->ClipRect; cr; cr = cr->Next) if (cr->obscured) n++;
		e->wde_Hidden = n;
		if ((l->Flags & LAYERREFRESH) && l->DamageList) {
			e->wde_DamageX0 = l->DamageList->bounds.MinX;
			e->wde_DamageX1 = l->DamageList->bounds.MaxX;
		}
	}
	e->wde_Seq = seq + 1;
}

BOOL Waveform_DiagOn(Object *wave)
{
	struct WaveData *wd;

	if (!wave || !wf_class) return FALSE;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	if (wd->wd_Diag) return TRUE;
	if (!(wd->wd_Diag = (struct DiagRing *) AllocVec(sizeof(struct DiagRing), MEMF_ANY | MEMF_CLEAR))) return FALSE;
	return TRUE;
}

UWORD Waveform_DiagRead(Object *wave, struct WaveDiagRead *rd)
{
	struct WaveData *wd;
	struct DiagRing *r;
	ULONG next, s;
	UWORD n = 0;

	rd->wdr_Lost = 0;
	if (!wave || !wf_class) return 0;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	if (!(r = wd->wd_Diag)) return 0;
	next = r->dr_Next;
	s = rd->wdr_After;
	if (next > WAVEDIAG_EVENTS && s < next - WAVEDIAG_EVENTS) {
		rd->wdr_Lost = next - WAVEDIAG_EVENTS - s;
		s = next - WAVEDIAG_EVENTS;
	}
	for (; s < next && n < rd->wdr_Max; s++) {
		/* number s + 1 in its slot: written to its end */
		if (r->dr_Ev[s % WAVEDIAG_EVENTS].wde_Seq != s + 1) break;
		rd->wdr_Out[n++] = r->dr_Ev[s % WAVEDIAG_EVENTS];
	}
	rd->wdr_After = s;
	return n;
}

/* columns c0 .. c1 of the lanes cleared to their ground at once, in the
   window and the buffer: a new view's columns over a paint's budget (the
   pixels there another view's), painted later -- one RectFill each, a
   paint of them cost an A1200 as much as their waves */
static void clear_span(struct Paint *pa, ULONG span)
{
	struct WaveData *wd = pa->pa_WD;
	struct Paint bp;
	struct IBox b;
	WORD from = (WORD) (span >> 16), to = (WORD) (span & 0xFFFF);

	b.Left = (WORD) (PIX(pa) + from);
	b.Width = (WORD) (to - from + 1);
	b.Top = (WORD) PLY(pa);
	b.Height = wd->wd_LanesH;
	SetAPen(pa->pa_RP, (ULONG) pen(wd, WAVEFORM_COLOR_BACKGROUND));
	box_fill(pa, &b);
	/* the buffer too where a drawn head puts its columns back from it:
	   a sprite's never does, and every column is painted there before it
	   is blitted (37 ms less an A1200 turn) */
	if (!wd->wd_BufRP.BitMap || wd->wd_HeadOff) return;
	bp.pa_RP = &wd->wd_BufRP;
	bp.pa_WD = wd;
	bp.pa_DX = (WORD) (pa->pa_DX - wd->wd_BufX);
	bp.pa_DY = (WORD) (pa->pa_DY - wd->wd_BufY);
	b.Left = (WORD) (PIX(&bp) + from);
	b.Top = (WORD) PLY(&bp);
	SetAPen(&wd->wd_BufRP, (ULONG) pen(wd, WAVEFORM_COLOR_BACKGROUND));
	box_fill(&bp, &b);
}

static void lazy_start(struct Paint *pa, BOOL keep);
static void lazy_update(struct Paint *pa);

/* the paint: how PAINT_#? says; the RastPort's font set by the caller.
   With a budget (WAVEFORM_PaintColumns, but not for a full paint) that
   many columns at most, from the left; a new view's others cleared
   (clear_span), an update's left as they are -- WAVEFORM_PaintLeft
   counts them, Waveform_PaintAhead() goes on with them (paint_more).
   A page turned to on a budget, and with WAVEFORM_FillNew any other new
   view, is lazy: Waveform_PaintAhead() fills it in (lazy_piece) */
static void paint(struct Paint *pa, UWORD how)
{
	struct WaveData *wd = pa->pa_WD;
	struct Overlay *ov = wd->wd_Ov;
	UWORD n, c, run, i, budget, done = 0, left = 0;
	BOOL full, off = (BOOL) ((wd->wd_Gad->Flags & GFLG_DISABLED) != 0);

	if (!wd->wd_Laid || wd->wd_IW <= 0 || wd->wd_IH <= 0 || !wd->wd_PensReady || !wd->wd_Cache) return;
	/* the buffer first: with it the play head is no overlay of the columns */
	if (!lane_buffer(wd, pa->pa_RP) && wd->wd_BufRP.BitMap) {
		free_buffer(wd);
		wd->wd_SigValid = FALSE;
		wd->wd_HeadUp = FALSE;
	}
	/* the head only: moved as Waveform_ShowPosition() moves it, nothing
	   planned (an A2000's Pause planned both views whole for it, 130 ms) */
	if (how == PAINT_HEAD) {
		if (wd->wd_BufRP.BitMap && wd->wd_SigValid && !off) {
			head_move(pa);
			wd->wd_Owed = FALSE;
			return;
		}
		how = PAINT_UPDATE;
	}
	budget = (UWORD) (how != PAINT_FULL && !off ? wd->wd_PaintCols : 0);
	if (budget && wd->wd_Turned && how >= PAINT_REFILL && wd->wd_BufRP.BitMap && wd->wd_IW > (WORD) budget) {
		/* a page turned (the last one lazy still or not: it is gone) */
		wd->wd_Turned = FALSE;
		lazy_start(pa, FALSE);
		wd->wd_Owed = FALSE;
		return;
	}
	wd->wd_Turned = FALSE;
	if (wd->wd_FillNew && how == PAINT_REFILL && !off && wd->wd_BufRP.BitMap && wd->wd_SigValid && wd->wd_Source) {
		/* any other new view: in pieces too, over the old picture (a
		   page painted whole cost an A1200 2.1 s, most of it the chip
		   bus) */
		lazy_start(pa, TRUE);
		wd->wd_Owed = FALSE;
		return;
	}
	if (wd->wd_Lazy && how < PAINT_REFILL && !off) {
		/* a change while a view fills in: in what it shows so far */
		lazy_update(pa);
		return;
	}
	if (wd->wd_Lazy) {
		/* a new view while a page fills in, not lazy: planned whole now
		   (the columns painted stay: their signatures are the same) */
		wd->wd_Lazy = FALSE;
		if (how < PAINT_REFILL) how = PAINT_REFILL;
	}
	/* disabled: everything, then ghosted (a span blitted over the ghost
	   would show plain) */
	full = (BOOL) (how == PAINT_FULL || !wd->wd_SigValid || off);
	wd->wd_DiagFull = full;
	wd->wd_DiagFirst = wd->wd_DiagLast = -1;
	if (how >= PAINT_REFILL || !wd->wd_CacheValid) fill_cache(wd);
	plan_grounds(wd);
	n = plan_overlays(wd);
	ov[n].ov_Kind = 0;
	plan_sigs(wd, ov, n);
	if (how == PAINT_FULL) paint_frame(pa);
	if (wd->wd_RulerH && (full || ruler_sig(wd) != wd->wd_RulerSig)) paint_ruler(pa);
	if (wd->wd_CacheCols < wd->wd_IW) {
		/* columns the cache could not hold: the ground */
		struct IBox b;

		SetAPen(pa->pa_RP, (ULONG) pen(wd, WAVEFORM_COLOR_RULERBACK));
		b.Left = (WORD) (PIX(pa) + wd->wd_CacheCols);
		b.Width = (WORD) (wd->wd_IW - wd->wd_CacheCols);
		b.Top = (WORD) PLY(pa);
		b.Height = wd->wd_LanesH;
		box_fill(pa, &b);
	}
	for (c = 0; c < wd->wd_CacheCols; c = run) {
		if (!full && wd->wd_NewSig[c] == wd->wd_Sig[c]) {
			run = (UWORD) (c + 1);
			continue;
		}
		for (run = (UWORD) (c + 1); run < wd->wd_CacheCols && (full || wd->wd_NewSig[run] != wd->wd_Sig[run]); run++) {
		}
		if (budget && done >= budget) {
			/* over the budget: what another view left cleared, to be
			   painted later; an update's old columns stay till then */
			if (full || how >= PAINT_REFILL) {
				clear_span(pa, ((ULONG) c << 16) | (ULONG) (run - 1));
				for (i = c; i < run; i++) wd->wd_Sig[i] = ~wd->wd_NewSig[i];
			}
			left = (UWORD) (left + run - c);
			continue;
		}
		if (budget && run - c > budget - done) {
			/* cut at the budget, never inside an overlay: a later
			   piece's grounds would cut what it drew past its columns */
			run = (UWORD) (c + budget - done);
			for (i = 0; i < MAX_OVERLAYS && ov[i].ov_Kind != 0; i++) {
				if (ov[i].ov_X0 < (LONG) run && ov[i].ov_X1 >= (LONG) run)
					run = (UWORD) (ov[i].ov_X1 + 1 < (LONG) wd->wd_CacheCols ? ov[i].ov_X1 + 1 : wd->wd_CacheCols);
			}
		}
		span_out(pa, ov, ((ULONG) c << 16) | (ULONG) (run - 1));
		for (i = c; i < run; i++) wd->wd_Sig[i] = wd->wd_NewSig[i];
		done = (UWORD) (done + run - c);
		if (wd->wd_DiagFirst < 0) wd->wd_DiagFirst = (WORD) c;
		wd->wd_DiagLast = (WORD) (run - 1);
	}
	wd->wd_PaintLeft = left;
	wd->wd_SigValid = TRUE;
	if (off) {
		/* ghosted as Intuition ghosts a gadget: no head over it */
		wd->wd_HeadUp = FALSE;
		SetDrMd(pa->pa_RP, JAM1);
		SetAPen(pa->pa_RP, (ULONG) (wd->wd_Dri ? wd->wd_Dri->dri_Pens[BLOCKPEN] : 1));
		SetAfPt(pa->pa_RP, ghost_pattern, 1);
		RectFill(pa->pa_RP, PIX(pa), wd->wd_IY + pa->pa_DY, PIX(pa) + wd->wd_IW - 1, wd->wd_IY + pa->pa_DY + wd->wd_IH - 1);
		SetAfPt(pa->pa_RP, NULL, 0);
	} else if (wd->wd_BufRP.BitMap) {
		/* the floating head over what was painted (a span's blit may
		   have taken it): its old columns back from the buffer, then
		   drawn anew */
		head_move(pa);
	}
	/* the head is where wd_Position says: nothing owed */
	wd->wd_Owed = FALSE;
}

/* Intuition's refresh of the damage a window over ours left (a simple
   refresh window: GM_RENDER GREDRAW_REDRAW between BeginRefresh and
   EndRefresh, the layer LAYERUPDATING, every drawing clipped to the
   damage): what the window showed, given back from the buffer -- nothing
   planned or painted anew. A column painted now would reach the screen
   only inside the damage, yet count as painted everywhere: MEASURED on the
   A4000T (2026-11-15, WaveSmith's Gain window closing over a view still
   filling in), the band under the closed window showed the new picture,
   above and below it the old one stayed for good. Work still owed (a view
   filling in) goes on afterwards, unclipped. FALSE: no refresh, or nothing
   to give back (no buffer, nothing painted yet, a box laid out anew, the
   gadget disabled): painted as ever. */
static BOOL refresh_from_buffer(struct Paint *pa, BOOL laid_anew)
{
	struct WaveData *wd = pa->pa_WD;
	struct RastPort *rp = pa->pa_RP;
	struct IBox b;
	WORD cols;

	if (!rp->Layer || !(rp->Layer->Flags & LAYERUPDATING) || laid_anew) return FALSE;
	if (!wd->wd_Laid || wd->wd_IW <= 0 || wd->wd_IH <= 0 || !wd->wd_PensReady || !wd->wd_Cache || !wd->wd_SigValid ||
	    !wd->wd_BufRP.BitMap || (wd->wd_Gad->Flags & GFLG_DISABLED))
		return FALSE;
	paint_frame(pa);
	if (wd->wd_RulerH) paint_ruler(pa);
	cols = (WORD) ((WORD) wd->wd_CacheCols < wd->wd_IW ? (WORD) wd->wd_CacheCols : wd->wd_IW);
	if (cols > 0)
		BltBitMapRastPort(wd->wd_BufRP.BitMap, (WORD) (wd->wd_IX - wd->wd_BufX), (WORD) (wd->wd_LaneY - wd->wd_BufY), rp,
				  wd->wd_IX, wd->wd_LaneY, cols, wd->wd_LanesH, 0xC0);
	if (cols < wd->wd_IW) {
		/* columns the cache could not hold: the ground, as paint() */
		SetAPen(rp, (ULONG) pen(wd, WAVEFORM_COLOR_RULERBACK));
		b.Left = (WORD) (PIX(pa) + cols);
		b.Width = (WORD) (wd->wd_IW - cols);
		b.Top = (WORD) PLY(pa);
		b.Height = wd->wd_LanesH;
		box_fill(pa, &b);
	}
	/* the floating head over it again (the blit took it) */
	head_move(pa);
	return TRUE;
}

/* the paint through the gadget's own RastPort (Intuition's GadgetInfo) */
static void repaint(struct WaveData *wd, struct GadgetInfo *gi, UWORD how)
{
	struct RastPort *rp;
	struct Paint pa;
	struct TextFont *old;

	if (!gi || how == PAINT_NONE || !wd->wd_Laid) return;
	if (how > PAINT_HEAD) wd->wd_Gen++;	/* a page painted ahead is not this any more */
	if (!(rp = ObtainGIRPort(gi))) {
		diag_note(wd, WAVEDIAG_NORPORT, NULL);
		wd->wd_DiagCtx = WAVEDIAG_INPUT;
		return;
	}
	if (!wd->wd_PensReady) obtain_pens(wd, gi);
	old = rp->Font;
	if (wd->wd_Font) SetFont(rp, wd->wd_Font);
	pa.pa_RP = rp;
	pa.pa_WD = wd;
	pa.pa_DX = pa.pa_DY = 0;
	paint(&pa, how);
	diag_note(wd, wd->wd_DiagCtx ? wd->wd_DiagCtx : WAVEDIAG_INPUT, rp);
	wd->wd_DiagCtx = WAVEDIAG_INPUT;
	if (old) SetFont(rp, old);
	ReleaseGIRPort(rp);
}

/* ---- layout ---- */

/* the ruler and the lanes in the inner box, from the class's own state:
   again whenever what they depend on changes -- a window lays out before
   the program sets its source, and the lanes of a mono layout under a
   stereo source put the second lane below the box. TRUE when they moved */
static BOOL place_lanes(struct WaveData *wd)
{
	WORD fh = font_height(wd), lanes = (WORD) lanes_of(wd), lh, rh, ry, ly, h, all;

	rh = (WORD) (wd->wd_Ruler != WAVEFORM_RULER_NONE && !wd->wd_Overview && wd->wd_IH > fh + 12 ? fh + 3 : 0);
	ry = (WORD) (wd->wd_Ruler == WAVEFORM_RULER_BOTTOM ? wd->wd_IY + wd->wd_IH - rh : wd->wd_IY);
	ly = (WORD) (wd->wd_Ruler == WAVEFORM_RULER_BOTTOM ? wd->wd_IY : wd->wd_IY + rh);
	lh = (WORD) (wd->wd_IH - rh);
	h = (WORD) ((lh - (lanes - 1) * (WORD) wd->wd_LaneGap) / lanes);
	if (h < 1) h = 1;
	all = (WORD) (h * lanes + (lanes - 1) * (WORD) wd->wd_LaneGap);
	if (rh == wd->wd_RulerH && ry == wd->wd_RulerY && ly == wd->wd_LaneY && lanes == (WORD) wd->wd_LaneCount &&
	    h == wd->wd_LaneH && all == wd->wd_LanesH)
		return FALSE;
	wd->wd_RulerH = rh;
	wd->wd_RulerY = ry;
	wd->wd_LaneY = ly;
	wd->wd_LaneCount = lanes;
	wd->wd_LaneH = h;
	wd->wd_LanesH = all;
	wd->wd_CacheValid = FALSE;
	wd->wd_SigValid = FALSE;
	return TRUE;
}

static void layout(struct WaveData *wd, struct GadgetInfo *gi)
{
	struct Gadget *g = wd->wd_Gad;
	struct IBox cbox, fbox;
	struct DrawInfo *dri = gi ? gi->gi_DrInfo : NULL;
	UWORD i;

	/* another screen (the window opened anew elsewhere): the pens were
	   the old one's -- forgotten, not released, as that screen may be
	   gone with its ColorMap; the buffer was a friend of its bitmap */
	if (wd->wd_PensReady && gi && gi->gi_Screen && gi->gi_Screen->ViewPort.ColorMap != wd->wd_CM) {
		for (i = 0; i < WAVEFORM_COLOR_COUNT; i++) wd->wd_Obtained[i] = FALSE;
		wd->wd_PensReady = FALSE;
		free_buffer(wd);
		wd->wd_HeadUp = FALSE;
	}
	wd->wd_Dri = dri;
	if (!wd->wd_FontGiven && dri && dri->dri_Font) wd->wd_Font = dri->dri_Font;
	if (wd->wd_Font) SetFont(&wd->wd_MRP, wd->wd_Font);
	wd->wd_FrameW = wd->wd_FrameH = 0;
	if (wd->wd_Bevel) {
		cbox.Left = cbox.Top = cbox.Width = cbox.Height = 0;
		fbox = cbox;
		wd->wd_FrameW = wd->wd_FrameH = 2;
		if (DoMethod(wd->wd_Bevel, IM_FRAMEBOX, (ULONG) &cbox, (ULONG) &fbox, (ULONG) dri, (ULONG) FRAMEF_MINIMAL)) {
			if (fbox.Left < 0) wd->wd_FrameW = (WORD) -fbox.Left;
			if (fbox.Top < 0) wd->wd_FrameH = (WORD) -fbox.Top;
		}
	}
	wd->wd_BoxL = g->LeftEdge;
	wd->wd_BoxT = g->TopEdge;
	wd->wd_BoxW = g->Width;
	wd->wd_BoxH = g->Height;
	wd->wd_IX = (WORD) (g->LeftEdge + wd->wd_FrameW);
	wd->wd_IY = (WORD) (g->TopEdge + wd->wd_FrameH);
	wd->wd_IW = (WORD) (g->Width - 2 * wd->wd_FrameW);
	wd->wd_IH = (WORD) (g->Height - 2 * wd->wd_FrameH);
	place_lanes(wd);
	if (wd->wd_IW > 0) {
		/* the view as wide as the cache holds (a failed growth: the old
		   one's width) */
		if (!ensure_cache(wd, (UWORD) wd->wd_IW)) wd->wd_IW = (WORD) wd->wd_CacheMax;
		wd->wd_View.wv_Columns = (UWORD) wd->wd_IW;
	}
	if (wd->wd_Overview) WaveView_Fit(&wd->wd_View);
	else WaveView_Clamp(&wd->wd_View);
	wd->wd_CacheValid = FALSE;
	wd->wd_SigValid = FALSE;
	wd->wd_Laid = TRUE;
	wd->wd_Gen++;
}

static ULONG domain(struct WaveData *wd, struct gpDomain *gpd)
{
	WORD fh = wd ? font_height(wd) : 8, w, h;

	switch (gpd->gpd_Which) {
	case GDOMAIN_MINIMUM:
		w = 64;
		h = (WORD) ((wd && wd->wd_Overview) ? 12 : fh + 3 + 2 * 12 + 4);
		break;
	case GDOMAIN_MAXIMUM:
		w = 0x7FFF;
		h = 0x7FFF;
		break;
	default:
		w = 320;
		h = (WORD) ((wd && wd->wd_Overview) ? 20 : fh + 3 + 2 * 40 + 4);
		break;
	}
	gpd->gpd_Domain.Left = gpd->gpd_Domain.Top = 0;
	gpd->gpd_Domain.Width = w;
	gpd->gpd_Domain.Height = h;
	return 1;
}

/* ---- attributes ---- */

/* a sound where there was none is shown whole (a Zoom or Top after it in
   the same tags still counts); the same one again keeps its view */
static void set_source(struct WaveData *wd, struct WaveSource *src)
{
	BOOL fresh = (BOOL) (!wd->wd_Source || wd->wd_View.wv_Frames == 0);

	if (src != wd->wd_Source) wd->wd_SampKept = FALSE;
	wd->wd_Source = src;
	wd->wd_View.wv_Frames = src ? src->ws_Frames : 0;
	if (wd->wd_Overview || wd->wd_View.wv_Zoom == 0 || fresh) WaveView_Fit(&wd->wd_View);
	else WaveView_Clamp(&wd->wd_View);
}

/* the tags of an OM_NEW or OM_SET: the paint they call for */
static UWORD set_attrs(struct WaveData *wd, struct opSet *ops)
{
	struct TagItem *tags = ops->ops_AttrList, *ti;
	struct WaveRange *range;
	UWORD how = PAINT_NONE, e;
	LONG step = 0;
	ULONG centre = WAVEFORM_NONE, d;
	BOOL zoom_verb = FALSE, view = FALSE, colours = FALSE;

	while ((ti = NextTagItem(&tags))) {
		d = ti->ti_Data;
		if (ti->ti_Tag >= WAVEFORM_PENTAG(0) && ti->ti_Tag <= WAVEFORM_RGBTAG(WAVEFORM_COLOR_COUNT - 1)) {
			e = (UWORD) ((ti->ti_Tag - WAVEFORM_PENTAG(0)) / 2);
			if ((ti->ti_Tag - WAVEFORM_PENTAG(0)) & 1) {
				wd->wd_RGB[e] = d;
				wd->wd_PenTag[e] = -1;
			} else {
				wd->wd_PenTag[e] = (LONG) d;
			}
			colours = TRUE;
			continue;
		}
		switch (ti->ti_Tag) {
		case WAVEFORM_Source: set_source(wd, (struct WaveSource *) d); how = PAINT_REFILL; break;
		case WAVEFORM_Top: wd->wd_View.wv_Top = d; view = TRUE; break;
		case WAVEFORM_Zoom:
			if (d) wd->wd_View.wv_Zoom = d;
			else WaveView_Fit(&wd->wd_View);
			view = TRUE;
			break;
		case WAVEFORM_ZoomMin: wd->wd_View.wv_ZoomMin = d ? d : 1; view = TRUE; break;
		case WAVEFORM_ZoomMax: wd->wd_View.wv_ZoomMax = d; view = TRUE; break;
		case WAVEFORM_ZoomStep: step = (LONG) d; zoom_verb = TRUE; break;
		case WAVEFORM_ZoomCenter: centre = d; break;
		case WAVEFORM_ShowRange:
			if ((range = (struct WaveRange *) d)) WaveView_Show(&wd->wd_View, range);
			view = TRUE;
			break;
		case WAVEFORM_ShowAll: if (d) WaveView_Fit(&wd->wd_View); view = TRUE; break;
		case WAVEFORM_Follow: wd->wd_Follow = (UWORD) d; break;
		case WAVEFORM_AutoScroll: wd->wd_AutoScroll = (BOOL) (d != 0); break;
		case WAVEFORM_Lanes: wd->wd_Lanes = (UWORD) d; how = PAINT_FULL; break;
		case WAVEFORM_LaneGap: wd->wd_LaneGap = (UWORD) d; how = PAINT_FULL; break;
		case WAVEFORM_LaneLabels: wd->wd_LaneLabels = (CONST_STRPTR *) d; how = PAINT_FULL; break;
		case WAVEFORM_Scale: wd->wd_Scale = (UWORD) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_DBRange: wd->wd_DBRange = (UWORD) d; wd->wd_SigValid = FALSE; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_Gain: wd->wd_Gain = (UWORD) (d ? d : 100); wd->wd_SigValid = FALSE; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_ClipMark: wd->wd_ClipMark = (BOOL) (d != 0); wd->wd_SigValid = FALSE; break;
		case WAVEFORM_WaveStyle: wd->wd_WaveStyle = (UWORD) d; wd->wd_SigValid = FALSE; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_CenterLine: wd->wd_CenterLine = (BOOL) (d != 0); wd->wd_SigValid = FALSE; break;
		case WAVEFORM_Grid: wd->wd_Grid = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_BevelStyle: wd->wd_BevelStyle = d; break;
		case WAVEFORM_Patterns: wd->wd_Patterns = (UWORD) d; colours = TRUE; break;
		case WAVEFORM_Font:
			wd->wd_Font = (struct TextFont *) d;
			wd->wd_FontGiven = (BOOL) (d != 0);
			if (wd->wd_Font) SetFont(&wd->wd_MRP, wd->wd_Font);
			how = PAINT_FULL;
			break;
		case WAVEFORM_Ruler: wd->wd_Ruler = (UWORD) d; how = PAINT_FULL; break;
		case WAVEFORM_TimeFormat: wd->wd_TimeFormat = (UWORD) d; wd->wd_RulerSig = 0; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_TimeOrigin: wd->wd_TimeOrigin = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_Cursor: wd->wd_Cursor = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_CursorStyle: wd->wd_CursorStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_ClickAction: wd->wd_ClickAction = (UWORD) d; break;
		case WAVEFORM_SelStart: wd->wd_SelStart = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_SelEnd: wd->wd_SelEnd = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_SelectMode: wd->wd_SelectMode = (UWORD) d; break;
		case WAVEFORM_SelectStyle: wd->wd_SelectStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_ExtendQualifier: wd->wd_ExtendQual = (UWORD) d; break;
		case WAVEFORM_DoubleClick: wd->wd_DoubleClick = (UWORD) d; break;
		case WAVEFORM_DragThreshold: wd->wd_DragThreshold = (UWORD) d; break;
		case WAVEFORM_DragEdges: wd->wd_DragEdges = (BOOL) (d != 0); break;
		case WAVEFORM_TrimIn: wd->wd_TrimIn = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_TrimOut: wd->wd_TrimOut = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_TrimHandles: wd->wd_TrimHandles = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_HandleStyle: wd->wd_HandleStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_HandleSize: wd->wd_HandleSize = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_HandleLabels: wd->wd_HandleLabels = (CONST_STRPTR *) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_HandleGrab: wd->wd_HandleGrab = (UWORD) d; break;
		case WAVEFORM_TrimShade: wd->wd_TrimShade = (UWORD) d; wd->wd_SigValid = FALSE; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_MinLength: wd->wd_MinLength = d ? d : 1; break;
		case WAVEFORM_Markers: wd->wd_Markers = (struct WaveMarker *) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_MarkerCount: wd->wd_MarkerCount = (UWORD) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_DragMarkers: wd->wd_DragMarkers = (BOOL) (d != 0); break;
		case WAVEFORM_MarkerStyle: wd->wd_MarkerStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_Regions: wd->wd_Regions = (struct WaveRegion *) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_RegionCount: wd->wd_RegionCount = (UWORD) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_Levels: wd->wd_Levels = (struct WaveLevel *) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_LevelCount: wd->wd_LevelCount = (UWORD) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_FadeIn: wd->wd_FadeIn = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_FadeOut: wd->wd_FadeOut = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_FadeShape: wd->wd_FadeShape = (UWORD) d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_ShowLevels: wd->wd_LevelLine = (BOOL) (d != 0); if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_Snap: wd->wd_Snap = (UWORD) d; break;
		case WAVEFORM_SnapStep: wd->wd_SnapStep = d; break;
		case WAVEFORM_SnapDistance: wd->wd_SnapDistance = (UWORD) d; break;
		case WAVEFORM_SnapHook: wd->wd_SnapHook = (struct Hook *) d; break;
		case WAVEFORM_Position:
			wd->wd_Position = d;
			/* the head gone (a stop): the next page's buffers given back */
			if (d == WAVEFORM_NONE) free_next(wd);
			if (!wd->wd_Overview && WaveView_Follow(&wd->wd_View, d, wd->wd_Follow)) {
				view = TRUE;
				changed(wd, WAVEFORM_CHANGED_VIEW);
			}
			/* the view where it was: the head alone (a Stop) */
			if (how < PAINT_HEAD) how = PAINT_HEAD;
			break;
		case WAVEFORM_PositionStyle: wd->wd_PositionStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_AheadColumns: wd->wd_AheadCols = (UWORD) (d && d < NX_COLS_MIN ? NX_COLS_MIN : d); break;
		/* no paint: what is left is painted when asked (Waveform_PaintAhead) */
		case WAVEFORM_PaintColumns: wd->wd_PaintCols = (UWORD) (d && d < NX_COLS_MIN ? NX_COLS_MIN : d > 0xFFFFUL ? 0xFFFFUL : d); break;
		case WAVEFORM_FillNew: wd->wd_FillNew = (BOOL) (d != 0); break;
		/* no paint either: the next piece stretches what it misses */
		case WAVEFORM_SamplesNone: wd->wd_NoSamples = (BOOL) (d != 0); break;
		/* drawn or taken away at once (a pause shows no more frames) */
		case WAVEFORM_HeadOff:
			wd->wd_HeadOff = (BOOL) (d != 0);
			wd->wd_Owed = TRUE;
			if (how < PAINT_HEAD) how = PAINT_HEAD;
			break;
		case WAVEFORM_Overview: wd->wd_Overview = (BOOL) (d != 0); break;
		case WAVEFORM_BoxTop: wd->wd_BoxTop = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_BoxVisible: wd->wd_BoxVisible = d; if (how < PAINT_UPDATE) how = PAINT_UPDATE; break;
		case WAVEFORM_BoxStyle: wd->wd_BoxStyle = (UWORD) d; wd->wd_SigValid = FALSE; break;
		case WAVEFORM_ReadOnly: wd->wd_ReadOnly = (BOOL) (d != 0); break;
		case WAVEFORM_Keys: wd->wd_Keys = (BOOL) (d != 0); break;
		case WAVEFORM_Help: wd->wd_Help = (BOOL) (d != 0); break;
		case WAVEFORM_HelpTexts: wd->wd_HelpTexts = (CONST_STRPTR *) d; break;
		case WAVEFORM_Pointers: wd->wd_Pointers = (BOOL) (d != 0); break;
		case WAVEFORM_HandlePointer: wd->wd_PointerType[0] = d; break;
		case WAVEFORM_EdgePointer: wd->wd_PointerType[1] = d; break;
		case WAVEFORM_WavePointer: wd->wd_PointerType[2] = d; break;
		case WAVEFORM_BoxPointer: wd->wd_PointerType[3] = d; break;
		case WAVEFORM_CustomPointers: wd->wd_CustomPointers = (Object **) d; break;
		case WAVEFORM_SignalTask: wd->wd_SigTask = (struct Task *) d; break;
		case WAVEFORM_SignalMask: wd->wd_SigMask = d; break;
		case WAVEFORM_Samples:
			/* more of the same sound, the view drawn from samples and
			   inside the new ones: its columns stay as they are -- a
			   refill would draw them again from the same frames (an
			   A1200's 0.18 s, each time a page's samples were read
			   ahead) */
			if (d && GetTagData(WAVEFORM_SamplesMore, FALSE, ops->ops_AttrList) && wd->wd_Samples &&
			    wd->wd_CacheValid && wd->wd_CacheCols && wd->wd_Answered >= wd->wd_CacheCols &&
			    ((struct WaveSamples *) d)->wsm_Channels == wd->wd_Samples->wsm_Channels &&
			    ((struct WaveSamples *) d)->wsm_First <= col_first(wd, 0) &&
			    ((struct WaveSamples *) d)->wsm_Frames >= col_first(wd, (LONG) wd->wd_CacheCols) - ((struct WaveSamples *) d)->wsm_First) {
				wd->wd_Samples = (struct WaveSamples *) d;
				wd->wd_NoSamples = FALSE;
				break;
			}
			wd->wd_Samples = (struct WaveSamples *) d;
			wd->wd_SampKept = FALSE;	/* new samples, maybe in the same buffer */
			wd->wd_NoSamples = FALSE;
			how = PAINT_REFILL;
			break;
		case WAVEFORM_Colors:
			if (d) {
				for (e = 0; e < WAVEFORM_COLOR_COUNT; e++) {
					if (((struct WaveColors *) d)->wc_RGB[e] != WAVEFORM_COLOR_DEFAULT) {
						wd->wd_RGB[e] = ((struct WaveColors *) d)->wc_RGB[e];
						wd->wd_PenTag[e] = -1;
					}
				}
			}
			colours = TRUE;
			break;
		case WAVEFORM_PenPrecision: wd->wd_Precision = d; break;
		case GA_Disabled: how = PAINT_FULL; break;
		}
	}
	if (zoom_verb) {
		if (centre == WAVEFORM_NONE && wd->wd_Cursor != WAVEFORM_NONE && col_of(wd, wd->wd_Cursor) >= 0) centre = wd->wd_Cursor;
		WaveView_Zoom(&wd->wd_View, step, centre);
		view = TRUE;
	}
	if (view) {
		WaveView_Clamp(&wd->wd_View);
		if (how < PAINT_REFILL) how = PAINT_REFILL;
	}
	if (colours) {
		release_pens(wd);
		how = PAINT_FULL;
	}
	/* the source's channels, the lanes, the gap, the ruler: placed anew */
	if (wd->wd_Laid && place_lanes(wd)) how = PAINT_FULL;
	if (!wd->wd_SigValid && how < PAINT_UPDATE) how = PAINT_UPDATE;
	return how;
}

static void defaults(struct WaveData *wd)
{
	UWORD i;

	wd->wd_Lanes = WAVEFORM_LANES_AUTO;
	wd->wd_LaneGap = 1;
	wd->wd_DBRange = 60;
	wd->wd_Gain = 100;
	wd->wd_ClipMark = TRUE;
	wd->wd_CenterLine = TRUE;
	wd->wd_BevelStyle = BVS_FIELD;
	wd->wd_Ruler = WAVEFORM_RULER_TOP;
	wd->wd_CursorStyle = WAVEFORM_CURSOR_DASHED;
	wd->wd_SelectMode = WAVEFORM_SELECT_RANGE;
	wd->wd_ExtendQual = IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT;
	wd->wd_DoubleClick = WAVEFORM_DOUBLE_REGION;
	wd->wd_DragThreshold = 3;
	wd->wd_DragEdges = TRUE;
	wd->wd_TrimHandles = WAVEFORM_HANDLES_DRAG;
	wd->wd_HandleStyle = WAVEFORM_HANDLE_FLAG;
	wd->wd_HandleGrab = 3;
	wd->wd_TrimShade = WAVEFORM_SHADE_DARKEN;
	wd->wd_MinLength = 1;
	wd->wd_MarkerStyle = WAVEFORM_HANDLE_TRIANGLE;
	wd->wd_FadeShape = WAVELEVEL_EVEN;
	wd->wd_LevelLine = TRUE;
	wd->wd_SnapDistance = 4;
	wd->wd_PositionStyle = WAVEFORM_POSITION_TRIANGLE;
	wd->wd_AheadCols = NX_COLS;
	wd->wd_AutoScroll = TRUE;
	wd->wd_Help = TRUE;
	wd->wd_Pointers = TRUE;
	wd->wd_PointerType[0] = POINTERTYPE_EASTWESTRESIZE;
	wd->wd_PointerType[1] = POINTERTYPE_EASTWESTRESIZE;
	wd->wd_PointerType[2] = POINTERTYPE_TEXT;
	wd->wd_PointerType[3] = POINTERTYPE_HAND;
	wd->wd_Cursor = wd->wd_SelStart = wd->wd_SelEnd = WAVEFORM_NONE;
	wd->wd_TrimIn = wd->wd_TrimOut = wd->wd_Position = WAVEFORM_NONE;
	wd->wd_Want.ww_First = WAVEFORM_NONE;
	wd->wd_ActiveMarker = -1;
	wd->wd_PointerFrame = WAVEFORM_NONE;
	wd->wd_View.wv_ZoomMin = 1;
	wd->wd_Precision = PRECISION_IMAGE;
	wd->wd_PtrShown = POINTERTYPE_NORMAL;
	for (i = 0; i < WAVEFORM_COLOR_COUNT; i++) {
		wd->wd_RGB[i] = default_rgb[i];
		wd->wd_PenTag[i] = -1;
		wd->wd_Pen[i] = 1;
	}
	InitRastPort(&wd->wd_MRP);
}

static BOOL get_attr(struct WaveData *wd, struct opGet *opg)
{
	ULONG v;

	switch (opg->opg_AttrID) {
	case WAVEFORM_Source: v = (ULONG) wd->wd_Source; break;
	case WAVEFORM_Frames: v = wd->wd_View.wv_Frames; break;
	case WAVEFORM_Rate: v = wd->wd_Source ? wd->wd_Source->ws_Rate : 0; break;
	case WAVEFORM_Channels: v = channels_of(wd); break;
	case WAVEFORM_Top: v = wd->wd_View.wv_Top; break;
	case WAVEFORM_Visible: v = WaveView_Visible(&wd->wd_View); if (v > wd->wd_View.wv_Frames) v = wd->wd_View.wv_Frames; break;
	case WAVEFORM_Zoom: v = wd->wd_View.wv_Zoom; break;
	case WAVEFORM_Cursor: v = wd->wd_Cursor; break;
	case WAVEFORM_SelStart: v = wd->wd_SelStart; break;
	case WAVEFORM_SelEnd: v = wd->wd_SelEnd; break;
	case WAVEFORM_TrimIn: v = wd->wd_TrimIn; break;
	case WAVEFORM_TrimOut: v = wd->wd_TrimOut; break;
	case WAVEFORM_FadeIn: v = wd->wd_FadeIn; break;
	case WAVEFORM_FadeOut: v = wd->wd_FadeOut; break;
	case WAVEFORM_BoxTop: v = wd->wd_BoxTop; break;
	case WAVEFORM_BoxVisible: v = wd->wd_BoxVisible; break;
	case WAVEFORM_Want: v = (ULONG) &wd->wd_Want; break;
	case WAVEFORM_Samples: v = (ULONG) wd->wd_Samples; break;
	case WAVEFORM_PointerFrame: v = wd->wd_PointerFrame; break;
	case WAVEFORM_ActiveMarker: v = (ULONG) wd->wd_ActiveMarker; break;
	case WAVEFORM_ActiveFrame: v = wd->wd_ActiveFrame; break;
	case WAVEFORM_AheadLeft: v = ahead_left(wd); break;
	case WAVEFORM_PaintLeft: v = wd->wd_PaintLeft; break;
	case WAVEFORM_HeadBox:
		wd->wd_HeadBox.hb_Left = wd->wd_IX;
		wd->wd_HeadBox.hb_Top = wd->wd_LaneY;
		wd->wd_HeadBox.hb_Columns = (UWORD) (wd->wd_Laid ? wd->wd_IW : 0);
		wd->wd_HeadBox.hb_Rows = (UWORD) wd->wd_LanesH;
		wd->wd_HeadBox.hb_First = wd->wd_View.wv_Top;
		wd->wd_HeadBox.hb_Zoom = wd->wd_View.wv_Zoom;
		v = (ULONG) &wd->wd_HeadBox;
		break;
	default: return FALSE;
	}
	*opg->opg_Storage = v;
	return TRUE;
}

/* ---- input ---- */

/* the part under an inner column x and gadget-relative line y */
static ULONG part_at(struct WaveData *wd, LONG x, LONG y)
{
	struct WaveHit h;
	LONG *mk = wd->wd_HitMarkers;
	UWORD i, n = 0;
	/* the overview's handles too, when the program asks (they only show
	   by default there): its knobs */
	BOOL handles = (BOOL) (wd->wd_TrimHandles == WAVEFORM_HANDLES_DRAG && !wd->wd_ReadOnly);

	h.wh_In = handles && wd->wd_TrimIn != WAVEFORM_NONE ? col_of(wd, wd->wd_TrimIn) : -1;
	h.wh_Out = handles && wd->wd_TrimOut != WAVEFORM_NONE ? col_of(wd, wd->wd_TrimOut > 0 ? wd->wd_TrimOut - 1 : 0) : -1;
	h.wh_SelStart = h.wh_SelEnd = -1;
	if (wd->wd_DragEdges && !wd->wd_ReadOnly && in_selection(wd, wd->wd_SelStart)) {
		h.wh_SelStart = col_of(wd, wd->wd_SelStart);
		h.wh_SelEnd = col_of(wd, wd->wd_SelEnd - 1);
	}
	if (wd->wd_DragMarkers && !wd->wd_ReadOnly) {
		for (i = 0; i < wd->wd_MarkerCount && i < MAX_HITMARKERS && wd->wd_Markers; i++) {
			mk[i] = (wd->wd_Markers[i].wm_Flags & (WAVEMARKER_FIXED | WAVEMARKER_HIDDEN)) ? -1 : col_of(wd, wd->wd_Markers[i].wm_Frame);
			n = (UWORD) (i + 1);
		}
	}
	h.wh_Markers = mk;
	h.wh_MarkerCount = n;
	h.wh_Grab = wd->wd_HandleGrab;
	/* the flags where paint_handle() puts them: In's right of its line,
	   Out's left of it, on the lanes' top */
	h.wh_FlagTop = h.wh_FlagBottom = 0;
	h.wh_InFlagX0 = h.wh_InFlagX1 = h.wh_OutFlagX0 = h.wh_OutFlagX1 = -1;
	if (handles && wd->wd_HandleStyle == WAVEFORM_HANDLE_FLAG) {
		WORD fh = (WORD) (wd->wd_HandleSize ? wd->wd_HandleSize : font_height(wd) + 2);

		if (fh > wd->wd_LanesH) fh = wd->wd_LanesH;
		h.wh_FlagTop = (WORD) (wd->wd_LaneY - wd->wd_Gad->TopEdge);
		h.wh_FlagBottom = (WORD) (h.wh_FlagTop + fh);
		if (h.wh_In >= 0) {
			h.wh_InFlagX0 = h.wh_In + 1;
			h.wh_InFlagX1 = h.wh_In + flag_width(wd, TRUE);
		}
		if (h.wh_Out >= 0) {
			h.wh_OutFlagX0 = h.wh_Out - flag_width(wd, FALSE);
			h.wh_OutFlagX1 = h.wh_Out - 1;
		}
	}
	h.wh_RulerTop = (WORD) (wd->wd_RulerY - wd->wd_Gad->TopEdge);
	h.wh_RulerBottom = (WORD) (h.wh_RulerTop + wd->wd_RulerH);
	h.wh_BoxLeft = h.wh_BoxRight = -1;
	if (wd->wd_Overview && wd->wd_BoxVisible) {
		h.wh_BoxLeft = col_of(wd, wd->wd_BoxTop);
		h.wh_BoxRight = col_of(wd, wd->wd_BoxTop + wd->wd_BoxVisible - 1);
		if (h.wh_BoxLeft < 0) h.wh_BoxLeft = 0;
		if (h.wh_BoxRight < 0) h.wh_BoxRight = wd->wd_IW - 1;
	}
	h.wh_Width = (UWORD) (wd->wd_IW > 0 ? wd->wd_IW : 0);
	h.wh_Height = (UWORD) wd->wd_Gad->Height;
	return WavePlan_Part(&h, x, y);
}

/* a frame where a drag left something: the snap the tags ask for */
static ULONG snapped(struct WaveData *wd, ULONG f)
{
	struct WaveSnap s;
	ULONG t[4];
	UWORD n = 0;

	if (wd->wd_Snap & (WAVEFORM_SNAP_STEP | WAVEFORM_SNAP_MARKERS)) {
		if (wd->wd_Drag != DRAG_IN && wd->wd_TrimIn != WAVEFORM_NONE) t[n++] = wd->wd_TrimIn;
		if (wd->wd_Drag != DRAG_OUT && wd->wd_TrimOut != WAVEFORM_NONE) t[n++] = wd->wd_TrimOut;
		if (wd->wd_Cursor != WAVEFORM_NONE) t[n++] = wd->wd_Cursor;
		s.wsn_Flags = wd->wd_Snap;
		s.wsn_TargetCount = n;
		s.wsn_Step = wd->wd_SnapStep;
		s.wsn_Targets = t;
		s.wsn_Distance = (ULONG) wd->wd_SnapDistance * wd->wd_View.wv_Zoom;
		f = WavePlan_Snap(&s, f);
	}
	if ((wd->wd_Snap & WAVEFORM_SNAP_HOOK) && wd->wd_SnapHook)
		f = CallHookPkt(wd->wd_SnapHook, wd->wd_Obj, &f);
	return f > wd->wd_View.wv_Frames ? wd->wd_View.wv_Frames : f;
}

/* In or Out to frame f, the other kept MinLength away */
static void move_trim(struct WaveData *wd, ULONG f)
{
	ULONG min = wd->wd_MinLength ? wd->wd_MinLength : 1;

	if (wd->wd_Drag == DRAG_IN) {
		if (wd->wd_TrimOut != WAVEFORM_NONE && f + min > wd->wd_TrimOut) f = wd->wd_TrimOut > min ? wd->wd_TrimOut - min : 0;
		wd->wd_TrimIn = f;
	} else {
		if (wd->wd_TrimIn != WAVEFORM_NONE && f < wd->wd_TrimIn + min) f = wd->wd_TrimIn + min;
		if (f > wd->wd_View.wv_Frames) f = wd->wd_View.wv_Frames;
		wd->wd_TrimOut = f;
	}
}

/* the selection from the anchor to f */
static void select_to(struct WaveData *wd, ULONG f)
{
	if (f < wd->wd_Anchor) {
		wd->wd_SelStart = f;
		wd->wd_SelEnd = wd->wd_Anchor;
	} else {
		wd->wd_SelStart = wd->wd_Anchor;
		wd->wd_SelEnd = f;
	}
}

/* a double-click's selection around f: the span between the handles,
   markers and region edges that surround it */
static void select_region(struct WaveData *wd, ULONG f)
{
	ULONG lo = 0, hi = wd->wd_View.wv_Frames, b, i;

	if (wd->wd_DoubleClick == WAVEFORM_DOUBLE_ALL) {
		wd->wd_SelStart = 0;
		wd->wd_SelEnd = hi;
		return;
	}
	/* a ULONG count: 4 + markers + 2 x regions passes a UWORD's range */
	for (i = 0; i < 4UL + wd->wd_MarkerCount + 2UL * wd->wd_RegionCount; i++) {
		if (i == 0) b = wd->wd_TrimIn;
		else if (i == 1) b = wd->wd_TrimOut;
		else if (i < 4) b = WAVEFORM_NONE;
		else if (i < 4UL + wd->wd_MarkerCount) b = wd->wd_Markers ? wd->wd_Markers[i - 4].wm_Frame : WAVEFORM_NONE;
		else if (wd->wd_Regions) {
			UWORD r = (UWORD) (i - 4 - wd->wd_MarkerCount);

			b = (r & 1) ? wd->wd_Regions[r / 2].wrg_End : wd->wd_Regions[r / 2].wrg_First;
		} else b = WAVEFORM_NONE;
		if (b == WAVEFORM_NONE) continue;
		if (b <= f && b > lo) lo = b;
		if (b > f && b < hi) hi = b;
	}
	wd->wd_SelStart = lo;
	wd->wd_SelEnd = hi;
}

static void drag_to(struct WaveData *wd, LONG x)
{
	ULONG f = WaveView_FrameAt(&wd->wd_View, x - wd->wd_GrabOffset);

	wd->wd_PointerFrame = WaveView_FrameAt(&wd->wd_View, x);
	switch (wd->wd_Drag) {
	case DRAG_PENDING:
		if ((x > wd->wd_PressX ? x - wd->wd_PressX : wd->wd_PressX - x) <= (LONG) wd->wd_DragThreshold) return;
		if (wd->wd_SelectMode != WAVEFORM_SELECT_RANGE || wd->wd_ReadOnly) return;
		wd->wd_Drag = DRAG_SELECT;
		/* fall through */
	case DRAG_SELECT:
		select_to(wd, f);
		changed(wd, WAVEFORM_CHANGED_SELECTION);
		break;
	case DRAG_IN:
	case DRAG_OUT:
		move_trim(wd, f);
		changed(wd, WAVEFORM_CHANGED_TRIM);
		break;
	case DRAG_SELSTART:
	case DRAG_SELEND:
		select_to(wd, f);
		changed(wd, WAVEFORM_CHANGED_SELECTION);
		break;
	case DRAG_MARKER:
		wd->wd_ActiveFrame = f;
		changed(wd, WAVEFORM_CHANGED_MARKER);
		break;
	case DRAG_BOX:
		if (wd->wd_BoxVisible >= wd->wd_View.wv_Frames) f = 0;
		else if (f > wd->wd_View.wv_Frames - wd->wd_BoxVisible) f = wd->wd_View.wv_Frames - wd->wd_BoxVisible;
		wd->wd_BoxTop = f;
		changed(wd, WAVEFORM_CHANGED_BOX);
		break;
	}
}

/* a press: what it grabs */
static ULONG go_active(struct WaveData *wd, struct gpInput *gpi)
{
	struct InputEvent *ie = gpi->gpi_IEvent;
	LONG x = gpi->gpi_Mouse.X - wd->wd_FrameW, y = gpi->gpi_Mouse.Y, hx;
	ULONG part, f;
	UWORD kind;

	if ((wd->wd_Gad->Flags & GFLG_DISABLED) || !wd->wd_Source || !wd->wd_Laid) return GMR_NOREUSE;
	if (!ie || ie->ie_Class != IECLASS_RAWMOUSE || ie->ie_Code != SELECTDOWN) return GMR_NOREUSE;
	/* the activating click is a click: the first of a double one */
	if (wd->wd_ClickCount && DoubleClick(wd->wd_ClickSecs, wd->wd_ClickMicros, ie->ie_TimeStamp.tv_secs, ie->ie_TimeStamp.tv_micro) &&
	    (x > wd->wd_ClickX ? x - wd->wd_ClickX : wd->wd_ClickX - x) <= 2)
		wd->wd_ClickCount++;
	else
		wd->wd_ClickCount = 1;
	wd->wd_ClickSecs = ie->ie_TimeStamp.tv_secs;
	wd->wd_ClickMicros = ie->ie_TimeStamp.tv_micro;
	wd->wd_ClickX = x;
	wd->wd_PressX = x;
	wd->wd_PressY = y;
	wd->wd_LastX = x;
	wd->wd_GrabOffset = 0;
	f = WaveView_FrameAt(&wd->wd_View, x);
	wd->wd_PointerFrame = f;
	part = part_at(wd, x, y);
	kind = (UWORD) WAVEPART_KIND(part);
	wd->wd_Drag = DRAG_NONE;
	switch (kind) {
	case WAVEPART_TRIMIN:
	case WAVEPART_TRIMOUT:
		wd->wd_Drag = kind == WAVEPART_TRIMIN ? DRAG_IN : DRAG_OUT;
		hx = col_of(wd, kind == WAVEPART_TRIMIN ? wd->wd_TrimIn : (wd->wd_TrimOut > 0 ? wd->wd_TrimOut - 1 : 0));
		wd->wd_GrabOffset = hx >= 0 ? x - hx : 0;
		break;
	case WAVEPART_MARKER:
		wd->wd_Drag = DRAG_MARKER;
		wd->wd_ActiveMarker = (LONG) WAVEPART_INDEX(part);
		wd->wd_ActiveFrame = wd->wd_Markers[wd->wd_ActiveMarker].wm_Frame;
		hx = col_of(wd, wd->wd_ActiveFrame);
		wd->wd_GrabOffset = hx >= 0 ? x - hx : 0;
		break;
	case WAVEPART_SELSTART:
		wd->wd_Drag = DRAG_SELSTART;
		wd->wd_Anchor = wd->wd_SelEnd;
		break;
	case WAVEPART_SELEND:
		wd->wd_Drag = DRAG_SELEND;
		wd->wd_Anchor = wd->wd_SelStart;
		break;
	case WAVEPART_BOX:
	case WAVEPART_BOXOUT:
		wd->wd_Drag = DRAG_BOX;
		if (kind == WAVEPART_BOXOUT) {
			/* a click beside the box: the box there, centred -- the
			   pointer holds its middle, for the drag that may follow */
			wd->wd_GrabOffset = wd->wd_View.wv_Zoom ? (LONG) (wd->wd_BoxVisible / 2 / wd->wd_View.wv_Zoom) : 0;
			drag_to(wd, x);
		} else {
			hx = col_of(wd, wd->wd_BoxTop);
			wd->wd_GrabOffset = hx >= 0 ? x - hx : 0;
		}
		break;
	default:
		if (wd->wd_ReadOnly || wd->wd_ClickAction == WAVEFORM_CLICK_SEEK) {
			wd->wd_Drag = DRAG_SEEK;
			break;
		}
		if (wd->wd_ClickAction == WAVEFORM_CLICK_NONE) break;
		if (wd->wd_ClickCount == 2 && wd->wd_DoubleClick != WAVEFORM_DOUBLE_NONE && wd->wd_SelectMode == WAVEFORM_SELECT_RANGE) {
			select_region(wd, f);
			changed(wd, WAVEFORM_CHANGED_SELECTION);
			break;
		}
		if ((ie->ie_Qualifier & wd->wd_ExtendQual) && wd->wd_SelectMode == WAVEFORM_SELECT_RANGE) {
			wd->wd_Anchor = in_selection(wd, wd->wd_SelStart) ? (f < wd->wd_SelStart ? wd->wd_SelEnd : wd->wd_SelStart) :
					(wd->wd_Cursor != WAVEFORM_NONE ? wd->wd_Cursor : f);
			wd->wd_Drag = DRAG_SELECT;
			select_to(wd, f);
			changed(wd, WAVEFORM_CHANGED_SELECTION);
			break;
		}
		wd->wd_Cursor = snapped(wd, f);
		if (wd->wd_SelStart != WAVEFORM_NONE) {
			wd->wd_SelStart = wd->wd_SelEnd = WAVEFORM_NONE;
			changed(wd, WAVEFORM_CHANGED_SELECTION);
		}
		changed(wd, WAVEFORM_CHANGED_CURSOR);
		wd->wd_Anchor = wd->wd_Cursor;
		wd->wd_Drag = DRAG_PENDING;
		break;
	}
	wd->wd_Activated = TRUE;
	repaint(wd, gpi->gpi_GInfo, PAINT_UPDATE);
	return GMR_MEACTIVE;
}

/* the drag let go: snapped, reported */
static ULONG release(struct WaveData *wd, struct gpInput *gpi)
{
	ULONG code;

	switch (wd->wd_Drag) {
	case DRAG_IN:
	case DRAG_OUT:
		move_trim(wd, snapped(wd, wd->wd_Drag == DRAG_IN ? wd->wd_TrimIn : wd->wd_TrimOut));
		changed(wd, WAVEFORM_CHANGED_TRIM);
		code = wd->wd_Drag == DRAG_IN ? WAVEFORM_UP_TRIMIN : WAVEFORM_UP_TRIMOUT;
		break;
	case DRAG_SELECT:
	case DRAG_SELSTART:
	case DRAG_SELEND:
		if (wd->wd_SelStart != WAVEFORM_NONE) {
			wd->wd_SelStart = snapped(wd, wd->wd_SelStart);
			wd->wd_SelEnd = snapped(wd, wd->wd_SelEnd);
		}
		changed(wd, WAVEFORM_CHANGED_SELECTION);
		code = WAVEFORM_UP_SELECTION;
		break;
	case DRAG_MARKER:
		wd->wd_ActiveFrame = snapped(wd, wd->wd_ActiveFrame);
		changed(wd, WAVEFORM_CHANGED_MARKER);
		code = WAVEFORM_UP_MARKER;
		break;
	case DRAG_BOX:
		code = WAVEFORM_UP_BOX;
		break;
	case DRAG_SEEK:
		changed(wd, WAVEFORM_CHANGED_SEEK);
		code = WAVEFORM_UP_SEEK;
		break;
	default:
		code = wd->wd_ClickCount == 2 ? WAVEFORM_UP_SELECTION : WAVEFORM_UP_CURSOR;
		break;
	}
	wd->wd_Drag = DRAG_NONE;
	repaint(wd, gpi->gpi_GInfo, PAINT_UPDATE);
	*gpi->gpi_Termination = code;
	return GMR_NOREUSE | GMR_VERIFY;
}

/* a drag held past an edge: the view moves an eighth a timer tick */
static void auto_scroll(struct WaveData *wd, struct gpInput *gpi)
{
	ULONG by = (ULONG) (wd->wd_View.wv_Columns / 8 + 1) * wd->wd_View.wv_Zoom, old = wd->wd_View.wv_Top;

	if (wd->wd_LastX < 0) wd->wd_View.wv_Top = wd->wd_View.wv_Top > by ? wd->wd_View.wv_Top - by : 0;
	else wd->wd_View.wv_Top += by;
	WaveView_Clamp(&wd->wd_View);
	if (wd->wd_View.wv_Top == old) return;
	changed(wd, WAVEFORM_CHANGED_VIEW);
	drag_to(wd, wd->wd_LastX < 0 ? 0 : wd->wd_IW - 1);
	repaint(wd, gpi->gpi_GInfo, PAINT_REFILL);
}

/* a drag called off (the menu button, a key, the gadget made inactive):
   nothing moves further; a marker shown where it was dragged goes back
   to its own frame */
static void abort_drag(struct WaveData *wd, struct GadgetInfo *gi)
{
	UWORD was = wd->wd_Drag;

	wd->wd_Drag = DRAG_NONE;
	if (was == DRAG_MARKER && wd->wd_Markers && wd->wd_ActiveMarker >= 0 && wd->wd_ActiveMarker < (LONG) wd->wd_MarkerCount) {
		wd->wd_ActiveFrame = wd->wd_Markers[wd->wd_ActiveMarker].wm_Frame;
		changed(wd, WAVEFORM_CHANGED_MARKER);
	}
	if (was != DRAG_NONE && gi) repaint(wd, gi, PAINT_UPDATE);
}

static ULONG handle_input(struct WaveData *wd, struct gpInput *gpi)
{
	struct InputEvent *ie = gpi->gpi_IEvent;
	LONG x;

	if (!ie) return GMR_MEACTIVE;
	x = gpi->gpi_Mouse.X - wd->wd_FrameW;
	switch (ie->ie_Class) {
	case IECLASS_RAWMOUSE:
		switch (ie->ie_Code) {
		case SELECTUP:
			return release(wd, gpi);
		case MENUDOWN:
			abort_drag(wd, gpi->gpi_GInfo);
			return GMR_REUSE;
		case IECODE_NOBUTTON:
			wd->wd_LastX = x;
			if (wd->wd_Drag == DRAG_NONE || wd->wd_Drag == DRAG_SEEK) return GMR_MEACTIVE;
			if (x < 0 || x >= wd->wd_IW) {
				if (!wd->wd_AutoScroll || wd->wd_Overview) drag_to(wd, x < 0 ? 0 : wd->wd_IW - 1);
			} else {
				drag_to(wd, x);
			}
			repaint(wd, gpi->gpi_GInfo, PAINT_UPDATE);
			return GMR_MEACTIVE;
		default:
			return GMR_MEACTIVE;
		}
	case IECLASS_TIMER:
		if (wd->wd_AutoScroll && !wd->wd_Overview && wd->wd_Drag != DRAG_NONE && wd->wd_Drag != DRAG_SEEK &&
		    wd->wd_Drag != DRAG_PENDING && (wd->wd_LastX < 0 || wd->wd_LastX >= wd->wd_IW))
			auto_scroll(wd, gpi);
		return GMR_MEACTIVE;
	case IECLASS_RAWKEY:
		/* no keyboard of its own: keys go back, Amiga keys to the menus;
		   a qualifier alone (Shift to extend, Ctrl) leaves the drag on */
		if (ie->ie_Code & IECODE_UP_PREFIX) return GMR_MEACTIVE;
		if (ie->ie_Code >= 0x60 && ie->ie_Code <= 0x67) return GMR_MEACTIVE;
		abort_drag(wd, gpi->gpi_GInfo);
		return GMR_REUSE;
	}
	return GMR_MEACTIVE;
}

/* ---- the pointer ---- */

static ULONG pointer_type(struct WaveData *wd, UWORD *slot)
{
	ULONG part;

	*slot = 4;
	if (!wd->wd_Pointers || !wd->wd_Laid) return POINTERTYPE_NORMAL;
	switch (wd->wd_Drag) {
	case DRAG_IN: case DRAG_OUT: case DRAG_MARKER: *slot = 0; return wd->wd_PointerType[0];
	case DRAG_SELSTART: case DRAG_SELEND: *slot = 1; return wd->wd_PointerType[1];
	case DRAG_BOX: *slot = 3; return wd->wd_PointerType[3];
	case DRAG_SELECT: case DRAG_PENDING: *slot = 2; return wd->wd_PointerType[2];
	}
	if (!wd->wd_HoverOK) return POINTERTYPE_NORMAL;
	part = part_at(wd, wd->wd_HoverX - wd->wd_FrameW, wd->wd_HoverY);
	switch (WAVEPART_KIND(part)) {
	case WAVEPART_TRIMIN: case WAVEPART_TRIMOUT: case WAVEPART_MARKER: *slot = 0; return wd->wd_PointerType[0];
	case WAVEPART_SELSTART: case WAVEPART_SELEND: *slot = 1; return wd->wd_PointerType[1];
	case WAVEPART_BOX: *slot = 3; return wd->wd_PointerType[3];
	case WAVEPART_WAVE:
		if (wd->wd_ReadOnly || wd->wd_ClickAction != WAVEFORM_CLICK_CURSOR) return POINTERTYPE_NORMAL;
		*slot = 2;
		return wd->wd_PointerType[2];
	}
	return POINTERTYPE_NORMAL;
}

/* the shape into gadgetclass when it changed, straight to the superclass
   (this class's OM_SET paints, and nothing here may) */
static void pointer_sync(struct WaveData *wd)
{
	struct TagItem tags[2];
	struct opSet ops;
	UWORD slot;
	ULONG type = pointer_type(wd, &slot);
	APTR own = (slot < 4 && wd->wd_CustomPointers) ? (APTR) wd->wd_CustomPointers[slot] : NULL;

	if (type == wd->wd_PtrShown && own == wd->wd_PtrObjShown) return;
	wd->wd_PtrShown = type;
	wd->wd_PtrObjShown = own;
	tags[0].ti_Tag = own ? GA_CustomMousePointer : GA_PointerType;
	tags[0].ti_Data = own ? (ULONG) own : type;
	tags[1].ti_Tag = TAG_DONE;
	tags[1].ti_Data = 0;
	ops.MethodID = OM_SET;
	ops.ops_AttrList = tags;
	ops.ops_GInfo = NULL;
	DoSuperMethodA(wd->wd_Class, wd->wd_Obj, (Msg) &ops);
}

/* ---- help bubbles ---- */

/* The bubble's text for what is under the pointer, one buffer for every
   object of the class (one pointer, one bubble; the program's HintInfo
   array takes its address before any object exists), and a serial that
   changes with the text: Intuition asks again only when the help code
   changes (AmiMatters' chanlist.c). GM_HELPTEST runs in Intuition's
   input handler: only the object's own state is read, and the scratch is
   static, not on that small stack (one input handler: no two at once). */
static char wf_help[WAVEFORM_HELPLEN], wf_scratch[WAVEFORM_HELPLEN];
static char wf_t0[WAVEPLAN_TIMELEN], wf_t1[WAVEPLAN_TIMELEN];
static UWORD wf_help_serial = 0;

static const char *const help_default[WAVEFORM_HELP_COUNT] = {
	"%s",		/* the sound: the time under the pointer */
	"%s %s",	/* a handle: its label, its time */
	"%s - %s",	/* the selection: its start, its end */
	"%s %s",	/* a marker: its label, its time */
	"%s - %s"	/* the overview's box: the view's start and end */
};

struct HelpArgs {
	CONST_STRPTR ha_Fmt;
	CONST_STRPTR ha_S[2];
};

/* ha's format with its %s filled in, into out (WAVEFORM_HELPLEN) */
static void help_put(const struct HelpArgs *ha, char *out)
{
	CONST_STRPTR f = ha->ha_Fmt, a;
	UWORD n = 0, k = 0;

	while (*f && n < WAVEFORM_HELPLEN - 1) {
		if (f[0] == '%' && f[1] == 's') {
			a = (k < 2 && ha->ha_S[k]) ? ha->ha_S[k] : (CONST_STRPTR) "";
			k++;
			while (*a && n < WAVEFORM_HELPLEN - 1) out[n++] = *a++;
			f += 2;
			continue;
		}
		out[n++] = *f++;
	}
	out[n] = '\0';
}

static void help_time(struct WaveData *wd, ULONG frame, char *buf)
{
	struct WaveClock ck;

	ck.wc_Rate = (wd->wd_Source && wd->wd_Source->ws_Rate) ? wd->wd_Source->ws_Rate : 1;
	ck.wc_Origin = wd->wd_TimeOrigin;
	ck.wc_Format = wd->wd_TimeFormat;
	ck.wc_Decimals = 3;
	WavePlan_Time(&ck, frame, buf);
}

/* the bubble for the pointer where GM_HELPTEST put it, into out:
   FALSE where there is none (the ruler, outside the sound) */
static BOOL help_text(struct WaveData *wd, char *out)
{
	struct HelpArgs ha;
	LONG x = wd->wd_HoverX - wd->wd_FrameW;
	ULONG part = part_at(wd, x, wd->wd_HoverY), m;
	UWORD i;

	ha.ha_S[0] = (CONST_STRPTR) wf_t0;
	ha.ha_S[1] = (CONST_STRPTR) wf_t1;
	switch (WAVEPART_KIND(part)) {
	case WAVEPART_WAVE:
	case WAVEPART_BOXOUT:
		if (WaveView_FrameAt(&wd->wd_View, x) >= wd->wd_View.wv_Frames) return FALSE;
		i = WAVEFORM_HELP_SOUND;
		help_time(wd, WaveView_FrameAt(&wd->wd_View, x), wf_t0);
		break;
	case WAVEPART_TRIMIN:
	case WAVEPART_TRIMOUT:
		i = WAVEFORM_HELP_HANDLE;
		m = WAVEPART_KIND(part) == WAVEPART_TRIMIN ? 0 : 1;
		ha.ha_S[0] = wd->wd_HandleLabels ? wd->wd_HandleLabels[m] : (CONST_STRPTR) (m ? "Out" : "In");
		help_time(wd, m ? wd->wd_TrimOut : wd->wd_TrimIn, wf_t1);
		ha.ha_S[1] = (CONST_STRPTR) wf_t1;
		break;
	case WAVEPART_SELSTART:
	case WAVEPART_SELEND:
		i = WAVEFORM_HELP_SELECTION;
		help_time(wd, wd->wd_SelStart, wf_t0);
		help_time(wd, wd->wd_SelEnd, wf_t1);
		break;
	case WAVEPART_MARKER:
		if (!wd->wd_Markers || (m = WAVEPART_INDEX(part)) >= wd->wd_MarkerCount) return FALSE;
		i = WAVEFORM_HELP_MARKER;
		ha.ha_S[0] = wd->wd_Markers[m].wm_Label ? wd->wd_Markers[m].wm_Label : (CONST_STRPTR) "";
		help_time(wd, wd->wd_Markers[m].wm_Frame, wf_t1);
		ha.ha_S[1] = (CONST_STRPTR) wf_t1;
		break;
	case WAVEPART_BOX:
		i = WAVEFORM_HELP_BOX;
		help_time(wd, wd->wd_BoxTop, wf_t0);
		help_time(wd, wd->wd_BoxTop + wd->wd_BoxVisible, wf_t1);
		break;
	default:
		return FALSE;
	}
	ha.ha_Fmt = (wd->wd_HelpTexts && wd->wd_HelpTexts[i]) ? wd->wd_HelpTexts[i] : (CONST_STRPTR) help_default[i];
	help_put(&ha, out);
	return TRUE;
}

/* GM_HELPTEST's answer: the bubble's text kept, the serial moved when it
   changed; no bubble where there is nothing to say (and the layout then
   forgets this gadget, so no old text shows elsewhere) */
static ULONG help_test(struct WaveData *wd)
{
	UWORD k;
	BOOL same = TRUE;

	if (!help_text(wd, wf_scratch)) wf_scratch[0] = '\0';
	for (k = 0; k < WAVEFORM_HELPLEN; k++) {
		if (wf_scratch[k] != wf_help[k]) same = FALSE;
		wf_help[k] = wf_scratch[k];
		if (!wf_scratch[k]) break;
	}
	if (!same) wf_help_serial++;
	return wf_help[0] ? GMR_HELPCODE | (ULONG) wf_help_serial : GMR_NOHELPHIT;
}

CONST_STRPTR Waveform_HelpText(void)
{
	return (CONST_STRPTR) wf_help;
}

/* ---- the dispatcher ---- */

DISP_QUAL ULONG Waveform_Dispatch(REG(a0, Class *cl), REG(a2, Object *o), REG(a1, Msg msg))
{
	struct WaveData *wd;
	struct opSet *ops;
	Object *newobj;
	ULONG result, bevel;
	UWORD how;

	switch (msg->MethodID) {
	case OM_NEW:
		if (!(newobj = (Object *) DoSuperMethodA(cl, o, msg))) return 0;
		wd = (struct WaveData *) INST_DATA(cl, newobj);
		/* every field cleared, then the defaults that are not 0 */
		clear_bytes((UBYTE *) wd, sizeof(*wd));
		wd->wd_Class = cl;
		wd->wd_Obj = newobj;
		wd->wd_Gad = (struct Gadget *) newobj;
		wd->wd_Owner = FindTask(NULL);
		wd->wd_DiagCtx = WAVEDIAG_INPUT;
		defaults(wd);
		ops = (struct opSet *) msg;
		/* Overview and BevelStyle before the rest: they decide the others */
		wd->wd_Overview = (BOOL) (GetTagData(WAVEFORM_Overview, FALSE, ops->ops_AttrList) != 0);
		if (wd->wd_Overview) {
			wd->wd_Ruler = WAVEFORM_RULER_NONE;
			wd->wd_TrimHandles = WAVEFORM_HANDLES_SHOW;
			wd->wd_HandleStyle = WAVEFORM_HANDLE_LINE;
			wd->wd_CenterLine = FALSE;
			wd->wd_SelectMode = WAVEFORM_SELECT_NONE;
		}
		set_attrs(wd, ops);
		/* the class from the library's base: the public name "bevel.image"
		   does not resolve on OS 3.2 (no frame was drawn, the columns
		   counted from the gadget's edge) */
		if (wf_bevel_lib && wd->wd_BevelStyle != BVS_NONE)
			wd->wd_Bevel = NewObject(((struct ClassLibrary *) wf_bevel_lib)->cl_Class, NULL,
						 BEVEL_Style, wd->wd_BevelStyle, TAG_DONE);
		return (ULONG) newobj;

	case OM_SET:
	case OM_UPDATE:
		result = DoSuperMethodA(cl, o, msg);
		wd = (struct WaveData *) INST_DATA(cl, o);
		ops = (struct opSet *) msg;
		bevel = wd->wd_BevelStyle;
		how = set_attrs(wd, ops);
		if (wd->wd_BevelStyle != bevel) {
			/* another frame: made anew, the inside laid out again (now,
			   or at the next render: wd_BoxW no box has) */
			if (wd->wd_Bevel) DisposeObject(wd->wd_Bevel);
			wd->wd_Bevel = NULL;
			if (wf_bevel_lib && wd->wd_BevelStyle != BVS_NONE)
				wd->wd_Bevel = NewObject(((struct ClassLibrary *) wf_bevel_lib)->cl_Class, NULL,
							 BEVEL_Style, wd->wd_BevelStyle, TAG_DONE);
			if (wd->wd_Laid && ops->ops_GInfo) layout(wd, ops->ops_GInfo);
			else wd->wd_BoxW = -1;
			how = PAINT_FULL;
		}
		/* the head alone leaves a page painted ahead as it is */
		if (how > PAINT_HEAD) wd->wd_Gen++;
		if (how != PAINT_NONE && ops->ops_GInfo && wd->wd_Laid) {
			wd->wd_DiagCtx = WAVEDIAG_SET;
			repaint(wd, ops->ops_GInfo, how);
			return 0;
		}
		if (how == PAINT_FULL || how == PAINT_REFILL) wd->wd_CacheValid = FALSE;
		return result | (how != PAINT_NONE ? 1UL : 0UL);

	case OM_GET:
		if (get_attr((struct WaveData *) INST_DATA(cl, o), (struct opGet *) msg)) return 1;
		return DoSuperMethodA(cl, o, msg);

	case OM_DISPOSE:
		wd = (struct WaveData *) INST_DATA(cl, o);
		release_pens(wd);
		free_cache(wd);
		free_buffer(wd);
		if (wd->wd_Bevel) DisposeObject(wd->wd_Bevel);
		wd->wd_Bevel = NULL;
		if (wd->wd_Diag) FreeVec(wd->wd_Diag);
		wd->wd_Diag = NULL;
		return DoSuperMethodA(cl, o, msg);

	case GM_DOMAIN:
		return domain(o ? (struct WaveData *) INST_DATA(cl, o) : NULL, (struct gpDomain *) msg);

	case GM_LAYOUT:
		wd = (struct WaveData *) INST_DATA(cl, o);
		layout(wd, ((struct gpLayout *) msg)->gpl_GInfo);
		return DoSuperMethodA(cl, o, msg);

	case GM_RENDER:
		wd = (struct WaveData *) INST_DATA(cl, o);
		if (((struct gpRender *) msg)->gpr_GInfo && ((struct gpRender *) msg)->gpr_GInfo->gi_DrInfo) {
			struct Paint pa;
			struct RastPort *rp = ((struct gpRender *) msg)->gpr_RPort;
			struct TextFont *old = rp->Font;
			BOOL anew = FALSE;

			/* the box the layout was made for, or laid out anew */
			if (!wd->wd_Laid || wd->wd_Gad->LeftEdge != wd->wd_BoxL || wd->wd_Gad->TopEdge != wd->wd_BoxT ||
			    wd->wd_Gad->Width != wd->wd_BoxW || wd->wd_Gad->Height != wd->wd_BoxH) {
				layout(wd, ((struct gpRender *) msg)->gpr_GInfo);
				anew = TRUE;
			}
			obtain_pens(wd, ((struct gpRender *) msg)->gpr_GInfo);
			if (wd->wd_Font) SetFont(rp, wd->wd_Font);
			pa.pa_RP = rp;
			pa.pa_WD = wd;
			pa.pa_DX = pa.pa_DY = 0;
			/* a refresh gives back what was there (refresh_from_buffer) */
			if (((struct gpRender *) msg)->gpr_Redraw == GREDRAW_UPDATE || !refresh_from_buffer(&pa, anew))
				paint(&pa, ((struct gpRender *) msg)->gpr_Redraw == GREDRAW_UPDATE ? PAINT_UPDATE : PAINT_FULL);
			diag_note(wd, (UBYTE) (((struct gpRender *) msg)->gpr_Redraw == GREDRAW_REDRAW ? WAVEDIAG_RENDER_REDRAW :
					       WAVEDIAG_RENDER_UPDATE), rp);
			if (old) SetFont(rp, old);
		}
		return 1;

	case GM_HITTEST:
		return GMR_GADGETHIT;

	case GM_HELPTEST:
		/* where the pointer is, for GM_QUERYHOVERED, which follows; the
		   bubble's text for it */
		wd = (struct WaveData *) INST_DATA(cl, o);
		wd->wd_HoverX = ((struct gpHitTest *) msg)->gpht_Mouse.X;
		wd->wd_HoverY = ((struct gpHitTest *) msg)->gpht_Mouse.Y;
		wd->wd_HoverOK = TRUE;
		if (!wd->wd_Help) return DoSuperMethodA(cl, o, msg);
		/* nothing to tell yet (no sound, or being decoded): no bubble --
		   gadgetclass's hit would open an empty one */
		if (!wd->wd_Source || !wd->wd_Laid) {
			wf_help[0] = '\0';
			return GMR_NOHELPHIT;
		}
		return help_test(wd);

	case GM_QUERYHOVERED:
		wd = (struct WaveData *) INST_DATA(cl, o);
		if (((struct opGet *) msg)->opg_AttrID == GA_GadgetHelpText && wd->wd_Help && wf_help[0]) {
			*((struct opGet *) msg)->opg_Storage = (ULONG) wf_help;
			return 1;
		}
		if (((struct opGet *) msg)->opg_AttrID == GA_CustomMousePointer) pointer_sync(wd);
		return DoSuperMethodA(cl, o, msg);

	case GM_GOACTIVE:
	case GM_HANDLEINPUT:
		wd = (struct WaveData *) INST_DATA(cl, o);
		if (((struct gpInput *) msg)->gpi_IEvent && ((struct gpInput *) msg)->gpi_IEvent->ie_Class == IECLASS_RAWMOUSE) {
			wd->wd_HoverX = ((struct gpInput *) msg)->gpi_Mouse.X;
			wd->wd_HoverY = ((struct gpInput *) msg)->gpi_Mouse.Y;
			wd->wd_HoverOK = TRUE;
		}
		result = msg->MethodID == GM_GOACTIVE ? go_active(wd, (struct gpInput *) msg) : handle_input(wd, (struct gpInput *) msg);
		pointer_sync(wd);
		return result;

	case GM_GOINACTIVE:
		wd = (struct WaveData *) INST_DATA(cl, o);
		abort_drag(wd, ((struct gpGoInactive *) msg)->gpgi_GInfo);
		wd->wd_Activated = FALSE;
		return 0;

	default:
		return DoSuperMethodA(cl, o, msg);
	}
}

/* ---- the class and its functions ---- */

Class *Waveform_MakeClass(void)
{
	Class *cl;

	if (!wf_bevel_lib) wf_bevel_lib = OpenLibrary((STRPTR) "images/bevel.image", 44);
	if ((cl = MakeClass(NULL, (STRPTR) GADGETCLASS, NULL, sizeof(struct WaveData), 0))) {
		cl->cl_Dispatcher.h_Entry = (ULONG (*)()) Waveform_Dispatch;
		cl->cl_Dispatcher.h_SubEntry = NULL;
		cl->cl_Dispatcher.h_Data = NULL;
		wf_class = cl;
	} else if (wf_bevel_lib) {
		CloseLibrary(wf_bevel_lib);
		wf_bevel_lib = NULL;
	}
	return cl;
}

void Waveform_FreeClass(Class *cl)
{
	/* objects of it still there: the class stays, and bevel.image with
	   it (their frames are its objects) */
	if (cl && !FreeClass(cl)) return;
	if (cl == wf_class) wf_class = NULL;
	if (wf_bevel_lib) CloseLibrary(wf_bevel_lib);
	wf_bevel_lib = NULL;
}

ULONG Waveform_Changes(Object *wave)
{
	struct WaveData *wd;
	ULONG c;

	if (!wave || !wf_class) return 0;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	Forbid();
	c = wd->wd_Changes;
	wd->wd_Changes = 0;
	Permit();
	return c;
}

ULONG Waveform_FrameAt(Object *wave, LONG x)
{
	struct WaveData *wd;

	if (!wave || !wf_class) return 0;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	return WaveView_FrameAt(&wd->wd_View, x - wd->wd_FrameW);
}

LONG Waveform_ColumnOf(Object *wave, ULONG frame)
{
	struct WaveData *wd;
	LONG c;

	if (!wave || !wf_class) return -1;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	c = WaveView_ColumnOf(&wd->wd_View, frame);
	return c < 0 ? -1 : c + wd->wd_FrameW;
}

LONG Waveform_PenOf(Object *wave, ULONG element)
{
	struct WaveData *wd;

	if (!wave || !wf_class || element >= WAVEFORM_COLOR_COUNT) return -1;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	return wd->wd_PensReady ? wd->wd_Pen[element] : -1;
}

/* cols more of the columns the last paint left, on its plan -- its
   grounds, overlays and signatures (anything changed since was painted,
   and planned anew): nothing planned again for a piece, which cost more
   than the piece; how many painted */
static UWORD lazy_piece(struct Paint *pa, UWORD cols);

static UWORD paint_more(struct Paint *pa, UWORD cols)
{
	struct WaveData *wd = pa->pa_WD;
	struct Overlay *ov = wd->wd_Ov;
	UWORD c, run, i, done = 0, left = 0;

	if (wd->wd_Lazy) return lazy_piece(pa, cols);

	for (c = 0; c < wd->wd_CacheCols && done < cols; c = run) {
		if (wd->wd_NewSig[c] == wd->wd_Sig[c]) {
			run = (UWORD) (c + 1);
			continue;
		}
		for (run = (UWORD) (c + 1); run < wd->wd_CacheCols && wd->wd_NewSig[run] != wd->wd_Sig[run] && run - c < cols - done;
		     run++) {
		}
		/* never ending inside an overlay */
		for (i = 0; i < MAX_OVERLAYS && ov[i].ov_Kind != 0; i++) {
			if (ov[i].ov_X0 < (LONG) run && ov[i].ov_X1 >= (LONG) run)
				run = (UWORD) (ov[i].ov_X1 + 1 < (LONG) wd->wd_CacheCols ? ov[i].ov_X1 + 1 : wd->wd_CacheCols);
		}
		span_out(pa, ov, ((ULONG) c << 16) | (ULONG) (run - 1));
		for (i = c; i < run; i++) wd->wd_Sig[i] = wd->wd_NewSig[i];
		done = (UWORD) (done + run - c);
	}
	for (c = 0; c < wd->wd_CacheCols; c++) left = (UWORD) (left + (wd->wd_NewSig[c] != wd->wd_Sig[c]));
	wd->wd_PaintLeft = left;
	/* the floating head over what was painted (a span's blit may have
	   taken it) */
	head_move(pa);
	return done;
}

/* a lazy page's signatures nothing planned equals (FNV gives it once in
   2^32: that column a piece late) */
#define LAZY_SIG	0x4C415A59UL	/* "LAZY" */

/* a page turned on a budget it cannot paint whole: nothing filled or
   planned -- the ruler and the overlays' places, the lanes cleared;
   lazy_piece() fills, plans and paints it a piece at a time from the
   left, the first in the program's next step (the controls looked at
   before it). The page planned whole cost an A1200's turn 260 ms (its
   cache 54, grounds 20, ruler 24, clearing 74), its first piece 40 more.
   keep: another new view (WAVEFORM_FillNew) -- nothing cleared, the old
   picture and its signatures kept: a piece paints the columns that look
   different only, and the head goes to its new place at once */
static void lazy_start(struct Paint *pa, BOOL keep)
{
	struct WaveData *wd = pa->pa_WD;
	struct IBox b;
	UWORD cols = (UWORD) wd->wd_IW, n, c;

	if (cols > wd->wd_CacheMax) cols = wd->wd_CacheMax;
	wd->wd_CacheCols = cols;
	wd->wd_CacheValid = wd->wd_SigValid = TRUE;
	wd->wd_SampKept = FALSE;
	/* as a page painted ahead: its pieces fill every column, from the
	   samples handed (none there: planned whole after all) */
	wd->wd_Answered = wd->wd_Drawn = cols;
	wd->wd_Want.ww_First = WAVEFORM_NONE;
	wd->wd_Want.ww_Frames = 0;
	wd->wd_NoSamples = FALSE;	/* this view's samples asked anew */
	for (wd->wd_LazyIn = cols; wd->wd_LazyIn && col_first(wd, (LONG) wd->wd_LazyIn - 1) >= wd->wd_View.wv_Frames;
	     wd->wd_LazyIn--) {
	}
	n = plan_overlays(wd);
	wd->wd_Ov[n].ov_Kind = 0;
	if (wd->wd_RulerH && (!keep || ruler_sig(wd) != wd->wd_RulerSig)) paint_ruler(pa);
	if (cols < wd->wd_IW && !keep) {
		/* columns the cache could not hold: the ground */
		SetAPen(pa->pa_RP, (ULONG) pen(wd, WAVEFORM_COLOR_RULERBACK));
		b.Left = (WORD) (PIX(pa) + cols);
		b.Width = (WORD) (wd->wd_IW - cols);
		b.Top = (WORD) PLY(pa);
		b.Height = wd->wd_LanesH;
		box_fill(pa, &b);
	}
	if (!keep) {
		if (cols) clear_span(pa, (ULONG) (cols - 1));
		for (c = 0; c < cols; c++) wd->wd_Sig[c] = LAZY_SIG;
	}
	wd->wd_LazyCol = 0;
	wd->wd_PaintLeft = cols;
	wd->wd_Lazy = TRUE;
	wd->wd_LazyKeep = keep;
	wd->wd_DiagFull = (BOOL) !keep;
	wd->wd_DiagFirst = wd->wd_DiagLast = -1;
	if (keep) head_move(pa);
}

/* a lazy view's columns c0 .. c1 - 1 (span) as a whole paint fills them
   (fill_cache) when the samples do not hold them all: the source's, the
   samples' (a last column they end inside left out, short of the
   sound's end), finer than the source the rest stretched from it -- how
   many filled */
static ULONG fill_part(struct WaveData *wd, ULONG span)
{
	struct WavePeakMsg q;
	const struct WaveSamples *sm = wd->wd_Samples;
	UWORD ch = channels_of(wd), c0 = (UWORD) (span >> 16), c1 = (UWORD) (span & 0xFFFF);
	ULONG got = 0, more, end;

	if (c0 >= c1) return 0;
	q.wpm_First = col_first(wd, (LONG) c0);
	q.wpm_Step = wd->wd_View.wv_Zoom;
	q.wpm_Count = (ULONG) (c1 - c0);
	q.wpm_Out = wd->wd_Cache + (ULONG) c0 * ch * 2;
	if (wd->wd_Source) got = CallHookPkt(&wd->wd_Source->ws_Peaks, wd->wd_Obj, &q);
	if (got > (ULONG) (c1 - c0)) got = (ULONG) (c1 - c0);
	if (got < (ULONG) (c1 - c0) && sm && sm->wsm_Channels == ch) {
		q.wpm_First = col_first(wd, (LONG) (c0 + got));
		q.wpm_Count = (ULONG) (c1 - c0) - got;
		q.wpm_Out = wd->wd_Cache + (ULONG) (c0 + got) * ch * 2;
		more = WavePlan_FromSamples(sm, &q);
		end = sm->wsm_First + sm->wsm_Frames;
		if (more && end < wd->wd_View.wv_Frames && end < col_first(wd, (LONG) (c0 + got + more))) more--;
		got += more;
	}
	if (got < (ULONG) (c1 - c0) && wd->wd_Source && wd->wd_Source->ws_Finest > 1 && wd->wd_View.wv_Zoom < wd->wd_Source->ws_Finest)
		got += stretch(wd, (ULONG) c0 + got, (ULONG) c1);
	return got;
}

/* a lazy view's next chunk, cols columns from c0 (never ending inside an
   overlay) filled from the source and the samples, its grounds and
   signatures planned, the columns that look different painted until
   *painted reaches cols (cut there, never inside an overlay): the column
   after the chunk's end, c0 when the samples do not hold it (asked for,
   WAVEFORM_Want) -- none will come (WAVEFORM_SamplesNone): stretched */
static UWORD lazy_chunk(struct Paint *pa, ULONG c0cols, UWORD *painted)
{
	struct WaveData *wd = pa->pa_WD;
	struct Overlay *ov = wd->wd_Ov;
	UWORD c0 = (UWORD) (c0cols >> 16), cols = (UWORD) (c0cols & 0xFFFF), c1, i, n, c, run, in;
	ULONG span, first, frames, got;

	c1 = (UWORD) (c0 + cols < wd->wd_CacheCols ? c0 + cols : wd->wd_CacheCols);
	for (n = 0; n < MAX_OVERLAYS && ov[n].ov_Kind != 0; n++) {
		if (ov[n].ov_X0 < (LONG) c1 && ov[n].ov_X1 >= (LONG) c1)
			c1 = (UWORD) (ov[n].ov_X1 + 1 < (LONG) wd->wd_CacheCols ? ov[n].ov_X1 + 1 : wd->wd_CacheCols);
	}
	in = c1 < wd->wd_LazyIn ? c1 : wd->wd_LazyIn;
	if (!next_fill(wd, ((ULONG) c0 << 16) | (ULONG) in)) {
		if (!wd->wd_NoSamples) {
			first = col_first(wd, (LONG) c0);
			frames = col_first(wd, (LONG) wd->wd_LazyIn) - first;
			if (wd->wd_Want.ww_First != first || wd->wd_Want.ww_Frames != frames) {
				wd->wd_Want.ww_First = first;
				wd->wd_Want.ww_Frames = frames;
				changed(wd, WAVEFORM_CHANGED_WANT);
			}
			return c0;
		}
		/* what the samples hold, the rest stretched, as a whole paint
		   fills it; samples handed later make the view new again (not
		   answered from here on) */
		got = fill_part(wd, ((ULONG) c0 << 16) | (ULONG) in);
		if (wd->wd_Answered > c0) wd->wd_Answered = c0;
		if (c0 + got < in && wd->wd_Drawn > c0 + got) wd->wd_Drawn = (UWORD) (c0 + got);
	}
	wd->wd_Want.ww_First = WAVEFORM_NONE;
	wd->wd_Want.ww_Frames = 0;
	span = ((ULONG) c0 << 16) | (ULONG) (c1 - 1);
	plan_grounds_span(wd, span);
	plan_sigs_span(wd, ov, n, span);
	for (c = c0; c < c1; c = run) {
		if (wd->wd_NewSig[c] == wd->wd_Sig[c]) {
			run = (UWORD) (c + 1);
			continue;
		}
		if (*painted >= cols) {
			/* the piece painted: the rest the next one's */
			c1 = c;
			break;
		}
		for (run = (UWORD) (c + 1); run < c1 && wd->wd_NewSig[run] != wd->wd_Sig[run]; run++) {
		}
		if (*painted + run - c > cols) {
			/* cut at the piece, never inside an overlay (as paint()'s
			   budget): the rest planned again by the next */
			run = (UWORD) (c + cols - *painted);
			for (i = 0; i < MAX_OVERLAYS && ov[i].ov_Kind != 0; i++) {
				if (ov[i].ov_X0 < (LONG) run && ov[i].ov_X1 >= (LONG) run)
					run = (UWORD) (ov[i].ov_X1 + 1 < (LONG) c1 ? ov[i].ov_X1 + 1 : c1);
			}
			c1 = run;
		}
		span_out(pa, ov, ((ULONG) c << 16) | (ULONG) (run - 1));
		for (i = c; i < run; i++) wd->wd_Sig[i] = wd->wd_NewSig[i];
		*painted = (UWORD) (*painted + run - c);
		if (wd->wd_DiagFirst < 0) wd->wd_DiagFirst = (WORD) c;
		wd->wd_DiagLast = (WORD) (run - 1);
	}
	return c1;
}

/* a lazy view's next piece from the first column not yet: chunks of cols
   columns (lazy_chunk) until cols are painted -- a kept view's chunks of
   columns that look the same cost their planning only, so up to four go
   to a piece (a piece sized by such columns painted 152 ms on an A1200,
   an edit's half page would be 500; looking ahead blindly filled at 712
   frames a column 1 ms a column more). The columns painted, 1 at least
   when it went on (the program sizes the next piece by them -- one that
   only looked says 1, too few to size by); 0: none, it waits for its
   samples */
static UWORD lazy_piece(struct Paint *pa, UWORD cols)
{
	struct WaveData *wd = pa->pa_WD;
	UWORD c0 = wd->wd_LazyCol, c, c1, k, painted = 0;

	if (c0 >= wd->wd_CacheCols) {
		wd->wd_Lazy = FALSE;
		wd->wd_PaintLeft = 0;
		return 0;
	}
	for (c = c0, k = 0; c < wd->wd_CacheCols && painted < cols && k < (UWORD) (wd->wd_LazyKeep ? 4 : 1); k++, c = c1) {
		if ((c1 = lazy_chunk(pa, ((ULONG) c << 16) | (ULONG) cols, &painted)) == c) break;
	}
	if (c == c0) return 0;
	wd->wd_LazyCol = c;
	wd->wd_PaintLeft = (UWORD) (wd->wd_CacheCols - c);
	if (c >= wd->wd_CacheCols) wd->wd_Lazy = FALSE;
	/* the floating head over what was painted */
	head_move(pa);
	return (UWORD) (painted ? painted : 1);
}

/* a change while a view fills in (the cursor, the selection, a marker,
   the handles): the columns painted so far planned and painted anew
   where they look different, the overlays placed anew; the others are
   planned by their pieces when they come (planning the view whole
   filled its cache anew: an A1200's 0.5 s at 712 frames a column) */
static void lazy_update(struct Paint *pa)
{
	struct WaveData *wd = pa->pa_WD;
	struct Overlay *ov = wd->wd_Ov;
	UWORD n, c, run, i, c1 = wd->wd_LazyCol;

	wd->wd_DiagFull = FALSE;
	wd->wd_DiagFirst = wd->wd_DiagLast = -1;
	n = plan_overlays(wd);
	ov[n].ov_Kind = 0;
	if (wd->wd_RulerH && ruler_sig(wd) != wd->wd_RulerSig) paint_ruler(pa);
	if (c1) {
		plan_grounds_span(wd, (ULONG) (c1 - 1));
		plan_sigs_span(wd, ov, n, (ULONG) (c1 - 1));
	}
	for (c = 0; c < c1; c = run) {
		if (wd->wd_NewSig[c] == wd->wd_Sig[c]) {
			run = (UWORD) (c + 1);
			continue;
		}
		for (run = (UWORD) (c + 1); run < c1 && wd->wd_NewSig[run] != wd->wd_Sig[run]; run++) {
		}
		span_out(pa, ov, ((ULONG) c << 16) | (ULONG) (run - 1));
		for (i = c; i < run; i++) wd->wd_Sig[i] = wd->wd_NewSig[i];
		if (wd->wd_DiagFirst < 0) wd->wd_DiagFirst = (WORD) c;
		wd->wd_DiagLast = (WORD) (run - 1);
	}
	head_move(pa);
	wd->wd_Owed = FALSE;
}

/* the view's columns left by a paint over its budget, cols of them
   painted into the window (its layer tried: busy, none) -- how many. On
   the last paint's plan; without the buffer (the head an overlay of the
   plan) or with the cache or the signatures given up, a paint anew */
static UWORD paint_left(struct WaveData *wd, struct Window *win, UWORD cols)
{
	struct RastPort rp;
	struct Paint pa;
	UWORD keep = wd->wd_PaintCols, before = wd->wd_PaintLeft, n;

	if (!win || !AttemptLockLayerRom(win->WLayer)) return 0;
	rp = *win->RPort;
	if (wd->wd_Font) SetFont(&rp, wd->wd_Font);
	pa.pa_RP = &rp;
	pa.pa_WD = wd;
	pa.pa_DX = pa.pa_DY = 0;
	if (wd->wd_BufRP.BitMap && wd->wd_CacheValid && wd->wd_SigValid) {
		n = paint_more(&pa, cols);
	} else {
		wd->wd_PaintCols = cols;
		paint(&pa, PAINT_UPDATE);
		wd->wd_PaintCols = keep;
		n = (UWORD) (before > wd->wd_PaintLeft ? before - wd->wd_PaintLeft : 0);
	}
	diag_note(wd, WAVEDIAG_HEAD, &rp);
	UnlockLayerRom(win->WLayer);
	return n;
}

ULONG Waveform_PaintAhead(Object *wave, struct Window *win, ULONG cols)
{
	struct WaveData *wd;
	UWORD n = (UWORD) (cols < NX_COLS_MIN ? NX_COLS_MIN : cols > 0xFFFFUL ? 0xFFFFUL : cols);

	if (!wave || !wf_class) return 0;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	if (!wd->wd_Laid || !wd->wd_PensReady || wd->wd_Drag != DRAG_NONE || (wd->wd_Gad->Flags & GFLG_DISABLED)) return 0;
	/* the page in view first */
	if (wd->wd_PaintLeft) return paint_left(wd, win, n);
	return next_step(wd, n);
}

ULONG Waveform_ShowPosition(Object *wave, struct Window *win, ULONG frame)
{
	struct WaveData *wd;
	struct RastPort rp;
	struct Paint pa;
	UWORD how = PAINT_UPDATE, ahead = 0;

	if (!wave || !wf_class || !win) return 0;
	wd = (struct WaveData *) INST_DATA(wf_class, wave);
	if (frame == wd->wd_Position && !wd->wd_Owed) return 0;
	wd->wd_Position = frame;
	/* the view follows -- not under a drag, which would lose its place;
	   whatever paints next fills the cache anew, this call or another */
	if (!wd->wd_Overview && wd->wd_Drag == DRAG_NONE && WaveView_Follow(&wd->wd_View, frame, wd->wd_Follow)) {
		wd->wd_CacheValid = FALSE;
		wd->wd_Turned = TRUE;
		changed(wd, WAVEFORM_CHANGED_VIEW);
	}
	if (!wd->wd_CacheValid) how = PAINT_REFILL;
	/* owed until drawn: a skipped frame is drawn by the next call, at the
	   same frame too */
	wd->wd_Owed = TRUE;
	if (!wd->wd_Laid || !wd->wd_PensReady || wd->wd_Drag != DRAG_NONE || (wd->wd_Gad->Flags & GFLG_DISABLED)) return 0;
	/* the layer tried, never waited for: busy, this frame is skipped and
	   the next one draws what changed meanwhile */
	if (!AttemptLockLayerRom(win->WLayer)) return 0;
	rp = *win->RPort;
	if (wd->wd_Font) SetFont(&rp, wd->wd_Font);
	pa.pa_RP = &rp;
	pa.pa_WD = wd;
	pa.pa_DX = pa.pa_DY = 0;
	if (how == PAINT_UPDATE && wd->wd_BufRP.BitMap && wd->wd_SigValid) {
		/* the floating head moved: what was under it back from the
		   buffer, the line at the new place -- nothing painted anew, and
		   nothing at all while it stays in its column */
		LONG x = frame == WAVEFORM_NONE ? -1 : col_of(wd, frame);

		if (!wd->wd_HeadUp || x < 0 || wd->wd_IX + (x > 3 ? x - 3 : 0) != wd->wd_HeadX0) head_move(&pa);
		wd->wd_Owed = FALSE;
		/* and a step on the next page */
		if (wd->wd_AheadCols) ahead = next_step(wd, wd->wd_AheadCols);
	} else if (how == PAINT_REFILL && next_swap(&pa)) {
		/* the page turned onto the one painted ahead */
		diag_note(wd, WAVEDIAG_HEAD, &rp);
	} else {
		paint(&pa, how);
		diag_note(wd, WAVEDIAG_HEAD, &rp);
	}
	UnlockLayerRom(win->WLayer);
	return ahead;
}
