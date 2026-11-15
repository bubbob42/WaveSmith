/*
 * test_normwav.c -- host tests for PrismaTools/normwav.c (Normalise).
 *
 *   cc -DPRISMA_HOST -I PrismaTools test/host/test_normwav.c PrismaTools/normwav.c -lm
 *
 * The headers: the chip's own 16-bit PCM one (datasheet 11.6.4, as in
 * test_riff.c), as fixed and as the encoder starts it ($FFFFFFFF sizes: a
 * take that was never closed), a plain 44-byte stereo one, a LIST chunk of
 * odd size before the data, WAVE_FORMAT_EXTENSIBLE, and what Normalise
 * must refuse: 8-bit, mu-law, no RIFF, the data before the format, no data
 * in reach. The decibels against pow(): every tenth to -99.9 dB within one
 * step of the sample, the peak's tenths back from the amplitude, the
 * TARGET strings. The table: the peak lands on the target exactly, the
 * signs mirror, nothing falls as the input rises, samples go through in
 * the file's byte order.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "normwav.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, what) \
	do { checks++; if (!(cond)) { printf("FAIL: %s (line %d)\n", what, __LINE__); failures++; } } while (0)

/* datasheet 11.6.4: 16-bit mono PCM as fixed at the end (fmt chunk of 20) */
static const UBYTE pcm_fixed[48] = {
	0x52, 0x49, 0x46, 0x46, 0x6c, 0x72, 0x1b, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
	0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x77, 0x01, 0x00,
	0x02, 0x00, 0x10, 0x00, 0x02, 0x00, 0x01, 0x00, 0x64, 0x61, 0x74, 0x61, 0x44, 0x72, 0x1b, 0x00
};

/* datasheet 11.6.4: 8 kHz mono mu-law, as the encoder starts it */
static const UBYTE ulaw_start[48] = {
	0x52, 0x49, 0x46, 0x46, 0xff, 0xff, 0xff, 0xff, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
	0x14, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00, 0x40, 0x1f, 0x00, 0x00, 0x40, 0x1f, 0x00, 0x00,
	0x01, 0x00, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00, 0x64, 0x61, 0x74, 0x61, 0xff, 0xff, 0xff, 0xff
};

static void le16(UBYTE *p, unsigned v)
{
	p[0] = (UBYTE) v;
	p[1] = (UBYTE) (v >> 8);
}

/* a plain 44-byte header: format tag, channels, rate, bits, data size */
static void plain(UBYTE *h, unsigned tag, unsigned ch, ULONG rate, unsigned bits, ULONG data)
{
	memcpy(h, "RIFF", 4);
	NormWav_PutLE32(h + 4, 36 + data);
	memcpy(h + 8, "WAVEfmt ", 8);
	NormWav_PutLE32(h + 16, 16);
	le16(h + 20, tag);
	le16(h + 22, ch);
	NormWav_PutLE32(h + 24, rate);
	NormWav_PutLE32(h + 28, rate * ch * bits / 8);
	le16(h + 32, ch * bits / 8);
	le16(h + 34, bits);
	memcpy(h + 36, "data", 4);
	NormWav_PutLE32(h + 40, data);
}

