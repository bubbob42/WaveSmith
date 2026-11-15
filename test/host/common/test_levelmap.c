/*
 * test_levelmap.c -- levelmap.c: the header, a look's byte, the name
 */
#include <string.h>

#include "check.h"
#include "levelmap.h"
#include "level.h"

int main(void)
{
	UBYTE h[LEVELMAP_HEADER];
	char name[32];

	LevelMap_Header(h, 40000);
	CHECK(!memcmp(h, "PRL1", 4) && h[4] == 0 && h[5] == 0 && h[6] == 0x9C && h[7] == 0x40, "PRL1, 40000 big-endian");
	CHECK(LevelMap_Byte(LEVEL_FULL << 16, FALSE) == 0, "full scale: 0");
	CHECK(LevelMap_Byte(Level_Peak(-20), FALSE) == 20 && LevelMap_Byte(Level_Peak(-20) << 16, FALSE) == 20,
	      "-20 dB on either side: 20");
	CHECK(LevelMap_Byte((Level_Peak(-30) << 16) | Level_Peak(-6), FALSE) == 6, "the louder side");
	CHECK(LevelMap_Byte((Level_Peak(-30) << 16) | Level_Peak(-6), TRUE) == 30, "mono: the left alone");
	CHECK(LevelMap_Byte(0, FALSE) == LEVELMAP_NONE && LevelMap_Byte(1, FALSE) == 90, "nothing: 255; the least: 90");

	CHECK(LevelMap_Name("Work:Takes/Take 1.mp3", name, sizeof(name)) && !strcmp(name, "Work:Takes/Take 1.prl"),
	      "the suffix replaced");
	CHECK(LevelMap_Name("Work:My.Takes/Take 1", name, sizeof(name)) && !strcmp(name, "Work:My.Takes/Take 1.prl"),
	      "no suffix (a dot in the drawer): added");
	CHECK(LevelMap_Name("A.b.flac", name, sizeof(name)) && !strcmp(name, "A.b.prl"), "the last dot");
	CHECK(!LevelMap_Name("Work:Takes/Take 1.mp3", name, 21) && LevelMap_Name("Work:Takes/Take 1.mp3", name, 22),
	      "the room: refused when short, not cut");
	return CHECK_DONE("test_levelmap");
}
