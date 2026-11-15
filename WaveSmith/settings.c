/*
 * settings.c -- see settings.h
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "settings.h"
#include "textutil.h"

#define FILE_MAX	1024		/* more is no settings file of ours */
#define PATH_LEN	48		/* "ENVARC:WaveSmith.prefs.new" and room */

static BOOL read_file(CONST_STRPTR path, struct EditPrefs *ep)
{
	struct Process *me = (struct Process *) FindTask(NULL);
	APTR window = me->pr_WindowPtr;
	STRPTR buf;
	BPTR fh;
	LONG len;

	/* no "please insert volume" for a file that may well not be there;
	   the window pointer is the Shell's too: put back */
	me->pr_WindowPtr = (APTR) -1L;
	fh = Open((STRPTR) path, MODE_OLDFILE);
	me->pr_WindowPtr = window;
	if (!fh) return FALSE;
	len = -1;
	if ((buf = AllocVec(FILE_MAX + 1, MEMF_ANY))) {
		len = Read(fh, buf, FILE_MAX + 1);
		/* longer than ours ever are: up to its last whole line */
		if (len > (LONG) FILE_MAX)
			for (len = FILE_MAX; len > 0 && buf[len - 1] != '\n'; len--)
				;
		if (len > 0) EditPrefs_Read(ep, buf, (ULONG) len);
		FreeVec(buf);
	}
	Close(fh);
	return (BOOL) (len >= 0);
}

UWORD Settings_Load(struct EditPrefs *ep)
{
	/* PROGDIR: is ZERO for a resident command, and dos.library then asks
	   for a volume called PROGDIR instead of failing: only with a home */
	if (GetProgramDir() && read_file((CONST_STRPTR) "PROGDIR:" EDITPREFS_NAME, ep)) return SETTINGS_PROGDIR;
	if (read_file((CONST_STRPTR) "ENV:" EDITPREFS_NAME, ep)) return SETTINGS_ENV;
	return SETTINGS_NONE;
}

/* the text into path: beside it first, complete before it takes the old
   one's place (a disk full while writing left half a file, read at the
   next start): 0 or the DOS error */
static LONG write_file(CONST_STRPTR path, CONST_STRPTR text)
{
	char tmp[PATH_LEN];
	BPTR fh;
	LONG len = (LONG) Text_Length(text), err = 0;

	Text_Copy((STRPTR) tmp, path, sizeof(tmp) - 4);
	Text_Put((STRPTR) tmp + Text_Length((CONST_STRPTR) tmp), (CONST_STRPTR) ".new");
	if (!(fh = Open((STRPTR) tmp, MODE_NEWFILE))) return IoErr();
	if (Write(fh, (APTR) text, len) != len) err = IoErr();
	if (!Close(fh) && !err) err = IoErr();
	if (!err && !DeleteFile((STRPTR) path) && IoErr() != ERROR_OBJECT_NOT_FOUND) err = IoErr();
	if (!err && !Rename((STRPTR) tmp, (STRPTR) path)) {
		/* the old one gone already: the new one stays under its own name,
		   not lost with it */
		return IoErr();
	}
	if (err) DeleteFile((STRPTR) tmp);
	return err;
}

LONG Settings_Save(const struct EditPrefs *ep, UWORD from)
{
	STRPTR buf;
	LONG err;

	if (!(buf = AllocVec(EDITPREFS_TEXTMAX, MEMF_ANY))) return ERROR_NO_FREE_STORE;
	if (!EditPrefs_Write(ep, buf, EDITPREFS_TEXTMAX)) {
		FreeVec(buf);
		return ERROR_NO_FREE_STORE;
	}
	if (from == SETTINGS_PROGDIR && GetProgramDir()) {
		err = write_file((CONST_STRPTR) "PROGDIR:" EDITPREFS_NAME, buf);
	} else {
		/* for good, and for now: ENV: is what the next start reads */
		err = write_file((CONST_STRPTR) "ENVARC:" EDITPREFS_NAME, buf);
		if (!err) err = write_file((CONST_STRPTR) "ENV:" EDITPREFS_NAME, buf);
	}
	FreeVec(buf);
	return err;
}
