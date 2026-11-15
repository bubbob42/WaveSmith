/*
 * wavesource.c -- ready WaveSources (wavesource.h): the hooks, register
 * entries for CallHookPkt (A0 the hook, A2 the object, A1 the message)
 */

#include <utility/hooks.h>

#include "wavesource.h"
#include "boopsi.h"

#define LOOK_NONE	255		/* levelmap.h's LEVELMAP_NONE */

static DISP_QUAL ULONG peaks_hook(REG(a0, struct Hook *h), REG(a2, Object *o), REG(a1, struct WavePeakMsg *q))
{
	(void) o;
	return WavePeaks_Query((const struct WavePeaks *) h->h_Data, q);
}

void WaveSource_Peaks(struct WaveSource *ws, struct WavePeaks *wp)
{
	ws->ws_Peaks.h_Entry = (ULONG (*)()) peaks_hook;
	ws->ws_Peaks.h_SubEntry = NULL;
	ws->ws_Peaks.h_Data = wp;
	ws->ws_Frames = wp->wp_Frames;
	ws->ws_Channels = wp->wp_Channels;
	ws->ws_Flags = 0;
	ws->ws_Finest = 1UL << WAVEPEAKS_SHIFT0;
	/* ws_Rate: the caller's (the picture knows no time) */
}

/* a look's byte (dB down, 0..90) as a peak's top byte, 0..127 */
static BYTE look_peak(UBYTE b)
{
	LONG v;

	if (b == LOOK_NONE || b >= 90) return 0;
	/* linear in dB over the 42 dB a top byte can show: crude, but the
	   take list's bars only rank loud and quiet */
	v = 127L - (LONG) b * 127L / 42L;
	return (BYTE) (v < 0 ? 0 : v);
}

static DISP_QUAL ULONG looks_hook(REG(a0, struct Hook *h), REG(a2, Object *o), REG(a1, struct WavePeakMsg *q))
{
	const struct WaveLooks *wl = (const struct WaveLooks *) h->h_Data;
	ULONG i, a, b, k, frames = wl->wl_Count * wl->wl_LookFrames;
	BYTE pk, best;

	(void) o;
	if (!wl->wl_LookFrames || !q->wpm_Step) return 0;
	for (i = 0; i < q->wpm_Count; i++) {
		if (i > (0xFFFFFFFFUL - q->wpm_First) / q->wpm_Step) break;
		a = q->wpm_First + i * q->wpm_Step;
		best = 0;
		if (a < frames) {
			b = q->wpm_Step > frames - a ? frames : a + q->wpm_Step;
			for (k = a / wl->wl_LookFrames; k * wl->wl_LookFrames < b && k < wl->wl_Count; k++) {
				pk = look_peak(wl->wl_Looks[k]);
				if (pk > best) best = pk;
			}
		}
		q->wpm_Out[i * 2] = (BYTE) -best;
		q->wpm_Out[i * 2 + 1] = best;
	}
	return i;
}

void WaveSource_Looks(struct WaveSource *ws, struct WaveLooks *wl)
{
	ws->ws_Peaks.h_Entry = (ULONG (*)()) looks_hook;
	ws->ws_Peaks.h_SubEntry = NULL;
	ws->ws_Peaks.h_Data = wl;
	ws->ws_Frames = wl->wl_Count * wl->wl_LookFrames;
	ws->ws_Channels = 1;
	ws->ws_Flags = WAVESOURCE_LEVELONLY;
	ws->ws_Finest = 1;
}
