/*
 * plan_wav.c -- WaveSmith's analysis on the host: a 16-bit PCM WAV read
 * as doc.c reads it (looklevel.c fed the words as the Amiga loads them,
 * each look's plain peak, wavepeaks.c), then trimplan.c; prints what the
 * window would suggest. A check of the whole chain on real samples
 * before the emulator (test/auto/EditTrim); exits 1 when no music is
 * found or In/Out fall outside the ranges given.
 *
 *   plan_wav <file.wav> [in_min in_max out_min out_max]   (seconds)
 */
#include <stdio.h>
#include <stdlib.h>

#include "normwav.h"
#include "looklevel.h"
#include "levelmap.h"
#include "wavepeaks.h"
#include "trimplan.h"

int main(int argc, char **argv)
{
	FILE *f;
	long size;
	unsigned char *b;
	struct NormWav nw;
	static struct LookLevel ll;
	static struct WavePeaks wp;
	static struct TrimPlan tp;
	struct TrimLooks tl;
	UBYTE *looks, *peaks;
	UWORD *w;
	ULONG frames, i, n, pc = 0, pin = 0, pl = 0, pr = 0, k, lmax;
	BYTE *lev[3];
	UWORD c;
	double t_in, t_out;

	if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
		fprintf(stderr, "plan_wav: no file\n");
		return 2;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	b = malloc((size_t) size);
	if (!b || fread(b, 1, (size_t) size, f) != (size_t) size) return 2;
	fclose(f);
	if (NormWav_Parse(b, (ULONG) size, &nw) != NORMWAV_OK) {
		fprintf(stderr, "plan_wav: not a 16-bit PCM WAV\n");
		return 2;
	}
	if (nw.nw_DataBytes == NORMWAV_TOEND || nw.nw_DataBytes > (ULONG) size - nw.nw_DataStart)
		nw.nw_DataBytes = (ULONG) size - nw.nw_DataStart;
	frames = nw.nw_DataBytes / nw.nw_Align;
	n = frames * nw.nw_Channels;
	w = malloc(n * 2);
	/* the Amiga loads a little-endian word as big-endian: the file's
	   first byte in the high half -- as looklevel.c expects */
	for (i = 0; i < n; i++) w[i] = (UWORD) ((b[nw.nw_DataStart + 2 * i] << 8) | b[nw.nw_DataStart + 2 * i + 1]);
	if (!LookLevel_Init(&ll, nw.nw_Rate, nw.nw_Channels)) return 2;
	lmax = frames / ll.ll_Frames + 2;
	looks = calloc(lmax, 1);
	peaks = calloc(lmax, 1);
	ll.ll_Out = looks;
	ll.ll_OutMax = lmax;
	LookLevel_Feed(&ll, w, frames);
	LookLevel_Finish(&ll);
	/* the machine's order (doc.c swaps after looklevel) */
	for (i = 0; i < n; i++) w[i] = (UWORD) ((w[i] >> 8) | (w[i] << 8));
	for (i = 0; i < frames; i++) {
		for (c = 0; c < nw.nw_Channels; c++) {
			long v = (WORD) w[i * nw.nw_Channels + c];
			ULONG a = (ULONG) (v < 0 ? -v : v);

			if (a > 32767) a = 32767;
			if (c == 0 && a > pl) pl = a;
			if (c == 1 && a > pr) pr = a;
		}
		if (++pin == ll.ll_Frames) {
			peaks[pc++] = LevelMap_Byte((pl << 16) | pr, (BOOL) (nw.nw_Channels == 1));
			pin = pl = pr = 0;
		}
	}
	if (pin) peaks[pc++] = LevelMap_Byte((pl << 16) | pr, (BOOL) (nw.nw_Channels == 1));
	/* the picture too: that it takes the whole file */
	WavePeaks_Init(&wp, frames, nw.nw_Channels);
	for (k = 0; k < 3; k++) lev[k] = wp.wp_Level[k] = calloc(WavePeaks_Bytes(&wp, (UWORD) k) + 4, 1);
	WavePeaks_Feed(&wp, (const WORD *) w, frames);
	tl.tl_Level = looks;
	tl.tl_Peak = peaks;
	tl.tl_Count = ll.ll_OutCount < pc ? ll.ll_OutCount : pc;
	tl.tl_LookFrames = ll.ll_Frames;
	tl.tl_Frames = frames;
	tl.tl_Rate = nw.nw_Rate;
	tl.tl_LeadMs = 500;
	tl.tl_TailMs = 2000;
	if (!TrimPlan_Make(&tl, &tp)) {
		printf("no music found (threshold %u dB)\n", tp.tp_Threshold);
		return 1;
	}
	t_in = (double) tp.tp_In / nw.nw_Rate;
	t_out = (double) tp.tp_Out / nw.nw_Rate;
	printf("%lu frames, %lu looks of %lu frames, picture %lu frames\n", (unsigned long) frames,
	       (unsigned long) tl.tl_Count, (unsigned long) ll.ll_Frames, (unsigned long) wp.wp_Done);
	printf("threshold %u dB down; music %.3f .. %.3f s\n", tp.tp_Threshold, (double) tp.tp_MusicStart / nw.nw_Rate,
	       (double) tp.tp_MusicEnd / nw.nw_Rate);
	printf("In %.3f s, Out %.3f s; lead-in clicks %u, run-out clicks %u, period %lu ms; %u regions\n", t_in, t_out,
	       tp.tp_LeadClicks, tp.tp_TailClicks, (unsigned long) tp.tp_PeriodMs, tp.tp_RegionCount);
	(void) lev;
	if (argc >= 6 && (t_in < atof(argv[2]) || t_in > atof(argv[3]) || t_out < atof(argv[4]) || t_out > atof(argv[5]))) {
		printf("FAIL In or Out outside %s..%s / %s..%s\n", argv[2], argv[3], argv[4], argv[5]);
		return 1;
	}
	return 0;
}
