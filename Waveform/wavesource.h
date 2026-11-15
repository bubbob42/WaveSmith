#ifndef WAVEFORM_WAVESOURCE_H
#define WAVEFORM_WAVESOURCE_H

/*
 * wavesource.h -- ready WaveSources for waveform.gadget: a WavePeaks
 * picture (wavepeaks.h), and a PrismaRec level map's looks (levelmap.h: a
 * byte a look, dB below full scale) drawn as a level envelope. Each hook
 * answers from RAM at once, as the class needs (gadgets/waveform.h).
 */

#include <exec/types.h>
#include <gadgets/waveform.h>
#include "wavepeaks.h"

/* ws from the picture wp, rate frames a second; wp borrowed */
void WaveSource_Peaks(struct WaveSource *ws, struct WavePeaks *wp);

/* the looks of a .prl, as the class gets them from a source */
struct WaveLooks {
	const UBYTE *wl_Looks;		/* a byte a look, LEVELMAP_NONE for nothing */
	ULONG wl_Count;
	ULONG wl_LookFrames;		/* frames a look (40 ms at the rate) */
};
/* ws from the looks wl (borrowed): frames = looks x look frames, mono, a
   level envelope (WAVESOURCE_LEVELONLY) */
void WaveSource_Looks(struct WaveSource *ws, struct WaveLooks *wl);

#endif
