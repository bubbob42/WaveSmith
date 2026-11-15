/*
 * proto/dos.h for the host tests: dos.library as doshost.c gives it
 */
#ifndef PROTO_DOS_H
#define PROTO_DOS_H

#include <exec/types.h>
#include <dos/dos.h>
#include <utility/tagitem.h>

BPTR Open(CONST_STRPTR name, LONG mode);
LONG Close(BPTR fh);
LONG Read(BPTR fh, APTR buf, LONG len);
LONG Write(BPTR fh, const void *buf, LONG len);
LONG Seek(BPTR fh, LONG pos, LONG mode);
LONG IoErr(void);
LONG SetIoErr(LONG code);
LONG DeleteFile(CONST_STRPTR name);
LONG Rename(CONST_STRPTR from, CONST_STRPTR to);
BPTR Lock(CONST_STRPTR name, LONG mode);
void UnLock(BPTR lock);
LONG Examine(BPTR lock, struct FileInfoBlock *fib);
LONG SetProtection(CONST_STRPTR name, LONG prot);
LONG SetComment(CONST_STRPTR name, CONST_STRPTR comment);
LONG SetFileDate(CONST_STRPTR name, const struct DateStamp *date);
APTR AllocDosObject(ULONG type, struct TagItem *tags);
void FreeDosObject(ULONG type, APTR obj);

#endif
