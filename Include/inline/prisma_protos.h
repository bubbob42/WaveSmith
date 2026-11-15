#ifndef _VBCCINLINE_PRISMA_H
#define _VBCCINLINE_PRISMA_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

struct PrismaCard * __Prisma_ObtainCard(__reg("a6") struct Library *)="\tjsr\t-30(a6)";
#define Prisma_ObtainCard() __Prisma_ObtainCard(PrismaBase)

void __Prisma_ReleaseCard(__reg("a6") struct Library *)="\tjsr\t-36(a6)";
#define Prisma_ReleaseCard() __Prisma_ReleaseCard(PrismaBase)

struct PrismaPlayer * __Prisma_CreatePlayer(__reg("a6") struct Library *, __reg("a1") struct MsgPort * msgport)="\tjsr\t-42(a6)";
#define Prisma_CreatePlayer(msgport) __Prisma_CreatePlayer(PrismaBase, (msgport))

void __Prisma_DeletePlayer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-48(a6)";
#define Prisma_DeletePlayer(pplayer) __Prisma_DeletePlayer(PrismaBase, (pplayer))

UWORD __Prisma_TerminatePlayer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-54(a6)";
#define Prisma_TerminatePlayer(pplayer) __Prisma_TerminatePlayer(PrismaBase, (pplayer))

UWORD __Prisma_Play(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") BPTR fh)="\tjsr\t-60(a6)";
#define Prisma_Play(pplayer, fh) __Prisma_Play(PrismaBase, (pplayer), (fh))

UWORD __Prisma_Stop(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-66(a6)";
#define Prisma_Stop(pplayer) __Prisma_Stop(PrismaBase, (pplayer))

UWORD __Prisma_Pause(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-72(a6)";
#define Prisma_Pause(pplayer) __Prisma_Pause(PrismaBase, (pplayer))

void __Prisma_SetVolume(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") UWORD volume)="\tjsr\t-78(a6)";
#define Prisma_SetVolume(pplayer, volume) __Prisma_SetVolume(PrismaBase, (pplayer), (volume))

void __Prisma_SetPanning(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") UWORD panning)="\tjsr\t-84(a6)";
#define Prisma_SetPanning(pplayer, panning) __Prisma_SetPanning(PrismaBase, (pplayer), (panning))

void __Prisma_SetSignal(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("a2") struct Task * task, __reg("d1") ULONG mask)="\tjsr\t-90(a6)";
#define Prisma_SetSignal(pplayer, task, mask) __Prisma_SetSignal(PrismaBase, (pplayer), (task), (mask))

struct ReplayBuffer * __Prisma_CreateBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") ULONG size)="\tjsr\t-96(a6)";
#define Prisma_CreateBuffer(pplayer, size) __Prisma_CreateBuffer(PrismaBase, (pplayer), (size))

void __Prisma_QueueBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("a2") struct ReplayBuffer * buffer)="\tjsr\t-102(a6)";
#define Prisma_QueueBuffer(pplayer, buffer) __Prisma_QueueBuffer(PrismaBase, (pplayer), (buffer))

struct ReplayBuffer * __Prisma_GetFreeBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-108(a6)";
#define Prisma_GetFreeBuffer(pplayer) __Prisma_GetFreeBuffer(PrismaBase, (pplayer))

void __Prisma_AddBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("a2") struct ReplayBuffer * buffer)="\tjsr\t-114(a6)";
#define Prisma_AddBuffer(pplayer, buffer) __Prisma_AddBuffer(PrismaBase, (pplayer), (buffer))

UWORD __Prisma_GetPlayerState(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-120(a6)";
#define Prisma_GetPlayerState(pplayer) __Prisma_GetPlayerState(PrismaBase, (pplayer))

void __Prisma_SetEqualizer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("a2") struct PrismaEqualizer * pequalizer)="\tjsr\t-126(a6)";
#define Prisma_SetEqualizer(pplayer, pequalizer) __Prisma_SetEqualizer(PrismaBase, (pplayer), (pequalizer))

void __Prisma_EnableEqualizer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") BOOL enable)="\tjsr\t-132(a6)";
#define Prisma_EnableEqualizer(pplayer, enable) __Prisma_EnableEqualizer(PrismaBase, (pplayer), (enable))

ULONG __Prisma_GetPlayerAttr(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") ULONG attr)="\tjsr\t-174(a6)";
#define Prisma_GetPlayerAttr(pplayer, attr) __Prisma_GetPlayerAttr(PrismaBase, (pplayer), (attr))

BOOL __Prisma_SetPlayerAttr(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") ULONG attr, __reg("d2") ULONG value)="\tjsr\t-180(a6)";
#define Prisma_SetPlayerAttr(pplayer, attr, value) __Prisma_SetPlayerAttr(PrismaBase, (pplayer), (attr), (value))

ULONG __Prisma_GetCardAttr(__reg("a6") struct Library *, __reg("a1") struct PrismaCard * card, __reg("d1") ULONG attr)="\tjsr\t-210(a6)";
#define Prisma_GetCardAttr(card, attr) __Prisma_GetCardAttr(PrismaBase, (card), (attr))

UWORD __Prisma_Record(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("d1") BPTR fh)="\tjsr\t-216(a6)";
#define Prisma_Record(pplayer, fh) __Prisma_Record(PrismaBase, (pplayer), (fh))

void __Prisma_QueueRecordBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer, __reg("a2") struct ReplayBuffer * buffer)="\tjsr\t-222(a6)";
#define Prisma_QueueRecordBuffer(pplayer, buffer) __Prisma_QueueRecordBuffer(PrismaBase, (pplayer), (buffer))

struct ReplayBuffer * __Prisma_GetRecordBuffer(__reg("a6") struct Library *, __reg("a1") struct PrismaPlayer * pplayer)="\tjsr\t-228(a6)";
#define Prisma_GetRecordBuffer(pplayer) __Prisma_GetRecordBuffer(PrismaBase, (pplayer))

BOOL __Prisma_SetCardAttr(__reg("a6") struct Library *, __reg("a1") struct PrismaCard * card, __reg("d1") ULONG attr, __reg("d2") ULONG value)="\tjsr\t-234(a6)";
#define Prisma_SetCardAttr(card, attr, value) __Prisma_SetCardAttr(PrismaBase, (card), (attr), (value))

#endif /*  _VBCCINLINE_PRISMA_H  */
