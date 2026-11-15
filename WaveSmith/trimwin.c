/*
 * trimwin.c -- the Trim window (trimwin.h)
 *
 * Built as PrismaRec's deck builds its sections (PrismaControl's
 * groupframe, the whole box: "the frame lines alone looked thin" -- the
 * user, 7 Oct 2026), opened and closed as Reflector's controls window is:
 * one window object kept, WM_OPEN and WM_CLOSE, the main window's menus
 * lent (LendMenus), its port in the caller's Wait().
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/string.h>
#include <images/label.h>
#include <images/bevel.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/label.h>
#include <clib/alib_protos.h>

#include "trimwin.h"
#include "groupframe.h"
#include "waveplan.h"
#include "wavesmith_msg.h"
#include "textutil.h"

#define TIME_LEN	WAVEPLAN_TIMELEN
#define FADE_LEN	16
#define SECTION_PAD	2		/* the deck's, round a section's body */

/* a side's gadgets: TW_GID + side * TW_SIDE + part (TRIMBOX_#?) */
#define TW_GID		400
#define TW_SIDE		10

static struct {
	Class *tw_GF;			/* the section frames' class */
	Object *tw_Win;			/* the window object, NULL before Make */
	Object *tw_Root;		/* its layout: measured for the first place */
	struct Window *tw_Window;	/* open: the window */
	Object *tw_Part[2][TRIMBOX_PARTS];
	char tw_Time[2][TIME_LEN];
	char tw_Fade[2][FADE_LEN];
	struct HintInfo tw_Hints[2 * TRIMBOX_PARTS + 1];
	WORD tw_Left, tw_Top;		/* where it was, for the next opening */
	BOOL tw_Placed;
} tw;

static WORD text_w(struct Screen *scr, CONST_STRPTR s)
{
	UWORD n = (UWORD) Text_Length(s);

	return (WORD) (scr ? TextLength(&scr->RastPort, (STRPTR) s, n) : n * 8);
}

static Object *button(UWORD id, CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_Text, (ULONG) text, TAG_END);
}

static Object *arrow(UWORD id, ULONG glyph)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, BUTTON_AutoButton, glyph, TAG_END);
}

/* a side's section: the time, the nudge arrows, Listen; the fade's length
   (a sunk read-out) and Show -- the box titled In or Out */
