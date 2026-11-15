/*
 * gaincalc.c -- the Gain window's sums (gaincalc.h)
 */

#include <exec/types.h>

#include "gaincalc.h"
#include "envelope.h"
#include "normwav.h"
#include "textutil.h"

STRPTR Gain_PutTenths(STRPTR buf, WORD tenths)
{
	STRPTR p = buf;
	ULONG t = (ULONG) (tenths < 0 ? -(LONG) tenths : tenths);

	if (tenths > 0) *p++ = '+';
	if (tenths < 0) *p++ = '-';
	p = Text_PutNumber(p, t / 10);
	*p++ = '.';
	return Text_PutNumber(p, t % 10);
}

BOOL Gain_ParseTenths(CONST_STRPTR s, WORD *tenths)
{
	LONG v = 0, sign = 1;
	UWORD digits = 0;

	while (*s == ' ') s++;
	if (*s == '+' || *s == '-') {
		if (*s == '-') sign = -1;
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		v = v * 10 + (*s++ - '0');
		if (v > 3000) return FALSE;
		digits++;
	}
	v *= 10;
	if (*s == '.' || *s == ',') {
		s++;
		if (*s >= '0' && *s <= '9') {
			v += *s++ - '0';
			digits++;
		}
	}
	while (*s == ' ') s++;
	if (*s || !digits) return FALSE;
	*tenths = (WORD) (sign * v);
	return TRUE;
}

WORD Gain_PeakTenths(ULONG peak)
{
	ULONG over;

	if (!peak) return GAIN_SILENT;
	if (peak <= 32767UL) return (WORD) -(LONG) NormWav_PeakTenths(peak);
	/* past full scale: its ratio to it, as a level */
	over = peak * ENV_UNITY / 32767UL;
	return Env_Tenths((UWORD) (over > ENV_MAX ? ENV_MAX : over));
}

ULONG Gain_Peak(ULONG peak, WORD tenths)
{
	return (peak * Env_FromTenths(tenths) + 2048UL) >> 12;
}

WORD Gain_Normalise(ULONG peak, WORD target)
{
	LONG t;
	ULONG amp;

	if (!peak) return 0;
	if (target > 0) target = 0;
	amp = NormWav_Amplitude((UWORD) -target);
	t = (LONG) target - (LONG) Gain_PeakTenths(peak);
	if (t > ENV_TENTHS_MAX) t = ENV_TENTHS_MAX;
	if (t < ENV_TENTHS_MIN) t = ENV_TENTHS_MIN;
	/* never past the level: a tenth down while the rounding would pass it */
	while (t > ENV_TENTHS_MIN && Gain_Peak(peak, (WORD) t) > amp) t--;
	return (WORD) t;
}
