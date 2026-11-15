/*
 * play.c -- listening on the card (play.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <libraries/prisma.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/prisma.h>

#include "play.h"
#include "ahiplay.h"
#include "textutil.h"
#include "clock.h"

#define PLAY_BUFS	4
#define PLAY_BUFSIZE	32768UL
#define OWNER_LEN	64
#define HEAD_MAX	4096UL

struct Library *PrismaBase = NULL;

static struct MsgPort *port = NULL;
static BYTE sig = -1;
static struct PrismaCard *card = NULL;
static struct PrismaPlayer *player = NULL;
static BPTR fh = 0;
static const struct Timeline *tl = NULL;	/* the doc's, while it plays */
static const struct TlLevels *levels = NULL;	/* and its end fades and dips */
static ULONG data_start = 0;		/* the samples' first byte in the file */
static ULONG next = 0;			/* the next timeline frame to queue */
static ULONG left = 0;			/* the span's frames not queued yet */
static ULONG first_frame = 0;
static ULONG head_bytes = 0;		/* the header, sent first */
static BOOL head_pending = FALSE;
static UWORD align = 4;
static BOOL playing = FALSE, stopping = FALSE;
static BOOL gone = FALSE;		/* the player's last word (TERMINATED) came */
static UBYTE head[HEAD_MAX];
static char owner[OWNER_LEN];

/* Settings, Play through; whether the play going on is AHI's */
static struct PlayWay way = { PLAY_WAY_AUTO, 0, 0 };
static BOOL on_ahi = FALSE;

/* the tests' player (ENV:WaveSmithSimPlay): the card's time without the
   card -- FS-UAE has none -- the frame from the clock, the end signalled */
static BOOL sim = FALSE;
static struct DateStamp sim_start;
static ULONG sim_end = 0, sim_rate = 44100;

/* the card's play position: from the clock since it began, never past
   what was sent -- PPA_WORDSSENT moves in bursts, the clock evenly */
static ULONG play_rate = 44100, start_ms = 0;

/* the frames rate plays in ms milliseconds, without the product that
   wraps after a minute and a half */
static ULONG frames_in(ULONG ms, ULONG rate)
{
	return ms / 1000UL * rate + ms % 1000UL * rate / 1000UL;
}

/* the frame the clock has reached since the start, at most the end */
static ULONG sim_frame(void)
{
	struct DateStamp now;
	ULONG ticks, f;

	if (Clock_TickHz()) {
		f = first_frame + frames_in(Clock_Ms() - start_ms, sim_rate);
		return f < sim_end ? f : sim_end;
	}
	DateStamp(&now);
	ticks = (ULONG) (((now.ds_Days - sim_start.ds_Days) * 1440L + now.ds_Minute - sim_start.ds_Minute) *
			 (60L * TICKS_PER_SECOND) + now.ds_Tick - sim_start.ds_Tick);
	/* ticks x rate / 50 without the product that wraps after a day */
	f = first_frame + ticks / TICKS_PER_SECOND * sim_rate + ticks % TICKS_PER_SECOND * sim_rate / TICKS_PER_SECOND;
	return f < sim_end ? f : sim_end;
}

static UWORD sim_play(const struct Doc *d, const struct PlaySpan *ps)
{
	first_frame = ps->ps_First;
	sim_end = ps->ps_End > Doc_Length(d) ? Doc_Length(d) : ps->ps_End;
	sim_rate = d->d_Rate;
	if (first_frame >= sim_end) return PLAY_FILE;
	DateStamp(&sim_start);
	start_ms = Clock_Ms();
	playing = TRUE;
	stopping = FALSE;
	return PLAY_OK;
}

/* the player's messages, every one replied: TRUE when code was among
   them; its last word (the process ended itself) noted in gone */
static BOOL drain(UWORD code)
{
	struct PlayerMsg *pm;
	BOOL seen = FALSE;

	if (!port) return FALSE;
	while ((pm = (struct PlayerMsg *) GetMsg(port))) {
		if (pm->pm_Code == code) seen = TRUE;
		if (pm->pm_Code == PSTATUS_TERMINATED) gone = TRUE;
		ReplyMsg((struct Message *) pm);
	}
	return seen;
}

