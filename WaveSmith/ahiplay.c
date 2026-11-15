/*
 * ahiplay.c -- listening through AHI (ahiplay.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <utility/hooks.h>
#include <devices/ahi.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>

#include "ahiplay.h"
#include "play.h"
#include "clock.h"
#include "boopsi.h"
#include "wavesmith_msg.h"

/* A sound's frames: 1.49 s at 44.1 kHz, 256 KB of stereo -- the window
   may be busy that long (a page turned deep in the view: 0.6 s on an
   A1200) before the next one must be queued; the first one shorter, for
   a quick start. The free one is filled a piece at a time (Ahi_Feed, the
   play head's beat), at once when its turn comes near */
#define BUF_FRAMES	65536UL
#define FIRST_FRAMES	32768UL
#define PIECE_FRAMES	16384UL		/* a piece at most (piece: what the program's time allows) */
#define LATE_MS		1000UL		/* the playing one's time left: the rest at once */
#define FULL_VOLUME	0x10000L
#define CENTRE		0x8000L
#define REQ_FREQ	44100UL		/* the requester's first rate when none was chosen */

struct Library *AHIBase = NULL;

static struct MsgPort *ahi_port = NULL;
static struct AHIRequest *ahi_req = NULL;
static struct AHIAudioCtrl *ctrl = NULL;
static struct Hook sound_hook;
static struct Task *task = NULL;
static BYTE sig = -1;
static BOOL asked = FALSE, there = FALSE;

static WORD *buf[2] = { NULL, NULL };
static ULONG buf_len[2];		/* frames in each, as queued */
static WORD filling = -1;		/* the free one being filled (-1 none) ... */
static ULONG fill_at = 0;		/* ... its frames so far */
static ULONG piece = PIECE_FRAMES;	/* frames a piece (Ahi_Feed's word) */
static const struct WaveSamples *have[2];	/* Ahi_Feed's fs_Have, during it */
static ULONG copied = 0, readin = 0;	/* the frames filled from have, from the file */
static BOOL check_copies = FALSE;	/* Ahi_CheckCopies() */
static UWORD *check_buf = NULL;		/* ... a piece read for it */
static ULONG differ = 0;		/* ... the copied frames not as read */

/* the hook's: the sound playing and the one queued behind it (the task
   queues under Disable()), the frames of those ended, and the E-clock when
   the one playing began -- read there, in the mixing interrupt: taken when
   the task came round to it (on the A2000, busy painting, up to a fifth
   of a second late), the frame stood at the sound's end meanwhile and the
   sprite head, carried on by the clock, stepped back. A sound begins only
   when one was queued; with none queued in time (an underrun) AHI plays
   the current one again -- from its start, nothing else moves */
#define SND_NONE	-1
#define SND_END		2		/* the silence after the last */
static volatile WORD h_cur = SND_NONE, h_queued = SND_NONE;
static volatile ULONG h_done = 0, h_tick = 0;
static BOOL ran_out = FALSE;		/* the last played out by the clock */

static BPTR fh = 0;
static const struct Timeline *tl = NULL;	/* the doc's, while it plays */
static const struct TlLevels *levels = NULL;	/* and its end fades and dips */
static ULONG data_start = 0, next = 0, left = 0, first_frame = 0, rate = 44100;
static UWORD align = 4, channels = 2;
static BOOL playing = FALSE;
static ULONG done = 0;			/* frames of the sounds ended */
static ULONG cur_tick = 0, cur_len = 0;	/* the sound playing: when it began (E-clock), its frames */
static ULONG end_frame = 0;		/* where the span ends on the timeline */

/* the frames rate plays in ms milliseconds, without the product that
   wraps after a minute and a half */
static ULONG frames_in(ULONG ms, ULONG r)
{
	return ms / 1000UL * r + ms % 1000UL * r / 1000UL;
}

/* the ms since the E-clock read tick (0 without the clock) */
static ULONG ms_since(ULONG tick)
{
	ULONG hz = Clock_TickHz(), e;

	if (!hz) return 0;
	e = Clock_Ticks() - tick;
	return e / hz * 1000UL + e % hz * 1000UL / hz;
}

/* AHI's sound hook, in its mixing interrupt: a sound began -- the queued
   one (the one before it ended), or the current one again; its start
   taken (ReadEClock() may be called from interrupts), the task told */
