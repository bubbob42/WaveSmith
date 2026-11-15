#ifndef _PROTO_PRISMADSP_H
#define _PROTO_PRISMADSP_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#if !defined(CLIB_PRISMADSP_PROTOS_H) && !defined(__GNUC__)
#include <clib/prismadsp_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *PrismaDSPBase;
#endif

#ifdef __GNUC__
#ifdef __AROS__
#include <defines/prismadsp.h>
#else
#include <inline/prismadsp.h>
#endif
#elif defined(__VBCC__)
#include <inline/prismadsp_protos.h>
#else
#include <pragma/prismadsp_lib.h>
#endif

#endif	/*  _PROTO_PRISMADSP_H  */
