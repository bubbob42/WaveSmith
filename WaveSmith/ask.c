/*
 * ask.c -- a question centred on the window (ask.h)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <images/bevel.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <clib/alib_protos.h>

#include "ask.h"
#include "textutil.h"

#define BODY_LEN	512
#define GADS_LEN	128
#define GID_ASK		100		/* the buttons: GID_ASK + n */
#define RAW_RETURN	0x44
#define RAW_ENTER	0x43
#define RAW_ESC		0x45

/* SAS/C follows no write through an index: the arrays cleared as blocks */
static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

/* the copies cut into pieces at sep, in place: how many */
struct Cut {
	char *ct_Text;
	char ct_Sep;
	UWORD ct_Max;
	CONST_STRPTR ct_Out[ASK_LINES];
};

static UWORD cut(struct Cut *ct)
{
	char *p, *start = ct->ct_Text;
	UWORD n = 0;

	for (p = ct->ct_Text; *p; p++) {
		if (*p != ct->ct_Sep) continue;
		*p = '\0';
		if (n < ct->ct_Max) ct->ct_Out[n++] = (CONST_STRPTR) start;
		start = p + 1;
	}
	if (n < ct->ct_Max && *start) ct->ct_Out[n++] = (CONST_STRPTR) start;
	return n;
}

/* the same question where EasyRequest puts it: without the memory for the
   window; the underscores (the buttons' keys) left out, the body passed
   as an argument (a file name may hold a %) */
static LONG ask_easy(const struct AskArgs *aa)
{
	struct EasyStruct es;
	char plain[GADS_LEN];
	UWORD i, n = 0;

	for (i = 0; aa->aa_Gadgets[i] && n < GADS_LEN - 1; i++) {
		if (aa->aa_Gadgets[i] != '_') plain[n++] = aa->aa_Gadgets[i];
	}
	plain[n] = '\0';
	es.es_StructSize = sizeof(es);
	es.es_Flags = 0;
	es.es_Title = (STRPTR) aa->aa_Title;
	es.es_TextFormat = (STRPTR) "%s";
	es.es_GadgetFormat = (STRPTR) plain;
	return EasyRequest(aa->aa_Over, &es, NULL, (ULONG) aa->aa_Body);
}

/* a line of the question: a read-only button with no frame, which centres
   (a label measures itself to its text and has nothing to centre in) */
static Object *line_of(CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ReadOnly, TRUE, GA_Text, (ULONG) text, BUTTON_BevelStyle, BVS_NONE,
			 BUTTON_Justification, BCJ_CENTER, TAG_DONE);
}

