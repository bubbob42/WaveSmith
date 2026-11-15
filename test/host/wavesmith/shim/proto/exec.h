/*
 * proto/exec.h for the host tests: exec's memory (doshost.c)
 */
#ifndef PROTO_EXEC_H
#define PROTO_EXEC_H

#include <exec/types.h>

APTR AllocVec(ULONG size, ULONG flags);
void FreeVec(APTR p);
void CopyMem(APTR from, APTR to, ULONG n);

#endif
