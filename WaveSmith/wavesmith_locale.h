#ifndef WAVESMITH_LOCALE_H
#define WAVESMITH_LOCALE_H

/*
 * wavesmith_locale.h -- locale.library and WaveSmith.catalog.
 *
 * Pulls in the generated id/accessor header (WaveSmith_cat.h): every
 * module that includes this sees the WS_* ids, struct LocaleInfo and
 * GetString(). The one LocaleInfo (WS_LI) and GetString()'s built-in
 * English table live in wavesmith_locale.c, the single translation
 * unit compiled with CATCOMP_CODE. PrismaRec's prismarec_locale.h, with WaveSmith's names.
 *
 * Opening is NON-FATAL: no locale.library, no catalog or a stale one --
 * GetString() answers in English and the program runs on.
 */

#include <exec/types.h>

#define CATCOMP_NUMBERS
#include "WaveSmith_cat.h"

extern struct LocaleInfo WS_LI;

BOOL WS_OpenCatalog(void);
void WS_CloseCatalog(void);

/* The user's first language as locale.library names it ("deutsch"), the
 * name of the Help/<language> and Catalogs/<language> drawers; NULL
 * without a locale. */
CONST_STRPTR WS_LanguageName(void);

#endif
