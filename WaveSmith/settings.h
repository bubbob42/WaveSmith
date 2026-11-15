/*
 * settings.h -- WaveSmith.prefs (editprefs.h has the text) on the disk:
 * read from PROGDIR:, then ENV:, else the defaults stay; Save settings
 * writes it back where it was read from -- PROGDIR: when it came from
 * there, else ENVARC: and ENV: (the Style Guide's search order, as
 * PrismaRec's prefsfile.c).
 */
#ifndef WAVESMITH_SETTINGS_H
#define WAVESMITH_SETTINGS_H

#include <exec/types.h>

#include "editprefs.h"

/* where the settings came from */
#define SETTINGS_NONE		0	/* nowhere: the defaults */
#define SETTINGS_PROGDIR	1
#define SETTINGS_ENV		2

/* The first file found over ep (which holds the defaults): SETTINGS_#? */
UWORD Settings_Load(struct EditPrefs *ep);
/* ep written where Settings_Load() found it (from): 0, or a DOS error */
LONG Settings_Save(const struct EditPrefs *ep, UWORD from);

#endif
