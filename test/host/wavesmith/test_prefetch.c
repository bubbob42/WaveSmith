/*
 * test_prefetch.c -- WaveSmith's prefetch.c, the samples ahead of the play
 * head, on the test single (make_edit_single.py): what it hands must be
 * what Doc_Fetch() reads of the same frames; a window moved before its
 * read was through keeps what was read of it (a page turned while the
 * read-ahead ran: it started over and was never through on an A1200);
 * what the gadget's samples hold of a window is copied, whoever read
 * them (a handed window, a fetch); a window apart from all reads it all.
 *   test_prefetch <drawer> <Single.wav>
 * The samples are turned big-endian in a copy first, as test_docends.c
 * does: prefetch.c and doc.c read them as the Amiga loads them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>

#include "doc.h"
#include "prefetch.h"
#include "doshost.h"
#include "check.h"

#define PATH_LEN	512
#define VIEW		110360UL	/* a page: 620 columns at 178 frames */
#define LEAD		(38UL * 178UL)	/* the head's sixteenth of the columns */
#define AT		421544UL	/* where the measured play turned its first page */

static struct Doc doc;
static struct Prefetch pf;

static int be_copy(const char *from, const char *to)
{
	FILE *f = fopen(from, "rb");
	unsigned char *b;
	unsigned long n, at, size, i;
	unsigned char t;
	int ok = 0;

	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	n = (unsigned long) ftell(f);
	fseek(f, 0, SEEK_SET);
	if (!(b = (unsigned char *) malloc(n)) || fread(b, 1, n, f) != n) {
		fclose(f);
		free(b);
		return 0;
	}
	fclose(f);
	for (at = 12; at + 8 <= n; at += 8 + size + (size & 1)) {
		size = b[at + 4] | ((unsigned long) b[at + 5] << 8) | ((unsigned long) b[at + 6] << 16) | ((unsigned long) b[at + 7] << 24);
		if (memcmp(b + at, "data", 4) == 0) {
			for (i = at + 8; i + 1 < at + 8 + size && i + 1 < n; i += 2) {
				t = b[i];
				b[i] = b[i + 1];
				b[i + 1] = t;
			}
			ok = 1;
			break;
		}
	}
	if (ok && (!(f = fopen(to, "wb")) || fwrite(b, 1, n, f) != n)) ok = 0;
	if (f) fclose(f);
	free(b);
	return ok;
}

static ULONG read_all, copied_all, copy_most;	/* step_out()'s: frames read, copied, the most a step copied */

/* steps until the window is handed (at most 1000): how many, the samples */
static struct WaveSamples *step_out(ULONG *steps)
{
	struct WaveSamples *ws = NULL;

	read_all = copied_all = copy_most = 0;
	for (*steps = 0; *steps < 1000 && !ws; (*steps)++) {
		ws = Prefetch_Step(&pf);
		read_all += pf.pf_Read;
		copied_all += pf.pf_Copied;
		if (pf.pf_Copied > copy_most) copy_most = pf.pf_Copied;
	}
	return ws;
}

/* the samples as Doc_Fetch() reads the same frames */
static int same_as_fetch(const struct WaveSamples *ws, ULONG first, ULONG frames)
{
	const struct WaveSamples *ref;
	WORD *keep;
	size_t bytes = (size_t) frames * doc.d_Channels * 2;
	int same;

	if (!ws || ws->wsm_First != first || ws->wsm_Frames != frames || ws->wsm_Channels != doc.d_Channels) return 0;
	if (!(keep = (WORD *) malloc(bytes))) return 0;
	memcpy(keep, ws->wsm_Data, bytes);
	ref = Doc_Fetch(&doc, first, frames);
	same = ref && ref->wsm_Frames == frames && memcmp(ref->wsm_Data, keep, bytes) == 0;
	free(keep);
	return same;
}