static DISP_QUAL ULONG sound_func(REG(a0, struct Hook *h), REG(a2, struct AHIAudioCtrl *ac), REG(a1, APTR msg))
{
	WORD c = h_cur, q = h_queued;

	(void) h;
	(void) ac;
	(void) msg;
	h_tick = Clock_Ticks();
	if (q != SND_NONE) {
		if (c == 0 || c == 1) h_done += buf_len[c];
		h_cur = q;
		h_queued = SND_NONE;
	}
	Signal(task, 1UL << sig);
	return 0;
}

/* ahi.device opened for its library (no unit): FALSE without it */
static BOOL open_device(void)
{
	if (AHIBase) return TRUE;
	if (!(ahi_port = CreateMsgPort())) return FALSE;
	if (!(ahi_req = (struct AHIRequest *) CreateIORequest(ahi_port, sizeof(struct AHIRequest)))) {
		DeleteMsgPort(ahi_port);
		ahi_port = NULL;
		return FALSE;
	}
	ahi_req->ahir_Version = 4;
	if (OpenDevice((STRPTR) AHINAME, AHI_NO_UNIT, (struct IORequest *) ahi_req, 0) != 0) {
		DeleteIORequest((struct IORequest *) ahi_req);
		DeleteMsgPort(ahi_port);
		ahi_req = NULL;
		ahi_port = NULL;
		return FALSE;
	}
	AHIBase = (struct Library *) ahi_req->ahir_Std.io_Device;
	return TRUE;
}

static void close_device(void)
{
	if (!AHIBase) return;
	CloseDevice((struct IORequest *) ahi_req);
	DeleteIORequest((struct IORequest *) ahi_req);
	DeleteMsgPort(ahi_port);
	ahi_req = NULL;
	ahi_port = NULL;
	AHIBase = NULL;
}

BOOL Ahi_Open(void)
{
	task = FindTask(NULL);
	if (sig < 0) sig = AllocSignal(-1);
	return (BOOL) (sig >= 0);
}

BOOL Ahi_There(void)
{
	BOOL mine;

	if (!asked) {
		/* the device closed again only when opened here: not under a play */
		asked = TRUE;
		mine = (BOOL) !AHIBase;
		there = open_device();
		if (mine) close_device();
	}
	return there;
}

ULONG Ahi_Signal(void)
{
	return sig >= 0 ? 1UL << sig : 0;
}

BOOL Ahi_Playing(void)
{
	return playing;
}

/* the frames from next on that samples read already hold (have: the
   same Tl_Read() at the same levels, words in the machine's order) --
   copied into w, as many as one holds up to want; 0 when none holds the
   first. Not while the levels are left out (Play_AsRecorded()) */
static ULONG from_have(UWORD *w, ULONG want)
{
	const struct WaveSamples *ws;
	ULONG n;
	UWORD k;

	if (Play_AsRecorded() || align != channels * 2) return 0;
	for (k = 0; k < 2; k++) {
		ws = have[k];
		if (!ws || !ws->wsm_Data || ws->wsm_Channels != channels || next < ws->wsm_First ||
		    next - ws->wsm_First >= ws->wsm_Frames)
			continue;
		n = ws->wsm_Frames - (next - ws->wsm_First);
		if (n > want) n = want;
		CopyMem(ws->wsm_Data + (next - ws->wsm_First) * channels, w, n * align);
		return n;
	}
	return 0;
}

/* frames frames from first along the timeline into w, the machine's
   order: how many */
static ULONG read_into(UWORD *w, ULONG first, ULONG frames)
{
	struct TlRead rd;
	ULONG got, n;

	Tl_ReadInit(&rd);
	rd.rd_FH = fh;
	rd.rd_Start = data_start;
	rd.rd_Align = align;
	rd.rd_Pad = 0;
	rd.rd_First = first;
	rd.rd_Frames = frames;
	rd.rd_Buf = (UBYTE *) w;
	rd.rd_Levels = Tl_Levels(levels);
	rd.rd_AsRecorded = Play_AsRecorded();
	got = Tl_Read(tl, &rd);
	/* a word at a time (a byte at a time took twice as long) */
	for (n = got * (align / 2); n; n--, w++) *w = (UWORD) ((*w >> 8) | (*w << 8));
	return got;
}

/* the tests' check: n frames copied to w, from first, read as well --
   the frames that differ counted */
static void check_copy(const UWORD *w, ULONG first, ULONG n)
{
	ULONG got, i, k, per = align / 2;

	if (!check_buf || (got = read_into(check_buf, first, n)) != n) {
		differ += n;
		return;
	}
	for (i = 0; i < n; i++)
		for (k = 0; k < per; k++)
			if (w[i * per + k] != check_buf[i * per + k]) {
				differ++;
				break;
			}
}

