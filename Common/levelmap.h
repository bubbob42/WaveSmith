/*
 * levelmap.h -- the level map written beside each take (<name>.prl): the
 * take's level at every look, so that later work (the gap detector, T2;
 * the take list's picture) need not read the music again. About 1.5 KB a
 * minute at 40 ms a look. Pure: host-tested (test/host/prismarec/
 * test_levelmap.c); lmapfile.c writes it.
 *
 * The file: "PRL1", the look period in microseconds (4 bytes, big-endian),
 * then a byte a look: the louder side, in dB below full scale (0 .. 90),
 * 255 for nothing at all.
 */
#ifndef PRISMAREC_LEVELMAP_H
#define PRISMAREC_LEVELMAP_H

#include <exec/types.h>

#define LEVELMAP_HEADER		8
#define LEVELMAP_NONE		255
#define LEVELMAP_SUFFIX		".prl"

void LevelMap_Header(UBYTE *head, ULONG look_micros);	/* LEVELMAP_HEADER bytes */
/* A look (left << 16 | right) as its byte; mono: the left side alone */
UBYTE LevelMap_Byte(ULONG look, BOOL mono);
/* The map's name beside a take's: its suffix (".mp3") replaced; FALSE when
 * it does not fit size */
BOOL LevelMap_Name(CONST_STRPTR take, STRPTR name, ULONG size);

#endif
