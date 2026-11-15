/*
 * gainwin.c -- Effects/Gain... in a window (gainwin.h)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/slider.h>
#include <gadgets/string.h>
#include <images/bevel.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/slider.h>
#include <proto/string.h>
#include <clib/alib_protos.h>

#include "gainwin.h"
#include "envelope.h"
#include "wavesmith_msg.h"
#include "textutil.h"

#define CLASS_VERSION	44
#define RAW_RETURN	0x44
#define RAW_ENTER	0x43
#define RAW_ESC		0x45
#define FIELD_LEN	8		/* "+18.0" and room */
#define SAID_LEN	96

extern struct Library *SliderBase;	/* margins.c's: opened while a window is up */

enum {
	GID_GN_SLIDER = 300, GID_GN_FIELD, GID_GN_NORMALISE, GID_GN_TARGET, GID_GN_LISTEN, GID_GN_USE, GID_GN_CANCEL
};

/* the window's gadgets and what they show */
struct Gain {
	struct GainArgs *gn_Args;
	Object *gn_Win;
	struct Window *gn_Window;
	Object *gn_Slider, *gn_Field, *gn_Said, *gn_Clips, *gn_Norm, *gn_Target, *gn_Listen, *gn_Use, *gn_Cancel;
	WORD gn_Tenths;			/* the gain shown */
	char gn_FieldText[FIELD_LEN];
	char gn_TargetText[FIELD_LEN];
	char gn_SaidText[SAID_LEN];
};

static WORD text_w(struct Screen *scr, CONST_STRPTR s)
{
	UWORD n = (UWORD) Text_Length(s);

	return (WORD) (scr ? TextLength(&scr->RastPort, (STRPTR) s, n) : n * 8);
}

/* text that does nothing */
static Object *shown(CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ReadOnly, TRUE, GA_Text, (ULONG) text,
			 BUTTON_BevelStyle, BVS_NONE, BUTTON_Justification, BCJ_LEFT, TAG_DONE);
}

static Object *button(ULONG id, CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_Text, (ULONG) text, TAG_DONE);
}

static Object *field(ULONG id, STRPTR text)
{
	return NewObject(STRING_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_TabCycle, TRUE,
			 STRINGA_TextVal, (ULONG) text, STRINGA_MaxChars, FIELD_LEN - 1, STRINGA_Justification, GACT_STRINGRIGHT,
			 TAG_DONE);
}

static void set_on(Object *o, struct Window *w, ULONG tag, ULONG value)
{
	if (SetGadgetAttrs((struct Gadget *) o, w, NULL, tag, value, TAG_DONE)) RefreshGList((struct Gadget *) o, w, NULL, 1);
}

static ULONG get(Object *o, ULONG tag)
{
	ULONG v = 0;

	GetAttr(tag, o, &v);
	return v;
}

static void tell(const struct Gain *gn, UWORD what)
{
	struct GainMsg gm;

	if (!gn->gn_Args->ga_Tell) return;
	gm.gm_What = what;
	gm.gm_Tenths = gn->gn_Tenths;
	gm.gm_Sigs = 0;
	gm.gm_Boxes = NULL;
	gn->gn_Args->ga_Tell(&gm);
}

/* the peak line for the gain shown: now and after, or a silent part */
static void say_peak(struct Gain *gn)
{
	struct TextArgs ta;
	char now[FIELD_LEN], after[FIELD_LEN];
	ULONG peak = gn->gn_Args->ga_Peak, gained;

	if (!peak) {
		Text_Copy((STRPTR) gn->gn_SaidText, (CONST_STRPTR) TXT_GN_SILENT, SAID_LEN);
	} else {
		/* a gain that takes the peak below the smallest sample: silent
		   after, not a figure of no meaning */
		gained = Gain_Peak(peak, gn->gn_Tenths);
		*Gain_PutTenths((STRPTR) now, Gain_PeakTenths(peak)) = '\0';
		*Gain_PutTenths((STRPTR) after, gained ? Gain_PeakTenths(gained) : 0) = '\0';
		ta.ta_Fmt = (CONST_STRPTR) (gained ? TXT_GN_PEAK : TXT_GN_PEAK_GONE);
		ta.ta_Str[0] = (CONST_STRPTR) now;
		ta.ta_Str[1] = (CONST_STRPTR) after;
		ta.ta_Str[2] = NULL;
		ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
		Text_Fmt((STRPTR) gn->gn_SaidText, SAID_LEN, &ta);
	}
}