static Object *side(UWORD s, struct Screen *scr)
{
	Object **p = tw.tw_Part[s], *top, *bottom, *body, *pad;
	UWORD id = (UWORD) (TW_GID + s * TW_SIDE);
	ULONG fw = (ULONG) (text_w(scr, (CONST_STRPTR) "0:00:00.000") + 20);	/* a time field */

	p[TRIMBOX_TIME] = NewObject(STRING_GetClass(), NULL, GA_ID, id + TRIMBOX_TIME, GA_RelVerify, TRUE, GA_TabCycle, TRUE,
				    STRINGA_TextVal, (ULONG) tw.tw_Time[s], STRINGA_MaxChars, TIME_LEN - 1, STRINGA_MinVisible, 10,
				    STRINGA_Justification, GACT_STRINGLEFT, TAG_END);
	p[TRIMBOX_BACK] = arrow((UWORD) (id + TRIMBOX_BACK), BAG_LFARROW);
	p[TRIMBOX_ON] = arrow((UWORD) (id + TRIMBOX_ON), BAG_RTARROW);
	p[TRIMBOX_LISTEN] = button((UWORD) (id + TRIMBOX_LISTEN), (CONST_STRPTR) TXT_LISTEN);
	p[TRIMBOX_FADE] = NewObject(BUTTON_GetClass(), NULL, GA_ID, id + TRIMBOX_FADE, GA_ReadOnly, TRUE, GA_Text, (ULONG) tw.tw_Fade[s],
				    BUTTON_BevelStyle, BVS_FIELD, BUTTON_Justification, BCJ_LEFT, TAG_END);
	p[TRIMBOX_SHOW] = button((UWORD) (id + TRIMBOX_SHOW), (CONST_STRPTR) TXT_SHOW);
	top = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			LAYOUT_AddChild, (ULONG) p[TRIMBOX_TIME], CHILD_WeightedWidth, 0, CHILD_MinWidth, fw, CHILD_MaxWidth, fw,
			LAYOUT_AddChild, (ULONG) p[TRIMBOX_BACK], CHILD_WeightedWidth, 0,
			LAYOUT_AddChild, (ULONG) p[TRIMBOX_ON], CHILD_WeightedWidth, 0,
			LAYOUT_AddChild, (ULONG) p[TRIMBOX_LISTEN], CHILD_WeightedWidth, 0,
			LAYOUT_AddChild, (ULONG) NewObject(LAYOUT_GetClass(), NULL, TAG_END), CHILD_WeightedWidth, 100,
			TAG_END);
	/* the fade's length as wide as "10.0 s" and a little: its own width
	   would be the label's text */
	bottom = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			   LAYOUT_AddChild, (ULONG) p[TRIMBOX_FADE], CHILD_WeightedWidth, 0,
			   CHILD_Label, (ULONG) NewObject(LABEL_GetClass(), NULL, LABEL_Text,
							  (ULONG) (s ? TXT_M_FADEOUT : TXT_M_FADEIN), TAG_END),
			   CHILD_MinWidth, (ULONG) (text_w(scr, (CONST_STRPTR) "10.0 s") + 16),
			   LAYOUT_AddChild, (ULONG) NewObject(LAYOUT_GetClass(), NULL, TAG_END), CHILD_WeightedWidth, 100,
			   LAYOUT_AddChild, (ULONG) p[TRIMBOX_SHOW], CHILD_WeightedWidth, 0,
			   TAG_END);
	body = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceInner, TRUE,
			 LAYOUT_AddChild, (ULONG) top, LAYOUT_AddChild, (ULONG) bottom, TAG_END);
	if (!body) return NULL;		/* a failed layout disposes what it was given */
	pad = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
			LAYOUT_LeftSpacing, SECTION_PAD, LAYOUT_RightSpacing, SECTION_PAD,
			LAYOUT_TopSpacing, SECTION_PAD, LAYOUT_BottomSpacing, SECTION_PAD,
			LAYOUT_AddChild, (ULONG) body, TAG_END);
	if (!pad) return NULL;
	return NewObject(tw.tw_GF, NULL, GROUPFRAME_Label, (ULONG) (s ? TXT_OUT : TXT_IN), GROUPFRAME_Style, GROUPFRAME_STYLE_BOX,
			 GROUPFRAME_PrefsLook, TRUE, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE,
			 LAYOUT_AddChild, (ULONG) pad, TAG_END);
}

static void hint(UWORD n, UWORD id, CONST_STRPTR text)
{
	tw.tw_Hints[n].hi_GadgetID = (WORD) id;
	tw.tw_Hints[n].hi_Code = -1;
	tw.tw_Hints[n].hi_Text = (STRPTR) text;
	tw.tw_Hints[n].hi_Flags = 0;
}

static void hints_fill(void)
{
	UWORD s, n = 0, id;

	for (s = 0; s < 2; s++) {
		id = (UWORD) (TW_GID + s * TW_SIDE);
		hint(n++, (UWORD) (id + TRIMBOX_TIME), (CONST_STRPTR) (s ? TXT_HINT_OUT : TXT_HINT_IN));
		hint(n++, (UWORD) (id + TRIMBOX_BACK), (CONST_STRPTR) TXT_HINT_BACK);
		hint(n++, (UWORD) (id + TRIMBOX_ON), (CONST_STRPTR) TXT_HINT_ON);
		hint(n++, (UWORD) (id + TRIMBOX_LISTEN), (CONST_STRPTR) (s ? TXT_HINT_OUTLISTEN : TXT_HINT_INLISTEN));
		hint(n++, (UWORD) (id + TRIMBOX_FADE), (CONST_STRPTR) TXT_HINT_FADELEN);
		hint(n++, (UWORD) (id + TRIMBOX_SHOW), (CONST_STRPTR) (s ? TXT_HINT_ATOUT : TXT_HINT_ATIN));
	}
	tw.tw_Hints[n].hi_GadgetID = -1;	/* the end */
	tw.tw_Hints[n].hi_Code = -1;
	tw.tw_Hints[n].hi_Text = NULL;
	tw.tw_Hints[n].hi_Flags = 0;
}

