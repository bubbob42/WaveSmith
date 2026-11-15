/*
 * doshost.c -- dos.library and exec's memory on the host (doshost.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "doshost.h"

#define MAX_FILES	16
#define MAX_LOCKS	16
#define MAX_META	32
#define NAME_LEN	512

struct DosHost dos_host;

static struct {
	FILE *f;
	char name[NAME_LEN];
	int writing;
} files[MAX_FILES];

static char locks[MAX_LOCKS][NAME_LEN];

/* what a file is besides its bytes, by name */
static struct {
	char name[NAME_LEN];
	LONG prot;
	char comment[80];
	struct DateStamp date;
} meta[MAX_META];

static LONG ioerr = 0;

void DosHost_Reset(void)
{
	memset(&dos_host, 0, sizeof(dos_host));
	dos_host.dh_WriteLeft = -1;
	ioerr = 0;
}

static void copy_name(char *to, CONST_STRPTR from)
{
	strncpy(to, (const char *) from, NAME_LEN - 1);
	to[NAME_LEN - 1] = '\0';
}

static int is_dir(CONST_STRPTR name)
{
	struct stat st;

	return stat((const char *) name, &st) == 0 && S_ISDIR(st.st_mode);
}

static int exists(CONST_STRPTR name)
{
	struct stat st;

	return stat((const char *) name, &st) == 0;
}

/* the handle open on name, writing or (writing 0) any: its index + 1 */
static int open_on(CONST_STRPTR name, int writing)
{
	int i;

	for (i = 0; i < MAX_FILES; i++) {
		if (files[i].f && strcmp(files[i].name, (const char *) name) == 0 && (!writing || files[i].writing))
			return i + 1;
	}
	return 0;
}

static int meta_of(CONST_STRPTR name, int make)
{
	int i, free_one = -1;

	for (i = 0; i < MAX_META; i++) {
		if (meta[i].name[0] && strcmp(meta[i].name, (const char *) name) == 0) return i;
		if (!meta[i].name[0] && free_one < 0) free_one = i;
	}
	if (!make || free_one < 0) return -1;
	memset(&meta[free_one], 0, sizeof(meta[free_one]));
	copy_name(meta[free_one].name, name);
	return free_one;
}

static void meta_forget(CONST_STRPTR name)
{
	int i = meta_of(name, 0);

	if (i >= 0) meta[i].name[0] = '\0';
}

APTR AllocVec(ULONG size, ULONG flags)
{
	APTR p;

	(void) flags;
	if (dos_host.dh_FailVecIn > 0 && --dos_host.dh_FailVecIn == 0) return NULL;
	p = calloc(1, size ? size : 1);
	if (p) dos_host.dh_Vecs++;
	return p;
}

void FreeVec(APTR p)
{
	if (p) dos_host.dh_Vecs--;
	free(p);
}

/* (exec's: no overlap promised -- memcpy, so a host sanitizer would say) */
void CopyMem(APTR from, APTR to, ULONG n)
{
	memcpy(to, from, n);
}

LONG IoErr(void)
{
	return ioerr;
}

LONG SetIoErr(LONG code)
{
	LONG old = ioerr;

	ioerr = code;
	return old;
}

static LONG fail(LONG code)
{
	ioerr = code;
	return 0;
}