/* a piece more into buffer b, up to cap frames in it, along the
   timeline -- copied from samples read already, else read: the file's
   little-endian words in the machine's order; its frames (0: the span
   is done) */
static ULONG fill_piece(UWORD b, ULONG cap)
{
	ULONG want = cap - fill_at, got = 0;
	UWORD *w = (UWORD *) buf[b] + fill_at * (align / 2);

	if (want > piece) want = piece;
	if (want > left) want = left;
	if (want && (got = from_have(w, want))) {
		if (check_copies) check_copy(w, next, got);
		copied += got;
	} else if (want) {
		got = read_into(w, next, want);
		readin += got;
	}
	if (!got) {
		left = 0;		/* a short file: what came is it */
	} else {
		next += got;
		left -= got;
	}
	fill_at += got;
	return got;
}

/* the hook's word read, all of it at once: the one playing and the one
   queued, the frames ended, when the one playing began */
static void look(WORD *cur, WORD *q)
{
	Disable();
	*cur = h_cur;
	*q = h_queued;
	done = h_done;
	cur_tick = h_tick;
	Enable();
	if (*cur == 0 || *cur == 1) cur_len = buf_len[*cur];
}

/* the free buffer a piece further, queued behind the one playing once it
   is full or the span through; with less than LATE_MS of that one left,
   the rest at once (the sound first). Behind: less in it than its share
   of the time it has -- full when LATE_MS are left -- another piece is
   wanted at once (the controls looked at between them) */
static void feed(struct FeedStep *fs)
{
	WORD cur, q, b;
	ULONG play_ms, past, n, want;

	fs->fs_Fed = 0;
	fs->fs_Behind = FALSE;
	look(&cur, &q);
	if ((cur != 0 && cur != 1) || q != SND_NONE) return;
	b = (WORD) (1 - cur);
	if (filling != b) {
		/* free since the other one began */
		filling = b;
		fill_at = 0;
	}
	play_ms = rate ? cur_len / rate * 1000UL + cur_len % rate * 1000UL / rate : 0;
	past = ms_since(cur_tick);
	if (fill_at < BUF_FRAMES && left && (fs->fs_Fed = fill_piece((UWORD) b, BUF_FRAMES)) && past + LATE_MS >= play_ms)
		while (fill_at < BUF_FRAMES && left && (n = fill_piece((UWORD) b, BUF_FRAMES))) fs->fs_Fed += n;
	if (fill_at < BUF_FRAMES && left) {
		/* (65536 frames at most, times of a minute at most: the products fit) */
		want = fill_at + left < BUF_FRAMES ? fill_at + left : BUF_FRAMES;
		fs->fs_Behind = (BOOL) (play_ms > LATE_MS && fill_at * (play_ms - LATE_MS) < past * want);
		return;
	}
	buf_len[b] = fill_at;
	filling = -1;
	Disable();
	if (buf_len[b]) {
		AHI_SetSound(0, (UWORD) b, 0, (LONG) buf_len[b], ctrl, 0);
		h_queued = b;
	} else {
		AHI_SetSound(0, AHI_NOSOUND, 0, 0, ctrl, 0);
		h_queued = SND_END;
	}
	Enable();
}

