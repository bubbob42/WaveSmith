/*
 * trimwin.h -- the Trim window (Documentation/Concept-WaveSmith-Panel-
 * 2026-11.md, 4 and 11): In and Out side by side, each its time, the
 * nudge arrows, Listen, its fade's length and Show. Not modal: a window
 * of its own beside WaveSmith's, served by the same loop -- the caller
 * Waits on TrimWin_Signal() as well and asks TrimWin_Input() what came;
 * the main window's menus are lent to it (LendMenus), so the menu button
 * and the Right-Amiga keys work from either.
 *
 * The window object and its gadgets live from TrimWin_Make() to
 * TrimWin_Dispose(); TrimWin_Open() and TrimWin_Close() only show and
 * hide it (window.class keeps the gadgets), so TrimWin_Show() may come at
 * any time: a closed window shows the values when it opens.
 */
#ifndef WAVESMITH_TRIMWIN_H
#define WAVESMITH_TRIMWIN_H

#include <exec/types.h>
#include <intuition/intuition.h>

/* what came (TrimEvent.te_What) */
#define TRIM_TYPED	1		/* a time typed into a side: TrimWin_Text() */
#define TRIM_BACK	2		/* a side's nudge arrow back */
#define TRIM_ON		3		/* and on */
#define TRIM_LISTEN	4
#define TRIM_SHOW	5		/* the view to the side's handle */
#define TRIM_CLOSE	6		/* the close gadget: the caller closes it */
#define TRIM_KEY	7		/* a key the window does not take: te_Key, te_Qual */

struct TrimEvent {
	UWORD te_What;			/* TRIM_#? */
	BOOL te_Out;			/* the side: Out, else In */
	UWORD te_Key, te_Qual;		/* TRIM_KEY: the raw key and its qualifiers */
};

/* what the window shows; texts the caller's, copied */
struct TrimShow {
	CONST_STRPTR ts_Time[2];	/* In, Out */
	CONST_STRPTR ts_Fade[2];	/* the fades' lengths, "2.0 s" or the word for none */
	BOOL ts_NoDoc;			/* nothing to trim: every gadget ghosted */
	BOOL ts_NoListen;		/* nothing to play through: Listen ghosted */
};

/* a gadget's place, for the tests' trace (TrimWin_Box) */
#define TRIMBOX_TIME	0
#define TRIMBOX_BACK	1
#define TRIMBOX_ON	2
#define TRIMBOX_LISTEN	3
#define TRIMBOX_FADE	4
#define TRIMBOX_SHOW	5
#define TRIMBOX_PARTS	6		/* a side's; Out's are TRIMBOX_PARTS on */

/* the window and its gadgets for this screen's font, not open: FALSE
   (nothing kept) without memory or classes */
BOOL TrimWin_Make(struct Screen *scr);
void TrimWin_Dispose(void);
/* open on main's screen where it was last (the session's), else centred
   on main with its foot on screen row foot (over the waveform's bottom,
   clear of the controls under it); main's menus lent to it. FALSE: not
   opened */
BOOL TrimWin_Open(struct Window *main, WORD foot);
void TrimWin_Close(void);
struct Window *TrimWin_Window(void);	/* NULL while closed */
ULONG TrimWin_Signal(void);		/* its port's signal; 0 while closed */
BOOL TrimWin_Input(struct TrimEvent *te);	/* the next event; FALSE: none left */
void TrimWin_Show(const struct TrimShow *ts);
CONST_STRPTR TrimWin_Text(BOOL out);	/* the text in a side's field now */
/* where a gadget is in the window (TRIMBOX_#?, + TRIMBOX_PARTS for Out):
   FALSE while closed */
BOOL TrimWin_Box(UWORD which, struct IBox *box);

#endif