/* the gain at t: the field, the slider (when the field moved it), the
   peak line, the clip line; the caller told */
struct Move {
	struct Gain *mv_Gain;
	WORD mv_Tenths;
	BOOL mv_Slider;			/* the slider set too */
};

static void move_to(const struct Move *mv)
{
	struct Gain *gn = mv->mv_Gain;
	struct Window *w = gn->gn_Window;
	WORD t = mv->mv_Tenths;

	if (t < ENV_TENTHS_MIN) t = ENV_TENTHS_MIN;
	if (t > ENV_TENTHS_MAX) t = ENV_TENTHS_MAX;
	if (t == gn->gn_Tenths && !mv->mv_Slider) return;
	gn->gn_Tenths = t;
	*Gain_PutTenths((STRPTR) gn->gn_FieldText, t) = '\0';
	set_on(gn->gn_Field, w, STRINGA_TextVal, (ULONG) gn->gn_FieldText);
	if (mv->mv_Slider) set_on(gn->gn_Slider, w, SLIDER_Level, (ULONG) (t - ENV_TENTHS_MIN));
	say_peak(gn);
	set_on(gn->gn_Said, w, GA_Text, (ULONG) gn->gn_SaidText);
	set_on(gn->gn_Clips, w, GA_Text, (ULONG) (Gain_Peak(gn->gn_Args->ga_Peak, t) > 32767UL ? (CONST_STRPTR) TXT_GN_CLIPS : (CONST_STRPTR) " "));
	tell(gn, GAIN_PREVIEW);
}

/* the target field read: its level (<= 0), or the one before */
static WORD target_of(struct Gain *gn)
{
	CONST_STRPTR s = (CONST_STRPTR) get(gn->gn_Target, STRINGA_TextVal);
	WORD t;

	if (s && Gain_ParseTenths(s, &t) && t <= 0 && t >= ENV_TENTHS_MIN) gn->gn_Args->ga_Target = t;
	return gn->gn_Args->ga_Target;
}

static void box_of(Object *o, struct IBox *b)
{
	struct Gadget *g = (struct Gadget *) o;

	b->Left = g->LeftEdge;
	b->Top = g->TopEdge;
	b->Width = g->Width;
	b->Height = g->Height;
}

static void shown_at(struct Gain *gn)
{
	struct IBox boxes[GAIN_BOXES];
	struct GainMsg gm;

	if (!gn->gn_Args->ga_Tell) return;
	boxes[GAIN_BOX_WINDOW].Left = gn->gn_Window->LeftEdge;
	boxes[GAIN_BOX_WINDOW].Top = gn->gn_Window->TopEdge;
	boxes[GAIN_BOX_WINDOW].Width = gn->gn_Window->Width;
	boxes[GAIN_BOX_WINDOW].Height = gn->gn_Window->Height;
	box_of(gn->gn_Field, &boxes[GAIN_BOX_FIELD]);
	box_of(gn->gn_Norm, &boxes[GAIN_BOX_NORMALISE]);
	box_of(gn->gn_Listen, &boxes[GAIN_BOX_LISTEN]);
	box_of(gn->gn_Use, &boxes[GAIN_BOX_USE]);
	box_of(gn->gn_Cancel, &boxes[GAIN_BOX_CANCEL]);
	gm.gm_What = GAIN_SHOWN;
	gm.gm_Tenths = gn->gn_Tenths;
	gm.gm_Sigs = 0;
	gm.gm_Boxes = boxes;
	gn->gn_Args->ga_Tell(&gm);
}

