/*
 * exec/exec.h for the host tests: only what libraries/prisma.h needs
 * whole (it embeds these structures).
 */
#ifndef EXEC_EXEC_H
#define EXEC_EXEC_H

#include <exec/types.h>

struct Node {
	struct Node *ln_Succ, *ln_Pred;
	UBYTE ln_Type;
	BYTE ln_Pri;
	char *ln_Name;
};

struct List {
	struct Node *lh_Head, *lh_Tail, *lh_TailPred;
	UBYTE lh_Type;
	UBYTE l_pad;
};

struct MsgPort;
struct Task;
struct Process;
struct SignalSemaphore;

struct Message {
	struct Node mn_Node;
	struct MsgPort *mn_ReplyPort;
	UWORD mn_Length;
};

struct Library {
	struct Node lib_Node;
	UBYTE lib_Flags, lib_pad;
	UWORD lib_NegSize, lib_PosSize, lib_Version, lib_Revision;
	APTR lib_IdString;
	ULONG lib_Sum;
	UWORD lib_OpenCnt;
};

#endif
