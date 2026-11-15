/*
 * gui.c -- WaveSmith's window (gui.h)
 *
 * Top to bottom (Concept-WaveSmith-2026-10.md, window A): the overview
 * (waveform.gadget in its overview mode: the whole file, a box for the
 * view), the waveform (the elastic row: a bigger window grows it), the
 * scroller in columns of the zoom, the trim row (In and Out with their
 * nudges, the kept length), the buttons, the status written into a fuel
 * gauge (no row of its own while reading: it shows the per cent).
 *
 * The file is read a piece at a time from this loop (doc.c): between two
 * pieces the window's signals are looked at without waiting, so the
 * window answers while a 229 MB side is read. Every per cent the gadgets
 * are told the source grew (WAVEFORM_Source again): the columns that
 * changed are drawn, nothing else.
 *
 * The gadgets talk back with a signal (live: a handle dragged, the view
 * scrolled, samples wanted) and a GADGETUP (a gesture done): the signal
 * is read with Waveform_Changes(), then what changed with OM_GET.
 *
 * ONE OBJECT, ONE PARENT: every gadget goes into the window's layout and
 * is disposed with it.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <intuition/icclass.h>
#include <intuition/imageclass.h>
#include <devices/inputevent.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <graphics/gfxbase.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/string.h>
#include <gadgets/scroller.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/tapedeck.h>
#include <images/label.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/asl.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/scroller.h>
#include <proto/fuelgauge.h>
#include <proto/label.h>
#include <proto/icon.h>
#include <clib/alib_protos.h>

#include <gadgets/waveform.h>

#include "gui.h"
#include "doc.h"
#include "play.h"
#include "decode.h"
#include "clock.h"
#include "prefetch.h"
#include "headsprite.h"
#include "trimwin.h"
#include "keyglyph.h"
#include "ask.h"
#include "about.h"
#include "undo.h"
#include "mapcut.h"
#include "settings.h"
#include "margins.h"
#include "marks.h"
#include "cuesheet.h"
#include "ahiplay.h"
#include "help.h"
#include "gainwin.h"
#include "beside.h"
#include "wavesmith_msg.h"
#include "textutil.h"
#include "levelmap.h"
#include "waveplan.h"
#include "boopsi.h"
#include "WaveSmith_rev.h"

#define CLASS_VERSION	44
#define READ_PIECES	2		/* pieces read between two looks at the window */
#define ATRANGE_MS	1500		/* At In / At Out: so long either side */
#define LISTEN_MS	3000		/* Listen: so long from In, up to Out */
#define SNAP_MIN	32		/* frames a zero-crossing search looks at least */
#define OVER_HEIGHT	24		/* points: the overview strip, its box */
#define HEAD_MS		20UL		/* the play head's beat: 50 a second */
#define FETCH_MAX	524288UL	/* frames a deep view's samples may be: 2 MB of stereo */
#define PRE_MAX		(2UL * FETCH_MAX)	/* a read-ahead buffer's at most: two of the deepest pages */
#define STATUS_LEN	160
#define TIME_LEN	WAVEPLAN_TIMELEN
#define FADE_TEXT	16		/* "120.5 s", the language's word for none */

/* Defined, never tentative: an uninitialised base is a COMMON symbol, and
 * GCC's linker then brings libnix's auto-open stub for it. */
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *AslBase = NULL;
struct Library *WindowBase = NULL;
struct Library *LayoutBase = NULL;
struct Library *DiskfontBase = NULL;	/* optional: the Trim window's section titles (groupframe) */
struct Library *ButtonBase = NULL;
struct Library *StringBase = NULL;
struct Library *ScrollerBase = NULL;
struct Library *FuelGaugeBase = NULL;
struct Library *TapeDeckBase = NULL;
struct Library *LabelBase = NULL;
struct Library *IconBase = NULL;		/* none: no icons made */

static CONST_STRPTR g_Missing = NULL;

enum {
	GID_OVER = 1, GID_WAVE, GID_SCROLL, GID_LEN, GID_DECK, GID_ZOOMIN, GID_ZOOMOUT, GID_ALL, GID_GAUGE,
	GID_CANCEL, GID_LOOP, GID_TRIMWIN, GID_FLAGIN, GID_FLAGOUT
};
enum {
	MID_OPEN = 1, MID_NEXT, MID_SAVE, MID_REVERT, MID_ABOUT, MID_QUIT,
	MID_ZOOMIN, MID_ZOOMOUT, MID_ALL, MID_ATIN, MID_ATOUT, MID_SCALE, MID_ZOOMSEL,
	MID_UNDO, MID_REDO, MID_CUTOUT, MID_SELALL, MID_SELNONE, MID_SPLIT,
	MID_SNAP, MID_MARGINS, MID_BACKUP, MID_ICONS, MID_SAVEPREFS, MID_SAVEAS,
	MID_PLAY, MID_PLAYSEL, MID_STOP, MID_LISTENIN, MID_LISTENOUT, MID_LOOP,
	MID_ADDMARK, MID_DELMARK, MID_FINDTRACKS, MID_SPLITMARKS,
	MID_WAYAUTO, MID_WAYCARD, MID_WAYAHI, MID_AHIMODE, MID_HELP, MID_FADEIN, MID_FADEOUT,
	MID_GAIN, MID_NORMALISE, MID_PLAIN, MID_PREVMARK, MID_NEXTMARK, MID_TRIMWIN
};

/* the keys the window takes itself (raw codes) */
#define RAW_SPACE	0x40
#define RAW_ENTER	0x43
#define RAW_RETURN	0x44
#define RAW_ESC		0x45
#define RAW_DEL		0x46
#define RAW_UP		0x4C
#define RAW_DOWN	0x4D
#define RAW_RIGHT	0x4E
#define RAW_LEFT	0x4F
#define RAW_HELP	0x5F
#define RAW_F1		0x50
#define RAW_F2		0x51

/* what the arrows move: the handle last touched, else the play marker */
#define TARGET_MARKER	0
#define TARGET_IN	1
#define TARGET_OUT	2

/* --- libraries ----------------------------------------------------------- */

static struct Library *open_class(CONST_STRPTR name)
{
	struct Library *base = OpenLibrary((STRPTR) name, CLASS_VERSION);

	if (!base && !g_Missing) g_Missing = name;
	return base;
}

BOOL Gui_OpenLibs(void)
{
	if (!(UtilityBase = OpenLibrary((STRPTR) "utility.library", 39))) {
		g_Missing = (CONST_STRPTR) "utility.library";
		return FALSE;
	}
	if (!(IntuitionBase = (struct IntuitionBase *) OpenLibrary((STRPTR) "intuition.library", 39))) {
		g_Missing = (CONST_STRPTR) "intuition.library";
		return FALSE;
	}
	if (!(GfxBase = (struct GfxBase *) OpenLibrary((STRPTR) "graphics.library", 39))) {
		g_Missing = (CONST_STRPTR) "graphics.library";
		return FALSE;
	}
	AslBase = OpenLibrary((STRPTR) "asl.library", 39);
	WindowBase = open_class((CONST_STRPTR) "window.class");
	LayoutBase = open_class((CONST_STRPTR) "gadgets/layout.gadget");
	DiskfontBase = OpenLibrary((STRPTR) "diskfont.library", 36);
	ButtonBase = open_class((CONST_STRPTR) "gadgets/button.gadget");
	StringBase = open_class((CONST_STRPTR) "gadgets/string.gadget");
	ScrollerBase = open_class((CONST_STRPTR) "gadgets/scroller.gadget");
	FuelGaugeBase = open_class((CONST_STRPTR) "gadgets/fuelgauge.gadget");
	TapeDeckBase = open_class((CONST_STRPTR) "gadgets/tapedeck.gadget");
	LabelBase = open_class((CONST_STRPTR) "images/label.image");
	IconBase = OpenLibrary((STRPTR) "icon.library", 44);
	Play_Open();			/* without the card's library: no playing */
	Decode_Open();			/* without prismadsp.library: WAV only */
	return (BOOL) (WindowBase && LayoutBase && ButtonBase && StringBase && ScrollerBase && FuelGaugeBase && TapeDeckBase &&
		       LabelBase);
}

void Gui_CloseLibs(void)
{
	Decode_Close();
	Play_Close();
	if (IconBase) { CloseLibrary(IconBase); IconBase = NULL; }
	if (LabelBase) { CloseLibrary(LabelBase); LabelBase = NULL; }
	if (TapeDeckBase) { CloseLibrary(TapeDeckBase); TapeDeckBase = NULL; }
	if (FuelGaugeBase) { CloseLibrary(FuelGaugeBase); FuelGaugeBase = NULL; }
	if (ScrollerBase) { CloseLibrary(ScrollerBase); ScrollerBase = NULL; }
	if (StringBase) { CloseLibrary(StringBase); StringBase = NULL; }
	if (ButtonBase) { CloseLibrary(ButtonBase); ButtonBase = NULL; }
	if (LayoutBase) { CloseLibrary(LayoutBase); LayoutBase = NULL; }
	if (DiskfontBase) { CloseLibrary(DiskfontBase); DiskfontBase = NULL; }
	if (WindowBase) { CloseLibrary(WindowBase); WindowBase = NULL; }
	if (AslBase) { CloseLibrary(AslBase); AslBase = NULL; }
	if (GfxBase) { CloseLibrary((struct Library *) GfxBase); GfxBase = NULL; }
	if (IntuitionBase) { CloseLibrary((struct Library *) IntuitionBase); IntuitionBase = NULL; }
	if (UtilityBase) { CloseLibrary(UtilityBase); UtilityBase = NULL; }
}

CONST_STRPTR Gui_Missing(void)
{
	return g_Missing;
}

/* --- the window's state ---------------------------------------------------- */

/* SAS/C keeps the statics near (A4-relative, 32K either side of it, all
   modules' together): the big ones far -- nothing to the other compilers */
#ifdef __SASC
#define WS_FAR	__far
#else
#define WS_FAR
#endif

static WS_FAR struct Doc g_Doc;
static WS_FAR struct Timeline g_SaveTl;	/* a save's timeline: the doc is closed after it */
static struct TlLevels g_SaveLv;		/* and its end fades and dips */
static struct IBox g_AskBox;		/* a question's window as it opened (the trace) */
static WS_FAR struct Undo g_Undo;	/* the user's changes, step by step (undo.h) */
/* a timeline put aside for a while: a step's pieces before they are shown
   (undo_step), the pieces before the Gain window (Cancel's) or Remove
   fades and gain -- one, not three: SAS/C merges far data into the 64K
   of the near (slink SD) */
static WS_FAR struct Timeline g_SpareTl;
static WS_FAR struct HelpContext g_Help;	/* the manual (help.h): opened at the first Help */

/* the suggestion's regions where they play on the timeline, a silence's
   tint, a mark at every join: the arrays the waveform borrows */
#define SHOWN_REGIONS	128
#define SHOWN_JOINS	64
static struct WaveRegion g_ShownRegions[SHOWN_REGIONS];
static struct WaveMarker g_Joins[SHOWN_JOINS + MARKS_MAX];	/* the markers first, then the joins */
/* the pieces at a level of their own (a fade, a gain), a tag's words
   where the gain changes: the arrays the waveform borrows */
#define SHOWN_LEVELS	128
#define SHOWN_TAGS	32
static struct WaveLevel g_ShownLevels[SHOWN_LEVELS];
static char g_LevelText[SHOWN_TAGS][12];		/* "+18.0 dB" */
/* the markers (marks.h): the tracks' starts, timeline frames; the one
   clicked (Del takes it away); each one's label, the track it starts */
static struct Marks g_Marks;
static UWORD g_MarkSel = MARKS_NONE;
static char g_MarkText[MARKS_MAX][4];
static BOOL g_HaveDoc = FALSE, g_Reading = FALSE;
static BOOL g_HeadSprite = FALSE;	/* the play heads sprites (headsprite.h) */
static UWORD g_HeadOff = 0;		/* the lines they show (1 << HEAD_#?): those gadgets told so */
static BOOL g_HaveClock = FALSE;	/* the E-clock and a MICROHZ wake: the head's beat, the time left */
static BOOL g_PlanShown = FALSE;			/* the suggestion shown (a long file's: while reading on) */
static BOOL g_Dirty = FALSE;			/* the user's changes not saved: the title's * */

static void set_dirty(BOOL d);
static void edit_menu(void);
static void changed(void);
static LONG ask(CONST_STRPTR text, CONST_STRPTR gadgets);
static void menu_on(UWORD num, BOOL on);
static void do_split(void);
static void do_save_as(void);
static BOOL there(CONST_STRPTR path);
static void cue_read(void);
static ULONG trim_in(void);
static ULONG trim_out(void);
static BOOL levels_now(void);
static BOOL doc_usable(void);
static void levels_shown(void);
static void timeline_shown(void);
static void clear_bytes(UBYTE *p, ULONG n);
static void play_way(void);
static void trace_marks(void);
static void marker_up(void);
static void add_marker(void);
static void del_marker(void);
static void trim_open(void);
static void find_tracks(void);
static void do_split_marks(void);
/* a cue sheet to write beside a new file (cue_write) */
struct CueOut {
	CONST_STRPTR co_Sound;
	ULONG co_In, co_Out, co_Rate;
	BOOL co_Backup;
};
static void cue_write(const struct CueOut *co);
static void marker_to(ULONG f);
static BOOL g_Decoding = FALSE;		/* prismadsp.library's job runs */
static BOOL g_Decoded = FALSE;		/* the doc is a work file: Save makes a new one */
static char g_Source[DOC_PATHLEN];	/* the file the user opened */
static UWORD g_Shown = 0xFFFF;			/* the per cent shown */
static struct DateStamp g_ReadStart;		/* when the reading began (the trace's) */
static ULONG g_ReadWork = 0;			/* ticks inside Doc_Read() (the trace's) */
static ULONG g_ReadWave = 0, g_ReadGauge = 0;	/* ticks of the per-cent shows: waveforms, gauge */
static Class *g_WaveClass = NULL;
static Object *g_WinObj = NULL;
static struct Window *g_Window = NULL;
static struct Menu *g_Strip = NULL;
static Object *g_Over, *g_Wave, *g_Scroll, *g_Len, *g_Gauge;
static Object *g_Cancel;
/* the bar under the scroller (concept Panel, V1): the deck, the Loop
   key, the length, Trim..., the lenses -- their marks drawn by the key
   glyph class, the images ours (a button never disposes its GA_Image) */
static Object *g_Deck, *g_LoopKey, *g_TrimBtn, *g_Lens[3];
static Class *g_GlyphClass;
#define GLYPHS		6		/* KG_LOOP .. KG_ALL, the two flags */
static Object *g_Glyph[GLYPHS];
/* the handle flags (concept Panel, 6): In and Out, one picked -- the
   handle last touched; their pennants in the waveform's handle pens,
   known once the gadget has laid its pens out (flag_pens) */
static Object *g_Flag[2];
static LONG g_FlagPen[2] = { -1, -1 };
static BOOL g_Pick = FALSE;		/* the flag picked: Out, else In */
static UWORD g_UpHandle = 0;		/* the handle let go last (1 In, 2 Out) and when: a double-click */
static ULONG g_UpSecs = 0, g_UpMicros = 0;
static BOOL g_DeckPaused = FALSE;	/* the deck's pause shown: a press of it differs */
static char g_LenHint[STATUS_LEN];	/* the length's bubble: the fades' lengths with it */
static BOOL g_Loop = FALSE;		/* Play > Loop? and the window's Loop: one state */
static Object *g_Root = NULL;		/* the window's layout: the strips' weights */
/* the Trim window's gadgets ghosted: no usable doc; nothing to play through */
static BOOL g_TrimOff = TRUE, g_TrimNoPlay = TRUE;
static ULONG g_StripsSum = 0;		/* the two strips' height when weighed */
static char g_TracePath[DOC_PATHLEN];		/* ENV:WaveSmithTrace: the tests' log */
static BYTE g_Sig = -1;
static ULONG g_SigMask = 0;
static BOOL g_DB = FALSE;

/* the texts the gadgets keep pointers to: static, two for the status so
   the old one is not changed under the gauge */
static char g_Title[STATUS_LEN];
static char g_Status[2][STATUS_LEN];
static UWORD g_StatusAt = 0;
static char g_InText[TIME_LEN], g_OutText[TIME_LEN], g_LenText[TIME_LEN + 72];
static CONST_STRPTR g_HandleLabels[2] = { NULL, NULL };	/* filled by hints_fill() */
static CONST_STRPTR g_LaneLabels[2] = { (CONST_STRPTR) "L", (CONST_STRPTR) "R" };
static char g_OpenPath[DOC_PATHLEN];

/* the menu table's texts: catalog ids until menu_fill(), then the words
   of the user's language (a lookup is no static initializer) */
/* the menus' places in the strip, for FULLMENUNUM() and the trace */
#define MENU_PROJECT	0
#define MENU_EDIT	1
#define MENU_EFFECTS	2
#define MENU_VIEW	3
#define MENU_PLAY	4
#define MENU_LOOPITEM	7		/* Play > Loop?, its place in the menu (bars count) */
#define MENU_SETTINGS	5
#define MENU_TEXT(id)	((STRPTR) (ULONG) ((id) + 1))
#define MENU_TEXT_MAX	2000UL		/* an id, not a string's address */

static struct NewMenu g_Menu[] = {
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_PROJECT), NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_OPEN), (STRPTR) "O", 0, 0, (APTR) MID_OPEN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_NEXT), NULL, 0, 0, (APTR) MID_NEXT },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SAVE), (STRPTR) "S", 0, 0, (APTR) MID_SAVE },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SAVEAS), (STRPTR) "A", 0, 0, (APTR) MID_SAVEAS },
	/* E4: its own command, not a cut -- two files, the doc kept */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SPLIT), NULL, NM_ITEMDISABLED, 0, (APTR) MID_SPLIT },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SPLITMARKS), NULL, NM_ITEMDISABLED, 0, (APTR) MID_SPLITMARKS },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_REVERT), NULL, 0, 0, (APTR) MID_REVERT },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_HELP), MENU_TEXT(WS_TXT_K_HELP), NM_COMMANDSTRING, 0, (APTR) MID_HELP },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ABOUT), (STRPTR) "?", 0, 0, (APTR) MID_ABOUT },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_QUIT), (STRPTR) "Q", 0, 0, (APTR) MID_QUIT },
	/* the Style Guide's Edit, between Project and View (E2: Redo on Y) */
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_EDIT), NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_UNDO), (STRPTR) "Z", NM_ITEMDISABLED, 0, (APTR) MID_UNDO },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_REDO), (STRPTR) "Y", NM_ITEMDISABLED, 0, (APTR) MID_REDO },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	/* E1: "Cut out", not Cut: nothing goes to the clipboard (yet) */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_CUTOUT), (STRPTR) "X", NM_ITEMDISABLED, 0, (APTR) MID_CUTOUT },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SELALL), NULL, NM_ITEMDISABLED, 0, (APTR) MID_SELALL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SELNONE), NULL, NM_ITEMDISABLED, 0, (APTR) MID_SELNONE },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	/* the marker keys the window takes itself, shown as text (Right-
	   Amiga M is the system's) */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ADDMARK), MENU_TEXT(WS_TXT_K_RETURN), NM_COMMANDSTRING | NM_ITEMDISABLED, 0, (APTR) MID_ADDMARK },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_DELMARK), MENU_TEXT(WS_TXT_K_DEL), NM_COMMANDSTRING | NM_ITEMDISABLED, 0, (APTR) MID_DELMARK },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_FINDTRACKS), NULL, NM_ITEMDISABLED, 0, (APTR) MID_FINDTRACKS },
	/* the handles' times, nudges, fades and Listen in a window of their
	   own (concept Panel, 11) */
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_TRIMWIN), (STRPTR) "T", 0, 0, (APTR) MID_TRIMWIN },
	/* Effects (F1, the user's name): the level of a part, concept 3.2;
	   Fade out on Right-Amiga Shift F, OS 3.2's shifted menu key */
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_EFFECTS), NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_FADEIN), (STRPTR) "F", NM_ITEMDISABLED, 0, (APTR) MID_FADEIN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_FADEOUT), (STRPTR) "F", MIF_SHIFTCOMMSEQ | NM_ITEMDISABLED, 0, (APTR) MID_FADEOUT },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_GAIN), (STRPTR) "G", NM_ITEMDISABLED, 0, (APTR) MID_GAIN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_NORMALISE), NULL, NM_ITEMDISABLED, 0, (APTR) MID_NORMALISE },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_PLAIN), NULL, NM_ITEMDISABLED, 0, (APTR) MID_PLAIN },
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_VIEW), NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ZOOMIN), (STRPTR) "+", 0, 0, (APTR) MID_ZOOMIN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ZOOMOUT), (STRPTR) "-", 0, 0, (APTR) MID_ZOOMOUT },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ALL), (STRPTR) "0", 0, 0, (APTR) MID_ALL },
	/* E5: Right-Amiga Shift Z, OS 3.2's shifted menu key (Amiga + Alt
	   is the system's mouse button: Style Guide, System keyboard shortcuts) */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ZOOMSEL), (STRPTR) "Z", MIF_SHIFTCOMMSEQ, 0, (APTR) MID_ZOOMSEL },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ATIN), NULL, 0, 0, (APTR) MID_ATIN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ATOUT), NULL, 0, 0, (APTR) MID_ATOUT },
	/* the function keys, still free (the user's): shown as text, the
	   window takes them itself */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_PREVMARK), MENU_TEXT(WS_TXT_K_F1), NM_COMMANDSTRING, 0, (APTR) MID_PREVMARK },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_NEXTMARK), MENU_TEXT(WS_TXT_K_F2), NM_COMMANDSTRING, 0, (APTR) MID_NEXTMARK },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SCALE), NULL, CHECKIT | MENUTOGGLE, 0, (APTR) MID_SCALE },
	/* Play: the keys the window takes itself, shown as text (Space has no
	   Amiga key; Right-Amiga P is the Style Guide's Print) */
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_PLAYMENU), NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_PLAY), MENU_TEXT(WS_TXT_K_SPACE), NM_COMMANDSTRING, 0, (APTR) MID_PLAY },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_PLAYSEL), MENU_TEXT(WS_TXT_K_SHIFTSPACE), NM_COMMANDSTRING, 0, (APTR) MID_PLAYSEL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_STOP), NULL, 0, 0, (APTR) MID_STOP },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_LISTENIN), NULL, 0, 0, (APTR) MID_LISTENIN },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_LISTENOUT), NULL, 0, 0, (APTR) MID_LISTENOUT },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_LOOP), NULL, CHECKIT | MENUTOGGLE, 0, (APTR) MID_LOOP },
	/* the Style Guide's Settings, second from the right; the ticks set
	   from the settings read (menu_ticks) */
	{ NM_TITLE, MENU_TEXT(WS_TXT_M_SETTINGS), NULL, 0, 0, NULL },
	/* where it plays: one of three, AHI's mode by its own requester */
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_PLAYWAY), NULL, 0, 0, NULL },
	{ NM_SUB, MENU_TEXT(WS_TXT_M_WAYAUTO), NULL, CHECKIT, 6, (APTR) MID_WAYAUTO },
	{ NM_SUB, MENU_TEXT(WS_TXT_M_WAYCARD), NULL, CHECKIT, 5, (APTR) MID_WAYCARD },
	{ NM_SUB, MENU_TEXT(WS_TXT_M_WAYAHI), NULL, CHECKIT, 3, (APTR) MID_WAYAHI },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_AHIMODE), NULL, 0, 0, (APTR) MID_AHIMODE },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SNAP), NULL, CHECKIT | MENUTOGGLE, 0, (APTR) MID_SNAP },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_MARGINS), NULL, 0, 0, (APTR) MID_MARGINS },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_BACKUP), NULL, CHECKIT | MENUTOGGLE, 0, (APTR) MID_BACKUP },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_ICONS), NULL, CHECKIT | MENUTOGGLE, 0, (APTR) MID_ICONS },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, MENU_TEXT(WS_TXT_M_SAVEPREFS), NULL, 0, 0, (APTR) MID_SAVEPREFS },
	{ NM_END, NULL, NULL, 0, 0, NULL }
};

/* the settings (editprefs.h), read before the window opens, and where
   from: Save settings writes them back there (settings.h) */
static struct EditPrefs g_Prefs;
static UWORD g_PrefsFrom = SETTINGS_NONE;
static BOOL g_SplitMoved = FALSE;	/* the weight bar dragged: its place saved */

/* the menu table's words, once, before the strip is made */
static CONST_STRPTR menu_text(CONST_STRPTR p)
{
	ULONG v = (ULONG) p;

	return (v && v <= MENU_TEXT_MAX) ? (CONST_STRPTR) GetString(&WS_LI, (LONG) (v - 1)) : p;
}

static void menu_fill(void)
{
	struct NewMenu *nm;

	for (nm = g_Menu; nm->nm_Type != NM_END; nm++) {
		if (nm->nm_Label != NM_BARLABEL) nm->nm_Label = menu_text(nm->nm_Label);
		if (nm->nm_Flags & NM_COMMANDSTRING) nm->nm_CommKey = menu_text(nm->nm_CommKey);
	}
}

/* the Settings menu's ticks as the settings are, before the strip is made */
static void menu_ticks(void)
{
	struct NewMenu *nm;
	BOOL on;

	for (nm = g_Menu; nm->nm_Type != NM_END; nm++) {
		switch ((ULONG) nm->nm_UserData) {
		case MID_SNAP: on = g_Prefs.ep_Snap; break;
		case MID_WAYAUTO: on = (BOOL) (g_Prefs.ep_Play == EDITPREFS_PLAY_AUTO); break;
		case MID_WAYCARD: on = (BOOL) (g_Prefs.ep_Play == EDITPREFS_PLAY_CARD); break;
		case MID_WAYAHI: on = (BOOL) (g_Prefs.ep_Play == EDITPREFS_PLAY_AHI); break;
		case MID_BACKUP: on = g_Prefs.ep_Backup; break;
		case MID_ICONS: on = g_Prefs.ep_Icons; break;
		case MID_LOOP: on = g_Loop; break;
		default: continue;
		}
		if (on) nm->nm_Flags |= CHECKED;
		else nm->nm_Flags &= ~CHECKED;
	}
}

/* --- small helpers ------------------------------------------------------------- */

static ULONG get(Object *o, ULONG attr)
{
	ULONG v = 0;

	GetAttr(attr, o, &v);
	return v;
}

/* a tag on a gadget in the open window */
static void set1(Object *o, ULONG tag, ULONG value)
{
	if (o && g_Window) SetGadgetAttrs((struct Gadget *) o, g_Window, NULL, tag, value, TAG_DONE);
}

/* a gadget ghosted or not; a class that leaves the repaint to the caller
   is repainted (checkbox-like classes answer 1) */
static void ghost(Object *o, BOOL off)
{
	if (o && g_Window && SetGadgetAttrs((struct Gadget *) o, g_Window, NULL, GA_Disabled, (ULONG) off, TAG_DONE))
		RefreshGList((struct Gadget *) o, g_Window, NULL, 1);
}

static void set_status(CONST_STRPTR s)
{
	g_StatusAt ^= 1;
	Text_Copy((STRPTR) g_Status[g_StatusAt], s, STATUS_LEN);
	set1(g_Gauge, GA_Text, (ULONG) g_Status[g_StatusAt]);
}

/* a status from a format: one or two texts, two numbers */
struct Say {
	CONST_STRPTR sy_Fmt;
	CONST_STRPTR sy_S0, sy_S1;
	LONG sy_N0, sy_N1;
};

static void say(const struct Say *sy)
{
	struct TextArgs ta;
	char buf[STATUS_LEN];

	ta.ta_Fmt = sy->sy_Fmt;
	ta.ta_Str[0] = sy->sy_S0;
	ta.ta_Str[1] = sy->sy_S1;
	ta.ta_Str[2] = NULL;
	ta.ta_Num[0] = sy->sy_N0;
	ta.ta_Num[1] = sy->sy_N1;
	ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) buf, STATUS_LEN, &ta);
	set_status((CONST_STRPTR) buf);
}

static void say1(CONST_STRPTR fmt, CONST_STRPTR s, LONG n)
{
	struct Say sy;

	sy.sy_Fmt = fmt;
	sy.sy_S0 = s;
	sy.sy_S1 = NULL;
	sy.sy_N0 = n;
	sy.sy_N1 = 0;
	say(&sy);
}

/* frame as m:ss.mmm into buf (TIME_LEN) */
static void time_text(ULONG frame, char *buf)
{
	struct WaveClock ck;

	ck.wc_Rate = g_Doc.d_Rate ? g_Doc.d_Rate : 44100;
	ck.wc_Origin = 0;
	ck.wc_Format = WAVEFORM_TIME_AUTO;
	ck.wc_Decimals = 3;
	WavePlan_Time(&ck, frame, buf);
}

/* the length's read-out, as wide as "00:00.000" (make_window()): an
   hour and more to the tenth, "1:02:03.4" -- its bubble says it exact */
static void readout_text(ULONG frames, char *buf)
{
	struct WaveClock ck;

	ck.wc_Rate = g_Doc.d_Rate ? g_Doc.d_Rate : 44100;
	ck.wc_Origin = 0;
	ck.wc_Format = WAVEFORM_TIME_AUTO;
	ck.wc_Decimals = frames / ck.wc_Rate >= 3600 ? 1 : 3;
	WavePlan_Time(&ck, frames, buf);
}

/* frames as seconds with a tenth: "4.1" */
static void tenths_text(ULONG frames, char *buf)
{
	ULONG t = g_Doc.d_Rate ? WavePlan_FramesToMs(frames, g_Doc.d_Rate) / 100 : 0;
	STRPTR p;

	p = Text_PutNumber((STRPTR) buf, t / 10);
	p = Text_Put(p, (CONST_STRPTR) TXT_DECIMAL);	/* the language's: "0,5" */
	Text_PutNumber(p, t % 10);
}

/* a number with its thousands apart, as the language parts them:
   "158,672", "158.672" (16 bytes: ten digits, three marks) */
static void grouped(ULONG n, char *buf)
{
	char d[12], mark = ((CONST_STRPTR) TXT_THOUSANDS)[0];
	UWORD k = 0, i = 0;

	do {
		d[k++] = (char) ('0' + n % 10);
		n /= 10;
	} while (n && k < 11);
	while (k) {
		buf[i++] = d[--k];
		if (k && k % 3 == 0 && mark) buf[i++] = mark;
	}
	buf[i] = '\0';
}

static BPTR g_TraceFH = 0;		/* the log's file, open from its first line on (shared: the tests read it) */

/* a line onto the tests' log (ENV:WaveSmithTrace names it): written at
   once, unbuffered, so it is there whatever happens next -- the file kept
   open (an open, a seek and a close a line cost an A2000 30 to 50 ms,
   inside the very steps the log times) */
static void trace(CONST_STRPTR line)
{
	if (!g_TracePath[0]) return;
	if (!g_TraceFH) {
		if (!(g_TraceFH = Open((STRPTR) g_TracePath, MODE_READWRITE))) return;
		Seek(g_TraceFH, 0, OFFSET_END);
	}
	Write(g_TraceFH, (APTR) line, (LONG) Text_Length(line));
	Write(g_TraceFH, (APTR) "\n", 1);
}

/* "<what> a b c d" onto the log */
struct TraceNums {
	CONST_STRPTR tn_What;
	ULONG tn_N[4];
	UWORD tn_Count;
};

static void trace_nums(const struct TraceNums *tn)
{
	char line[STATUS_LEN];
	STRPTR p;
	UWORD i;

	if (!g_TracePath[0]) return;
	p = Text_Put((STRPTR) line, tn->tn_What);
	for (i = 0; i < tn->tn_Count && i < 4; i++) {
		p = Text_Put(p, (CONST_STRPTR) " ");
		p = Text_PutNumber(p, tn->tn_N[i]);
	}
	trace((CONST_STRPTR) line);
}

/* a gadget's box in the window onto the log */
static void trace_box(CONST_STRPTR what, Object *o)
{
	struct TraceNums tn;
	struct Gadget *g = (struct Gadget *) o;

	if (!o) return;
	tn.tn_What = what;
	tn.tn_N[0] = (ULONG) g->LeftEdge;
	tn.tn_N[1] = (ULONG) g->TopEdge;
	tn.tn_N[2] = (ULONG) g->Width;
	tn.tn_N[3] = (ULONG) g->Height;
	tn.tn_Count = 4;
	trace_nums(&tn);
}

/* where a menu's title and one of its items sit on the screen, for the
   tests' pointer (a bar counts as an item) */
struct MenuSpot {
	CONST_STRPTR ms_What;		/* the trace line's words */
	UWORD ms_Menu, ms_Item;
	UWORD ms_Sub;			/* a sub-item's, or MENU_NOSUB */
	CONST_STRPTR ms_SubY;		/* the sub-item's y: its own trace line */
};
#define MENU_NOSUB	0xFFFF

static void trace_menu_item(const struct MenuSpot *ms)
{
	struct Menu *m = g_Strip;
	struct MenuItem *it;
	struct TraceNums tn;
	UWORD i;

	for (i = 0; m && i < ms->ms_Menu; i++) m = m->NextMenu;
	if (!m) return;
	for (it = m->FirstItem, i = 0; it && i < ms->ms_Item; i++) it = it->NextItem;
	if (!it) return;
	tn.tn_What = ms->ms_What;
	tn.tn_N[0] = (ULONG) (m->LeftEdge + 4);
	tn.tn_N[1] = (ULONG) (m->LeftEdge + it->LeftEdge + it->Width / 2);
	tn.tn_N[2] = (ULONG) (g_Window->WScreen->BarHeight + 1 + it->TopEdge + it->Height / 2);
	if (ms->ms_Sub != MENU_NOSUB) {
		/* a sub-item: its box from its item's */
		struct MenuItem *sub = it->SubItem;

		for (i = 0; sub && i < ms->ms_Sub; i++) sub = sub->NextItem;
		if (!sub) return;
		tn.tn_N[3] = (ULONG) (m->LeftEdge + it->LeftEdge + sub->LeftEdge + sub->Width / 2);
		tn.tn_N[2] = (ULONG) (g_Window->WScreen->BarHeight + 1 + it->TopEdge + sub->TopEdge + sub->Height / 2);
		tn.tn_N[1] = (ULONG) (m->LeftEdge + it->LeftEdge + it->Width / 2);
		/* title x, item x, item y (the item's own), sub x, sub y */
		{
			struct TraceNums t2;

			t2.tn_What = ms->ms_What;
			t2.tn_N[0] = (ULONG) (m->LeftEdge + 4);
			t2.tn_N[1] = tn.tn_N[1];
			t2.tn_N[2] = (ULONG) (g_Window->WScreen->BarHeight + 1 + it->TopEdge + it->Height / 2);
			t2.tn_N[3] = tn.tn_N[3];
			t2.tn_Count = 4;
			trace_nums(&t2);
			t2.tn_What = ms->ms_SubY;
			t2.tn_N[0] = tn.tn_N[2];
			t2.tn_Count = 1;
			trace_nums(&t2);
		}
		return;
	}
	tn.tn_Count = 3;
	trace_nums(&tn);
}

