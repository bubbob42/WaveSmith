/*
 * levelmap.c -- see levelmap.h
 */
#include "levelmap.h"
#include "level.h"

void LevelMap_Header(UBYTE *head, ULONG look_micros)
{
	head[0] = 'P';
	head[1] = 'R';
	head[2] = 'L';
	head[3] = '1';
	head[4] = (UBYTE) (look_micros >> 24);
	head[5] = (UBYTE) (look_micros >> 16);
	head[6] = (UBYTE) (look_micros >> 8);
	head[7] = (UBYTE) look_micros;
}

UBYTE LevelMap_Byte(ULONG look, BOOL mono)
{
	LONG db = Level_Db(Level_Louder(look, mono));

	return (UBYTE) (db == LEVEL_NONE ? LEVELMAP_NONE : -db);
}

BOOL LevelMap_Name(CONST_STRPTR take, STRPTR name, ULONG size)
{
	ULONG n = 0, dot = 0, i, k;
	CONST_STRPTR s = LEVELMAP_SUFFIX;

	while (take[n]) {
		if (take[n] == '.') dot = n;
		else if (take[n] == '/' || take[n] == ':') dot = 0;
		n++;
	}
	if (!dot) dot = n;			/* no suffix: added */
	if (dot + 5 > size) return FALSE;
	for (i = 0; i < dot; i++) name[i] = take[i];
	for (k = 0; s[k]; k++) name[i++] = s[k];
	name[i] = '\0';
	return TRUE;
}