static void test_parse(void)
{
	struct NormWav nw;
	UBYTE h[256];

	memset(&nw, 0, sizeof(nw));
	CHECK(NormWav_Parse(pcm_fixed, sizeof(pcm_fixed), &nw) == NORMWAV_OK, "the chip's PCM header");
	CHECK(nw.nw_DataStart == 48, "its samples at 48");
	CHECK(nw.nw_DataBytes == 0x1b7244UL, "its data size");
	CHECK(nw.nw_Channels == 1 && nw.nw_Align == 2 && nw.nw_Rate == 48000UL, "mono, 2 bytes a frame, 48 kHz");

	memcpy(h, pcm_fixed, sizeof(pcm_fixed));
	NormWav_PutLE32(h + 4, 0xFFFFFFFFUL);
	NormWav_PutLE32(h + 44, 0xFFFFFFFFUL);
	CHECK(NormWav_Parse(h, sizeof(pcm_fixed), &nw) == NORMWAV_OK, "the chip's header, a take never closed");
	CHECK(nw.nw_DataBytes == NORMWAV_TOEND, "  its samples run to the end");
	NormWav_PutLE32(h + 44, 0);
	CHECK(NormWav_Parse(h, sizeof(pcm_fixed), &nw) == NORMWAV_OK && nw.nw_DataBytes == NORMWAV_TOEND,
	      "a data size of 0: to the end too");

	plain(h, 1, 2, 44100, 16, 1000);
	CHECK(NormWav_Parse(h, 44, &nw) == NORMWAV_OK, "a plain stereo header");
	CHECK(nw.nw_DataStart == 44 && nw.nw_DataBytes == 1000 && nw.nw_Channels == 2 && nw.nw_Align == 4,
	      "  at 44, 1000 bytes, stereo, 4 a frame");

	/* a LIST chunk of 5 bytes (and its pad byte) before the data */
	plain(h, 1, 2, 44100, 16, 1000);
	memcpy(h + 36, "LIST", 4);
	NormWav_PutLE32(h + 40, 5);
	memcpy(h + 44, "INFOx", 5);
	h[49] = 0;
	memcpy(h + 50, "data", 4);
	NormWav_PutLE32(h + 54, 1000);
	CHECK(NormWav_Parse(h, 58, &nw) == NORMWAV_OK && nw.nw_DataStart == 58, "past an odd LIST chunk and its pad");

	/* WAVE_FORMAT_EXTENSIBLE, 16-bit PCM in stereo */
	memset(h, 0, sizeof(h));
	memcpy(h, "RIFF", 4);
	memcpy(h + 8, "WAVEfmt ", 8);
	NormWav_PutLE32(h + 16, 40);
	le16(h + 20, 0xFFFE);
	le16(h + 22, 2);
	NormWav_PutLE32(h + 24, 48000);
	NormWav_PutLE32(h + 28, 192000);
	le16(h + 32, 4);
	le16(h + 34, 16);
	le16(h + 36, 22);
	le16(h + 38, 16);
	NormWav_PutLE32(h + 40, 3);
	memcpy(h + 44, "\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71", 16);
	memcpy(h + 60, "data", 4);
	NormWav_PutLE32(h + 64, 400);
	CHECK(NormWav_Parse(h, 68, &nw) == NORMWAV_OK && nw.nw_DataStart == 68 && nw.nw_Channels == 2,
	      "WAVE_FORMAT_EXTENSIBLE with PCM");
	h[44] = 3;				/* IEEE float */
	CHECK(NormWav_Parse(h, 68, &nw) == NORMWAV_NOTPCM16, "  with float: refused");

	plain(h, 1, 1, 22050, 8, 100);
	CHECK(NormWav_Parse(h, 44, &nw) == NORMWAV_NOTPCM16, "8-bit: refused");
	plain(h, 1, 2, 44100, 16, 100);
	le16(h + 32, 2);			/* a block that is not 2 x channels */
	CHECK(NormWav_Parse(h, 44, &nw) == NORMWAV_NOTPCM16, "a block of the wrong size: refused");
	CHECK(NormWav_Parse(ulaw_start, sizeof(ulaw_start), &nw) == NORMWAV_NOTPCM16, "mu-law: refused");

	plain(h, 1, 2, 44100, 16, 100);
	memcpy(h, "FORM", 4);
	CHECK(NormWav_Parse(h, 44, &nw) == NORMWAV_NOTWAV, "no RIFF: not a WAV");
	CHECK(NormWav_Parse(h, 8, &nw) == NORMWAV_NOTWAV, "8 bytes: not a WAV");

	plain(h, 1, 2, 44100, 16, 100);
	memcpy(h + 12, "data", 4);		/* the data first, then a "fmt " */
	memcpy(h + 36, "fmt ", 4);
	CHECK(NormWav_Parse(h, 44, &nw) != NORMWAV_OK, "the data before the format: refused");

	plain(h, 1, 2, 44100, 16, 100);
	CHECK(NormWav_Parse(h, 40, &nw) == NORMWAV_NODATA, "the data chunk cut off: no data");
	memcpy(h + 36, "LIST", 4);
	NormWav_PutLE32(h + 40, 100000);	/* a chunk beyond the bytes given */
	CHECK(NormWav_Parse(h, 44, &nw) == NORMWAV_NODATA, "a chunk past the bytes given: no data");
}