static void trace_layout(void)
{
	struct TraceNums tn;

	if (!g_TracePath[0] || !g_Window) return;
	{
		struct MenuSpot ms;

		ms.ms_Sub = MENU_NOSUB;
		ms.ms_What = (CONST_STRPTR) "menuitem title x y";	/* View, Zoom to selection */
		ms.ms_Menu = MENU_VIEW;
		ms.ms_Item = 3;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "splititem title x y";	/* Project, Split */
		ms.ms_Menu = MENU_PROJECT;
		ms.ms_Item = 4;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "splitmarksitem title x y";	/* Project, Split at markers */
		ms.ms_Item = 5;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "helpitem title x y";	/* Project, Help... */
		ms.ms_Item = 8;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "aboutitem title x y";	/* Project, About... */
		ms.ms_Item = 9;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "findtracksitem title x y";	/* Edit, Find tracks */
		ms.ms_Menu = MENU_EDIT;
		ms.ms_Item = 10;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "trimwinitem title x y";	/* Edit, Trim... */
		ms.ms_Item = 12;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "atinitem title x y";	/* View, At In (the tests: no button now) */
		ms.ms_Menu = MENU_VIEW;
		ms.ms_Item = 5;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "fadeinitem title x y";	/* Effects, Fade in */
		ms.ms_Menu = MENU_EFFECTS;
		ms.ms_Item = 0;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "fadeoutitem title x y";	/* Effects, Fade out */
		ms.ms_Item = 1;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "gainitem title x y";	/* Effects, Gain... */
		ms.ms_Item = 3;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "normaliseitem title x y";	/* Effects, Normalise */
		ms.ms_Item = 4;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "plainitem title x y";	/* Effects, Remove fades and gain */
		ms.ms_Item = 6;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "playselitem title x y";	/* Play, Play selection */
		ms.ms_Menu = MENU_PLAY;
		ms.ms_Item = 1;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "loopitem title x y";	/* Play, Loop? */
		ms.ms_Item = 7;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "snapitem title x y";	/* Settings, Snap ... */
		ms.ms_Menu = MENU_SETTINGS;
		ms.ms_Item = 3;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "marginsitem title x y";	/* Settings, Lead and tail... */
		ms.ms_Item = 4;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "backupitem title x y";	/* Settings, Keep a backup? */
		ms.ms_Item = 6;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "saveprefsitem title x y";	/* Settings, Save settings */
		ms.ms_Item = 9;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "ahimodeitem title x y";	/* Settings, AHI mode... */
		ms.ms_Item = 1;
		trace_menu_item(&ms);
		/* Settings, Play through, Automatic and AHI: title x, item x, item y, sub x
		   (the sub's y its own line: wayautoy, wayahiy) */
		ms.ms_What = (CONST_STRPTR) "wayautoitem title x y sub";
		ms.ms_SubY = (CONST_STRPTR) "wayautoy";
		ms.ms_Item = 0;
		ms.ms_Sub = 0;
		trace_menu_item(&ms);
		ms.ms_What = (CONST_STRPTR) "wayahiitem title x y sub";
		ms.ms_SubY = (CONST_STRPTR) "wayahiy";
		ms.ms_Sub = 2;
		trace_menu_item(&ms);
		ms.ms_Sub = MENU_NOSUB;
	}
	tn.tn_What = (CONST_STRPTR) "window";
	tn.tn_N[0] = (ULONG) g_Window->LeftEdge;
	tn.tn_N[1] = (ULONG) g_Window->TopEdge;
	tn.tn_N[2] = (ULONG) g_Window->Width;
	tn.tn_N[3] = (ULONG) g_Window->Height;
	tn.tn_Count = 4;
	trace_nums(&tn);
	/* the refresh window.class chose (the user's ReAction settings): a
	   smart window's covered parts come back from its layer, a simple
	   one's are drawn again */
	tn.tn_What = (CONST_STRPTR) "refresh simple smart nocare";
	tn.tn_N[0] = (ULONG) ((g_Window->Flags & WFLG_REFRESHBITS) == WFLG_SIMPLE_REFRESH);
	tn.tn_N[1] = (ULONG) ((g_Window->Flags & WFLG_REFRESHBITS) == WFLG_SMART_REFRESH);
	tn.tn_N[2] = (ULONG) ((g_Window->Flags & WFLG_NOCAREREFRESH) != 0);
	tn.tn_Count = 3;
	trace_nums(&tn);
	trace_box((CONST_STRPTR) "overview", g_Over);
	trace_box((CONST_STRPTR) "wave", g_Wave);
	trace_box((CONST_STRPTR) "scroller", g_Scroll);
	trace_box((CONST_STRPTR) "deck", g_Deck);
	trace_box((CONST_STRPTR) "loopkey", g_LoopKey);
	trace_box((CONST_STRPTR) "flagin", g_Flag[0]);
	trace_box((CONST_STRPTR) "flagout", g_Flag[1]);
	trace_box((CONST_STRPTR) "len", g_Len);
	trace_box((CONST_STRPTR) "trimbutton", g_TrimBtn);
	trace_box((CONST_STRPTR) "cancelbutton", g_Cancel);
	trace_box((CONST_STRPTR) "lensout", g_Lens[0]);
	trace_box((CONST_STRPTR) "lensin", g_Lens[1]);
	trace_box((CONST_STRPTR) "lensall", g_Lens[2]);
}

/* the overview's and the waveform's weights as their heights share the
   room out, per cent (layout.gadget takes no more as a tag). The weight
   bar between them gives the overview its new weight from its height and
   weight at the press (new height x weight / height): an overview held at
   its least height while its weight is worth less moves only a pixel or
   so, however far the bar is dragged -- the window's first layout at
   PAL's height does that, and so can a window made smaller */
static void weigh_strips(void)
{
	struct Gadget *o = (struct Gadget *) g_Over, *w = (struct Gadget *) g_Wave;
	ULONG sum, pc;

	if (!g_Root || !g_Window || o->Height <= 0 || w->Height <= 0) return;
	sum = (ULONG) o->Height + (ULONG) w->Height;
	pc = ((ULONG) o->Height * 100 + sum / 2) / sum;
	if (pc < 1) pc = 1;
	if (pc > 99) pc = 99;
	SetGadgetAttrs((struct Gadget *) g_Root, g_Window, NULL,
		       LAYOUT_ModifyChild, (ULONG) g_Over, CHILD_WeightedHeight, pc,
		       LAYOUT_ModifyChild, (ULONG) g_Wave, CHILD_WeightedHeight, 100 - pc, TAG_DONE);
	g_StripsSum = sum;
}

/* every tick: the strips weighed anew when the window's size changed
   their room (a drag of the bar keeps it, and its weights fit); the
   overview and the waveform onto the trace when their heights changed */
static void strips_tick(void)
{
	static WORD traced = -1;
	struct Gadget *o = (struct Gadget *) g_Over, *w = (struct Gadget *) g_Wave;

	if (!o || !w) return;
	if ((ULONG) o->Height + (ULONG) w->Height != g_StripsSum) weigh_strips();
	else if (traced >= 0 && o->Height != traced) g_SplitMoved = TRUE;
	if (o->Height == traced) return;
	if (!g_TracePath[0]) {
		traced = o->Height;
		return;
	}
	if (traced >= 0) {
		trace_box((CONST_STRPTR) "overview", g_Over);
		trace_box((CONST_STRPTR) "wave", g_Wave);
	}
	traced = o->Height;
}

/* the handles and the view onto the log */
static void trace_trim(CONST_STRPTR what)
{
	struct TraceNums tn;

	if (!g_TracePath[0] || !g_Wave) return;
	tn.tn_What = what;
	tn.tn_N[0] = get(g_Wave, WAVEFORM_TrimIn);
	tn.tn_N[1] = get(g_Wave, WAVEFORM_TrimOut);
	tn.tn_N[2] = get(g_Wave, WAVEFORM_Top);
	tn.tn_N[3] = get(g_Wave, WAVEFORM_Zoom);
	tn.tn_Count = 4;
	trace_nums(&tn);
}

/* the file's name (the user's, not a work file's), its directory left off */
static CONST_STRPTR file_name(void)
{
	return (CONST_STRPTR) FilePart((STRPTR) g_Source);
}

/* --- the trim fields and the views ------------------------------------------------ */

/* a fade's length as the Trim window shows it: "2.0 s", or none */
static void fade_text(ULONG frames, char *buf)
{
	if (!frames) {
		Text_Copy((STRPTR) buf, (CONST_STRPTR) TXT_FADE_NONE, FADE_TEXT);
		return;
	}
	tenths_text(frames, buf);
	Text_Put((STRPTR) buf + Text_Length((CONST_STRPTR) buf), (CONST_STRPTR) " s");
}

/* the Trim window told what the handles say -- a closed one shows it when
   it opens: the times, the fades' lengths, ghosted without a doc to trim
   or a way to play */
static void trim_shown(void)
{
	struct TrimShow ts;
	char fin[FADE_TEXT], fout[FADE_TEXT];

	fin[0] = fout[0] = '\0';
	if (g_HaveDoc) {
		fade_text(g_Doc.d_Levels.lv_FadeIn, fin);
		fade_text(g_Doc.d_Levels.lv_FadeOut, fout);
	}
	ts.ts_Time[0] = (CONST_STRPTR) g_InText;
	ts.ts_Time[1] = (CONST_STRPTR) g_OutText;
	ts.ts_Fade[0] = (CONST_STRPTR) fin;
	ts.ts_Fade[1] = (CONST_STRPTR) fout;
	ts.ts_NoDoc = g_TrimOff;
	ts.ts_NoListen = g_TrimNoPlay;
	TrimWin_Show(&ts);
}

static void show_trim(void)
{
	ULONG in, out;
	char len[TIME_LEN], rd[TIME_LEN], fin[12], fout[12];
	struct TextArgs ta;

	if (!g_HaveDoc) return;
	in = get(g_Wave, WAVEFORM_TrimIn);
	out = get(g_Wave, WAVEFORM_TrimOut);
	if (in == WAVEFORM_NONE) in = 0;
	if (out == WAVEFORM_NONE) out = Doc_Length(&g_Doc);
	time_text(in, g_InText);
	time_text(out, g_OutText);
	readout_text(out > in ? out - in : 0, rd);
	time_text(out > in ? out - in : 0, len);
	/* the end fades go with the handles: their lengths said beside */
	levels_now();
	tenths_text(g_Doc.d_Levels.lv_FadeIn, fin);
	tenths_text(g_Doc.d_Levels.lv_FadeOut, fout);
	ta.ta_Fmt = (CONST_STRPTR) TXT_LENGTH;
	ta.ta_Str[0] = (CONST_STRPTR) len;
	ta.ta_Str[1] = ta.ta_Str[2] = NULL;
	if (g_Doc.d_Levels.lv_FadeIn && g_Doc.d_Levels.lv_FadeOut) {
		ta.ta_Fmt = (CONST_STRPTR) TXT_LENGTH_FADES;
		ta.ta_Str[1] = (CONST_STRPTR) fin;
		ta.ta_Str[2] = (CONST_STRPTR) fout;
	} else if (g_Doc.d_Levels.lv_FadeIn || g_Doc.d_Levels.lv_FadeOut) {
		ta.ta_Fmt = (CONST_STRPTR) (g_Doc.d_Levels.lv_FadeIn ? TXT_LENGTH_FADEIN : TXT_LENGTH_FADEOUT);
		ta.ta_Str[1] = (CONST_STRPTR) (g_Doc.d_Levels.lv_FadeIn ? fin : fout);
	}
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	/* the read-out says the time; its bubble the words and the fades
	   (concept Panel: the fades are drawn, the status line says each) */
	Text_Fmt((STRPTR) g_LenHint, sizeof(g_LenHint), &ta);
	Text_Copy((STRPTR) g_LenText, (CONST_STRPTR) rd, sizeof(g_LenText));
	set1(g_Len, GA_Text, (ULONG) g_LenText);
	trim_shown();
	/* the overview's handles follow */
	if (g_Over && g_Window)
		SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_TrimIn, in, WAVEFORM_TrimOut, out, TAG_DONE);
}

/* the overview's box and the scroller after the waveform's view */
static void sync_views(void)
{
	ULONG top, vis, zoom, total;

	if (!g_HaveDoc || !g_Window) return;
	top = get(g_Wave, WAVEFORM_Top);
	vis = get(g_Wave, WAVEFORM_Visible);
	zoom = get(g_Wave, WAVEFORM_Zoom);
	if (!zoom) zoom = 1;
	SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_BoxTop, top, WAVEFORM_BoxVisible, vis, TAG_DONE);
	/* the scroller in columns of the zoom */
	total = Doc_Length(&g_Doc) / zoom + 1;
	SetGadgetAttrs((struct Gadget *) g_Scroll, g_Window, NULL, SCROLLER_Total, total,
		       SCROLLER_Visible, vis / zoom, SCROLLER_Top, top / zoom, TAG_DONE);
}

static void view_top(ULONG top)
{
	set1(g_Wave, WAVEFORM_Top, top);
	sync_views();
}

/* --- the bubbles ---------------------------------------------------------------------- */

/* window.class shows a gadget's hint from this array, and builds its hints
   only when it has one. The two waveforms' entries are the class's buffer
   (their bubble names what is under the pointer, through GM_HELPTEST and
   GM_QUERYHOVERED; an older window.class reads it from here): filled in
   at run time, a function's result being no static initialiser. */
static const struct {
	UWORD hf_ID;
	LONG hf_Msg;			/* the catalog's id */
} g_HintTexts[] = {
	{ GID_DECK, WS_TXT_HINT_DECK }, { GID_LOOP, WS_TXT_HINT_LOOP }, { GID_TRIMWIN, WS_TXT_HINT_TRIMWIN },
	{ GID_CANCEL, WS_TXT_HINT_CANCEL }, { GID_ZOOMIN, WS_TXT_HINT_ZOOMIN }, { GID_ZOOMOUT, WS_TXT_HINT_ZOOMOUT },
	{ GID_ALL, WS_TXT_HINT_ALL }, { GID_FLAGIN, WS_TXT_HINT_FLAGIN }, { GID_FLAGOUT, WS_TXT_HINT_FLAGOUT }
};
#define HINT_TEXTS (sizeof(g_HintTexts) / sizeof(g_HintTexts[0]))
static struct HintInfo g_Hints[HINT_TEXTS + 4];	/* the waveforms, the length, the end */
static char g_HelpShown[WAVEFORM_HELPLEN];	/* the trace's: the bubble text last written */

static void hint(UWORD n, WORD id, CONST_STRPTR text)
{
	g_Hints[n].hi_GadgetID = id;
	g_Hints[n].hi_Code = -1;
	g_Hints[n].hi_Text = (STRPTR) text;
	g_Hints[n].hi_Flags = 0;
}

static void hints_fill(void)
{
	UWORD i;

	hint(0, GID_WAVE, Waveform_HelpText());
	hint(1, GID_OVER, Waveform_HelpText());
	for (i = 0; i < HINT_TEXTS; i++)
		hint((UWORD) (i + 2), (WORD) g_HintTexts[i].hf_ID, (CONST_STRPTR) GetString(&WS_LI, g_HintTexts[i].hf_Msg));
	/* the handles' labels, the user's words */
	g_HandleLabels[0] = (CONST_STRPTR) TXT_IN;
	g_HandleLabels[1] = (CONST_STRPTR) TXT_OUT;
	/* the length's bubble: its words and the fades', changed as they do */
	hint((UWORD) (HINT_TEXTS + 2), GID_LEN, (CONST_STRPTR) g_LenHint);
	hint((UWORD) (HINT_TEXTS + 3), -1, NULL);
}

/* the waveforms' bubble text into the tests' trace when it changed (read
   on the window's ticks: the class writes it on Intuition's) */
/* the waveforms' paint log onto the trace (Waveform_DiagOn, with the
   trace on): "wdiag" the waveform, "odiag" the overview; a line an event:
   seq kind other full first last drawn layerflags hidden damagex0 x1 */
static WS_FAR struct WaveDiagEvent g_DiagEv[32];
static ULONG g_DiagAfter[2] = { 0, 0 };

static STRPTR put_signed(STRPTR p, LONG v)
{
	if (v < 0) {
		*p++ = '-';
		v = -v;
	}
	return Text_PutNumber(p, (ULONG) v);
}

static void trace_diag(void)
{
	struct WaveDiagRead rd;
	const struct WaveDiagEvent *e;
	Object *o;
	char line[STATUS_LEN];
	STRPTR p;
	UWORD i, k, n;

	if (!g_TracePath[0]) return;
	for (k = 0; k < 2; k++) {
		o = k ? g_Over : g_Wave;
		do {
			rd.wdr_After = g_DiagAfter[k];
			rd.wdr_Out = g_DiagEv;
			rd.wdr_Max = 32;
			n = Waveform_DiagRead(o, &rd);
			g_DiagAfter[k] = rd.wdr_After;
			if (rd.wdr_Lost) {
				p = Text_Put((STRPTR) line, (CONST_STRPTR) (k ? "odiaglost " : "wdiaglost "));
				Text_PutNumber(p, rd.wdr_Lost);
				trace((CONST_STRPTR) line);
			}
			for (i = 0; i < n; i++) {
				e = &g_DiagEv[i];
				p = Text_Put((STRPTR) line, (CONST_STRPTR) (k ? "odiag " : "wdiag "));
				p = Text_Put(Text_PutNumber(p, e->wde_Seq), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_Kind), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_Other), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_Full), (CONST_STRPTR) " ");
				p = Text_Put(put_signed(p, e->wde_First), (CONST_STRPTR) " ");
				p = Text_Put(put_signed(p, e->wde_Last), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_Drawn), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_LayerFlags), (CONST_STRPTR) " ");
				p = Text_Put(Text_PutNumber(p, e->wde_Hidden), (CONST_STRPTR) " ");
				p = Text_Put(put_signed(p, e->wde_DamageX0), (CONST_STRPTR) " ");
				put_signed(p, e->wde_DamageX1);
				trace((CONST_STRPTR) line);
			}
		} while (n == 32);
	}
}

/* the window's own events onto the trace, beside the paints: when it was
   activated, put behind or before, moved, sized */
static void trace_win(CONST_STRPTR what)
{
	struct TraceNums tn;

	if (!g_TracePath[0] || !g_Window) return;
	tn.tn_What = what;
	tn.tn_N[0] = (ULONG) g_Window->LeftEdge;
	tn.tn_N[1] = (ULONG) g_Window->TopEdge;
	tn.tn_N[2] = (ULONG) g_Window->Width;
	tn.tn_N[3] = (ULONG) g_Window->Height;
	tn.tn_Count = 4;
	trace_nums(&tn);
}

static void trace_help(void)
{
	CONST_STRPTR t = Waveform_HelpText();
	char line[WAVEFORM_HELPLEN + 8];
	UWORD k;
	BOOL same = TRUE;

	if (!g_TracePath[0]) return;
	for (k = 0; k < WAVEFORM_HELPLEN; k++) {
		if (t[k] != g_HelpShown[k]) same = FALSE;
		g_HelpShown[k] = t[k];
		if (!t[k]) break;
	}
	if (same) return;
	Text_Put(Text_Put((STRPTR) line, (CONST_STRPTR) "help "), g_HelpShown[0] ? (CONST_STRPTR) g_HelpShown : (CONST_STRPTR) "-");
	trace((CONST_STRPTR) line);
}

/* --- the mouse wheel ------------------------------------------------------------------ */

/* Intuition V47 sends a turn of the wheel to the window under the pointer
   as IDCMP_EXTENDEDMOUSE with an IntuiWheelData, once the gadget under it
   has passed (gadgetclass answers GM_HANDLESCROLL with GMR_REUSE, layout
   asks the child under the pointer; the scroller keeps its own).
   window.class has no WMHI code for it: its IDCMP hook only counts the
   notches, and the loop spends them once the input is read -- a spin is
   several messages and one repaint. Over the waveform or the overview:
   the wheel zooms where the pointer is (away from the user in), Shift or
   the tilt scrolls a tenth of the view a notch. */
static struct Hook g_WheelHook;

/* the window an AppWindow: a file dropped on it is opened (the first) */
static struct MsgPort *g_AppPort = NULL;
static struct Hook g_AppHook;
static char g_DropPath[DOC_PATHLEN];
static BOOL g_Dropped = FALSE;

/* window.class's call for an AppMessage (from WM_HANDLEINPUT): the path
   kept for the loop, which asks before the doc goes; a drawer (no name)
   is no file */
static DISP_QUAL ULONG app_hook(REG(a0, struct Hook *h), REG(a2, Object *win), REG(a1, struct AppMessage *am))
{
	struct WBArg *wa;

	(void) h;
	(void) win;
	if (!am || am->am_NumArgs < 1) return 0;
	wa = am->am_ArgList;
	if (!wa->wa_Name || !wa->wa_Name[0]) return 0;
	if (!NameFromLock(wa->wa_Lock, (STRPTR) g_DropPath, DOC_PATHLEN) || !AddPart((STRPTR) g_DropPath, wa->wa_Name, DOC_PATHLEN))
		return 0;
	g_Dropped = TRUE;
	return 0;
}
static LONG g_WheelZoom = 0, g_WheelScroll = 0;	/* notches, + toward the user */
static Object *g_WheelOn = NULL;		/* the gadget the wheel turned over */
static WORD g_WheelX = 0;			/* the pointer: that gadget's column */

static BOOL pointer_over(Object *o, const struct IntuiMessage *im)
{
	struct Gadget *g = (struct Gadget *) o;

	return (BOOL) (g && im->MouseX >= g->LeftEdge && im->MouseX < g->LeftEdge + g->Width &&
		       im->MouseY >= g->TopEdge && im->MouseY < g->TopEdge + g->Height);
}

static DISP_QUAL ULONG wheel_hook(REG(a0, struct Hook *h), REG(a2, Object *win), REG(a1, struct IntuiMessage *im))
{
	struct IntuiWheelData *wd;
	Object *on;

	(void) h;
	(void) win;
	/* the Code says which structure IAddress is */
	if (!im || im->Class != IDCMP_EXTENDEDMOUSE || !(im->Code & IMSGCODE_INTUIWHEELDATA) ||
	    !(wd = (struct IntuiWheelData *) im->IAddress))
		return 0;
	if (pointer_over(g_Wave, im)) on = g_Wave;
	else if (pointer_over(g_Over, im)) on = g_Over;
	else return 0;
	g_WheelOn = on;
	g_WheelX = (WORD) (im->MouseX - ((struct Gadget *) on)->LeftEdge);
	g_WheelScroll += wd->WheelX;
	if (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) g_WheelScroll += wd->WheelY;
	else g_WheelZoom += wd->WheelY;
	return 0;
}

/* the notches the input brought, spent at once */
static void wheel_spend(void)
{
	LONG z = g_WheelZoom, sc = g_WheelScroll, t;
	ULONG centre, vis, step;

	g_WheelZoom = g_WheelScroll = 0;
	if (!g_HaveDoc || !g_WheelOn || (!z && !sc)) return;
	if (z) {
		/* the frame under the pointer stays under it; over the overview
		   the view goes there first */
		centre = Waveform_FrameAt(g_WheelOn, g_WheelX);
		vis = get(g_Wave, WAVEFORM_Visible);
		if (g_WheelOn == g_Over)
			set1(g_Wave, WAVEFORM_Top, centre > vis / 2 ? centre - vis / 2 : 0);
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_ZoomCenter, centre,
			       WAVEFORM_ZoomStep, (ULONG) -z, TAG_DONE);
	}
	if (sc) {
		vis = get(g_Wave, WAVEFORM_Visible);
		step = vis / 10 ? vis / 10 : 1;
		t = (LONG) get(g_Wave, WAVEFORM_Top) + sc * (LONG) step;
		set1(g_Wave, WAVEFORM_Top, (ULONG) (t < 0 ? 0 : t));
	}
	sync_views();
	trace_trim((CONST_STRPTR) "wheel in out top zoom");
}

/* show 1.5 s either side of a frame */
/* View: Zoom to selection -- the part marked by a drag fills the view,
   a twentieth of it to spare on each side */
static void zoom_selection(void)
{
	struct WaveRange r;
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd), t;

	if (a == WAVEFORM_NONE || b == WAVEFORM_NONE || a == b) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	r.wr_First = a;
	r.wr_End = b;
	r.wr_Margin = 5;
	r.wr_Pad = 0;
	set1(g_Wave, WAVEFORM_ShowRange, (ULONG) &r);
	sync_views();
	trace_trim((CONST_STRPTR) "view in out top zoom");
}

static void show_around(ULONG frame)
{
	struct WaveRange r;
	ULONG half = WavePlan_MsToFrames(ATRANGE_MS, g_Doc.d_Rate);

	r.wr_First = frame > half ? frame - half : 0;
	r.wr_End = frame + half;
	r.wr_Margin = 0;
	r.wr_Pad = 0;
	set1(g_Wave, WAVEFORM_ShowRange, (ULONG) &r);
	sync_views();
}

/* a handle to frame f, kept clear of the other one: FALSE when it has
   none */
static BOOL handle_to(BOOL out, ULONG f)
{
	ULONG at = get(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn), other = get(g_Wave, out ? WAVEFORM_TrimIn : WAVEFORM_TrimOut);

	if (at == WAVEFORM_NONE) return FALSE;
	if (f > Doc_Length(&g_Doc)) f = Doc_Length(&g_Doc);
	/* In before Out, also at the very ends (an empty timeline: both 0) */
	if (out && other != WAVEFORM_NONE && f <= other) f = other < Doc_Length(&g_Doc) ? other + 1 : Doc_Length(&g_Doc);
	if (!out && other != WAVEFORM_NONE && f >= other) f = other ? other - 1 : 0;
	set1(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn, f);
	show_trim();
	return TRUE;
}

/* a handle moved by so many frames (the nudge) */
static void nudge(BOOL out, LONG by)
{
	ULONG f = get(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn);

	if (f == WAVEFORM_NONE) return;
	if (by < 0) f = f > (ULONG) -by ? f - (ULONG) -by : 0;
	else f = f + (ULONG) by;
	if (handle_to(out, f)) changed();
}

static LONG nudge_step(void)
{
	ULONG z = get(g_Wave, WAVEFORM_Zoom);

	return (LONG) (z ? z : 1);
}

/* frame on the nearest zero crossing within a column of the view (at
   least SNAP_MIN frames), as Settings, Snap to zero crossings? says;
   frame itself when it says not to */
static ULONG snapped(ULONG frame)
{
	ULONG z = get(g_Wave, WAVEFORM_Zoom);

	if (!g_Prefs.ep_Snap) return frame;
	return Doc_ZeroCross(&g_Doc, frame, z > SNAP_MIN ? z : SNAP_MIN);
}

