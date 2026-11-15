/*
 * about.c -- the About window (about.h)
 *
 * The picture's palette has an alpha per entry (tools/make_about_pic.py):
 * each entry is blended with the colour of the screen's BACKGROUNDPEN --
 * what window.class fills the window with -- and then becomes a pen one of
 * two ways, as in Herald:
 *
 *   16 colours or more   the exact colour, from ObtainBestPenA -- all of
 *                        them or none, given back when it shuts
 *   anything else        the nearest colour the screen already has, found
 *                        with GetRGB32 and borrowed, never allocated: it
 *                        cannot fail and takes no pen another program
 *                        wants (an 8-colour Workbench has none to give)
 *
 * Drawn in runs, not pixels: each run is one RectFill.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <images/bevel.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <clib/alib_protos.h>

#include "about.h"
#include "about_pic.h"
#include "wavesmith_msg.h"
#include "textutil.h"
#include "keyglyph.h"
#include "WaveSmith_rev.h"

#define GID_OK		1
#define ABOUT_LINES	8		/* lines of text at most */
#define ABOUT_TEXT	384
#define EXACT_DEPTH	4		/* from 16 colours up the exact colours are asked for */
#define RAW_RETURN	0x44
#define RAW_ENTER	0x43
#define RAW_ESC		0x45

struct AboutWin {
	Object *aw_WinObj;
	struct Window *aw_Window;
	Object *aw_Pic;			/* the picture's place: an empty layout of its size */
	struct ColorMap *aw_Map;	/* the screen's */
	BOOL aw_Obtained;		/* the pens ours, to give back */
	BOOL aw_HavePens;
	UWORD aw_SX;			/* screen pixels a picture pixel is wide: 2 where they
					   are twice as tall as wide (hires, not laced) */
	ULONG aw_Pens[ABOUT_PIC_COLOURS];
	UBYTE aw_Rgb[ABOUT_PIC_COLOURS * 3];	/* the entries over the background */
	char aw_Text[ABOUT_TEXT];	/* the lines' (GA_Text keeps the pointer) */
};

static struct AboutWin *g_About = NULL;

/* SAS/C follows no write through an index: an array cleared as a block */
static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

/* the entries over the background colour (32-bit guns, the top byte) */
static void blend_all(struct AboutWin *aw, const ULONG *bg)
{
	const UBYTE *e = about_rgba();
	UWORD i, c;
	ULONG a;

	for (i = 0; i < ABOUT_PIC_COLOURS; i++, e += 4) {
		a = e[3];
		for (c = 0; c < 3; c++)
			aw->aw_Rgb[i * 3 + c] = (UBYTE) (((ULONG) e[c] * a + (bg[c] >> 24) * (255UL - a) + 127UL) / 255UL);
	}
}

/* every entry's exact colour, or none (the ones had given back) */
static BOOL pens_obtain(struct AboutWin *aw)
{
	const UBYTE *rgb;
	UWORD i;
	LONG pen;

	for (i = 0; i < ABOUT_PIC_COLOURS; i++) {
		rgb = &aw->aw_Rgb[i * 3];
		pen = ObtainBestPenA(aw->aw_Map, (ULONG) rgb[0] * 0x01010101UL, (ULONG) rgb[1] * 0x01010101UL,
				     (ULONG) rgb[2] * 0x01010101UL, NULL);
		if (pen < 0) {
			while (i > 0) ReleasePen(aw->aw_Map, aw->aw_Pens[--i]);
			return FALSE;
		}
		aw->aw_Pens[i] = (ULONG) pen;
	}
	return TRUE;
}

/* how far a screen colour (32-bit guns) lies from an entry */
static LONG distance(const ULONG *screen, const UBYTE *want)
{
	WORD dr = (WORD) ((WORD) (screen[0] >> 24) - (WORD) want[0]);
	WORD dg = (WORD) ((WORD) (screen[1] >> 24) - (WORD) want[1]);
	WORD db = (WORD) ((WORD) (screen[2] >> 24) - (WORD) want[2]);

	return (LONG) dr * dr + (LONG) dg * dg + (LONG) db * db;
}

/* each entry the nearest colour the screen has: one pass over its pens */
static void pens_borrow(struct AboutWin *aw, UWORD depth)
{
	LONG best[ABOUT_PIC_COLOURS], d;
	ULONG rgb[3], pens = depth >= 8 ? 256UL : 1UL << depth, p;
	UWORD i;

	for (i = 0; i < ABOUT_PIC_COLOURS; i++) {
		best[i] = 0x7FFFFFFFL;
		aw->aw_Pens[i] = 0;
	}
	for (p = 0; p < pens; p++) {
		GetRGB32(aw->aw_Map, p, 1, rgb);
		for (i = 0; i < ABOUT_PIC_COLOURS; i++) {
			if ((d = distance(rgb, &aw->aw_Rgb[i * 3])) < best[i]) {
				best[i] = d;
				aw->aw_Pens[i] = p;
			}
		}
	}
}

