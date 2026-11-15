/*
 * prefetch.c -- the samples ahead of the play head (prefetch.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "prefetch.h"

static void clear_bytes(UBYTE *p, ULONG n)
{
	while (n--) *p++ = 0;
}

/* the words as loaded (the file's little-endian bytes) in the machine's order */
static void swap_words(UWORD *w, ULONG n)
{
	while (n--) {
		*w = (UWORD) ((*w >> 8) | (*w << 8));
		w++;
	}
}

BOOL Prefetch_Open(struct Prefetch *pf, const struct Doc *d, ULONG max)
{
	UWORD k;

	clear_bytes((UBYTE *) pf, sizeof(*pf));
	pf->pf_Max = max;
	pf->pf_Piece = PREFETCH_PIECE;
	pf->pf_Tl = &d->d_Tl;
	pf->pf_Levels = &d->d_Levels;
	pf->pf_Frames = Doc_Length(d);
	pf->pf_Start = d->d_Wav.nw_DataStart;
	pf->pf_Align = (UWORD) d->d_Wav.nw_Align;
	pf->pf_Channels = d->d_Channels;
	pf->pf_First = WAVEFORM_NONE;
	for (k = 0; k < 2; k++) {
		if (!(pf->pf_Buf[k] = (WORD *) AllocVec(max * pf->pf_Align, MEMF_ANY))) {
			Prefetch_Close(pf);
			return FALSE;
		}
	}
	if (!(pf->pf_FH = Open((STRPTR) d->d_Path, MODE_OLDFILE))) {
		Prefetch_Close(pf);
		return FALSE;
	}
	return TRUE;
}

void Prefetch_Close(struct Prefetch *pf)
{
	UWORD k;

	if (pf->pf_FH) Close(pf->pf_FH);
	for (k = 0; k < 2; k++) {
		if (pf->pf_Buf[k]) FreeVec(pf->pf_Buf[k]);
	}
	clear_bytes((UBYTE *) pf, sizeof(*pf));
}

void Prefetch_Want(struct Prefetch *pf, ULONG first, ULONG frames, const struct WaveSamples *have)
{
	ULONG n = 0, kept = 0, i;
	WORD *to, *from;

	if (!pf->pf_FH || first >= pf->pf_Frames) return;
	if (frames > pf->pf_Frames - first) frames = pf->pf_Frames - first;
	if (frames > pf->pf_Max) frames = pf->pf_Max;
	/* being filled, or filled and shown, or given up: nothing more to do
	   -- unless shown and the gadget took other samples since (a fetch
	   at a turn, then Stop back on this page: fetched again, 1.6 s on an
	   A2000): the buffer, as it was, handed again */
	if (first == pf->pf_First && frames == pf->pf_Want) {
		/* a copy not through, the gadget let go of its samples: the rest read */
		if (pf->pf_Got < pf->pf_Copy && (!have || have->wsm_Data != pf->pf_From || have->wsm_First != pf->pf_FromFirst))
			pf->pf_Copy = pf->pf_Got;
		if (pf->pf_Handed && (!have || have->wsm_Data != pf->pf_Buf[1 - pf->pf_Fill])) {
			pf->pf_Fill = (UWORD) (1 - pf->pf_Fill);
			pf->pf_Handed = FALSE;
		}
		return;
	}
	/* what the buffer being filled holds of it already (a page turned
	   before the read was through: it would never be): kept, moved to
	   its start */
	if (!pf->pf_Handed && !pf->pf_Failed && pf->pf_First != WAVEFORM_NONE && first >= pf->pf_First &&
	    first < pf->pf_First + pf->pf_Got) {
		kept = pf->pf_First + pf->pf_Got - first;
		if (kept > frames) kept = frames;
	}
	/* what the gadget's samples hold of it (never the buffer being
	   filled: the gadget is not handed that one) */
	if (have && have->wsm_Data && have->wsm_Channels == pf->pf_Channels && have->wsm_Data != pf->pf_Buf[pf->pf_Fill] &&
	    first >= have->wsm_First && first < have->wsm_First + have->wsm_Frames) {
		n = have->wsm_First + have->wsm_Frames - first;
		if (n > frames) n = frames;
	}
	pf->pf_Copy = 0;
	if (kept && kept >= n) {
		/* forward, word by word: the two may overlap, the start first */
		to = pf->pf_Buf[pf->pf_Fill];
		from = to + (first - pf->pf_First) * pf->pf_Channels;
		if (from != to)
			for (i = kept * pf->pf_Channels; i; i--) *to++ = *from++;
		n = kept;
	} else if (n) {
		/* copied a piece a step, not read again -- at once, a turn's
		   overlap (1.9 MB at 10 s a page) cost an A2000 320 ms */
		pf->pf_From = have->wsm_Data;
		pf->pf_FromFirst = have->wsm_First;
		pf->pf_Copy = n;
		n = 0;
	}
	pf->pf_First = first;
	pf->pf_Want = frames;
	pf->pf_Got = n;
	pf->pf_Handed = pf->pf_Failed = FALSE;
}

