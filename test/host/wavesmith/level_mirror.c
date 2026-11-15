/*
 * level_mirror.c -- the level's sums in C against their copy in Python
 * (tools/make_edit_single.py, which checks WaveSmith's saves from the
 * Mac): random timelines -- cuts, silences, gains, fades, end fades,
 * dips -- each read through Tl_Read in odd pieces into
 * <drawer>/mirror-<n>.raw, its pieces and levels described in
 * <drawer>/mirror.txt as WaveSmith's trace writes them, with the exact
 * level at some frames ("levelat t level"). tools/check_level_mirror.py
 * renders each the Python way and compares, byte for byte.
 *
 *   level_mirror <drawer>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/dos.h>

#include "doshost.h"
#include "timeline.h"

#define FRAMES	20000UL
#define CASES	60

static UBYTE file[FRAMES * 4];
static UBYTE out[FRAMES * 4];

int main(int argc, char **argv)
{
	static struct Timeline tl;
	struct TlPart pt;
	struct TlLevels lv;
	struct TlRead rd;
	char path[512];
	FILE *f, *txt;
	BPTR fh;
	ULONG i, at, step, a, b, len;
	UWORD k, n, op;

	if (argc < 2) return 2;
	srand(3263);
	for (i = 0; i < FRAMES * 4; i += 2) {
		/* loud and quiet stretches: some clip at a gain, some do not */
		long v = (long) ((rand() % 65536) - 32768) / ((i / 4000) % 3 + 1);

		file[i] = (UBYTE) (v & 255);
		file[i + 1] = (UBYTE) ((v >> 8) & 255);
	}
	snprintf(path, sizeof(path), "%s/mirror.src", argv[1]);
	if (!(f = fopen(path, "wb"))) return 1;
	fwrite(file, 1, sizeof(file), f);
	fclose(f);
	snprintf(path, sizeof(path), "%s/mirror.txt", argv[1]);
	if (!(txt = fopen(path, "w"))) return 1;
	snprintf(path, sizeof(path), "%s/mirror.src", argv[1]);
	fh = Open((STRPTR) path, MODE_OLDFILE);
	for (k = 0; k < CASES; k++) {
		Tl_Init(&tl, FRAMES);
		n = (UWORD) (rand() % 8 + 1);
		while (n--) {
			len = tl.tl_Frames;
			a = (ULONG) rand() % len;
			b = a + 1 + (ULONG) rand() % (len - a > 6000 ? 6000 : len - a);
			if (b > len) b = len;
			pt.tp_First = a;
			pt.tp_End = b;
			pt.tp_Pad = 0;
			op = (UWORD) (rand() % 6);
			if (op == 0 && b - a < len) Tl_Cut(&tl, a, b);
			else if (op == 1) Tl_Silence(&tl, a, b);
			else if (op == 2) {
				pt.tp_Gain = Env_FromTenths((WORD) (rand() % 421 - 240));
				Tl_Gain(&tl, &pt);
			} else if (op == 3 || op == 4) {
				pt.tp_Fade = (UBYTE) ((op == 3 ? TL_FADE_IN : TL_FADE_OUT) | (rand() % ENV_SHAPES));
				Tl_Fade(&tl, &pt);
			} else {
				Tl_Plain(&tl, &pt);
			}
		}
		len = tl.tl_Frames;
		lv.lv_In = (ULONG) rand() % (len / 4 + 1);
		lv.lv_Out = len - (ULONG) rand() % (len / 4 + 1);
		lv.lv_FadeIn = rand() % 2 ? (ULONG) rand() % 3000 : 0;
		lv.lv_FadeOut = rand() % 2 ? (ULONG) rand() % 3000 : 0;
		lv.lv_Dip = rand() % 2 ? (ULONG) rand() % 300 : 0;
		lv.lv_Shape = (UWORD) (rand() % ENV_SHAPES);
		lv.lv_Pad = 0;
		fprintf(txt, "case %u frames %lu\n", k, (unsigned long) len);
		fprintf(txt, "saveplan pieces frames %u %lu\n", tl.tl_Count, (unsigned long) len);
		for (i = 0; i < tl.tl_Count; i++) {
			const struct Piece *p = &tl.tl_Pieces[i];

			fprintf(txt, "piece from frames gain fade u0 u1 %lu %lu %u %u %u %u\n", (unsigned long) p->p_From,
				(unsigned long) p->p_Frames, p->p_Gain, p->p_Fade, p->p_U0, p->p_U1);
		}
		fprintf(txt, "savelevels in out fadein fadeout dip shape %lu %lu %lu %lu %lu %u\n", (unsigned long) lv.lv_In,
			(unsigned long) lv.lv_Out, (unsigned long) lv.lv_FadeIn, (unsigned long) lv.lv_FadeOut,
			(unsigned long) lv.lv_Dip, lv.lv_Shape);
		for (i = 0; i < 64; i++) {
			ULONG t = (ULONG) rand() % len;

			fprintf(txt, "levelat %lu %u\n", (unsigned long) t, Tl_LevelAt(&tl, Tl_Levels(&lv), t));
		}
		/* read in odd pieces, as the card, AHI or a save might */
		memset(out, 0x5A, sizeof(out));
		Tl_ReadInit(&rd);
		rd.rd_FH = fh;
		rd.rd_Start = 0;
		rd.rd_Align = 4;
		rd.rd_Levels = Tl_Levels(&lv);
		for (at = 0, step = 333; at < len; at += rd.rd_Frames) {
			rd.rd_First = at;
			rd.rd_Frames = step < len - at ? step : len - at;
			rd.rd_Buf = out + at * 4;
			if (Tl_Read(&tl, &rd) != rd.rd_Frames) {
				fprintf(stderr, "level_mirror: a short read\n");
				return 1;
			}
			step = step * 7 % 4093 + 17;
		}
		snprintf(path, sizeof(path), "%s/mirror-%u.raw", argv[1], k);
		if (!(f = fopen(path, "wb"))) return 1;
		fwrite(out, 1, len * 4, f);
		fclose(f);
	}
	Close(fh);
	fclose(txt);
	printf("level_mirror: %d cases written\n", CASES);
	return 0;
}
