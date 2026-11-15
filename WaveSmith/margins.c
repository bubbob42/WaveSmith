/*
 * margins.c -- the lead and the tail in a window (margins.h)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/slider.h>
#include <gadgets/integer.h>
#include <images/bevel.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/slider.h>
#include <proto/integer.h>
#include <clib/alib_protos.h>

#include "margins.h"
#include "wavesmith_msg.h"
#include "textutil.h"

struct Library *SliderBase = NULL;
struct Library *IntegerBase = NULL;

#define CLASS_VERSION	44
#define SECS_LEN	MARGINS_SECSLEN	/* "30.000 s" */
#define RAW_RETURN	0x44
#define RAW_ENTER	0x43
#define RAW_ESC		0x45

enum {
	GID_MG_LEADSLIDE = 200, GID_MG_LEAD, GID_MG_TAILSLIDE, GID_MG_TAIL, GID_MG_USE, GID_MG_CANCEL
};

/* one margin's row: the slider, its milliseconds (the slider's level, the
   same unit: the number follows the slider as it moves, ICA_MAP), the
   seconds read-only */
struct Row {
	Object *rw_Slider, *rw_Number, *rw_Secs;
	ULONG rw_Ms, rw_Max;
	UWORD rw_Id;			/* the slider's; the number's the next */
	char rw_Text[SECS_LEN];
};

static struct TagItem g_LevelToNumber[] = {
	{ SLIDER_Level, INTEGER_Number },
	{ GA_ID, TAG_IGNORE },		/* the slider's ID kept off the field */
	{ TAG_END, 0 }
};

void Margins_Secs(ULONG ms, char *buf)
{
	STRPTR p = Text_PutNumber((STRPTR) buf, ms / 1000);
	ULONG f = ms % 1000;
	UWORD digits = 3;

	*p++ = '.';
	if (!f) {
		*p++ = '0';
	} else {
		while (f % 10 == 0) {
			f /= 10;
			digits--;
		}
		/* the leading zeros a fraction of 5 ms has: "005" */
		if (digits >= 2 && f < 10) *p++ = '0';
		if (digits == 3 && f < 100) *p++ = '0';
		p = Text_PutNumber(p, f);
	}
	Text_Put(p, (CONST_STRPTR) " s");
}

static WORD text_w(struct Screen *scr, CONST_STRPTR s)
{
	UWORD n = (UWORD) Text_Length(s);

	return (WORD) (scr ? TextLength(&scr->RastPort, (STRPTR) s, n) : n * 8);
}

/* text that does nothing: no frame, or a field's (read-only, the seconds) */
static Object *shown(CONST_STRPTR text, BOOL field)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ReadOnly, TRUE, GA_Text, (ULONG) text,
			 BUTTON_BevelStyle, field ? BVS_FIELD : BVS_NONE, BUTTON_Justification, field ? BCJ_RIGHT : BCJ_LEFT,
			 TAG_DONE);
}

static void drop_row(struct Row *rw);

/* the row's gadgets (all or none: a NULL left nothing behind) */
static BOOL make_row(struct Row *rw)
{
	Margins_Secs(rw->rw_Ms, rw->rw_Text);
	rw->rw_Slider = NewObject(SLIDER_GetClass(), NULL, GA_ID, rw->rw_Id, GA_RelVerify, TRUE,
				  SLIDER_Min, 0, SLIDER_Max, rw->rw_Max, SLIDER_Level, rw->rw_Ms,
				  SLIDER_Orientation, SLIDER_HORIZONTAL, TAG_DONE);
	rw->rw_Number = NewObject(INTEGER_GetClass(), NULL, GA_ID, rw->rw_Id + 1, GA_RelVerify, TRUE, GA_TabCycle, TRUE,
				  INTEGER_Minimum, 0, INTEGER_Maximum, rw->rw_Max, INTEGER_Number, rw->rw_Ms,
				  INTEGER_MaxChars, 5, INTEGER_MinVisible, 6, TAG_DONE);
	rw->rw_Secs = shown((CONST_STRPTR) rw->rw_Text, TRUE);
	if (rw->rw_Slider && rw->rw_Number && rw->rw_Secs) {
		SetAttrs(rw->rw_Slider, ICA_TARGET, (ULONG) rw->rw_Number, ICA_MAP, (ULONG) g_LevelToNumber, TAG_DONE);
		return TRUE;
	}
	drop_row(rw);
	return FALSE;
}

