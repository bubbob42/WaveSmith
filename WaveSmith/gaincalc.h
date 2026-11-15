/*
 * gaincalc.h -- the Gain window's sums (concept 3.2): a gain's tenths of a
 * dB as text and back, a peak in tenths of full scale, the gain Normalise
 * sets. Pure: host-tested (test/host/wavesmith/test_gaincalc.c).
 */
#ifndef WAVESMITH_GAINCALC_H
#define WAVESMITH_GAINCALC_H

#include <exec/types.h>

/* tenths as "+4.6", "-9.5" or "0.0" into buf (7 bytes and its 0 at most):
   its end */
STRPTR Gain_PutTenths(STRPTR buf, WORD tenths);

/* "4.6", "+4.6", "-9.5", "-0.5", "3" (a sign, digits, a point and one
   digit at most) into *tenths: FALSE, *tenths untouched, for anything
   else -- the range is the caller's */
BOOL Gain_ParseTenths(CONST_STRPTR s, WORD *tenths);

/* a peak (0 .. 32768) in tenths of a dB of full scale: 0 at 32767, below
   it negative, past it (a gain's) positive; GAIN_SILENT for 0 */
#define GAIN_SILENT	(-32000)
WORD Gain_PeakTenths(ULONG peak);

/* peak at the gain (tenths), as Env_Apply rounds a sample; past 32767
   when it clips */
ULONG Gain_Peak(ULONG peak, WORD tenths);

/* the gain that brings peak to target (tenths, <= 0) without passing it,
   in the gain's range (ENV_TENTHS_MIN .. _MAX): what Normalise sets;
   0 for a silent part */
WORD Gain_Normalise(ULONG peak, WORD target);

#endif