static void pens_get(struct AboutWin *aw)
{
	struct Screen *scr = aw->aw_Window->WScreen;
	struct DrawInfo *dri;
	ULONG bg[3], pen = 0, depth;

	if (!(aw->aw_Map = scr->ViewPort.ColorMap)) return;
	if ((dri = GetScreenDrawInfo(scr)) != NULL) {
		pen = dri->dri_Pens[BACKGROUNDPEN];
		FreeScreenDrawInfo(scr, dri);
	}
	GetRGB32(aw->aw_Map, pen, 1, bg);
	blend_all(aw, bg);
	depth = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH);
	aw->aw_Obtained = (BOOL) (depth >= EXACT_DEPTH && pens_obtain(aw));
	if (!aw->aw_Obtained) pens_borrow(aw, (UWORD) depth);
	aw->aw_HavePens = TRUE;
}

static void pens_free(struct AboutWin *aw)
{
	UWORD i;

	if (aw->aw_Obtained)
		for (i = 0; i < ABOUT_PIC_COLOURS; i++) ReleasePen(aw->aw_Map, aw->aw_Pens[i]);
	aw->aw_Obtained = aw->aw_HavePens = FALSE;
}

/* the runs, each a span filled */
static void pic_draw(struct AboutWin *aw)
{
	const UBYTE *r = about_runs();
	struct Gadget *g = (struct Gadget *) aw->aw_Pic;
	struct RastPort *rp = aw->aw_Window->RPort;
	WORD x = 0, y = 0;
	UWORD i;

	if (!aw->aw_HavePens) return;
	for (i = 0; i < ABOUT_PIC_PAIRS; i++, r += 2) {
		if (r[1] != ABOUT_PIC_NONE) {
			SetAPen(rp, aw->aw_Pens[r[1]]);
			RectFill(rp, (LONG) (g->LeftEdge + x * aw->aw_SX), (LONG) (g->TopEdge + y),
				 (LONG) (g->LeftEdge + (x + r[0]) * aw->aw_SX - 1), (LONG) (g->TopEdge + y));
		}
		if ((x = (WORD) (x + r[0])) >= ABOUT_PIC_W) {
			x = 0;
			y++;
		}
	}
}

/* a line: a read-only button with no frame, which centres (as ask.c's) */
static Object *line_of(CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ReadOnly, TRUE, GA_Text, (ULONG) text, BUTTON_BevelStyle, BVS_NONE,
			 BUTTON_Justification, BCJ_CENTER, TAG_DONE);
}

/* the version, what it does, its home and the artist, a line each; the
   copy cut into lines in place (n of them) */
static UWORD text_lines(struct AboutWin *aw, CONST_STRPTR *line)
{
	struct TextArgs ta;
	char ver[16];
	STRPTR p;
	ULONG n;
	UWORD k = 0;

	p = Text_PutNumber((STRPTR) ver, VERSION);
	p = Text_Put(p, (CONST_STRPTR) ".");
	Text_PutNumber(p, REVISION);
	ta.ta_Fmt = (CONST_STRPTR) TXT_ABOUT;
	ta.ta_Str[0] = (CONST_STRPTR) ver;
	ta.ta_Str[1] = ta.ta_Str[2] = NULL;
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	n = Text_Fmt((STRPTR) aw->aw_Text, ABOUT_TEXT, &ta);
	n += Text_Copy((STRPTR) aw->aw_Text + n, (CONST_STRPTR) "\n\n", ABOUT_TEXT - n);
	Text_Copy((STRPTR) aw->aw_Text + n, (CONST_STRPTR) TXT_ABOUT_ART, ABOUT_TEXT - n);
	line[k++] = (CONST_STRPTR) aw->aw_Text;
	for (p = (STRPTR) aw->aw_Text; *p; p++) {
		if (*p != '\n') continue;
		*p = '\0';
		if (k < ABOUT_LINES) line[k++] = (CONST_STRPTR) (p + 1);
	}
	return k;
}

/* the lines one under the other. WARNING: the layout owns its children
   from here, and disposes them all when it cannot be made */
static Object *text_column(struct AboutWin *aw)
{
	CONST_STRPTR line[ABOUT_LINES];
	Object *lab[ABOUT_LINES];
	UWORD n = text_lines(aw, line), i;

	clear_bytes((UBYTE *) lab, sizeof(lab));
	for (i = 0; i < n; i++) lab[i] = line_of(line[i]);
	return NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_VertAlignment, LALIGN_CENTER,
			 lab[0] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[0], lab[0] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[1] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[1], lab[1] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[2] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[2], lab[2] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[3] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[3], lab[3] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[4] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[4], lab[4] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[5] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[5], lab[5] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[6] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[6], lab[6] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[7] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[7], lab[7] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 TAG_DONE);
}