static void drop_row(struct Row *rw)
{
	if (rw->rw_Slider) DisposeObject(rw->rw_Slider);
	if (rw->rw_Number) DisposeObject(rw->rw_Number);
	if (rw->rw_Secs) DisposeObject(rw->rw_Secs);
	rw->rw_Slider = rw->rw_Number = rw->rw_Secs = NULL;
}

/* the row in its titled group: slider, milliseconds, "ms", seconds --
   NULL with the row's gadgets gone either way (a layout that cannot be
   made disposes what it was given) */
struct RowLayout {
	struct Row *rl_Row;
	CONST_STRPTR rl_Title;
	struct Screen *rl_Screen;
};

static Object *row_layout(const struct RowLayout *rl)
{
	struct Row *rw = rl->rl_Row;
	Object *ms = shown((CONST_STRPTR) TXT_MG_MS, FALSE);

	if (!ms) {
		drop_row(rw);
		return NULL;
	}
	return NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
			 LAYOUT_BevelStyle, BVS_GROUP, LAYOUT_Label, (ULONG) rl->rl_Title,
			 LAYOUT_SpaceOuter, TRUE, LAYOUT_SpaceInner, TRUE, LAYOUT_VertAlignment, LALIGN_CENTER,
			 LAYOUT_AddChild, (ULONG) rw->rw_Slider, CHILD_WeightedWidth, 100,
			 CHILD_MinWidth, text_w(rl->rl_Screen, (CONST_STRPTR) "0000000000000000"),
			 LAYOUT_AddChild, (ULONG) rw->rw_Number, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) ms, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) rw->rw_Secs, CHILD_WeightedWidth, 0,
			 CHILD_MinWidth, text_w(rl->rl_Screen, (CONST_STRPTR) "30.000 s") + 12,
			 TAG_DONE);
}

/* a gadget told something; a class that leaves the repaint to the caller
   repainted */
static void set_on(Object *o, struct Window *w, ULONG tag, ULONG value)
{
	if (SetGadgetAttrs((struct Gadget *) o, w, NULL, tag, value, TAG_DONE)) RefreshGList((struct Gadget *) o, w, NULL, 1);
}

/* the row at ms: the seconds, and the slider when the number moved */
struct RowSet {
	struct Row *rs_Row;
	struct Window *rs_Window;
	ULONG rs_Ms;
	BOOL rs_Slider;
};

static void row_at(const struct RowSet *rs)
{
	struct Row *rw = rs->rs_Row;
	ULONG ms = rs->rs_Ms > rw->rw_Max ? rw->rw_Max : rs->rs_Ms;

	if (ms == rw->rw_Ms) return;
	rw->rw_Ms = ms;
	Margins_Secs(ms, rw->rw_Text);
	set_on(rw->rw_Secs, rs->rs_Window, GA_Text, (ULONG) rw->rw_Text);
	if (rs->rs_Slider) set_on(rw->rw_Slider, rs->rs_Window, SLIDER_Level, ms);
}

static ULONG level_of(Object *o, ULONG tag)
{
	ULONG v = 0;

	GetAttr(tag, o, &v);
	return v;
}

/* the box of a gadget in its window */
static void box_of(Object *o, struct IBox *b)
{
	struct Gadget *g = (struct Gadget *) o;

	b->Left = g->LeftEdge;
	b->Top = g->TopEdge;
	b->Width = g->Width;
	b->Height = g->Height;
}

/* the window's own loop until Use or Cancel; the rows kept as the user
   moves them (the slider's level looked at every tick: ICA_MAP moves the
   number with it, nothing tells the seconds) */
struct Loop {
	Object *lp_Win;
	struct Window *lp_Window;
	struct Row *lp_Rows;		/* two */
};