int main(int argc, char **argv)
{
	char single[PATH_LEN];
	struct WaveSamples *ws;
	ULONG steps, b, c, kept;

	if (argc < 3) return 2;
	dos_host.dh_WriteLeft = -1;
	sprintf(single, "%s/Prefetch.wav", argv[1]);
	CHECK(be_copy(argv[2], single), "the single copied, its samples big-endian");
	CHECK(Doc_Open(&doc, (CONST_STRPTR) single) == DOC_OK, "opened");
	while (Doc_Read(&doc)) {
	}
	CHECK(doc.d_Done && Doc_Length(&doc) == 1764000UL, "read: 40 s");
	CHECK(Prefetch_Open(&pf, &doc, 2 * VIEW), "the read-ahead open, two pages a buffer");

	/* a few pieces of the first window, then the page turns */
	Prefetch_Want(&pf, AT, 2 * VIEW, NULL);
	for (steps = 0; steps < 5; steps++) CHECK(Prefetch_Step(&pf) == NULL, "a piece: not through yet");
	CHECK(pf.pf_Got == 5 * PREFETCH_PIECE, "five pieces read");
	{
		/* what AHI may copy instead of reading again */
		const struct WaveSamples *part = Prefetch_Part(&pf);

		CHECK(part && part->wsm_First == AT && part->wsm_Frames == 5 * PREFETCH_PIECE &&
		      same_as_fetch(part, AT, 5 * PREFETCH_PIECE), "the part read so far: its frames as Doc_Fetch reads them");
	}
	b = AT + 20000UL;
	Prefetch_Want(&pf, b, 2 * VIEW, NULL);
	kept = pf.pf_Got;
	CHECK(kept == 5 * PREFETCH_PIECE - 20000UL, "moved on before it was through: what it read of the new window kept");
	ws = step_out(&steps);
	CHECK(steps == (2 * VIEW - kept + PREFETCH_PIECE - 1) / PREFETCH_PIECE, "only the rest read");
	CHECK(same_as_fetch(ws, b, 2 * VIEW), "the window as Doc_Fetch reads it");
	CHECK(Prefetch_Step(&pf) == NULL, "handed: nothing more");
	CHECK(Prefetch_Part(&pf) == NULL, "handed: no part being filled");

	/* the next page: their overlap copied from the window handed, which
	   the gadget draws from */
	c = b + VIEW - LEAD;
	Prefetch_Want(&pf, c, 2 * VIEW, ws);
	CHECK(pf.pf_Got == 0 && pf.pf_Copy == b + 2 * VIEW - c, "the next page: the overlap with the gadget's samples to copy");
	ws = step_out(&steps);
	CHECK(copied_all == b + 2 * VIEW - c && read_all == c - b, "the overlap copied, a page's worth read");
	CHECK(copy_most == PREFETCH_COPY * PREFETCH_PIECE, "copied a piece a step: so many pieces' worth, not at once");
	CHECK(steps == (b + 2 * VIEW - c + PREFETCH_COPY * PREFETCH_PIECE - 1) / (PREFETCH_COPY * PREFETCH_PIECE) +
		       (c - b + PREFETCH_PIECE - 1) / PREFETCH_PIECE, "the copy's steps, then the read's");
	CHECK(same_as_fetch(ws, c, 2 * VIEW), "the next window as Doc_Fetch reads it");

	/* the same window again: nothing */
	Prefetch_Want(&pf, c, 2 * VIEW, ws);
	CHECK(Prefetch_Step(&pf) == NULL, "the same window: nothing to read");

	/* the same window, the gadget drawing from other samples since (a
	   fetch at a turn, then Stop back on this page): handed again, as it
	   was, nothing read */
	{
		const struct WaveSamples *fetched = Doc_Fetch(&doc, 1300000UL, VIEW);
		struct WaveSamples *again;

		CHECK(fetched != NULL, "a page fetched elsewhere");
		Prefetch_Want(&pf, c, 2 * VIEW, fetched);
		again = Prefetch_Step(&pf);
		CHECK(again == ws && pf.pf_Read == 0, "the gadget took other samples: the window handed again, nothing read");
		CHECK(same_as_fetch(again, c, 2 * VIEW), "handed again: the window as Doc_Fetch reads it");
		CHECK(Prefetch_Step(&pf) == NULL, "handed again: nothing more");
		Prefetch_Want(&pf, c, 2 * VIEW, NULL);
		CHECK(Prefetch_Step(&pf) == ws && pf.pf_Read == 0, "no samples at all: handed again too");
		Prefetch_Want(&pf, c, 2 * VIEW, ws);
		CHECK(Prefetch_Step(&pf) == NULL, "the gadget draws from it again: nothing");
	}

	/* the gadget drawing from a fetch (a page fetched at a turn): this
	   page copied from it, only the next one read */
	{
		const struct WaveSamples *fetched = Doc_Fetch(&doc, 900000UL, VIEW);

		CHECK(fetched != NULL, "a page fetched");
		Prefetch_Want(&pf, 900000UL, 2 * VIEW, fetched);
		ws = step_out(&steps);
		CHECK(copied_all == VIEW && read_all == VIEW, "the fetched page copied, only the next page read");
		CHECK(same_as_fetch(ws, 900000UL, 2 * VIEW), "the window as Doc_Fetch reads it");
	}

	/* the gadget let go of its samples before the copy was through
	   (another fetch): what was copied kept, the rest read */
	{
		const struct WaveSamples *fetched = Doc_Fetch(&doc, 600000UL, VIEW);

		CHECK(fetched != NULL, "another page fetched");
		Prefetch_Want(&pf, 600000UL, 2 * VIEW, fetched);
		CHECK(Prefetch_Step(&pf) == NULL && pf.pf_Copied == PREFETCH_COPY * PREFETCH_PIECE && pf.pf_Read == 0,
		      "a piece copied");
		Prefetch_Want(&pf, 600000UL, 2 * VIEW, NULL);
		ws = step_out(&steps);
		CHECK(copied_all == 0 && read_all == 2 * VIEW - PREFETCH_COPY * PREFETCH_PIECE,
		      "let go of: no more copied, the rest read");
		CHECK(same_as_fetch(ws, 600000UL, 2 * VIEW), "the window as Doc_Fetch reads it");
	}

	/* back before all: read whole */
	Prefetch_Want(&pf, 1000UL, 2 * VIEW, ws);
	CHECK(pf.pf_Got == 0, "a window apart: nothing kept");
	Prefetch_Want(&pf, 1000UL + 3 * PREFETCH_PIECE, 2 * VIEW, ws);
	CHECK(pf.pf_Got == 0, "moved on before a piece was read: nothing to keep");
	(void) Prefetch_Step(&pf);
	(void) Prefetch_Step(&pf);
	/* the read-ahead's own window moved back: no frame of the new one read */
	Prefetch_Want(&pf, 1000UL, 2 * VIEW, ws);
	CHECK(pf.pf_Got == 0, "moved back before what it read: nothing kept");
	ws = step_out(&steps);
	CHECK(same_as_fetch(ws, 1000UL, 2 * VIEW), "read whole, as Doc_Fetch reads it");

	/* at the end of the sound: the window cut */
	Prefetch_Want(&pf, 1764000UL - 50000UL, 2 * VIEW, ws);
	ws = step_out(&steps);
	CHECK(same_as_fetch(ws, 1764000UL - 50000UL, 50000UL), "at the end: cut to the sound");

	/* a smaller piece (the time the program has): read, and said */
	Prefetch_Want(&pf, 1200000UL, 2 * VIEW, NULL);
	pf.pf_Piece = 1000;
	CHECK(Prefetch_Step(&pf) == NULL && pf.pf_Read == 1000 && pf.pf_Got == 1000, "a piece of 1000 frames: 1000 read, said");
	pf.pf_Piece = PREFETCH_PIECE;

	Prefetch_Close(&pf);
	Doc_Close(&doc);
	CHECK(dos_host.dh_Vecs == 0 && dos_host.dh_Handles == 0 && dos_host.dh_Locks == 0, "everything given back");
	return CHECK_DONE("test_prefetch");
}
