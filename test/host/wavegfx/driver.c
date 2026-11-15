/*
 * driver.c -- waveform.gadget drawn on the host: Single.wav (tools/
 * make_edit_single.py) read as WaveSmith's doc.c reads it, the plan
 * made, the overview and the waveform laid out as WaveSmith's window on
 * a 640x256 screen, then the states a user goes through, each drawn into
 * the frame buffer and written as a PPM (each line twice):
 *   a  opened, the picture only      b  the suggestion
 *   c  At In: 3 s around In          d  In dragged 40 pixels right
 *   e  the play head moved           f  dB scale, a selection
 *   g  a page turned onto the next one, painted ahead while the head ran
 *   h  the level: a gain's bar and tag, a fade's line, the end fades
 * Every paint is clipped to its gadget's box: a pixel outside counts;
 * at the end everything must be given back.
 *   driver [-v] <Single.wav> <outdir> [nopens]
 * -v: every check said, not only those that fail; nopens: no ColorMap,
 * the class on DrawInfo's pens and patterns.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stub/amiga.h"
#include "gfx.h"
#include <gadgets/waveform.h>
#include "wavepeaks.h"
#include "wavesource.h"
#include "looklevel.h"
#include "levelmap.h"
#include "normwav.h"
#include "trimplan.h"
#include "waveplan.h"

static Class *cl;
static struct Screen scr;
static struct DrawInfo dri;
static UWORD pens[NUMDRIPENS] = { 0, 1, 2, 3, 4, 1, 2, 2, 2, 1, 1, 4 };
static struct TextFont topaz = { 8, 8, 6 };
static struct RastPort rp;
static struct BitMap bm = { 8, 0, 0, NULL };
static struct GadgetInfo gi;
static struct Window win;
static struct WavePeaks wp;
static struct WaveSource src;
/* the source's columns asked, counted (a turned page filled a piece at a
   time asks for a piece's) */
static ULONG (*peaks_real)(struct Hook *, APTR, APTR);
static ULONG peaks_asked = 0;

static ULONG count_peaks(struct Hook *h, APTR o, APTR m)
{
	peaks_asked += ((struct WavePeakMsg *) m)->wpm_Count;
	return peaks_real(h, o, m);
}
static struct TrimPlan tp;
static const char *outdir;
static int failures = 0, checks = 0, verbose = 0;
static WORD *all_samples;
static struct WaveSamples ws;
static ULONG get(Object *o, ULONG attr);
static void set1(Object *o, ULONG tag, ULONG data);

/* what the class wants, as WaveSmith's loop answers it (doc.c's
   Doc_Fetch: the frames from the file; here from memory) */
static void answer_wants(Object *o)
{
	struct WaveWant *w;

	if (!(Waveform_Changes(o) & WAVEFORM_CHANGED_WANT)) return;
	if (!(w = (struct WaveWant *) get(o, WAVEFORM_Want)) || w->ww_First == WAVEFORM_NONE) return;
	ws.wsm_First = w->ww_First;
	ws.wsm_Frames = w->ww_Frames;
	ws.wsm_Channels = 2;
	ws.wsm_Data = all_samples + w->ww_First * 2;
	set1(o, WAVEFORM_Samples, (ULONG) &ws);
}

static ULONG dispatch(Object *o, APTR m)
{
	return CallHookPkt(&cl->cl_Dispatcher, (APTR) o, m);
}

static void clip_to(Object *o)
{
	struct Gadget *g = (struct Gadget *) o;

	gfx_clip.x0 = g->LeftEdge;
	gfx_clip.y0 = g->TopEdge;
	gfx_clip.x1 = g->LeftEdge + g->Width - 1;
	gfx_clip.y1 = g->TopEdge + g->Height - 1;
}

static void set(Object *o, struct TagItem *tags)
{
	struct opSet ops;

	ops.MethodID = OM_SET;
	ops.ops_AttrList = tags;
	ops.ops_GInfo = &gi;
	clip_to(o);
	dispatch(o, &ops);
}

static void set1(Object *o, ULONG tag, ULONG data)
{
	struct TagItem t[2];

	t[0].ti_Tag = tag;
	t[0].ti_Data = data;
	t[1].ti_Tag = TAG_DONE;
	set(o, t);
}

static ULONG get(Object *o, ULONG attr)
{
	struct opGet opg;
	ULONG v = 0;

	opg.MethodID = OM_GET;
	opg.opg_AttrID = attr;
	opg.opg_Storage = &v;
	dispatch(o, &opg);
	return v;
}

static void place(Object *o, WORD l, WORD t, WORD w, WORD h)
{
	struct Gadget *g = (struct Gadget *) o;
	struct gpLayout gl;
	struct gpRender gr;

	g->LeftEdge = l;
	g->TopEdge = t;
	g->Width = w;
	g->Height = h;
	gl.MethodID = GM_LAYOUT;
	gl.gpl_GInfo = &gi;
	gl.gpl_Initial = 1;
	dispatch(o, &gl);
	gr.MethodID = GM_RENDER;
	gr.gpr_GInfo = &gi;
	gr.gpr_RPort = &rp;
	gr.gpr_Redraw = GREDRAW_REDRAW;
	clip_to(o);
	dispatch(o, &gr);
}

/* a paint of what is there (GREDRAW_UPDATE): the columns whose
   signatures moved */
static void update(Object *o)
{
	struct gpRender gr;

	gr.MethodID = GM_RENDER;
	gr.gpr_GInfo = &gi;
	gr.gpr_RPort = &rp;
	gr.gpr_Redraw = GREDRAW_UPDATE;
	clip_to(o);
	dispatch(o, &gr);
}

/* Intuition's refresh of a simple refresh window (a window over ours went
   away): GM_RENDER GREDRAW_REDRAW with the layer LAYERUPDATING, between
   BeginRefresh and EndRefresh -- on the Amiga clipped to the damage */
static void refreshed(Object *o)
{
	static struct Layer lay;
	struct gpRender gr;

	memset(&lay, 0, sizeof(lay));
	lay.Flags = LAYERUPDATING;
	rp.Layer = &lay;
	gr.MethodID = GM_RENDER;
	gr.gpr_GInfo = &gi;
	gr.gpr_RPort = &rp;
	gr.gpr_Redraw = GREDRAW_REDRAW;
	clip_to(o);
	dispatch(o, &gr);
	rp.Layer = NULL;
}

/* an input event at x, y (gadget-relative): its answer */
static ULONG input(Object *o, ULONG method, UWORD code, WORD x, WORD y)
{
	static ULONG secs = 100;
	struct InputEvent ie;
	struct gpInput gp;
	LONG term = 0;

	memset(&ie, 0, sizeof(ie));
	ie.ie_Class = IECLASS_RAWMOUSE;
	ie.ie_Code = code;
	ie.ie_TimeStamp.tv_secs = secs += 2;
	gp.MethodID = method;
	gp.gpi_GInfo = &gi;
	gp.gpi_IEvent = &ie;
	gp.gpi_Termination = &term;
	gp.gpi_Mouse.X = x;
	gp.gpi_Mouse.Y = y;
	gp.gpi_TabletData = NULL;
	clip_to(o);
	return dispatch(o, &gp);
}

/* the pointer resting at x, y as Intuition's hover asks: the help test's
   answer (the bubble's text then in Waveform_HelpText()), the pointer's
   shape in gfx_pointer */
static ULONG hover(Object *o, WORD x, WORD y)
{
	struct gpHitTest ht;
	struct opGet og;
	ULONG v, r;

	ht.MethodID = GM_HELPTEST;
	ht.gpht_GInfo = &gi;
	ht.gpht_Mouse.X = x;
	ht.gpht_Mouse.Y = y;
	r = dispatch(o, &ht);
	og.MethodID = GM_QUERYHOVERED;
	og.opg_AttrID = GA_CustomMousePointer;
	og.opg_Storage = &v;
	dispatch(o, &og);
	return r;
}