const struct WaveSamples *Prefetch_Part(struct Prefetch *pf)
{
	if (!pf->pf_FH || pf->pf_First == WAVEFORM_NONE || pf->pf_Handed || !pf->pf_Got) return NULL;
	pf->pf_Part.wsm_First = pf->pf_First;
	pf->pf_Part.wsm_Frames = pf->pf_Got;
	pf->pf_Part.wsm_Channels = pf->pf_Channels;
	pf->pf_Part.wsm_Pad = 0;
	pf->pf_Part.wsm_Data = pf->pf_Buf[pf->pf_Fill];
	return &pf->pf_Part;
}

struct WaveSamples *Prefetch_Step(struct Prefetch *pf)
{
	struct WaveSamples *ws;
	struct TlRead rd;
	ULONG n;
	WORD *at;

	pf->pf_Read = pf->pf_Copied = 0;
	if (!pf->pf_FH || pf->pf_First == WAVEFORM_NONE || pf->pf_Handed || pf->pf_Failed) return NULL;
	n = pf->pf_Piece && pf->pf_Piece < PREFETCH_PIECE ? pf->pf_Piece : PREFETCH_PIECE;
	if (pf->pf_Got < pf->pf_Copy) {
		/* the gadget's part first, PREFETCH_COPY pieces' worth */
		n *= PREFETCH_COPY;
		if (n > pf->pf_Copy - pf->pf_Got) n = pf->pf_Copy - pf->pf_Got;
		CopyMem((APTR) (pf->pf_From + (pf->pf_First + pf->pf_Got - pf->pf_FromFirst) * pf->pf_Channels),
			pf->pf_Buf[pf->pf_Fill] + pf->pf_Got * pf->pf_Channels, n * pf->pf_Align);
		pf->pf_Got += n;
		pf->pf_Copied = n;
		if (pf->pf_Got < pf->pf_Want) return NULL;
	} else if (pf->pf_Got < pf->pf_Want) {
		if (n > pf->pf_Want - pf->pf_Got) n = pf->pf_Want - pf->pf_Got;
		at = pf->pf_Buf[pf->pf_Fill] + pf->pf_Got * pf->pf_Channels;
		Tl_ReadInit(&rd);
		rd.rd_FH = pf->pf_FH;
		rd.rd_Start = pf->pf_Start;
		rd.rd_Align = pf->pf_Align;
		rd.rd_Pad = 0;
		rd.rd_First = pf->pf_First + pf->pf_Got;
		rd.rd_Frames = n;
		rd.rd_Buf = (UBYTE *) at;
		rd.rd_Levels = Tl_Levels(pf->pf_Levels);
		if (!(n = Tl_Read(pf->pf_Tl, &rd))) {
			pf->pf_Failed = TRUE;	/* the file would not give it: this window given up */
			return NULL;
		}
		swap_words((UWORD *) at, n * pf->pf_Channels);
		pf->pf_Got += n;
		pf->pf_Read = n;
		if (pf->pf_Got < pf->pf_Want) return NULL;
	}
	/* whole: the gadget's from now on; the other buffer is filled next */
	ws = &pf->pf_WS[pf->pf_Fill];
	ws->wsm_First = pf->pf_First;
	ws->wsm_Frames = pf->pf_Got;
	ws->wsm_Channels = pf->pf_Channels;
	ws->wsm_Pad = 0;
	ws->wsm_Data = pf->pf_Buf[pf->pf_Fill];
	pf->pf_Fill = (UWORD) (1 - pf->pf_Fill);
	pf->pf_Handed = TRUE;
	return ws;
}
