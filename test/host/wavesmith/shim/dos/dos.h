/*
 * dos/dos.h for the host tests of WaveSmith's doc.c: what it uses of
 * dos/dos.h, dos/dosextens.h and dos/dosasl.h, the values the NDK's
 */
#ifndef DOS_DOS_H
#define DOS_DOS_H

#include <exec/types.h>

#define RETURN_OK		0
#define RETURN_WARN		5
#define RETURN_ERROR		10
#define RETURN_FAIL		20

#define DOSTRUE			(-1L)
#define DOSFALSE		0L

#define MODE_OLDFILE		1005
#define MODE_NEWFILE		1006
#define MODE_READWRITE		1004

#define OFFSET_BEGINNING	(-1)
#define OFFSET_CURRENT		0
#define OFFSET_END		1

#define SHARED_LOCK		(-2)
#define ACCESS_READ		(-2)
#define EXCLUSIVE_LOCK		(-1)

#define DOS_FIB			2

#define ERROR_NO_FREE_STORE		103
#define ERROR_LINE_TOO_LONG		120
#define ERROR_OBJECT_IN_USE		202
#define ERROR_OBJECT_EXISTS		203
#define ERROR_OBJECT_NOT_FOUND		205
#define ERROR_OBJECT_WRONG_TYPE		212
#define ERROR_DISK_WRITE_PROTECTED	214
#define ERROR_SEEK_ERROR		219
#define ERROR_DISK_FULL			221
#define ERROR_DELETE_PROTECTED		222

struct DateStamp {
	LONG ds_Days;
	LONG ds_Minute;
	LONG ds_Tick;
};

struct FileInfoBlock {
	LONG fib_DiskKey;
	LONG fib_DirEntryType;
	char fib_FileName[108];
	LONG fib_Protection;
	LONG fib_EntryType;
	LONG fib_Size;
	LONG fib_NumBlocks;
	struct DateStamp fib_Date;
	char fib_Comment[80];
	UWORD fib_OwnerUID;
	UWORD fib_OwnerGID;
	char fib_Reserved[32];
};

#endif