static void test_fix(void)
{
	struct NormWav nw;
	UBYTE h[48];

	memcpy(h, pcm_fixed, sizeof(h));
	NormWav_PutLE32(h + 4, 0xFFFFFFFFUL);
	NormWav_PutLE32(h + 44, 0xFFFFFFFFUL);
	NormWav_Parse(h, sizeof(h), &nw);
	NormWav_FixSizes(h, &nw, 1000);
	CHECK(NormWav_LE32(h + 44) == 1000 && NormWav_LE32(h + 4) == 48 + 1000 - 8, "sizes fixed for 1000 bytes");
	NormWav_FixSizes(h, &nw, 1001);
	CHECK(NormWav_LE32(h + 44) == 1001 && NormWav_LE32(h + 4) == 48 + 1002 - 8, "an odd size: the pad counted");
	NormWav_FixSizes(h, &nw, 0x1b7244UL);
	CHECK(memcmp(h, pcm_fixed, sizeof(h)) == 0, "the chip's own fixed header back");
}

static void test_decibels(void)
{
	UWORD t, bad = 0, back = 0;
	double exact;
	LONG d;

	CHECK(NormWav_Amplitude(0) == 32767, "0 dB: 32767");
	CHECK(NormWav_Amplitude(10) == 29204, "-1 dB: 29204");
	CHECK(NormWav_Amplitude(60) == 16422, "-6 dB: 16422");
	for (t = 0; t <= NORMWAV_MAXTENTHS; t++) {
		exact = 32767.0 * pow(10.0, -(double) t / 200.0);
		d = (LONG) NormWav_Amplitude(t) - (LONG) floor(exact + 0.5);
		if (d < -1 || d > 1) bad++;
	}
	CHECK(bad == 0, "every tenth to -99.9 dB within one step of the sample");
	for (t = 0; t <= 400; t++)
		if (NormWav_PeakTenths(NormWav_Amplitude(t)) != t) back++;
	CHECK(back == 0, "the tenths back from the amplitude, to -40 dB");
	CHECK(NormWav_PeakTenths(32768) == 0 && NormWav_PeakTenths(32767) == 0, "full scale: 0");
	CHECK(NormWav_PeakTenths(0) == NORMWAV_MAXTENTHS, "silence: the deepest");
	CHECK(NormWav_PeakTenths(1) == 903, "one step: -90.3 dB");
	CHECK(NormWav_PeakTenths(100) == 503, "100: -50.3 dB");
	CHECK(NormWav_PeakTenths(3276) == 200, "3276: -20.0 dB, below the step to x10");
	CHECK(NormWav_PeakTenths(16500) == 60, "16500: -6.0 dB (-5.96)");
}

static void test_targets(void)
{
	UWORD t = 1234;

	CHECK(NormWav_ParseTenths("-1", &t) && t == 10, "-1");
	CHECK(NormWav_ParseTenths("-1.5", &t) && t == 15, "-1.5");
	CHECK(NormWav_ParseTenths("-0.1", &t) && t == 1, "-0.1");
	CHECK(NormWav_ParseTenths("-.5", &t) && t == 5, "-.5");
	CHECK(NormWav_ParseTenths("0", &t) && t == 0, "0");
	CHECK(NormWav_ParseTenths("+0.0", &t) && t == 0, "+0.0");
	CHECK(NormWav_ParseTenths("-99.9", &t) && t == 999, "-99.9");
	t = 77;
	CHECK(!NormWav_ParseTenths("1", &t) && t == 77, "1: above full scale, refused, nothing written");
	CHECK(!NormWav_ParseTenths("+0.5", &t), "+0.5: refused");
	CHECK(!NormWav_ParseTenths("-1.55", &t), "two decimals: refused");
	CHECK(!NormWav_ParseTenths("-100", &t), "-100: too deep");
	CHECK(!NormWav_ParseTenths("", &t), "nothing: refused");
	CHECK(!NormWav_ParseTenths("-", &t), "a sign alone: refused");
	CHECK(!NormWav_ParseTenths("-1dB", &t), "a unit: refused");
	CHECK(!NormWav_ParseTenths("abc", &t), "a word: refused");
}