BOOL TrimWin_Make(struct Screen *scr)
{
	Object *in, *out, *root;

	if (tw.tw_Win) return TRUE;
	if (!(tw.tw_GF = GroupFrame_MakeClass())) return FALSE;
	hints_fill();
	in = side(0, scr);
	out = side(1, scr);
	/* two sides of a size: the In and Out boxes side by side */
	tw.tw_Root = root = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_EvenSize, TRUE,
			 LAYOUT_SpaceOuter, TRUE, LAYOUT_SpaceInner, TRUE, GA_GadgetHelp, TRUE,
			 in ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) in, out ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) out, TAG_END);
	if (!in || !out || !root) {
		if (root) DisposeObject(root);
		else {
			if (in) DisposeObject(in);
			if (out) DisposeObject(out);
		}
		GroupFrame_FreeClass(tw.tw_GF);
		tw.tw_GF = NULL;
		return FALSE;
	}
	tw.tw_Win = NewObject(WINDOW_GetClass(), NULL, WA_Title, (ULONG) TXT_TRIM_TITLE, WA_DragBar, TRUE, WA_DepthGadget, TRUE,
			      WA_CloseGadget, TRUE, WA_Activate, TRUE, WA_IDCMP, IDCMP_RAWKEY,
			      WINDOW_HintInfo, (ULONG) tw.tw_Hints, WINDOW_GadgetHelp, TRUE, WINDOW_Layout, (ULONG) root, TAG_END);
	if (!tw.tw_Win) {
		DisposeObject(root);
		tw.tw_Root = NULL;
		GroupFrame_FreeClass(tw.tw_GF);
		tw.tw_GF = NULL;
		return FALSE;
	}
	return TRUE;
}

void TrimWin_Dispose(void)
{
	TrimWin_Close();
	if (tw.tw_Win) DisposeObject(tw.tw_Win);	/* its layout and gadgets with it */
	tw.tw_Win = tw.tw_Root = NULL;
	if (tw.tw_GF) GroupFrame_FreeClass(tw.tw_GF);
	tw.tw_GF = NULL;
}

BOOL TrimWin_Open(struct Window *main, WORD foot)
{
	struct Screen *scr;
	struct LayoutLimits lim;
	LONG l, t, w, h;

	if (!tw.tw_Win || !main) return FALSE;
	if (tw.tw_Window) {
		WindowToFront(tw.tw_Window);
		ActivateWindow(tw.tw_Window);
		return TRUE;
	}
	scr = main->WScreen;
	if (tw.tw_Placed) {
		l = tw.tw_Left;
		t = tw.tw_Top;
	} else {
		/* centred on the main window, standing on the waveform's foot:
		   the controls under it stay free (the main window fills the
		   screen, under it there is no room) */
		w = 320;
		h = 80;
		if (tw.tw_Root) {
			FlushLayoutDomainCache((struct Gadget *) tw.tw_Root);
			LayoutLimits((struct Gadget *) tw.tw_Root, &lim, scr->RastPort.Font, scr);
			w = lim.MinWidth + scr->WBorLeft + scr->WBorRight;
			h = lim.MinHeight + scr->WBorTop + scr->Font->ta_YSize + 1 + scr->WBorBottom;
		}
		l = main->LeftEdge + (main->Width - w) / 2;
		t = foot - h;
	}
	if (l < 0) l = 0;
	if (t < 0) t = 0;
	SetAttrs(tw.tw_Win, WA_PubScreen, (ULONG) scr, WA_Left, l, WA_Top, t, TAG_DONE);
	if (!(tw.tw_Window = (struct Window *) DoMethod(tw.tw_Win, WM_OPEN, NULL))) return FALSE;
	LendMenus(tw.tw_Window, main);
	return TRUE;
}

void TrimWin_Close(void)
{
	if (!tw.tw_Window) return;
	tw.tw_Left = tw.tw_Window->LeftEdge;
	tw.tw_Top = tw.tw_Window->TopEdge;
	tw.tw_Placed = TRUE;
	DoMethod(tw.tw_Win, WM_CLOSE);
	tw.tw_Window = NULL;
}

struct Window *TrimWin_Window(void)
{
	return tw.tw_Window;
}

ULONG TrimWin_Signal(void)
{
	ULONG m = 0;

	if (tw.tw_Win && tw.tw_Window) GetAttr(WINDOW_SigMask, tw.tw_Win, &m);
	return m;
}

