#ifndef _INCLUDE_PRAGMA_PRISMA_LIB_H
#define _INCLUDE_PRAGMA_PRISMA_LIB_H

#ifndef CLIB_PRISMA_PROTOS_H
#include <clib/prisma_protos.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(PrismaBase,0x01e,Prisma_ObtainCard())
#pragma amicall(PrismaBase,0x024,Prisma_ReleaseCard())
#pragma amicall(PrismaBase,0x02a,Prisma_CreatePlayer(a1))
#pragma amicall(PrismaBase,0x030,Prisma_DeletePlayer(a1))
#pragma amicall(PrismaBase,0x036,Prisma_TerminatePlayer(a1))
#pragma amicall(PrismaBase,0x03c,Prisma_Play(a1,d1))
#pragma amicall(PrismaBase,0x042,Prisma_Stop(a1))
#pragma amicall(PrismaBase,0x048,Prisma_Pause(a1))
#pragma amicall(PrismaBase,0x04e,Prisma_SetVolume(a1,d1))
#pragma amicall(PrismaBase,0x054,Prisma_SetPanning(a1,d1))
#pragma amicall(PrismaBase,0x05a,Prisma_SetSignal(a1,a2,d1))
#pragma amicall(PrismaBase,0x060,Prisma_CreateBuffer(a1,d1))
#pragma amicall(PrismaBase,0x066,Prisma_QueueBuffer(a1,a2))
#pragma amicall(PrismaBase,0x06c,Prisma_GetFreeBuffer(a1))
#pragma amicall(PrismaBase,0x072,Prisma_AddBuffer(a1,a2))
#pragma amicall(PrismaBase,0x078,Prisma_GetPlayerState(a1))
#pragma amicall(PrismaBase,0x07e,Prisma_SetEqualizer(a1,a2))
#pragma amicall(PrismaBase,0x084,Prisma_EnableEqualizer(a1,d1))
#pragma amicall(PrismaBase,0x0ae,Prisma_GetPlayerAttr(a1,d1))
#pragma amicall(PrismaBase,0x0b4,Prisma_SetPlayerAttr(a1,d1,d2))
#pragma amicall(PrismaBase,0x0d2,Prisma_GetCardAttr(a1,d1))
#pragma amicall(PrismaBase,0x0d8,Prisma_Record(a1,d1))
#pragma amicall(PrismaBase,0x0de,Prisma_QueueRecordBuffer(a1,a2))
#pragma amicall(PrismaBase,0x0e4,Prisma_GetRecordBuffer(a1))
#pragma amicall(PrismaBase,0x0ea,Prisma_SetCardAttr(a1,d1,d2))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall PrismaBase Prisma_ObtainCard      01e 00
#pragma  libcall PrismaBase Prisma_ReleaseCard     024 00
#pragma  libcall PrismaBase Prisma_CreatePlayer    02a 901
#pragma  libcall PrismaBase Prisma_DeletePlayer    030 901
#pragma  libcall PrismaBase Prisma_TerminatePlayer 036 901
#pragma  libcall PrismaBase Prisma_Play            03c 1902
#pragma  libcall PrismaBase Prisma_Stop            042 901
#pragma  libcall PrismaBase Prisma_Pause           048 901
#pragma  libcall PrismaBase Prisma_SetVolume       04e 1902
#pragma  libcall PrismaBase Prisma_SetPanning      054 1902
#pragma  libcall PrismaBase Prisma_SetSignal       05a 1a903
#pragma  libcall PrismaBase Prisma_CreateBuffer    060 1902
#pragma  libcall PrismaBase Prisma_QueueBuffer     066 a902
#pragma  libcall PrismaBase Prisma_GetFreeBuffer   06c 901
#pragma  libcall PrismaBase Prisma_AddBuffer       072 a902
#pragma  libcall PrismaBase Prisma_GetPlayerState  078 901
#pragma  libcall PrismaBase Prisma_SetEqualizer    07e a902
#pragma  libcall PrismaBase Prisma_EnableEqualizer 084 1902
#pragma  libcall PrismaBase Prisma_GetPlayerAttr   0ae 1902
#pragma  libcall PrismaBase Prisma_SetPlayerAttr   0b4 21903
#pragma  libcall PrismaBase Prisma_GetCardAttr     0d2 1902
#pragma  libcall PrismaBase Prisma_Record          0d8 1902
#pragma  libcall PrismaBase Prisma_QueueRecordBuffer 0de a902
#pragma  libcall PrismaBase Prisma_GetRecordBuffer 0e4 901
#pragma  libcall PrismaBase Prisma_SetCardAttr     0ea 21903
#endif

#endif	/*  _INCLUDE_PRAGMA_PRISMA_LIB_H  */