UWORD Ahi_Start(struct Doc *d, const struct AhiSpan *as)
{
	struct AHISampleInfo si;
	ULONG end = as->as_End > Doc_Length(d) ? Doc_Length(d) : as->as_End;
	UWORD b;

	Ahi_Stop();
	if (as->as_First >= end) return PLAY_FILE;
	if (d->d_Channels != 1 && d->d_Channels != 2) return PLAY_FILE;
	if (!Ahi_Open()) return AHIPLAY_NOMEMORY;
	if (!open_device()) return AHIPLAY_NODEVICE;
	align = (UWORD) d->d_Wav.nw_Align;
	channels = d->d_Channels;
	copied = readin = differ = 0;
	have[0] = have[1] = NULL;
	if (check_copies && !(check_buf = (UWORD *) AllocVec(PIECE_FRAMES * align, MEMF_ANY))) {
		Ahi_Stop();
		return AHIPLAY_NOMEMORY;
	}
	for (b = 0; b < 2; b++) {
		if (!(buf[b] = (WORD *) AllocVec(BUF_FRAMES * align, MEMF_PUBLIC | MEMF_CLEAR))) {
			Ahi_Stop();
			return AHIPLAY_NOMEMORY;
		}
	}
	if (!(fh = Open((STRPTR) d->d_Path, MODE_OLDFILE))) {
		Ahi_Stop();
		return PLAY_FILE;
	}
	sound_hook.h_MinNode.mln_Succ = NULL;
	sound_hook.h_MinNode.mln_Pred = NULL;
	/* h_Entry's own type: this GCC's NDK makes ULONG uint32_t, so the
	   HOOKFUNC typedef does not match it (as AHIBench) */
	sound_hook.h_Entry = (ULONG (*)()) sound_func;
	sound_hook.h_SubEntry = NULL;
	sound_hook.h_Data = NULL;
	h_cur = h_queued = SND_NONE;
	h_done = 0;
	ran_out = FALSE;
	filling = -1;
	fill_at = 0;
	/* the default mode at its own rate (a rate kept with it by an older
	   WaveSmith was the requester's Paula clock) */
	if (!(ctrl = AHI_AllocAudio(AHIA_AudioID, as->as_Mode && as->as_Mode->am_ID ? as->as_Mode->am_ID : AHI_DEFAULT_ID,
				    AHIA_MixFreq, as->as_Mode && as->as_Mode->am_ID && as->as_Mode->am_Freq ? as->as_Mode->am_Freq : AHI_DEFAULT_FREQ,
				    AHIA_Channels, 1, AHIA_Sounds, 2, AHIA_SoundFunc, (ULONG) &sound_hook, TAG_DONE))) {
		Ahi_Stop();
		return AHIPLAY_NOMODE;
	}
	for (b = 0; b < 2; b++) {
		si.ahisi_Type = d->d_Channels == 2 ? AHIST_S16S : AHIST_M16S;
		si.ahisi_Address = buf[b];
		si.ahisi_Length = BUF_FRAMES;
		if (AHI_LoadSound(b, AHIST_DYNAMICSAMPLE, &si, ctrl) != AHIE_OK) {
			Ahi_Stop();
			return AHIPLAY_NOMODE;
		}
	}
	tl = &d->d_Tl;
	levels = &d->d_Levels;
	data_start = d->d_Wav.nw_DataStart;
	first_frame = next = as->as_First;
	left = end - as->as_First;
	end_frame = end;
	rate = d->d_Rate;
	done = 0;
	/* the first sound, shorter: the second is filled while it plays */
	while (fill_at < FIRST_FRAMES && left && fill_piece(0, FIRST_FRAMES)) {
	}
	buf_len[0] = fill_at;
	fill_at = 0;
	if (!buf_len[0]) {
		/* nothing could be read */
		Ahi_Stop();
		return PLAY_FILE;
	}
	if (AHI_ControlAudio(ctrl, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK) {
		Ahi_Stop();
		return AHIPLAY_NOMODE;
	}
	AHI_SetFreq(0, rate, ctrl, AHISF_IMM);
	AHI_SetVol(0, FULL_VOLUME, CENTRE, ctrl, AHISF_IMM);
	h_tick = cur_tick = Clock_Ticks();
	cur_len = buf_len[0];
	playing = TRUE;
	/* the first at once; the hook's word of its start queues the second */
	h_queued = 0;
	AHI_SetSound(0, 0, 0, (LONG) buf_len[0], ctrl, AHISF_IMM);
	return PLAY_OK;
}

BOOL Ahi_Tick(const struct FeedStep *give)
{
	struct FeedStep fs;
	WORD cur, q;

	if (sig >= 0) SetSignal(0, 1UL << sig);
	if (!playing || !ctrl) return FALSE;
	look(&cur, &q);
	if (cur == SND_END || ran_out) {
		/* the silence after the last: it has played out */
		playing = FALSE;
		Ahi_Stop();
		return FALSE;
	}
	/* the other one: ended (filled anew) -- a piece of it, queued behind
	   this one once full (h_cur does not move while nothing is) */
	fs.fs_Piece = piece;
	have[0] = give ? give->fs_Have[0] : NULL;
	have[1] = give ? give->fs_Have[1] : NULL;
	feed(&fs);
	have[0] = have[1] = NULL;
	return TRUE;
}

void Ahi_Feed(struct FeedStep *fs)
{
	WORD cur, q;

	fs->fs_Fed = 0;
	fs->fs_Behind = FALSE;
	if (!playing || !ctrl) return;
	piece = fs->fs_Piece && fs->fs_Piece < PIECE_FRAMES ? fs->fs_Piece : PIECE_FRAMES;
	have[0] = fs->fs_Have[0];
	have[1] = fs->fs_Have[1];
	look(&cur, &q);
	/* the end is Ahi_Tick's (the hook's signal, or Ahi_Frame's) */
	if (cur != SND_END && !ran_out) feed(fs);
	/* the caller's, of this call only */
	have[0] = have[1] = NULL;
}

void Ahi_Counts(struct AhiCounts *ac)
{
	ac->ac_Copied = copied;
	ac->ac_Read = readin;
	ac->ac_Differ = differ;
}

void Ahi_CheckCopies(BOOL on)
{
	check_copies = on;
}

ULONG Ahi_Frame(void)
{
	WORD cur, q;
	ULONG f, base, ms;

	if (!playing) return first_frame + done;
	/* the hook's word read here too: the frame never waits for a beat */
	look(&cur, &q);
	base = first_frame + done;
	ms = ms_since(cur_tick);
	f = base + frames_in(ms, rate);
	if (f > base + cur_len) f = base + cur_len;
	/* the last sound played out by the clock: the word the hook may not
	   send for the silence after it */
	if (q == SND_END && !ran_out && f >= base + cur_len && Clock_TickHz() &&
	    ms > cur_len * 1000UL / rate + 100) {
		ran_out = TRUE;
		Signal(task, 1UL << sig);
	}
	return f < end_frame ? f : end_frame;
}

void Ahi_Stop(void)
{
	UWORD b;

	if (ctrl) {
		AHI_ControlAudio(ctrl, AHIC_Play, FALSE, TAG_DONE);
		for (b = 0; b < 2; b++) AHI_UnloadSound(b, ctrl);
		AHI_FreeAudio(ctrl);
		ctrl = NULL;
	}
	close_device();
	for (b = 0; b < 2; b++) {
		if (buf[b]) FreeVec(buf[b]);
		buf[b] = NULL;
		buf_len[b] = 0;
	}
	if (check_buf) FreeVec(check_buf);
	check_buf = NULL;
	if (fh) Close(fh);
	fh = 0;
	tl = NULL;
	levels = NULL;
	playing = ran_out = FALSE;
	filling = -1;
	fill_at = 0;
	h_cur = h_queued = SND_NONE;
	left = 0;
}

void Ahi_Close(void)
{
	Ahi_Stop();
	if (sig >= 0) FreeSignal(sig);
	sig = -1;
}

BOOL Ahi_Request(struct Window *w, struct AhiMode *am)
{
	struct AHIAudioModeRequester *req;
	BOOL ok = FALSE, mine = (BOOL) !AHIBase;

	if (!open_device()) return FALSE;
	/* AHI_DEFAULT_FREQ is for AHI_AllocAudio() alone: given to the
	   requester it showed Paula's clock (3546895) as the default mode's
	   rate */
	if ((req = AHI_AllocAudioRequest(AHIR_Window, (ULONG) w, AHIR_SleepWindow, TRUE, AHIR_TitleText, (ULONG) TXT_AHI_TITLE,
					 AHIR_InitialAudioID, am->am_ID ? am->am_ID : AHI_DEFAULT_ID,
					 AHIR_InitialMixFreq, am->am_ID && am->am_Freq ? am->am_Freq : REQ_FREQ,
					 AHIR_DoMixFreq, TRUE, AHIR_DoDefaultMode, TRUE, TAG_DONE))) {
		if (AHI_AudioRequest(req, TAG_DONE)) {
			/* the default mode plays at the rate AHI's settings give it,
			   whatever the requester's slider said */
			am->am_ID = req->ahiam_AudioID;
			am->am_Freq = am->am_ID != AHI_DEFAULT_ID ? req->ahiam_MixFreq : AHI_DEFAULT_FREQ;
			ok = TRUE;
		}
		AHI_FreeAudioRequest(req);
	}
	if (mine) close_device();
	return ok;
}

BOOL Ahi_ModeName(const struct AhiName *an)
{
	BOOL ok, mine = (BOOL) !AHIBase;

	if (an->an_Size) an->an_Buf[0] = '\0';
	if (!an->an_Mode->am_ID || !an->an_Size || !open_device()) return FALSE;
	ok = AHI_GetAudioAttrs(an->an_Mode->am_ID, NULL, AHIDB_Name, (ULONG) an->an_Buf, AHIDB_BufferLen, an->an_Size, TAG_DONE);
	if (mine) close_device();
	return ok;
}