/* the window's loop until Use or Cancel */
static BOOL used(struct Gain *gn)
{
	struct GainArgs *ga = gn->gn_Args;
	struct GainMsg gm;
	struct Move mv;
	ULONG sigs = 0, got, r;
	UWORD code;
	LONG id;
	WORD t;

	mv.mv_Gain = gn;
	for (;;) {
		GetAttr(WINDOW_SigMask, gn->gn_Win, &sigs);
		got = Wait(sigs | ga->ga_SigMask | SIGBREAKF_CTRL_C);
		if (got & SIGBREAKF_CTRL_C) return FALSE;
		if ((got & ga->ga_SigMask) && ga->ga_Tell) {
			gm.gm_What = GAIN_SIGNALS;
			gm.gm_Tenths = gn->gn_Tenths;
			gm.gm_Sigs = got & ga->ga_SigMask;
			gm.gm_Boxes = NULL;
			ga->ga_Tell(&gm);
		}
		while ((r = DoMethod(gn->gn_Win, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (r & WMHI_CLASSMASK) {
			case WMHI_CLOSEWINDOW:
				return FALSE;
			case WMHI_GADGETUP:
				id = (LONG) (r & WMHI_GADGETMASK);
				if (id == GID_GN_USE) return TRUE;
				if (id == GID_GN_CANCEL) return FALSE;
				if (id == GID_GN_LISTEN) tell(gn, GAIN_LISTEN);
				if (id == GID_GN_SLIDER) {
					mv.mv_Tenths = (WORD) ((LONG) get(gn->gn_Slider, SLIDER_Level) + ENV_TENTHS_MIN);
					mv.mv_Slider = FALSE;
					move_to(&mv);
				}
				if (id == GID_GN_FIELD) {
					CONST_STRPTR s = (CONST_STRPTR) get(gn->gn_Field, STRINGA_TextVal);

					mv.mv_Tenths = gn->gn_Tenths;
					if (s && Gain_ParseTenths(s, &t)) mv.mv_Tenths = t;
					mv.mv_Slider = TRUE;
					move_to(&mv);
				}
				if (id == GID_GN_NORMALISE) {
					mv.mv_Tenths = Gain_Normalise(ga->ga_Peak, target_of(gn));
					mv.mv_Slider = TRUE;
					move_to(&mv);
				}
				if (id == GID_GN_TARGET) target_of(gn);
				break;
			case WMHI_INTUITICK:
				/* the slider dragged: its level looked at, nothing tells it */
				mv.mv_Tenths = (WORD) ((LONG) get(gn->gn_Slider, SLIDER_Level) + ENV_TENTHS_MIN);
				mv.mv_Slider = FALSE;
				move_to(&mv);
				break;
			case WMHI_RAWKEY:
				if ((r & WMHI_KEYMASK) == RAW_ESC) return FALSE;
				if ((r & WMHI_KEYMASK) == RAW_RETURN || (r & WMHI_KEYMASK) == RAW_ENTER) return TRUE;
				break;
			}
		}
	}
}

/* the rows: gain, the peak, the clip line, Normalise, the buttons -- the
   root layout, or NULL with every gadget gone */
static Object *make_root(struct Gain *gn, struct Screen *scr)
{
	Object *g1, *g2, *g3, *g4, *gap, *rest, *row1, *row3, *row4, *root;
	WORD fw = (WORD) (text_w(scr, (CONST_STRPTR) "+18.0") + 24);

	gn->gn_Slider = NewObject(SLIDER_GetClass(), NULL, GA_ID, GID_GN_SLIDER, GA_RelVerify, TRUE,
				  SLIDER_Min, 0, SLIDER_Max, ENV_TENTHS_MAX - ENV_TENTHS_MIN,
				  SLIDER_Level, (ULONG) (gn->gn_Tenths - ENV_TENTHS_MIN), SLIDER_Orientation, SLIDER_HORIZONTAL,
				  TAG_DONE);
	gn->gn_Field = field(GID_GN_FIELD, (STRPTR) gn->gn_FieldText);
	gn->gn_Said = shown((CONST_STRPTR) gn->gn_SaidText);
	gn->gn_Clips = shown((CONST_STRPTR) " ");	/* a blank: a line high all the same */
	gn->gn_Norm = button(GID_GN_NORMALISE, (CONST_STRPTR) TXT_GN_NORMALISE);
	gn->gn_Target = field(GID_GN_TARGET, (STRPTR) gn->gn_TargetText);
	gn->gn_Listen = button(GID_GN_LISTEN, (CONST_STRPTR) TXT_LISTEN);
	gn->gn_Use = button(GID_GN_USE, (CONST_STRPTR) TXT_MG_USE);
	gn->gn_Cancel = button(GID_GN_CANCEL, (CONST_STRPTR) TXT_MG_CANCEL);
	g1 = shown((CONST_STRPTR) TXT_GN_DB);
	g2 = shown((CONST_STRPTR) TXT_GN_TO);
	g3 = shown((CONST_STRPTR) TXT_GN_DB);
	g4 = shown((CONST_STRPTR) TXT_GN_GAIN);
	gap = NewObject(LAYOUT_GetClass(), NULL, TAG_DONE);
	rest = NewObject(LAYOUT_GetClass(), NULL, TAG_DONE);
	if (!gn->gn_Slider || !gn->gn_Field || !gn->gn_Said || !gn->gn_Clips || !gn->gn_Norm || !gn->gn_Target ||
	    !gn->gn_Listen || !gn->gn_Use || !gn->gn_Cancel || !g1 || !g2 || !g3 || !g4 || !gap || !rest) {
		Object *all[15];
		UWORD i;

		all[0] = gn->gn_Slider; all[1] = gn->gn_Field; all[2] = gn->gn_Said; all[3] = gn->gn_Clips;
		all[4] = gn->gn_Norm; all[5] = gn->gn_Target; all[6] = gn->gn_Listen; all[7] = gn->gn_Use;
		all[8] = gn->gn_Cancel; all[9] = g1; all[10] = g2; all[11] = g3; all[12] = g4; all[13] = gap; all[14] = rest;
		for (i = 0; i < 15; i++) if (all[i]) DisposeObject(all[i]);
		return NULL;
	}
	/* WARNING: from here each layout owns the children it was given and
	   disposes them when it cannot be made itself */
	row1 = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			 LAYOUT_VertAlignment, LALIGN_CENTER,
			 LAYOUT_AddChild, (ULONG) g4, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) gn->gn_Slider, CHILD_WeightedWidth, 100,
			 CHILD_MinWidth, text_w(scr, (CONST_STRPTR) "0000000000000000000000000"),
			 LAYOUT_AddChild, (ULONG) gn->gn_Field, CHILD_WeightedWidth, 0, CHILD_MinWidth, fw, CHILD_MaxWidth, fw,
			 LAYOUT_AddChild, (ULONG) g1, CHILD_WeightedWidth, 0, TAG_DONE);
	row3 = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			 LAYOUT_VertAlignment, LALIGN_CENTER,
			 LAYOUT_AddChild, (ULONG) gn->gn_Norm, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) g2, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) gn->gn_Target, CHILD_WeightedWidth, 0, CHILD_MinWidth, fw, CHILD_MaxWidth, fw,
			 LAYOUT_AddChild, (ULONG) g3, CHILD_WeightedWidth, 0,
			 /* the row packed to the left: the room after it empty */
			 LAYOUT_AddChild, (ULONG) rest, CHILD_WeightedWidth, 100, TAG_DONE);
	row4 = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			 LAYOUT_AddChild, (ULONG) gn->gn_Listen, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) gap, CHILD_WeightedWidth, 100,
			 LAYOUT_AddChild, (ULONG) gn->gn_Use, CHILD_WeightedWidth, 0,
			 LAYOUT_AddChild, (ULONG) gn->gn_Cancel, CHILD_WeightedWidth, 0, TAG_DONE);
	if (!row1 || !row3 || !row4) {
		if (row1) DisposeObject(row1);
		if (row3) DisposeObject(row3);
		if (row4) DisposeObject(row4);
		/* a row that could not be made took its children; the peak and
		   the clip lines were never given to one */
		DisposeObject(gn->gn_Said);
		DisposeObject(gn->gn_Clips);
		return NULL;
	}
	root = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE,
			 LAYOUT_SpaceInner, TRUE,
			 LAYOUT_AddChild, (ULONG) row1, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) gn->gn_Said, CHILD_WeightedHeight, 0,
			 CHILD_MinWidth, text_w(scr, (CONST_STRPTR) TXT_GN_PEAK) + 60,
			 LAYOUT_AddChild, (ULONG) gn->gn_Clips, CHILD_WeightedHeight, 0,
			 CHILD_MinWidth, text_w(scr, (CONST_STRPTR) TXT_GN_CLIPS) + 12,
			 LAYOUT_AddChild, (ULONG) row3, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) row4, CHILD_WeightedHeight, 0, TAG_DONE);
	return root;
}