/* a handle let go: on the nearest zero crossing within its column */
static void snap_handle(BOOL out)
{
	ULONG f = get(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn), s, other;
	char t[TIME_LEN], n[16];
	struct Say sy;

	if (f == WAVEFORM_NONE || f == 0 || f >= Doc_Length(&g_Doc)) {
		show_trim();
		return;
	}
	s = snapped(f);
	/* never onto or past the other handle (the crossing may lie beyond it) */
	other = get(g_Wave, out ? WAVEFORM_TrimIn : WAVEFORM_TrimOut);
	if (other != WAVEFORM_NONE && (out ? s <= other : s >= other)) s = f;
	if (s != f) set1(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn, s);
	{
		struct TraceNums tn;

		tn.tn_What = (CONST_STRPTR) "snap from to";
		tn.tn_N[0] = f;
		tn.tn_N[1] = s;
		tn.tn_Count = 2;
		trace_nums(&tn);
	}
	show_trim();
	trace_trim((CONST_STRPTR) (out ? "snapped-out in out top zoom" : "snapped-in in out top zoom"));
	time_text(s, t);
	grouped(s, n);
	if (g_Prefs.ep_Snap) sy.sy_Fmt = (CONST_STRPTR) (out ? TXT_ST_SNAPPED_OUT : TXT_ST_SNAPPED_IN);
	else sy.sy_Fmt = (CONST_STRPTR) (out ? TXT_ST_PUT_OUT : TXT_ST_PUT_IN);
	sy.sy_S0 = (CONST_STRPTR) t;
	sy.sy_S1 = (CONST_STRPTR) n;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

/* a time typed into In or Out */
static void typed(BOOL out)
{
	struct WaveClock ck;
	ULONG f, other = get(g_Wave, out ? WAVEFORM_TrimIn : WAVEFORM_TrimOut);
	CONST_STRPTR s = TrimWin_Text(out);

	/* before the suggestion there is nothing to move; and Tab or Return
	   on the time as shown is no change -- parsed back it would lose the
	   frames under a millisecond, off the zero crossing */
	if (!doc_usable()) {
		show_trim();
		return;
	}
	if (s && Text_Same(s, (CONST_STRPTR) (out ? g_OutText : g_InText))) return;
	ck.wc_Rate = g_Doc.d_Rate;
	ck.wc_Origin = 0;
	ck.wc_Format = WAVEFORM_TIME_AUTO;
	ck.wc_Decimals = 3;
	if (!s || (f = WavePlan_ParseTime(&ck, s)) == WAVEFORM_NONE) {
		set_status((CONST_STRPTR) TXT_ST_BADTIME);
		show_trim();
		return;
	}
	if (f > Doc_Length(&g_Doc)) f = Doc_Length(&g_Doc);
	if (out && other != WAVEFORM_NONE && f <= other) f = other + 1;
	if (!out && other != WAVEFORM_NONE && f >= other) f = other ? other - 1 : 0;
	set1(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn, f);
	show_trim();
	changed();
}

/* --- a job's progress: reading, decoding, saving ------------------------------------ */

#define LEFT_LEN	48		/* TXT_LEFT_PRE (16 at most) + the time + TXT_LEFT_POST (13) */
static ULONG g_JobFrom = 0;		/* Clock_Ms() at the job's start */
static BOOL g_LeftSaid = FALSE;		/* the trace has the job's first guess */
static LONG g_SlowSave = 0;		/* ENV:WaveSmithSlowSave: ticks a per cent (the tests' Cancel) */
static BOOL g_CancelSave = FALSE;	/* Cancel (or Esc) pressed while a save copies */

/* a job begun (Cancel lit, its clock started) or none running any more */
static void job_on(BOOL on)
{
	if (on) {
		g_JobFrom = g_HaveClock ? Clock_Ms() : 0;
		g_LeftSaid = FALSE;
	}
	if (g_Window && g_Cancel) ghost(g_Cancel, (BOOL) !on);
}

/* ", about 1:20 left" from the time the job took for pc per cent, into
   buf (LEFT_LEN); nothing before 3 % and 2 s, when the guess would jump */
static void time_left(UWORD pc, char *buf)
{
	struct TraceNums tn;
	ULONG ms, s;
	STRPTR p;

	buf[0] = '\0';
	if (!g_HaveClock || pc < 3 || pc >= 100 || (ms = Clock_Ms() - g_JobFrom) < 2000) return;
	s = (ms / pc * (100 - pc) + 999) / 1000;
	p = Text_Put((STRPTR) buf, (CONST_STRPTR) TXT_LEFT_PRE);
	p = Text_PutNumber(p, s / 60);
	p = Text_Put(p, (CONST_STRPTR) (s % 60 < 10 ? ":0" : ":"));
	p = Text_PutNumber(p, s % 60);
	Text_Put(p, (CONST_STRPTR) TXT_LEFT_POST);
	if (g_LeftSaid) return;
	g_LeftSaid = TRUE;
	tn.tn_What = (CONST_STRPTR) "timeleft s";
	tn.tn_N[0] = s;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the file a job writes when it is not the one open: a split's part
   (write_part()); NULL the rest of the time */
static CONST_STRPTR g_JobName = NULL;

/* a job's line: "Reading Side.wav: 42 %, about 1:20 left" -- a split's
   "Saving Side-01.wav: ..." */
static void job_said(CONST_STRPTR fmt, UWORD pc)
{
	char left[LEFT_LEN];
	struct Say sy;

	time_left(pc, left);
	sy.sy_Fmt = fmt;
	sy.sy_S0 = g_JobName ? g_JobName : file_name();
	sy.sy_S1 = (CONST_STRPTR) left;
	sy.sy_N0 = (LONG) pc;
	sy.sy_N1 = 0;
	say(&sy);
}

/* --- the file ----------------------------------------------------------------------- */

/* the doc can be played and saved: read, or its suggestion shown (a long
   file's ends are read first, doc.h) */
static BOOL doc_usable(void)
{
	return (BOOL) (g_HaveDoc && (!g_Reading || g_PlanShown));
}

static void ghost_file(BOOL off)
{
	BOOL noplay = (BOOL) (off || !Play_There());

	ghost(g_Deck, noplay);
	ghost(g_LoopKey, noplay);
	g_TrimOff = off;
	g_TrimNoPlay = noplay;
	trim_shown();
	if (g_Strip) {
		/* Save and Save as; the Play menu as its buttons */
		menu_on(FULLMENUNUM(MENU_PROJECT, 2, NOSUB), (BOOL) !off);
		menu_on(FULLMENUNUM(MENU_PROJECT, 3, NOSUB), (BOOL) !off);
		menu_on(FULLMENUNUM(MENU_PLAY, NOITEM, NOSUB), (BOOL) !noplay);
	}
	edit_menu();
	if (g_Strip && g_Window) menu_on(FULLMENUNUM(MENU_PROJECT, 4, NOSUB), (BOOL) (!off && !g_Reading));
}

/* the play head off both views */
static void head_off(void)
{
	set1(g_Wave, WAVEFORM_Position, WAVEFORM_NONE);
	set1(g_Over, WAVEFORM_Position, WAVEFORM_NONE);
	trace((CONST_STRPTR) "head off");
}

static BOOL g_PlaySaid;
static ULONG g_Due = 0;			/* the beat's timetable: the next, in Clock_Ms() */
static ULONG g_Beats = 0, g_BeatsFrom = 0;	/* beats since the play began, when (the trace) */
static ULONG g_Looked = 0;		/* the input last looked at, Clock_Ms() */
static ULONG g_Deaf = 0;		/* the longest without it, since the play began (the trace) */

/* A step of the play's work sized to the time it has (the controls are
   looked at between the steps): its cost a unit measured as it goes, the
   next piece what fits in PACE_MS -- a fast machine keeps the most, a
   slow one gets small ones. Measured on an A2000 (25 MHz 68030): 160
   columns painted ahead 215 ms, 16384 frames read 150 ms; on an A1200
   at 178 frames a column, 160 columns 175 ms -- in pieces of 37 the
   controls waited 182 ms at most, 322 a piece of each a beat */
#define PACE_MS		50UL
struct Pace {
	ULONG pc_Units;			/* the next piece */
	ULONG pc_Min, pc_Max;
	ULONG pc_Us;			/* us a unit, averaged; 0 none measured */
};
/* small at first: the first piece is measured, a fast machine's next is
   the most (a full first piece kept an A1200 deaf 182 ms) */
static struct Pace g_PacePaint = { 32, 8, 160, 0 };		/* the next page's columns */
static struct Pace g_PaceFeed = { 4096, 1024, 16384, 0 };	/* AHI's frames */
static struct Pace g_PaceRead = { 4096, 1024, 16384, 0 };	/* the read-ahead's frames */
static struct Pace g_PaceOver = { 32, 8, 160, 0 };		/* the overview's columns (lower: cheaper) */
static BOOL g_NoPace = FALSE;		/* WaveSmithNoPace set: a piece of each a beat, as before (to compare) */
static ULONG g_PaintSet = 0;		/* the gadget's WAVEFORM_PaintColumns */
static ULONG g_FillSet = 0;		/* ... its WAVEFORM_FillNew */
static ULONG g_OverSet = 0;		/* the overview's PaintColumns (and FillNew: not 0) */
static BOOL g_WantOwed = FALSE;		/* samples the gadget wanted while it played, or stopped with
					   a page filling in: the read-ahead's, else fetched */
static BOOL g_FetchNow = FALSE;		/* the read-ahead could not: what is wanted fetched whole */

/* n units done in the ms since from: the next piece sized (a piece cut
   short -- a page's last columns -- tells too little); one over twice
   its time sizes the next by itself alone (a trimmed part's columns,
   hatched, cost 3.5 times the others: 242 ms, then 127, averaged) */
static void pace(struct Pace *pc, ULONG n, ULONG from)
{
	ULONG us, ms;

	if (!g_HaveClock || !n || n < pc->pc_Units / 2) return;
	ms = Clock_Ms() - from;
	us = ms * 1000UL / n;
	pc->pc_Us = pc->pc_Us && ms <= 2 * PACE_MS ? (pc->pc_Us * 3UL + us) / 4UL : us;
	pc->pc_Units = pc->pc_Us ? PACE_MS * 1000UL / pc->pc_Us : pc->pc_Max;
	if (pc->pc_Units > pc->pc_Max) pc->pc_Units = pc->pc_Max;
	if (pc->pc_Units < pc->pc_Min) pc->pc_Units = pc->pc_Min;
}

/* while it plays, a paint of the lanes paints a piece at most -- a page
   turned before the next one was ready, samples come in -- the rest in
   the pieces after it (Waveform_PaintAhead); stopped, whole as ever once
   the page a play left is (a Pause turning a page: no 577 ms painting).
   A new view -- a zoom, a scroll, new samples, an edit, a jump -- fills
   in by pieces over the old picture, the main loop's (an A1200 painted
   a page in 2.1 s); not while a file is read: the reading's loop paints
   none. Stopped, no budget: a jump is such a view (a turn's clearing
   cost 74 ms), a change while one fills in paints what it shows */
static void paint_budget(void)
{
	ULONG want = Play_Playing() && !g_NoPace ? g_PacePaint.pc_Units : 0, fill = (ULONG) (!g_NoPace && !g_Reading);

	if (!g_Wave) return;
	if (fill != g_FillSet) {
		g_FillSet = fill;
		set1(g_Wave, WAVEFORM_FillNew, fill);
	}
	/* the overview's changes in pieces as well, playing or not: its box
	   shrunk by a zoom repainted 313 columns at once (617 ms) */
	if (g_Over && g_Window && (fill ? g_PaceOver.pc_Units : 0) != g_OverSet) {
		g_OverSet = fill ? g_PaceOver.pc_Units : 0;
		SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_PaintColumns, g_OverSet, WAVEFORM_FillNew, fill,
			       TAG_DONE);
	}
	if (want == g_PaintSet) return;
	g_PaintSet = want;
	set1(g_Wave, WAVEFORM_PaintColumns, want);
}
static struct Prefetch g_Pre;		/* the samples ahead of the play head (prefetch.h) */
static BOOL g_PreOpen = FALSE, g_PreTried = FALSE;

/* the read-ahead given up: the gadget lets go of its buffers first */
static void prefetch_off(void)
{
	if (!g_PreOpen) {
		g_PreTried = FALSE;
		return;
	}
	if (g_Window) set1(g_Wave, WAVEFORM_Samples, 0);
	Prefetch_Close(&g_Pre);
	g_PreOpen = g_PreTried = FALSE;
}

/* pages pages of the view a read-ahead can hold: its buffers open with
   room for them, or to be opened (its own given back first) at PRE_MAX
   frames and the two half the memory free at most -- a fixed 2 MB had
   ruled out the next page of a 10 s view, fetched at each turn while it
   played (2 s on an A2000) */
static BOOL pre_room(ULONG pages)
{
	ULONG want = pages * get(g_Wave, WAVEFORM_Visible), avail;

	if (!want || want > PRE_MAX) return FALSE;
	if (g_PreOpen && want <= g_Pre.pf_Max) return TRUE;
	avail = AvailMem(MEMF_ANY) + (g_PreOpen ? 2 * g_Pre.pf_Max * g_Pre.pf_Align : 0);
	return (BOOL) (4 * want * g_Doc.d_Wav.nw_Align <= avail);
}

/* while it plays finer than the picture holds: the samples of this page
   and the next read ahead (pages 2; stopped, this page's: 1), a piece a
   beat (frames at most), handed to the gadget whole -- the next page is
   drawn from them the moment it turns; the frames read (1: copied from
   the gadget's samples, or handed only) */
static ULONG prefetch_beat(ULONG frames, ULONG pages)
{
	struct WaveSamples *ws;
	struct TraceNums tn;
	ULONG zoom = get(g_Wave, WAVEFORM_Zoom), top = get(g_Wave, WAVEFORM_Top), vis = get(g_Wave, WAVEFORM_Visible);

	if (!zoom || zoom >= g_Doc.d_Source.ws_Finest || !pre_room(pages)) return 0;
	/* buffers for two pages where they fit, else one (stopped): grown
	   when a zoom wants more */
	if (g_PreOpen && pages * vis > g_Pre.pf_Max) prefetch_off();
	if (!g_PreOpen) {
		if (g_PreTried) return 0;	/* once a doc and a size: no memory, no file */
		g_PreTried = TRUE;
		if (!(g_PreOpen = Prefetch_Open(&g_Pre, &g_Doc, pre_room(2) ? 2 * vis : vis))) return 0;
	}
	Prefetch_Want(&g_Pre, top, pages * vis, (const struct WaveSamples *) get(g_Wave, WAVEFORM_Samples));
	g_Pre.pf_Piece = frames;
	if ((ws = Prefetch_Step(&g_Pre)) != NULL) {
		/* the same sound: what the gadget drew from the last samples stays */
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Samples, (ULONG) ws, WAVEFORM_SamplesMore, TRUE,
			       TAG_DONE);
		tn.tn_What = (CONST_STRPTR) "prefetched first frames";
		tn.tn_N[0] = ws->wsm_First;
		tn.tn_N[1] = ws->wsm_Frames;
		tn.tn_Count = 2;
		trace_nums(&tn);
	}
	/* a step that copied or only handed: done, 1 (nothing to time a read by) */
	return g_Pre.pf_Read ? g_Pre.pf_Read : (ULONG) (ws || g_Pre.pf_Copied);
}
static BOOL g_Paused = FALSE;		/* Pause pressed: Play goes on from g_PauseAt */
static ULONG g_PauseAt = 0;

/* where gadget o's head goes, a sprite's line (hl, cleared first): shown
   while it plays in the active window */
static void head_line(Object *o, struct HeadLine *hl)
{
	const struct WaveHeadBox *hb = o ? (const struct WaveHeadBox *) get(o, WAVEFORM_HeadBox) : NULL;

	clear_bytes((UBYTE *) hl, sizeof(*hl));
	if (!hb || !hb->hb_Columns) return;
	hl->hl_On = (BOOL) (Play_Playing() && (g_Window->Flags & WFLG_WINDOWACTIVE));
	hl->hl_First = hb->hb_First;
	hl->hl_Zoom = hb->hb_Zoom;
	hl->hl_Left = (WORD) (g_Window->LeftEdge + hb->hb_Left);
	hl->hl_Top = (WORD) (g_Window->TopEdge + hb->hb_Top);
	hl->hl_Columns = hb->hb_Columns;
	hl->hl_Rows = hb->hb_Rows;
}

/* the sprite heads told where the play head is and where they may show:
   playing, the window the active one; wherever one may not (or has no
   sprite, or too many layers lie over it), its gadget draws the head
   itself (concept Transport 4, 11) */
static void head_place(void)
{
	struct HeadPlace hp;
	UWORD on, now;

	if (!g_HeadSprite || !g_Window || !g_Wave) return;
	hp.hp_Frame = Play_Frame();
	hp.hp_Ticks = Clock_Ticks();
	hp.hp_Rate = g_Doc.d_Rate;
	head_line(g_Wave, &hp.hp_Line[HEAD_MAIN]);
	head_line(g_Over, &hp.hp_Line[HEAD_OVER]);
	on = HeadSprite_Place(&hp);
	now = (UWORD) (on ^ g_HeadOff);
	g_HeadOff = on;
	if (now & (1 << HEAD_MAIN)) {
		set1(g_Wave, WAVEFORM_HeadOff, (ULONG) ((on >> HEAD_MAIN) & 1));
		/* not "head ...": the tests count those as the head taken off */
		trace((CONST_STRPTR) (on & (1 << HEAD_MAIN) ? "headsprite moves" : "headsprite drawn"));
	}
	if ((now & (1 << HEAD_OVER)) && g_Over) {
		set1(g_Over, WAVEFORM_HeadOff, (ULONG) ((on >> HEAD_OVER) & 1));
		trace((CONST_STRPTR) (on & (1 << HEAD_OVER) ? "oversprite moves" : "oversprite drawn"));
	}
}

/* the deck shows what plays: its Play key down while it plays, its Pause
   key while paused, Stop else (a Rewind or a Forward springs back) */
static void play_label(void)
{
	ULONG mode = (ULONG) (Play_Playing() || g_Paused ? BUT_PLAY : BUT_STOP), m = 0, p = 0;

	g_DeckPaused = g_Paused;
	/* drawn only when it shows otherwise (its own keys move it too): a
	   Stop set it twice, 30 ms each on an A2000 */
	if (g_Window && g_Deck && (!GetAttr(TDECK_Mode, g_Deck, &m) || !GetAttr(TDECK_Paused, g_Deck, &p) || m != mode ||
				   (BOOL) (p != 0) != g_Paused))
		SetGadgetAttrs((struct Gadget *) g_Deck, g_Window, NULL, TDECK_Mode, mode, TDECK_Paused, (ULONG) g_Paused, TAG_DONE);
	head_place();		/* played, paused, stopped: the sprite as well */
	paint_budget();
	/* the samples it wanted while it played: fetched now (wave_changes) */
	if (g_WantOwed && !Play_Playing()) Signal(FindTask(NULL), g_SigMask);
}

/* how often the head moved since the play began, for the trace */
static void beats_said(void)
{
	struct TraceNums tn;

	if (!g_HaveClock || !g_BeatsFrom) return;
	tn.tn_What = (CONST_STRPTR) "beats count ms";
	tn.tn_N[0] = g_Beats;
	tn.tn_N[1] = Clock_Ms() - g_BeatsFrom;
	tn.tn_Count = 2;
	trace_nums(&tn);
	/* the longest the controls waited for the play's work, and the
	   pieces it came in by then */
	tn.tn_What = (CONST_STRPTR) "deaf ms";
	tn.tn_N[0] = g_Deaf;
	tn.tn_Count = 1;
	trace_nums(&tn);
	tn.tn_What = (CONST_STRPTR) "pace columns feed read";
	tn.tn_N[0] = g_PacePaint.pc_Units;
	tn.tn_N[1] = g_PaceFeed.pc_Units;
	tn.tn_N[2] = g_PaceRead.pc_Units;
	tn.tn_Count = 3;
	trace_nums(&tn);
	/* AHI's frames: copied from what was read already, read again */
	{
		struct AhiCounts ac;

		Ahi_Counts(&ac);
		tn.tn_What = (CONST_STRPTR) "ahi copied read differ";
		tn.tn_N[0] = ac.ac_Copied;
		tn.tn_N[1] = ac.ac_Read;
		tn.tn_N[2] = ac.ac_Differ;
		tn.tn_Count = 3;
		trace_nums(&tn);
	}
	g_BeatsFrom = 0;
}

static void stop_play(void)
{
	beats_said();
	if (Play_Playing()) Play_Stop();
	head_off();
	g_PlaySaid = FALSE;		/* a stop's own action says what happened */
	g_Paused = FALSE;
	play_label();
}

/* the gadgets let go of the doc's picture (before it is freed) */
static void forget_doc(void)
{
	stop_play();
	if (g_Window) {
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Source, 0, WAVEFORM_Regions, 0,
			       WAVEFORM_RegionCount, 0, WAVEFORM_Markers, 0, WAVEFORM_MarkerCount, 0, WAVEFORM_LevelCount, 0,
			       WAVEFORM_FadeIn, 0, WAVEFORM_FadeOut, 0, WAVEFORM_Samples, 0, WAVEFORM_TrimIn, WAVEFORM_NONE,
			       WAVEFORM_TrimOut, WAVEFORM_NONE, WAVEFORM_Cursor, WAVEFORM_NONE,
			       WAVEFORM_SelStart, WAVEFORM_NONE, WAVEFORM_SelEnd, WAVEFORM_NONE, TAG_DONE);
		SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_Source, 0, WAVEFORM_TrimIn, WAVEFORM_NONE,
			       WAVEFORM_TrimOut, WAVEFORM_NONE, TAG_DONE);
	}
	prefetch_off();
	if (g_HaveDoc) Doc_Close(&g_Doc);
	Undo_Free(&g_Undo);
	/* a work file goes with the doc (closed first: it is open) */
	if (g_Decoding || g_Decoded) Decode_Forget();
	g_HaveDoc = FALSE;
	set_dirty(FALSE);
	g_Reading = FALSE;
	job_on(FALSE);
	g_PlanShown = FALSE;
	g_Decoding = FALSE;
	g_Decoded = FALSE;
	/* no file: the title and the times say none */
	Text_Copy((STRPTR) g_Title, (CONST_STRPTR) TXT_TITLE, STATUS_LEN);
	if (g_WinObj) SetAttrs(g_WinObj, WA_Title, (ULONG) g_Title, TAG_DONE);
	g_InText[0] = g_OutText[0] = g_LenText[0] = g_LenHint[0] = '\0';
	if (g_Window) set1(g_Len, GA_Text, (ULONG) g_LenText);
	trim_shown();
}

static void title_for_doc(void)
{
	struct TextArgs ta;
	char len[TIME_LEN], name[112];
	UWORD n;

	time_text(Doc_Length(&g_Doc), len);
	/* the name, a * behind it while the user's changes are not saved */
	Text_Copy((STRPTR) name, file_name(), sizeof(name) - 1);
	if (g_Dirty) {
		n = (UWORD) Text_Length((CONST_STRPTR) name);
		name[n] = '*';
		name[n + 1] = '\0';
	}
	ta.ta_Fmt = (CONST_STRPTR) TXT_TITLE_FILE;
	ta.ta_Str[0] = (CONST_STRPTR) name;
	ta.ta_Str[1] = (CONST_STRPTR) len;
	ta.ta_Str[2] = (CONST_STRPTR) (g_Doc.d_Channels == 1 ? TXT_MONO : TXT_STEREO);
	ta.ta_Num[0] = (LONG) g_Doc.d_Rate;
	ta.ta_Num[1] = ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) g_Title, STATUS_LEN, &ta);
	if (g_WinObj) SetAttrs(g_WinObj, WA_Title, (ULONG) g_Title, TAG_DONE);
}

/* the user's changes saved or not: the title says it */
static void set_dirty(BOOL d)
{
	struct TraceNums tn;

	if (d == g_Dirty) return;
	g_Dirty = d;
	if (g_HaveDoc) title_for_doc();
	tn.tn_What = (CONST_STRPTR) "dirty";
	tn.tn_N[0] = (ULONG) d;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* --- the history: Undo and Redo (undo.h) ------------------------------------------- */

/* the timeline on the trace: "<what> <pieces> <frames>" */
static void trace_pieces(CONST_STRPTR what)
{
	struct TraceNums tn;

	tn.tn_What = what;
	tn.tn_N[0] = g_Doc.d_Tl.tl_Count;
	tn.tn_N[1] = g_Doc.d_Tl.tl_Frames;
	tn.tn_Count = 2;
	trace_nums(&tn);
}

/* every piece on the trace before a save, its level with it, and the
   levels from outside the pieces: the tests' checker rebuilds the file
   from them (tools/make_edit_single.py) */
static void trace_save_pieces(void)
{
	const struct Piece *pc;
	const struct TlLevels *lv = &g_Doc.d_Levels;
	char line[STATUS_LEN];
	STRPTR p;
	UWORD i;

	if (!g_TracePath[0]) return;
	/* not "saving ...": the tests count those lines as the save's per cents */
	trace_pieces((CONST_STRPTR) "saveplan pieces frames");
	for (i = 0; i < g_Doc.d_Tl.tl_Count; i++) {
		pc = &g_Doc.d_Tl.tl_Pieces[i];
		p = Text_Put((STRPTR) line, (CONST_STRPTR) "piece from frames gain fade u0 u1 ");
		p = Text_Put(Text_PutNumber(p, pc->p_From), (CONST_STRPTR) " ");
		p = Text_Put(Text_PutNumber(p, pc->p_Frames), (CONST_STRPTR) " ");
		p = Text_Put(Text_PutNumber(p, pc->p_Gain), (CONST_STRPTR) " ");
		p = Text_Put(Text_PutNumber(p, pc->p_Fade), (CONST_STRPTR) " ");
		p = Text_Put(Text_PutNumber(p, pc->p_U0), (CONST_STRPTR) " ");
		Text_PutNumber(p, pc->p_U1);
		trace((CONST_STRPTR) line);
	}
	p = Text_Put((STRPTR) line, (CONST_STRPTR) "savelevels in out fadein fadeout dip shape ");
	p = Text_Put(Text_PutNumber(p, lv->lv_In), (CONST_STRPTR) " ");
	p = Text_Put(Text_PutNumber(p, lv->lv_Out), (CONST_STRPTR) " ");
	p = Text_Put(Text_PutNumber(p, lv->lv_FadeIn), (CONST_STRPTR) " ");
	p = Text_Put(Text_PutNumber(p, lv->lv_FadeOut), (CONST_STRPTR) " ");
	p = Text_Put(Text_PutNumber(p, lv->lv_Dip), (CONST_STRPTR) " ");
	Text_PutNumber(p, lv->lv_Shape);
	trace((CONST_STRPTR) line);
}

static void menu_on(UWORD num, BOOL on)
{
	if (on) OnMenu(g_Window, num);
	else OffMenu(g_Window, num);
}

/* the Edit menu as the doc and its history allow */
static void edit_menu(void)
{
	BOOL usable = doc_usable();

	if (!g_Strip || !g_Window) return;
	menu_on(FULLMENUNUM(MENU_EDIT, 0, NOSUB), (BOOL) (usable && Undo_CanBack(&g_Undo)));
	menu_on(FULLMENUNUM(MENU_EDIT, 1, NOSUB), (BOOL) (usable && Undo_CanForward(&g_Undo)));
	menu_on(FULLMENUNUM(MENU_EDIT, 3, NOSUB), (BOOL) (usable && !g_Reading));
	menu_on(FULLMENUNUM(MENU_EDIT, 5, NOSUB), usable);
	menu_on(FULLMENUNUM(MENU_EDIT, 6, NOSUB), usable);
	menu_on(FULLMENUNUM(MENU_EDIT, 8, NOSUB), usable);
	menu_on(FULLMENUNUM(MENU_EDIT, 9, NOSUB), (BOOL) (usable && g_Marks.mk_Count));
	menu_on(FULLMENUNUM(MENU_EDIT, 10, NOSUB), (BOOL) (usable && !g_Reading));
	/* Effects: a level is set on a part once the file is read (Cut out's rule) */
	menu_on(FULLMENUNUM(MENU_EFFECTS, 0, NOSUB), (BOOL) (usable && !g_Reading));
	menu_on(FULLMENUNUM(MENU_EFFECTS, 1, NOSUB), (BOOL) (usable && !g_Reading));
	menu_on(FULLMENUNUM(MENU_EFFECTS, 3, NOSUB), (BOOL) (usable && !g_Reading));
	menu_on(FULLMENUNUM(MENU_EFFECTS, 4, NOSUB), (BOOL) (usable && !g_Reading));
	menu_on(FULLMENUNUM(MENU_EFFECTS, 6, NOSUB), (BOOL) (usable && !g_Reading));
	/* Project, Split at markers: with markers between In and Out */
	menu_on(FULLMENUNUM(MENU_PROJECT, 5, NOSUB), (BOOL) (usable && !g_Reading &&
						 Marks_Inside(&g_Marks, get(g_Wave, WAVEFORM_TrimIn) == WAVEFORM_NONE ? 0 :
							      get(g_Wave, WAVEFORM_TrimIn),
							      get(g_Wave, WAVEFORM_TrimOut) == WAVEFORM_NONE ? Doc_Length(&g_Doc) :
							      get(g_Wave, WAVEFORM_TrimOut))));
}

/* a shape of the level's (ENV_SHAPE_#?) as the waveform names it */
static UWORD level_shape(UWORD shape)
{
	if (shape == ENV_SHAPE_LINEAR) return WAVELEVEL_LINEAR;
	if (shape == ENV_SHAPE_SCURVE) return WAVELEVEL_SCURVE;
	return WAVELEVEL_EVEN;
}

/* the pieces at a level of their own (concept 3.2) into g_ShownLevels,
   timeline frames: a fade's line, a gain's bar, a tag with the gain where
   it changes -- how many */
static UWORD levels_listed(void)
{
	const struct Piece *p;
	struct WaveLevel *lv;
	ULONG start = 0;
	UWORD i, n = 0, tags = 0, before = ENV_UNITY, dir;
	STRPTR q;

	for (i = 0; i < g_Doc.d_Tl.tl_Count && n < SHOWN_LEVELS; i++) {
		p = &g_Doc.d_Tl.tl_Pieces[i];
		dir = (UWORD) (p->p_Fade & TL_FADE_DIR);
		if (p->p_Gain != ENV_UNITY || dir) {
			lv = &g_ShownLevels[n++];
			lv->wlv_First = start;
			lv->wlv_End = start + p->p_Frames;
			lv->wlv_Gain = p->p_Gain;
			lv->wlv_Fade = (UWORD) (!dir ? WAVELEVEL_NONE :
						(dir == TL_FADE_IN ? WAVELEVEL_IN : WAVELEVEL_OUT) | level_shape((UWORD) (p->p_Fade & TL_FADE_SHAPE)));
			lv->wlv_U0 = p->p_U0;
			lv->wlv_U1 = p->p_U1;
			lv->wlv_Label = NULL;
			if (p->p_Gain != ENV_UNITY && p->p_Gain != before && tags < SHOWN_TAGS) {
				q = Gain_PutTenths((STRPTR) g_LevelText[tags], Env_Tenths(p->p_Gain));
				q[0] = ' ';
				q[1] = 'd';
				q[2] = 'B';
				q[3] = '\0';
				lv->wlv_Label = (CONST_STRPTR) g_LevelText[tags++];
			}
		}
		before = p->p_Gain;
		start += p->p_Frames;
	}
	return n;
}

/* the suggestion's regions (the file's frames) where they play on the
   timeline, each silence tinted as the lead-in's, a thin mark at every
   join (concept 3.1), the pieces' levels and the end fades (3.2): into
   the waveform */
static void regions_shown(void)
{
	const struct TrimPlan *tp = &g_Doc.d_Plan;
	struct TlRange rg[SHOWN_REGIONS];
	struct TlRanges tr;
	ULONG joins[SHOWN_JOINS], start = 0;
	const struct Piece *p;
	UWORD i, k, n = 0, m;

	for (i = 0; i < tp->tp_RegionCount && n < SHOWN_REGIONS; i++) {
		tr.tr_First = tp->tp_Region[i].wrg_First;
		tr.tr_End = tp->tp_Region[i].wrg_End;
		tr.tr_Out = rg;
		tr.tr_Max = (UWORD) (SHOWN_REGIONS - n);
		m = Tl_Ranges(&g_Doc.d_Tl, &tr);
		for (k = 0; k < m; k++, n++) {
			g_ShownRegions[n] = tp->tp_Region[i];
			g_ShownRegions[n].wrg_First = rg[k].tg_First;
			g_ShownRegions[n].wrg_End = rg[k].tg_End;
		}
	}
	for (i = 0; i < g_Doc.d_Tl.tl_Count; i++) {
		p = &g_Doc.d_Tl.tl_Pieces[i];
		if (p->p_From == TL_SILENCE && n < SHOWN_REGIONS) {
			g_ShownRegions[n].wrg_First = start;
			g_ShownRegions[n].wrg_End = start + p->p_Frames;
			g_ShownRegions[n].wrg_Kind = WAVEREGION_SILENCE;
			g_ShownRegions[n].wrg_Style = WAVEREGION_TINT;
			g_ShownRegions[n].wrg_Pen = -1;
			n++;
		}
		start += p->p_Frames;
	}
	/* the markers, labelled with the track each starts (the first track
	   is the file's start), the one clicked shown as such; then the joins */
	for (i = 0; i < g_Marks.mk_Count; i++) {
		STRPTR q = Text_PutNumber((STRPTR) g_MarkText[i], (ULONG) i + 2);

		*q = '\0';
		g_Joins[i].wm_Frame = g_Marks.mk_Frame[i];
		g_Joins[i].wm_Kind = WAVEMARKER_CUE;
		g_Joins[i].wm_Flags = (UWORD) (i == g_MarkSel ? WAVEMARKER_SELECTED : 0);
		g_Joins[i].wm_Pen = -1;
		g_Joins[i].wm_Label = (CONST_STRPTR) g_MarkText[i];
	}
	k = g_Marks.mk_Count;
	m = Tl_Joins(&g_Doc.d_Tl, joins, SHOWN_JOINS);
	for (i = 0; i < m; i++, k++) {
		g_Joins[k].wm_Frame = joins[i];
		g_Joins[k].wm_Kind = WAVEMARKER_SPLIT;
		g_Joins[k].wm_Flags = WAVEMARKER_FIXED | WAVEMARKER_NOLABEL;
		g_Joins[k].wm_Pen = -1;
		g_Joins[k].wm_Label = NULL;
	}
	m = levels_listed();
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Regions, (ULONG) g_ShownRegions,
		       WAVEFORM_RegionCount, n, WAVEFORM_Markers, (ULONG) g_Joins, WAVEFORM_MarkerCount, k,
		       WAVEFORM_Levels, (ULONG) g_ShownLevels, WAVEFORM_LevelCount, m, WAVEFORM_FadeIn, g_Doc.d_Levels.lv_FadeIn,
		       WAVEFORM_FadeOut, g_Doc.d_Levels.lv_FadeOut, WAVEFORM_FadeShape, level_shape(g_Prefs.ep_FadeShape), TAG_DONE);
}

/* the levels the picture is drawn with (timeline_shown's) */
static struct TlLevels g_LvShown;

/* the end fades on the handles (concept 3.2: they move with them, their
   lengths kept, never longer than In .. Out), the setting's shape: TRUE
   when the picture is drawn with other levels */
static BOOL levels_now(void)
{
	struct TlLevels *lv = &g_Doc.d_Levels, *sh = &g_LvShown;
	ULONG in, out, room;

	if (!g_HaveDoc || !g_Wave) return FALSE;
	in = trim_in();
	out = trim_out();
	room = out > in ? out - in : 0;
	lv->lv_In = in;
	lv->lv_Out = out;
	lv->lv_Shape = g_Prefs.ep_FadeShape;
	if (lv->lv_FadeIn > room) lv->lv_FadeIn = room;
	if (lv->lv_FadeOut > room) lv->lv_FadeOut = room;
	if (!Tl_Levels(lv) && !Tl_Levels(sh)) return FALSE;
	return (BOOL) (lv->lv_In != sh->lv_In || lv->lv_Out != sh->lv_Out || lv->lv_FadeIn != sh->lv_FadeIn ||
		       lv->lv_FadeOut != sh->lv_FadeOut || lv->lv_Dip != sh->lv_Dip || lv->lv_Shape != sh->lv_Shape);
}

/* the picture drawn anew when the end fades moved or changed (a drag let
   go, an Undo, a fade made): the read-ahead's samples were at the old */
static void levels_shown(void)
{
	struct TraceNums tn;

	if (!levels_now()) return;
	prefetch_off();
	timeline_shown();
	tn.tn_What = (CONST_STRPTR) "endfades fadein fadeout";
	tn.tn_N[0] = g_Doc.d_Levels.lv_FadeIn;
	tn.tn_N[1] = g_Doc.d_Levels.lv_FadeOut;
	tn.tn_Count = 2;
	trace_nums(&tn);
}

/* the state shown, as the history keeps it */
static void mark_now(struct UndoMark *m)
{
	m->um_In = get(g_Wave, WAVEFORM_TrimIn);
	m->um_Out = get(g_Wave, WAVEFORM_TrimOut);
	m->um_FadeIn = g_Doc.d_Levels.lv_FadeIn;
	m->um_FadeOut = g_Doc.d_Levels.lv_FadeOut;
	m->um_Tl = &g_Doc.d_Tl;
	m->um_Marks = g_Marks.mk_Frame;
	m->um_MarkCount = g_Marks.mk_Count;
	m->um_Pad = 0;
}

/* the history anew: the doc as opened, its suggestion shown, the first step */
static UWORD g_Target = TARGET_MARKER;	/* the arrows' (a file opened: the marker) */
static UWORD g_KeyRun = TARGET_MARKER;	/* the handle whose arrow steps the step shown is */

static void history_start(void)
{
	struct UndoMark m;

	g_Target = g_KeyRun = TARGET_MARKER;
	/* the picture of the file opened has no end fade (Doc_Open's levels) */
	clear_bytes((UBYTE *) &g_LvShown, sizeof(g_LvShown));
	levels_shown();
	mark_now(&m);
	if (!Undo_Start(&g_Undo, &m)) set_status((CONST_STRPTR) TXT_ST_NOUNDO);
	set_dirty(FALSE);		/* a history anew: the state is the file's */
	edit_menu();
}

/* a change of the user's made (a handle moved, a cut): the history's
   next step, the title's * from it */
static void changed(void)
{
	struct UndoMark m;

	g_KeyRun = TARGET_MARKER;	/* another change: the arrows' run ends */
	levels_shown();			/* the end fades where the handles are now */
	mark_now(&m);
	if (Undo_Push(&g_Undo, &m) == UNDO_NOMEM) set_status((CONST_STRPTR) TXT_ST_NOUNDO);
	set_dirty(Undo_Dirty(&g_Undo));
	edit_menu();
}

/* the timeline changed (a cut, an Undo): the gadgets draw it anew, the
   title says its length */
static void timeline_shown(void)
{
	levels_now();
	g_LvShown = g_Doc.d_Levels;	/* what the picture is drawn with */
	Doc_Changed(&g_Doc);
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Samples, 0, WAVEFORM_Source, (ULONG) &g_Doc.d_Source,
		       TAG_DONE);
	set1(g_Over, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
	regions_shown();
	title_for_doc();
	sync_views();
	trace_pieces((CONST_STRPTR) "timeline pieces frames");
}

/* Undo, or Redo: the step's pieces shown (playing and the read-ahead
   stopped first: they read the timeline), its In and Out */