/* a sample as the Amiga loads it: the file's first byte (the low one) high */
static UWORD word_of(int s)
{
	unsigned u = (unsigned) s & 0xFFFFU;

	return (UWORD) (((u & 0xFF) << 8) | (u >> 8));
}

static int sample_of(UWORD w)
{
	int s = ((w & 0xFF) << 8) | (w >> 8);

	return s >= 32768 ? s - 65536 : s;
}

static UWORD table[65536];

static void test_table(void)
{
	UWORD w[6];
	UBYTE bytes[4];
	int s, prev, bad_sign = 0, bad_order = 0, bad_same = 0;
	ULONG target = NormWav_Amplitude(10), peak = 16422;

	w[0] = word_of(0);
	w[1] = word_of(1000);
	w[2] = word_of(-32768);
	w[3] = word_of(32767);
	w[4] = word_of(-5);
	CHECK(NormWav_Peak(w, 5, 0) == 32768, "the peak of -32768: 32768");
	CHECK(NormWav_Peak(w, 2, 0) == 1000, "the peak of 0 and 1000");
	CHECK(NormWav_Peak(w, 2, 2000) == 2000, "a peak carried on");
	w[0] = word_of(-1);
	CHECK(NormWav_Peak(w, 1, 0) == 1, "-1: 1");

	NormWav_Table(table, target, peak);
	CHECK(sample_of(table[word_of(16422)]) == 29204, "the peak becomes the target");
	CHECK(sample_of(table[word_of(-16422)]) == -29204, "  and its negative the negative");
	CHECK(sample_of(table[word_of(0)]) == 0, "0 stays 0");
	CHECK(sample_of(table[word_of(1)]) == 2 && sample_of(table[word_of(-1)]) == -2, "1 x 1.778: 2, rounded");
	prev = -40000;
	for (s = -32767; s <= 32767; s++) {
		int o = sample_of(table[word_of(s)]);
		if (sample_of(table[word_of(-s)]) != -o) bad_sign++;
		if (o < prev) bad_order++;
		prev = o;
	}
	CHECK(bad_sign == 0, "the signs mirror");
	CHECK(bad_order == 0, "nothing falls as the input rises");
	CHECK(sample_of(table[word_of(32767)]) == 32767, "beyond the peak: held at full scale");

	NormWav_Table(table, 32767, 32768);
	CHECK(sample_of(table[word_of(-32768)]) == -32767, "-32768 to a full-scale target: -32767");
	NormWav_Table(table, 20000, 20000);
	for (s = -20000; s <= 20000; s++)
		if (sample_of(table[word_of(s)]) != s) bad_same++;
	CHECK(bad_same == 0, "a peak at the target: every sample as it was");

	/* through the table in the file's byte order: 1000 and -1000, gain 2 */
	NormWav_Table(table, 20000, 10000);
	bytes[0] = 0xE8; bytes[1] = 0x03;	/* 1000 */
	bytes[2] = 0x18; bytes[3] = 0xFC;	/* -1000 */
	w[0] = (UWORD) ((bytes[0] << 8) | bytes[1]);	/* as the Amiga loads them */
	w[1] = (UWORD) ((bytes[2] << 8) | bytes[3]);
	NormWav_Apply(w, 2, table);
	CHECK((w[0] >> 8) == 0xD0 && (w[0] & 0xFF) == 0x07, "1000 x 2: 2000 = $07D0, stored D0 07");
	CHECK((w[1] >> 8) == 0x30 && (w[1] & 0xFF) == 0xF8, "-1000 x 2: -2000 = $F830, stored 30 F8");
}

int main(void)
{
	alarm(20);
	CHECK(sizeof(ULONG) == 4 && sizeof(UWORD) == 2, "ULONG 32 bits, UWORD 16, as on the Amiga");
	test_parse();
	test_fix();
	test_decibels();
	test_targets();
	test_table();
	if (failures) {
		printf("test_normwav: %d of %d checks failed\n", failures, checks);
		return 1;
	}
	printf("test_normwav: all %d checks passed\n", checks);
	return 0;
}
