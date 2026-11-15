/*
 * normwav.c -- see normwav.h
 */
#include "normwav.h"

/* 10^(-d/20) for whole decibels d, Q16 (65536 = 1), and 10^(-i/200) for
 * the tenths i, Q15: the level t tenths down is whole[t / 10] x
 * tenth[t % 10] (the values: round(65536 x 10^(-d/20)), round(32768 x
 * 10^(-i/200))) */
static const ULONG whole_q16[100] = {
	65536UL, 58409, 52057, 46396, 41350, 36854, 32846, 29274, 26090, 23253,
	20724, 18471, 16462, 14672, 13076, 11654, 10387, 9257, 8250, 7353,
	6554, 5841, 5206, 4640, 4135, 3685, 3285, 2927, 2609, 2325,
	2072, 1847, 1646, 1467, 1308, 1165, 1039, 926, 825, 735,
	655, 584, 521, 464, 414, 369, 328, 293, 261, 233,
	207, 185, 165, 147, 131, 117, 104, 93, 83, 74,
	66, 58, 52, 46, 41, 37, 33, 29, 26, 23,
	21, 18, 16, 15, 13, 12, 10, 9, 8, 7,
	7, 6, 5, 5, 4, 4, 3, 3, 3, 2,
	2, 2, 2, 1, 1, 1, 1, 1, 1, 1
};
static const UWORD tenth_q15[10] = {
	32768U, 32393, 32022, 31656, 31293, 30935, 30581, 30231, 29885, 29543
};

static UWORD le16(const UBYTE *p)
{
	return (UWORD) (p[0] | (p[1] << 8));
}

ULONG NormWav_LE32(const UBYTE *p)
{
	return (ULONG) p[0] | ((ULONG) p[1] << 8) | ((ULONG) p[2] << 16) | ((ULONG) p[3] << 24);
}

void NormWav_PutLE32(UBYTE *p, ULONG v)
{
	p[0] = (UBYTE) v;
	p[1] = (UBYTE) (v >> 8);
	p[2] = (UBYTE) (v >> 16);
	p[3] = (UBYTE) (v >> 24);
}

static int same4(const UBYTE *p, const char *id)
{
	return p[0] == (UBYTE) id[0] && p[1] == (UBYTE) id[1] && p[2] == (UBYTE) id[2] && p[3] == (UBYTE) id[3];
}

/* WAVE_FORMAT_EXTENSIBLE's SubFormat for PCM, after its first two bytes
 * (1, the format tag): KSDATAFORMAT_SUBTYPE_PCM */