static void undo_step(BOOL redo)
{
	const struct UndoState *s;
	struct TraceNums tn;
	char tin[TIME_LEN], tout[TIME_LEN];
	struct Say sy;
	ULONG out;

	if (!doc_usable()) return;
	if (!(s = redo ? Undo_Forward(&g_Undo) : Undo_Back(&g_Undo))) return;
	g_KeyRun = TARGET_MARKER;
	{
		struct MarkSet ms;

		ms.ms_Frames = s->us_Marks;
		ms.ms_Count = s->us_MarkCount;
		ms.ms_End = 0xFFFFFFFFUL;
		Marks_Set(&g_Marks, &ms);
		g_MarkSel = MARKS_NONE;
	}
	Undo_Apply(s, &g_SpareTl);
	if (!Tl_Same(&g_SpareTl, &g_Doc.d_Tl)) {
		stop_play();
		prefetch_off();
		g_Doc.d_Tl = g_SpareTl;
		timeline_shown();
	}
	/* In before Out at every moment: the one moving out of the other's
	   way set first */
	out = get(g_Wave, WAVEFORM_TrimOut);
	if (out != WAVEFORM_NONE && s->us_In != WAVEFORM_NONE && s->us_In >= out) {
		set1(g_Wave, WAVEFORM_TrimOut, s->us_Out);
		set1(g_Wave, WAVEFORM_TrimIn, s->us_In);
	} else {
		set1(g_Wave, WAVEFORM_TrimIn, s->us_In);
		set1(g_Wave, WAVEFORM_TrimOut, s->us_Out);
	}
	/* the step's end fades, drawn where its handles are */
	g_Doc.d_Levels.lv_FadeIn = s->us_FadeIn;
	g_Doc.d_Levels.lv_FadeOut = s->us_FadeOut;
	levels_shown();
	show_trim();
	regions_shown();		/* the markers as the step had them */
	trace_marks();
	set_dirty(Undo_Dirty(&g_Undo));
	edit_menu();
	tn.tn_What = (CONST_STRPTR) (redo ? "redone in out pieces" : "undone in out pieces");
	tn.tn_N[0] = s->us_In;
	tn.tn_N[1] = s->us_Out;
	tn.tn_N[2] = s->us_Count;
	tn.tn_Count = 3;
	trace_nums(&tn);
	time_text(s->us_In, tin);
	time_text(s->us_Out, tout);
	sy.sy_Fmt = (CONST_STRPTR) (redo ? TXT_ST_REDONE : TXT_ST_UNDONE);
	sy.sy_S0 = (CONST_STRPTR) tin;
	sy.sy_S1 = (CONST_STRPTR) tout;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

/* a frame after a joined cut: on by what went before it, at the cut's
   place when it was in it */
static ULONG after_cut(ULONG f, const struct TlRange *cut)
{
	if (f == WAVEFORM_NONE || f <= cut->tg_First) return f;
	return f >= cut->tg_End ? f - (cut->tg_End - cut->tg_First) : cut->tg_First;
}

/* Edit/Cut out: the selection out of the timeline, its edges on zero
   crossings as the handles' (D7); asked every time (E3) whether the
   parts on either side join or silence takes its place */
static void cut_out(void)
{
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd), t;
	ULONG in = get(g_Wave, WAVEFORM_TrimIn), out = get(g_Wave, WAVEFORM_TrimOut), c = get(g_Wave, WAVEFORM_Cursor);
	struct TraceNums tn;
	struct TlRange cut;
	char len[TIME_LEN];
	LONG answer;
	BOOL ok;

	if (!doc_usable()) return;
	if (g_Reading) {
		say1((CONST_STRPTR) TXT_ST_CUTREADING, file_name(), 0);
		return;
	}
	if (a == WAVEFORM_NONE || b == WAVEFORM_NONE || a == b) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	if (b > Doc_Length(&g_Doc)) b = Doc_Length(&g_Doc);
	/* playing stopped first: it reads the timeline, and the question
	   holds the window's loop */
	stop_play();
	prefetch_off();
	if ((answer = ask((CONST_STRPTR) TXT_ASK_CUT, (CONST_STRPTR) TXT_ASK_CUT_GADS)) == 0) return;
	if (a) a = snapped(a);
	if (b < Doc_Length(&g_Doc)) b = snapped(b);
	if (b <= a) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	ok = answer == 1 ? Tl_Cut(&g_Doc.d_Tl, a, b) : Tl_Silence(&g_Doc.d_Tl, a, b);
	if (!ok) {
		set_status((CONST_STRPTR) TXT_ST_CUTFULL);
		return;
	}
	if (answer == 1) {
		/* the handles, the cursor and the markers on by what went before them */
		Marks_Cut(&g_Marks, a, b);
		g_MarkSel = MARKS_NONE;
		cut.tg_First = a;
		cut.tg_End = b;
		in = after_cut(in, &cut);
		out = after_cut(out, &cut);
		if (out != WAVEFORM_NONE && in != WAVEFORM_NONE && out <= in) out = in + 1 < Doc_Length(&g_Doc) ? in + 1 : Doc_Length(&g_Doc);
		c = after_cut(c, &cut);
	}
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_SelStart, WAVEFORM_NONE, WAVEFORM_SelEnd, WAVEFORM_NONE,
		       WAVEFORM_Cursor, c, TAG_DONE);
	timeline_shown();
	set1(g_Wave, WAVEFORM_TrimIn, in);
	set1(g_Wave, WAVEFORM_TrimOut, out);
	show_trim();
	changed();
	tn.tn_What = (CONST_STRPTR) "cut from to join";
	tn.tn_N[0] = a;
	tn.tn_N[1] = b;
	tn.tn_N[2] = (ULONG) (answer == 1);
	tn.tn_Count = 3;
	trace_nums(&tn);
	time_text(b - a, len);
	say1((CONST_STRPTR) (answer == 1 ? TXT_ST_CUT_JOINED : TXT_ST_CUT_SILENCE), (CONST_STRPTR) len, 0);
}

/* the shapes' names, as Settings/Fades... has them (ENV_SHAPE_#?) */
static CONST_STRPTR shape_name(UWORD shape)
{
	if (shape == ENV_SHAPE_LINEAR) return (CONST_STRPTR) TXT_SHAPE_LINEAR;
	if (shape == ENV_SHAPE_SCURVE) return (CONST_STRPTR) TXT_SHAPE_SCURVE;
	return (CONST_STRPTR) TXT_SHAPE_EVEN;
}

/* the part an effect works on: the one marked, else (whole: Gain,
   Normalise, Remove, F5) In .. Out; its edges on zero crossings as a
   cut's. FALSE, said, for none; TRUE with playing and the read-ahead
   stopped (they read the timeline) */
static BOOL effect_part(struct TlPart *pt, BOOL whole)
{
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd), t;

	if (!doc_usable()) return FALSE;
	if (g_Reading) {
		say1((CONST_STRPTR) TXT_ST_CUTREADING, file_name(), 0);
		return FALSE;
	}
	if (a == WAVEFORM_NONE || b == WAVEFORM_NONE || a == b) {
		if (!whole) {
			set_status((CONST_STRPTR) TXT_ST_NOSEL);
			return FALSE;
		}
		a = trim_in();
		b = trim_out();
	}
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	if (b > Doc_Length(&g_Doc)) b = Doc_Length(&g_Doc);
	if (a) a = snapped(a);
	if (b < Doc_Length(&g_Doc)) b = snapped(b);
	if (b <= a) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return FALSE;
	}
	stop_play();
	prefetch_off();
	pt->tp_First = a;
	pt->tp_End = b;
	pt->tp_Gain = (UWORD) ENV_UNITY;
	pt->tp_Fade = TL_FADE_NONE;
	pt->tp_Pad = 0;
	return TRUE;
}

/* Fade in or Fade out with nothing marked (concept 3.2, F5): the fade at
   In or at Out, as long as the lead or the tail -- In to the music, the
   music to Out, as the suggestion found it; the settings' lengths when
   the music is not between the handles -- its inner end on a zero
   crossing, a fade there before replaced; a step for Undo */
static void end_fade(BOOL out)
{
	const struct TrimPlan *tp = &g_Doc.d_Plan;
	struct TlLevels *lv = &g_Doc.d_Levels;
	struct TraceNums tn;
	ULONG in = trim_in(), end = trim_out(), m = TL_NONE, len, at;
	char secs[12];

	if (end <= in) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	if (tp->tp_Found) m = Tl_Find(&g_Doc.d_Tl, out ? (tp->tp_MusicEnd ? tp->tp_MusicEnd - 1 : 0) : tp->tp_MusicStart);
	if (!out) len = (m != TL_NONE && m > in && m < end) ? m - in : WavePlan_MsToFrames(g_Prefs.ep_LeadMs, g_Doc.d_Rate);
	else len = (m != TL_NONE && m + 1 > in && m + 1 < end) ? end - (m + 1) : WavePlan_MsToFrames(g_Prefs.ep_TailMs, g_Doc.d_Rate);
	if (len > end - in) len = end - in;
	/* the inner end on a zero crossing, as a handle's */
	at = snapped(out ? end - len : in + len);
	if (at < in) at = in;
	if (at > end) at = end;
	len = out ? end - at : at - in;
	if (!len) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	if (out) lv->lv_FadeOut = len;
	else lv->lv_FadeIn = len;
	levels_shown();
	show_trim();
	changed();
	tn.tn_What = (CONST_STRPTR) "endfade out frames";
	tn.tn_N[0] = (ULONG) out;
	tn.tn_N[1] = len;
	tn.tn_Count = 2;
	trace_nums(&tn);
	tenths_text(len, secs);
	say1((CONST_STRPTR) (out ? TXT_ST_ENDFADE_OUT : TXT_ST_ENDFADE_IN), (CONST_STRPTR) secs, 0);
}

/* Effects/Fade in or Fade out (concept 3.2): the part marked faded with
   the settings' shape, its edges on zero crossings as a cut's; the fade a
   step for Undo, a fade there before replaced */
static void fade_part(BOOL out)
{
	struct TraceNums tn;
	struct TextArgs ta;
	struct TlPart pt;
	char ta0[TIME_LEN], ta1[TIME_LEN], said[STATUS_LEN];
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd);

	/* nothing marked: the fade at In or at Out (F5) */
	if (doc_usable() && !g_Reading && (a == WAVEFORM_NONE || b == WAVEFORM_NONE || a == b)) {
		end_fade(out);
		return;
	}
	if (!effect_part(&pt, FALSE)) return;
	a = pt.tp_First;
	b = pt.tp_End;
	pt.tp_Fade = (UBYTE) ((out ? TL_FADE_OUT : TL_FADE_IN) | (g_Prefs.ep_FadeShape & TL_FADE_SHAPE));
	if (!Tl_Fade(&g_Doc.d_Tl, &pt)) {
		set_status((CONST_STRPTR) TXT_ST_CUTFULL);
		return;
	}
	timeline_shown();
	changed();
	tn.tn_What = (CONST_STRPTR) "fade from to out shape";
	tn.tn_N[0] = a;
	tn.tn_N[1] = b;
	tn.tn_N[2] = (ULONG) out;
	tn.tn_N[3] = g_Prefs.ep_FadeShape;
	tn.tn_Count = 4;
	trace_nums(&tn);
	time_text(a, ta0);
	time_text(b, ta1);
	ta.ta_Fmt = (CONST_STRPTR) (out ? TXT_ST_FADED_OUT : TXT_ST_FADED_IN);
	ta.ta_Str[0] = (CONST_STRPTR) ta0;
	ta.ta_Str[1] = (CONST_STRPTR) ta1;
	ta.ta_Str[2] = shape_name(g_Prefs.ep_FadeShape);
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) said, sizeof(said), &ta);
	set_status((CONST_STRPTR) said);
}

/* Select all or Select none: the selection a .. b - 1 (WAVEFORM_NONE:
   none), on the trace as a drag's is */
static void select_range(ULONG a, ULONG b)
{
	struct TraceNums tn;

	if (!doc_usable()) return;
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_SelStart, a, WAVEFORM_SelEnd, b, TAG_DONE);
	tn.tn_What = (CONST_STRPTR) "selected from to";
	tn.tn_N[0] = a;
	tn.tn_N[1] = b;
	tn.tn_Count = 2;
	trace_nums(&tn);
}

/* a question in a window of its own, centred on this one (ask.h: an
   EasyRequest cannot be placed), the file's name in text's %s; the answer
   on the trace, EasyRequest's numbers (0 the rightmost) */
static LONG ask(CONST_STRPTR text, CONST_STRPTR gadgets)
{
	struct AskArgs aa;
	struct TextArgs ta;
	struct TraceNums tn;
	char body[STATUS_LEN * 2];
	LONG r;

	ta.ta_Fmt = text;
	ta.ta_Str[0] = file_name();
	ta.ta_Str[1] = ta.ta_Str[2] = NULL;
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) body, sizeof(body), &ta);
	aa.aa_Over = g_Window;
	aa.aa_Title = (CONST_STRPTR) TXT_TITLE;
	aa.aa_Body = (CONST_STRPTR) body;
	aa.aa_Gadgets = gadgets;
	g_AskBox.Left = g_AskBox.Top = g_AskBox.Width = g_AskBox.Height = 0;
	aa.aa_Opened = &g_AskBox;
	SetAttrs(g_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
	r = Ask_Window(&aa);
	SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
	tn.tn_What = (CONST_STRPTR) "askopened left top width height";
	tn.tn_N[0] = (ULONG) (LONG) g_AskBox.Left;
	tn.tn_N[1] = (ULONG) (LONG) g_AskBox.Top;
	tn.tn_N[2] = (ULONG) (LONG) g_AskBox.Width;
	tn.tn_N[3] = (ULONG) (LONG) g_AskBox.Height;
	tn.tn_Count = 4;
	trace_nums(&tn);
	tn.tn_What = (CONST_STRPTR) "asked answer";
	tn.tn_N[0] = (ULONG) r;
	tn.tn_Count = 1;
	trace_nums(&tn);
	return r;
}

static void do_save(void);
static BOOL g_SaveFailed = FALSE;	/* the last do_save() failed (the doc may be gone too) */

/* before the doc goes (another file, Quit): the user's changes saved,
   dropped, or the step called off -- TRUE: go on */
static BOOL may_leave(void)
{
	LONG r;

	if (!g_HaveDoc || !g_Dirty) return TRUE;
	stop_play();			/* the card is not fed while the question waits */
	r = ask((CONST_STRPTR) TXT_ASK_UNSAVED, (CONST_STRPTR) TXT_ASK_UNSAVED_GADS);
	if (r == 0) return FALSE;	/* Cancel */
	if (r == 2) return TRUE;	/* Discard */
	do_save();
	return (BOOL) (!g_Dirty && !g_SaveFailed);	/* a save that failed: stay */
}

static void open_error(LONG err, CONST_STRPTR path)
{
	CONST_STRPTR name = (CONST_STRPTR) FilePart((STRPTR) path);

	switch (err) {
	/* without the decoder: what it would take */
	case DOC_E_NOTWAV: say1((CONST_STRPTR) (Decode_There() ? TXT_E_NOTWAV : TXT_E_NEEDDSP), name, 0); break;
	case DOC_E_NOTPCM16: say1((CONST_STRPTR) (Decode_There() ? TXT_E_NOTPCM16 : TXT_E_NEEDDSP), name, 0); break;
	case DOC_E_NOMEM: say1((CONST_STRPTR) TXT_E_NOMEM, name, 0); break;
	case DOC_E_EMPTY: say1((CONST_STRPTR) TXT_E_EMPTY, name, 0); break;
	default: say1((CONST_STRPTR) TXT_E_DOS, name, err); break;
	}
}

/* the doc open from path (a WAV, or the work file): reading begins */
static void start_reading(CONST_STRPTR path)
{
	LONG err;

	set_dirty(FALSE);
	if ((err = Doc_Open(&g_Doc, path)) != DOC_OK) {
		Doc_Close(&g_Doc);
		/* no doc and nothing read: the loop must not go on reading */
		g_HaveDoc = FALSE;
		g_Reading = FALSE;
		g_PlanShown = FALSE;
		job_on(FALSE);
		open_error(err, (CONST_STRPTR) g_Source);
		ghost_file(TRUE);
		if (g_Decoded) Decode_Forget();
		g_Decoded = FALSE;
		return;
	}
	g_HaveDoc = TRUE;
	g_Reading = TRUE;
	paint_budget();			/* the reading's paints whole: its loop paints no pieces */
	g_PlanShown = FALSE;
	cue_read();
	job_on(TRUE);
	g_Shown = 0xFFFF;
	DateStamp(&g_ReadStart);
	g_ReadWork = g_ReadWave = g_ReadGauge = 0;
	title_for_doc();
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Source, (ULONG) &g_Doc.d_Source,
		       WAVEFORM_ShowAll, TRUE, WAVEFORM_Cursor, 0, TAG_DONE);
	SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_Source, (ULONG) &g_Doc.d_Source, TAG_DONE);
	sync_views();
	ghost_file(TRUE);
	if (g_Doc.d_Phase == DOC_READ_HEAD) {
		struct TraceNums tn;

		/* a long file: its ends first, the frames they end and begin at */
		tn.tn_What = (CONST_STRPTR) "ends head tail";
		tn.tn_N[0] = g_Doc.d_HeadEnd;
		tn.tn_N[1] = g_Doc.d_TailFrom;
		tn.tn_Count = 2;
		trace_nums(&tn);
	}
}

static void open_file(CONST_STRPTR path)
{
	LONG err;

	if (!may_leave()) return;
	Text_Copy((STRPTR) g_OpenPath, path, DOC_PATHLEN);
	forget_doc();
	Text_Copy((STRPTR) g_Source, (CONST_STRPTR) g_OpenPath, DOC_PATHLEN);
	/* a 16-bit PCM WAV is edited as it is; anything else decoded first */
	err = Doc_Open(&g_Doc, (CONST_STRPTR) g_OpenPath);
	Doc_Close(&g_Doc);
	/* prismadsp.library looked for again: installed since the start */
	if ((err == DOC_E_NOTWAV || err == DOC_E_NOTPCM16) && (Decode_There() || Decode_Open())) {
		if (Decode_Start((CONST_STRPTR) g_OpenPath) == DECODE_RUNNING) {
			g_Decoding = TRUE;
			job_on(TRUE);
			say1((CONST_STRPTR) TXT_ST_DECODING, file_name(), 0);
			ghost_file(TRUE);
			return;
		}
		say1((CONST_STRPTR) TXT_ST_DECODE_FAILED, file_name(), Decode_Error());
		ghost_file(TRUE);
		return;
	}
	start_reading((CONST_STRPTR) g_OpenPath);
}

/* the job's progress, and its end: the work file read as the doc */
static void decode_signals(void)
{
	UWORD r = Decode_Poll(), pc;

	if (r == DECODE_RUNNING) {
		/* the gauge too, as reading and saving have it */
		pc = Decode_Percent();
		set1(g_Gauge, FUELGAUGE_Level, pc);
		job_said((CONST_STRPTR) TXT_ST_DECODING, pc);
		return;
	}
	g_Decoding = FALSE;
	job_on(FALSE);
	set1(g_Gauge, FUELGAUGE_Level, 0);
	if (r == DECODE_FAILED) {
		say1((CONST_STRPTR) TXT_ST_DECODE_FAILED, file_name(), Decode_Error());
		return;
	}
	g_Decoded = TRUE;
	start_reading(Decode_Work());
}

/* ticks (1/50 s) from a to b */
static ULONG ticks_between(const struct DateStamp *a, const struct DateStamp *b)
{
	return (ULONG) (((b->ds_Days - a->ds_Days) * 1440L + b->ds_Minute - a->ds_Minute) * (60L * TICKS_PER_SECOND) +
			b->ds_Tick - a->ds_Tick);
}

/* one end of the suggestion as the status line says it: its seconds and
   its clicks ("4.1 s, 8 clicks"), or the run-out's period ("6.3 s, a
   click every 1.8 s"); into buf (PART_LEN) */
#define PART_LEN 64
struct Part {
	CONST_STRPTR pt_Secs;
	CONST_STRPTR pt_Period;		/* NULL: the clicks counted */
	LONG pt_Clicks;
};

static void part_text(const struct Part *pt, char *buf)
{
	struct TextArgs ta;
	char clicks[32];

	ta.ta_Fmt = (CONST_STRPTR) (pt->pt_Clicks == 0 ? TXT_CLICKS_NONE : pt->pt_Clicks == 1 ? TXT_CLICKS_ONE : TXT_CLICKS_MANY);
	ta.ta_Num[0] = pt->pt_Clicks;
	ta.ta_Num[1] = ta.ta_Num[2] = 0;
	ta.ta_Str[0] = ta.ta_Str[1] = ta.ta_Str[2] = NULL;
	Text_Fmt((STRPTR) clicks, sizeof(clicks), &ta);
	ta.ta_Fmt = (CONST_STRPTR) (pt->pt_Period ? TXT_ST_PART_PERIOD : TXT_ST_PART);
	ta.ta_Num[0] = 0;
	ta.ta_Str[0] = pt->pt_Secs;
	ta.ta_Str[1] = pt->pt_Period ? pt->pt_Period : (CONST_STRPTR) clicks;
	Text_Fmt((STRPTR) buf, PART_LEN, &ta);
}

/* the suggestion into the gadgets, said in the status line: at the
   reading's end, or a long file's once its ends are read */
static void show_plan(void)
{
	struct TrimPlan *tp = &g_Doc.d_Plan;
	char lead[16], tail[16], period[16], leadpart[PART_LEN], tailpart[PART_LEN];
	struct TraceNums tn;
	struct DateStamp now;
	struct Say sy;
	struct Part pt;
	STRPTR p;

	g_PlanShown = TRUE;
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Source, (ULONG) &g_Doc.d_Source,
		       WAVEFORM_TrimIn, tp->tp_In, WAVEFORM_TrimOut, tp->tp_Out, WAVEFORM_Cursor, tp->tp_In, TAG_DONE);
	SetGadgetAttrs((struct Gadget *) g_Over, g_Window, NULL, WAVEFORM_Source, (ULONG) &g_Doc.d_Source, TAG_DONE);
	regions_shown();
	show_trim();
	sync_views();
	/* the history from here: the suggestion is no change of the user's */
	history_start();
	ghost_file(FALSE);
	trace_trim((CONST_STRPTR) "shown in out top zoom");
	/* where the first column is in the gadget: the tests' pointer aims
	   by it (the bevel's width is the system's) */
	tn.tn_What = (CONST_STRPTR) "firstcol";
	tn.tn_N[0] = (ULONG) Waveform_ColumnOf(g_Wave, get(g_Wave, WAVEFORM_Top));
	tn.tn_Count = 1;
	trace_nums(&tn);
	/* and the overview's last column: Out's line at the end */
	tn.tn_What = (CONST_STRPTR) "overlast";
	tn.tn_N[0] = (ULONG) Waveform_ColumnOf(g_Over, Doc_Length(&g_Doc) - 1);
	trace_nums(&tn);
	/* how long the suggestion took, in ticks (1/50 s) */
	DateStamp(&now);
	tn.tn_What = (CONST_STRPTR) "planned ticks";
	tn.tn_N[0] = ticks_between(&g_ReadStart, &now);
	tn.tn_Count = 1;
	trace_nums(&tn);
	tn.tn_What = (CONST_STRPTR) "plan in out leadclicks tailclicks";
	tn.tn_N[0] = tp->tp_In;
	tn.tn_N[1] = tp->tp_Out;
	tn.tn_N[2] = tp->tp_LeadClicks;
	tn.tn_N[3] = tp->tp_TailClicks;
	tn.tn_Count = 4;
	trace_nums(&tn);
	if (!tp->tp_Found) {
		set_status((CONST_STRPTR) TXT_ST_NOMUSIC);
		return;
	}
	tenths_text(tp->tp_MusicStart, lead);
	tenths_text(g_Doc.d_Frames - tp->tp_MusicEnd, tail);
	pt.pt_Secs = (CONST_STRPTR) lead;
	pt.pt_Clicks = tp->tp_LeadClicks;
	pt.pt_Period = NULL;
	part_text(&pt, leadpart);
	if (tp->tp_PeriodMs) {
		/* the period as seconds and a tenth */
		p = Text_PutNumber((STRPTR) period, tp->tp_PeriodMs / 1000);
		p = Text_Put(p, (CONST_STRPTR) ".");
		Text_PutNumber(p, tp->tp_PeriodMs % 1000 / 100);
		pt.pt_Period = (CONST_STRPTR) period;
	}
	pt.pt_Secs = (CONST_STRPTR) tail;
	pt.pt_Clicks = tp->tp_TailClicks;
	part_text(&pt, tailpart);
	sy.sy_Fmt = (CONST_STRPTR) TXT_ST_FOUND;
	sy.sy_S0 = (CONST_STRPTR) leadpart;
	sy.sy_S1 = (CONST_STRPTR) tailpart;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

/* the whole file read: the suggestion shown, if it is not yet (the
   handles are the user's once it is), the picture complete */
