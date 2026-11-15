#ifndef _PROTO_PRISMA_H
#define _PROTO_PRISMA_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#if !defined(CLIB_PRISMA_PROTOS_H) && !defined(__GNUC__)
#include <clib/prisma_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *PrismaBase;
#endif

#ifdef __GNUC__
#ifdef __AROS__
#include <defines/prisma.h>
#else
#include <inline/prisma.h>
#endif
#elif defined(__VBCC__)
#include <inline/prisma_protos.h>
#else
#include <pragma/prisma_lib.h>
#endif

#endif	/*  _PROTO_PRISMA_H  */
