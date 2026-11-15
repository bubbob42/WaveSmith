/*
 * doshost.h -- dos.library and exec's memory on the host, for
 * test_docsave.c (doshost.c): files are the host's, under the names
 * given; what AmigaDOS refuses is refused (an open file deleted, a
 * rename over a file, a new file over an open one, Seek past the end);
 * a new file starts with no protection, comment or date. What is held
 * is counted, and the next call of a kind can be made to fail.
 */
#ifndef DOSHOST_H
#define DOSHOST_H

struct DosHost {
	long dh_Vecs;		/* AllocVec blocks held */
	long dh_Handles;	/* files open */
	long dh_Locks;		/* locks held */
	long dh_Objs;		/* AllocDosObject's held */
	long dh_WriteLeft;	/* bytes the disk still takes; -1: no end */
	int dh_FailDelete;	/* the next DeleteFile fails (delete-protected) */
	int dh_FailRename;	/* the next Rename fails (write-protected) */
	int dh_FailClose;	/* the next Close of a written file fails (disk full) */
	int dh_FailOpenOld;	/* the next MODE_OLDFILE Open fails (in use) */
	int dh_FailVecIn;	/* n > 0: the n-th AllocVec from now fails; 0: none */
};

extern struct DosHost dos_host;

/* the counters and the failures back to none */
void DosHost_Reset(void);

#endif
