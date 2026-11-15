/*
 * utility/hooks.h for the host tests: struct Hook's shape
 */
#ifndef UTILITY_HOOKS_H
#define UTILITY_HOOKS_H

#include <exec/types.h>

struct MinNode {
	struct MinNode *mln_Succ, *mln_Pred;
};

struct Hook {
	struct MinNode h_MinNode;
	ULONG (*h_Entry)(void);
	ULONG (*h_SubEntry)(void);
	APTR h_Data;
};

#endif
