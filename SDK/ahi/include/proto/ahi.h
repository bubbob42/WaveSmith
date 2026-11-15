#ifndef PROTO_AHI_H
#define PROTO_AHI_H

/*
 * ahi.device's functions for both compilers: the AHI 4.18 kit's SAS/C
 * pragmas, or the GCC inlines generated from its fd and clib files.
 */

#include <exec/types.h>
#include <clib/ahi_protos.h>

extern struct Library *AHIBase;

#ifdef __GNUC__
#include <inline/ahi.h>
#else
#include <pragmas/ahi_pragmas.h>
#endif

#endif