static BOOL used(const struct Loop *lp)
{
	struct RowSet rs;
	ULONG sigs = 0, r;
	UWORD code, i;
	LONG id;

	rs.rs_Window = lp->lp_Window;
	for (;;) {
		GetAttr(WINDOW_SigMask, lp->lp_Win, &sigs);
		if (Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) return FALSE;
		while ((r = DoMethod(lp->lp_Win, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (r & WMHI_CLASSMASK) {
			case WMHI_CLOSEWINDOW:
				return FALSE;
			case WMHI_GADGETUP:
				id = (LONG) (r & WMHI_GADGETMASK);
				if (id == GID_MG_USE) return TRUE;
				if (id == GID_MG_CANCEL) return FALSE;
				for (i = 0; i < 2; i++) {
					rs.rs_Row = &lp->lp_Rows[i];
					if (id == (LONG) lp->lp_Rows[i].rw_Id) {
						rs.rs_Ms = level_of(lp->lp_Rows[i].rw_Slider, SLIDER_Level);
						rs.rs_Slider = FALSE;
						row_at(&rs);
					} else if (id == (LONG) lp->lp_Rows[i].rw_Id + 1) {
						rs.rs_Ms = level_of(lp->lp_Rows[i].rw_Number, INTEGER_Number);
						rs.rs_Slider = TRUE;
						row_at(&rs);
					}
				}
				break;
			case WMHI_INTUITICK:
				for (i = 0; i < 2; i++) {
					rs.rs_Row = &lp->lp_Rows[i];
					rs.rs_Ms = level_of(lp->lp_Rows[i].rw_Slider, SLIDER_Level);
					rs.rs_Slider = FALSE;
					row_at(&rs);
				}
				break;
			case WMHI_RAWKEY:
				if ((r & WMHI_KEYMASK) == RAW_ESC) return FALSE;
				if ((r & WMHI_KEYMASK) == RAW_RETURN || (r & WMHI_KEYMASK) == RAW_ENTER) return TRUE;
				break;
			}
		}
	}
}

/* the window made, opened, answered and gone: TRUE when used */
static BOOL ask_margins(struct MarginArgs *ma, struct Row *rows)
{
	struct Screen *scr = ma->ma_Over ? ma->ma_Over->WScreen : NULL;
	struct RowLayout rl;
	struct Loop lp;
	Object *lead, *tail, *use, *cancel, *gap, *buttons, *root, *win;
	BOOL ok;

	if (!make_row(&rows[0])) return FALSE;
	if (!make_row(&rows[1])) {
		drop_row(&rows[0]);
		return FALSE;
	}
	/* WARNING: from here every layout made owns its children and disposes
	   them when it cannot be made; the rows' rest is ours until then */
	rl.rl_Screen = scr;
	rl.rl_Row = &rows[0];
	rl.rl_Title = (CONST_STRPTR) TXT_MG_LEAD;
	lead = row_layout(&rl);
	rl.rl_Row = &rows[1];
	rl.rl_Title = (CONST_STRPTR) TXT_MG_TAIL;
	tail = row_layout(&rl);
	use = NewObject(BUTTON_GetClass(), NULL, GA_ID, GID_MG_USE, GA_RelVerify, TRUE, GA_Text, (ULONG) TXT_MG_USE, TAG_DONE);
	cancel = NewObject(BUTTON_GetClass(), NULL, GA_ID, GID_MG_CANCEL, GA_RelVerify, TRUE, GA_Text, (ULONG) TXT_MG_CANCEL,
			   TAG_DONE);
	gap = NewObject(LAYOUT_GetClass(), NULL, TAG_DONE);
	/* Use at the left, Cancel at the right (the Style Guide's), the room
	   between them empty */
	buttons = (use && cancel && gap) ? NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
						     LAYOUT_SpaceInner, TRUE,
						     LAYOUT_AddChild, (ULONG) use, CHILD_WeightedWidth, 0,
						     LAYOUT_AddChild, (ULONG) gap, CHILD_WeightedWidth, 100,
						     LAYOUT_AddChild, (ULONG) cancel, CHILD_WeightedWidth, 0, TAG_DONE) : NULL;
	if (!buttons && !(use && cancel && gap)) {
		/* never given to a layout: still ours */
		if (use) DisposeObject(use);
		if (cancel) DisposeObject(cancel);
		if (gap) DisposeObject(gap);
	}
	if (!lead || !tail || !buttons) {
		/* a row without its layout is gone already (row_layout) */
		if (lead) DisposeObject(lead);
		if (tail) DisposeObject(tail);
		if (buttons) DisposeObject(buttons);
		return FALSE;
	}
	root = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE,
			 LAYOUT_SpaceInner, TRUE,
			 LAYOUT_AddChild, (ULONG) lead, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) tail, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) buttons, CHILD_WeightedHeight, 0, TAG_DONE);
	if (!root) return FALSE;
	win = NewObject(WINDOW_GetClass(), NULL, WA_Title, (ULONG) TXT_MG_TITLE, WA_CloseGadget, TRUE, WA_DragBar, TRUE,
			WA_DepthGadget, TRUE, WA_Activate, TRUE, WA_AutoAdjust, TRUE,
			/* the keys (Return, Esc) and the ticks the seconds follow by */
			WA_IDCMP, IDCMP_RAWKEY | IDCMP_INTUITICKS,
			WA_PubScreen, (ULONG) scr,
			ma->ma_Over ? WINDOW_RefWindow : TAG_IGNORE, (ULONG) ma->ma_Over,
			WINDOW_Position, ma->ma_Over ? WPOS_CENTERWINDOW : WPOS_CENTERSCREEN,
			WINDOW_Layout, (ULONG) root, TAG_DONE);
	if (!win) {
		DisposeObject(root);
		return FALSE;
	}
	if (!(lp.lp_Window = (struct Window *) DoMethod(win, WM_OPEN, NULL))) {
		DisposeObject(win);
		return FALSE;
	}
	ma->ma_NoWindow = FALSE;
	if (ma->ma_Shown) {
		struct IBox boxes[MARGINS_BOXES];

		boxes[MARGINS_BOX_WINDOW].Left = lp.lp_Window->LeftEdge;
		boxes[MARGINS_BOX_WINDOW].Top = lp.lp_Window->TopEdge;
		boxes[MARGINS_BOX_WINDOW].Width = lp.lp_Window->Width;
		boxes[MARGINS_BOX_WINDOW].Height = lp.lp_Window->Height;
		box_of(rows[0].rw_Number, &boxes[MARGINS_BOX_LEAD]);
		box_of(rows[1].rw_Number, &boxes[MARGINS_BOX_TAIL]);
		box_of(use, &boxes[MARGINS_BOX_USE]);
		ma->ma_Shown(boxes);
	}
	lp.lp_Win = win;
	lp.lp_Rows = rows;
	ok = used(&lp);
	if (ok) {
		/* a number typed and not yet entered counts: what it says now */
		ma->ma_Lead = level_of(rows[0].rw_Number, INTEGER_Number);
		ma->ma_Tail = level_of(rows[1].rw_Number, INTEGER_Number);
		if (ma->ma_Lead > ma->ma_LeadMax) ma->ma_Lead = ma->ma_LeadMax;
		if (ma->ma_Tail > ma->ma_TailMax) ma->ma_Tail = ma->ma_TailMax;
	}
	DisposeObject(win);		/* the window, the layouts, the gadgets */
	return ok;
}