static void reading_done(void)
{
	struct TraceNums tn;
	struct DateStamp now;

	g_Reading = FALSE;
	paint_budget();		/* what the suggestion changes filled in by pieces (whole: an A1200's 1.2 s) */
	job_on(FALSE);
	set1(g_Gauge, FUELGAUGE_Level, 0);
	if (!g_PlanShown) {
		show_plan();
	} else {
		set1(g_Wave, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
		set1(g_Over, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
		ghost_file(FALSE);
	}
	/* how long the reading took, in ticks (1/50 s) */
	DateStamp(&now);
	tn.tn_What = (CONST_STRPTR) "read frames ticks";
	tn.tn_N[0] = g_Doc.d_Frames;
	tn.tn_N[1] = ticks_between(&g_ReadStart, &now);
	tn.tn_Count = 2;
	trace_nums(&tn);
	/* of them inside Doc_Read(): the rest is the window's */
	tn.tn_What = (CONST_STRPTR) "readwork ticks";
	tn.tn_N[0] = g_ReadWork;
	tn.tn_Count = 1;
	trace_nums(&tn);
	/* the per-cent shows: the waveforms set again, the gauge and its text */
	tn.tn_What = (CONST_STRPTR) "readshow wave gauge";
	tn.tn_N[0] = g_ReadWave;
	tn.tn_N[1] = g_ReadGauge;
	tn.tn_Count = 2;
	trace_nums(&tn);
}

/* a few pieces more; every per cent the gadgets are told */
static void read_some(void)
{
	struct DateStamp a, b;
	UWORD i, pc;
	BOOL more = TRUE;

	/* the reading's own work timed apart from the window's (the trace) */
	DateStamp(&a);
	for (i = 0; i < READ_PIECES && more; i++) more = Doc_Read(&g_Doc);
	DateStamp(&b);
	g_ReadWork += ticks_between(&a, &b);
	if (!more) {
		reading_done();
		return;
	}
	if (g_Doc.d_PlanReady && !g_PlanShown) show_plan();
	if ((pc = Doc_Percent(&g_Doc)) != g_Shown) {
		g_Shown = pc;
		set1(g_Wave, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
		set1(g_Over, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
		DateStamp(&a);
		g_ReadWave += ticks_between(&b, &a);
		set1(g_Gauge, FUELGAUGE_Level, pc);
		/* the suggestion said stays: the gauge alone goes on */
		if (!g_PlanShown) job_said((CONST_STRPTR) TXT_ST_READING, pc);
		DateStamp(&b);
		g_ReadGauge += ticks_between(&a, &b);
	}
}

static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

/* a new file's icon (Settings, Create icons?): the system's for a WAV
   (ENV:Sys/def_wav.info), else its project icon, without a place of its
   own -- Workbench finds it one (Style Guide); a file that has an icon
   keeps it. The trace says what came of it */
static void make_icon(CONST_STRPTR path)
{
	struct DiskObject *dob;
	struct TraceNums tn;
	char info[DOC_PATHLEN + 6];
	ULONG made = 0;

	if (!g_Prefs.ep_Icons || !IconBase) return;
	Text_Copy((STRPTR) info, path, DOC_PATHLEN);
	Text_Put((STRPTR) info + Text_Length((CONST_STRPTR) info), (CONST_STRPTR) ".info");
	{
		BPTR lock = Lock((STRPTR) info, SHARED_LOCK);

		if (lock) {
			UnLock(lock);
			made = 2;			/* one there: kept */
		}
	}
	if (!made && (dob = GetIconTags(NULL, ICONGETA_GetDefaultName, (ULONG) "wav", ICONGETA_GetDefaultType, WBPROJECT,
				       TAG_DONE))) {
		dob->do_CurrentX = NO_ICON_POSITION;
		dob->do_CurrentY = NO_ICON_POSITION;
		if (PutIconTags((STRPTR) path, dob, ICONPUTA_NotifyWorkbench, TRUE, TAG_DONE)) made = 1;
		FreeDiskObject(dob);
	}
	tn.tn_What = (CONST_STRPTR) "icon made";
	tn.tn_N[0] = made;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* PrismaRec's level map (<name>.prl) cut as the take was (mapcut.h);
   the trace says how */
static void cut_map(const struct MapCut *mc)
{
	struct TraceNums tn;

	tn.tn_What = (CONST_STRPTR) "mapcut";
	tn.tn_N[0] = MapCut_Cut(mc);
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* what a job's progress says: saving, or measuring for Find tracks */
static CONST_STRPTR g_ProgressText = NULL;	/* NULL: saving */

/* a save's progress (doc.h): the gauge and the status line at once --
   the save holds the loop; the trace gets the quarters */
static BOOL save_progress(UWORD pc)
{
	struct TraceNums tn;
	ULONG r;
	UWORD code;

	set1(g_Gauge, FUELGAUGE_Level, pc);
	job_said(g_ProgressText ? g_ProgressText : (CONST_STRPTR) TXT_ST_SAVING, pc);
	if (pc % 25 == 0) {
		tn.tn_What = (CONST_STRPTR) "saving percent";
		tn.tn_N[0] = pc;
		tn.tn_Count = 1;
		trace_nums(&tn);
	}
	if (g_SlowSave) Delay(g_SlowSave);	/* the tests: time to press Cancel */
	/* the window's input while it copies: Cancel or Esc calls it off,
	   the rest waits (the busy pointer says so) */
	while ((r = DoMethod(g_WinObj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
		if (((r & WMHI_CLASSMASK) == WMHI_GADGETUP && (r & WMHI_GADGETMASK) == GID_CANCEL) ||
		    ((r & WMHI_CLASSMASK) == WMHI_RAWKEY && (r & WMHI_KEYMASK) == 0x45))
			g_CancelSave = TRUE;
	}
	if (g_CancelSave) trace((CONST_STRPTR) "cancelled saving");
	return (BOOL) !g_CancelSave;
}

/* a save's error said: the DOS error, or what DOC_E_EMPTY means */
static void save_error(LONG err)
{
	if (err == DOC_E_EMPTY) set_status((CONST_STRPTR) TXT_E_SAVE_EMPTY);
	else say1((CONST_STRPTR) TXT_E_SAVE, NULL, err);
}

/* the view a save kept: its first frame (the saved file's) and zoom */
struct KeptView {
	ULONG vw_Top, vw_Zoom;
};

/* the file saved in place is the doc already (Doc_Save kept it, its
   picture made while it was written): shown as a reading's end shows a
   file, without reading it again -- the suggestion made anew, the
   markers its cue sheet's, the view where it was */
static void doc_kept(const struct KeptView *vw)
{
	struct TraceNums tn;

	set_dirty(FALSE);
	g_Reading = FALSE;
	g_PlanShown = FALSE;
	job_on(g_Decoding);
	cue_read();
	DateStamp(&g_ReadStart);
	title_for_doc();
	show_plan();
	if (vw->vw_Zoom) {
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Zoom, vw->vw_Zoom, WAVEFORM_Top, vw->vw_Top,
			       TAG_DONE);
		sync_views();
	}
	tn.tn_What = (CONST_STRPTR) "keptsave frames top zoom";
	tn.tn_N[0] = g_Doc.d_Frames;
	tn.tn_N[1] = get(g_Wave, WAVEFORM_Top);
	tn.tn_N[2] = get(g_Wave, WAVEFORM_Zoom);
	tn.tn_Count = 3;
	trace_nums(&tn);
}

static void do_save(void)
{
	ULONG in = get(g_Wave, WAVEFORM_TrimIn), out = get(g_Wave, WAVEFORM_TrimOut), top, zoom;
	struct MapCut mc;
	struct CueOut co;
	struct KeptView vw;
	char len[TIME_LEN];
	BOOL kept;
	LONG err;

	g_SaveFailed = FALSE;
	if (!doc_usable()) return;
	/* not over the user's file (another format): a new WAV, named first */
	if (g_Decoded) {
		do_save_as();
		return;
	}
	if (in == WAVEFORM_NONE) in = 0;
	if (out == WAVEFORM_NONE) out = Doc_Length(&g_Doc);
	stop_play();			/* its handle on the file closed first */
	prefetch_off();			/* and the read-ahead's: an open file cannot be deleted */
	g_CancelSave = FALSE;
	job_on(TRUE);
	job_said((CONST_STRPTR) TXT_ST_SAVING, 0);
	g_Doc.d_SaveProgress = save_progress;
	/* no busy pointer: window.class's opens a requester that blocks every
	   gadget, Cancel with them -- the gauge shows the save, and the rest
	   of the window's input is dropped while it copies (save_progress) */
	time_text(out - in, len);
	/* the gadgets let go first: a save that works closes the doc */
	top = get(g_Wave, WAVEFORM_Top);
	zoom = get(g_Wave, WAVEFORM_Zoom);
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Source, 0, WAVEFORM_Regions, 0,
		       WAVEFORM_RegionCount, 0, WAVEFORM_Markers, 0, WAVEFORM_MarkerCount, 0, WAVEFORM_LevelCount, 0,
		       WAVEFORM_FadeIn, 0, WAVEFORM_FadeOut, 0, WAVEFORM_Samples, 0, TAG_DONE);
	set1(g_Over, WAVEFORM_Source, 0);
	trace_save_pieces();
	clear_bytes((UBYTE *) &mc, sizeof(mc));
	mc.mc_From = mc.mc_To = (CONST_STRPTR) g_OpenPath;
	mc.mc_Backup = g_Prefs.ep_Backup;	/* the old map beside the old take */
	co.co_Sound = (CONST_STRPTR) g_OpenPath;
	co.co_In = in;
	co.co_Out = out;
	co.co_Rate = g_Doc.d_Rate;		/* the doc is closed after a save */
	co.co_Backup = g_Prefs.ep_Backup;
	mc.mc_In = in;
	mc.mc_Out = out;
	mc.mc_Rate = g_Doc.d_Rate;		/* the doc is closed after a save */
	mc.mc_Tl = NULL;
	mc.mc_Levels = NULL;
	levels_now();				/* the end fades at In and Out as saved */
	g_SaveLv = g_Doc.d_Levels;		/* the end fades and dips, as the timeline */
	if (!Tl_Whole(&g_Doc.d_Tl, g_Doc.d_Frames) || Tl_Levels(&g_SaveLv)) {
		g_SaveTl = g_Doc.d_Tl;		/* and its timeline with it */
		mc.mc_Tl = &g_SaveTl;
		mc.mc_Levels = Tl_Levels(&g_SaveLv);
	}
	err = Doc_Save(&g_Doc, in, out);
	kept = (BOOL) (err == DOC_KEPT);
	if (kept) err = 0;
	set1(g_Gauge, FUELGAUGE_Level, 0);
	job_on((BOOL) (g_Reading || g_Decoding));
	{
		struct TraceNums tn;

		tn.tn_What = (CONST_STRPTR) "saved in out err";
		tn.tn_N[0] = in;
		tn.tn_N[1] = out;
		tn.tn_N[2] = (ULONG) err;
		tn.tn_Count = 3;
		trace_nums(&tn);
	}
	if (err && g_Doc.d_FH) {
		/* nothing changed: the doc as it was, the handles where the user
		   left them, the view too */
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_Source, (ULONG) &g_Doc.d_Source,
			       WAVEFORM_Zoom, zoom, WAVEFORM_Top, top, TAG_DONE);
		set1(g_Over, WAVEFORM_Source, (ULONG) &g_Doc.d_Source);
		regions_shown();
		sync_views();
		trace_trim((CONST_STRPTR) "kept in out top zoom");
		if (err == DOC_E_CANCELLED) say1((CONST_STRPTR) TXT_ST_SAVE_CANCELLED, file_name(), 0);
		else save_error(err);
		g_SaveFailed = TRUE;
		return;
	}
	if (err) {
		/* the doc gone: the file as it was but not to be opened again, or
		   the trimmed one left under the temporary name */
		forget_doc();
		ghost_file(TRUE);
		if (err == DOC_E_KEPT) {
			struct Say sy;

			/* where the sound is now, and the original */
			sy.sy_Fmt = (CONST_STRPTR) (g_Prefs.ep_Backup ? TXT_E_KEPT_BAK : TXT_E_KEPT);
			sy.sy_S0 = (CONST_STRPTR) FilePart((STRPTR) Doc_LastTemp());
			sy.sy_S1 = (CONST_STRPTR) FilePart((STRPTR) Doc_LastBackup());
			sy.sy_N0 = sy.sy_N1 = 0;
			say(&sy);
		} else {
			save_error(err);
		}
		g_SaveFailed = TRUE;	/* the doc gone: Quit and Open stay, the message read */
		return;
	}
	/* the trimmed file the doc: kept (its picture made as it was
	   written), else opened again and read anew */
	if (!kept) g_HaveDoc = FALSE;
	/* the selection too: its frames were the old timeline's */
	SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_TrimIn, WAVEFORM_NONE,
		       WAVEFORM_TrimOut, WAVEFORM_NONE, WAVEFORM_SelStart, WAVEFORM_NONE, WAVEFORM_SelEnd, WAVEFORM_NONE,
		       TAG_DONE);
	set1(g_Over, WAVEFORM_TrimIn, WAVEFORM_NONE);
	set1(g_Over, WAVEFORM_TrimOut, WAVEFORM_NONE);
	cut_map(&mc);
	cue_write(&co);				/* read back as the file is read anew */
	make_icon((CONST_STRPTR) g_OpenPath);		/* one it had is kept */
	if (kept) {
		/* the view where it was: its first frame moved by the part cut
		   off before In */
		vw.vw_Zoom = zoom;
		vw.vw_Top = top > in ? top - in : 0;
		doc_kept(&vw);
	} else {
		start_reading((CONST_STRPTR) g_OpenPath);
	}
	if (g_HaveDoc) say1((CONST_STRPTR) TXT_ST_SAVED, (CONST_STRPTR) len, 0);
}

/* two names for one file (a name not there is no file's) */
static BOOL same_file(CONST_STRPTR a, CONST_STRPTR b)
{
	BPTR la, lb;
	BOOL same = FALSE;

	if (!(la = Lock((STRPTR) a, SHARED_LOCK))) return FALSE;
	if ((lb = Lock((STRPTR) b, SHARED_LOCK))) {
		same = (BOOL) (SameLock(la, lb) == LOCK_SAME);
		UnLock(lb);
	}
	UnLock(la);
	return same;
}

/* the name a Save as starts from: the file's own -- a decoded file's
   <name>-trim.wav beside it (it is no WAV) */
#define SAVEAS_NAMELEN	108
static void save_as_name(char *name)
{
	STRPTR p, dot = NULL;

	Text_Copy((STRPTR) name, (CONST_STRPTR) FilePart((STRPTR) g_Source), SAVEAS_NAMELEN - 10);
	if (!g_Decoded) return;
	for (p = (STRPTR) name; *p; p++) {
		if (*p == '.') dot = p;
	}
	Text_Put(dot ? dot : p, (CONST_STRPTR) "-trim.wav");
}

/* the timeline from in to out into path, a new file (Doc_SaveAs), the
   level map cut beside it, an icon: 0, or the error said */
struct SaveCopy {
	ULONG sc_In, sc_Out;
	CONST_STRPTR sc_Path;
};

static LONG save_copy(const struct SaveCopy *sc)
{
	struct DocSave ds;
	struct MapCut mc;
	struct CueOut co;
	struct TraceNums tn;
	char line[DOC_PATHLEN + 16];	/* "savedasname " and a name of up to 255 */
	LONG err;

	prefetch_off();			/* its handle on the file let go */
	g_CancelSave = FALSE;
	job_on(TRUE);
	job_said((CONST_STRPTR) TXT_ST_SAVING, 0);
	g_Doc.d_SaveProgress = save_progress;
	trace_save_pieces();
	Text_Put(Text_Put((STRPTR) line, (CONST_STRPTR) "savedasname "), (CONST_STRPTR) FilePart((STRPTR) sc->sc_Path));
	trace((CONST_STRPTR) line);
	ds.ds_In = sc->sc_In;
	ds.ds_Out = sc->sc_Out;
	ds.ds_Path = sc->sc_Path;
	err = Doc_SaveAs(&g_Doc, &ds);
	set1(g_Gauge, FUELGAUGE_Level, 0);
	job_on((BOOL) (g_Reading || g_Decoding));
	changed();			/* a handle dragged while it copied: its step */
	tn.tn_What = (CONST_STRPTR) "savedas in out err";
	tn.tn_N[0] = sc->sc_In;
	tn.tn_N[1] = sc->sc_Out;
	tn.tn_N[2] = (ULONG) err;
	tn.tn_Count = 3;
	trace_nums(&tn);
	if (err == DOC_E_CANCELLED) {
		say1((CONST_STRPTR) TXT_ST_SAVE_CANCELLED, file_name(), 0);
		return err;
	}
	if (err) {
		save_error(err);
		return err;
	}
	/* PrismaRec's level map beside the original, cut beside the new file */
	clear_bytes((UBYTE *) &mc, sizeof(mc));
	mc.mc_From = (CONST_STRPTR) g_Source;
	mc.mc_To = sc->sc_Path;
	mc.mc_In = sc->sc_In;
	mc.mc_Out = sc->sc_Out;
	mc.mc_Rate = g_Doc.d_Rate;
	mc.mc_Levels = Tl_Levels(&g_Doc.d_Levels);
	mc.mc_Tl = (Tl_Whole(&g_Doc.d_Tl, g_Doc.d_Frames) && !mc.mc_Levels) ? NULL : &g_Doc.d_Tl;
	cut_map(&mc);
	co.co_Sound = sc->sc_Path;
	co.co_In = sc->sc_In;
	co.co_Out = sc->sc_Out;
	co.co_Rate = g_Doc.d_Rate;
	co.co_Backup = FALSE;
	cue_write(&co);
	make_icon(sc->sc_Path);
	return 0;
}

/* Project, Save as...: the timeline from In to Out into a file the user
   names in a requester, the file's drawer and name filled in; then the
   window works on the new file, read anew -- the original as it was.
   The file's own name: Save. A name that is there: asked first */
static void do_save_as(void)
{
	ULONG in = get(g_Wave, WAVEFORM_TrimIn), out = get(g_Wave, WAVEFORM_TrimOut);
	struct FileRequester *fr;
	struct SaveCopy sc;
	struct Say sy;
	char path[DOC_PATHLEN], dir[DOC_PATHLEN], name[SAVEAS_NAMELEN], len[TIME_LEN];
	BOOL got = FALSE;

	if (!doc_usable() || !AslBase) return;
	if (in == WAVEFORM_NONE) in = 0;
	if (out == WAVEFORM_NONE) out = Doc_Length(&g_Doc);
	Text_Copy((STRPTR) dir, (CONST_STRPTR) g_Source, DOC_PATHLEN);
	*PathPart((STRPTR) dir) = '\0';
	save_as_name(name);
	stop_play();
	if (!(fr = (struct FileRequester *) AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (ULONG) g_Window,
								 ASLFR_TitleText, (ULONG) TXT_SAVEAS_TITLE,
								 ASLFR_InitialDrawer, (ULONG) dir, ASLFR_InitialFile, (ULONG) name,
								 ASLFR_DoSaveMode, TRUE, ASLFR_RejectIcons, TRUE, TAG_DONE)))
		return;
	SetAttrs(g_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
	if (AslRequest(fr, NULL) && fr->fr_File && fr->fr_File[0]) {
		Text_Copy((STRPTR) path, (CONST_STRPTR) fr->fr_Drawer, DOC_PATHLEN);
		got = (BOOL) (AddPart((STRPTR) path, (STRPTR) fr->fr_File, DOC_PATHLEN) != 0);
	}
	SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
	FreeAslRequest(fr);
	if (!got) return;
	if (!g_Decoded && same_file((CONST_STRPTR) path, (CONST_STRPTR) g_OpenPath)) {
		do_save();
		return;
	}
	if (there((CONST_STRPTR) path) && ask((CONST_STRPTR) TXT_ASK_REPLACE, (CONST_STRPTR) TXT_ASK_REPLACE_GADS) != 1) return;
	sc.sc_In = in;
	sc.sc_Out = out;
	sc.sc_Path = (CONST_STRPTR) path;
	if (save_copy(&sc)) return;
	time_text(out - in, len);
	/* the new file the window's: read anew, nothing left to ask */
	set_dirty(FALSE);
	open_file((CONST_STRPTR) path);
	sy.sy_Fmt = (CONST_STRPTR) TXT_ST_SAVED_AS;
	sy.sy_S0 = (CONST_STRPTR) FilePart((STRPTR) path);
	sy.sy_S1 = (CONST_STRPTR) len;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

/* a name with a sound file's suffix (and not a work file) */
static BOOL sound_name(CONST_STRPTR name, UWORD n)
{
	static const char *const suffix[] = {
		".wav", ".aif", ".aiff", ".aifc", ".au", ".snd", ".8svx", ".16sv", ".maud", ".flac",
		".mp3", ".mp2", ".ogg", ".m4a", ".aac", ".wma", NULL
	};
	UWORD i, k;

	k = (UWORD) Text_Length((CONST_STRPTR) DECODE_SUFFIX);
	if (n > k && Stricmp((STRPTR) name + n - k, (STRPTR) DECODE_SUFFIX) == 0) return FALSE;
	for (i = 0; suffix[i]; i++) {
		k = (UWORD) Text_Length((CONST_STRPTR) suffix[i]);
		if (n > k && Stricmp((STRPTR) name + n - k, (STRPTR) suffix[i]) == 0) return TRUE;
	}
	return FALSE;
}

/* the drawer's next sound file after the open one, by name */
static void do_next(void)
{
	struct FileInfoBlock *fib;
	char dir[DOC_PATHLEN], best[108], path[DOC_PATHLEN];
	CONST_STRPTR cur;
	BPTR lock;
	UWORD n;
	BOOL found = FALSE;

	if (!g_HaveDoc) return;
	Text_Copy((STRPTR) dir, (CONST_STRPTR) g_Source, DOC_PATHLEN);
	*PathPart((STRPTR) dir) = '\0';
	cur = (CONST_STRPTR) FilePart((STRPTR) g_Source);
	best[0] = '\0';
	if (!(fib = AllocDosObject(DOS_FIB, NULL))) return;
	if ((lock = Lock((STRPTR) dir, SHARED_LOCK))) {
		if (Examine(lock, fib)) {
			while (ExNext(lock, fib)) {
				if (fib->fib_DirEntryType > 0) continue;
				n = (UWORD) Text_Length((CONST_STRPTR) fib->fib_FileName);
				if (!sound_name((CONST_STRPTR) fib->fib_FileName, n)) continue;
				if (Stricmp((STRPTR) fib->fib_FileName, (STRPTR) cur) <= 0) continue;
				if (found && Stricmp((STRPTR) fib->fib_FileName, (STRPTR) best) >= 0) continue;
				Text_Copy((STRPTR) best, (CONST_STRPTR) fib->fib_FileName, sizeof(best));
				found = TRUE;
			}
		}
		UnLock(lock);
	}
	FreeDosObject(DOS_FIB, fib);
	if (!found) {
		set_status((CONST_STRPTR) TXT_ST_NONEXT);
		return;
	}
	Text_Copy((STRPTR) path, (CONST_STRPTR) dir, DOC_PATHLEN);
	if (AddPart((STRPTR) path, (STRPTR) best, DOC_PATHLEN)) open_file((CONST_STRPTR) path);
}

static void do_open(void)
{
	struct FileRequester *fr;
	char path[DOC_PATHLEN], dir[DOC_PATHLEN];

	if (!AslBase) return;
	dir[0] = '\0';
	if (g_Source[0]) {
		Text_Copy((STRPTR) dir, (CONST_STRPTR) g_Source, DOC_PATHLEN);
		*PathPart((STRPTR) dir) = '\0';
	}
	if (!(fr = (struct FileRequester *) AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (ULONG) g_Window,
								 ASLFR_TitleText, (ULONG) TXT_OPEN_TITLE,
								 ASLFR_InitialDrawer, (ULONG) dir,
								 ASLFR_DoPatterns, TRUE,
								 ASLFR_InitialPattern,
								 (ULONG) "#?.(wav|aif|aiff|aifc|au|snd|8svx|16sv|maud|flac|mp3|mp2|ogg|m4a|aac|wma)",
								 ASLFR_RejectIcons, TRUE, TAG_DONE)))
		return;
	SetAttrs(g_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
	if (AslRequest(fr, NULL) && fr->fr_File && fr->fr_File[0]) {
		Text_Copy((STRPTR) path, (CONST_STRPTR) fr->fr_Drawer, DOC_PATHLEN);
		if (AddPart((STRPTR) path, (STRPTR) fr->fr_File, DOC_PATHLEN)) {
			SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
			FreeAslRequest(fr);
			open_file((CONST_STRPTR) path);
			return;
		}
	}
	SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
	FreeAslRequest(fr);
}

/* Mason's picture beside the version and the credits (about.h): a
   window of its own, the play and the painting going on meanwhile */
static void do_about(void)
{
	About_Open(g_Window);
	trace((CONST_STRPTR) (About_Signal() ? "about open" : "about none"));
}

/* --- help -------------------------------------------------------------------------------- */

/* the pointer over this gadget's box (a layout's: its row) */
static BOOL under(Object *o)
{
	struct Gadget *g = (struct Gadget *) o;
	WORD x, y;

	if (!o || !g_Window) return FALSE;
	x = g_Window->MouseX;
	y = g_Window->MouseY;
	return (BOOL) (x >= g->LeftEdge && x < g->LeftEdge + g->Width && y >= g->TopEdge && y < g->TopEdge + g->Height);
}

/* the page about what the pointer is over: every name a node of every
   guide (tools/check_helpnodes.py) */
static CONST_STRPTR help_node(void)
{
	if (under(g_Over) || under(g_Wave) || under(g_Scroll) || under(g_Lens[0]) || under(g_Lens[1]) || under(g_Lens[2]))
		return HELP_NODE("Waveform");
	if (under(g_Deck) || under(g_LoopKey)) return HELP_NODE("Listening");
	if (under(g_TrimBtn) || under(g_Flag[0]) || under(g_Flag[1])) return HELP_NODE("Trimming");
	if (under(g_Len)) return HELP_NODE("Trimming");
	if (under(g_Gauge) || under(g_Cancel)) return HELP_NODE("Window");
	return HELP_NODE("Main");
}

/* the manual at that page, on the window's screen; none installed: said */
static void show_help(CONST_STRPTR node)
{
	char line[HELP_NODE_MAX + 16];
	BOOL shown;

	g_Help.hc_Screen = g_Window ? g_Window->WScreen : NULL;
	shown = Help_Show(&g_Help, node);
	if (!shown) set_status((CONST_STRPTR) TXT_ST_NOHELP);
	if (g_TracePath[0]) {
		Text_Put(Text_Put(Text_Put((STRPTR) line, (CONST_STRPTR) "helpnode "), node), (CONST_STRPTR) (shown ? " 1" : " 0"));
		trace((CONST_STRPTR) line);
	}
}

/* --- listening ------------------------------------------------------------------------- */

/* the frames first .. end - 1 on the card, the play head shown */
/* the span playing, for the status line when it has played out */
static ULONG g_PlayFirst = 0, g_PlayEnd = 0;
/* g_PlaySaid (above stop_play): "Playing ..." is what the status says */
/* Play, Loop?: what played out is played again -- the selection, In to
   Out, a Listen's three seconds: what was asked for, not where a seek
   or a pause went on from */
static ULONG g_LoopFirst = 0, g_LoopEnd = 0;

static void say_span(CONST_STRPTR fmt, ULONG first, ULONG end)
{
	char a[TIME_LEN], b[TIME_LEN];
	struct Say sy;

	time_text(first, a);
	time_text(end, b);
	sy.sy_Fmt = fmt;
	sy.sy_S0 = (CONST_STRPTR) a;
	sy.sy_S1 = (CONST_STRPTR) b;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

static void play_span(ULONG first, ULONG end)
{
	struct PlaySpan ps;
	UWORD r;

	if (!doc_usable()) return;
	levels_now();			/* the end fades where the handles are now */
	ps.ps_First = first;
	ps.ps_End = end;
	switch ((r = Play_Start(&g_Doc, &ps))) {
	case PLAY_OK:
		g_PlayFirst = first;
		g_PlayEnd = end;
		{
			struct TraceNums tn;

			tn.tn_What = (CONST_STRPTR) "play first end";
			tn.tn_N[0] = first;
			tn.tn_N[1] = end;
			tn.tn_Count = 2;
			trace_nums(&tn);
		}
		{
			struct TraceNums tn;

			tn.tn_What = (CONST_STRPTR) "playthrough ahi";
			tn.tn_N[0] = Play_Through();
			tn.tn_Count = 1;
			trace_nums(&tn);
		}
		/* Automatic with the card in use: through AHI, said so */
		if (Play_Through() == PLAY_THROUGH_AHI && Play_Owner()[0]) {
			/* prismadsp's encoding job (not its decoder, named apart):
			   it turns the card's input down to nothing while it runs
			   (job.c) */
			CONST_STRPTR o = Play_Owner();

			say1((CONST_STRPTR) (Text_Prefix(&o, (CONST_STRPTR) "prismadsp encoder") && !*o ? TXT_ST_PLAY_AHIJOB
					     : TXT_ST_PLAY_AHIBUSY),
			     Play_Owner(), 0);
		}
		else say_span((CONST_STRPTR) TXT_ST_PLAYING, first, end);
		g_PlaySaid = TRUE;
		g_Paused = FALSE;
		play_label();
		if (g_HaveClock) {
			g_Due = Clock_Ms() + HEAD_MS;
			g_Beats = 0;
			g_BeatsFrom = g_Looked = Clock_Ms();
			g_Deaf = 0;
			Clock_Wake(HEAD_MS);	/* one at a time: a running one stays */
		}
		break;
	case PLAY_BUSY: say1((CONST_STRPTR) TXT_ST_PLAY_BUSY, Play_Owner(), 0); break;
	case PLAY_NOCARD: set_status((CONST_STRPTR) TXT_ST_PLAY_NOCARD); break;
	case PLAY_NOLIB: set_status((CONST_STRPTR) TXT_ST_PLAY_NOLIB); break;
	case AHIPLAY_NODEVICE: set_status((CONST_STRPTR) TXT_ST_PLAY_NOAHI); break;
	case AHIPLAY_NOMODE: set_status((CONST_STRPTR) TXT_ST_PLAY_AHIMODE); break;
	case AHIPLAY_NOMEMORY: case PLAY_NOPLAYER: set_status((CONST_STRPTR) TXT_ST_PLAY_NOMEM); break;
	default: say1((CONST_STRPTR) TXT_ST_PLAY_ERR, NULL, r); break;
	}
	if (r != PLAY_OK) {
		/* Play_Start stopped what played before: nothing plays now */
		g_PlaySaid = FALSE;
		head_off();
		play_label();
	}
}

static ULONG trim_in(void)
{
	ULONG f = get(g_Wave, WAVEFORM_TrimIn);

	return f == WAVEFORM_NONE ? 0 : f;
}

static ULONG trim_out(void)
{
	ULONG f = get(g_Wave, WAVEFORM_TrimOut);

	return f == WAVEFORM_NONE ? Doc_Length(&g_Doc) : f;
}

/* Play: from the cursor (In when it is outside the kept part) to Out */
static void play_kept(void)
{
	ULONG c = get(g_Wave, WAVEFORM_Cursor), out = trim_out();

	/* from the play marker (the cursor), up to Out, or past Out to the end */
	if (c == WAVEFORM_NONE) c = trim_in();
	if (c >= Doc_Length(&g_Doc)) {
		set_status((CONST_STRPTR) TXT_ST_ATEND);
		return;
	}
	/* looped: In to Out (past Out: from the marker to the end) */
	g_LoopFirst = c < out ? trim_in() : c;
	g_LoopEnd = c < out ? out : Doc_Length(&g_Doc);
	play_span(c, c < out ? out : Doc_Length(&g_Doc));
}

/* a part marked by a drag, of some length */
static BOOL part_marked(void)
{
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd);

	return (BOOL) (a != WAVEFORM_NONE && b != WAVEFORM_NONE && a != b);
}

/* Play, Play selection (Shift Space): the part marked, and only it */
static void play_selection(void)
{
	ULONG a = get(g_Wave, WAVEFORM_SelStart), b = get(g_Wave, WAVEFORM_SelEnd), t;

	if (a == WAVEFORM_NONE || b == WAVEFORM_NONE || a == b) {
		set_status((CONST_STRPTR) TXT_ST_NOSEL);
		return;
	}
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	if (b > Doc_Length(&g_Doc)) b = Doc_Length(&g_Doc);
	g_LoopFirst = a;
	g_LoopEnd = b;
	play_span(a, b);
}

/* Listen at In: 3 s from it; at Out: the 3 s up to it */
static void listen(BOOL at_out)
{
	ULONG in = trim_in(), out = trim_out(), l = WavePlan_MsToFrames(LISTEN_MS, g_Doc.d_Rate);

	g_LoopFirst = at_out ? (out > in + l ? out - l : in) : in;
	g_LoopEnd = at_out ? out : (in + l < out ? in + l : out);
	play_span(g_LoopFirst, g_LoopEnd);
}

/* The user's controls go before the work a play brings (the user,
   2026-11-02: "all controls should have priority over caching and
   movement"): the window's input is answered first in every pass, and
   the beat looks for more before each of its steps -- moving the view,
   feeding AHI, reading ahead -- leaving the rest to the next beat. What
   counts is what the user does: keys, buttons, menus, clicks, the wheel
   -- not the ticks and moves that come by themselves */
#define USER_INPUT	(IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS | IDCMP_GADGETDOWN | IDCMP_GADGETUP | \
			 IDCMP_MENUPICK | IDCMP_CLOSEWINDOW | IDCMP_EXTENDEDMOUSE)

/* the time since the input was last looked at, the longest kept while
   it plays; from now */
static void looked(void)
{
	ULONG now;

	if (!g_HaveClock) return;
	now = Clock_Ms();
	if (Play_Playing() && now - g_Looked > g_Deaf) g_Deaf = now - g_Looked;
	g_Looked = now;
}

/* the user's input waiting at port mp (read under Disable(): Intuition
   puts the messages there) */
static BOOL port_input(struct MsgPort *mp)
{
	struct Node *n;
	BOOL any = FALSE;

	Disable();
	for (n = mp->mp_MsgList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
		if (((struct IntuiMessage *) n)->Class & USER_INPUT) {
			any = TRUE;
			break;
		}
	}
	Enable();
	return any;
}

/* the user's input waiting at the window or the Trim window's */
static BOOL input_waiting(void)
{
	struct Window *tw = TrimWin_Window();

	looked();
	if (g_Window && g_Window->UserPort && port_input(g_Window->UserPort)) return TRUE;
	return (BOOL) (tw && tw->UserPort && (!g_Window || tw->UserPort != g_Window->UserPort) && port_input(tw->UserPort));
}

/* a step of the play's work that kept the controls waiting long: said
   on the trace with its ms */
#define SLOW_MS		100UL
static void slow_step(CONST_STRPTR what, ULONG from)
{
	struct TraceNums tn;
	ULONG ms;

	if (!g_HaveClock || !g_TracePath[0] || (ms = Clock_Ms() - from) < SLOW_MS) return;
	tn.tn_What = what;
	tn.tn_N[0] = ms;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the play head where the card is, from this task (never through
   SetGadgetAttrs 10 times a second); the next page's columns painted */
static ULONG show_head(void)
{
	ULONG f = Play_Frame(), n, from = g_HaveClock ? Clock_Ms() : 0;

	n = Waveform_ShowPosition(g_Wave, g_Window, f);
	slow_step((CONST_STRPTR) "slow wave ms", from);
	if (g_HaveClock) from = Clock_Ms();
	Waveform_ShowPosition(g_Over, g_Window, f);
	slow_step((CONST_STRPTR) "slow over ms", from);
	return n;
}

/* what is read already, the gadget's samples and the read-ahead's so
   far: copied for AHI, not read from the file a second time (of the
   next Play_Feed() or Play_Tick() only: the next read moves them) */
static void feed_have(struct FeedStep *fs)
{
	fs->fs_Have[0] = g_NoPace ? NULL : (const struct WaveSamples *) get(g_Wave, WAVEFORM_Samples);
	fs->fs_Have[1] = g_NoPace || !g_PreOpen ? NULL : Prefetch_Part(&g_Pre);
}

/* the pictures a piece further and the samples ahead a piece further,
   by turns: the page in view or the next one painted, then read ahead --
   painting first only when there was nothing else (at 45 frames a column
   an A1200 always had columns to paint: nothing was read ahead, and
   every turn fetched its samples at once). FALSE: nothing done */
#define AHEAD_MS	200UL		/* the pieces a beat, at most */
static ULONG paint_piece(ULONG from);

static BOOL ahead_piece(void)
{
	ULONG n, r;

	n = paint_piece(g_Looked);
	slow_step((CONST_STRPTR) "slow paint ms", g_Looked);
	if (input_waiting()) return (BOOL) (n != 0);
	r = prefetch_beat(g_PaceRead.pc_Units, 2);
	pace(&g_PaceRead, r, g_Looked);
	slow_step((CONST_STRPTR) "slow read ms", g_Looked);
	return (BOOL) (n || r);
}

/* the read-ahead can read pages pages of the view (pre_room): open, or
   not tried yet for this doc and this size */
static BOOL pre_serves(ULONG pages)
{
	ULONG zoom = get(g_Wave, WAVEFORM_Zoom);

	return (BOOL) (!g_NoPace && zoom && zoom < g_Doc.d_Source.ws_Finest && pre_room(pages) && (g_PreOpen || !g_PreTried));
}

/* a piece of painting begun at from, its columns: the view's page first,
   then the overview's changes, then (playing) the next page -- each kind
   timed for its next piece (the overview's columns, cheaper, taught the
   view a 319 ms piece) */
static ULONG g_LeftSeen = 0;		/* the view's columns left after the last piece */

static ULONG paint_piece(ULONG from)
{
	ULONG n = 0, left = get(g_Wave, WAVEFORM_PaintLeft);

	/* a view new since the last piece: what its columns cost is not
	   known (a trimmed part's, hatched, 3.5 times the others' -- a
	   first piece sized by the last view's took 200-300 ms), the first
	   piece the smallest, sized as such */
	if (left > g_LeftSeen) g_PacePaint.pc_Units = g_PacePaint.pc_Min;
	if (left) n = Waveform_PaintAhead(g_Wave, g_Window, g_PacePaint.pc_Units);
	g_LeftSeen = get(g_Wave, WAVEFORM_PaintLeft);
	if (!n && g_Over && get(g_Over, WAVEFORM_PaintLeft)) {
		n = Waveform_PaintAhead(g_Over, g_Window, g_PaceOver.pc_Units);
		pace(&g_PaceOver, n, from);
		return n;
	}
	if (!n && Play_Playing()) n = Waveform_PaintAhead(g_Wave, g_Window, g_PacePaint.pc_Units);
	pace(&g_PacePaint, n, from);
	return n;
}

/* the view or the overview left to fill in */
static BOOL paint_left(void)
{
	return (BOOL) (g_Wave && (get(g_Wave, WAVEFORM_PaintLeft) || (g_Over && get(g_Over, WAVEFORM_PaintLeft))));
}

/* stopped, a view filling in: its next piece, timed (the next sized by
   it); FALSE when none was painted (it waits for its samples, its layer
   is busy) */
static BOOL idle_paint(void)
{
	ULONG from = g_HaveClock ? Clock_Ms() : 0, n;

	n = paint_piece(from);
	slow_step((CONST_STRPTR) "slow paint ms", from);
	return (BOOL) (n != 0);
}

/* stopped, a page filling in that wants samples (a Pause turned it): the
   read-ahead reads this page a piece a pass, the input answered between
   them (the samples fetched whole and the page planned whole cost an
   A1200 335 ms after a Pause). Nothing to read -- handed over, or the
   read-ahead cannot -- what is still wanted is fetched: FALSE */
static BOOL owed_read(void)
{
	ULONG from = g_HaveClock ? Clock_Ms() : 0, r;

	r = prefetch_beat(g_PaceRead.pc_Units, 1);
	pace(&g_PaceRead, r, from);
	slow_step((CONST_STRPTR) "slow read ms", from);
	if (r) return TRUE;
	g_WantOwed = FALSE;
	g_FetchNow = TRUE;
	Signal(FindTask(NULL), g_SigMask);
	return FALSE;
}

/* the next page short of where it should be: more of it left to paint
   than of the page in view left to play, less a quarter (in per cent) */
static BOOL ahead_behind(void)
{
	const struct WaveHeadBox *hb = (const struct WaveHeadBox *) get(g_Wave, WAVEFORM_HeadBox);
	ULONG left = get(g_Wave, WAVEFORM_AheadLeft), vis = get(g_Wave, WAVEFORM_Visible), top, f, rest;

	/* the page in view not whole yet: behind whatever the time */
	if (get(g_Wave, WAVEFORM_PaintLeft)) return TRUE;
	if (!left || !hb || !hb->hb_Columns || !vis) return FALSE;
	top = get(g_Wave, WAVEFORM_Top);
	f = Play_Frame();
	rest = f < top ? 100 : f >= top + vis ? 0 : (top + vis - f) / (vis / 100UL + 1UL);
	return (BOOL) (left * 100UL / hb->hb_Columns + 25UL > rest);
}

/* the play head's beat, 50 a second while it plays (the card's own
   word and IntuiTicks come ten times a second): a timetable -- the next
   due a beat after the last, at least a millisecond off; behind it, the
   table starts from now, a beat skipped, never raced */
static void head_beat(void)
{
	struct FeedStep fs;
	ULONG now, from;

	if (!Clock_Woke() || !Play_Playing()) return;
	paint_budget();
	/* a control waiting goes first: the rest of the beat's work is the
	   next one's (the sprite heads move on by themselves meanwhile). The
	   sound before the picture: AHI fed a piece, more while it is behind */
	fs.fs_Behind = TRUE;
	fs.fs_Fed = 1;
	feed_have(&fs);
	while (fs.fs_Behind && fs.fs_Fed && !input_waiting()) {
		fs.fs_Piece = g_NoPace ? 0 : g_PaceFeed.pc_Units;
		Play_Feed(&fs);
		pace(&g_PaceFeed, fs.fs_Fed, g_Looked);
		slow_step((CONST_STRPTR) "slow feed ms", g_Looked);
		if (g_NoPace) break;
	}
	/* the head: the sprites carry it, on RTG it is drawn (without the
	   pieces, a step of the next page in it as well) */
	if (!input_waiting()) {
		show_head();
		slow_step((CONST_STRPTR) "slow head ms", g_Looked);
	}
	head_place();
	/* the next page: a piece, more while it is behind the head -- for
	   AHEAD_MS at most, then the sound again */
	if (g_NoPace) {
		if (!input_waiting()) {
			prefetch_beat(0, 2);
			slow_step((CONST_STRPTR) "slow read ms", g_Looked);
		}
	} else {
		from = Clock_Ms();
		while (!input_waiting() && ahead_piece() && ahead_behind() && Clock_Ms() - from < AHEAD_MS) {
		}
	}
	looked();
	g_Beats++;
	now = Clock_Ms();
	g_Due += HEAD_MS;
	if ((LONG) (g_Due - now) < 1) g_Due = now + HEAD_MS;
	Clock_Wake(g_Due - now);
}

static void play_signals(void)
{
	struct FeedStep fs;

	feed_have(&fs);
	if (Play_Tick(&fs)) {
		/* the head the beat's, with the clock: a page turned here as
		   well came on top of the player's piece (an A1200 deaf 340 ms) */
		if (!g_HaveClock) show_head();
	} else if (g_PlaySaid) {
		/* played out by itself -- a Pause or a Stop said what happened, and
		   the card's late word of its stop must not take the head away */
		beats_said();
		if (g_Loop && g_LoopEnd > g_LoopFirst) {
			struct TraceNums tn;

			tn.tn_What = (CONST_STRPTR) "looped first end";
			tn.tn_N[0] = g_LoopFirst;
			tn.tn_N[1] = g_LoopEnd;
			tn.tn_Count = 2;
			trace_nums(&tn);
			play_span(g_LoopFirst, g_LoopEnd);
			if (Play_Playing()) return;
		}
		head_off();
		play_label();
		ghost_file(!doc_usable());
		/* played out: the status says so, not "Playing" still */
		trace((CONST_STRPTR) "playedout");
		if (g_PlaySaid) say_span((CONST_STRPTR) TXT_ST_PLAYED, g_PlayFirst, g_PlayEnd);
		g_PlaySaid = FALSE;
	}
}

/* Pause: the card stopped and given back, the play head left where it
   was; Play (or Space) goes on from there to where it would have ended */
static void pause_play(void)
{
	struct TraceNums tn;
	char t[TIME_LEN];
	ULONG f = Play_Frame();

	beats_said();
	Play_Stop();
	g_Paused = TRUE;
	g_PauseAt = f;
	g_PlaySaid = FALSE;
	Waveform_ShowPosition(g_Wave, g_Window, f);
	Waveform_ShowPosition(g_Over, g_Window, f);
	play_label();
	time_text(f, t);
	say1((CONST_STRPTR) TXT_ST_PAUSED, (CONST_STRPTR) t, 0);
	tn.tn_What = (CONST_STRPTR) "paused at";
	tn.tn_N[0] = f;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the Play button and Space: Pause while it plays, on from a pause, else
   Play from the cursor */
static void play_toggle(void)
{
	if (Play_Playing()) pause_play();
	else if (g_Paused && g_PauseAt < g_PlayEnd) play_span(g_PauseAt, g_PlayEnd);
	else {
		g_Paused = FALSE;	/* paused at the very end: from the start again */
		/* a part marked: that part, and Loop repeats it (concept
		   Transport 5); none: from the play marker to Out */
		if (part_marked()) play_selection();
		else play_kept();
	}
}

/* the Play menu's tick as the window's Loop is (the strip as it stands:
   ClearMenuStrip(), the flag, ResetMenuStrip()) */
static void loop_tick(BOOL on)
{
	struct MenuItem *it;

	if (!g_Strip || !g_Window || !(it = ItemAddress(g_Strip, FULLMENUNUM(MENU_PLAY, MENU_LOOPITEM, NOSUB)))) return;
	ClearMenuStrip(g_Window);
	if (on) it->Flags |= CHECKED;
	else it->Flags &= ~CHECKED;
	ResetMenuStrip(g_Window, g_Strip);
}

/* a toggle key pressed in or out; a class that leaves the repaint to the
   caller answers 1 */
static void key_down(Object *o, BOOL down)
{
	if (g_Window && o && SetGadgetAttrs((struct Gadget *) o, g_Window, NULL, GA_Selected, (ULONG) down, TAG_DONE))
		RefreshGList((struct Gadget *) o, g_Window, NULL, 1);
}

/* the flags show the handle picked: one pressed in -- the trace says a
   change */
static void pick_shown(BOOL out)
{
	struct TraceNums tn;

	key_down(g_Flag[0], (BOOL) !out);
	key_down(g_Flag[1], out);
	if (out == g_Pick) return;
	g_Pick = out;
	tn.tn_What = (CONST_STRPTR) "pick out";
	tn.tn_N[0] = (ULONG) out;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the handle pens the waveform drew its flags in, on the bar's flags too
   (asked on the window's ticks: the gadget lays its pens out when it is
   first drawn, and anew on another screen's) */
static void flag_pens(void)
{
	LONG in = Waveform_PenOf(g_Wave, WAVEFORM_COLOR_INHANDLE), out = Waveform_PenOf(g_Wave, WAVEFORM_COLOR_OUTHANDLE);

	if (in == g_FlagPen[0] && out == g_FlagPen[1]) return;
	g_FlagPen[0] = in;
	g_FlagPen[1] = out;
	if (g_Window && g_Flag[0] && g_Flag[1]) {
		RefreshGList((struct Gadget *) g_Flag[0], g_Window, NULL, 1);
		RefreshGList((struct Gadget *) g_Flag[1], g_Window, NULL, 1);
	}
}

/* Loop on or off, from the menu or from the window: the other one follows */
static void loop_set(BOOL on, BOOL from_menu)
{
	struct TraceNums tn;

	g_Loop = on;
	if (!from_menu) loop_tick(on);
	/* a class that leaves the repaint to the caller answers 1 (the
	   checkbox before the key did: MEASURED on the A1200) */
	else if (g_Window && g_LoopKey &&
		 SetGadgetAttrs((struct Gadget *) g_LoopKey, g_Window, NULL, GA_Selected, (ULONG) on, TAG_DONE))
		RefreshGList((struct Gadget *) g_LoopKey, g_Window, NULL, 1);
	tn.tn_What = (CONST_STRPTR) "loop on";
	tn.tn_N[0] = (ULONG) on;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the Stop button: where the card was said */
static void stop_button(void)
{
	char t[TIME_LEN];
	ULONG f;

	if (!Play_Playing()) {
		if (g_Paused) {
			time_text(g_PauseAt, t);
			say1((CONST_STRPTR) TXT_ST_STOPPED, (CONST_STRPTR) t, 0);
		}
		stop_play();
		marker_to(0);
		return;
	}
	f = Play_Frame();
	stop_play();
	time_text(f, t);
	say1((CONST_STRPTR) TXT_ST_STOPPED, (CONST_STRPTR) t, 0);
	g_PlaySaid = FALSE;
	marker_to(0);			/* Stop: back to the start, as a player does */
}

/* --- what the gadgets tell ------------------------------------------------------------ */

static void wave_changes(void)
{
	ULONG c = Waveform_Changes(g_Wave), o = Waveform_Changes(g_Over), first, frames, top, from;
	const struct WaveSamples *ws;
	struct WaveWant *w;

	if (c & WAVEFORM_CHANGED_TRIM) show_trim();
	if (c & WAVEFORM_CHANGED_VIEW) {
		sync_views();
		trace_trim((CONST_STRPTR) "viewed in out top zoom");
	}
	if (c & WAVEFORM_CHANGED_SELECTION) {
		struct TraceNums tn;

		tn.tn_What = (CONST_STRPTR) "selected from to";
		tn.tn_N[0] = get(g_Wave, WAVEFORM_SelStart);
		tn.tn_N[1] = get(g_Wave, WAVEFORM_SelEnd);
		tn.tn_Count = 2;
		trace_nums(&tn);
	}
	/* the samples a deep view wants -- beyond FETCH_MAX frames the class's
	   coarse picture stays. From the view's left edge, not only the part
	   it wants: the samples handed are all it draws from, and the part
	   its last ones gave would be wanted, read and drawn again (a page
	   turned: two reads, three paints) */
	w = NULL;
	if (((c & WAVEFORM_CHANGED_WANT) || g_WantOwed || g_FetchNow) && g_HaveDoc) w = (struct WaveWant *) get(g_Wave, WAVEFORM_Want);
	if (w && w->ww_First == WAVEFORM_NONE) w = NULL;	/* (the read-ahead's came meanwhile) */
	/* while it plays the read-ahead reads them, a piece at a time -- the
	   stretched picture meanwhile (a fetch here cost an A1200 324 ms at a
	   turn); stopped with a page filling in, the same (owed_read); else
	   fetched */
	g_WantOwed = (BOOL) (w && !g_FetchNow &&
			     (Play_Playing() ? g_PreOpen && pre_serves(2) : get(g_Wave, WAVEFORM_PaintLeft) && pre_serves(1)));
	g_FetchNow = FALSE;
	if (w && !g_WantOwed) {
		first = w->ww_First;
		frames = w->ww_Frames;
		top = get(g_Wave, WAVEFORM_Top);
		if (top < first && frames <= FETCH_MAX && first - top <= FETCH_MAX - frames) {
			frames += first - top;
			first = top;
		}
		from = g_HaveClock ? Clock_Ms() : 0;
		ws = frames <= FETCH_MAX ? Doc_Fetch(&g_Doc, first, frames) : NULL;
		slow_step((CONST_STRPTR) "slow fetch ms", from);
	} else {
		ws = NULL;
		first = frames = 0;
	}
	if (ws) {
		struct TraceNums tn;

		/* told before the samples are handed: the refill clears the want */
		tn.tn_What = (CONST_STRPTR) "fetched first frames";
		tn.tn_N[0] = first;
		tn.tn_N[1] = frames;
		tn.tn_Count = 2;
		trace_nums(&tn);
		/* a buffer the class was not drawing from (doc.h): it never
		   reads one that is being filled */
		from = g_HaveClock ? Clock_Ms() : 0;
		set1(g_Wave, WAVEFORM_Samples, (ULONG) ws);
		slow_step((CONST_STRPTR) "slow fetched paint ms", from);
	} else if (w && !g_WantOwed) {
		/* none will come (too many, the file failing, no memory): a
		   view filling in stretches what it misses, as a whole paint */
		set1(g_Wave, WAVEFORM_SamplesNone, TRUE);
	}
	if (o & WAVEFORM_CHANGED_BOX) view_top(get(g_Over, WAVEFORM_BoxTop));
}

/* the play marker to f (the cursor too): shown in both views, the view
   turned to it; playing, the card goes on from it (at the end: stopped);
   paused, the pause is there; else Play starts there */
static void marker_to(ULONG f)
{
	struct TraceNums tn;
	ULONG out = trim_out();

	if (!doc_usable()) return;
	if (f > Doc_Length(&g_Doc)) f = Doc_Length(&g_Doc);
	set1(g_Wave, WAVEFORM_Cursor, f);
	if (Play_Playing()) {
		if (f < Doc_Length(&g_Doc)) {
			play_span(f, f < out ? out : Doc_Length(&g_Doc));
		} else {
			char t[TIME_LEN];

			time_text(Play_Frame(), t);
			stop_play();
			say1((CONST_STRPTR) TXT_ST_STOPPED, (CONST_STRPTR) t, 0);
		}
		if (Play_Playing()) return;
	}
	if (g_Paused) {
		g_PauseAt = f;
		g_PlayEnd = f < out ? out : Doc_Length(&g_Doc);
	}
	Waveform_ShowPosition(g_Wave, g_Window, f);
	Waveform_ShowPosition(g_Over, g_Window, f);
	tn.tn_What = (CONST_STRPTR) "seek at";
	tn.tn_N[0] = f;
	tn.tn_Count = 1;
	trace_nums(&tn);
}

/* the arrows' run of steps on a handle: one step in the history (the
   first press pushes it, the rest amend it -- undo.h) */
static void key_changed(UWORD target)
{
	struct UndoMark m;
	UWORD r;

	levels_shown();
	mark_now(&m);
	r = g_KeyRun == target ? Undo_Amend(&g_Undo, &m) : Undo_Push(&g_Undo, &m);
	if (r == UNDO_PUSHED) g_KeyRun = target;
	else if (r != UNDO_SAME) g_KeyRun = TARGET_MARKER;
	if (r == UNDO_NOMEM) set_status((CONST_STRPTR) TXT_ST_NOUNDO);
	set_dirty(Undo_Dirty(&g_Undo));
	edit_menu();
}

/* where the arrows' target is */
static ULONG target_frame(void)
{
	ULONG c;

	if (g_Target == TARGET_IN) return trim_in();
	if (g_Target == TARGET_OUT) return trim_out();
	c = get(g_Wave, WAVEFORM_Cursor);
	return c == WAVEFORM_NONE ? trim_in() : c;
}

/* the view turned to f when it is out of it, f in the middle */
static void keep_in_view(ULONG f)
{
	ULONG top = get(g_Wave, WAVEFORM_Top), vis = get(g_Wave, WAVEFORM_Visible);

	if (f >= top && f < top + vis) return;
	view_top(f > vis / 2 ? f - vis / 2 : 0);
}

/* F1 and F2 (View, Previous and Next marker): the play marker to the
   marker before or after it -- In and Out the ends of the way -- the view
   turned there; playing, the play goes on from there. Playing, a marker
   passed less than a second ago is the one it is at: F1 goes to the one
   before it, as a CD player's Previous does */
/* the stop before or after the play marker (playing) or the cursor: In,
   the markers between In and Out, Out -- and with ends, past them the
   file's start or end (the deck's Rewind and Forward) */
static void jump_stop(BOOL next, BOOL ends)
{
	struct TraceNums tn;
	char at[TIME_LEN];
	ULONG f, stop, in, out, grace = 0, best = WAVEFORM_NONE;
	LONG which = -1;		/* the marker's index; -1 In, -2 Out */
	UWORD i;

	if (!doc_usable()) return;
	in = trim_in();
	out = trim_out();
	/* from the head: playing, paused (not the play marker it started
	   from), else the marker */
	f = Play_Playing() ? Play_Frame() : g_Paused ? g_PauseAt : get(g_Wave, WAVEFORM_Cursor);
	if (f == WAVEFORM_NONE) f = in;
	if (!next && Play_Playing()) grace = g_Doc.d_Rate;
	/* the stops: the markers between In and Out, In and Out -- a marker
	   outside them is no stop, In and Out are the ends of the way */
	for (i = 0; i < g_Marks.mk_Count + 2; i++) {
		stop = i < g_Marks.mk_Count ? g_Marks.mk_Frame[i] : i == g_Marks.mk_Count ? in : out;
		if (i < g_Marks.mk_Count && (stop < in || stop > out)) continue;
		/* the grace is a marker's: In stays a stop to go back to */
		if (next ? (stop > f && (best == WAVEFORM_NONE || stop < best)) :
			   (stop + (i < g_Marks.mk_Count ? grace : 0) < f && (best == WAVEFORM_NONE || stop > best))) {
			best = stop;
			which = i < g_Marks.mk_Count ? (LONG) i : i == g_Marks.mk_Count ? -1L : -2L;
		}
	}
	/* past In and Out: the file's start or end (-3, -4) */
	if (best == WAVEFORM_NONE && ends) {
		if (next && f < Doc_Length(&g_Doc)) {
			best = Doc_Length(&g_Doc);
			which = -4;
		} else if (!next && f > 0) {
			best = 0;
			which = -3;
		}
	}
	time_text(f, at);
	if (best == WAVEFORM_NONE) {
		say1((CONST_STRPTR) (next ? TXT_ST_NO_MARK_AFTER : TXT_ST_NO_MARK_BEFORE), (CONST_STRPTR) at, 0);
		return;
	}
	g_Target = TARGET_MARKER;
	/* Out and the end are the end of the way: playing, the play ends
	   there, not past it */
	if ((which == -2 || which == -4) && Play_Playing()) stop_play();
	marker_to(best);
	keep_in_view(best);
	tn.tn_What = (CONST_STRPTR) "jump next from to";
	tn.tn_N[0] = (ULONG) next;
	tn.tn_N[1] = f;
	tn.tn_N[2] = best;
	tn.tn_Count = 3;
	trace_nums(&tn);
	time_text(best, at);
	/* the marker's number as the waveform labels it: the track it begins
	   (In begins the first) */
	if (which >= 0) say1((CONST_STRPTR) TXT_ST_AT_MARK, (CONST_STRPTR) at, which + 2);
	else if (which >= -2) say1((CONST_STRPTR) (which == -1 ? TXT_ST_AT_IN : TXT_ST_AT_OUT), (CONST_STRPTR) at, 0);
}

/* F1, F2 and View's marker items: the stops between In and Out */
static void jump_marker(BOOL next)
{
	jump_stop(next, FALSE);
}

/* a deck key pressed -- its release code the mode, DECK_PAUSED while
   paused (Pause is a toggle that keeps the mode: a change of that bit
   against what the deck showed, the key shown kept, is Pause; another
   key pressed while paused clears the bit as well, and is that key --
   taken for Pause, Rewind went on playing) -- the deck shown as what
   plays after it */
#define DECK_PAUSED	0x1000

static void deck_key(UWORD code)
{
	struct TraceNums tn;
	UWORD mode = (UWORD) (code & 0x0FFF), shown = (UWORD) (Play_Playing() || g_Paused ? BUT_PLAY : BUT_STOP);
	BOOL pause = (BOOL) ((BOOL) ((code & DECK_PAUSED) != 0) != g_DeckPaused && mode == shown);

	tn.tn_What = (CONST_STRPTR) "deckkey mode pause";
	tn.tn_N[0] = mode;
	tn.tn_N[1] = (ULONG) pause;
	tn.tn_Count = 2;
	trace_nums(&tn);
	if (pause) {
		if (Play_Playing() || g_Paused) play_toggle();
	} else {
		switch (mode) {
		case BUT_PLAY: if (!Play_Playing()) play_toggle(); break;
		case BUT_STOP: stop_button(); break;
		case BUT_REWIND: jump_stop(FALSE, TRUE); break;
		case BUT_FORWARD: jump_stop(TRUE, TRUE); break;
		}
	}
	play_label();
}

/* the cursor keys (Style Guide 8): Left and Right move the handle last
   touched, else the play marker -- a column of the view a press, Shift a
   page, Ctrl to the start or the end; Up and Down zoom in and out with
   it where it is */
struct Arrow {
	UWORD ar_Key, ar_Qual;
};

static void arrow_key(const struct Arrow *ar)
{
	ULONG len = Doc_Length(&g_Doc), f, step;
	BOOL left = (BOOL) (ar->ar_Key == RAW_LEFT);
	struct TraceNums tn;

	if (!doc_usable()) return;
	f = target_frame();
	if (ar->ar_Key == RAW_UP || ar->ar_Key == RAW_DOWN) {
		SetGadgetAttrs((struct Gadget *) g_Wave, g_Window, NULL, WAVEFORM_ZoomCenter, f,
			       WAVEFORM_ZoomStep, (ULONG) (ar->ar_Key == RAW_UP ? 1 : -1), TAG_DONE);
		sync_views();
		trace_trim((CONST_STRPTR) "keyzoom in out top zoom");
		return;
	}
	step = (ar->ar_Qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? get(g_Wave, WAVEFORM_Visible) : (ULONG) nudge_step();
	if (!step) step = 1;
	if (ar->ar_Qual & IEQUALIFIER_CONTROL) f = left ? 0 : len;
	else if (left) f = f > step ? f - step : 0;
	else f = f + step < len ? f + step : len;
	if (g_Target == TARGET_MARKER) {
		marker_to(f);
	} else if (handle_to((BOOL) (g_Target == TARGET_OUT), f)) {
		keep_in_view(target_frame());
		key_changed(g_Target);
	}
	tn.tn_What = (CONST_STRPTR) "arrow target frame";
	tn.tn_N[0] = g_Target;
	tn.tn_N[1] = target_frame();
	tn.tn_Count = 2;
	trace_nums(&tn);
}

/* Cancel (or Esc) while a file is read or decoded: called off, the file
   closed (a save's Cancel is heard while it copies: save_progress) */
static void cancel_job(void)
{
	struct TraceNums tn;
	CONST_STRPTR what;

	if (!g_Reading && !g_Decoding) return;
	/* the rest of a long file being read, changes made since its
	   suggestion: asked first (a Save there ends the reading) */
	if (g_Dirty && !may_leave()) return;
	if (!g_Reading && !g_Decoding) return;
	what = (CONST_STRPTR) (g_Decoding ? TXT_ST_DECODE_CANCELLED : TXT_ST_READ_CANCELLED);
	trace((CONST_STRPTR) (g_Decoding ? "cancelled decoding" : "cancelled reading"));
	/* the gauge emptied too: it kept the level the job had come to */
	set1(g_Gauge, FUELGAUGE_Level, 0);
	tn.tn_What = (CONST_STRPTR) "gauge level";
	tn.tn_N[0] = get(g_Gauge, FUELGAUGE_Level);
	tn.tn_Count = 1;
	trace_nums(&tn);
	say1(what, file_name(), 0);
	forget_doc();			/* a decode's job aborted with it */
	ghost_file(TRUE);
}

/* a click on the sound, let go (not a drag): the play marker there */
static void click_seek(void)
{
	ULONG c = get(g_Wave, WAVEFORM_Cursor);

	if (c != WAVEFORM_NONE) marker_to(c);
}

/* the overview's knobs: In or Out dropped there -- the waveform's handle
   set to it and snapped, as a drag in the waveform is */
static void over_up(UWORD code)
{
	BOOL out = (BOOL) (code == WAVEFORM_UP_TRIMOUT);

	if (code == WAVEFORM_UP_TRIMIN || code == WAVEFORM_UP_TRIMOUT) {
		set1(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn, get(g_Over, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn));
		snap_handle(out);
		changed();
		g_Target = out ? TARGET_OUT : TARGET_IN;
		pick_shown(out);
	}
	wave_changes();
}

/* a handle let go in the waveform: its flag picked; the same one let go
   again within the double-click's time (a double-click on its flag):
   the Trim window */
static void handle_up(BOOL out)
{
	ULONG secs, micros;

	pick_shown(out);
	CurrentTime(&secs, &micros);
	if (g_UpHandle == (UWORD) (out ? 2 : 1) && DoubleClick(g_UpSecs, g_UpMicros, secs, micros)) {
		g_UpHandle = 0;
		trim_open();
		return;
	}
	g_UpHandle = (UWORD) (out ? 2 : 1);
	g_UpSecs = secs;
	g_UpMicros = micros;
}

static void wave_up(UWORD code)
{
	switch (code) {
	case WAVEFORM_UP_TRIMIN: snap_handle(FALSE); changed(); g_Target = TARGET_IN; handle_up(FALSE); break;
	case WAVEFORM_UP_TRIMOUT: snap_handle(TRUE); changed(); g_Target = TARGET_OUT; handle_up(TRUE); break;
	case WAVEFORM_UP_CURSOR: show_trim(); click_seek(); g_Target = TARGET_MARKER; break;
	case WAVEFORM_UP_MARKER: marker_up(); break;
	default: show_trim(); break;
	}
	wave_changes();
}

static void zoom(LONG steps)
{
	set1(g_Wave, WAVEFORM_ZoomStep, (ULONG) steps);
	sync_views();
}

static void at_handle(BOOL out)
{
	ULONG f = get(g_Wave, out ? WAVEFORM_TrimOut : WAVEFORM_TrimIn);

	if (f != WAVEFORM_NONE) show_around(f);
	trace_trim((CONST_STRPTR) "view in out top zoom");
}

static void gadget_up(ULONG id, UWORD code)
{
	/* Cancel before the doc's check: a file being decoded has no doc yet,
	   and its Cancel was dropped here */
	if (id == GID_CANCEL) {
		cancel_job();
		return;
	}
	/* the Trim window shows nothing to trim without a doc, ghosted */
	if (id == GID_TRIMWIN) {
		trim_open();
		return;
	}
	if (!g_HaveDoc) return;
	switch (id) {
	case GID_WAVE: wave_up(code); break;
	case GID_OVER: over_up(code); break;
	case GID_SCROLL: view_top(get(g_Scroll, SCROLLER_Top) * (get(g_Wave, WAVEFORM_Zoom) ? get(g_Wave, WAVEFORM_Zoom) : 1)); break;
	case GID_ZOOMIN: zoom(1); break;
	case GID_ZOOMOUT: zoom(-1); break;
	case GID_ALL: set1(g_Wave, WAVEFORM_ShowAll, TRUE); sync_views(); break;
	case GID_DECK: deck_key(code); break;
	/* a flag: that handle picked (the arrows move it) and shown, as At In
	   and At Out were */
	case GID_FLAGIN:
	case GID_FLAGOUT:
		g_Target = id == GID_FLAGOUT ? TARGET_OUT : TARGET_IN;
		pick_shown((BOOL) (id == GID_FLAGOUT));
		at_handle((BOOL) (id == GID_FLAGOUT));
		break;
	case GID_LOOP: loop_set((BOOL) (get(g_LoopKey, GA_Selected) != 0), FALSE); break;
	}
}

/* Settings, Play through and AHI mode: to the player, for the plays from
   now on; the trace says so */
static void play_way(void)
{
	struct PlayWay pw;
	struct TraceNums tn;

	pw.pw_Way = g_Prefs.ep_Play;
	pw.pw_AhiID = g_Prefs.ep_AhiID;
	pw.pw_AhiFreq = g_Prefs.ep_AhiFreq;
	Play_SetWay(&pw);
	tn.tn_What = (CONST_STRPTR) "playway way id freq";
	tn.tn_N[0] = pw.pw_Way;
	tn.tn_N[1] = pw.pw_AhiID;
	tn.tn_N[2] = pw.pw_AhiFreq;
	tn.tn_Count = 3;
	trace_nums(&tn);
}

/* Settings, AHI mode...: AHI's own requester over the window; the mode
   chosen said by its name */
static void ahi_mode(void)
{
	struct AhiMode am;
	struct AhiName an;
	char name[64];

	stop_play();
	am.am_ID = g_Prefs.ep_AhiID;
	am.am_Freq = g_Prefs.ep_AhiFreq;
	if (!Ahi_There()) {
		set_status((CONST_STRPTR) TXT_ST_PLAY_NOAHI);
		return;
	}
	if (!Ahi_Request(g_Window, &am)) return;
	g_Prefs.ep_AhiID = am.am_ID;
	g_Prefs.ep_AhiFreq = am.am_Freq;
	play_way();
	an.an_Mode = &am;
	an.an_Buf = (STRPTR) name;
	an.an_Size = sizeof(name);
	if (!Ahi_ModeName(&an)) Text_Copy((STRPTR) name, (CONST_STRPTR) TXT_AHI_DEFAULT, sizeof(name));
	say1((CONST_STRPTR) TXT_ST_AHIMODE, (CONST_STRPTR) name, 0);
}

/* the settings onto the trace (the tests): snap lead tail backup icons
   overview, where they were read from */
static void trace_prefs(void)
{
	struct TraceNums tn;

	tn.tn_What = (CONST_STRPTR) "prefs snap lead tail backup";
	tn.tn_N[0] = (ULONG) g_Prefs.ep_Snap;
	tn.tn_N[1] = g_Prefs.ep_LeadMs;
	tn.tn_N[2] = g_Prefs.ep_TailMs;
	tn.tn_N[3] = (ULONG) g_Prefs.ep_Backup;
	tn.tn_Count = 4;
	trace_nums(&tn);
	tn.tn_What = (CONST_STRPTR) "prefs2 icons overview from";
	tn.tn_N[0] = (ULONG) g_Prefs.ep_Icons;
	tn.tn_N[1] = g_Prefs.ep_Overview;
	tn.tn_N[2] = g_PrefsFrom;
	tn.tn_Count = 3;
	trace_nums(&tn);
	/* the way it plays (the first play_way() came before the trace's path) */
	tn.tn_What = (CONST_STRPTR) "playway way id freq";
	tn.tn_N[0] = g_Prefs.ep_Play;
	tn.tn_N[1] = g_Prefs.ep_AhiID;
	tn.tn_N[2] = g_Prefs.ep_AhiFreq;
	tn.tn_Count = 3;
	trace_nums(&tn);
}

/* Settings, Save settings: where they were read from (settings.h); the
   weight bar's place with them once it was dragged -- else the share
   read stays, however this screen laid it out */
static void save_prefs(void)
{
	struct Gadget *o = (struct Gadget *) g_Over, *w = (struct Gadget *) g_Wave;
	struct TraceNums tn;
	ULONG sum, pc;
	LONG err;

	if (g_SplitMoved && o->Height > 0 && w->Height > 0) {
		sum = (ULONG) o->Height + (ULONG) w->Height;
		pc = ((ULONG) o->Height * 100 + sum / 2) / sum;
		if (pc < EDITPREFS_OVER_MIN) pc = EDITPREFS_OVER_MIN;
		if (pc > EDITPREFS_OVER_MAX) pc = EDITPREFS_OVER_MAX;
		g_Prefs.ep_Overview = (UWORD) pc;
	}
	err = Settings_Save(&g_Prefs, g_PrefsFrom);
	tn.tn_What = (CONST_STRPTR) "prefssaved err";
	tn.tn_N[0] = (ULONG) err;
	tn.tn_Count = 1;
	trace_nums(&tn);
	trace_prefs();
	if (err) say1((CONST_STRPTR) TXT_E_PREFS, NULL, err);
	else set_status((CONST_STRPTR) (g_PrefsFrom == SETTINGS_PROGDIR ? TXT_ST_PREFS_PROGDIR : TXT_ST_PREFS_SAVED));
}

/* ---- Effects: Gain..., Normalise, Remove fades and gain ---- */

static struct TlPart g_GainPt;		/* the part it gains */
static WORD g_GainShown = 0;		/* the gain the pieces show, tenths of a dB */

/* a gain in tenths for the trace: 1000 is 0 dB */
#define TRACE_TENTHS(t)	((ULONG) (1000L + (LONG) (t)))

/* the preview painted now: the Gain window holds the loop, so no idle
   pieces fill the view in while it is open (since a stopped view fills in
   by pieces, the waveform behind the window had stopped following the
   slider). A kept view paints only the columns that look different: the
   part gained. Pieces it must wait for (samples at a deep zoom) are left
   to the main loop. Traced: the view's columns left before and after, the
   overview's after. */
static void gain_painted(void)
{
	struct TraceNums tn;
	ULONG before = get(g_Wave, WAVEFORM_PaintLeft);
	UWORD k;

	for (k = 0; k < 400 && paint_left(); k++)
		if (!paint_piece(g_HaveClock ? Clock_Ms() : 0)) break;
	tn.tn_What = (CONST_STRPTR) "gainpaint before after over";
	tn.tn_N[0] = before;
	tn.tn_N[1] = get(g_Wave, WAVEFORM_PaintLeft);
	tn.tn_N[2] = g_Over ? get(g_Over, WAVEFORM_PaintLeft) : 0;
	tn.tn_Count = 3;
	trace_nums(&tn);
}

/* the pieces at the gain: the ones before the window, the part gained
   (a part with too many pieces for it stays as it was) */
static void gain_preview(WORD tenths)
{
	struct TraceNums tn;

	if (tenths == g_GainShown) return;
	prefetch_off();
	g_Doc.d_Tl = g_SpareTl;
	g_GainPt.tp_Gain = Env_FromTenths(tenths);
	if (tenths) Tl_Gain(&g_Doc.d_Tl, &g_GainPt);
	g_GainShown = tenths;
	timeline_shown();
	tn.tn_What = (CONST_STRPTR) "gainpreview level tenths";
	tn.tn_N[0] = g_GainPt.tp_Gain;
	tn.tn_N[1] = TRACE_TENTHS(tenths);
	tn.tn_Count = 2;
	trace_nums(&tn);
	gain_painted();
}

/* the Gain window's boxes onto the trace (the tests click into it) */
static void gain_boxes(const struct IBox *b)
{
	static CONST_STRPTR what[GAIN_BOXES] = {
		(CONST_STRPTR) "gainwin", (CONST_STRPTR) "gainfield", (CONST_STRPTR) "gainnormalise",
		(CONST_STRPTR) "gainlisten", (CONST_STRPTR) "gainuse", (CONST_STRPTR) "gaincancel"
	};
	struct TraceNums tn;
	UWORD i;

	for (i = 0; i < GAIN_BOXES; i++) {
		tn.tn_What = what[i];
		tn.tn_N[0] = (ULONG) b[i].Left;
		tn.tn_N[1] = (ULONG) b[i].Top;
		tn.tn_N[2] = (ULONG) b[i].Width;
		tn.tn_N[3] = (ULONG) b[i].Height;
		tn.tn_Count = 4;
		trace_nums(&tn);
	}
}

/* what the Gain window tells while it is open: the waveform behind
   follows its gain, Listen plays the part, the player is kept fed */
static void gain_tell(const struct GainMsg *gm)
{
	switch (gm->gm_What) {
	case GAIN_PREVIEW:
		gain_preview(gm->gm_Tenths);
		break;
	case GAIN_LISTEN:
		stop_play();
		g_LoopFirst = g_GainPt.tp_First;
		g_LoopEnd = g_GainPt.tp_End;
		play_span(g_GainPt.tp_First, g_GainPt.tp_End);
		break;
	case GAIN_SIGNALS:
		if (gm->gm_Sigs & Play_Signals()) play_signals();
		if (g_HaveClock && (gm->gm_Sigs & Clock_Signal())) head_beat();
		break;
	case GAIN_SHOWN:
		if (g_TracePath[0]) gain_boxes(gm->gm_Boxes);
		break;
	}
}

/* a part's two ends and a gain said: fmt with the gain, from, to */
static void say_gain(CONST_STRPTR fmt, const struct TlPart *pt, WORD tenths)
{
	struct TextArgs ta;
	char db[8], t0[TIME_LEN], t1[TIME_LEN], said[STATUS_LEN];

	*Gain_PutTenths((STRPTR) db, tenths) = '\0';
	time_text(pt->tp_First, t0);
	time_text(pt->tp_End, t1);
	ta.ta_Fmt = fmt;
	ta.ta_Str[0] = (CONST_STRPTR) db;
	ta.ta_Str[1] = (CONST_STRPTR) t0;
	ta.ta_Str[2] = (CONST_STRPTR) t1;
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) said, sizeof(said), &ta);
	set_status((CONST_STRPTR) said);
}

/* the part's loudest sample now, the pointer busy while it is measured
   (a part at a level is read through) */
static ULONG part_peak(const struct TlPart *pt)
{
	ULONG peak;

	SetAttrs(g_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
	peak = Doc_PartPeak(&g_Doc, pt->tp_First, pt->tp_End);
	SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
	return peak;
}

/* Effects/Gain... (concept 3.2, F6 F8 F9): the part marked (else In ..
   Out) in the Gain window, the waveform following its slider; Use a step
   for Undo, Cancel the pieces as they were. Normalise's level a setting
   whichever way it closes. */
static void gain_part(void)
{
	struct GainArgs ga;
	struct TraceNums tn;
	ULONG peak;
	BOOL used;

	if (!effect_part(&g_GainPt, TRUE)) return;
	peak = part_peak(&g_GainPt);
	g_SpareTl = g_Doc.d_Tl;
	g_GainShown = 0;
	clear_bytes((UBYTE *) &ga, sizeof(ga));
	ga.ga_Over = g_Window;
	ga.ga_Tenths = 0;
	ga.ga_Target = (WORD) -(WORD) g_Prefs.ep_Normalise;
	ga.ga_Peak = peak;
	ga.ga_Tell = gain_tell;
	ga.ga_SigMask = Play_Signals() | (g_HaveClock ? Clock_Signal() : 0);
	tn.tn_What = (CONST_STRPTR) "gain from to peak";
	tn.tn_N[0] = g_GainPt.tp_First;
	tn.tn_N[1] = g_GainPt.tp_End;
	tn.tn_N[2] = peak;
	tn.tn_Count = 3;
	trace_nums(&tn);
	/* the window holds the loop: the main one's pointer busy meanwhile */
	SetAttrs(g_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
	used = Gain_Window(&ga);
	SetAttrs(g_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
	stop_play();
	prefetch_off();
	if (ga.ga_Target <= 0 && ga.ga_Target >= -EDITPREFS_NORMALISE_MAX) g_Prefs.ep_Normalise = (UWORD) -ga.ga_Target;
	if (used && ga.ga_Tenths) gain_preview(ga.ga_Tenths);
	if (!used || !ga.ga_Tenths || Tl_Same(&g_Doc.d_Tl, &g_SpareTl)) {
		/* Cancel, 0 dB, or no room for the pieces: as it was */
		if (!Tl_Same(&g_Doc.d_Tl, &g_SpareTl)) {
			g_Doc.d_Tl = g_SpareTl;
			timeline_shown();
		}
		g_GainShown = 0;
		tn.tn_What = (CONST_STRPTR) "gainkept used";
		tn.tn_N[0] = (ULONG) used;
		tn.tn_Count = 1;
		trace_nums(&tn);
		if (ga.ga_NoWindow) set_status((CONST_STRPTR) TXT_E_GAINWIN);
		else if (used && ga.ga_Tenths) set_status((CONST_STRPTR) TXT_ST_CUTFULL);
		else if (used) set_status((CONST_STRPTR) TXT_ST_GAIN_KEPT);
		return;
	}
	g_GainShown = 0;
	changed();
	tn.tn_What = (CONST_STRPTR) "gained from to level tenths";
	tn.tn_N[0] = g_GainPt.tp_First;
	tn.tn_N[1] = g_GainPt.tp_End;
	tn.tn_N[2] = g_GainPt.tp_Gain;
	tn.tn_N[3] = TRACE_TENTHS(ga.ga_Tenths);
	tn.tn_Count = 4;
	trace_nums(&tn);
	say_gain((CONST_STRPTR) (Gain_Peak(peak, ga.ga_Tenths) > 32767UL ? TXT_ST_GAINED_CLIPS : TXT_ST_GAINED), &g_GainPt,
		 ga.ga_Tenths);
}

/* Effects/Normalise (F7): the part marked (else In .. Out) at once to the
   settings' level, its louder channel's peak there and never past it */
static void normalise_part(void)
{
	struct TraceNums tn;
	struct TextArgs ta;
	struct TlPart pt;
	char db[8], to[8], said[STATUS_LEN];
	WORD target = (WORD) -(WORD) g_Prefs.ep_Normalise, tenths;
	ULONG peak;

	if (!effect_part(&pt, TRUE)) return;
	peak = part_peak(&pt);
	tenths = Gain_Normalise(peak, target);
	tn.tn_What = (CONST_STRPTR) "normalise from to peak tenths";
	tn.tn_N[0] = pt.tp_First;
	tn.tn_N[1] = pt.tp_End;
	tn.tn_N[2] = peak;
	tn.tn_N[3] = TRACE_TENTHS(tenths);
	tn.tn_Count = 4;
	trace_nums(&tn);
	if (!peak) {
		set_status((CONST_STRPTR) TXT_ST_NORM_SILENT);
		return;
	}
	if (!tenths) {
		*Gain_PutTenths((STRPTR) db, Gain_PeakTenths(peak)) = '\0';
		say1((CONST_STRPTR) TXT_ST_NORM_THERE, (CONST_STRPTR) db, 0);
		return;
	}
	pt.tp_Gain = Env_FromTenths(tenths);
	if (!Tl_Gain(&g_Doc.d_Tl, &pt)) {
		set_status((CONST_STRPTR) TXT_ST_CUTFULL);
		return;
	}
	timeline_shown();
	changed();
	*Gain_PutTenths((STRPTR) to, target) = '\0';
	*Gain_PutTenths((STRPTR) db, tenths) = '\0';
	ta.ta_Fmt = (CONST_STRPTR) TXT_ST_NORMALISED;
	ta.ta_Str[0] = (CONST_STRPTR) to;
	ta.ta_Str[1] = (CONST_STRPTR) db;
	ta.ta_Str[2] = NULL;
	ta.ta_Num[0] = ta.ta_Num[1] = ta.ta_Num[2] = 0;
	Text_Fmt((STRPTR) said, sizeof(said), &ta);
	set_status((CONST_STRPTR) said);
}

/* Effects/Remove fades and gain: the part marked (else In .. Out) as
   recorded again; nothing there to remove said */
static void plain_part(void)
{
	struct TraceNums tn;
	struct TlPart pt;

	struct TlLevels *lv = &g_Doc.d_Levels;
	BOOL ends = FALSE;

	if (!effect_part(&pt, TRUE)) return;
	g_SpareTl = g_Doc.d_Tl;
	if (!Tl_Plain(&g_Doc.d_Tl, &pt)) {
		set_status((CONST_STRPTR) TXT_ST_CUTFULL);
		return;
	}
	/* an end fade reaching into the part goes with it */
	levels_now();
	if (lv->lv_FadeIn && pt.tp_First < lv->lv_In + lv->lv_FadeIn && pt.tp_End > lv->lv_In) {
		lv->lv_FadeIn = 0;
		ends = TRUE;
	}
	if (lv->lv_FadeOut && pt.tp_End > lv->lv_Out - lv->lv_FadeOut && pt.tp_First < lv->lv_Out) {
		lv->lv_FadeOut = 0;
		ends = TRUE;
	}
	tn.tn_What = (CONST_STRPTR) "plain from to changed";
	tn.tn_N[0] = pt.tp_First;
	tn.tn_N[1] = pt.tp_End;
	tn.tn_N[2] = (ULONG) (ends || !Tl_Same(&g_Doc.d_Tl, &g_SpareTl));
	tn.tn_Count = 3;
	trace_nums(&tn);
	if (!tn.tn_N[2]) {
		set_status((CONST_STRPTR) TXT_ST_NOTHING_PLAIN);
		return;
	}
	timeline_shown();
	show_trim();
	changed();
	say_span((CONST_STRPTR) TXT_ST_PLAINED, pt.tp_First, pt.tp_End);
}

/* the Lead and tail window's boxes onto the trace while it is open (the
   tests type into it) */
static void margins_shown(const struct IBox *b)
{
	static CONST_STRPTR what[MARGINS_BOXES] = {
		(CONST_STRPTR) "marginswin", (CONST_STRPTR) "marginslead", (CONST_STRPTR) "marginstail",
		(CONST_STRPTR) "marginsuse"
	};
	struct TraceNums tn;
	UWORD i;

	for (i = 0; i < MARGINS_BOXES; i++) {
		tn.tn_What = what[i];
		tn.tn_N[0] = (ULONG) b[i].Left;
		tn.tn_N[1] = (ULONG) b[i].Top;
		tn.tn_N[2] = (ULONG) b[i].Width;
		tn.tn_N[3] = (ULONG) b[i].Height;
		tn.tn_Count = 4;
		trace_nums(&tn);
	}
}

/* Settings, Lead and tail...: the margins in their window (margins.h);
   the open file's suggestion made again with them while its handles are
   still the suggestion's (no * in the title) -- handles the user placed
   stay where they are, the next file gets the new margins */
static void do_margins(void)
{
	struct MarginArgs ma;
	struct TraceNums tn;
	char lead[MARGINS_SECSLEN], tail[MARGINS_SECSLEN];
	struct Say sy;
	BOOL moved = FALSE;

	clear_bytes((UBYTE *) &ma, sizeof(ma));
	ma.ma_Over = g_Window;
	ma.ma_Lead = g_Prefs.ep_LeadMs;
	ma.ma_Tail = g_Prefs.ep_TailMs;
	ma.ma_LeadMax = EDITPREFS_LEAD_MAX;
	ma.ma_TailMax = EDITPREFS_TAIL_MAX;
	if (g_TracePath[0]) ma.ma_Shown = margins_shown;
	stop_play();			/* the window holds the loop */
	if (!Margins_Window(&ma)) {
		trace((CONST_STRPTR) "margins kept");
		if (ma.ma_NoWindow) set_status((CONST_STRPTR) TXT_E_MARGINS);
		return;
	}
	g_Prefs.ep_LeadMs = ma.ma_Lead;
	g_Prefs.ep_TailMs = ma.ma_Tail;
	Doc_SetMargins(ma.ma_Lead, ma.ma_Tail);
	if (doc_usable() && !g_Reading && !g_Decoding && !g_Dirty && Doc_Replan(&g_Doc)) {
		show_plan();
		moved = TRUE;
	}
	tn.tn_What = (CONST_STRPTR) "margins lead tail moved";
	tn.tn_N[0] = ma.ma_Lead;
	tn.tn_N[1] = ma.ma_Tail;
	tn.tn_N[2] = (ULONG) moved;
	tn.tn_Count = 3;
	trace_nums(&tn);
	trace_prefs();
	if (moved) return;		/* the status line says the new suggestion */
	Margins_Secs(ma.ma_Lead, lead);
	Margins_Secs(ma.ma_Tail, tail);
	sy.sy_Fmt = (CONST_STRPTR) (g_HaveDoc ? TXT_ST_MARGINS_KEPT : TXT_ST_MARGINS);
	sy.sy_S0 = (CONST_STRPTR) lead;
	sy.sy_S1 = (CONST_STRPTR) tail;
	sy.sy_N0 = sy.sy_N1 = 0;
	say(&sy);
}

/* a menu pick (several in one go: the chain read before acting -- a
   close frees the strip) */
#define MENU_PICKS	16
static BOOL menu_pick(UWORD code)
{
	struct MenuItem *item;
	UWORD codes[MENU_PICKS], n = 0, k;
	ULONG mid;

	/* the whole chain read before any pick is acted on: a question or a
	   job's own loop in between takes new picks, and they rewrite the
	   items' NextSelect */
	while (code != MENUNULL && g_Strip && n < MENU_PICKS && (item = ItemAddress(g_Strip, code))) {
		codes[n++] = code;
		code = item->NextSelect;
	}
	for (k = 0; k < n && g_Strip; k++) {
		if (!(item = ItemAddress(g_Strip, codes[k]))) break;
		mid = (ULONG) GTMENUITEM_USERDATA(item);
		switch (mid) {
		case MID_OPEN: do_open(); break;
		case MID_NEXT: do_next(); break;
		case MID_SAVE: do_save(); break;
		case MID_SAVEAS: do_save_as(); break;
		case MID_SPLIT: do_split(); break;
		case MID_REVERT:
			/* the changes asked for, then gone: the file read again */
			if (g_HaveDoc && (!g_Dirty || ask((CONST_STRPTR) TXT_ASK_REVERT, (CONST_STRPTR) TXT_ASK_REVERT_GADS) == 1)) {
				set_dirty(FALSE);
				open_file((CONST_STRPTR) g_OpenPath);
			}
			break;
		case MID_HELP: show_help(HELP_NODE("Main")); break;
		case MID_ABOUT: do_about(); break;
		case MID_QUIT: if (may_leave()) return FALSE; break;
		case MID_ZOOMIN: zoom(1); break;
		case MID_ZOOMOUT: zoom(-1); break;
		case MID_ALL: set1(g_Wave, WAVEFORM_ShowAll, TRUE); sync_views(); break;
		case MID_ZOOMSEL: zoom_selection(); break;
		case MID_UNDO: undo_step(FALSE); break;
		case MID_REDO: undo_step(TRUE); break;
		case MID_CUTOUT: cut_out(); break;
		case MID_FADEIN: fade_part(FALSE); break;
		case MID_FADEOUT: fade_part(TRUE); break;
		case MID_GAIN: gain_part(); break;
		case MID_NORMALISE: normalise_part(); break;
		case MID_PLAIN: plain_part(); break;
		case MID_SELALL: select_range(0, Doc_Length(&g_Doc)); break;
		case MID_SELNONE: select_range(WAVEFORM_NONE, WAVEFORM_NONE); break;
		case MID_ATIN: at_handle(FALSE); break;
		case MID_ATOUT: at_handle(TRUE); break;
		case MID_PREVMARK: jump_marker(FALSE); break;
		case MID_NEXTMARK: jump_marker(TRUE); break;
		case MID_SCALE:
			g_DB = (BOOL) ((item->Flags & CHECKED) != 0);
			set1(g_Wave, WAVEFORM_Scale, g_DB ? WAVEFORM_SCALE_DB : WAVEFORM_SCALE_LINEAR);
			break;
		case MID_SNAP: g_Prefs.ep_Snap = (BOOL) ((item->Flags & CHECKED) != 0); trace_prefs(); break;
		case MID_BACKUP:
			g_Prefs.ep_Backup = (BOOL) ((item->Flags & CHECKED) != 0);
			Doc_SetBackup(g_Prefs.ep_Backup);
			trace_prefs();
			break;
		case MID_ICONS: g_Prefs.ep_Icons = (BOOL) ((item->Flags & CHECKED) != 0); trace_prefs(); break;
		case MID_MARGINS: do_margins(); break;
		case MID_WAYAUTO: case MID_WAYCARD: case MID_WAYAHI:
			/* the one ticked now (a radio: the others went) */
			if (item->Flags & CHECKED) {
				g_Prefs.ep_Play = (UWORD) (mid == MID_WAYAUTO ? EDITPREFS_PLAY_AUTO :
							   mid == MID_WAYCARD ? EDITPREFS_PLAY_CARD : EDITPREFS_PLAY_AHI);
				stop_play();
				play_way();
				ghost_file(!doc_usable());
			}
			break;
		case MID_AHIMODE: ahi_mode(); break;
		case MID_ADDMARK: add_marker(); break;
		case MID_DELMARK: del_marker(); break;
		case MID_FINDTRACKS: find_tracks(); break;
		case MID_TRIMWIN: trim_open(); break;
		case MID_SPLITMARKS: do_split_marks(); break;
		case MID_PLAY: if (doc_usable() && Play_There()) play_toggle(); break;
		case MID_PLAYSEL: if (doc_usable() && Play_There()) play_selection(); break;
		case MID_STOP: stop_button(); break;
		case MID_LISTENIN: listen(FALSE); break;
		case MID_LISTENOUT: listen(TRUE); break;
		case MID_LOOP: loop_set((BOOL) ((item->Flags & CHECKED) != 0), TRUE); break;
		case MID_SAVEPREFS: save_prefs(); break;
		}
	}
	return TRUE;
}

/* --- the markers and the cue sheet ------------------------------------------------- */

/* <sound>.cue: the sound's name, its suffix off */
static void cue_path(CONST_STRPTR sound, char *buf)
{
	STRPTR p, dot = NULL;

	Text_Copy((STRPTR) buf, sound, DOC_PATHLEN - 5);
	for (p = (STRPTR) FilePart((STRPTR) buf); *p; p++) {
		if (*p == '.') dot = p;
	}
	Text_Put(dot ? dot : p, (CONST_STRPTR) ".cue");
}

/* the markers onto the trace: how many, the one clicked, the first four */
static void trace_marks(void)
{
	struct TraceNums tn;
	UWORD i;

	tn.tn_What = (CONST_STRPTR) "marks count sel";
	tn.tn_N[0] = g_Marks.mk_Count;
	tn.tn_N[1] = g_MarkSel;
	tn.tn_Count = 2;
	trace_nums(&tn);
	tn.tn_What = (CONST_STRPTR) "markslist first";
	for (i = 0; i < 4; i++) tn.tn_N[i] = i < g_Marks.mk_Count ? g_Marks.mk_Frame[i] : 0;
	tn.tn_Count = 4;
	trace_nums(&tn);
}

/* the cue sheet beside the file opened (the user's: a decoded file's
   too) as the markers -- none there: none; frames past the sound's end
   dropped */
static void cue_read(void)
{
	static struct CueRead cr;
	struct MarkSet ms;
	struct Process *me = (struct Process *) FindTask(NULL);
	APTR window = me->pr_WindowPtr;
	char path[DOC_PATHLEN];
	STRPTR text;
	BPTR fh;
	LONG n = 0;

	Marks_Clear(&g_Marks);
	g_MarkSel = MARKS_NONE;
	cue_path((CONST_STRPTR) g_Source, path);
	me->pr_WindowPtr = (APTR) -1L;		/* no requester for a sheet not there */
	fh = Open((STRPTR) path, MODE_OLDFILE);
	me->pr_WindowPtr = window;
	if (!fh) return;
	/* the buffer off the heap: the statics share SAS/C's 64K */
	if ((text = (STRPTR) AllocVec(CUE_TEXTMAX * 2, MEMF_ANY))) n = Read(fh, text, CUE_TEXTMAX * 2);
	Close(fh);
	if (n > 0) {
		cr.cr_Rate = g_Doc.d_Rate;
		cr.cr_Frames = Doc_Length(&g_Doc);
		Cue_Read(&cr, (CONST_STRPTR) text, (ULONG) n);
		ms.ms_Frames = cr.cr_Marks;
		ms.ms_Count = cr.cr_Count;
		ms.ms_End = Doc_Length(&g_Doc);
		Marks_Set(&g_Marks, &ms);
		trace_marks();
	}
	if (text) FreeVec(text);
}

/* the markers between in and out as the cue sheet of the file written
   from that stretch (their frames from in), beside it; without any, an
   old sheet there goes (it would cut a file that is not there any more);
   Keep a backup?: the old sheet as <name>.cue.bak, as the file's own */
static void cue_write(const struct CueOut *co)
{
	static ULONG marks[MARKS_MAX];
	char path[DOC_PATHLEN], side[DOC_PATHLEN + 6];
	struct CueSheet cs;
	struct TraceNums tn;
	STRPTR text = NULL;
	ULONG len;
	LONG err = 0;
	UWORD i, n = 0;
	BPTR fh;

	for (i = 0; i < g_Marks.mk_Count; i++) {
		if (g_Marks.mk_Frame[i] > co->co_In && g_Marks.mk_Frame[i] < co->co_Out) marks[n++] = g_Marks.mk_Frame[i] - co->co_In;
	}
	cue_path(co->co_Sound, path);
	/* the backup's and the temporary file's names cut to fit FFS's 30
	   characters, apart from the sheet's own (beside.h) */
	if (co->co_Backup && Beside_BackupName((CONST_STRPTR) path, (STRPTR) side)) {
		DeleteFile((STRPTR) side);
		Rename((STRPTR) path, (STRPTR) side);
	}
	if (n && !(text = (STRPTR) AllocVec(CUE_TEXTMAX, MEMF_ANY))) {
		err = ERROR_NO_FREE_STORE;
	} else if (n) {
		cs.cs_File = (CONST_STRPTR) FilePart((STRPTR) co->co_Sound);
		cs.cs_Rate = co->co_Rate;
		cs.cs_Marks = marks;
		cs.cs_Count = n;
		len = Cue_Write(&cs, text, CUE_TEXTMAX);
		/* complete beside it before it takes the old one's place */
		if (!Beside_Temp((CONST_STRPTR) path, (STRPTR) side) || !(fh = Open((STRPTR) side, MODE_NEWFILE))) {
			err = IoErr();
		} else {
			if (Write(fh, text, (LONG) len) != (LONG) len) err = IoErr();
			if (!Close(fh) && !err) err = IoErr();
			if (!err && !DeleteFile((STRPTR) path) && IoErr() != ERROR_OBJECT_NOT_FOUND) err = IoErr();
			if (!err && !Rename((STRPTR) side, (STRPTR) path)) err = IoErr();
			else if (err) DeleteFile((STRPTR) side);
		}
		FreeVec(text);
	} else {
		DeleteFile((STRPTR) path);
	}
	tn.tn_What = (CONST_STRPTR) "cue written err";
	tn.tn_N[0] = n;
	tn.tn_N[1] = (ULONG) err;
	tn.tn_Count = 2;
	trace_nums(&tn);
	if (err) say1((CONST_STRPTR) TXT_E_CUE, NULL, err);
}

/* Edit, Add marker (Return or Enter): a track starts at the play marker
   -- while it plays, where the card is (tapped along with the music);
   stopped, on a zero crossing as the handles are */
static void add_marker(void)
{
	char t[TIME_LEN];
	ULONG f;
	UWORD i;

	if (!doc_usable()) return;
	if (Play_Playing()) f = Play_Frame();
	else if ((f = get(g_Wave, WAVEFORM_Cursor)) != WAVEFORM_NONE) f = snapped(f);
	if (f == WAVEFORM_NONE) {
		set_status((CONST_STRPTR) TXT_ST_MARK_WHERE);
		return;
	}
	if ((i = Marks_Add(&g_Marks, f, Doc_Length(&g_Doc))) == MARKS_NONE) {
		set_status((CONST_STRPTR) (g_Marks.mk_Count >= MARKS_MAX ? TXT_ST_MARKS_FULL : TXT_ST_MARK_WHERE));
		return;
	}
	g_MarkSel = i;
	regions_shown();
	changed();
	trace_marks();
	time_text(g_Marks.mk_Frame[i], t);
	say1((CONST_STRPTR) TXT_ST_MARKED, (CONST_STRPTR) t, (LONG) i + 2);
}

/* Edit, Remove marker (Del): the marker clicked */
static void del_marker(void)
{
	if (!doc_usable()) return;
	if (g_MarkSel == MARKS_NONE || !Marks_Remove(&g_Marks, g_MarkSel)) {
		set_status((CONST_STRPTR) TXT_ST_MARK_NOSEL);
		return;
	}
	g_MarkSel = MARKS_NONE;
	regions_shown();
	changed();
	trace_marks();
	set_status((CONST_STRPTR) TXT_ST_UNMARKED);
}

/* a marker let go: clicked, it is the one Del takes away; dragged, it
   moves there (a zero crossing; the start or the end: it goes) */
static void marker_up(void)
{
	LONG idx = (LONG) get(g_Wave, WAVEFORM_ActiveMarker);
	ULONG f = get(g_Wave, WAVEFORM_ActiveFrame);
	struct MarkMove mm;

	/* the joins after the markers: fixed, never dragged */
	if (idx < 0 || idx >= (LONG) g_Marks.mk_Count) return;
	if (f == g_Marks.mk_Frame[idx]) {
		g_MarkSel = (UWORD) idx;
		regions_shown();
		trace_marks();
		say1((CONST_STRPTR) TXT_ST_MARK_PICKED, NULL, idx + 2);
		return;
	}
	mm.mm_Index = (UWORD) idx;
	mm.mm_Frame = snapped(f);
	mm.mm_End = Doc_Length(&g_Doc);
	g_MarkSel = Marks_Move(&g_Marks, &mm);
	regions_shown();
	changed();
	trace_marks();
}

/* Edit, Find tracks (the user's word, never by itself): a marker at each
   track's start after the first, as the suggestion's gap detector finds
   them -- a long file read ends first measured through its middle first
   (the gauge, Cancel); the markers there before replaced, one step */
static void find_tracks(void)
{
	static struct TrimPlan tp;
	struct MarkSet ms;
	ULONG in = trim_in(), out = trim_out(), f;
	static ULONG at[MARKS_MAX];
	LONG err;
	UWORD i, n = 0;

	if (!doc_usable()) return;
	if (g_Reading) {
		say1((CONST_STRPTR) TXT_ST_CUTREADING, file_name(), 0);
		return;
	}
	stop_play();
	prefetch_off();
	if (!g_Doc.d_LevelWhole) {
		g_CancelSave = FALSE;
		g_ProgressText = (CONST_STRPTR) TXT_ST_MEASURING;
		job_on(TRUE);
		job_said(g_ProgressText, 0);
		g_Doc.d_SaveProgress = save_progress;
		err = Doc_MeasureLevel(&g_Doc);
		g_ProgressText = NULL;
		set1(g_Gauge, FUELGAUGE_Level, 0);
		job_on((BOOL) (g_Reading || g_Decoding));
		if (err) {
			if (err == DOC_E_CANCELLED) set_status((CONST_STRPTR) TXT_ST_FIND_CANCELLED);
			else say1((CONST_STRPTR) TXT_E_DOS, file_name(), err);
			return;
		}
	}
	if (!Doc_Tracks(&g_Doc, &tp)) {
		set_status((CONST_STRPTR) TXT_ST_NOTRACKS);
		return;
	}
	/* where they play on the timeline, between In and Out */
	for (i = 1; i < tp.tp_TrackCount && n < MARKS_MAX; i++) {
		f = Tl_Find(&g_Doc.d_Tl, tp.tp_TrackStart[i]);
		if (f != TL_NONE && f > in && f < out) at[n++] = f;
	}
	ms.ms_Frames = at;
	ms.ms_Count = n;
	ms.ms_End = Doc_Length(&g_Doc);
	Marks_Set(&g_Marks, &ms);
	g_MarkSel = MARKS_NONE;
	regions_shown();
	changed();
	trace_marks();
	say1((CONST_STRPTR) TXT_ST_TRACKS, NULL, (LONG) n + 1);
}

/* a part's name beside the original: <name>-<n>.wav, its extension
   gone (DOC_PATHLEN) */
static void part_name(char *buf, UWORD n)
{
	STRPTR p, dot = NULL;
	char tail[8];

	Text_Copy((STRPTR) buf, (CONST_STRPTR) g_Source, DOC_PATHLEN - 8);
	for (p = (STRPTR) FilePart((STRPTR) buf); *p; p++) {
		if (*p == '.') dot = p;
	}
	tail[0] = '-';
	tail[1] = (char) ('0' + n);
	tail[2] = '\0';
	p = Text_Put(dot ? dot : p, (CONST_STRPTR) tail);
	Text_Put(p, (CONST_STRPTR) ".wav");
}

static BOOL there(CONST_STRPTR path)
{
	BPTR lock = Lock((STRPTR) path, SHARED_LOCK);

	if (!lock) return FALSE;
	UnLock(lock);
	return TRUE;
}

/* a part of a split written: in .. out - 1 of the timeline into path,
   the take's level map cut beside it */
struct SplitPart {
	ULONG pa_In, pa_Out;
	CONST_STRPTR pa_Path;
};

static LONG write_part(const struct SplitPart *pa)
{
	struct DocSave ds;
	struct MapCut mc;
	LONG err;

	ds.ds_In = pa->pa_In;
	ds.ds_Out = pa->pa_Out;
	ds.ds_Path = pa->pa_Path;
	/* the status line names the part being written (and the trace, for
	   the tests: not "saving ...", which they count as per cents) */
	g_JobName = (CONST_STRPTR) FilePart((STRPTR) pa->pa_Path);
	if (g_TracePath[0]) {
		char line[DOC_PATHLEN + 12];

		Text_Put(Text_Put((STRPTR) line, (CONST_STRPTR) "partname "), g_JobName);
		trace((CONST_STRPTR) line);
	}
	err = Doc_SaveAs(&g_Doc, &ds);
	g_JobName = NULL;
	changed();			/* a handle dragged while it copied: its step */
	if (err) return err;
	clear_bytes((UBYTE *) &mc, sizeof(mc));
	mc.mc_From = (CONST_STRPTR) g_Source;
	mc.mc_To = pa->pa_Path;
	mc.mc_In = pa->pa_In;
	mc.mc_Out = pa->pa_Out;
	mc.mc_Rate = g_Doc.d_Rate;
	mc.mc_Levels = Tl_Levels(&g_Doc.d_Levels);
	mc.mc_Tl = (Tl_Whole(&g_Doc.d_Tl, g_Doc.d_Frames) && !mc.mc_Levels) ? NULL : &g_Doc.d_Tl;
	cut_map(&mc);
	return 0;
}

/* Project/Split at the cursor (E4): the timeline between In and Out as
   two files beside the original -- <name>-1.wav up to the play marker
   (on a zero crossing), <name>-2.wav from it; the original and the doc
   as they are; asked first when the parts are there already */
static void do_split(void)
{
	ULONG in = get(g_Wave, WAVEFORM_TrimIn), out = get(g_Wave, WAVEFORM_TrimOut), c = get(g_Wave, WAVEFORM_Cursor);
	char one[DOC_PATHLEN], two[DOC_PATHLEN];
	struct TraceNums tn;
	struct SplitPart pa;
	struct Say sy;
	LONG err;

	if (!doc_usable()) return;
	if (g_Reading) {
		say1((CONST_STRPTR) TXT_ST_CUTREADING, file_name(), 0);
		return;
	}
	if (in == WAVEFORM_NONE) in = 0;
	if (out == WAVEFORM_NONE) out = Doc_Length(&g_Doc);
	if (c != WAVEFORM_NONE && c > in && c < out) c = snapped(c);
	if (c == WAVEFORM_NONE || c <= in || c >= out) {
		set_status((CONST_STRPTR) TXT_ST_SPLIT_WHERE);
		return;
	}
	part_name(one, 1);
	part_name(two, 2);
	stop_play();
	prefetch_off();
	if ((there((CONST_STRPTR) one) || there((CONST_STRPTR) two)) &&
	    ask((CONST_STRPTR) TXT_ASK_SPLIT_OVER, (CONST_STRPTR) TXT_ASK_SPLIT_OVER_GADS) != 1)
		return;
	g_CancelSave = FALSE;
	job_on(TRUE);
	g_Doc.d_SaveProgress = save_progress;
	trace_save_pieces();
	pa.pa_In = in;
	pa.pa_Out = c;
	pa.pa_Path = (CONST_STRPTR) one;
	if (!(err = write_part(&pa))) {
		pa.pa_In = c;
		pa.pa_Out = out;
		pa.pa_Path = (CONST_STRPTR) two;
		/* no half a split: the first part goes with the second */
		if ((err = write_part(&pa))) {
			char map[DOC_PATHLEN];

			DeleteFile((STRPTR) one);
			if (LevelMap_Name((CONST_STRPTR) one, (STRPTR) map, DOC_PATHLEN)) DeleteFile((STRPTR) map);
		}
	}
	set1(g_Gauge, FUELGAUGE_Level, 0);
	job_on((BOOL) (g_Reading || g_Decoding));
	/* the icons before the trace's line: it says the split is done */
	if (!err) {
		make_icon((CONST_STRPTR) one);
		make_icon((CONST_STRPTR) two);
	}
	tn.tn_What = (CONST_STRPTR) "split in at out err";
	tn.tn_N[0] = in;
	tn.tn_N[1] = c;
	tn.tn_N[2] = out;
	tn.tn_N[3] = (ULONG) err;
	tn.tn_Count = 4;
	trace_nums(&tn);
	if (err == DOC_E_CANCELLED) {
		set_status((CONST_STRPTR) TXT_ST_SPLIT_CANCELLED);
	} else if (err) {
		save_error(err);
	} else {
		sy.sy_Fmt = (CONST_STRPTR) TXT_ST_SPLIT;
		sy.sy_S0 = (CONST_STRPTR) FilePart((STRPTR) one);
		sy.sy_S1 = (CONST_STRPTR) FilePart((STRPTR) two);
		sy.sy_N0 = sy.sy_N1 = 0;
		say(&sy);
	}
}

/* a track's name beside the original: <name>-NN.wav (SplitTracks'),
   its extension gone */
static void track_name(char *buf, UWORD n)
{
	STRPTR p, dot = NULL;
	char tail[6];

	Text_Copy((STRPTR) buf, (CONST_STRPTR) g_Source, DOC_PATHLEN - 9);
	for (p = (STRPTR) FilePart((STRPTR) buf); *p; p++) {
		if (*p == '.') dot = p;
	}
	tail[0] = '-';
	tail[1] = (char) ('0' + n / 10 % 10);
	tail[2] = (char) ('0' + n % 10);
	tail[3] = '\0';
	p = Text_Put(dot ? dot : p, (CONST_STRPTR) tail);
	Text_Put(p, (CONST_STRPTR) ".wav");
}

/* Project, Split at markers: In to the first marker, from it to the
   next ... the last to Out, each a file beside the original --
   <name>-01.wav, -02 ... (the cuts in them, each with its level map);
   the original and the doc as they are; asked first when one is there.
   One that cannot be written takes the ones before it with it */
static void do_split_marks(void)
{
	static ULONG cuts[MARKS_MAX + 2];
	ULONG in = trim_in(), out = trim_out();
	char name[DOC_PATHLEN], map[DOC_PATHLEN];
	struct TraceNums tn;
	struct SplitPart pa;
	BOOL any = FALSE;
	LONG err = 0;
	UWORD n = 0, i, k, done = 0;

	if (!doc_usable()) return;
	if (g_Reading) {
		say1((CONST_STRPTR) TXT_ST_CUTREADING, file_name(), 0);
		return;
	}
	cuts[n++] = in;
	for (i = 0; i < g_Marks.mk_Count; i++) {
		if (g_Marks.mk_Frame[i] > in && g_Marks.mk_Frame[i] < out) cuts[n++] = g_Marks.mk_Frame[i];
	}
	cuts[n++] = out;
	if (n < 3) {
		set_status((CONST_STRPTR) TXT_ST_SPLITMARKS_WHERE);
		return;
	}
	for (k = 1; k < n; k++) {
		track_name(name, k);
		if (there((CONST_STRPTR) name)) any = TRUE;
	}
	stop_play();
	prefetch_off();
	if (any && ask((CONST_STRPTR) TXT_ASK_SPLITMARKS_OVER, (CONST_STRPTR) TXT_ASK_SPLIT_OVER_GADS) != 1) return;
	g_CancelSave = FALSE;
	job_on(TRUE);
	g_Doc.d_SaveProgress = save_progress;
	trace_save_pieces();
	for (k = 1; k < n && !err; k++) {
		track_name(name, k);
		tn.tn_What = (CONST_STRPTR) "trackpart n first end";
		tn.tn_N[0] = k;
		tn.tn_N[1] = cuts[k - 1];
		tn.tn_N[2] = cuts[k];
		tn.tn_Count = 3;
		trace_nums(&tn);
		pa.pa_In = cuts[k - 1];
		pa.pa_Out = cuts[k];
		pa.pa_Path = (CONST_STRPTR) name;
		if (!(err = write_part(&pa))) done = k;
	}
	if (err) {
		/* no half a split: the parts written go */
		for (k = 1; k <= done; k++) {
			track_name(name, k);
			DeleteFile((STRPTR) name);
			if (LevelMap_Name((CONST_STRPTR) name, (STRPTR) map, DOC_PATHLEN)) DeleteFile((STRPTR) map);
		}
	}
	set1(g_Gauge, FUELGAUGE_Level, 0);
	job_on((BOOL) (g_Reading || g_Decoding));
	/* the icons before the trace's line: it says the split is done */
	for (k = 1; !err && k < n; k++) {
		track_name(name, k);
		make_icon((CONST_STRPTR) name);
	}
	tn.tn_What = (CONST_STRPTR) "splitmarks parts err";
	tn.tn_N[0] = (ULONG) n - 1;
	tn.tn_N[1] = (ULONG) err;
	tn.tn_Count = 2;
	trace_nums(&tn);
	if (err == DOC_E_CANCELLED) {
		set_status((CONST_STRPTR) TXT_ST_SPLIT_CANCELLED);
	} else if (err) {
		save_error(err);
	} else {
		track_name(name, 1);
		say1((CONST_STRPTR) TXT_ST_SPLITMARKS, (CONST_STRPTR) FilePart((STRPTR) name), (LONG) n - 1);
	}
}

/* --- building it ---------------------------------------------------------------------- */

static Object *button(UWORD id, CONST_STRPTR text)
{
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_Text, (ULONG) text, TAG_END);
}

/* a text's width in the screen's font, which the gadgets take */
static WORD text_w(struct Screen *scr, CONST_STRPTR s)
{
	UWORD n = (UWORD) Text_Length(s);

	return (WORD) (scr ? TextLength(&scr->RastPort, (STRPTR) s, n) : n * 8);
}

/* a button's width: its label and some air (BUTTON_TextPadding's air put
   the row of nine past 640 points) -- a floor above its content */
static ULONG btn_w(struct Screen *scr, CONST_STRPTR s)
{
	return (ULONG) (text_w(scr, s) + 16);
}

/* the size gadget's height: a window's bottom border with WA_SizeBBottom */
static WORD size_height(struct Screen *scr)
{
	struct DrawInfo *dri;
	Object *im;
	ULONG h = 10;

	if (!(dri = GetScreenDrawInfo(scr))) return (WORD) h;
	if ((im = NewObject(NULL, (STRPTR) SYSICLASS, SYSIA_DrawInfo, (ULONG) dri, SYSIA_Which, SIZEIMAGE,
			    SYSIA_Size, (scr->Flags & SCREENHIRES) ? SYSISIZE_MEDRES : SYSISIZE_LOWRES, TAG_END))) {
		GetAttr(IA_Height, im, &h);
		DisposeObject(im);
	}
	FreeScreenDrawInfo(scr, dri);
	return (WORD) h;
}

static Object *spacer(void)
{
	return NewObject(LAYOUT_GetClass(), NULL, TAG_END);
}

/* tapedeck.gadget's width for all five keys (PrismaRec, measured): a key
   a fifth of it -- the Loop key and the lenses as wide */
#define DECK_W		156
#define KEY_W		(DECK_W / 5)

/* a bar key wearing a drawn mark (keyglyph.h): the image kept here, the
   button never disposes it; Loop a toggle. No class or image: the
   words */
/* the Loop sign's height: a row above the font's -- half again on a
   screen of square pixels, where rows are cheap and the bar grows with
   it (too flat there: the user, A1200, 2 Nov 2026) */
static UWORD loop_size(struct Screen *scr)
{
	UWORD h = (UWORD) (scr->Font->ta_YSize + 1);

	if (KeyGlyph_SquarePixels(scr)) h = (UWORD) (h + h / 2);
	return h;
}

static Object *glyph_key(UWORD id, ULONG kind, struct Screen *scr)
{
	Object *im = NULL;
	CONST_STRPTR words;

	if (g_GlyphClass && scr)
		im = g_Glyph[kind] = NewObject(g_GlyphClass, NULL, KEYGLYPH_Kind, kind,
					       KEYGLYPH_Size, (ULONG) (kind == KG_LOOP ? loop_size(scr) : scr->Font->ta_YSize + 1),
					       KEYGLYPH_Screen, (ULONG) scr,
					       /* inside the key's bevel, 2 a side (measured) */
					       KEYGLYPH_Width, (ULONG) (KEY_W - 4), TAG_END);
	if (!im) {
		switch (kind) {
		case KG_LOOP: words = (CONST_STRPTR) TXT_LOOP; break;
		case KG_ZOOMOUT: words = (CONST_STRPTR) TXT_ZOOMOUT; break;
		case KG_ZOOMIN: words = (CONST_STRPTR) TXT_ZOOMIN; break;
		default: words = (CONST_STRPTR) TXT_ALL; break;
		}
		return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_Text, (ULONG) words,
				 BUTTON_PushButton, (ULONG) (kind == KG_LOOP), TAG_END);
	}
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, id, GA_RelVerify, TRUE, GA_Image, (ULONG) im,
			 BUTTON_PushButton, (ULONG) (kind == KG_LOOP), TAG_END);
}

/* a handle's flag key: its pennant in the waveform's handle pen, its word
   (the image ours, as the others'); a push button, picked or not */
static Object *flag_key(BOOL out, struct Screen *scr)
{
	Object *im = NULL;
	UWORD slot = (UWORD) (out ? GLYPHS - 1 : GLYPHS - 2);
	CONST_STRPTR word = (CONST_STRPTR) (out ? TXT_OUT : TXT_IN);

	if (g_GlyphClass && scr)
		im = g_Glyph[slot] = NewObject(g_GlyphClass, NULL, KEYGLYPH_Kind, KG_FLAG, KEYGLYPH_Size, (ULONG) (scr->Font->ta_YSize + 1),
					       KEYGLYPH_Screen, (ULONG) scr, KEYGLYPH_Text, (ULONG) word,
					       KEYGLYPH_PenPtr, (ULONG) &g_FlagPen[out ? 1 : 0], TAG_END);
	if (!im)
		return NewObject(BUTTON_GetClass(), NULL, GA_ID, out ? GID_FLAGOUT : GID_FLAGIN, GA_RelVerify, TRUE,
				 GA_Text, (ULONG) word, BUTTON_PushButton, TRUE, TAG_END);
	return NewObject(BUTTON_GetClass(), NULL, GA_ID, out ? GID_FLAGOUT : GID_FLAGIN, GA_RelVerify, TRUE,
			 GA_Image, (ULONG) im, BUTTON_PushButton, TRUE, TAG_END);
}

static Object *make_window(struct Screen *scr)
{
	Object *bar, *status, *root, *win, *play, *lenses, *flags, *tools;
	WORD w = 620, h = 205, l = 0, t = 0, bw, bh, room;
	/* the length's read-out: its bevel and a margin (4 and 2 a side,
	   measured) round "00:00.000" -- an hour and more is said to the
	   tenth, as wide (readout_text()) */
	ULONG tw = (ULONG) (text_w(scr, (CONST_STRPTR) "00:00.000") + 12);
	/* square pixels: the Loop key as tall as its sign, its bevel and a
	   row each side -- the bar's row with it */
	ULONG tall = (ULONG) (scr && KeyGlyph_SquarePixels(scr) ? loop_size(scr) + 6 : 0);

	g_Over = NewObject(g_WaveClass, NULL, GA_ID, GID_OVER, GA_RelVerify, TRUE,
			   WAVEFORM_Overview, TRUE, WAVEFORM_BoxStyle, WAVEFORM_BOX_FRAME,
			   WAVEFORM_TrimShade, WAVEFORM_SHADE_DARKEN,
			   /* In and Out to drag there too, by their lines (no flags) */
			   WAVEFORM_TrimHandles, WAVEFORM_HANDLES_DRAG,
			   WAVEFORM_SignalTask, (ULONG) FindTask(NULL), WAVEFORM_SignalMask, g_SigMask, TAG_END);
	g_Wave = NewObject(g_WaveClass, NULL, GA_ID, GID_WAVE, GA_RelVerify, TRUE,
			   WAVEFORM_Lanes, WAVEFORM_LANES_AUTO, WAVEFORM_LaneLabels, (ULONG) g_LaneLabels,
			   WAVEFORM_HandleLabels, (ULONG) g_HandleLabels,
			   /* the view a page on when the play head leaves it */
			   WAVEFORM_Follow, WAVEFORM_FOLLOW_PAGE,
			   /* a join's mark: a thin line, fixed (concept 3.1) */
			   WAVEFORM_MarkerStyle, WAVEFORM_HANDLE_LINE, WAVEFORM_DragMarkers, TRUE,
			   WAVEFORM_SignalTask, (ULONG) FindTask(NULL), WAVEFORM_SignalMask, g_SigMask, TAG_END);
	g_Scroll = NewObject(SCROLLER_GetClass(), NULL, GA_ID, GID_SCROLL, GA_RelVerify, TRUE,
			     SCROLLER_Orientation, SORIENT_HORIZ, SCROLLER_Total, 1, SCROLLER_Visible, 1,
			     SCROLLER_Top, 0, SCROLLER_Arrows, TRUE,
			     SCROLLER_SignalTask, (ULONG) FindTask(NULL), SCROLLER_SignalTaskBit, g_SigMask, TAG_END);
	/* the bar under the scroller (concept Panel, V1): the deck and the
	   Loop key; the length, a sunk read-out (its bubble the fades);
	   Trim..., the lenses -- one row, the scroller as long as the sound */
	g_Len = NewObject(BUTTON_GetClass(), NULL, GA_ID, GID_LEN, GA_ReadOnly, TRUE, GA_Text, (ULONG) g_LenText,
			  BUTTON_BevelStyle, BVS_FIELD, BUTTON_Justification, BCJ_LEFT, TAG_END);
	/* TDECK_Tape TRUE: the five keys of a tape deck (PrismaRec's deck) */
	g_Deck = NewObject(NULL, (STRPTR) "tapedeck.gadget", GA_ID, GID_DECK, GA_RelVerify, TRUE,
			   TDECK_Tape, TRUE, TDECK_Mode, BUT_STOP, TAG_END);
	g_LoopKey = glyph_key(GID_LOOP, KG_LOOP, scr);
	g_Flag[0] = flag_key(FALSE, scr);
	g_Flag[1] = flag_key(TRUE, scr);
	if (g_Flag[0]) SetAttrs(g_Flag[0], GA_Selected, TRUE, TAG_DONE);
	g_Pick = FALSE;
	if (g_LoopKey) SetAttrs(g_LoopKey, GA_Selected, (ULONG) g_Loop, TAG_DONE);
	g_TrimBtn = button(GID_TRIMWIN, (CONST_STRPTR) TXT_M_TRIMWIN);
	g_Lens[0] = glyph_key(GID_ZOOMOUT, KG_ZOOMOUT, scr);
	g_Lens[1] = glyph_key(GID_ZOOMIN, KG_ZOOMIN, scr);
	g_Lens[2] = glyph_key(GID_ALL, KG_ALL, scr);
	/* the Loop key touching the deck, a sixth key (concept Panel, 7) */
	play = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, FALSE,
			 LAYOUT_AddChild, (ULONG) g_Deck, CHILD_WeightedWidth, 0, CHILD_MinWidth, DECK_W,
			 LAYOUT_AddChild, (ULONG) g_LoopKey, CHILD_WeightedWidth, 0, CHILD_MinWidth, KEY_W,
			 tall ? CHILD_MinHeight : TAG_IGNORE, tall, TAG_END);
	/* the two flags touching, a pair */
	flags = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, FALSE,
			  LAYOUT_AddChild, (ULONG) g_Flag[0], CHILD_WeightedWidth, 0,
			  LAYOUT_AddChild, (ULONG) g_Flag[1], CHILD_WeightedWidth, 0, TAG_END);
	/* the lenses touching, as the deck's keys */
	lenses = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, FALSE,
			   LAYOUT_AddChild, (ULONG) g_Lens[0], CHILD_WeightedWidth, 0, CHILD_MinWidth, KEY_W,
			   LAYOUT_AddChild, (ULONG) g_Lens[1], CHILD_WeightedWidth, 0, CHILD_MinWidth, KEY_W,
			   LAYOUT_AddChild, (ULONG) g_Lens[2], CHILD_WeightedWidth, 0, CHILD_MinWidth, KEY_W, TAG_END);
	/* Trim... and the lenses together at the right */
	tools = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			  LAYOUT_AddChild, (ULONG) g_TrimBtn, CHILD_WeightedWidth, 0, CHILD_MinWidth, btn_w(scr, (CONST_STRPTR) TXT_M_TRIMWIN),
			  LAYOUT_AddChild, (ULONG) lenses, CHILD_WeightedWidth, 0, TAG_END);
	/* the length's read-out without a word (its bubble says "Length":
	   the room went to the flags, the user's choice, 2 Nov 2026), midway
	   between the flags and Trim... -- apart from the flags, so it does
	   not read as a handle's time */
	bar = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			LAYOUT_AddChild, (ULONG) play, CHILD_WeightedWidth, 0,
			LAYOUT_AddChild, (ULONG) flags, CHILD_WeightedWidth, 0,
			LAYOUT_AddChild, (ULONG) spacer(), CHILD_WeightedWidth, 50,
			LAYOUT_AddChild, (ULONG) g_Len, CHILD_WeightedWidth, 0, CHILD_MinWidth, tw,
			LAYOUT_AddChild, (ULONG) spacer(), CHILD_WeightedWidth, 50,
			LAYOUT_AddChild, (ULONG) tools, CHILD_WeightedWidth, 0, TAG_END);
	g_Gauge = NewObject(FUELGAUGE_GetClass(), NULL, GA_ID, GID_GAUGE, FUELGAUGE_Min, 0, FUELGAUGE_Max, 100,
			    FUELGAUGE_Level, 0, FUELGAUGE_Ticks, 0, FUELGAUGE_Percent, FALSE,
			    FUELGAUGE_Justification, FGJ_LEFT, GA_Text, (ULONG) g_Status[0], TAG_END);
	/* the status line, Cancel beside it for a job that runs */
	g_Cancel = button(GID_CANCEL, (CONST_STRPTR) TXT_CANCEL);
	status = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ, LAYOUT_SpaceInner, TRUE,
			   LAYOUT_AddChild, (ULONG) g_Gauge, CHILD_WeightedWidth, 100,
			   LAYOUT_AddChild, (ULONG) g_Cancel, CHILD_WeightedWidth, 0, CHILD_MinWidth, btn_w(scr, (CONST_STRPTR) TXT_CANCEL),
			   TAG_END);
	root = NewObject(LAYOUT_GetClass(), NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
			 LAYOUT_SpaceOuter, TRUE, LAYOUT_SpaceInner, TRUE, LAYOUT_DeferLayout, TRUE,
			 GA_GadgetHelp, TRUE,
			 /* the overview twice its own least height: easier to hit;
			    a weight bar under it shares the height out between it
			    and the waveform as the user drags (layout.gadget moves
			    the weights itself), so both of them carry a weight */
			 LAYOUT_AddChild, (ULONG) g_Over, CHILD_WeightedHeight, g_Prefs.ep_Overview, CHILD_MinHeight, OVER_HEIGHT,
			 CHILD_WeightBar, TRUE,
			 LAYOUT_AddChild, (ULONG) g_Wave, CHILD_WeightedHeight, 100 - g_Prefs.ep_Overview,
			 LAYOUT_AddChild, (ULONG) g_Scroll, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) bar, CHILD_WeightedHeight, 0,
			 LAYOUT_AddChild, (ULONG) status, CHILD_WeightedHeight, 0,
			 TAG_END);
	if (!root) return NULL;		/* a failed layout disposes its children */
	g_Root = root;
	if (scr) {
		/* the screen below its bar, 1000 x 420 inside at most, centred:
		   window.class takes WA_Width and WA_Height as the INSIDE, so
		   the borders come off first (the size gadget's in the bottom) */
		bw = (WORD) (scr->WBorLeft + scr->WBorRight);
		bh = (WORD) (scr->WBorTop + scr->Font->ta_YSize + 1 + size_height(scr));
		room = (WORD) (scr->Height - scr->BarHeight - 1);
		w = (WORD) (scr->Width - bw > 1000 ? 1000 : scr->Width - bw);
		h = (WORD) (room - bh > 420 ? 420 : room - bh);
		l = (WORD) ((scr->Width - w - bw) / 2);
		t = (WORD) (scr->BarHeight + 1 + (room - h - bh) / 2);
	}
	win = NewObject(WINDOW_GetClass(), NULL,
			 WA_Title, (ULONG) g_Title,
			 WA_Activate, TRUE,
			 WA_DragBar, TRUE,
			 WA_DepthGadget, TRUE,
			 WA_CloseGadget, TRUE,
			 WA_SizeGadget, TRUE,
			 WA_SizeBBottom, TRUE,
			 WA_Left, l,
			 WA_Top, t,
			 WA_Width, w,
			 WA_Height, h,
			 WA_PubScreen, (ULONG) scr,
			 WA_IDCMP, IDCMP_RAWKEY | IDCMP_INTUITICKS | IDCMP_EXTENDEDMOUSE,
			 WINDOW_IDCMPHook, (ULONG) &g_WheelHook,
			 g_AppPort ? WINDOW_AppPort : TAG_IGNORE, (ULONG) g_AppPort,
			 WINDOW_AppWindow, (ULONG) (g_AppPort != NULL),
			 WINDOW_AppMsgHook, (ULONG) &g_AppHook,
			 WINDOW_IDCMPHookBits, IDCMP_EXTENDEDMOUSE,
			 WINDOW_NewMenu, (ULONG) g_Menu,
			 WINDOW_GadgetHelp, TRUE,	/* the hover asks the gadgets for their pointers */
			 WINDOW_HintInfo, (ULONG) g_Hints,
			 WINDOW_Layout, (ULONG) root,
			 TAG_END);
	/* the window not made: the layout and its gadgets still ours */
	if (!win) DisposeObject(root);
	return win;
}

/* a key the window does not take itself (the main one's or the Trim
   window's); node: the Help key's page, NULL the one under the pointer */
static void raw_key(struct Arrow *ar, CONST_STRPTR node)
{
	/* a key held down: only the arrows go on stepping; Space, Return,
	   Enter and Del act once a press */
	if ((ar->ar_Qual & IEQUALIFIER_REPEAT) && (ar->ar_Key < RAW_UP || ar->ar_Key > RAW_LEFT)) return;
	/* Space (down, not up): Play and Pause, as the button; Shift Space:
	   the selection */
	if (ar->ar_Key == RAW_SPACE && doc_usable() && Play_There()) {
		if (ar->ar_Qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) play_selection();
		else play_toggle();
	}
	/* Esc: a file being read or decoded called off, as Cancel */
	if (ar->ar_Key == RAW_ESC) cancel_job();
	/* Return or Enter: a marker; Del: the one clicked away */
	if (ar->ar_Key == RAW_RETURN || ar->ar_Key == RAW_ENTER) add_marker();
	if (ar->ar_Key == RAW_DEL) del_marker();
	if (ar->ar_Key >= RAW_UP && ar->ar_Key <= RAW_LEFT) arrow_key(ar);
	/* Help: the page about what the pointer is over */
	if (ar->ar_Key == RAW_HELP) show_help(node ? node : help_node());
	/* F1, F2: the marker before and after the play marker */
	if (ar->ar_Key == RAW_F1 || ar->ar_Key == RAW_F2) jump_marker((BOOL) (ar->ar_Key == RAW_F2));
}

/* the Trim window's gadgets on the trace, where they are in it (the tests
   click them there: InputPoke WIN=Trim) */
static const char *const g_TrimBoxNames[2 * TRIMBOX_PARTS] = {
	"trimin", "triminback", "triminon", "triminlisten", "triminfade", "triminshow",
	"trimout", "trimoutback", "trimouton", "trimoutlisten", "trimoutfade", "trimoutshow"
};

static void trace_trimwin(void)
{
	struct Window *w = TrimWin_Window();
	struct TraceNums tn;
	struct IBox b;
	UWORD i;

	if (!w || !g_TracePath[0]) return;
	tn.tn_What = (CONST_STRPTR) "trimwin l t w h";
	tn.tn_N[0] = (ULONG) w->LeftEdge;
	tn.tn_N[1] = (ULONG) w->TopEdge;
	tn.tn_N[2] = (ULONG) w->Width;
	tn.tn_N[3] = (ULONG) w->Height;
	tn.tn_Count = 4;
	trace_nums(&tn);
	for (i = 0; i < 2 * TRIMBOX_PARTS; i++) {
		if (!TrimWin_Box(i, &b)) continue;
		tn.tn_What = (CONST_STRPTR) g_TrimBoxNames[i];
		tn.tn_N[0] = (ULONG) b.Left;
		tn.tn_N[1] = (ULONG) b.Top;
		tn.tn_N[2] = (ULONG) b.Width;
		tn.tn_N[3] = (ULONG) b.Height;
		trace_nums(&tn);
	}
}

/* Edit, Trim...: the window beside the main one, showing the handles now
   (made the first time, kept until the end) */
static void trim_open(void)
{
	if (!g_Window) return;
	/* the first time standing on the waveform's foot (the scroller's top):
	   the buttons under it stay free */
	if (!TrimWin_Make(g_Window->WScreen) ||
	    !TrimWin_Open(g_Window, (WORD) (g_Window->TopEdge + ((struct Gadget *) g_Scroll)->TopEdge - 2))) {
		say1((CONST_STRPTR) TXT_E_NOMEM, (CONST_STRPTR) TXT_TRIM_TITLE, 0);
		return;
	}
	trim_shown();
	trace_trimwin();
}

/* what came from the Trim window: its gadgets as the main window's were */
static void trim_input(void)
{
	struct TrimEvent te;
	struct Arrow ar;

	while (TrimWin_Input(&te)) {
		/* the tests' trace: what came */
		if (g_TracePath[0]) {
			struct TraceNums tn;

			tn.tn_What = (CONST_STRPTR) "trimev what out key qual";
			tn.tn_N[0] = te.te_What;
			tn.tn_N[1] = (ULONG) te.te_Out;
			tn.tn_N[2] = te.te_Key;
			tn.tn_N[3] = te.te_Qual;
			tn.tn_Count = 4;
			trace_nums(&tn);
		}
		if (te.te_What == TRIM_CLOSE) {
			TrimWin_Close();
			trace((CONST_STRPTR) "trimwin closed");
			continue;
		}
		if (te.te_What == TRIM_KEY) {
			ar.ar_Key = te.te_Key;
			ar.ar_Qual = te.te_Qual;
			raw_key(&ar, HELP_NODE("Trimming"));
			continue;
		}
		if (!g_HaveDoc || g_TrimOff) continue;
		switch (te.te_What) {
		case TRIM_TYPED: typed(te.te_Out); break;
		case TRIM_BACK: nudge(te.te_Out, -nudge_step()); break;
		case TRIM_ON: nudge(te.te_Out, nudge_step()); break;
		case TRIM_LISTEN: listen(te.te_Out); break;
		case TRIM_SHOW: at_handle(te.te_Out); break;
		}
		if (te.te_What != TRIM_LISTEN) {
			g_Target = (UWORD) (te.te_Out ? TARGET_OUT : TARGET_IN);
			pick_shown(te.te_Out);
		}
	}
}

/* nothing to play through, neither the card nor AHI: said before any
   window (on the default public screen), and the program ends -- a
   trimmer that cannot be listened to is no use */
static void no_player(void)
{
	struct EasyStruct es;

	es.es_StructSize = sizeof(es);
	es.es_Flags = 0;
	es.es_Title = (STRPTR) TXT_TITLE;
	es.es_TextFormat = (STRPTR) "%s";
	es.es_GadgetFormat = (STRPTR) TXT_M_QUIT;
	EasyRequest(NULL, &es, NULL, (ULONG) TXT_NOPLAYER);
}

/* --- the loop ---------------------------------------------------------------------------- */

LONG Gui_Run(CONST_STRPTR path, CONST_STRPTR pubscreen)
{
	struct Screen *scr;
	ULONG winsig = 0, sigs, result, mask;
	UWORD code;
	BOOL done = FALSE;
	LONG rc = RETURN_OK;

	Text_Copy((STRPTR) g_Title, (CONST_STRPTR) TXT_TITLE, STATUS_LEN);
	/* the formats it opens: every one PrismaEncode reads, with the decoder */
	Text_Copy((STRPTR) g_Status[0], (CONST_STRPTR) (Decode_There() ? TXT_ST_NOFILE : TXT_ST_NOFILE_WAV), STATUS_LEN);
	g_InText[0] = g_OutText[0] = g_LenText[0] = g_LenHint[0] = '\0';
	if (!Play_Any()) {
		no_player();
		return RETURN_FAIL;
	}
	if ((g_Sig = AllocSignal(-1)) < 0) return RETURN_FAIL;
	g_SigMask = 1UL << g_Sig;
	if (!(g_WaveClass = Waveform_MakeClass())) {
		rc = RETURN_FAIL;
		goto out;
	}
	g_GlyphClass = KeyGlyph_MakeClass();	/* none: the bar's keys wear words */
	/* the screen held until the window is on it */
	scr = LockPubScreen((STRPTR) pubscreen);
	g_WheelHook.h_Entry = (ULONG (*)()) wheel_hook;
	g_WheelHook.h_SubEntry = NULL;
	g_WheelHook.h_Data = NULL;
	g_AppHook.h_Entry = (ULONG (*)()) app_hook;
	g_AppHook.h_SubEntry = NULL;
	g_AppHook.h_Data = NULL;
	g_AppPort = CreateMsgPort();	/* none: no dropping, the rest the same */
	hints_fill();
	EditPrefs_Defaults(&g_Prefs);
	g_PrefsFrom = Settings_Load(&g_Prefs);
	play_way();
	Doc_SetMargins(g_Prefs.ep_LeadMs, g_Prefs.ep_TailMs);
	Doc_SetBackup(g_Prefs.ep_Backup);
	menu_fill();
	menu_ticks();
	g_WinObj = make_window(scr);
	if (g_WinObj) g_Window = (struct Window *) DoMethod(g_WinObj, WM_OPEN, NULL);
	if (scr) UnlockPubScreen(NULL, scr);
	if (!g_Window) {
		rc = RETURN_FAIL;
		goto out;
	}
	GetAttr(WINDOW_SigMask, g_WinObj, &winsig);
	GetAttr(WINDOW_MenuStrip, g_WinObj, (ULONG *) &g_Strip);
	g_TracePath[0] = '\0';
	if (GetVar((STRPTR) "WaveSmithTrace", (STRPTR) g_TracePath, DOC_PATHLEN, 0) <= 0) g_TracePath[0] = '\0';
	/* with the trace: every paint of the strips logged into it */
	if (g_TracePath[0]) {
		Waveform_DiagOn(g_Wave);
		Waveform_DiagOn(g_Over);
	}
	{
		char ends[12];
		LONG n;

		/* the seconds of a long file's ends read first (doc.h): the tests' */
		if (GetVar((STRPTR) "WaveSmithEnds", (STRPTR) ends, sizeof(ends), 0) > 0 && StrToLong((STRPTR) ends, &n) > 0 && n >= 0)
			Doc_SetEnds((ULONG) n);
		/* a save slowed, ticks a per cent: the tests' Cancel lands in it */
		if (GetVar((STRPTR) "WaveSmithSlowSave", (STRPTR) ends, sizeof(ends), 0) > 0 && StrToLong((STRPTR) ends, &n) > 0 && n > 0)
			g_SlowSave = n;
	}
	weigh_strips();
	trace_layout();
	trace_prefs();
	ghost(g_Deck, TRUE);
	ghost(g_LoopKey, TRUE);
	ghost(g_Cancel, TRUE);
	ghost_file(TRUE);
	g_HaveClock = Clock_Open(FALSE);	/* none: the head moves with IntuiTicks, no time left said */
	/* on a native screen the play head a sprite of its own task's (the
	   mover reads the clock); WaveSmithNoSprite set: drawn, as on RTG */
	{
		char v[4];

		if (g_HaveClock && GetVar((STRPTR) "WaveSmithNoSprite", (STRPTR) v, sizeof(v), 0) < 0)
			g_HeadSprite = HeadSprite_Open(g_Window);
		trace((CONST_STRPTR) (g_HeadSprite ? "headsprite on" : "headsprite off"));
		/* the next page painted in pieces of the time there is, by the
		   beat (WaveSmithNoPace set, or no clock: a piece a head's move,
		   as before) */
		g_NoPace = (BOOL) (!g_HaveClock || GetVar((STRPTR) "WaveSmithNoPace", (STRPTR) v, sizeof(v), 0) >= 0);
		if (!g_NoPace) set1(g_Wave, WAVEFORM_AheadColumns, 0);
		trace((CONST_STRPTR) (g_NoPace ? "pace off" : "pace on"));
		/* the tests: AHI's copies of what was read compared with the file */
		Ahi_CheckCopies((BOOL) (GetVar((STRPTR) "WaveSmithCheckCopy", (STRPTR) v, sizeof(v), 0) >= 0));
	}
	if (path) open_file(path);
	mask = winsig | g_SigMask | Play_Signals() | Decode_Signals() | SIGBREAKF_CTRL_C |
	       (g_AppPort ? 1UL << g_AppPort->mp_SigBit : 0) | (g_HaveClock ? Clock_Signal() : 0);

	while (!done) {
		/* the manual's, the player's and the decoder's signals: allocated
		   when they are first wanted (AHI at its first play, prismadsp
		   when it was installed since the start), asked every pass */
		ULONG all = mask | Help_Signal(&g_Help) | Play_Signals() | Decode_Signals() | TrimWin_Signal() | About_Signal();

		paint_budget();
		if (g_Reading) {
			read_some();
			sigs = SetSignal(0, all) & all;
		} else if (!Play_Playing() && g_Window && paint_left() && !input_waiting() && idle_paint()) {
			/* a page a play left half painted, a new view filling in:
			   a piece a pass, the input answered between them (its
			   layer busy: waited for as anything else) */
			sigs = SetSignal(0, all) & all;
		} else if (!Play_Playing() && g_Window && g_Wave && g_WantOwed && !input_waiting() && owed_read()) {
			/* its samples read a piece a pass (the page waits for
			   them) */
			sigs = SetSignal(0, all) & all;
		} else {
			looked();	/* listening from here */
			sigs = Wait(all);
			if (g_HaveClock) g_Looked = Clock_Ms();
		}
		if (sigs & Help_Signal(&g_Help)) Help_HandleMessages(&g_Help);
		if (sigs & About_Signal()) {
			About_HandleInput();
			if (!About_Signal()) trace((CONST_STRPTR) "about closed");
		}
		if ((sigs & SIGBREAKF_CTRL_C) && may_leave()) done = TRUE;	/* changes asked about first */
		if (sigs & g_SigMask) {
			ULONG from = g_HaveClock ? Clock_Ms() : 0;

			wave_changes();
			slow_step((CONST_STRPTR) "slow changes ms", from);
			/* the scroller dragged: the view follows */
			if (doc_usable()) {
				ULONG z = get(g_Wave, WAVEFORM_Zoom), t = get(g_Scroll, SCROLLER_Top) * (z ? z : 1);

				if (t != get(g_Wave, WAVEFORM_Top) && (t > get(g_Wave, WAVEFORM_Top) ? t - get(g_Wave, WAVEFORM_Top) : get(g_Wave, WAVEFORM_Top) - t) >= (z ? z : 1))
					view_top(t);
			}
		}
		while ((result = DoMethod(g_WinObj, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
			case WMHI_CLOSEWINDOW:
				if (may_leave()) done = TRUE;
				break;
			case WMHI_GADGETUP:
				gadget_up(result & WMHI_GADGETMASK, code);
				break;
			case WMHI_MENUPICK:
				if (!menu_pick((UWORD) (result & WMHI_MENUMASK))) done = TRUE;
				break;
			case WMHI_RAWKEY:
				{
					struct InputEvent *ie = NULL;
					struct Arrow ar;

					ar.ar_Key = (UWORD) (result & WMHI_KEYMASK);
					GetAttr(WINDOW_InputEvent, g_WinObj, (ULONG *) &ie);
					ar.ar_Qual = ie ? ie->ie_Qualifier : 0;
					raw_key(&ar, NULL);
				}
				break;
			case WMHI_ACTIVE: trace_win((CONST_STRPTR) "winactive l t w h"); break;
			case WMHI_INACTIVE: trace_win((CONST_STRPTR) "wininactive l t w h"); break;
			case WMHI_CHANGEWINDOW: trace_win((CONST_STRPTR) "winchange l t w h"); break;
			case WMHI_NEWSIZE: trace_win((CONST_STRPTR) "winsize l t w h"); break;
			case WMHI_INTUITICK:
				/* the head: the clock's beat moves it (here as well, a
				   second move a tick: 25 ms each on an A1200) */
				if (Play_Playing() && !g_HaveClock) {
					struct FeedStep fs;

					fs.fs_Piece = 0;
					fs.fs_Have[0] = fs.fs_Have[1] = NULL;
					show_head();
					Play_Feed(&fs);
				}
				/* the decode's progress ten times a second: the chip's
				   decode sends no word of it, its bytes in are asked for */
				if (g_Decoding) decode_signals();
				trace_help();
				strips_tick();
				flag_pens();
				break;
			}
		}
		trim_input();
		wheel_spend();
		/* stopped, a pass's work since the input was last looked at (a
		   piece, the changes, the controls): the trace's longest steps */
		if (!Play_Playing() && !g_Reading) slow_step((CONST_STRPTR) "slow pass ms", g_Looked);
		looked();
		/* the play's work after the controls: a Pause pressed while the
		   beat was due stops it before anything is read or painted */
		if (sigs & Play_Signals()) {
			play_signals();
			slow_step((CONST_STRPTR) "slow tick ms", g_Looked);
		}
		if (g_HaveClock && (sigs & Clock_Signal())) head_beat();
		if (g_Decoding && (sigs & Decode_Signals())) decode_signals();
		trace_diag();
		/* a file dropped on the window: opened, asked first if changed */
		if (g_Dropped && !done) {
			char dropped[DOC_PATHLEN];

			/* a copy: a drop during the question or a save it brings
			   writes g_DropPath again */
			g_Dropped = FALSE;
			Text_Copy((STRPTR) dropped, (CONST_STRPTR) g_DropPath, DOC_PATHLEN);
			open_file((CONST_STRPTR) dropped);
		}
	}

out:
	trace((CONST_STRPTR) "leaving");
	About_Close();			/* its pens given back while the screen is held */
	Help_Cleanup(&g_Help);		/* the manual's window, while the screen is held */
	forget_doc();
	/* the mover gone before the clock it reads */
	if (g_HeadSprite) {
		trace((CONST_STRPTR) "headsprite closing");
		HeadSprite_Close();
		trace((CONST_STRPTR) "headsprite closed");
	}
	g_HeadSprite = FALSE;
	g_HeadOff = 0;
	if (g_HaveClock) Clock_Close();	/* a wake still running aborted */
	g_HaveClock = FALSE;
	/* the Trim window first: it borrows the main window's menus */
	TrimWin_Dispose();
	if (g_WinObj) {
		DisposeObject(g_WinObj);	/* the window, the layout, the gadgets */
		g_WinObj = NULL;
	}
	/* the keys' marks after their buttons: ours, not the buttons' */
	{
		UWORD i;

		for (i = 0; i < GLYPHS; i++) {
			if (g_Glyph[i]) DisposeObject(g_Glyph[i]);
			g_Glyph[i] = NULL;
		}
		KeyGlyph_FreeClass(g_GlyphClass);
		g_GlyphClass = NULL;
	}
	if (g_AppPort) {
		struct Message *m;

		/* no AppWindow any more: what came meanwhile answered */
		while ((m = GetMsg(g_AppPort))) ReplyMsg(m);
		DeleteMsgPort(g_AppPort);
		g_AppPort = NULL;
	}
	g_Window = NULL;
	g_Strip = NULL;
	if (g_WaveClass) Waveform_FreeClass(g_WaveClass);
	g_WaveClass = NULL;
	if (g_Sig >= 0) FreeSignal(g_Sig);
	g_Sig = -1;
	if (g_TraceFH) Close(g_TraceFH);
	g_TraceFH = 0;
	return rc;
}