/* code awaited -- or the player's end, after which nothing comes */
static void wait_for(UWORD code)
{
	while (!drain(code) && !gone) WaitPort(port);
}

UWORD Play_Open(void)
{
	char v[4];

	Ahi_Open();			/* its signal in the window's mask from the start */
	if (GetVar((STRPTR) "WaveSmithSimPlay", (STRPTR) v, sizeof(v), 0) >= 0) {
		sim = TRUE;
		return (UWORD) ((sig = AllocSignal(-1)) < 0 ? PLAY_NOLIB : PLAY_OK);
	}
	if (!(PrismaBase = OpenLibrary((STRPTR) PRISMALIB_NAME, 3))) return PLAY_NOLIB;
	if (!(port = CreateMsgPort()) || (sig = AllocSignal(-1)) < 0) {
		/* the card's things only: AHI's signal stays in the window's mask */
		if (port) DeleteMsgPort(port);
		port = NULL;
		CloseLibrary(PrismaBase);
		PrismaBase = NULL;
		return PLAY_NOLIB;
	}
	return PLAY_OK;
}

void Play_Close(void)
{
	Play_Stop();
	Ahi_Close();
	if (sig >= 0) FreeSignal(sig);
	sig = -1;
	if (port) DeleteMsgPort(port);
	port = NULL;
	if (PrismaBase) CloseLibrary(PrismaBase);
	PrismaBase = NULL;
}

void Play_SetWay(const struct PlayWay *pw)
{
	way = *pw;
}

/* the card's library opened */
static BOOL card_there(void)
{
	return (BOOL) (PrismaBase != NULL && port != NULL);
}

/* the card found -- obtained and let go at once when nobody has it, as a
   mixer does; whoever has it, it was found then -- or AHI, or the tests'
   player, whatever the way */
BOOL Play_Any(void)
{
	if (sim) return (BOOL) (sig >= 0);
	if (card_there()) {
		if (Prisma_ObtainCard()) Prisma_ReleaseCard();
		if (Prisma_GetCardAttr(NULL, PCA_CHIPVERSION) != 0) return TRUE;
	}
	return Ahi_There();
}

BOOL Play_There(void)
{
	if (sim) return (BOOL) (sig >= 0);
	if (way.pw_Way != PLAY_WAY_AHI && card_there()) return TRUE;
	return (BOOL) (way.pw_Way != PLAY_WAY_CARD && Ahi_There());
}

UWORD Play_Through(void)
{
	return (UWORD) (on_ahi ? PLAY_THROUGH_AHI : PLAY_THROUGH_CARD);
}

ULONG Play_Signals(void)
{
	return (port ? 1UL << port->mp_SigBit : 0) | (sig >= 0 ? 1UL << sig : 0) | Ahi_Signal();
}

BOOL Play_Playing(void)
{
	return (BOOL) (on_ahi ? Ahi_Playing() : playing);
}

CONST_STRPTR Play_Owner(void)
{
	return (CONST_STRPTR) owner;
}

/* the free buffers filled -- the header first, then the span along the
   timeline -- and queued */
static void feed(void)
{
	struct ReplayBuffer *rb;
	struct TlRead rd;
	ULONG n, want, got;
	UBYTE *p;

	while ((head_pending || left) && (rb = Prisma_GetFreeBuffer(player))) {
		p = (UBYTE *) rb->rb_Data;
		n = 0;
		if (head_pending) {
			CopyMem(head, p, head_bytes);
			n = head_bytes;
			head_pending = FALSE;
		}
		want = (PLAY_BUFSIZE - n) / align;
		if (want > left) want = left;
		if (want) {
			Tl_ReadInit(&rd);
			rd.rd_FH = fh;
			rd.rd_Start = data_start;
			rd.rd_Align = align;
			rd.rd_Pad = 0;
			rd.rd_First = next;
			rd.rd_Frames = want;
			rd.rd_Buf = p + n;
			rd.rd_Levels = Tl_Levels(levels);
			rd.rd_AsRecorded = Play_AsRecorded();
			if (!(got = Tl_Read(tl, &rd))) {
				left = 0;		/* a short file: what came is it */
			} else {
				n += got * align;
				next += got;
				left -= got;
			}
		}
		if (!n) {
			Prisma_AddBuffer(player, rb);
			break;
		}
		rb->rb_Size = n;
		rb->rb_Pos = rb->rb_Data;
		rb->rb_End = rb->rb_Data + n / 2;
		Prisma_QueueBuffer(player, rb);
	}
}

