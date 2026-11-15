/*
 * beside.c -- see beside.h
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "beside.h"
#include "textutil.h"

/* where path's own name begins (after its drawer's ':' or '/') */
static UWORD name_at(CONST_STRPTR path)
{
	UWORD i, at = 0;

	for (i = 0; path[i]; i++) {
		if (path[i] == ':' || path[i] == '/') at = (UWORD) (i + 1);
	}
	return at;
}

BOOL Beside_Name(STRPTR out, CONST_STRPTR name)
{
	UWORD at = name_at((CONST_STRPTR) out), k;

	for (k = 0; name[k]; k++) ;
	if ((ULONG) at + k >= BESIDE_LEN) return FALSE;
	for (k = 0; name[k]; k++) out[at + k] = name[k];
	out[at + k] = '\0';
	return TRUE;
}

/* path into out (BESIDE_LEN), cut to fit */
static void copy_path(CONST_STRPTR path, STRPTR out)
{
	Text_Copy(out, path, BESIDE_LEN);
}

BOOL Beside_TempName(CONST_STRPTR path, UWORD k, STRPTR out)
{
	char name[24];
	STRPTR p;

	p = Text_Put((STRPTR) name, (CONST_STRPTR) "WaveSmith-");
	p = Text_PutNumber(p, k);
	Text_Put(p, (CONST_STRPTR) ".tmp");
	copy_path(path, out);
	return Beside_Name(out, (CONST_STRPTR) name);
}

BOOL Beside_BackupName(CONST_STRPTR path, STRPTR out)
{
	char name[BESIDE_FIT + 1];
	UWORD at = name_at(path), n;

	for (n = 0; path[at + n] && n < BESIDE_FIT - 4; n++) name[n] = path[at + n];
	Text_Put((STRPTR) name + n, (CONST_STRPTR) ".bak");
	copy_path(path, out);
	return (BOOL) (Beside_Name(out, (CONST_STRPTR) name) && !Text_Same((CONST_STRPTR) out, path));
}

BOOL Beside_Temp(CONST_STRPTR path, STRPTR out)
{
	BPTR lock;
	UWORD k;

	for (k = 0; k < BESIDE_TRIES; k++) {
		if (!Beside_TempName(path, k, out)) {
			SetIoErr(ERROR_LINE_TOO_LONG);
			return FALSE;
		}
		if (!(lock = Lock((STRPTR) out, SHARED_LOCK))) return TRUE;
		UnLock(lock);
	}
	SetIoErr(ERROR_OBJECT_EXISTS);
	return FALSE;
}
