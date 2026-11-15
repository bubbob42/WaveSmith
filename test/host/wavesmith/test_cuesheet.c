/*
 * test_cuesheet.c -- cuesheet.c: a sheet written and read back frame for
 * frame (the REMs), its INDEX times as a player reads them (mm:ss:ff, a
 * side of 25 minutes past 32 bits' frame x 75), a sheet from another
 * program (no REM: the INDEX time), its first track's start and a
 * second FILE passed over, 99 tracks, marks out of order or twice, bad
 * lines, the worst case inside CUE_TEXTMAX.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "cuesheet.h"

static char buf[CUE_TEXTMAX + 16];

static UWORD read_text(struct CueRead *cr, const char *text)
{
	return Cue_Read(cr, (CONST_STRPTR) text, (ULONG) strlen(text));
}

int main(void)
{
	static ULONG marks[CUE_MARKS_MAX + 2];
	struct CueSheet cs;
	static struct CueRead cr;
	ULONG len, i;
	int ok;

	/* two marks at 44.1 kHz: the text, then read back exactly */
	marks[0] = 1764000;			/* 40.0 s */
	marks[1] = 3695580;			/* 83.8 s */
	cs.cs_File = (CONST_STRPTR) "Side A.wav";
	cs.cs_Rate = 44100;
	cs.cs_Marks = marks;
	cs.cs_Count = 2;
	len = Cue_Write(&cs, (STRPTR) buf, sizeof(buf));
	buf[len] = '\0';
	CHECK(len > 0 && strstr(buf, "FILE \"Side A.wav\" WAVE\n") != NULL, "written: the file named");
	CHECK(strstr(buf, "  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n") != NULL, "track 1 at the start");
	CHECK(strstr(buf, "  TRACK 02 AUDIO\n    REM FRAME 1764000\n    INDEX 01 00:40:00\n") != NULL, "track 2: 00:40:00, its frame");
	/* 83.8 s: 1:23 and 0.8 s = 60 75ths */
	CHECK(strstr(buf, "  TRACK 03 AUDIO\n    REM FRAME 3695580\n    INDEX 01 01:23:60\n") != NULL, "track 3: 01:23:60");
	cr.cr_Rate = 44100;
	cr.cr_Frames = 44100UL * 120;
	CHECK(Cue_Read(&cr, (CONST_STRPTR) buf, len) == 2 && cr.cr_Marks[0] == 1764000 && cr.cr_Marks[1] == 3695580,
	      "read back: frame for frame");

	/* a 25-minute side: frame x 75 would wrap past 21.6 minutes */
	marks[0] = 44100UL * (24 * 60 + 59) + 44100UL / 2;	/* 24:59.5 */
	cs.cs_Count = 1;
	len = Cue_Write(&cs, (STRPTR) buf, sizeof(buf));
	buf[len] = '\0';
	CHECK(strstr(buf, "INDEX 01 24:59:37\n") != NULL, "past 32 bits of frame x 75: 24:59:37");
	cr.cr_Frames = 44100UL * 1800;
	CHECK(Cue_Read(&cr, (CONST_STRPTR) buf, len) == 1 && cr.cr_Marks[0] == marks[0], "read back past 21 minutes, exact");
	/* 48 kHz: 75ths of it */
	marks[0] = 48000UL * 61 + 640;		/* 1:01 and one 75th */
	cs.cs_Rate = 48000;
	len = Cue_Write(&cs, (STRPTR) buf, sizeof(buf));
	buf[len] = '\0';
	CHECK(strstr(buf, "INDEX 01 01:01:01\n") != NULL, "48 kHz: 01:01:01");

	/* another program's sheet: no REM, times only; the first track not
	   at the start is a marker too; a second FILE is another sound's */
	cr.cr_Rate = 44100;
	cr.cr_Frames = 44100UL * 600;
	CHECK(read_text(&cr, "PERFORMER \"Someone\"\r\nTITLE \"A side\"\r\nFILE \"x.wav\" WAVE\r\n"
			     "  TRACK 01 AUDIO\r\n    TITLE \"One\"\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n"
			     "  TRACK 02 AUDIO\r\n    INDEX 01 03:10:15\r\n"
			     "FILE \"y.wav\" WAVE\r\n  TRACK 03 AUDIO\r\n    INDEX 01 00:05:00\r\n") == 1,
	      "another program's: one mark, the second file passed over");
	CHECK(cr.cr_First == 44100UL * 2 && cr.cr_Marks[0] == 44100UL * 190 + 44100UL * 15 / 75,
	      "another program's: the first track from INDEX 01 (not 00, the pregap before it), 75ths as frames");
	/* lower case, tabs, a REM of another kind, a REM before any TRACK */
	CHECK(read_text(&cr, "rem FRAME 999\nfile \"x\" wave\n\ttrack 01 audio\n\t\tindex 01 00:00:00\n"
			     "\ttrack 02 audio\n\t\trem date 1981\n\t\tindex 01 00:10:00\n") == 1 && cr.cr_Marks[0] == 441000UL &&
	      cr.cr_First == 0,
	      "either case, tabs; a REM before the tracks and a REM DATE passed over");
	/* out of order, twice, at 0, past the end: rising, each once */
	cr.cr_Frames = 1000000UL;
	CHECK(read_text(&cr, "FILE \"x\" WAVE\nTRACK 01 AUDIO\nREM FRAME 50000\nINDEX 01 00:00:00\n"
			     "TRACK 02 AUDIO\nREM FRAME 500000\nINDEX 01 00:00:00\nTRACK 03 AUDIO\nREM FRAME 100000\n"
			     "TRACK 04 AUDIO\nREM FRAME 500000\nTRACK 05 AUDIO\nREM FRAME 40000\nTRACK 06 AUDIO\nREM FRAME 1000000\n") == 2 &&
	      cr.cr_First == 50000 && cr.cr_Marks[0] == 100000 && cr.cr_Marks[1] == 500000,
	      "out of order, twice, before the first track, past the end: two, rising; the first track's apart");
	/* bad lines: no mark from them (the sound long enough for each) */
	cr.cr_Frames = 44100UL * 600;
	CHECK(read_text(&cr, "FILE \"x\" WAVE\nTRACK 01 AUDIO\nINDEX 01 00:61:00\nTRACK 02 AUDIO\nINDEX 01 00:01:75\n"
			     "TRACK 03 AUDIO\nINDEX 01 1:2\nTRACK 04 AUDIO\nINDEX 02 00:03:00\nTRACK 05 AUDIO\nREM FRAME 12x\n"
			     "INDEX01 00:04:00\nTRACK 06 AUDIO\nINDEX 01 00:05:00\n") == 1 && cr.cr_Marks[0] == 220500UL,
	      "bad times, INDEX 02, REM FRAME 12x, INDEX01: passed over, the good one taken");
	CHECK(read_text(&cr, "") == 0 && read_text(&cr, "INDEX 01 00:05:00\n") == 0, "nothing, an INDEX before any TRACK: none");

	/* 99 tracks, the longest name, the largest frames: inside CUE_TEXTMAX */
	for (i = 0; i < CUE_MARKS_MAX + 2; i++) marks[i] = 0xFFFFFF00UL + i;
	memset(buf, 'n', CUE_NAMELEN + 20);
	buf[CUE_NAMELEN + 20] = '\0';
	{
		static char name[CUE_NAMELEN + 21];

		memcpy(name, buf, sizeof(name));
		name[3] = '"';
		cs.cs_File = (CONST_STRPTR) name;
		cs.cs_Rate = 8000;
		cs.cs_Count = CUE_MARKS_MAX + 2;
		memset(buf, 0x55, sizeof(buf));
		len = Cue_Write(&cs, (STRPTR) buf, CUE_TEXTMAX);
		CHECK(len > 0 && len < CUE_TEXTMAX && buf[len] == '\0' && (unsigned char) buf[CUE_TEXTMAX] == 0x55,
		      "the worst case fits, ended, nothing past CUE_TEXTMAX");
		CHECK(strstr(buf, "TRACK 99 AUDIO") != NULL && strstr(buf, "TRACK 100") == NULL, "99 tracks, no more");
		CHECK(strstr(buf, "FILE \"nnn'") != NULL, "a quote in the name: an apostrophe");
		CHECK(Cue_Write(&cs, (STRPTR) buf, CUE_TEXTMAX - 1) == 0, "less room: nothing");
	}
	/* 98 marks read back, the 99th track's start too */
	for (i = 0; i < CUE_MARKS_MAX; i++) marks[i] = 1000UL * (i + 1);
	cs.cs_File = (CONST_STRPTR) "x.wav";
	cs.cs_Rate = 44100;
	cs.cs_Count = CUE_MARKS_MAX;
	len = Cue_Write(&cs, (STRPTR) buf, sizeof(buf));
	cr.cr_Frames = 1UL << 30;
	ok = Cue_Read(&cr, (CONST_STRPTR) buf, len) == CUE_MARKS_MAX;
	for (i = 0; ok && i < CUE_MARKS_MAX; i++) ok = cr.cr_Marks[i] == 1000UL * (i + 1);
	CHECK(ok, "98 marks read back, each one");
	return CHECK_DONE("test_cuesheet");
}
