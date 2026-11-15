/*
 * exec/types.h for the host tests: the Amiga's sizes on a 64-bit host
 * (LONG is 32 bits: long is 64 there).
 */
#ifndef EXEC_TYPES_H
#define EXEC_TYPES_H

#include <stddef.h>

typedef unsigned int ULONG;
typedef int LONG;
typedef unsigned short UWORD;
typedef short WORD;
typedef unsigned char UBYTE;
typedef signed char BYTE;
typedef short BOOL;
typedef char *STRPTR;
typedef const char *CONST_STRPTR;
typedef void *APTR;
typedef LONG BPTR;

#define TRUE	1
#define FALSE	0
#define VOID	void

#endif