/* on the card: PLAY_#? */
static UWORD card_start(struct Doc *d, const struct PlaySpan *ps)
{
	struct NormWav nw;
	struct ReplayBuffer *rb;
	CONST_STRPTR who;
	ULONG end = ps->ps_End > Doc_Length(d) ? Doc_Length(d) : ps->ps_End;
	UWORD i;

	if (ps->ps_First >= end || d->d_Wav.nw_DataStart > HEAD_MAX) return PLAY_FILE;
	if (!(card = Prisma_ObtainCard())) {
		/* PCA_OWNER: a copy the library keeps until the next owner */
		who = (CONST_STRPTR) Prisma_GetCardAttr(NULL, PCA_OWNER);
		if (!who) return PLAY_NOCARD;
		Text_Copy((STRPTR) owner, who, OWNER_LEN);
		return PLAY_BUSY;
	}
	gone = FALSE;
	if (!(player = Prisma_CreatePlayer(port))) {
		Prisma_ReleaseCard();
		card = NULL;
		return PLAY_NOPLAYER;
	}
	Prisma_SetSignal(player, FindTask(NULL), 1UL << sig);
	for (i = 0; i < PLAY_BUFS; i++) {
		if (!(rb = Prisma_CreateBuffer(player, PLAY_BUFSIZE))) break;
		Prisma_AddBuffer(player, rb);
	}
	if (i == 0 || !(fh = Open((STRPTR) d->d_Path, MODE_OLDFILE))) {
		Play_Stop();
		return (UWORD) (i == 0 ? PLAY_NOPLAYER : PLAY_FILE);
	}
	align = (UWORD) d->d_Wav.nw_Align;
	first_frame = next = ps->ps_First;
	left = end - ps->ps_First;
	tl = &d->d_Tl;
	levels = &d->d_Levels;
	data_start = d->d_Wav.nw_DataStart;
	/* the header, its sizes the span's: the chip takes it as a whole WAV */
	CopyMem(d->d_Head, head, d->d_Wav.nw_DataStart);
	nw = d->d_Wav;
	NormWav_FixSizes(head, &nw, left * align);
	head_bytes = d->d_Wav.nw_DataStart;
	head_pending = TRUE;
	feed();
	if (Prisma_Play(player, 0) != PMSG_OK) {
		Play_Stop();
		return PLAY_REFUSED;
	}
	play_rate = d->d_Rate;
	start_ms = Clock_Ms();
	playing = TRUE;
	stopping = FALSE;
	return PLAY_OK;
}

UWORD Play_Start(struct Doc *d, const struct PlaySpan *ps)
{
	struct AhiMode am;
	struct AhiSpan as;
	UWORD r = PLAY_NOLIB;

	Play_Stop();
	owner[0] = '\0';
	if (sim) return (UWORD) (sig >= 0 ? sim_play(d, ps) : PLAY_NOLIB);
	if (way.pw_Way != PLAY_WAY_AHI && card_there()) {
		r = card_start(d, ps);
		/* Automatic: no card, or another program's -- AHI then (the
		   owner kept for the status line) */
		if (r == PLAY_OK || way.pw_Way == PLAY_WAY_CARD || (r != PLAY_NOCARD && r != PLAY_BUSY)) return r;
	}
	if (way.pw_Way == PLAY_WAY_CARD) return r;
	/* through AHI by choice: who has the card all the same (the status
	   line says it -- a program that holds it may silence its input,
	   where Paula's sound may come in) */
	if (!owner[0] && card_there()) {
		CONST_STRPTR who = (CONST_STRPTR) Prisma_GetCardAttr(NULL, PCA_OWNER);

		if (who) Text_Copy((STRPTR) owner, who, OWNER_LEN);
	}
	am.am_ID = way.pw_AhiID;
	am.am_Freq = way.pw_AhiFreq;
	as.as_First = ps->ps_First;
	as.as_End = ps->ps_End;
	as.as_Mode = &am;
	if ((r = Ahi_Start(d, &as)) == PLAY_OK) on_ahi = TRUE;
	return r;
}