BOOL Margins_Window(struct MarginArgs *ma)
{
	struct Row rows[2];
	BOOL ok = FALSE;
	UWORD i;

	ma->ma_NoWindow = TRUE;
	for (i = 0; i < sizeof(rows); i++) ((UBYTE *) rows)[i] = 0;
	rows[0].rw_Ms = ma->ma_Lead > ma->ma_LeadMax ? ma->ma_LeadMax : ma->ma_Lead;
	rows[0].rw_Max = ma->ma_LeadMax;
	rows[0].rw_Id = GID_MG_LEADSLIDE;
	rows[1].rw_Ms = ma->ma_Tail > ma->ma_TailMax ? ma->ma_TailMax : ma->ma_Tail;
	rows[1].rw_Max = ma->ma_TailMax;
	rows[1].rw_Id = GID_MG_TAILSLIDE;
	SliderBase = OpenLibrary((STRPTR) "gadgets/slider.gadget", CLASS_VERSION);
	IntegerBase = OpenLibrary((STRPTR) "gadgets/integer.gadget", CLASS_VERSION);
	if (SliderBase && IntegerBase) ok = ask_margins(ma, rows);
	if (IntegerBase) CloseLibrary(IntegerBase);
	if (SliderBase) CloseLibrary(SliderBase);
	IntegerBase = SliderBase = NULL;
	return ok;
}
