#ifndef _INLINE_PRISMA_H
#define _INLINE_PRISMA_H

#ifndef CLIB_PRISMA_PROTOS_H
#define CLIB_PRISMA_PROTOS_H
#endif

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef  DOS_DOS_H
#include <dos/dos.h>
#endif
#ifndef  LIBRARIES_PRISMA_H
#include <libraries/prisma.h>
#endif

#ifndef PRISMA_BASE_NAME
#define PRISMA_BASE_NAME PrismaBase
#endif

#define Prisma_ObtainCard() \
	LP0(0x1e, struct PrismaCard *, Prisma_ObtainCard, \
	, PRISMA_BASE_NAME)

#define Prisma_ReleaseCard() \
	LP0NR(0x24, Prisma_ReleaseCard, \
	, PRISMA_BASE_NAME)

#define Prisma_CreatePlayer(msgport) \
	LP1(0x2a, struct PrismaPlayer *, Prisma_CreatePlayer, struct MsgPort *, msgport, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_DeletePlayer(pplayer) \
	LP1NR(0x30, Prisma_DeletePlayer, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_TerminatePlayer(pplayer) \
	LP1(0x36, UWORD, Prisma_TerminatePlayer, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_Play(pplayer, fh) \
	LP2(0x3c, UWORD, Prisma_Play, struct PrismaPlayer *, pplayer, a1, BPTR, fh, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_Stop(pplayer) \
	LP1(0x42, UWORD, Prisma_Stop, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_Pause(pplayer) \
	LP1(0x48, UWORD, Prisma_Pause, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetVolume(pplayer, volume) \
	LP2NR(0x4e, Prisma_SetVolume, struct PrismaPlayer *, pplayer, a1, UWORD, volume, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetPanning(pplayer, panning) \
	LP2NR(0x54, Prisma_SetPanning, struct PrismaPlayer *, pplayer, a1, UWORD, panning, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetSignal(pplayer, task, mask) \
	LP3NR(0x5a, Prisma_SetSignal, struct PrismaPlayer *, pplayer, a1, struct Task *, task, a2, ULONG, mask, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_CreateBuffer(pplayer, size) \
	LP2(0x60, struct ReplayBuffer *, Prisma_CreateBuffer, struct PrismaPlayer *, pplayer, a1, ULONG, size, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_QueueBuffer(pplayer, buffer) \
	LP2NR(0x66, Prisma_QueueBuffer, struct PrismaPlayer *, pplayer, a1, struct ReplayBuffer *, buffer, a2, \
	, PRISMA_BASE_NAME)

#define Prisma_GetFreeBuffer(pplayer) \
	LP1(0x6c, struct ReplayBuffer *, Prisma_GetFreeBuffer, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_AddBuffer(pplayer, buffer) \
	LP2NR(0x72, Prisma_AddBuffer, struct PrismaPlayer *, pplayer, a1, struct ReplayBuffer *, buffer, a2, \
	, PRISMA_BASE_NAME)

#define Prisma_GetPlayerState(pplayer) \
	LP1(0x78, UWORD, Prisma_GetPlayerState, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetEqualizer(pplayer, pequalizer) \
	LP2NR(0x7e, Prisma_SetEqualizer, struct PrismaPlayer *, pplayer, a1, struct PrismaEqualizer *, pequalizer, a2, \
	, PRISMA_BASE_NAME)

#define Prisma_EnableEqualizer(pplayer, enable) \
	LP2NR(0x84, Prisma_EnableEqualizer, struct PrismaPlayer *, pplayer, a1, BOOL, enable, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_GetPlayerAttr(pplayer, attr) \
	LP2(0xae, ULONG, Prisma_GetPlayerAttr, struct PrismaPlayer *, pplayer, a1, ULONG, attr, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetPlayerAttr(pplayer, attr, value) \
	LP3(0xb4, BOOL, Prisma_SetPlayerAttr, struct PrismaPlayer *, pplayer, a1, ULONG, attr, d1, ULONG, value, d2, \
	, PRISMA_BASE_NAME)

#define Prisma_GetCardAttr(card, attr) \
	LP2(0xd2, ULONG, Prisma_GetCardAttr, struct PrismaCard *, card, a1, ULONG, attr, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_Record(pplayer, fh) \
	LP2(0xd8, UWORD, Prisma_Record, struct PrismaPlayer *, pplayer, a1, BPTR, fh, d1, \
	, PRISMA_BASE_NAME)

#define Prisma_QueueRecordBuffer(pplayer, buffer) \
	LP2NR(0xde, Prisma_QueueRecordBuffer, struct PrismaPlayer *, pplayer, a1, struct ReplayBuffer *, buffer, a2, \
	, PRISMA_BASE_NAME)

#define Prisma_GetRecordBuffer(pplayer) \
	LP1(0xe4, struct ReplayBuffer *, Prisma_GetRecordBuffer, struct PrismaPlayer *, pplayer, a1, \
	, PRISMA_BASE_NAME)

#define Prisma_SetCardAttr(card, attr, value) \
	LP3(0xea, BOOL, Prisma_SetCardAttr, struct PrismaCard *, card, a1, ULONG, attr, d1, ULONG, value, d2, \
	, PRISMA_BASE_NAME)

#endif /*  _INLINE_PRISMA_H  */