/* the bubble the class hands window.class now */
static const char *bubble(Object *o)
{
	struct opGet og;
	ULONG v = 0;

	og.MethodID = GM_QUERYHOVERED;
	og.opg_AttrID = GA_GadgetHelpText;
	og.opg_Storage = &v;
	return dispatch(o, &og) ? (const char *) (size_t) v : "";
}

#define MISSING_PEN 6	/* a pen DrawInfo and ObtainBestPen leave alone */
#define LEVEL_PEN 7	/* another */

/* the pixels of pen p in o's box */
static long pixels_of(Object *o, int p)
{
	struct Gadget *g = (struct Gadget *) o;
	long n = 0;
	int x, y;

	for (y = g->TopEdge; y < g->TopEdge + g->Height; y++)
		for (x = g->LeftEdge; x < g->LeftEdge + g->Width; x++) n += fb[y][x] == p;
	return n;
}

/* what the class wants from the program now, the changes read */
static struct WaveWant *wants(Object *o)
{
	(void) Waveform_Changes(o);
	return (struct WaveWant *) get(o, WAVEFORM_Want);
}

static void dump(const char *name)
{
	char path[512];

	snprintf(path, sizeof(path), "%s/%s.ppm", outdir, name);
	gfx_dump(path, FB_W, FB_H);
	checks++;
	if (verbose || gfx_out_of_box || gfx_inverted || gfx_chipread)
		printf("%s %s: %ld rects, %ld texts, %ld outside the box, %ld inverted, %ld template reads outside chip RAM\n",
		       gfx_out_of_box || gfx_inverted || gfx_chipread ? "FAIL" : "ok  ", name, gfx_rects, gfx_texts, gfx_out_of_box,
		       gfx_inverted, gfx_chipread);
	if (gfx_out_of_box || gfx_inverted || gfx_chipread) failures++;
	gfx_rects = gfx_texts = gfx_out_of_box = gfx_inverted = gfx_chipread = 0;
}

static void check(int ok, const char *what)
{
	checks++;
	if (verbose || !ok) printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failures++;
}

/* Single.wav as doc.c reads it: the picture, the looks, the plan */
static ULONG read_wav(const char *path)
{
	FILE *f = fopen(path, "rb");
	long size;
	unsigned char *b;
	struct NormWav nw;
	static struct LookLevel ll;
	struct TrimLooks tl;
	UBYTE *looks, *peaks;
	UWORD *w;
	ULONG frames, i, n, pc = 0, pin = 0, pl = 0, pr = 0, k;
	UWORD c;

	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	b = malloc((size_t) size);
	if (fread(b, 1, (size_t) size, f) != (size_t) size) return 0;
	fclose(f);
	if (NormWav_Parse(b, (ULONG) size, &nw) != NORMWAV_OK) return 0;
	frames = nw.nw_DataBytes / nw.nw_Align;
	n = frames * nw.nw_Channels;
	w = malloc(n * 2);
	for (i = 0; i < n; i++) w[i] = (UWORD) ((b[nw.nw_DataStart + 2 * i] << 8) | b[nw.nw_DataStart + 2 * i + 1]);
	LookLevel_Init(&ll, nw.nw_Rate, nw.nw_Channels);
	looks = calloc(frames / ll.ll_Frames + 2, 1);
	peaks = calloc(frames / ll.ll_Frames + 2, 1);
	ll.ll_Out = looks;
	ll.ll_OutMax = frames / ll.ll_Frames + 2;
	LookLevel_Feed(&ll, w, frames);
	LookLevel_Finish(&ll);
	for (i = 0; i < n; i++) w[i] = (UWORD) ((w[i] >> 8) | (w[i] << 8));
	for (i = 0; i < frames; i++) {
		for (c = 0; c < nw.nw_Channels; c++) {
			long v = (WORD) w[i * nw.nw_Channels + c];
			ULONG a = (ULONG) (v < 0 ? -v : v);

			if (a > 32767) a = 32767;
			if (c == 0 && a > pl) pl = a;
			if (c == 1 && a > pr) pr = a;
		}
		if (++pin == ll.ll_Frames) {
			peaks[pc++] = LevelMap_Byte((pl << 16) | pr, (BOOL) (nw.nw_Channels == 1));
			pin = pl = pr = 0;
		}
	}
	WavePeaks_Init(&wp, frames, nw.nw_Channels);
	for (k = 0; k < WAVEPEAKS_LEVELS; k++) wp.wp_Level[k] = calloc(WavePeaks_Bytes(&wp, (UWORD) k) + 4, 1);
	/* half of it first: the reading part-way (state a) */
	WavePeaks_Feed(&wp, (const WORD *) w, frames / 2);
	WaveSource_Peaks(&src, &wp);
	peaks_real = (ULONG (*)(struct Hook *, APTR, APTR)) src.ws_Peaks.h_Entry;
	src.ws_Peaks.h_Entry = (ULONG (*)()) count_peaks;
	src.ws_Rate = nw.nw_Rate;
	tl.tl_Level = looks;
	tl.tl_Peak = peaks;
	tl.tl_Count = ll.ll_OutCount < pc ? ll.ll_OutCount : pc;
	tl.tl_LookFrames = ll.ll_Frames;
	tl.tl_Frames = frames;
	tl.tl_Rate = nw.nw_Rate;
	tl.tl_LeadMs = 500;
	tl.tl_TailMs = 2000;
	TrimPlan_Make(&tl, &tp);
	/* the rest kept for later */
	wp.wp_Pad = 0;
	return (ULONG) (size_t) w;
}