static const UBYTE pcm_guid_tail[14] = {
	0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

/* a "fmt " chunk of size bytes at f: 16-bit PCM into nw, else 0 */
static int pcm16(const UBYTE *f, ULONG size, struct NormWav *nw)
{
	UWORD tag, channels, align, bits;
	ULONG i;

	if (size < 16) return 0;
	tag = le16(f);
	channels = le16(f + 2);
	align = le16(f + 12);
	bits = le16(f + 14);
	if (tag == 0xFFFE) {			/* WAVE_FORMAT_EXTENSIBLE */
		if (size < 40 || le16(f + 24) != 1) return 0;
		for (i = 0; i < sizeof(pcm_guid_tail); i++)
			if (f[26 + i] != pcm_guid_tail[i]) return 0;
	} else if (tag != 1) {
		return 0;
	}
	if (bits != 16 || channels == 0 || align != (UWORD) (channels * 2)) return 0;
	nw->nw_Channels = channels;
	nw->nw_Align = align;
	nw->nw_Rate = NormWav_LE32(f + 4);
	return 1;
}

UWORD NormWav_Parse(const UBYTE *head, ULONG len, struct NormWav *nw)
{
	ULONG pos, size;
	int fmt = 0;

	if (len < 12 || !same4(head, "RIFF") || !same4(head + 8, "WAVE")) return NORMWAV_NOTWAV;
	pos = 12;
	while (pos + 8 <= len) {
		size = NormWav_LE32(head + pos + 4);
		if (same4(head + pos, "fmt ")) {
			if (pos + 8 + size > len || !pcm16(head + pos + 8, size, nw)) return NORMWAV_NOTPCM16;
			fmt = 1;
		} else if (same4(head + pos, "data")) {
			if (!fmt) return NORMWAV_NOTWAV;	/* the format comes first */
			nw->nw_DataStart = pos + 8;
			nw->nw_DataBytes = (size == 0 || size == NORMWAV_TOEND) ? NORMWAV_TOEND : size;
			return NORMWAV_OK;
		}
		/* on to the next chunk: a size that runs past what is here ends
		 * the search (no data in these bytes) */
		if (size > len - pos - 8) break;
		pos += 8 + size + (size & 1);
	}
	return (UWORD) (fmt ? NORMWAV_NODATA : NORMWAV_NOTPCM16);
}

void NormWav_FixSizes(UBYTE *head, const struct NormWav *nw, ULONG databytes)
{
	NormWav_PutLE32(head + nw->nw_DataStart - 4, databytes);
	NormWav_PutLE32(head + 4, nw->nw_DataStart + databytes + (databytes & 1) - 8);
}

ULONG NormWav_Amplitude(UWORD tenths)
{
	ULONG factor, w;

	if (tenths > NORMWAV_MAXTENTHS) tenths = NORMWAV_MAXTENTHS;
	w = whole_q16[tenths / 10];
	factor = (w * tenth_q15[tenths % 10] + 16384UL) >> 15;		/* Q16 */
	return (32767UL * factor + 32768UL) >> 16;
}

UWORD NormWav_PeakTenths(ULONG peak)
{
	UWORD t;
	ULONG a, above;

	if (peak >= 32767UL) return 0;
	if (peak == 0) return NORMWAV_MAXTENTHS;
	/* below -20 dB the tables grow coarse: ten times the peak is exactly
	 * 20 dB more, so measure that and add the 20 back */
	if (peak < 3277UL) {
		t = (UWORD) (NormWav_PeakTenths(peak * 10UL) + 200);
		return (UWORD) (t > NORMWAV_MAXTENTHS ? NORMWAV_MAXTENTHS : t);
	}
	for (t = 1; t <= 200; t++) {
		a = NormWav_Amplitude(t);
		if (a <= peak) {
			above = NormWav_Amplitude((UWORD) (t - 1));
			return (UWORD) (above - peak < peak - a ? t - 1 : t);
		}
	}
	return NORMWAV_MAXTENTHS;
}

LONG NormWav_ParseTenths(CONST_STRPTR s, UWORD *tenths)
{
	ULONG t = 0;
	int neg = 0, digits = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		t = t * 10UL + (ULONG) (*s++ - '0');
		digits++;
		if (t > NORMWAV_MAXTENTHS) return 0;
	}
	t *= 10UL;
	if (*s == '.') {
		s++;
		if (*s >= '0' && *s <= '9') {
			t += (ULONG) (*s++ - '0');
			digits++;
		}
	}
	if (*s != '\0' || digits == 0 || t > NORMWAV_MAXTENTHS) return 0;
	if (t != 0 && !neg) return 0;		/* above full scale */
	*tenths = (UWORD) t;
	return 1;
}

/* the sample a word as loaded stands for: its bytes the other way round */
#define SAMPLE(w)	((WORD) (((w) << 8) | ((w) >> 8)))
#define WORD_OF(s)	((UWORD) ((((UWORD) (s)) << 8) | (((UWORD) (s)) >> 8)))

ULONG NormWav_Peak(const UWORD *w, ULONG n, ULONG peak)
{
	LONG s;

	while (n--) {
		s = SAMPLE(*w);
		w++;
		if (s < 0) s = -s;
		if ((ULONG) s > peak) peak = (ULONG) s;
	}
	return peak;
}

void NormWav_Table(UWORD *table, ULONG target, ULONG peak)
{
	ULONG i, m, o;
	LONG s;

	for (i = 0; i < 65536UL; i++) {
		s = SAMPLE((UWORD) i);
		m = (ULONG) (s < 0 ? -s : s);
		/* m x target is at most 32768 x 32767: in 32 bits */
		o = (m * target + peak / 2UL) / peak;
		if (o > 32767UL) o = 32767UL;
		table[i] = WORD_OF(s < 0 ? -(LONG) o : (LONG) o);
	}
}

void NormWav_Apply(UWORD *w, ULONG n, const UWORD *table)
{
	while (n--) {
		*w = table[*w];
		w++;
	}
}
