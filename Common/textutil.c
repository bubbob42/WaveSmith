/*
 * textutil.c -- see textutil.h
 */
#include "textutil.h"

static UBYTE upper(UBYTE c)
{
	return (UBYTE) ((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

BOOL Text_Same(CONST_STRPTR a, CONST_STRPTR b)
{
	while (*a && upper((UBYTE) *a) == upper((UBYTE) *b)) {
		a++;
		b++;
	}
	return (BOOL) (*a == '\0' && *b == '\0');
}

BOOL Text_Prefix(CONST_STRPTR *s, CONST_STRPTR prefix)
{
	CONST_STRPTR p = *s;

	while (*prefix) {
		if (upper((UBYTE) *p) != upper((UBYTE) *prefix)) return FALSE;
		p++;
		prefix++;
	}
	*s = p;
	return TRUE;
}

STRPTR Text_Put(STRPTR p, CONST_STRPTR s)
{
	while (*s) *p++ = *s++;
	*p = '\0';
	return p;
}

STRPTR Text_PutNumber(STRPTR p, ULONG n)
{
	char digits[10];
	UWORD i = 0;

	do {
		digits[i++] = (char) ('0' + n % 10UL);
		n /= 10UL;
	} while (n);
	while (i) *p++ = digits[--i];
	*p = '\0';
	return p;
}

BOOL Text_GetNumber(CONST_STRPTR *s, ULONG *n)
{
	CONST_STRPTR p = *s;
	ULONG v = 0, d;

	if (*p < '0' || *p > '9') return FALSE;
	while (*p >= '0' && *p <= '9') {
		d = (ULONG) (*p - '0');
		if (v > (0xFFFFFFFFUL - d) / 10UL) return FALSE;
		v = v * 10UL + d;
		p++;
	}
	*n = v;
	*s = p;
	return TRUE;
}

ULONG Text_Copy(STRPTR dst, CONST_STRPTR src, ULONG size)
{
	ULONG n = 0;

	if (size == 0) return 0;
	while (n + 1 < size && src[n]) {
		dst[n] = src[n];
		n++;
	}
	dst[n] = '\0';
	return n;
}

ULONG Text_Length(CONST_STRPTR s)
{
	ULONG n = 0;

	while (s[n]) n++;
	return n;
}

ULONG Text_Fmt(STRPTR buf, ULONG size, const struct TextArgs *ta)
{
	CONST_STRPTR f = ta->ta_Fmt, s;
	char num[12];
	ULONG n = 0;
	UWORD nums = 0, strs = 0;
	LONG v;

	if (size == 0) return 0;
	while (*f && n + 1 < size) {
		s = NULL;
		if (f[0] == '%' && f[1] == '%') {
			buf[n++] = '%';
			f += 2;
			continue;
		}
		if (f[0] == '%' && f[1] == 'l' && (f[2] == 'd' || f[2] == 'u')) {
			v = nums < TEXT_ARGS ? ta->ta_Num[nums] : 0;
			nums++;
			if (f[2] == 'd' && v < 0) {
				num[0] = '-';
				Text_PutNumber(num + 1, 0UL - (ULONG) v);	/* -v overflows at the most negative */
			} else {
				Text_PutNumber(num, (ULONG) v);
			}
			s = num;
			f += 3;
		} else if (f[0] == '%' && f[1] == 's') {
			s = strs < TEXT_ARGS && ta->ta_Str[strs] ? ta->ta_Str[strs] : (CONST_STRPTR) "";
			strs++;
			f += 2;
		}
		if (!s) {
			buf[n++] = *f++;
			continue;
		}
		while (*s && n + 1 < size) buf[n++] = *s++;
	}
	buf[n] = '\0';
	return n;
}
