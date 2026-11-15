/*
 * test_looklevel.c -- looklevel.c: a record's noise must not pass for
 * music, music must. At 44.1 kHz (1764 frames a look, pieces of 220):
 *
 *   groove noise at amplitude 30 (-61 dB)          silence (55 dB or down)
 *     + a click every 20 ms (a spike of three       still silence; a plain
 *       samples, 20000 -12000 5000: what a preamp   peak would read -4 dB
 *       makes of a speck in the groove)
 *     + rumble: 8 Hz at 1000 (-30 dB)               still silence
 *     + a DC offset of 2000 (-24 dB)                still silence
 *   a 1 kHz tone at 8192 (-12 dB)                  12 dB
 *   a bass line, 60 Hz at 8192                     louder than -20 dB
 *   noise left, the tone right                     the louder side: 12
 *   nothing at all                                 255
 *
 * Fed a frame at a time it gives the bytes it gives fed all at once; a
 * look begun goes out at the end; Init refuses what it cannot take.
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "looklevel.h"
#include "levelmap.h"
#include "level.h"

#define RATE	44100UL
#define SECONDS	2UL
#define FRAMES	(RATE * SECONDS)

static UWORD buf[FRAMES * 2];
static UBYTE out[200], out2[200];
static unsigned long seed;

static UWORD word_of(long s)
{
	unsigned u;

	if (s > 32767) s = 32767;
	if (s < -32768) s = -32768;
	u = (unsigned) s & 0xFFFFU;
	return (UWORD) (((u & 0xFF) << 8) | (u >> 8));
}

static long noise(long amp)
{
	seed = seed * 1103515245UL + 12345UL;
	return (long) ((seed >> 16) % (unsigned long) (2 * amp + 1)) - amp;
}

/* FRAMES frames, one channel: noise, a click every click_ms (0: none),
 * rumble, a DC offset, a tone (hz, amp) */
static void make(long namp, unsigned click_ms, long rumble, long dc, double hz, long amp)
{
	unsigned long i, period = click_ms ? RATE * click_ms / 1000UL : 0;

	seed = 1;
	for (i = 0; i < FRAMES; i++) {
		long s = noise(namp) + dc;
		if (period && i % period < 3) s += i % period == 0 ? 20000 : i % period == 1 ? -12000 : 5000;
		if (rumble) s += (long) (rumble * sin(2 * M_PI * 8.0 * i / RATE));
		if (amp) s += (long) (amp * sin(2 * M_PI * hz * i / RATE));
		buf[i] = word_of(s);
	}
}

/* the looks of buf (channels 1), into o; their count */
static ULONG looks(UBYTE *o, int one_by_one, UWORD channels)
{
	struct LookLevel ll;
	ULONG i;

	CHECK(LookLevel_Init(&ll, RATE, channels), "Init at 44.1 kHz");
	ll.ll_Out = o;
	ll.ll_OutMax = 200;
	if (one_by_one)
		for (i = 0; i < FRAMES; i++) LookLevel_Feed(&ll, buf + i * channels, 1);
	else
		LookLevel_Feed(&ll, buf, FRAMES);
	LookLevel_Finish(&ll);
	return ll.ll_OutCount;
}

/* the quietest and the loudest of n looks */
static void range(const UBYTE *o, ULONG n, int *quiet, int *loudest)
{
	ULONG i;

	*quiet = 0;
	*loudest = 255;
	for (i = 0; i < n; i++) {
		if (o[i] > *quiet) *quiet = o[i];
		if (o[i] < *loudest) *loudest = o[i];
	}
}

/* the plain peak of the first look, as the chip's map would have it */
static int plain_first(void)
{
	ULONG i, peak = 0;

	for (i = 0; i < 1764; i++) {
		long s = (short) (((buf[i] & 0xFF) << 8) | (buf[i] >> 8));
		if (s < 0) s = -s;
		if ((ULONG) s > peak) peak = (ULONG) s;
	}
	return LevelMap_Byte(peak << 16, TRUE);
}