BPTR Open(CONST_STRPTR name, LONG mode)
{
	FILE *f = NULL;
	int i;

	for (i = 0; i < MAX_FILES && files[i].f; i++) ;
	if (i == MAX_FILES) return (BPTR) fail(ERROR_NO_FREE_STORE);
	if (is_dir(name)) return (BPTR) fail(ERROR_OBJECT_WRONG_TYPE);
	switch (mode) {
	case MODE_OLDFILE:
		if (dos_host.dh_FailOpenOld) {
			dos_host.dh_FailOpenOld = 0;
			return (BPTR) fail(ERROR_OBJECT_IN_USE);
		}
		if (open_on(name, 1)) return (BPTR) fail(ERROR_OBJECT_IN_USE);
		f = fopen((const char *) name, "rb");
		break;
	case MODE_NEWFILE:
		/* an exclusive lock: refused while the file is open */
		if (open_on(name, 0)) return (BPTR) fail(ERROR_OBJECT_IN_USE);
		f = fopen((const char *) name, "w+b");
		if (f) meta_forget(name);
		break;
	default:
		f = fopen((const char *) name, "r+b");
		if (!f) f = fopen((const char *) name, "w+b");
		break;
	}
	if (!f) return (BPTR) fail(ERROR_OBJECT_NOT_FOUND);
	files[i].f = f;
	copy_name(files[i].name, name);
	files[i].writing = mode != MODE_OLDFILE;
	dos_host.dh_Handles++;
	return (BPTR) (i + 1);
}

static FILE *file_of(BPTR fh)
{
	if (fh < 1 || fh > MAX_FILES) return NULL;
	return files[fh - 1].f;
}

LONG Close(BPTR fh)
{
	FILE *f = file_of(fh);
	int bad;

	if (!f) return DOSTRUE;
	/* a Close that fails lets go of the handle all the same */
	bad = files[fh - 1].writing && dos_host.dh_FailClose;
	if (bad) dos_host.dh_FailClose = 0;
	fclose(f);
	files[fh - 1].f = NULL;
	dos_host.dh_Handles--;
	return bad ? fail(ERROR_DISK_FULL) : DOSTRUE;
}

LONG Read(BPTR fh, APTR buf, LONG len)
{
	FILE *f = file_of(fh);
	size_t n;

	if (!f || len < 0) return -1;
	n = fread(buf, 1, (size_t) len, f);
	if (ferror(f)) {
		ioerr = ERROR_SEEK_ERROR;
		return -1;
	}
	return (LONG) n;
}

LONG Write(BPTR fh, const void *buf, LONG len)
{
	FILE *f = file_of(fh);
	LONG n = len;

	if (!f || len < 0 || !files[fh - 1].writing) return -1;
	/* a full disk: what fits written, then -1 */
	if (dos_host.dh_WriteLeft >= 0 && len > dos_host.dh_WriteLeft) n = (LONG) dos_host.dh_WriteLeft;
	if (n && fwrite(buf, 1, (size_t) n, f) != (size_t) n) return -1;
	if (dos_host.dh_WriteLeft >= 0) dos_host.dh_WriteLeft -= n;
	if (n < len) {
		ioerr = ERROR_DISK_FULL;
		return -1;
	}
	return len;
}

/* the position before; past either end refused, as AmigaDOS does */
LONG Seek(BPTR fh, LONG pos, LONG mode)
{
	FILE *f = file_of(fh);
	long old, size, to;

	if (!f) return -1;
	old = ftell(f);
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	to = mode == OFFSET_BEGINNING ? pos : mode == OFFSET_END ? size + pos : old + pos;
	if (to < 0 || to > size) {
		fseek(f, old, SEEK_SET);
		ioerr = ERROR_SEEK_ERROR;
		return -1;
	}
	fseek(f, to, SEEK_SET);
	return (LONG) old;
}

LONG DeleteFile(CONST_STRPTR name)
{
	if (open_on(name, 0)) return fail(ERROR_OBJECT_IN_USE);
	if (dos_host.dh_FailDelete) {
		dos_host.dh_FailDelete = 0;
		return fail(ERROR_DELETE_PROTECTED);
	}
	/* remove() takes an empty directory too, as DeleteFile does */
	if (remove((const char *) name) != 0) return fail(ERROR_OBJECT_NOT_FOUND);
	meta_forget(name);
	return DOSTRUE;
}