int main(int argc, char **argv)
{
	static CONST_STRPTR lanes[2] = { "L", "R" }, handles[2] = { "In", "Out" };
	Object *over, *wave;
	struct TagItem t[8];
	struct WaveRange r;
	WORD *samples;
	ULONG in0, in1, zoom, f, r1, r2;
	LONG x;

	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		verbose = 1;
		argv++;
		argc--;
	}
	if (argc < 3) return 2;
	outdir = argv[2];
	gfx_reset();
	/* WAVEGFX_STANDARD set: the bitmaps planar in chip RAM, the wave drawn
	   through the class's mask -- the frames must be the RectFills' */
	gfx_standard = getenv("WAVEGFX_STANDARD") != NULL;
	if (!(samples = (WORD *) (size_t) read_wav(argv[1]))) {
		printf("FAIL cannot read %s\n", argv[1]);
		return 2;
	}
	all_samples = samples;
	{
		static struct ColorMap cm;

		scr.ViewPort.ColorMap = argc > 3 ? NULL : &cm;	/* nopens: no pens to obtain */
	}
	dri.dri_Pens = pens;
	dri.dri_Font = &topaz;
	scr.RastPort.BitMap = &bm;
	InitRastPort(&rp);
	rp.BitMap = &bm;
	SetFont(&rp, &topaz);
	gi.gi_Screen = &scr;
	gi.gi_DrInfo = &dri;
	gi.gi_RastPort = &rp;
	win.RPort = &rp;
	if (!(cl = Waveform_MakeClass())) return 2;
	over = NewObject(cl, NULL, WAVEFORM_Overview, TRUE, WAVEFORM_TrimShade, WAVEFORM_SHADE_DARKEN, TAG_DONE);
	wave = NewObject(cl, NULL, WAVEFORM_LaneLabels, (ULONG) lanes, WAVEFORM_HandleLabels, (ULONG) handles,
			 WAVEFORM_MissingPen, MISSING_PEN, TAG_DONE);
	check(over && wave, "the objects made");

	/* a: opened, half read: the rest Missing. Laid out first, the source
	   set after, as a window does (its layout comes before the file) */
	place(over, 8, 25, 624, 20);
	place(wave, 8, 48, 624, 134);
	set1(over, WAVEFORM_Source, (ULONG) &src);
	set1(wave, WAVEFORM_Source, (ULONG) &src);
	check(pixels_of(wave, MISSING_PEN) > 1000, "half read: the rest drawn Missing");
	check(wants(wave)->ww_First == WAVEFORM_NONE && wants(over)->ww_First == WAVEFORM_NONE,
	      "half read, the whole shown: no samples wanted (the picture is the program's to finish)");
	dump("h-a");

	/* b: all read, the suggestion */
	WavePeaks_Feed(&wp, samples + wp.wp_Done * 2, wp.wp_Frames - wp.wp_Done);
	t[0].ti_Tag = WAVEFORM_Source; t[0].ti_Data = (ULONG) &src;
	t[1].ti_Tag = WAVEFORM_TrimIn; t[1].ti_Data = tp.tp_In;
	t[2].ti_Tag = WAVEFORM_TrimOut; t[2].ti_Data = tp.tp_Out;
	t[3].ti_Tag = WAVEFORM_Regions; t[3].ti_Data = (ULONG) tp.tp_Region;
	t[4].ti_Tag = WAVEFORM_RegionCount; t[4].ti_Data = tp.tp_RegionCount;
	t[5].ti_Tag = WAVEFORM_Cursor; t[5].ti_Data = tp.tp_In;
	t[6].ti_Tag = TAG_DONE;
	set(wave, t);
	t[3].ti_Tag = TAG_DONE;
	set(over, t);
	check(get(wave, WAVEFORM_TrimIn) == tp.tp_In && get(wave, WAVEFORM_Zoom) == (wp.wp_Frames + 619) / 620,
	      "In set, the whole file in 620 columns");
	/* the bevel made: the first column inside its frame (2 points) */
	check(Waveform_ColumnOf(wave, get(wave, WAVEFORM_Top)) == 2 && Waveform_FrameAt(wave, 2) == get(wave, WAVEFORM_Top),
	      "the frame there: the first column 2 points in");
	dump("h-b");
	/* the source set again, nothing new in it (a read's every per cent):
	   nothing to paint */
	set1(wave, WAVEFORM_Source, (ULONG) &src);
	set1(over, WAVEFORM_Source, (ULONG) &src);
	if (verbose || gfx_rects || gfx_texts) printf("     the same source again: %ld rects, %ld texts\n", gfx_rects, gfx_texts);
	check(gfx_rects == 0 && gfx_texts == 0, "the same source again: nothing painted");
	gfx_rects = gfx_texts = 0;

	/* c: At In, 3 s around it */
	r.wr_First = tp.tp_In - 66150;
	r.wr_End = tp.tp_In + 66150;
	r.wr_Margin = 0;
	set1(wave, WAVEFORM_ShowRange, (ULONG) &r);
	zoom = get(wave, WAVEFORM_Zoom);
	{
		struct WaveWant *w = (struct WaveWant *) get(wave, WAVEFORM_Want);

		check(w->ww_First == get(wave, WAVEFORM_Top) && w->ww_Frames <= 620 * zoom,
		      "finer than the picture: the view's samples wanted, no more");
	}
	check(pixels_of(wave, MISSING_PEN) == 0, "finer than the picture, no samples yet: the picture stretched, nothing Missing");
	answer_wants(wave);
	check(zoom == (132300 + 619) / 620, "At In: 3 s across the view");
	t[0].ti_Tag = WAVEFORM_BoxTop; t[0].ti_Data = get(wave, WAVEFORM_Top);
	t[1].ti_Tag = WAVEFORM_BoxVisible; t[1].ti_Data = get(wave, WAVEFORM_Visible);
	t[2].ti_Tag = TAG_DONE;
	set(over, t);
	dump("h-c");

	/* d: In dragged 40 pixels to the right */
	in0 = get(wave, WAVEFORM_TrimIn);
	x = Waveform_ColumnOf(wave, in0);
	hover(wave, (WORD) (x + 6), 15);
	check(gfx_pointer == POINTERTYPE_EASTWESTRESIZE, "over In's flag: the two-headed arrow (it grabs In)");
	r1 = hover(wave, (WORD) x, 80);
	check(gfx_pointer == POINTERTYPE_EASTWESTRESIZE, "over In: the two-headed arrow");
	check((r1 & GMR_HELPCODE) && r1 != GMR_HELPHIT && strcmp(bubble(wave), "In 0:03.600") == 0,
	      "over In: the bubble says In 0:03.600");
	r2 = hover(wave, (WORD) (x + 30), 80);
	check(gfx_pointer == POINTERTYPE_TEXT, "over the sound: the I-beam");
	{
		struct WaveClock ck;
		char t[WAVEPLAN_TIMELEN];

		ck.wc_Rate = 44100;
		ck.wc_Origin = 0;
		ck.wc_Format = WAVEFORM_TIME_AUTO;
		ck.wc_Decimals = 3;
		WavePlan_Time(&ck, Waveform_FrameAt(wave, x + 30), t);
		check(strcmp(bubble(wave), t) == 0 && r2 != r1 && (r2 & GMR_HELPCODE),
		      "over the sound: the bubble is the time there, a new help code");
	}
	check(hover(wave, (WORD) (x + 30), 3) == GMR_NOHELPHIT && bubble(wave)[0] == '\0',
	      "over the ruler: no bubble");
	check(input(wave, GM_GOACTIVE, SELECTDOWN, (WORD) x, 80) == GMR_MEACTIVE, "In grabbed");
	for (f = 1; f <= 8; f++) input(wave, GM_HANDLEINPUT, IECODE_NOBUTTON, (WORD) (x + 5 * (LONG) f), 80);
	check(input(wave, GM_HANDLEINPUT, SELECTUP, (WORD) (x + 40), 80) == (GMR_NOREUSE | GMR_VERIFY), "let go: VERIFY");
	in1 = get(wave, WAVEFORM_TrimIn);
	if (verbose) printf("     In %lu -> %lu (%ld columns at %lu frames)\n", in0, in1, (long) (in1 - in0) / (long) zoom, zoom);
	check(in1 > in0 && (in1 - in0) / zoom >= 39 && (in1 - in0) / zoom <= 41, "In moved by 40 columns");
	check(Waveform_Changes(wave) & WAVEFORM_CHANGED_TRIM, "the change said");
	dump("h-d");

	/* e: the play head, from the program's task -- floating over the
	   lanes, composed off the window: a move is one blit into it (the
	   buffer's columns and the line, put together in the head's bitmap),
	   nothing drawn into the window, nothing painted anew; a far move
	   gives the old place back first (a blit of its own); within its
	   column nothing at all */
	Waveform_ShowPosition(wave, &win, in1 + 30 * zoom);
	{
		long r0 = gfx_rects, b0 = gfx_blits, wr0 = gfx_winrects, wb0 = gfx_winblits;

		Waveform_ShowPosition(wave, &win, in1 + 32 * zoom);
		if (verbose)
			printf("     a head moved 2 columns: %ld window blits, %ld window rects, %ld rects\n", gfx_winblits - wb0,
			       gfx_winrects - wr0, gfx_rects - r0);
		check(gfx_winblits - wb0 == 1 && gfx_winrects == wr0 && gfx_rects - r0 >= 1 && gfx_rects - r0 <= 6,
		      "a head moved: one blit into the window, the line drawn off it, no column painted");
		r0 = gfx_rects;
		b0 = gfx_blits;
		wr0 = gfx_winrects;
		wb0 = gfx_winblits;
		Waveform_ShowPosition(wave, &win, in1 + 120 * zoom);
		if (verbose)
			printf("     a head moved far: %ld window blits, %ld window rects\n", gfx_winblits - wb0, gfx_winrects - wr0);
		check(gfx_winblits - wb0 == 2 && gfx_winrects == wr0, "a head moved far: its old place back, then the new one, a blit each");
		r0 = gfx_rects;
		b0 = gfx_blits;
		Waveform_ShowPosition(wave, &win, in1 + 120 * zoom + 1);
		check(gfx_blits == b0 && gfx_rects == r0, "a head within its column: nothing drawn");
		Waveform_ShowPosition(wave, &win, in1 + 120 * zoom);
		/* a sprite takes the head (WAVEFORM_HeadOff) and gives it back
		   (a Pause): the head alone, as a move -- no column planned or
		   painted (an A2000 planned both views whole: 130 ms) -- and the
		   pixels a whole paint gives */
		{
			static UBYTE withhead[FB_H][FB_W], nohead[FB_H][FB_W];
			struct Gadget *g = (struct Gadget *) wave;

			memcpy(withhead, fb, sizeof(withhead));
			r0 = gfx_rects;
			wr0 = gfx_winrects;
			wb0 = gfx_winblits;
			set1(wave, WAVEFORM_HeadOff, TRUE);
			if (verbose)
				printf("     a sprite took the head: %ld window blits, %ld window rects, %ld rects\n", gfx_winblits - wb0,
				       gfx_winrects - wr0, gfx_rects - r0);
			check(gfx_winblits - wb0 == 1 && gfx_winrects == wr0 && gfx_rects == r0,
			      "a sprite took the head: its place given back, one blit, nothing painted");
			memcpy(nohead, fb, sizeof(nohead));
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(memcmp(nohead, fb, sizeof(nohead)) == 0 && memcmp(nohead, withhead, sizeof(nohead)) != 0,
			      "the head taken: the pixels a paint without it gives");
			r0 = gfx_rects;
			wr0 = gfx_winrects;
			wb0 = gfx_winblits;
			set1(wave, WAVEFORM_HeadOff, FALSE);
			if (verbose)
				printf("     the head given back: %ld window blits, %ld window rects, %ld rects\n", gfx_winblits - wb0,
				       gfx_winrects - wr0, gfx_rects - r0);
			check(gfx_winblits - wb0 == 1 && gfx_winrects == wr0 && gfx_rects - r0 >= 1 && gfx_rects - r0 <= 6,
			      "the head given back: drawn as a move, no column painted");
			check(memcmp(withhead, fb, sizeof(withhead)) == 0, "the head given back: the pixels as before");
			/* the head gone (a Stop: WAVEFORM_Position NONE), the view
			   where it was: the same, its place given back alone */
			r0 = gfx_rects;
			wr0 = gfx_winrects;
			wb0 = gfx_winblits;
			set1(wave, WAVEFORM_Position, WAVEFORM_NONE);
			check(gfx_winblits - wb0 == 1 && gfx_winrects == wr0 && gfx_rects == r0 && memcmp(nohead, fb, sizeof(nohead)) == 0,
			      "the head gone (a Stop): its place given back, nothing painted, the pixels without it");
			Waveform_ShowPosition(wave, &win, in1 + 120 * zoom);
			check(memcmp(withhead, fb, sizeof(withhead)) == 0, "the head shown again: the pixels as before");
			/* laid out anew, not painted yet (a window resized): the
			   buffer made anew holds nothing -- the head's change paints
			   the view, not the head alone */
			{
				struct gpLayout gl;

				g->Width -= 16;
				gl.MethodID = GM_LAYOUT;
				gl.gpl_GInfo = &gi;
				gl.gpl_Initial = 0;
				dispatch(wave, &gl);
				r0 = gfx_rects;
				set1(wave, WAVEFORM_HeadOff, TRUE);
				if (verbose) printf("     the head taken, laid out anew: %ld rects\n", gfx_rects - r0);
				check(gfx_rects - r0 > 6, "the head taken, laid out anew and not painted: the view painted, not the head alone");
				place(wave, g->LeftEdge, g->TopEdge, (WORD) (g->Width + 16), g->Height);
				set1(wave, WAVEFORM_HeadOff, FALSE);
				check(memcmp(withhead, fb, sizeof(withhead)) == 0, "laid out back, the head given back: the pixels as before");
			}
		}
	}
	dump("h-e");

	/* f: dB scale, a selection */
	t[0].ti_Tag = WAVEFORM_Scale; t[0].ti_Data = WAVEFORM_SCALE_DB;
	t[1].ti_Tag = WAVEFORM_SelStart; t[1].ti_Data = in1 + 200 * zoom;
	t[2].ti_Tag = WAVEFORM_SelEnd; t[2].ti_Data = in1 + 350 * zoom;
	t[3].ti_Tag = WAVEFORM_Position; t[3].ti_Data = WAVEFORM_NONE;
	t[4].ti_Tag = TAG_DONE;
	set(wave, t);
	hover(wave, (WORD) Waveform_ColumnOf(wave, in1 + 200 * zoom), 80);
	check(gfx_pointer == POINTERTYPE_EASTWESTRESIZE, "over a selection's edge: the two-headed arrow");
	dump("h-f");

	/* g: pages followed, the samples of this page and the next handed as
	   the read-ahead hands them; the head run over the page, a step of
	   the next page's painting at each move -- the turn one blit of the
	   lanes and the head's, nothing painted; its pixels those a paint of
	   the same view gives */
	set1(wave, WAVEFORM_Follow, WAVEFORM_FOLLOW_PAGE);
	{
		static UBYTE before[FB_H][FB_W];
		struct Gadget *g = (struct Gadget *) wave;
		ULONG top = get(wave, WAVEFORM_Top), vis = get(wave, WAVEFORM_Visible), k;
		long br0, wb0, tp0;

		ws.wsm_First = top;
		ws.wsm_Frames = 2 * vis;
		ws.wsm_Channels = 2;
		ws.wsm_Data = all_samples + top * 2;
		t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
		t[1].ti_Tag = WAVEFORM_SamplesMore; t[1].ti_Data = TRUE;
		t[2].ti_Tag = TAG_DONE;
		set(wave, t);
		for (k = 1; k <= 40; k++) Waveform_ShowPosition(wave, &win, top + k * (vis / 50));
		br0 = gfx_rects - gfx_winrects;
		wb0 = gfx_winblits;
		tp0 = gfx_templates;
		Waveform_ShowPosition(wave, &win, top + vis + 10);
		if (verbose)
			printf("     the turn: %ld rects off the window, %ld templates, %ld window blits\n", gfx_rects - gfx_winrects - br0,
			       gfx_templates - tp0, gfx_winblits - wb0);
		check(get(wave, WAVEFORM_Top) == top + vis - 38 * zoom, "the page turned by its width less a sixteenth");
		/* the head's own line and marks off the window, as a move (e) */
		check(gfx_rects - gfx_winrects - br0 <= 6 && gfx_templates == tp0 && gfx_winblits - wb0 == 2,
		      "turned onto the page painted ahead: the lanes' blit and the head's, nothing painted");
		memcpy(before, fb, sizeof(before));
		dump("h-g");
		/* the same view painted whole (laid out again: everything anew) */
		place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
		check(memcmp(before, fb, sizeof(before)) == 0, "the page painted ahead: the pixels a paint of it gives");
		Waveform_ShowPosition(wave, &win, top + vis + 10 + 2 * vis);
		check(get(wave, WAVEFORM_Top) == top + vis + 10 + 2 * vis - 38 * zoom, "a jump past the next page: from the head, painted");
		/* the same painted by the program, 16 columns a piece (a slow
		   machine's: WAVEFORM_AheadColumns 0, Waveform_PaintAhead()):
		   ShowPosition() paints nothing ahead, each piece says its
		   columns, AheadLeft counts them down, the turn the same blits,
		   its pixels a paint's */
		{
			ULONG top2 = get(wave, WAVEFORM_Top), n, sum = 0, most = 0, shown = 0, left0;

			set1(wave, WAVEFORM_AheadColumns, 0);
			ws.wsm_First = top2;
			ws.wsm_Frames = 2 * vis;
			ws.wsm_Data = all_samples + top2 * 2;
			set(wave, t);
			left0 = get(wave, WAVEFORM_AheadLeft);
			for (k = 1; k <= 60; k++) {
				shown += Waveform_ShowPosition(wave, &win, top2 + k * (vis / 70));
				n = Waveform_PaintAhead(wave, &win, 16);
				sum += n;
				if (n > most) most = n;
			}
			if (verbose) printf("     16 a piece: %lu columns in all, %lu at most, %lu left at first\n", sum, most, left0);
			check(shown == 0 && most >= 16 && most < 160 && sum >= 600, "16 columns a piece painted ahead by the program, each said");
			check(left0 >= 600 && get(wave, WAVEFORM_AheadLeft) == 0, "the columns left: all at first, none once painted");
			/* the head taken by a sprite and given back meanwhile (the
			   window made inactive and active again): the page painted
			   ahead kept, the turn below its blit */
			set1(wave, WAVEFORM_HeadOff, TRUE);
			set1(wave, WAVEFORM_HeadOff, FALSE);
			check(get(wave, WAVEFORM_AheadLeft) == 0, "the head taken and given back: the page painted ahead kept");
			set1(wave, WAVEFORM_Position, top2 + 60 * (vis / 70));
			check(get(wave, WAVEFORM_AheadLeft) == 0, "the head set where it is (WAVEFORM_Position): the page painted ahead kept");
			wb0 = gfx_winblits;
			tp0 = gfx_templates;
			Waveform_ShowPosition(wave, &win, top2 + vis + 10);
			check(get(wave, WAVEFORM_Top) == top2 + vis - 38 * zoom && gfx_templates == tp0 && gfx_winblits - wb0 == 2,
			      "turned onto the page painted 16 columns a piece: the lanes' blit and the head's");
			memcpy(before, fb, sizeof(before));
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(memcmp(before, fb, sizeof(before)) == 0, "painted 16 columns a piece: the pixels a paint gives");
			set1(wave, WAVEFORM_AheadColumns, 160);
		}
		/* a turn before the next page was painted (a slow machine's), a
		   paint's budget 16 columns (WAVEFORM_PaintColumns): nothing
		   filled, planned or painted, the lanes cleared and counted, the
		   source asked for nothing; Waveform_PaintAhead() painting it a
		   piece at a time; then the pixels a paint of the view gives --
		   with a drawn head (the buffer cleared as well) and with a
		   sprite's (WAVEFORM_HeadOff: the window only) */
		{
			static UBYTE turned[FB_H][FB_W];
			ULONG top3, left0, k2, asked, pass;

			for (pass = 0; pass < 2; pass++) {
				top3 = get(wave, WAVEFORM_Top);
				set1(wave, WAVEFORM_HeadOff, pass);
				set1(wave, WAVEFORM_AheadColumns, 0);
				set1(wave, WAVEFORM_PaintColumns, 16);
				ws.wsm_First = top3;
				ws.wsm_Frames = 2 * vis;
				ws.wsm_Data = all_samples + top3 * 2;
				set(wave, t);
				peaks_asked = 0;
				Waveform_ShowPosition(wave, &win, top3 + vis + 10);
				asked = peaks_asked;
				left0 = get(wave, WAVEFORM_PaintLeft);
				memcpy(turned, fb, sizeof(turned));
				if (verbose) printf("     the turn on a budget (head %s): %lu columns left, %lu asked of the source\n",
						    pass ? "a sprite's" : "drawn", left0, asked);
				check(get(wave, WAVEFORM_Top) == top3 + vis - 38 * zoom && left0 == 620,
				      "turned before the next page was painted: the page left to its pieces");
				check(asked == 0, "the turned page not filled at the turn");
				{
					/* a column far right: its ground only, top to foot --
					   not the last page's wave */
					const struct WaveHeadBox *hb = (const struct WaveHeadBox *) get(wave, WAVEFORM_HeadBox);
					int x = hb->hb_Left + 500, y, plain = 1;

					/* (the box in the window's pixels) */
					for (y = hb->hb_Top + 1; y < hb->hb_Top + hb->hb_Rows; y++) plain &= fb[y][x] == fb[hb->hb_Top][x];
					check(plain, "the rest cleared to the ground, not the last page's wave");
				}
				for (k2 = 0; k2 < 100 && get(wave, WAVEFORM_PaintLeft); k2++) Waveform_PaintAhead(wave, &win, 16);
				check(get(wave, WAVEFORM_PaintLeft) == 0 && k2 >= 30, "the rest painted 16 a piece");
				if (pass) {
					/* a paint of what is there: the pieces' signatures a
					   whole plan's, nothing again */
					long wb1 = gfx_winblits, tp1 = gfx_templates, r1 = gfx_rects;

					update(wave);
					check(gfx_winblits == wb1 && gfx_templates == tp1 && gfx_rects == r1,
					      "planned a piece at a time as planned whole: nothing painted again");
				}
				memcpy(before, fb, sizeof(before));
				set1(wave, WAVEFORM_PaintColumns, 0);
				place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
				check(memcmp(before, fb, sizeof(before)) == 0 && memcmp(turned, fb, sizeof(turned)) != 0,
				      "painted in pieces: the pixels a paint gives (not those at the turn)");
				set1(wave, WAVEFORM_AheadColumns, 160);
			}
			set1(wave, WAVEFORM_HeadOff, FALSE);
			/* the same, the samples handed short of the turned page (the
			   read-ahead behind): nothing painted, its frames wanted; the
			   samples that hold it more of the same -- nothing painted
			   then either -- and the pieces paint it */
			{
				struct WaveWant *w;
				ULONG top4 = get(wave, WAVEFORM_Top), k3;
				long wb2;

				set1(wave, WAVEFORM_AheadColumns, 0);
				set1(wave, WAVEFORM_PaintColumns, 16);
				ws.wsm_First = top4;
				ws.wsm_Frames = vis / 2;
				ws.wsm_Data = all_samples + top4 * 2;
				t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
				t[1].ti_Tag = TAG_DONE;
				set(wave, t);
				Waveform_ShowPosition(wave, &win, top4 + vis + 10);
				(void) wants(wave);
				check(get(wave, WAVEFORM_PaintLeft) == 620 && !Waveform_PaintAhead(wave, &win, 16) &&
				      (w = wants(wave)) && w->ww_First == get(wave, WAVEFORM_Top),
				      "a turned page without its samples: nothing painted, its frames wanted");
				ws.wsm_Frames = 3 * vis;
				t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
				t[1].ti_Tag = WAVEFORM_SamplesMore; t[1].ti_Data = TRUE;
				t[2].ti_Tag = TAG_DONE;
				wb2 = gfx_winblits;
				set(wave, t);
				check(gfx_winblits == wb2 && get(wave, WAVEFORM_PaintLeft) == 620, "its samples come: more of the same, nothing painted");
				for (k3 = 0; k3 < 100 && get(wave, WAVEFORM_PaintLeft); k3++) Waveform_PaintAhead(wave, &win, 16);
				memcpy(before, fb, sizeof(before));
				set1(wave, WAVEFORM_PaintColumns, 0);
				place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
				check(k3 >= 30 && memcmp(before, fb, sizeof(before)) == 0, "then painted a piece at a time: the pixels a paint gives");
				set1(wave, WAVEFORM_AheadColumns, 160);
			}
			/* a turn while the last page fills in still (deeper than the
			   machine paints): the new page lazy as well, not planned whole */
			{
				ULONG top5 = get(wave, WAVEFORM_Top), asked2;

				set1(wave, WAVEFORM_AheadColumns, 0);
				set1(wave, WAVEFORM_PaintColumns, 16);
				ws.wsm_First = top5;
				ws.wsm_Frames = 3 * vis;
				ws.wsm_Data = all_samples + top5 * 2;
				t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
				t[1].ti_Tag = TAG_DONE;
				set(wave, t);
				Waveform_ShowPosition(wave, &win, top5 + vis + 10);
				peaks_asked = 0;
				Waveform_ShowPosition(wave, &win, get(wave, WAVEFORM_Top) + vis + 10);
				asked2 = peaks_asked;
				check(get(wave, WAVEFORM_PaintLeft) >= 560 && asked2 <= 40, "a turn while the last page filled in: lazy again");
				set1(wave, WAVEFORM_PaintColumns, 0);
				place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
				set1(wave, WAVEFORM_AheadColumns, 160);
			}
		}
		/* a new view not turned to, WAVEFORM_FillNew (a stopped view's
		   zoom, scroll, new samples, edit): nothing filled or painted at
		   once, the old picture kept -- a column far right shows what it
		   did; the pieces paint it, the pixels a paint of the view gives,
		   their signatures a whole plan's. A change while it fills in (a
		   selection) paints what it shows so far only, the source asked
		   nothing; the same source again paints no wave. Samples that
		   will not come (WAVEFORM_SamplesNone): stretched as a whole
		   paint stretches; samples that come later: the view new again */
		{
			static UBYTE old[FB_H][FB_W];
			const struct WaveHeadBox *hb;
			ULONG top6, z6, k4, asked3, left1, sel0;
			long tp2, r2;
			int x, y, same;

			set1(wave, WAVEFORM_Position, WAVEFORM_NONE);
			set1(wave, WAVEFORM_PaintColumns, 0);
			set1(wave, WAVEFORM_AheadColumns, 0);
			top6 = get(wave, WAVEFORM_Top);
			z6 = get(wave, WAVEFORM_Zoom);
			ws.wsm_First = top6;
			ws.wsm_Frames = 3 * vis;
			ws.wsm_Data = all_samples + top6 * 2;
			t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
			t[1].ti_Tag = TAG_DONE;
			set(wave, t);
			set1(wave, WAVEFORM_FillNew, TRUE);
			memcpy(old, fb, sizeof(old));
			peaks_asked = 0;
			tp2 = gfx_templates;
			t[0].ti_Tag = WAVEFORM_ZoomCenter; t[0].ti_Data = top6;
			t[1].ti_Tag = WAVEFORM_ZoomStep; t[1].ti_Data = 1;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			asked3 = peaks_asked;
			hb = (const struct WaveHeadBox *) get(wave, WAVEFORM_HeadBox);
			x = hb->hb_Left + 500;
			same = 1;
			for (y = hb->hb_Top; y < hb->hb_Top + hb->hb_Rows; y++) same &= fb[y][x] == old[y][x];
			if (verbose) printf("     a zoom filling in: zoom %lu -> %lu, %lu left, %lu asked of the source\n", z6,
					    get(wave, WAVEFORM_Zoom), get(wave, WAVEFORM_PaintLeft), asked3);
			check(get(wave, WAVEFORM_Zoom) < z6 && get(wave, WAVEFORM_PaintLeft) == 620 && asked3 == 0 && gfx_templates == tp2,
			      "a zoom filling in: nothing filled or painted at once");
			check(same, "a zoom filling in: the old picture kept");
			/* a refresh while it fills in (a window over ours closed):
			   what was there given back, nothing painted or counted as
			   painted -- on the Amiga the refresh is clipped to the
			   damage, and the columns it painted kept the picture before
			   everywhere else (A4000T, the Gain window, 2026-11-15) */
			memcpy(before, fb, sizeof(before));
			tp2 = gfx_templates;
			refreshed(wave);
			if (verbose) printf("     a refresh while it fills in: %lu left, %ld templates\n", get(wave, WAVEFORM_PaintLeft),
					    gfx_templates - tp2);
			check(memcmp(before, fb, sizeof(before)) == 0 && get(wave, WAVEFORM_PaintLeft) == 620 && gfx_templates == tp2,
			      "a refresh while a view fills in: the picture there given back, nothing painted, all still to fill in");
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			tp2 = gfx_templates;
			r2 = gfx_rects;
			update(wave);
			check(k4 >= 30 && get(wave, WAVEFORM_PaintLeft) == 0 && gfx_templates == tp2 && gfx_rects == r2,
			      "a zoom filled in a piece at a time, planned as a whole plan: no wave painted again");
			memcpy(before, fb, sizeof(before));
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(memcmp(before, fb, sizeof(before)) == 0, "a zoom filled in: the pixels a paint gives");
			/* back out, a few pieces, a selection over half the view */
			t[0].ti_Tag = WAVEFORM_ZoomCenter; t[0].ti_Data = top6;
			t[1].ti_Tag = WAVEFORM_ZoomStep; t[1].ti_Data = (ULONG) -1L;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			for (k4 = 0; k4 < 5; k4++) Waveform_PaintAhead(wave, &win, 16);
			left1 = get(wave, WAVEFORM_PaintLeft);
			peaks_asked = 0;
			sel0 = get(wave, WAVEFORM_Top);
			t[0].ti_Tag = WAVEFORM_SelStart; t[0].ti_Data = sel0;
			t[1].ti_Tag = WAVEFORM_SelEnd; t[1].ti_Data = sel0 + get(wave, WAVEFORM_Visible) / 2;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			if (verbose) printf("     a selection while it fills in: %lu left before, %lu after, %lu asked\n", left1,
					    get(wave, WAVEFORM_PaintLeft), peaks_asked);
			check(left1 >= 500 && get(wave, WAVEFORM_PaintLeft) == left1 && peaks_asked == 0,
			      "a selection while a view fills in: in what it shows, the source asked nothing");
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			memcpy(before, fb, sizeof(before));
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(memcmp(before, fb, sizeof(before)) == 0, "then filled in: the pixels a paint with the selection gives");
			/* the same source again: lazy, no wave painted */
			tp2 = gfx_templates;
			r2 = gfx_rects;
			set1(wave, WAVEFORM_Source, (ULONG) &src);
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			if (verbose) printf("     the same source again: %ld templates, %ld rects\n", gfx_templates - tp2, gfx_rects - r2);
			check(k4 >= 1 && gfx_templates == tp2 && gfx_rects == r2, "the same source again, filling in: no wave painted");
			/* samples for half the view, the rest will not come: the half
			   painted, then nothing until told, then the rest stretched as
			   a whole paint stretches it */
			ws.wsm_First = get(wave, WAVEFORM_Top);
			ws.wsm_Frames = get(wave, WAVEFORM_Visible) / 2;
			ws.wsm_Data = all_samples + ws.wsm_First * 2;
			t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
			t[1].ti_Tag = TAG_DONE;
			set(wave, t);
			(void) wants(wave);
			for (k4 = 0; k4 < 100 && Waveform_PaintAhead(wave, &win, 16); k4++) {
			}
			left1 = get(wave, WAVEFORM_PaintLeft);
			if (verbose) printf("     half the samples: %lu pieces, %lu left waiting\n", k4, left1);
			check(left1 >= 250 && left1 <= 420 && wants(wave) != NULL && wants(wave)->ww_First != WAVEFORM_NONE,
			      "half the samples: that half painted, the view waits for the rest, wanted");
			set1(wave, WAVEFORM_SamplesNone, TRUE);
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			memcpy(old, fb, sizeof(old));
			check(k4 >= 15 && get(wave, WAVEFORM_PaintLeft) == 0, "none will come: the rest painted, stretched");
			/* they come after all, more of the same: the view new again
			   (its stretched columns not kept), filled in */
			sel0 = ws.wsm_Frames;
			ws.wsm_Frames = 2 * get(wave, WAVEFORM_Visible);
			t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
			t[1].ti_Tag = WAVEFORM_SamplesMore; t[1].ti_Data = TRUE;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			left1 = get(wave, WAVEFORM_PaintLeft);
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			memcpy(before, fb, sizeof(before));
			set1(wave, WAVEFORM_FillNew, FALSE);
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(left1 == 620 && memcmp(before, fb, sizeof(before)) == 0 && memcmp(old, fb, sizeof(old)) != 0,
			      "the samples after the stretched picture: filled in anew, the pixels a paint with them gives");
			/* the stretched picture was a whole paint's with half the samples */
			ws.wsm_Frames = sel0;
			t[0].ti_Tag = WAVEFORM_Samples; t[0].ti_Data = (ULONG) &ws;
			t[1].ti_Tag = TAG_DONE;
			set(wave, t);
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(memcmp(old, fb, sizeof(old)) == 0, "none will come: the rest stretched, the pixels a whole paint gives");
			t[0].ti_Tag = WAVEFORM_SelStart; t[0].ti_Data = WAVEFORM_NONE;
			t[1].ti_Tag = WAVEFORM_SelEnd; t[1].ti_Data = WAVEFORM_NONE;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			/* a change in the right half only (a selection from 45 per
			   cent, the source set again: a kept view new): fewer pieces
			   than columns over 16 (four pieces' looked at through the
			   left), and none paints more than its 16 columns -- also the
			   one whose look-ahead meets the change in its second chunk */
			{
				ULONG half = get(wave, WAVEFORM_Top) + get(wave, WAVEFORM_Visible) / 20 * 9, pieces, most = 0, cnt;
				int xx, yy, diff;

				ws.wsm_First = 0;
				ws.wsm_Frames = get(wave, WAVEFORM_Frames);
				ws.wsm_Data = all_samples;
				set1(wave, WAVEFORM_FillNew, FALSE);
				set1(wave, WAVEFORM_Samples, (ULONG) &ws);
				set1(wave, WAVEFORM_FillNew, TRUE);
				hb = (const struct WaveHeadBox *) get(wave, WAVEFORM_HeadBox);
				t[0].ti_Tag = WAVEFORM_Source; t[0].ti_Data = (ULONG) &src;
				t[1].ti_Tag = WAVEFORM_SelStart; t[1].ti_Data = half;
				t[2].ti_Tag = WAVEFORM_SelEnd; t[2].ti_Data = half + get(wave, WAVEFORM_Visible) / 3;
				t[3].ti_Tag = TAG_DONE;
				set(wave, t);
				for (pieces = 0; pieces < 100 && get(wave, WAVEFORM_PaintLeft); pieces++) {
					memcpy(old, fb, sizeof(old));
					Waveform_PaintAhead(wave, &win, 16);
					cnt = 0;
					for (xx = hb->hb_Left; xx < hb->hb_Left + (int) hb->hb_Columns; xx++) {
						diff = 0;
						for (yy = hb->hb_Top; yy < hb->hb_Top + hb->hb_Rows; yy++) diff |= fb[yy][xx] != old[yy][xx];
						cnt += (ULONG) diff;
					}
					if (cnt > most) most = cnt;
				}
				if (verbose) printf("     the right half changed: %lu pieces, %lu columns at most a piece\n", pieces, most);
				memcpy(before, fb, sizeof(before));
				set1(wave, WAVEFORM_FillNew, FALSE);
				place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
				check(pieces <= 30 && most >= 8 && most <= 16 && memcmp(before, fb, sizeof(before)) == 0,
				      "the right half changed: the left looked through, no piece over its columns, a paint's pixels");
				t[0].ti_Tag = WAVEFORM_SelStart; t[0].ti_Data = WAVEFORM_NONE;
				t[1].ti_Tag = WAVEFORM_SelEnd; t[1].ti_Data = WAVEFORM_NONE;
				t[2].ti_Tag = TAG_DONE;
				set(wave, t);
			}
			/* a drawn head through a zoom filling in (the samples of the
			   whole sound): the pixels a paint gives */
			ws.wsm_First = 0;
			ws.wsm_Frames = get(wave, WAVEFORM_Frames);
			ws.wsm_Data = all_samples;
			set1(wave, WAVEFORM_Samples, (ULONG) &ws);
			set1(wave, WAVEFORM_FillNew, TRUE);
			set1(wave, WAVEFORM_Position, get(wave, WAVEFORM_Top) + get(wave, WAVEFORM_Visible) / 3);
			t[0].ti_Tag = WAVEFORM_ZoomCenter; t[0].ti_Data = get(wave, WAVEFORM_Position);
			t[1].ti_Tag = WAVEFORM_ZoomStep; t[1].ti_Data = (ULONG) -1L;
			t[2].ti_Tag = TAG_DONE;
			set(wave, t);
			for (k4 = 0; k4 < 100 && get(wave, WAVEFORM_PaintLeft); k4++) Waveform_PaintAhead(wave, &win, 16);
			memcpy(before, fb, sizeof(before));
			set1(wave, WAVEFORM_FillNew, FALSE);
			place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
			check(k4 >= 30 && memcmp(before, fb, sizeof(before)) == 0, "a drawn head through a zoom filling in: the pixels a paint gives");
			set1(wave, WAVEFORM_Position, WAVEFORM_NONE);
			set1(wave, WAVEFORM_AheadColumns, 160);
		}
		set1(wave, WAVEFORM_Position, WAVEFORM_NONE);
	}

	/* h: the level -- a gained part (+6 dB, its tag), a fade out after
	   it, the end fades on the handles: the fade's line either side of
	   each lane's centre, the gain's bar along the foot, the tag's box
	   and label; In moved, its fade goes with it; each update the pixels
	   a full paint gives; the levels gone, nothing of them left */
	{
		static UBYTE before[FB_H][FB_W];
		static struct WaveLevel lv[2];
		struct Gadget *g = (struct Gadget *) wave;
		const struct WaveHeadBox *hb;
		ULONG in = get(wave, WAVEFORM_TrimIn), out = get(wave, WAVEFORM_TrimOut), span = out - in, fade = span / 16;
		LONG lx, half, lh, y, col, n, tagged = 0, bar = 0, fadecol = -1, endcol = -1;

		set1(wave, WAVEFORM_LevelPen, LEVEL_PEN);
		set1(wave, WAVEFORM_ShowAll, TRUE);
		memset(lv, 0, sizeof(lv));
		lv[0].wlv_First = in + span / 4;
		lv[0].wlv_End = in + span / 2;
		lv[0].wlv_Gain = 8192;
		lv[0].wlv_Label = (CONST_STRPTR) "+6.0 dB";
		lv[1].wlv_First = in + span / 2;
		lv[1].wlv_End = in + span / 2 + span / 8;
		lv[1].wlv_Gain = WAVELEVEL_UNITY;
		lv[1].wlv_Fade = WAVELEVEL_OUT | WAVELEVEL_EVEN;
		lv[1].wlv_U1 = WAVELEVEL_U_MAX;
		t[0].ti_Tag = WAVEFORM_Levels; t[0].ti_Data = (ULONG) lv;
		t[1].ti_Tag = WAVEFORM_LevelCount; t[1].ti_Data = 2;
		t[2].ti_Tag = WAVEFORM_FadeIn; t[2].ti_Data = fade;
		t[3].ti_Tag = WAVEFORM_FadeOut; t[3].ti_Data = fade;
		t[4].ti_Tag = WAVEFORM_SelStart; t[4].ti_Data = WAVEFORM_NONE;
		t[5].ti_Tag = WAVEFORM_SelEnd; t[5].ti_Data = WAVEFORM_NONE;
		t[6].ti_Tag = TAG_DONE;
		set(wave, t);
		hb = (const struct WaveHeadBox *) get(wave, WAVEFORM_HeadBox);
		lx = hb->hb_Left;
		lh = (hb->hb_Rows - 1) / 2;
		half = lh / 2;
		check(get(wave, WAVEFORM_FadeIn) == fade && get(wave, WAVEFORM_FadeOut) == fade, "the end fades kept");
		/* a column a quarter into the fade out: two dots a lane, level
		   with each other about the centre, inside the lane */
		col = Waveform_ColumnOf(wave, lv[1].wlv_First + span / 32);
		for (n = 0, y = hb->hb_Top; y < hb->hb_Top + hb->hb_Rows; y++) n += fb[y][lx + col] == LEVEL_PEN;
		{
			LONG up = -1, down = -1, mid = hb->hb_Top + lh / 2;

			for (y = hb->hb_Top; y < mid; y++) if (fb[y][lx + col] == LEVEL_PEN) up = mid - y;
			for (y = hb->hb_Top + lh - 1; y > mid; y--) if (fb[y][lx + col] == LEVEL_PEN) down = y - mid;
			if (verbose) printf("     the fade's column %ld: %ld dots, %ld up, %ld down of %ld\n", col, n, up, down, half);
			check(n == 4 && up > 2 && up < half && down == up, "a fade's column: its level either side of each lane's centre");
		}
		/* the gained part: the bar along the foot, no line (a gain alone);
		   past its tag, short of the column it shares with the fade */
		for (col = Waveform_ColumnOf(wave, lv[0].wlv_First) + 60; col < Waveform_ColumnOf(wave, lv[0].wlv_End - 1) - 2; col++) {
			for (n = 0, y = hb->hb_Top; y < hb->hb_Top + hb->hb_Rows; y++) n += fb[y][lx + col] == LEVEL_PEN;
			bar += n == 2 && fb[hb->hb_Top + hb->hb_Rows - 1][lx + col] == LEVEL_PEN &&
			       fb[hb->hb_Top + hb->hb_Rows - 2][lx + col] == LEVEL_PEN;
			tagged++;
		}
		check(tagged > 20 && bar == tagged, "a gain: its bar along the foot, two lines, nothing more");
		/* its tag: a box on the bar at its start, the label in it */
		col = Waveform_ColumnOf(wave, lv[0].wlv_First);
		y = hb->hb_Top + hb->hb_Rows - 1;
		for (n = 0, x = lx + col + 1; x < lx + col + 50; x++) n += fb[y - 5][x] != LEVEL_PEN;
		check(fb[y - 1][lx + col] == LEVEL_PEN && fb[y - 10][lx + col] == LEVEL_PEN && fb[y - 11][lx + col] != LEVEL_PEN && n > 4,
		      "the gain's tag: a box ten lines high on the bar, its label in it");
		/* the end fades: In's half way in has its line, a column past it
		   none -- in the second lane (In's flag lies over the first's) */
		fadecol = Waveform_ColumnOf(wave, in + fade / 2);
		endcol = Waveform_ColumnOf(wave, in + fade + fade / 4);
		for (n = 0, y = hb->hb_Top + lh + 1; y < hb->hb_Top + hb->hb_Rows; y++)
			n += (fb[y][lx + fadecol] == LEVEL_PEN) + (fb[y][lx + endcol] == LEVEL_PEN) * 100;
		check(n == 2, "In's fade: its line half way in, none past it");
		/* its line whole: each column's points meet the column before's,
		   however steep (the second lane's upper half) */
		{
			LONG top1 = hb->hb_Top + lh + 1, mid1 = top1 + lh / 2, lo, hi, plo = -1, phi = -1, gaps = 0, cols = 0;

			for (col = Waveform_ColumnOf(wave, in) + 1; col < Waveform_ColumnOf(wave, in + fade) - 1; col++) {
				for (lo = -1, hi = -1, y = top1; y < mid1; y++) {
					if (fb[y][lx + col] != LEVEL_PEN) continue;
					if (lo < 0) lo = y;
					hi = y;
				}
				cols++;
				if (lo < 0 || (plo >= 0 && (hi < plo - 1 || lo > phi + 1))) gaps++;
				plo = lo;
				phi = hi;
			}
			check(cols > 10 && gaps == 0, "In's fade: a line without gaps, however steep");
		}
		memcpy(before, fb, sizeof(before));
		place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
		check(memcmp(before, fb, sizeof(before)) == 0, "the levels updated: the pixels a full paint gives");
		dump("h-h");

		/* the gained part faded out to nothing: the fade after it starts
		   at its old level, joined by a line down its first column --
		   a column whose own level stayed, repainted for its neighbour's */
		lv[0].wlv_Fade = WAVELEVEL_OUT | WAVELEVEL_LINEAR;
		lv[0].wlv_U1 = WAVELEVEL_U_MAX;
		set1(wave, WAVEFORM_Levels, (ULONG) lv);
		memcpy(before, fb, sizeof(before));
		place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
		check(memcmp(before, fb, sizeof(before)) == 0, "the line joined across two levels: the pixels a full paint gives");
		lv[0].wlv_Fade = WAVELEVEL_NONE;
		lv[0].wlv_U1 = 0;
		set1(wave, WAVEFORM_Levels, (ULONG) lv);

		/* In moved on by its fade's length: the fade with it, a column
		   past the old fade's end now has the line */
		set1(wave, WAVEFORM_TrimIn, in + fade);
		for (n = 0, y = hb->hb_Top + lh + 1; y < hb->hb_Top + hb->hb_Rows; y++)
			n += (fb[y][lx + endcol] == LEVEL_PEN) + (fb[y][lx + fadecol] == LEVEL_PEN) * 100;
		check(n == 2 && get(wave, WAVEFORM_FadeIn) == fade, "In moved: its fade with it, its length kept");
		memcpy(before, fb, sizeof(before));
		place(wave, g->LeftEdge, g->TopEdge, g->Width, g->Height);
		check(memcmp(before, fb, sizeof(before)) == 0, "the fade moved by an update: the pixels a full paint gives");

		/* gone: nothing of the level left */
		t[0].ti_Tag = WAVEFORM_LevelCount; t[0].ti_Data = 0;
		t[1].ti_Tag = WAVEFORM_FadeIn; t[1].ti_Data = 0;
		t[2].ti_Tag = WAVEFORM_FadeOut; t[2].ti_Data = 0;
		t[3].ti_Tag = WAVEFORM_TrimIn; t[3].ti_Data = in;
		t[4].ti_Tag = TAG_DONE;
		set(wave, t);
		check(pixels_of(wave, LEVEL_PEN) == 0, "the levels gone: none of their pixels left");
	}

	/* no sound (none opened yet, or being decoded): no bubble at all --
	   gadgetclass's own hit opened an empty one */
	set1(wave, WAVEFORM_Source, 0);
	check(hover(wave, 200, 80) == GMR_NOHELPHIT && bubble(wave)[0] == '\0', "no sound: no bubble");

	/* g: everything given back -- memory, pens, objects, libraries */
	DisposeObject(wave);
	DisposeObject(over);
	Waveform_FreeClass(cl);
	free(samples);
	if (gfx_vecs || gfx_pens || gfx_objs || gfx_libs || gfx_bitmaps)
		printf("     still held: %ld blocks, %ld pens, %ld objects, %ld libraries, %ld bitmaps\n", gfx_vecs, gfx_pens, gfx_objs,
		       gfx_libs, gfx_bitmaps);
	check(!gfx_vecs && !gfx_pens && !gfx_objs && !gfx_libs && !gfx_bitmaps, "disposed: nothing held");
	check(gfx_standard ? gfx_templates > 0 : gfx_templates == 0, "the wave through the mask only on planar bitmaps");
	check(gfx_blits > 0, "the lanes came through the buffer (blits)");

	printf("wavegfx%s: %d checks, %d failed\n", argc > 3 ? " (no pens)" : "", checks, failures);
	return failures ? 1 : 0;
}