static void test_noise(void)
{
	ULONG n;
	int q, l;

	make(30, 0, 0, 0, 0, 0);
	n = looks(out, 0, 1);
	CHECK(n == 50, "2 s: 50 looks");
	range(out, n, &q, &l);
	CHECK(l >= 55, "groove noise (-61 dB): silence");

	make(30, 20, 0, 0, 0, 0);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(l >= 55, "a click every 20 ms: still silence, as the noise alone");
	CHECK(plain_first() <= 6, "  (a plain peak reads the clicks: -4 dB)");

	make(30, 0, 1000, 0, 0, 0);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(l >= 48, "rumble at 8 Hz, -30 dB: silence");
	CHECK(plain_first() <= 31, "  (a plain peak: -30 dB)");

	make(30, 0, 0, 2000, 0, 0);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(l >= 55, "a DC offset of -24 dB: silence, from the first look");

	memset(buf, 0, sizeof(buf));
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(l == LEVELMAP_NONE, "nothing at all: 255");
}

static void test_music(void)
{
	ULONG n, i;
	int q, l;

	make(0, 0, 0, 0, 1000.0, 8192);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(l >= 12 && q <= 13, "a 1 kHz tone at -12 dB: 12 dB (13 at most)");

	make(0, 0, 0, 0, 60.0, 8192);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(q <= 20, "a bass line at 60 Hz, -12 dB: louder than -20 dB");

	make(30, 20, 0, 0, 1000.0, 8192);
	n = looks(out, 0, 1);
	range(out, n, &q, &l);
	CHECK(q <= 13, "the tone with the noise and the clicks: music");

	/* stereo: noise left, the tone right */
	make(0, 0, 0, 0, 1000.0, 8192);
	for (i = FRAMES; i-- > 0;) {
		buf[i * 2 + 1] = buf[i];
		buf[i * 2] = word_of(noise(30));
	}
	n = looks(out, 0, 2);
	range(out, n, &q, &l);
	CHECK(n == 50 && q <= 13, "stereo: the louder side");
}

static void test_feeding(void)
{
	struct LookLevel ll;
	ULONG n1, n2;

	make(30, 20, 1000, 500, 1000.0, 2000);
	n1 = looks(out, 0, 1);
	n2 = looks(out2, 1, 1);
	CHECK(n1 == n2 && memcmp(out, out2, n1) == 0, "a frame at a time: the same bytes");

	/* a look and a half: the half goes out at the end */
	make(0, 0, 0, 0, 1000.0, 8192);
	LookLevel_Init(&ll, RATE, 1);
	ll.ll_Out = out;
	ll.ll_OutMax = 200;
	LookLevel_Feed(&ll, buf, 1764 + 882);
	CHECK(ll.ll_OutCount == 1, "a look and a half: one look so far");
	LookLevel_Finish(&ll);
	CHECK(ll.ll_OutCount == 2 && out[1] >= 12 && out[1] <= 13, "  the half at the end, measured from its pieces");
	LookLevel_Finish(&ll);
	CHECK(ll.ll_OutCount == 2, "  Finish twice: nothing more");

	LookLevel_Init(&ll, RATE, 1);
	ll.ll_Out = out;
	ll.ll_OutMax = 3;
	LookLevel_Feed(&ll, buf, FRAMES);
	CHECK(ll.ll_OutCount == 3, "OutMax: no more than that");
}

static void test_init(void)
{
	struct LookLevel ll;

	CHECK(!LookLevel_Init(&ll, 0, 1) && !LookLevel_Init(&ll, 192000, 1), "rates it cannot take");
	CHECK(!LookLevel_Init(&ll, RATE, 0) && !LookLevel_Init(&ll, RATE, 9), "channels it cannot take");
	CHECK(LookLevel_Init(&ll, 44100, 2) && LookLevel_Micros(&ll, 44100) == 40000, "44.1 kHz: 40 ms looks");
	CHECK(LookLevel_Init(&ll, 48000, 2) && LookLevel_Micros(&ll, 48000) == 40000, "48 kHz: 40 ms");
	CHECK(LookLevel_Init(&ll, 22050, 1) && LookLevel_Micros(&ll, 22050) == 40000, "22.05 kHz: 40 ms");
	CHECK(LookLevel_Init(&ll, 32000, 1) && ll.ll_Pole > 30000 && ll.ll_Pole < 32767, "the pole below 1");
}

int main(void)
{
	test_noise();
	test_music();
	test_feeding();
	test_init();
	return CHECK_DONE("test_looklevel");
}