BOOL Gain_Window(struct GainArgs *ga)
{
	static struct Gain gn;
	struct Screen *scr = ga->ga_Over ? ga->ga_Over->WScreen : NULL;
	struct Move mv;
	Object *root;
	CONST_STRPTR s;
	WORD t;
	BOOL ok = FALSE;
	UBYTE *p = (UBYTE *) &gn;
	ULONG i;

	for (i = 0; i < sizeof(gn); i++) p[i] = 0;
	ga->ga_NoWindow = TRUE;
	gn.gn_Args = ga;
	gn.gn_Tenths = ga->ga_Tenths < ENV_TENTHS_MIN ? ENV_TENTHS_MIN : ga->ga_Tenths > ENV_TENTHS_MAX ? ENV_TENTHS_MAX : ga->ga_Tenths;
	*Gain_PutTenths((STRPTR) gn.gn_FieldText, gn.gn_Tenths) = '\0';
	*Gain_PutTenths((STRPTR) gn.gn_TargetText, ga->ga_Target) = '\0';
	say_peak(&gn);
	if (!(SliderBase = OpenLibrary((STRPTR) "gadgets/slider.gadget", CLASS_VERSION))) return FALSE;
	if (!(root = make_root(&gn, scr))) {
		CloseLibrary(SliderBase);
		SliderBase = NULL;
		return FALSE;
	}
	gn.gn_Win = NewObject(WINDOW_GetClass(), NULL, WA_Title, (ULONG) TXT_GN_TITLE, WA_CloseGadget, TRUE, WA_DragBar, TRUE,
			      WA_DepthGadget, TRUE, WA_Activate, TRUE, WA_AutoAdjust, TRUE,
			      WA_IDCMP, IDCMP_RAWKEY | IDCMP_INTUITICKS,
			      WA_PubScreen, (ULONG) scr,
			      ga->ga_Over ? WINDOW_RefWindow : TAG_IGNORE, (ULONG) ga->ga_Over,
			      WINDOW_Position, ga->ga_Over ? WPOS_CENTERWINDOW : WPOS_CENTERSCREEN,
			      WINDOW_Layout, (ULONG) root, TAG_DONE);
	if (!gn.gn_Win) {
		DisposeObject(root);
	} else if ((gn.gn_Window = (struct Window *) DoMethod(gn.gn_Win, WM_OPEN, NULL))) {
		ga->ga_NoWindow = FALSE;
		shown_at(&gn);
		/* the waveform behind at the gain the window opens with */
		mv.mv_Gain = &gn;
		mv.mv_Tenths = gn.gn_Tenths;
		mv.mv_Slider = TRUE;
		move_to(&mv);
		ok = used(&gn);
		if (ok) {
			/* a gain typed and not yet entered counts: what the field says */
			s = (CONST_STRPTR) get(gn.gn_Field, STRINGA_TextVal);
			if (s && Gain_ParseTenths(s, &t)) {
				mv.mv_Tenths = t;
				mv.mv_Slider = TRUE;
				move_to(&mv);
			}
			ga->ga_Tenths = gn.gn_Tenths;
		}
		target_of(&gn);
	}
	if (gn.gn_Win) DisposeObject(gn.gn_Win);	/* the window, the layouts, the gadgets */
	CloseLibrary(SliderBase);
	SliderBase = NULL;
	return ok;
}
