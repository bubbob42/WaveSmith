/*
 * wavesmith_locale.c -- locale.library and WaveSmith.catalog, and, as the
 * single CATCOMP_CODE translation unit, GetString() with the built-in
 * English table from WaveSmith_cat.h. PrismaRec's prismarec_locale.c,
 * with WaveSmith's names.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <utility/tagitem.h>
#include <libraries/locale.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#define CATCOMP_CODE
#include "wavesmith_locale.h"
#include "wavesmith_catver.h"	/* WS_CATALOG_VERSION, WS_CATALOG_SIG */

/* NULL on purpose: a tentative definition makes libnix link an auto-open
 * stub instead */
struct LocaleBase *LocaleBase = NULL;

struct LocaleInfo WS_LI = { NULL, NULL };

static struct Locale *ws_Locale = NULL;

#define CATALOG_NAME	"WaveSmith.catalog"
#define CATALOG_ASSIGN	"Prisma:"	/* the drawer the Installer made */

static BOOL same_str(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (BOOL) (*a == *b);
}

/*
 * Where the catalogs are. OpenCatalog looks in PROGDIR:Catalogs/<language>
 * and LOCALE:Catalogs/<language>, nowhere else -- and a copy of
 * WaveSmith started from elsewhere has no Catalogs beside it. Its drawer
 * is Prisma:, so: no PROGDIR:Catalogs but a Prisma:Catalogs -> a lock on
 * Prisma:, for SetProgramDir() around the open (dos.library documents
 * that use). 0: look where OpenCatalog always looks. Requesters off: an
 * assign whose disk is not in the drive must fail, not ask.
 */
static BPTR catalog_home(void)
{
	struct Process *me = (struct Process *) FindTask(NULL);
	APTR oldwin = me->pr_WindowPtr;
	BPTR lock, home = 0;

	me->pr_WindowPtr = (APTR) -1;
	if ((lock = Lock((STRPTR) "PROGDIR:Catalogs", ACCESS_READ))) {
		UnLock(lock);
	} else if ((lock = Lock((STRPTR) CATALOG_ASSIGN "Catalogs", ACCESS_READ))) {
		UnLock(lock);
		home = Lock((STRPTR) CATALOG_ASSIGN, ACCESS_READ);
	}
	me->pr_WindowPtr = oldwin;
	return home;
}

BOOL WS_OpenCatalog(void)
{
	struct TagItem tags[3];
	CONST_STRPTR sig;
	BPTR home, oldhome = 0;

	if (!(LocaleBase = (struct LocaleBase *) OpenLibrary((STRPTR) "locale.library", 38)))
		return FALSE;		/* English, and on we go */
	WS_LI.li_LocaleBase = (APTR) LocaleBase;
	ws_Locale = OpenLocale(NULL);

	tags[0].ti_Tag = OC_BuiltInLanguage;
	tags[0].ti_Data = (ULONG) "english";
	/* EXACT: the ids are numbered in the seed's order, and a catalog for
	 * another layout would hand out shifted strings -- wrong words, and
	 * wrong arguments for a format */
	tags[1].ti_Tag = OC_Version;
	tags[1].ti_Data = WS_CATALOG_VERSION;
	tags[2].ti_Tag = TAG_DONE;

	if ((home = catalog_home())) oldhome = SetProgramDir(home);
	WS_LI.li_Catalog = (APTR) OpenCatalogA(ws_Locale, (STRPTR) CATALOG_NAME, tags);
	if (home) {
		SetProgramDir(oldhome);
		UnLock(home);
	}

	/* belt to OC_Version's braces: id 0 carries the layout's fingerprint,
	 * which a catalog for any other layout cannot have */
	if (WS_LI.li_Catalog) {
		sig = (CONST_STRPTR) GetCatalogStr((struct Catalog *) WS_LI.li_Catalog,
						   WS_SIG_CATALOG, (STRPTR) "WSCAT-builtin");
		if (!same_str((const char *) sig, WS_CATALOG_SIG)) {
			CloseCatalog((struct Catalog *) WS_LI.li_Catalog);
			WS_LI.li_Catalog = NULL;
		}
	}
	return (BOOL) (WS_LI.li_Catalog != NULL);
}

CONST_STRPTR WS_LanguageName(void)
{
	if (!ws_Locale) return NULL;
	if (ws_Locale->loc_PrefLanguages[0] && ws_Locale->loc_PrefLanguages[0][0])
		return (CONST_STRPTR) ws_Locale->loc_PrefLanguages[0];
	return (CONST_STRPTR) ws_Locale->loc_LanguageName;
}

void WS_CloseCatalog(void)
{
	if (WS_LI.li_Catalog) {
		CloseCatalog((struct Catalog *) WS_LI.li_Catalog);
		WS_LI.li_Catalog = NULL;
	}
	if (ws_Locale) {
		CloseLocale(ws_Locale);
		ws_Locale = NULL;
	}
	if (LocaleBase) {
		CloseLibrary((struct Library *) LocaleBase);
		LocaleBase = NULL;
		WS_LI.li_LocaleBase = NULL;
	}
}
