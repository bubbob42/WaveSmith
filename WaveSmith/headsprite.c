/*
 * headsprite.c -- the play heads as hardware sprites (headsprite.h)
 *
 * Measured with C:SpriteProbe (concept Transport, 8): the sprite is asked
 * for in the width the sprites have now (another width makes the system
 * build the mouse pointer anew); MoveSprite()'s x is in the screen's
 * pixels, a hires pixel a step on AGA, y halved on a laced screen, and a
 * sprite moved below its screen shows nothing. A move costs ~50 us; at
 * priority 5, paced by WaitTOF(), the mover kept 50 moves a second beside
 * a WaveSmith playing a deep view (29 at priority 0).
 *
 * The colour (concept Transport, 11): a sprite pair shares its colours,
 * so the two lines are a pair's sprites in its colour 1. A 6-plane A1200
 * Workbench gives that colour as a pen of its own; an 8-colour A2000 one
 * gives none -- colours past the screen's own lie outside palette sharing
 * (graphics.doc, AttachPalExtra: the set is the mode's and depth's), no
 * pixel of the screen shows them, and only the pair's sprites, ours, do:
 * there it is set here and given back as it was at the end.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/sprite.h>
#include <graphics/view.h>
#include <graphics/videocontrol.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <clib/compiler-specific.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "clock.h"
#include "headsprite.h"

#ifdef __GNUC__
#define SAVEDS
#else
#define SAVEDS __SAVE_DS__	/* the mover reads our data through A4 */
#endif

#define MOVER_PRI	5		/* above the window's task (measured) */
#define MOVER_STACK	4096
#define HEAD_R		0xFFFFFFFFUL	/* the gadget's play head, $FF8A00 */
#define HEAD_G		0x8A8A8A8AUL
#define HEAD_B		0x00000000UL
#define BACK_COLS	4		/* a step back no larger: the clock ran ahead */
#define MAX_MS		10000UL		/* the main task's word older: not carried further */

/* a colour the sprites show: a pen taken, or one set and given back */
struct HeadColour {
	LONG hc_Index;			/* -1: none */
	BOOL hc_Pen;			/* a pen (else hc_RGB is what it was) */
	ULONG hc_RGB[3];
};

/* a line made anew for a new height, and the one to free after */
struct Remake {
	struct ExtSprite *rm_Line;
	struct BitMap *rm_BM;
};

static struct SignalSemaphore lock;	/* place and lines: the main task's, read by the mover */
static struct HeadPlace place;
static const struct HeadPlace no_place;
static struct ExtSprite *line[HEAD_LINES];
static struct BitMap *line_bm[HEAD_LINES];
static UWORD line_rows[HEAD_LINES];
static LONG num[HEAD_LINES] = { -1, -1 };	/* the sprites; -1 none (that head drawn) */
static struct HeadColour colour[HEAD_LINES];	/* none taken: hc_Index -1 (HeadSprite_Open) */
static struct Window *win = NULL;
static struct Screen *scr = NULL;
static struct Task *owner = NULL;
static BYTE done_sig = -1;
static volatile BOOL stop = FALSE;
static BOOL running = FALSE, lace = FALSE;
static BOOL too_many[HEAD_LINES];
static UWORD covers[HEAD_LINES];	/* the layers over each line, last found */
static WORD cover_x0[HEAD_LINES][HEAD_COVERS], cover_x1[HEAD_LINES][HEAD_COVERS];
/* the mover's own: each line as it last moved it */
static struct ExtSprite *drawn[HEAD_LINES];
static ULONG drawn_first[HEAD_LINES];
static LONG drawn_x[HEAD_LINES];

/* the line's image: one sprite pixel wide in colour 1, as tall as the
   lanes (a laced screen's sprite line spans two rows) */
static struct ExtSprite *make_line(UWORD rows, struct BitMap **bm)
{
	struct RastPort rp;
	struct TagItem t[3];
	struct ExtSprite *es;
	UWORD lines = (UWORD) (lace ? (rows + 1) / 2 : rows);

	if (!lines) lines = 1;
	if (!(*bm = AllocBitMap(16, lines, 2, BMF_CLEAR, NULL))) return NULL;
	InitRastPort(&rp);
	rp.BitMap = *bm;
	SetAPen(&rp, 1);
	RectFill(&rp, 0, 0, 0, (LONG) lines - 1);
	t[0].ti_Tag = SPRITEA_Width;
	t[0].ti_Data = (ULONG) GfxBase->SpriteWidth * 16UL;
	t[1].ti_Tag = SPRITEA_OutputHeight;
	t[1].ti_Data = lines;
	t[2].ti_Tag = TAG_DONE;
	t[2].ti_Data = 0;
	if (!(es = AllocSpriteDataA(*bm, t))) {
		FreeBitMap(*bm);
		*bm = NULL;
	}
	return es;
}