BOOL Play_AsRecorded(void)
{
	return (BOOL) !(SysBase->AttnFlags & AFF_68020);
}

BOOL Play_Tick(const struct FeedStep *fs)
{
	if (on_ahi) {
		if (!Ahi_Tick(fs)) on_ahi = FALSE;
		return on_ahi;
	}
	if (sim) {
		SetSignal(0, 1UL << sig);
		if (playing && sim_frame() >= sim_end) playing = FALSE;
		return playing;
	}
	if (!player) return FALSE;
	SetSignal(0, 1UL << sig);
	if (drain(PSTATUS_STOPPED) || gone) {
		/* the stream is done (or the player ended): the card back */
		playing = FALSE;
		Play_Stop();
		return FALSE;
	}
	if (!playing) return FALSE;
	feed();
	/* all of it queued and sent -- the buffer being sent too (PPA_QUEUED
	   leaves it out; an older library without PPA_QUEUEDBYTES gives 0):
	   the chip plays out what it holds */
	if (!left && !head_pending && !stopping && Prisma_GetPlayerAttr(player, PPA_QUEUED) == 0 &&
	    Prisma_GetPlayerAttr(player, PPA_QUEUEDBYTES) == 0) {
		stopping = TRUE;
		Prisma_Stop(player);
	}
	return TRUE;
}

void Play_Feed(struct FeedStep *fs)
{
	fs->fs_Fed = 0;
	fs->fs_Behind = FALSE;
	if (on_ahi) Ahi_Feed(fs);
}

ULONG Play_Frame(void)
{
	ULONG bytes;

	if (on_ahi) return Ahi_Frame();
	if (sim) {
		bytes = playing ? sim_frame() : first_frame;
		/* the end: the signal a card's player would send */
		if (playing && bytes >= sim_end) Signal(FindTask(NULL), 1UL << sig);
		return bytes;
	}
	if (!player) return first_frame;
	bytes = Prisma_GetPlayerAttr(player, PPA_WORDSSENT) * 2;
	bytes = first_frame + (bytes > head_bytes ? (bytes - head_bytes) / align : 0);
	/* the clock's frame where it is behind what was sent (always, but at an
	   underrun, when the head waits for the card) */
	if (Clock_TickHz()) {
		ULONG clk = first_frame + frames_in(Clock_Ms() - start_ms, play_rate);

		if (clk < bytes) return clk;
	}
	return bytes;
}

void Play_Stop(void)
{
	if (on_ahi) {
		Ahi_Stop();
		on_ahi = FALSE;
	}
	/* the tests' player stops as the card's does: its word comes after */
	if (sim && playing) Signal(FindTask(NULL), 1UL << sig);
	if (player) {
		/* a word awaited only when the player took the command */
		if (playing && !stopping && !gone && Prisma_Stop(player) == PMSG_OK) wait_for(PSTATUS_STOPPED);
		if (!gone && Prisma_TerminatePlayer(player) == PMSG_OK) wait_for(PSTATUS_TERMINATED);
		Prisma_DeletePlayer(player);
		player = NULL;
	}
	if (card) {
		Prisma_ReleaseCard();
		card = NULL;
	}
	if (fh) Close(fh);
	fh = 0;
	tl = NULL;
	levels = NULL;
	playing = stopping = FALSE;
	left = 0;
	head_pending = FALSE;
}
