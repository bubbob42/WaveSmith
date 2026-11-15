/*
 * test_editprefs.c -- editprefs.c: the defaults, a file read (spaces,
 * CR LF, comments, either case, a last line without its LF), bad values
 * and unknown keys passed over with the default kept, a line too long
 * for a setting, every value written and read back, the worst case
 * inside EDITPREFS_TEXTMAX.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "editprefs.h"

static UWORD read_text(struct EditPrefs *ep, const char *text)
{
	return EditPrefs_Read(ep, (CONST_STRPTR) text, (ULONG) strlen(text));
}

static int same(const struct EditPrefs *a, const struct EditPrefs *b)
{
	return a->ep_Snap == b->ep_Snap && a->ep_LeadMs == b->ep_LeadMs && a->ep_TailMs == b->ep_TailMs &&
	       a->ep_Backup == b->ep_Backup && a->ep_Icons == b->ep_Icons && a->ep_Overview == b->ep_Overview &&
	       a->ep_Play == b->ep_Play && a->ep_AhiID == b->ep_AhiID && a->ep_AhiFreq == b->ep_AhiFreq &&
	       a->ep_FadeShape == b->ep_FadeShape && a->ep_Normalise == b->ep_Normalise;
}

int main(void)
{
	struct EditPrefs ep, def, back;
	char buf[EDITPREFS_TEXTMAX + 16], line[200];
	ULONG len;
	UWORD n;

	/* the defaults: the concept's D6 and D7, trimplan's lead and tail */
	EditPrefs_Defaults(&def);
	CHECK(def.ep_Snap && !def.ep_Backup && def.ep_Icons, "defaults: snap on, no backup, icons");
	CHECK(def.ep_LeadMs == 500 && def.ep_TailMs == 2000 && def.ep_Overview == 10, "defaults: 500 ms, 2000 ms, 10 %");
	CHECK(def.ep_Play == EDITPREFS_PLAY_AUTO && def.ep_AhiID == 0 && def.ep_AhiFreq == 0, "defaults: Automatic, AHI's default mode");
	CHECK(def.ep_FadeShape == 1, "defaults: the fades Even (F2)");
	CHECK(def.ep_Normalise == 10, "defaults: Normalise to -1.0 dB (F7)");

	/* every key, written untidily */
	ep = def;
	n = read_text(&ep, "; a comment\r\n# another\n  snap = no \r\nLEAD=1500\n\tTail\t=\t30000\n"
			   "Backup=Yes\nICONS=off\nPLAY=ahi\nAHIMODE=2228225\nAHIFREQ=48000\nFadeShape=scurve\nnormalise=3\nOverview=33");
	CHECK(n == 11, "all eleven keys taken");
	CHECK(ep.ep_Normalise == 3, "NORMALISE read");
	CHECK(ep.ep_FadeShape == 2, "FADESHAPE read");
	CHECK(ep.ep_Play == EDITPREFS_PLAY_AHI && ep.ep_AhiID == 2228225UL && ep.ep_AhiFreq == 48000, "PLAY, AHIMODE, AHIFREQ read");
	CHECK(!ep.ep_Snap && ep.ep_LeadMs == 1500 && ep.ep_TailMs == 30000, "snap, lead, tail read");
	CHECK(ep.ep_Backup && !ep.ep_Icons && ep.ep_Overview == 33, "backup, icons, the last line without LF read");

	/* bad values: the default stays, the count leaves them out */
	ep = def;
	n = read_text(&ep, "SNAP=maybe\nLEAD=abc\nLEAD=10001\nTAIL=-5\nTAIL=30001\nOVERVIEW=4\nOVERVIEW=51\n"
			   "BACKUP=\nICONS=YES please\nLEAD=12x\nCOLOUR=red\n=5\nLEAD\nTAIL 100\nPLAY=paula\nAHIMODE=0x10\nAHIFREQ=\nFADESHAPE=wobbly\nNORMALISE=241\nNORMALISE=-1.0\n");
	CHECK(n == 0, "no bad value taken");
	CHECK(same(&ep, &def), "the defaults kept over every bad value");

	/* the ends of the ranges are good values */
	ep = def;
	n = read_text(&ep, "LEAD=0\nTAIL=0\nOVERVIEW=5\nNORMALISE=0\n");
	CHECK(n == 4 && ep.ep_LeadMs == 0 && ep.ep_TailMs == 0 && ep.ep_Overview == 5 && ep.ep_Normalise == 0, "the low ends taken");
	n = read_text(&ep, "LEAD=10000\nTAIL=30000\nOVERVIEW=50\nNORMALISE=240\n");
	CHECK(n == 4 && ep.ep_LeadMs == 10000 && ep.ep_TailMs == 30000 && ep.ep_Overview == 50 && ep.ep_Normalise == 240, "the high ends taken");

	/* a line longer than a setting's: passed over whole, the next one read */
	ep = def;
	memset(line, ' ', sizeof(line));
	memcpy(line, "LEAD=1000", 9);
	memcpy(line + sizeof(line) - 12, "\nLEAD=2500\n", 12);
	n = read_text(&ep, line);
	CHECK(n == 1 && ep.ep_LeadMs == 2500, "a long line passed over, the next one taken");

	/* a later line wins */
	ep = def;
	n = read_text(&ep, "TAIL=100\nTAIL=200\n");
	CHECK(n == 2 && ep.ep_TailMs == 200, "the later line wins");

	/* written and read back: every value, both ways */
	ep.ep_Snap = FALSE;
	ep.ep_LeadMs = 1234;
	ep.ep_TailMs = 25000;
	ep.ep_Backup = TRUE;
	ep.ep_Icons = FALSE;
	ep.ep_Overview = 42;
	ep.ep_Play = EDITPREFS_PLAY_CARD;
	ep.ep_AhiID = 0x00020004UL;
	ep.ep_AhiFreq = 22050;
	ep.ep_FadeShape = 0;
	ep.ep_Normalise = 37;
	len = EditPrefs_Write(&ep, (STRPTR) buf, sizeof(buf));
	buf[len] = '\0';
	CHECK(len > 0 && len < EDITPREFS_TEXTMAX, "written");
	CHECK(strncmp(buf, "; WaveSmith settings\n", strlen("; WaveSmith settings\n")) == 0, "the file says what it is");
	back = def;
	CHECK(EditPrefs_Read(&back, (CONST_STRPTR) buf, len) == 11 && same(&back, &ep), "read back the same");
	len = EditPrefs_Write(&def, (STRPTR) buf, sizeof(buf));
	ep = back;
	CHECK(EditPrefs_Read(&ep, (CONST_STRPTR) buf, len) == 11 && same(&ep, &def), "the defaults read back");

	/* the worst case: the numbers at 32 bits (a struct set by hand) */
	ep.ep_LeadMs = 0xFFFFFFFFUL;
	ep.ep_TailMs = 0xFFFFFFFFUL;
	ep.ep_Overview = 0xFFFF;
	ep.ep_Play = 7;
	ep.ep_AhiID = 0xFFFFFFFFUL;
	ep.ep_AhiFreq = 0xFFFFFFFFUL;
	ep.ep_FadeShape = 0xFFFF;
	ep.ep_Normalise = 0xFFFF;
	memset(buf, 0x55, sizeof(buf));
	len = EditPrefs_Write(&ep, (STRPTR) buf, EDITPREFS_TEXTMAX);
	CHECK(len > 0 && len < EDITPREFS_TEXTMAX && buf[len] == '\0', "the worst case fits, ended");
	CHECK((unsigned char) buf[EDITPREFS_TEXTMAX] == 0x55, "nothing past EDITPREFS_TEXTMAX");
	CHECK(EditPrefs_Write(&ep, (STRPTR) buf, EDITPREFS_TEXTMAX - 1) == 0, "less room: nothing written");

	return CHECK_DONE("test_editprefs");
}
