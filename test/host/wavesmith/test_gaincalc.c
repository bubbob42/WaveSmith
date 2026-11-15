/*
 * test_gaincalc.c -- gaincalc.c: tenths as text and back, every one; a
 * peak in tenths of full scale (at it, below, past it, nothing); and
 * Normalise against what it must do -- for 3000 random peaks and levels
 * the gain it sets brings the peak to the level and not past it, a tenth
 * more would pass it (where the range allows), and it stays in the range.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "gaincalc.h"
#include "envelope.h"
#include "normwav.h"

int main(void)
{
	char buf[16];
	WORD t, back;
	int bad = 0, loose = 0, k;
	ULONG peak, amp;
	WORD target, g;

	for (t = ENV_TENTHS_MIN; t <= ENV_TENTHS_MAX; t++) {
		*Gain_PutTenths((STRPTR) buf, t) = '\0';
		if (!Gain_ParseTenths((CONST_STRPTR) buf, &back) || back != t) bad++;
	}
	CHECK(!bad, "every tenth written and read back as itself");
	*Gain_PutTenths((STRPTR) buf, 46) = '\0';
	CHECK(!strcmp(buf, "+4.6"), "+4.6");
	*Gain_PutTenths((STRPTR) buf, -5) = '\0';
	CHECK(!strcmp(buf, "-0.5"), "-0.5");
	*Gain_PutTenths((STRPTR) buf, 0) = '\0';
	CHECK(!strcmp(buf, "0.0"), "0.0");
	CHECK(Gain_ParseTenths((CONST_STRPTR) " 3 ", &back) && back == 30, "3: three dB");
	CHECK(Gain_ParseTenths((CONST_STRPTR) "-1,5", &back) && back == -15, "-1,5: a comma for the point");
	back = 77;
	CHECK(!Gain_ParseTenths((CONST_STRPTR) "", &back) && !Gain_ParseTenths((CONST_STRPTR) "abc", &back) &&
	      !Gain_ParseTenths((CONST_STRPTR) "1.2.3", &back) && !Gain_ParseTenths((CONST_STRPTR) "+", &back) && back == 77,
	      "not a number: refused, nothing changed");

	CHECK(Gain_PeakTenths(32767) == 0 && Gain_PeakTenths(0) == GAIN_SILENT, "full scale 0.0, nothing silent");
	CHECK(Gain_PeakTenths(16384) == -60, "half of full scale: -6.0");
	CHECK(Gain_PeakTenths(65534) == 60 && Gain_PeakTenths(32768) == 0, "twice full scale: +6.0");

	srand(1063);
	bad = 0;
	for (k = 0; k < 3000; k++) {
		peak = 1 + (ULONG) rand() % 32768;
		if (k % 7 == 0) peak = 1 + (ULONG) rand() % 300;	/* quiet: the range ends at +18 */
		target = (WORD) -(rand() % 120);
		g = Gain_Normalise(peak, target);
		amp = NormWav_Amplitude((UWORD) -target);
		if (g < ENV_TENTHS_MIN || g > ENV_TENTHS_MAX) bad++;
		else if (Gain_Peak(peak, g) > amp) bad++;
		else if (g < ENV_TENTHS_MAX && Gain_Peak(peak, (WORD) (g + 1)) <= amp) loose++;
	}
	CHECK(!bad, "Normalise: in the range, the peak never past the level");
	CHECK(!loose, "Normalise: a tenth more would pass the level (as near as a tenth gets)");
	CHECK(Gain_Normalise(0, -10) == 0, "a silent part: no gain");
	CHECK(Gain_Normalise(1, -10) == ENV_TENTHS_MAX, "a whisper: the most there is, +18 dB");

	return CHECK_DONE("test_gaincalc");
}
