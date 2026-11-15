/*
 * keyglyph.c -- the bar keys' drawn marks (keyglyph.h)
 *
 * Integer arithmetic against the box the image is drawn in, as
 * AmiMatters' glyph.c: shares of the box, never pixel art, so a mark is
 * right at Topaz 8 and at a large font alike. A pixel is not square on
 * every screen (a PAL hires one is twice as tall as wide): the image's
 * width is its height in the screen's pixel shape, asked of the mode's
 * bits at OM_NEW; RTG pixels are square.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/modeid.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>

#include "keyglyph.h"
#include "boopsi.h"
#include "textutil.h"

struct KeyGlyph {
	ULONG kg_Kind;
	CONST_STRPTR kg_Text;		/* KG_FLAG: its word */
	const LONG *kg_Pen;		/* and its pen, read at each draw */
	WORD kg_FlagW;			/* the pennant's width (the word after it) */
	WORD kg_SX;			/* a pixel's height in columns: Loop's sides */
};

/* the text pen: a drawn mark looks like a written label */
static ULONG text_pen(const struct DrawInfo *dri)
{
	return (ULONG) (dri && dri->dri_NumPens > TEXTPEN ? dri->dri_Pens[TEXTPEN] : 1);
}

/* how many pixels wide a square of height h is on this screen: the
   mode's bits (a superhires pixel is a quarter, a hires one half as wide
   as a lores one; laced rows are half as tall); RTG and none: h */
