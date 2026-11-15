/*
 * beside.h -- the names of the files a save makes beside a file: its
 * temporary file and its backup. FFS without long names takes
 * BESIDE_FIT characters and cuts a longer name silently, so a name and a
 * suffix added to it could be cut into one and the same file (a save
 * renamed the original back over itself, the edits lost; a .prl's
 * temporary file opened the .prl itself). So a temporary file has a
 * short name of its own, the first of a few that is not there, and a
 * backup's name is cut so that it and ".bak" fit.
 */
#ifndef WAVESMITH_BESIDE_H
#define WAVESMITH_BESIDE_H

#include <exec/types.h>

#define BESIDE_FIT	30		/* FFS without long names */
#define BESIDE_TRIES	16		/* temporary names tried */
#define BESIDE_LEN	256		/* a path's buffer (doc.h's DOC_PATHLEN) */

/* the own name of the path in out (BESIDE_LEN) replaced by name, its
   drawer kept: FALSE (out as it was) when it does not fit. Pure */
BOOL Beside_Name(STRPTR out, CONST_STRPTR name);
/* the k-th temporary name beside path: "WaveSmith-<k>.tmp". Pure */
BOOL Beside_TempName(CONST_STRPTR path, UWORD k, STRPTR out);
/* the backup's name beside path: its name cut to fit BESIDE_FIT with
   ".bak" -- FALSE when that would be path's own name. Pure */
BOOL Beside_BackupName(CONST_STRPTR path, STRPTR out);
/* the first temporary name beside path that is not there: FALSE (IoErr()
   set) when none is free or it does not fit */
BOOL Beside_Temp(CONST_STRPTR path, STRPTR out);

#endif