LONG Rename(CONST_STRPTR from, CONST_STRPTR to)
{
	int i;

	if (dos_host.dh_FailRename) {
		dos_host.dh_FailRename = 0;
		return fail(ERROR_DISK_WRITE_PROTECTED);
	}
	if (!exists(from)) return fail(ERROR_OBJECT_NOT_FOUND);
	if (exists(to)) return fail(ERROR_OBJECT_EXISTS);
	if (rename((const char *) from, (const char *) to) != 0) return fail(ERROR_OBJECT_NOT_FOUND);
	meta_forget(to);
	if ((i = meta_of(from, 0)) >= 0) copy_name(meta[i].name, to);
	return DOSTRUE;
}

BPTR Lock(CONST_STRPTR name, LONG mode)
{
	int i;

	(void) mode;
	if (!exists(name)) return (BPTR) fail(ERROR_OBJECT_NOT_FOUND);
	for (i = 0; i < MAX_LOCKS && locks[i][0]; i++) ;
	if (i == MAX_LOCKS) return (BPTR) fail(ERROR_NO_FREE_STORE);
	copy_name(locks[i], name);
	dos_host.dh_Locks++;
	return (BPTR) (i + 1);
}

void UnLock(BPTR lock)
{
	if (lock < 1 || lock > MAX_LOCKS || !locks[lock - 1][0]) return;
	locks[lock - 1][0] = '\0';
	dos_host.dh_Locks--;
}

LONG Examine(BPTR lock, struct FileInfoBlock *fib)
{
	struct stat st;
	const char *name;
	int m;

	if (lock < 1 || lock > MAX_LOCKS || !locks[lock - 1][0]) return fail(ERROR_OBJECT_NOT_FOUND);
	name = locks[lock - 1];
	if (stat(name, &st) != 0) return fail(ERROR_OBJECT_NOT_FOUND);
	memset(fib, 0, sizeof(*fib));
	fib->fib_DirEntryType = S_ISDIR(st.st_mode) ? 2 : -3;
	fib->fib_EntryType = fib->fib_DirEntryType;
	fib->fib_Size = (LONG) st.st_size;
	strncpy(fib->fib_FileName, strrchr(name, '/') ? strrchr(name, '/') + 1 : name, sizeof(fib->fib_FileName) - 1);
	if ((m = meta_of((CONST_STRPTR) name, 0)) >= 0) {
		fib->fib_Protection = meta[m].prot;
		fib->fib_Date = meta[m].date;
		memcpy(fib->fib_Comment, meta[m].comment, sizeof(fib->fib_Comment));
	}
	return DOSTRUE;
}

LONG SetProtection(CONST_STRPTR name, LONG prot)
{
	int m;

	if (!exists(name) || (m = meta_of(name, 1)) < 0) return fail(ERROR_OBJECT_NOT_FOUND);
	meta[m].prot = prot;
	return DOSTRUE;
}

LONG SetComment(CONST_STRPTR name, CONST_STRPTR comment)
{
	int m;

	if (!exists(name) || (m = meta_of(name, 1)) < 0) return fail(ERROR_OBJECT_NOT_FOUND);
	strncpy(meta[m].comment, (const char *) comment, sizeof(meta[m].comment) - 1);
	return DOSTRUE;
}

LONG SetFileDate(CONST_STRPTR name, const struct DateStamp *date)
{
	int m;

	if (!exists(name) || (m = meta_of(name, 1)) < 0) return fail(ERROR_OBJECT_NOT_FOUND);
	meta[m].date = *date;
	return DOSTRUE;
}

APTR AllocDosObject(ULONG type, struct TagItem *tags)
{
	APTR p;

	(void) tags;
	if (type != DOS_FIB) return NULL;
	if ((p = calloc(1, sizeof(struct FileInfoBlock)))) dos_host.dh_Objs++;
	return p;
}

void FreeDosObject(ULONG type, APTR obj)
{
	(void) type;
	if (obj) dos_host.dh_Objs--;
	free(obj);
}