/* below the screen: the sprite shows nothing */
static void hide(struct ExtSprite *es)
{
	MoveSprite(&scr->ViewPort, &es->es_SimpleSprite, 0, (WORD) (scr->Height + 2));
}

/* the first colour of the even or the odd sprites' bank */
static ULONG bank(ULONG tag)
{
	struct TagItem t[2];

	t[0].ti_Tag = tag;
	t[0].ti_Data = 16;
	t[1].ti_Tag = TAG_DONE;
	t[1].ti_Data = 0;
	return VideoControl(scr->ViewPort.ColorMap, t) ? 16UL : t[0].ti_Data;
}

/* colour idx the head's orange for line n's sprite: a pen of its own, or
   -- past the screen's own colours, on a screen whose pixels show no
   more (no HAM, half-brite or dual playfield) -- set, what it was kept */
static BOOL take_colour(UWORD n, ULONG idx)
{
	struct ColorMap *cm = scr->ViewPort.ColorMap;
	struct HeadColour *hc = &colour[n];
	ULONG depth;
	LONG got;

	hc->hc_Index = -1;
	if (idx >= (ULONG) cm->Count) return FALSE;
	got = ObtainPen(cm, idx, HEAD_R, HEAD_G, HEAD_B, PEN_EXCLUSIVE);
	if (got == (LONG) idx) {
		hc->hc_Index = (LONG) idx;
		hc->hc_Pen = TRUE;
		return TRUE;
	}
	depth = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH);
	if (depth >= 8 || idx < (1UL << depth) || (scr->ViewPort.Modes & (HAM | EXTRA_HALFBRITE | DUALPF))) return FALSE;
	GetRGB32(cm, idx, 1, hc->hc_RGB);
	SetRGB32(&scr->ViewPort, idx, HEAD_R, HEAD_G, HEAD_B);
	hc->hc_Index = (LONG) idx;
	hc->hc_Pen = FALSE;
	return TRUE;
}

/* line n's colour given back: the pen, or the colour as it was */
static void give_colour(UWORD n)
{
	struct HeadColour *hc = &colour[n];

	if (hc->hc_Index < 0) return;
	if (hc->hc_Pen)
		ReleasePen(scr->ViewPort.ColorMap, (ULONG) hc->hc_Index);
	else
		SetRGB32(&scr->ViewPort, (ULONG) hc->hc_Index, hc->hc_RGB[0], hc->hc_RGB[1], hc->hc_RGB[2]);
	hc->hc_Index = -1;
}

/* sprite want for line n: FALSE when it is not free */
static BOOL take_sprite(UWORD n, LONG want)
{
	struct TagItem t[2];

	t[0].ti_Tag = GSTAG_SPRITE_NUM;
	t[0].ti_Data = (ULONG) want;
	t[1].ti_Tag = TAG_DONE;
	t[1].ti_Data = 0;
	if (GetExtSpriteA(line[n], t) != want) return FALSE;
	num[n] = want;
	return TRUE;
}

static void drop_sprite(UWORD n)
{
	FreeSprite((WORD) num[n]);
	num[n] = -1;
}

/* a pair, 2/3, 4/5 or 6/7, in the head's orange: the lanes' line the
   even sprite, the overview's the odd one (its colour the even one's
   where the pair shares a bank), or no overview line when the odd one is
   not to be had. No pair: sprite 1 for the lanes in the pointer's
   colour, nothing taken */
static BOOL take_sprites(void)
{
	ULONG even = bank(VTAG_SPEVEN_BASE_GET), odd = bank(VTAG_SPODD_BASE_GET), idx;
	LONG n;

	for (n = 2; n <= 6; n += 2) {
		if (!take_sprite(HEAD_MAIN, n)) continue;
		if (!take_colour(HEAD_MAIN, even + (ULONG) (n >> 1) * 4UL + 1UL)) {
			drop_sprite(HEAD_MAIN);
			continue;
		}
		idx = odd + (ULONG) (n >> 1) * 4UL + 1UL;
		if (take_sprite(HEAD_OVER, n + 1) && idx != (ULONG) colour[HEAD_MAIN].hc_Index && !take_colour(HEAD_OVER, idx))
			drop_sprite(HEAD_OVER);
		return TRUE;
	}
	return take_sprite(HEAD_MAIN, 1);
}

/* where line n's head is now: the main task's frame carried on by the
   clock; -1 out of view (before it; past it, the last column until the
   page turns on the main task's next beat) */