static WORD square_w(struct Screen *scr, WORD h)
{
	UWORD m;
	LONG tx, ty;

	if (!scr || !(GetBitMapAttr(scr->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD)) return h;
	m = scr->ViewPort.Modes;
	tx = (m & SUPERHIRES) ? 11 : (m & HIRES) ? 22 : 44;
	ty = (m & LACE) ? 22 : 44;
	return (WORD) (((LONG) h * ty + tx / 2) / tx);
}

static void bar(struct RastPort *rp, const struct Rectangle *r)
{
	if (r->MaxX < r->MinX || r->MaxY < r->MinY) return;
	RectFill(rp, r->MinX, r->MinY, r->MaxX, r->MaxY);
}

/* a rectangle's corners (a macro: a function of four numbers and a
   pointer would pass them on the stack, which `gmake checks` forbids) */
#define RECT(r, x0, y0, x1, y1)	((r).MinX = (WORD) (x0), (r).MinY = (WORD) (y0), (r).MaxX = (WORD) (x1), (r).MaxY = (WORD) (y1))

/* a lens: a ring in the top left seven ninths of the box, a handle of
   two strokes down to the corner, and inside the ring a minus, a plus or
   a small frame */
static void lens(struct RastPort *rp, ULONG kind, struct IBox *b)
{
	WORD d = (WORD) ((b->Height * 7 + 4) / 9), dw = (WORD) ((b->Width * 7 + 4) / 9);
	WORD rx, ry, cx, cy, mx, my, vw, k, hx, hy;
	struct Rectangle r;

	if (d < 5) d = 5;
	if (dw < 5) dw = 5;
	rx = (WORD) ((dw - 1) / 2);
	ry = (WORD) ((d - 1) / 2);
	cx = (WORD) (b->Left + rx);
	cy = (WORD) (b->Top + ry);
	DrawEllipse(rp, cx, cy, rx, ry);
	/* the handle from the ring's lower right to the box's corner, as
	   wide as a pixel is tall (two strokes at least) */
	hx = (WORD) (cx + (rx * 7 + 5) / 10);
	hy = (WORD) (cy + (ry * 7 + 5) / 10);
	vw = (WORD) ((b->Width + b->Height - 1) / b->Height);
	if (vw < 1) vw = 1;
	for (k = 0; k <= vw; k++) {
		Move(rp, (WORD) (hx + k), hy);
		Draw(rp, (WORD) (b->Left + b->Width - 1 - vw + k), (WORD) (b->Top + b->Height - 1));
	}
	mx = (WORD) ((rx * 3) / 5);
	my = (WORD) ((ry * 3) / 5);
	if (mx < 1) mx = 1;
	if (my < 1) my = 1;
	if (kind == KG_ALL) {
		Move(rp, (WORD) (cx - mx), (WORD) (cy - my));
		Draw(rp, (WORD) (cx + mx), (WORD) (cy - my));
		Draw(rp, (WORD) (cx + mx), (WORD) (cy + my));
		Draw(rp, (WORD) (cx - mx), (WORD) (cy + my));
		Draw(rp, (WORD) (cx - mx), (WORD) (cy - my));
		return;
	}
	RECT(r, (WORD) (cx - mx), cy, (WORD) (cx + mx), cy);
	bar(rp, &r);
	if (kind == KG_ZOOMIN) {
		RECT(r, (WORD) (cx - (vw - 1) / 2), (WORD) (cy - my), (WORD) (cx + vw / 2), (WORD) (cy + my));
		bar(rp, &r);
	}
}

/* an arrowhead: its base the full height, the rows shorter by a pixel's
   height in columns towards the tip, the middle rows (as many as the bar
   it ends is thick) as long as the head; hd_Y the bar's top row (the
   rows the other way round drew it backwards) */
struct Head {
	WORD hd_X, hd_Y;		/* the tip's column, the bar's top row */
	WORD hd_N, hd_SX, hd_T;		/* rows to the base's ends, a row's width, the bar's rows */
	BOOL hd_Right;			/* pointing right, else left */
};

static void head(struct RastPort *rp, const struct Head *hd)
{
	struct Rectangle r;
	WORD k, y, len, hl = (WORD) (hd->hd_N * hd->hd_SX);

	for (k = (WORD) (1 - hd->hd_N); k < hd->hd_T + hd->hd_N - 1; k++) {
		y = (WORD) (hd->hd_Y + k);
		len = (WORD) (k < 0 ? (hd->hd_N + k) * hd->hd_SX : k >= hd->hd_T ? (hd->hd_N - (k - hd->hd_T + 1)) * hd->hd_SX : hl);
		if (hd->hd_Right) RECT(r, hd->hd_X - hl + 1, y, hd->hd_X - hl + len, y);
		else RECT(r, hd->hd_X + hl - len, y, hd->hd_X + hl - 1, y);
		bar(rp, &r);
	}
}

/* Loop, the repeat sign, pixel by pixel in the box, after the common
   repeat icon: two halves chasing each other -- up the left side, along
   the top into an arrowhead pointing right at the right edge; down the
   right side, along the foot into one pointing left at the left edge.
   A side stands on its head's base, a gap before the other half's head;
   each bar runs into its head, the sign edge to edge (a scaled grid left
   gaps, the heads backwards and the sign off centre: the user, A1200,
   2 Nov 2026). sx: a pixel's height in columns */
static void loop(struct RastPort *rp, const struct IBox *b, WORD sx)
{
	WORD h = b->Height, n, bt, gap, hl, sw, ty, fy, l = b->Left, r = (WORD) (b->Left + b->Width - 1);
	struct Rectangle rc;
	struct Head hd;

	n = (WORD) ((h + 3) / 5);			/* the heads' base two fifths of the height */
	if (n < 2) n = 2;
	bt = (WORD) ((h + 1) / 10);			/* the bars' rows, a tenth */
	if (bt < 1) bt = 1;
	gap = (WORD) ((h * 3 + 10) / 20);		/* where a side ends short of a head */
	if (gap < 1) gap = 1;
	hl = (WORD) (n * sx);
	sw = (WORD) (bt * sx);				/* the sides as wide as the bars are tall */
	ty = (WORD) (b->Top + n - 1);
	fy = (WORD) (b->Top + h - n - bt + 1);
	/* up the left side, along the top into the head */
	RECT(rc, l + hl - sw, ty, l + hl - 1, fy - n - gap);
	bar(rp, &rc);
	RECT(rc, l + hl - sw, ty, r - hl, ty + bt - 1);
	bar(rp, &rc);
	hd.hd_X = r;
	hd.hd_Y = ty;
	hd.hd_N = n;
	hd.hd_SX = sx;
	hd.hd_T = bt;
	hd.hd_Right = TRUE;
	head(rp, &hd);
	/* down the right side, along the foot into the head */
	RECT(rc, r - hl + 1, ty + bt + n + gap - 1, r - hl + sw, fy + bt - 1);
	bar(rp, &rc);
	RECT(rc, l + hl, fy, r - hl + sw, fy + bt - 1);
	bar(rp, &rc);
	hd.hd_X = l;
	hd.hd_Y = fy;
	hd.hd_Right = FALSE;
	head(rp, &hd);
}

/* a handle's flag: a pole down the left, a pennant from its top in the
   handle's pen (outlined in the text pen: a light handle colour stays
   seen), the word after it -- the waveform's flags in small */
struct Mark {
	struct RastPort *mk_RP;
	struct IBox mk_Box;
};

static void flag(const struct KeyGlyph *kg, const struct Mark *m)
{
	struct RastPort *rp = m->mk_RP;
	const struct IBox *b = &m->mk_Box;
	ULONG text = (ULONG) rp->FgPen;
	LONG pen = kg->kg_Pen ? *kg->kg_Pen : -1;
	WORD fw = kg->kg_FlagW, sp = (WORD) ((kg->kg_FlagW + 2) / 3), fh = (WORD) ((b->Height * 5 + 4) / 9), x, l;
	struct Rectangle r;

	/* room each side as wide as the pennant's gap to its word (the
	   key's edge too close, the user, 2 Nov 2026) -- none in a frame
	   too small for it */
	l = (WORD) (b->Width >= fw + 2 * sp ? b->Left + sp : b->Left);
	if (fw > b->Width) fw = b->Width;
	if (fh < 3) fh = 3;
	/* the pennant: filled, then its edge and the pole */
	if (pen >= 0) {
		SetAPen(rp, (ULONG) pen);
		RECT(r, l + 1, b->Top + 1, l + fw - 2, b->Top + fh - 2);
		bar(rp, &r);
		SetAPen(rp, text);
	}
	Move(rp, l, (WORD) (b->Top + b->Height - 1));
	Draw(rp, l, b->Top);
	Draw(rp, (WORD) (l + fw - 1), b->Top);
	Draw(rp, (WORD) (l + fw - 1), (WORD) (b->Top + fh - 1));
	Draw(rp, l, (WORD) (b->Top + fh - 1));
	/* the word, upright in the box: the font the button draws in */
	if (kg->kg_Text) {
		x = (WORD) (l + fw + sp);
		Move(rp, x, (WORD) (b->Top + (b->Height - rp->TxHeight) / 2 + rp->TxBaseline));
		Text(rp, (STRPTR) kg->kg_Text, Text_Length(kg->kg_Text));
	}
}

static DISP_QUAL ULONG dispatch(REG(a0, Class *cl), REG(a2, Object *o), REG(a1, Msg msg))
{
	struct KeyGlyph *kg;
	struct Image *im;
	struct impDraw *imp;
	struct TagItem *tags;
	struct IBox b;
	struct Screen *scr;
	ULONG result;
	WORD size, w, tw, kw, sp;

	switch (msg->MethodID) {
	case OM_NEW:
		if (!(result = DoSuperMethodA(cl, o, msg))) return 0;
		tags = ((struct opSet *) msg)->ops_AttrList;
		kg = (struct KeyGlyph *) INST_DATA(cl, (Object *) result);
		kg->kg_Kind = GetTagData(KEYGLYPH_Kind, KG_LOOP, tags);
		kg->kg_Text = (CONST_STRPTR) GetTagData(KEYGLYPH_Text, 0, tags);
		kg->kg_Pen = (const LONG *) GetTagData(KEYGLYPH_PenPtr, 0, tags);
		scr = (struct Screen *) GetTagData(KEYGLYPH_Screen, 0, tags);
		size = (WORD) GetTagData(KEYGLYPH_Size, 9, tags);
		if (size < 7) size = 7;
		w = square_w(scr, size);
		im = (struct Image *) result;
		im->Height = size;
		/* a lens square on the screen; the repeat sign across its key but
		   for a flag's margin each side, whatever a pixel's shape (11 to 9
		   of a square left a small sign in a wide key on square pixels:
		   the user, A1200, 2 Nov 2026); a flag two thirds of a square, a
		   third of it apart from its word and from either edge, then its
		   word in the screen's font */
		kg->kg_FlagW = (WORD) ((w * 2 + 2) / 3);
		kg->kg_SX = (WORD) ((square_w(scr, 8) + 4) / 8);
		if (kg->kg_SX < 1) kg->kg_SX = 1;
		sp = (WORD) ((kg->kg_FlagW + 2) / 3);
		kw = (WORD) GetTagData(KEYGLYPH_Width, 0, tags);
		if (kg->kg_Kind == KG_LOOP) im->Width = kw > 2 * sp + size ? (WORD) (kw - 2 * sp) : (WORD) ((w * 11 + 4) / 9);
		else if (kg->kg_Kind == KG_FLAG) {
			tw = 0;
			if (kg->kg_Text)
				tw = scr ? (WORD) TextLength(&scr->RastPort, (STRPTR) kg->kg_Text, Text_Length(kg->kg_Text)) :
					   (WORD) (8 * Text_Length(kg->kg_Text));
			im->Width = (WORD) (kg->kg_FlagW + 3 * ((kg->kg_FlagW + 2) / 3) + tw);
		}
		else im->Width = w;
		/* a BOOPSI image, IM_DRAW asked for -- not a bitmap DrawImage blits */
		im->Depth = CUSTOMIMAGEDEPTH;
		return result;
	case IM_DRAW:
	case IM_DRAWFRAME:
		imp = (struct impDraw *) msg;
		im = (struct Image *) o;
		kg = (struct KeyGlyph *) INST_DATA(cl, o);
		b.Width = im->Width;
		b.Height = im->Height;
		b.Left = (WORD) (imp->imp_Offset.X + im->LeftEdge);
		b.Top = (WORD) (imp->imp_Offset.Y + im->TopEdge);
		/* a frame (button.gadget's inside): the mark at its own size,
		   centred; a smaller frame takes a smaller mark */
		if (msg->MethodID == IM_DRAWFRAME) {
			if (imp->imp_Dimensions.Width < b.Width) b.Width = imp->imp_Dimensions.Width;
			if (imp->imp_Dimensions.Height < b.Height) b.Height = imp->imp_Dimensions.Height;
			b.Left = (WORD) (b.Left + (imp->imp_Dimensions.Width - b.Width) / 2);
			b.Top = (WORD) (b.Top + (imp->imp_Dimensions.Height - b.Height) / 2);
		}
		if (b.Width < 5 || b.Height < 5) return 1;
		SetAPen(imp->imp_RPort, text_pen(imp->imp_DrInfo));
		SetDrMd(imp->imp_RPort, JAM1);
		if (kg->kg_Kind == KG_LOOP) loop(imp->imp_RPort, &b, kg->kg_SX);
		else if (kg->kg_Kind == KG_FLAG) {
			struct Mark mk;

			mk.mk_RP = imp->imp_RPort;
			mk.mk_Box = b;
			flag(kg, &mk);
		}
		else lens(imp->imp_RPort, kg->kg_Kind, &b);
		return 1;
	}
	return DoSuperMethodA(cl, o, msg);
}

Class *KeyGlyph_MakeClass(void)
{
	Class *cl;

	if (!(cl = MakeClass(NULL, (STRPTR) "imageclass", NULL, sizeof(struct KeyGlyph), 0))) return NULL;
	cl->cl_Dispatcher.h_Entry = (ULONG (*)()) dispatch;
	cl->cl_Dispatcher.h_SubEntry = NULL;
	cl->cl_Dispatcher.h_Data = NULL;
	return cl;
}

void KeyGlyph_FreeClass(Class *cl)
{
	if (cl) FreeClass(cl);
}

BOOL KeyGlyph_SquarePixels(struct Screen *scr)
{
	return (BOOL) (square_w(scr, 8) < 12);
}

WORD KeyGlyph_SquareWidth(struct Screen *scr, WORD h)
{
	return square_w(scr, h);
}