/* the picture beside the text, OK under them: the root layout (it owns
   the picture's place and the text from here, made or not) */
static Object *root_of(struct AboutWin *aw, Object *text)
{
	Object *row, *ok;

	row = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			LAYOUT_VertAlignment, LALIGN_CENTER,
			LAYOUT_AddChild, (ULONG) aw->aw_Pic, CHILD_MinWidth, ABOUT_PIC_W * aw->aw_SX,
			CHILD_MaxWidth, ABOUT_PIC_W * aw->aw_SX,
			CHILD_MinHeight, ABOUT_PIC_H, CHILD_MaxHeight, ABOUT_PIC_H, CHILD_WeightedWidth, 0, CHILD_WeightedHeight, 0,
			text ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) text, CHILD_WeightedWidth, 100, TAG_DONE);
	if (!row) return NULL;
	if (!(ok = NewObject(BUTTON_GetClass(), NULL, GA_ID, GID_OK, GA_RelVerify, TRUE, GA_Text, (ULONG) TXT_OK, TAG_DONE))) {
		DisposeObject(row);
		return NULL;
	}
	return NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE,
			 LAYOUT_SpaceInner, TRUE, LAYOUT_HorizAlignment, LALIGN_CENTER,
			 LAYOUT_AddChild, (ULONG) row, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) ok, CHILD_WeightedHeight, 0, CHILD_WeightedWidth, 0, CHILD_MinWidth, 80, TAG_DONE);
}

void About_Open(struct Window *over)
{
	struct AboutWin *aw;
	Object *root;

	if (g_About) {
		WindowToFront(g_About->aw_Window);
		ActivateWindow(g_About->aw_Window);
		return;
	}
	if (!over || !(aw = (struct AboutWin *) AllocVec(sizeof(*aw), MEMF_ANY | MEMF_CLEAR))) return;
	/* square on the screen: twice as wide where a pixel is twice as tall */
	aw->aw_SX = (UWORD) ((KeyGlyph_SquareWidth(over->WScreen, ABOUT_PIC_H) + ABOUT_PIC_W / 2) / ABOUT_PIC_W);
	if (aw->aw_SX < 1) aw->aw_SX = 1;
	if (aw->aw_SX > 4) aw->aw_SX = 4;
	/* a layout not made disposed what it was given: nothing is left over */
	if (!(aw->aw_Pic = NewObject(LAYOUT_GetClass(), NULL, TAG_DONE)) || !(root = root_of(aw, text_column(aw)))) {
		FreeVec(aw);
		return;
	}
	aw->aw_WinObj = NewObject(WINDOW_GetClass(), NULL, WA_Title, (ULONG) TXT_TITLE, WA_Activate, TRUE, WA_DragBar, TRUE,
				  WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, FALSE,
				  /* Intuition keeps the picture drawn by hand */
				  WA_SmartRefresh, TRUE, WA_AutoAdjust, TRUE,
				  /* Esc and Return close it */
				  WA_IDCMP, IDCMP_RAWKEY, WA_PubScreen, (ULONG) over->WScreen, WINDOW_RefWindow, (ULONG) over,
				  WINDOW_Position, WPOS_CENTERWINDOW, WINDOW_Layout, (ULONG) root, TAG_DONE);
	if (!aw->aw_WinObj) {
		DisposeObject(root);
		FreeVec(aw);
		return;
	}
	if (!(aw->aw_Window = (struct Window *) DoMethod(aw->aw_WinObj, WM_OPEN, NULL))) {
		DisposeObject(aw->aw_WinObj);
		FreeVec(aw);
		return;
	}
	g_About = aw;
	pens_get(aw);
	pic_draw(aw);
}

ULONG About_Signal(void)
{
	ULONG sigs = 0;

	if (g_About) GetAttr(WINDOW_SigMask, g_About->aw_WinObj, &sigs);
	return sigs;
}

void About_HandleInput(void)
{
	ULONG r;
	UWORD code;

	if (!g_About) return;
	while ((r = DoMethod(g_About->aw_WinObj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
		switch (r & WMHI_CLASSMASK) {
		case WMHI_CLOSEWINDOW:
			About_Close();
			return;		/* the object is gone: read no more of it */
		case WMHI_GADGETUP:
			if ((r & WMHI_GADGETMASK) == GID_OK) {
				About_Close();
				return;
			}
			break;
		case WMHI_RAWKEY:
			if ((r & WMHI_KEYMASK) == RAW_ESC || (r & WMHI_KEYMASK) == RAW_RETURN || (r & WMHI_KEYMASK) == RAW_ENTER) {
				About_Close();
				return;
			}
			break;
		}
	}
}

void About_Close(void)
{
	struct AboutWin *aw = g_About;

	if (!aw) return;
	g_About = NULL;
	DisposeObject(aw->aw_WinObj);	/* the window, the layout, the gadgets */
	pens_free(aw);
	FreeVec(aw);
}