static LONG head_x(const struct HeadPlace *hp, UWORD n)
{
	const struct HeadLine *hl = &hp->hp_Line[n];
	ULONG ms = (Clock_Ticks() - hp->hp_Ticks) / (Clock_TickHz() / 1000UL), f, col;

	if (ms > MAX_MS) ms = MAX_MS;
	f = hp->hp_Frame + ms * hp->hp_Rate / 1000UL;
	if (!hl->hl_Zoom || !hl->hl_Columns || f < hl->hl_First) return -1;
	col = (f - hl->hl_First) / hl->hl_Zoom;
	if (col >= hl->hl_Columns) col = hl->hl_Columns - 1UL;
	return (LONG) hl->hl_Left + (LONG) col;
}

/* line n where its head is (show: the window active, no menu, its screen
   in front), hidden under a layer in front of the window */
static void move_line(const struct HeadPlace *hp, UWORD n, BOOL show)
{
	const struct HeadLine *hl = &hp->hp_Line[n];
	struct ExtSprite *es = line[n];
	LONG x;
	UWORD i;

	if (num[n] < 0) return;
	x = show && hl->hl_On ? head_x(hp, n) : -1;
	for (i = 0; x >= 0 && i < hl->hl_Covers; i++)
		if (x + 1 >= hl->hl_CoverX0[i] && x <= hl->hl_CoverX1[i]) x = -1;
	if (x >= 0) {
		/* never a little way back on the same page: the main task's
		   next word only caught the clock up */
		if (es == drawn[n] && hl->hl_First == drawn_first[n] && x < drawn_x[n] && x > drawn_x[n] - BACK_COLS)
			x = drawn_x[n];
		if (es != drawn[n] || x != drawn_x[n]) {
			MoveSprite(&scr->ViewPort, &es->es_SimpleSprite, (WORD) x, hl->hl_Top);
			drawn[n] = es;
			drawn_x[n] = x;
			drawn_first[n] = hl->hl_First;
		}
	} else if (drawn[n]) {
		hide(es);
		drawn[n] = NULL;
		drawn_x[n] = -1;
	}
}

static SAVEDS void mover(void)
{
	UWORD n;
	BOOL show;

	while (!stop) {
		WaitTOF();
		ObtainSemaphore(&lock);
		show = (BOOL) (IntuitionBase->ActiveWindow == win && !(win->Flags & WFLG_MENUSTATE) &&
			       IntuitionBase->FirstScreen == scr);
		for (n = 0; n < HEAD_LINES; n++) move_line(&place, n, show);
		ReleaseSemaphore(&lock);
	}
	Forbid();			/* told and gone before the owner runs */
	Signal(owner, 1UL << done_sig);
}

BOOL HeadSprite_There(void)
{
	return running;
}

