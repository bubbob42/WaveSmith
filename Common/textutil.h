/*
 * textutil.h -- the little string work the pure modules share, without a
 * C library: words compared, numbers written and read, bounded copies.
 * Pure: host-tested with the modules that use it.
 */
#ifndef PRISMAREC_TEXTUTIL_H
#define PRISMAREC_TEXTUTIL_H

#include <exec/types.h>

/* Equal, ASCII letters in either case */
BOOL Text_Same(CONST_STRPTR a, CONST_STRPTR b);
/* *s starts with prefix (either case): *s moves past it */
BOOL Text_Prefix(CONST_STRPTR *s, CONST_STRPTR prefix);
/* Appends at p and returns the new end (a '\0' written there). The
 * caller sees to the room. */
STRPTR Text_Put(STRPTR p, CONST_STRPTR s);
STRPTR Text_PutNumber(STRPTR p, ULONG n);
/* Digits at *s: n and TRUE, *s past them; FALSE without a digit or past
 * 32 bits */
BOOL Text_GetNumber(CONST_STRPTR *s, ULONG *n);
/* At most size - 1 bytes and the '\0'; the length copied */
ULONG Text_Copy(STRPTR dst, CONST_STRPTR src, ULONG size);
ULONG Text_Length(CONST_STRPTR s);

/* A catalog text with its numbers and texts filled in: the n-th %ld (or
 * %lu) takes ta_Num[n], the n-th %s ta_Str[n] -- counted apart, so a
 * translation may put them in another order. %% is a percent sign;
 * anything else is copied. At most size - 1 bytes; the length. */
#define TEXT_ARGS	3
struct TextArgs {
	CONST_STRPTR ta_Fmt;
	LONG ta_Num[TEXT_ARGS];
	CONST_STRPTR ta_Str[TEXT_ARGS];
};
ULONG Text_Fmt(STRPTR buf, ULONG size, const struct TextArgs *ta);

#endif