/* the window's own loop until a button, a key or the close gadget answers */
static LONG answer_of(Object *win, UWORD buttons)
{
	ULONG sigs = 0, r;
	UWORD code;
	LONG n;

	for (;;) {
		GetAttr(WINDOW_SigMask, win, &sigs);
		if (Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) return 0;
		while ((r = DoMethod(win, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (r & WMHI_CLASSMASK) {
			case WMHI_CLOSEWINDOW:
				return 0;
			case WMHI_GADGETUP:
				n = (LONG) (r & WMHI_GADGETMASK) - GID_ASK;
				if (n >= 0 && n < (LONG) buttons) return n == (LONG) buttons - 1 ? 0 : n + 1;
				break;
			case WMHI_RAWKEY:
				if ((r & WMHI_KEYMASK) == RAW_ESC) return 0;
				if ((r & WMHI_KEYMASK) == RAW_RETURN || (r & WMHI_KEYMASK) == RAW_ENTER) return buttons > 1 ? 1 : 0;
				break;
			}
		}
	}
}

LONG Ask_Window(const struct AskArgs *aa)
{
	char body[BODY_LEN], gads[GADS_LEN];
	struct Cut lines, names;
	Object *btn[ASK_BUTTONS], *lab[ASK_LINES], *row, *centre, *root, *win;
	struct Window *w;
	UWORD nl, nb, i;
	LONG answer;

	Text_Copy((STRPTR) body, aa->aa_Body, BODY_LEN);
	Text_Copy((STRPTR) gads, aa->aa_Gadgets, GADS_LEN);
	lines.ct_Text = body;
	lines.ct_Sep = '\n';
	lines.ct_Max = ASK_LINES;
	names.ct_Text = gads;
	names.ct_Sep = '|';
	names.ct_Max = ASK_BUTTONS;
	nl = cut(&lines);
	if ((nb = cut(&names)) < 1) return 0;
	clear_bytes((UBYTE *) btn, sizeof(btn));
	clear_bytes((UBYTE *) lab, sizeof(lab));
	for (i = 0; i < nb; i++)
		btn[i] = NewObject(BUTTON_GetClass(), NULL, GA_ID, GID_ASK + i, GA_RelVerify, TRUE, GA_Text, (ULONG) names.ct_Out[i],
				   TAG_DONE);
	for (i = 0; i < nl; i++) lab[i] = line_of(lines.ct_Out[i]);
	/* the buttons as wide as the widest, the row centred under the text */
	row = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_EvenSize, TRUE,
			LAYOUT_SpaceInner, TRUE,
			btn[0] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) btn[0], btn[0] ? CHILD_WeightedWidth : TAG_IGNORE, 0,
			btn[1] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) btn[1], btn[1] ? CHILD_WeightedWidth : TAG_IGNORE, 0,
			btn[2] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) btn[2], btn[2] ? CHILD_WeightedWidth : TAG_IGNORE, 0,
			btn[3] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) btn[3], btn[3] ? CHILD_WeightedWidth : TAG_IGNORE, 0,
			TAG_DONE);
	if (!row) {
		/* nothing has a parent yet: everything still ours */
		for (i = 0; i < ASK_BUTTONS; i++) if (btn[i]) DisposeObject(btn[i]);
		for (i = 0; i < ASK_LINES; i++) if (lab[i]) DisposeObject(lab[i]);
		return ask_easy(aa);
	}
	centre = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_HorizAlignment, LALIGN_CENTER,
			   LAYOUT_AddChild, (ULONG) row, CHILD_WeightedWidth, 0, TAG_DONE);
	if (!centre) {
		DisposeObject(row);		/* it took the buttons */
		for (i = 0; i < ASK_LINES; i++) if (lab[i]) DisposeObject(lab[i]);
		return ask_easy(aa);
	}
	/* WARNING: the layout owns its children from here, and disposes them
	   all when it cannot be made */
	root = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE,
			 LAYOUT_SpaceInner, TRUE,
			 lab[0] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[0], lab[0] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[1] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[1], lab[1] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[2] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[2], lab[2] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[3] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[3], lab[3] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[4] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[4], lab[4] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[5] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[5], lab[5] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[6] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[6], lab[6] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 lab[7] ? LAYOUT_AddChild : TAG_IGNORE, (ULONG) lab[7], lab[7] ? CHILD_WeightedHeight : TAG_IGNORE, 0,
			 LAYOUT_AddChild, (ULONG) centre, CHILD_WeightedHeight, 0, TAG_DONE);
	if (!root) return ask_easy(aa);
	win = NewObject(WINDOW_GetClass(), NULL, WA_Title, (ULONG) aa->aa_Title, WA_CloseGadget, TRUE, WA_DragBar, TRUE,
			WA_DepthGadget, TRUE, WA_Activate, TRUE, WA_AutoAdjust, TRUE,
			/* keys at all: without them the underscored letters do nothing */
			WA_IDCMP, IDCMP_RAWKEY,
			WA_PubScreen, (ULONG) (aa->aa_Over ? aa->aa_Over->WScreen : NULL),
			aa->aa_Over ? WINDOW_RefWindow : TAG_IGNORE, (ULONG) aa->aa_Over,
			WINDOW_Position, aa->aa_Over ? WPOS_CENTERWINDOW : WPOS_CENTERSCREEN,
			WINDOW_Layout, (ULONG) root, TAG_DONE);
	if (!win) {
		DisposeObject(root);
		return ask_easy(aa);
	}
	if (!(w = (struct Window *) DoMethod(win, WM_OPEN, NULL))) {
		DisposeObject(win);
		return ask_easy(aa);
	}
	if (aa->aa_Opened) {
		aa->aa_Opened->Left = w->LeftEdge;
		aa->aa_Opened->Top = w->TopEdge;
		aa->aa_Opened->Width = w->Width;
		aa->aa_Opened->Height = w->Height;
	}
	answer = answer_of(win, nb);
	DisposeObject(win);		/* the window, the layout, the buttons */
	return answer;
}