BOOL HeadSprite_Open(struct Window *w)
{
	struct Screen *s = w->WScreen;
	UWORD n;

	if (running) return TRUE;
	/* a planar screen of the chipset's: an RTG one has no sprites to give */
	if (!(GetBitMapAttr(s->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD)) return FALSE;
	win = w;
	scr = s;
	lace = (BOOL) ((GetVPModeID(&s->ViewPort) & LACE) != 0);
	InitSemaphore(&lock);
	place = no_place;
	for (n = 0; n < HEAD_LINES; n++) {
		num[n] = -1;
		colour[n].hc_Index = -1;
		line[n] = NULL;
		line_bm[n] = NULL;
	}
	for (n = 0; n < HEAD_LINES; n++) {
		covers[n] = 0;
		too_many[n] = FALSE;
		drawn[n] = NULL;
		drawn_x[n] = -1;
		drawn_first[n] = 0;
		line_rows[n] = 1;
		if (!(line[n] = make_line(1, &line_bm[n]))) {
			HeadSprite_Close();
			return FALSE;
		}
	}
	if (!take_sprites() || (done_sig = AllocSignal(-1)) == -1) {
		HeadSprite_Close();
		return FALSE;
	}
	for (n = 0; n < HEAD_LINES; n++)
		if (num[n] >= 0) hide(line[n]);
	owner = FindTask(NULL);
	stop = FALSE;
	if (!CreateNewProcTags(NP_Entry, (ULONG) mover, NP_Name, (ULONG) "WaveSmith head", NP_Priority, MOVER_PRI,
			       NP_StackSize, MOVER_STACK, TAG_DONE)) {
		HeadSprite_Close();
		return FALSE;
	}
	running = TRUE;
	return TRUE;
}

void HeadSprite_Close(void)
{
	UWORD n;
	BOOL freed = FALSE;

	if (running) {
		stop = TRUE;
		Wait(1UL << done_sig);	/* it ends after its next frame */
		running = FALSE;
	}
	for (n = 0; n < HEAD_LINES; n++) {
		if (num[n] < 0) continue;
		hide(line[n]);
		drop_sprite(n);		/* given back blank */
		freed = TRUE;
	}
	if (freed) WaitTOF();		/* the display past the lines' data */
	for (n = 0; n < HEAD_LINES; n++) {
		if (scr) give_colour(n);
		if (line[n]) FreeSpriteData(line[n]);
		line[n] = NULL;
		if (line_bm[n]) FreeBitMap(line_bm[n]);
		line_bm[n] = NULL;
	}
	if (done_sig != -1) FreeSignal(done_sig);
	done_sig = -1;
	win = NULL;
	scr = NULL;
}

/* the layers in front of the window over line hl (n), as column ranges:
   the screen's Layer_Info lock (graphics/layers.h) tried -- busy (a menu
   holds it), the last ones stay */
static void find_covers(const struct HeadLine *hl, UWORD n)
{
	struct Layer_Info *li = &scr->LayerInfo;
	struct Layer *l;
	WORD x1 = (WORD) (hl->hl_Left + hl->hl_Columns - 1), y1 = (WORD) (hl->hl_Top + hl->hl_Rows - 1);
	UWORD k = 0;
	BOOL over = FALSE;

	if (!AttemptSemaphore(&li->Lock)) return;
	for (l = li->top_layer; l && l != win->WLayer; l = l->back) {
		if ((l->Flags & LAYERHIDDEN) || l->bounds.MaxX < hl->hl_Left || l->bounds.MinX > x1 ||
		    l->bounds.MaxY < hl->hl_Top || l->bounds.MinY > y1)
			continue;
		if (k == HEAD_COVERS) {
			over = TRUE;
			break;
		}
		cover_x0[n][k] = l->bounds.MinX;
		cover_x1[n][k] = l->bounds.MaxX;
		k++;
	}
	ReleaseSemaphore(&li->Lock);
	covers[n] = k;
	too_many[n] = over;
}

/* line n's new height handed over (the lock held): the sprite's number
   moved onto the new image -- graphics keeps the image moved last in its
   table of the sprites in use and reads it on every move of the pointer
   (is its partner attached?), so the table never stays on the old one;
   rm then holds the one to free (the new one when refused: the old line
   stays, this height not tried again). The mover shows the new one */
static void hand_over(struct Remake *rm, UWORD n)
{
	struct ExtSprite *es = rm->rm_Line;
	struct BitMap *bm = rm->rm_BM;

	hide(line[n]);
	if (ChangeExtSpriteA(&scr->ViewPort, line[n], es, NULL)) {
		rm->rm_Line = line[n];
		rm->rm_BM = line_bm[n];
		line[n] = es;
		line_bm[n] = bm;
	}
	line_rows[n] = place.hp_Line[n].hl_Rows;
}

UWORD HeadSprite_Place(const struct HeadPlace *hp)
{
	struct Remake rm[HEAD_LINES];
	struct HeadLine *pl;
	UWORD n, k, shown = 0;
	BOOL waited = FALSE;

	if (!running) return 0;
	for (n = 0; n < HEAD_LINES; n++) {
		rm[n].rm_Line = NULL;
		rm[n].rm_BM = NULL;
		if (num[n] < 0) continue;
		if (hp->hp_Line[n].hl_On) find_covers(&hp->hp_Line[n], n);
		if (hp->hp_Line[n].hl_Rows && hp->hp_Line[n].hl_Rows != line_rows[n])
			rm[n].rm_Line = make_line(hp->hp_Line[n].hl_Rows, &rm[n].rm_BM);
	}
	ObtainSemaphore(&lock);
	place = *hp;
	for (n = 0; n < HEAD_LINES; n++) {
		pl = &place.hp_Line[n];
		pl->hl_On = (BOOL) (pl->hl_On && num[n] >= 0 && !too_many[n]);
		if (pl->hl_On) shown |= (UWORD) (1 << n);
		pl->hl_Covers = covers[n];
		for (k = 0; k < covers[n]; k++) {
			pl->hl_CoverX0[k] = cover_x0[n][k];
			pl->hl_CoverX1[k] = cover_x1[n][k];
		}
		if (rm[n].rm_Line) hand_over(&rm[n], n);
	}
	ReleaseSemaphore(&lock);
	/* what was handed over freed once the display is past it */
	for (n = 0; n < HEAD_LINES; n++) {
		if (!rm[n].rm_Line) continue;
		if (!waited) {
			WaitTOF();
			WaitTOF();
			waited = TRUE;
		}
		FreeSpriteData(rm[n].rm_Line);
		FreeBitMap(rm[n].rm_BM);
	}
	return shown;
}