BOOL TrimWin_Input(struct TrimEvent *te)
{
	struct InputEvent *ie;
	ULONG result, id;
	UWORD code;

	while (tw.tw_Win && tw.tw_Window && (result = DoMethod(tw.tw_Win, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
		te->te_Out = FALSE;
		te->te_Key = te->te_Qual = 0;
		switch (result & WMHI_CLASSMASK) {
		case WMHI_CLOSEWINDOW:
			te->te_What = TRIM_CLOSE;
			return TRUE;
		case WMHI_GADGETUP:
			id = (result & WMHI_GADGETMASK) - TW_GID;
			te->te_Out = (BOOL) (id >= TW_SIDE);
			switch (id % TW_SIDE) {
			case TRIMBOX_TIME: te->te_What = TRIM_TYPED; return TRUE;
			case TRIMBOX_BACK: te->te_What = TRIM_BACK; return TRUE;
			case TRIMBOX_ON: te->te_What = TRIM_ON; return TRUE;
			case TRIMBOX_LISTEN: te->te_What = TRIM_LISTEN; return TRUE;
			case TRIMBOX_SHOW: te->te_What = TRIM_SHOW; return TRUE;
			}
			break;
		case WMHI_RAWKEY:
			ie = NULL;
			GetAttr(WINDOW_InputEvent, tw.tw_Win, (ULONG *) &ie);
			te->te_What = TRIM_KEY;
			te->te_Key = (UWORD) (result & WMHI_KEYMASK);
			te->te_Qual = ie ? ie->ie_Qualifier : 0;
			return TRUE;
		}
	}
	return FALSE;
}

/* ghosted or not; a class that leaves the repaint to the caller is
   repainted (gui.c's ghost()) */
static void ghost(Object *o, BOOL off)
{
	if (SetGadgetAttrs((struct Gadget *) o, tw.tw_Window, NULL, GA_Disabled, (ULONG) off, TAG_DONE) && tw.tw_Window)
		RefreshGList((struct Gadget *) o, tw.tw_Window, NULL, 1);
}

void TrimWin_Show(const struct TrimShow *ts)
{
	UWORD s;
	struct Window *w = tw.tw_Window;

	if (!tw.tw_Win) return;
	for (s = 0; s < 2; s++) {
		Text_Copy((STRPTR) tw.tw_Time[s], ts->ts_Time[s] ? ts->ts_Time[s] : (CONST_STRPTR) "", TIME_LEN);
		Text_Copy((STRPTR) tw.tw_Fade[s], ts->ts_Fade[s] ? ts->ts_Fade[s] : (CONST_STRPTR) "", FADE_LEN);
		/* a closed window takes them now and draws them when it opens */
		SetGadgetAttrs((struct Gadget *) tw.tw_Part[s][TRIMBOX_TIME], w, NULL, STRINGA_TextVal, (ULONG) tw.tw_Time[s], TAG_DONE);
		SetGadgetAttrs((struct Gadget *) tw.tw_Part[s][TRIMBOX_FADE], w, NULL, GA_Text, (ULONG) tw.tw_Fade[s], TAG_DONE);
		ghost(tw.tw_Part[s][TRIMBOX_TIME], ts->ts_NoDoc);
		ghost(tw.tw_Part[s][TRIMBOX_BACK], ts->ts_NoDoc);
		ghost(tw.tw_Part[s][TRIMBOX_ON], ts->ts_NoDoc);
		ghost(tw.tw_Part[s][TRIMBOX_SHOW], ts->ts_NoDoc);
		ghost(tw.tw_Part[s][TRIMBOX_LISTEN], (BOOL) (ts->ts_NoDoc || ts->ts_NoListen));
	}
}

CONST_STRPTR TrimWin_Text(BOOL out)
{
	STRPTR s = NULL;

	if (tw.tw_Win) GetAttr(STRINGA_TextVal, tw.tw_Part[out ? 1 : 0][TRIMBOX_TIME], (ULONG *) &s);
	return (CONST_STRPTR) s;
}

BOOL TrimWin_Box(UWORD which, struct IBox *box)
{
	struct Gadget *g;

	if (!tw.tw_Window || which >= 2 * TRIMBOX_PARTS) return FALSE;
	g = (struct Gadget *) tw.tw_Part[which / TRIMBOX_PARTS][which % TRIMBOX_PARTS];
	box->Left = g->LeftEdge;
	box->Top = g->TopEdge;
	box->Width = g->Width;
	box->Height = g->Height;
	return TRUE;
}
