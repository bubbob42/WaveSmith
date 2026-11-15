#ifndef CLIB_PRISMA_PROTOS_H
#define CLIB_PRISMA_PROTOS_H

/*
 * prisma.library protos
 *
 * (c) 2016-2026 Christian Euler & Marcus Gerards
 */

#include <exec/types.h>
#include <dos/dos.h>

#include <libraries/prisma.h>

struct PrismaCard *Prisma_ObtainCard(void);
void Prisma_ReleaseCard(void);
struct PrismaPlayer *Prisma_CreatePlayer(struct MsgPort *msgport);
void Prisma_DeletePlayer(struct PrismaPlayer *pplayer);
UWORD Prisma_TerminatePlayer(struct PrismaPlayer *pplayer);
UWORD Prisma_Play(struct PrismaPlayer *pplayer, BPTR fh);
UWORD Prisma_Stop(struct PrismaPlayer *pplayer);
UWORD Prisma_Pause(struct PrismaPlayer *pplayer);
void Prisma_SetVolume(struct PrismaPlayer *pplayer, UWORD volume);
void Prisma_SetPanning(struct PrismaPlayer *pplayer, UWORD panning);
void Prisma_SetSignal(struct PrismaPlayer *pplayer, struct Task *task, ULONG mask);
struct ReplayBuffer *Prisma_CreateBuffer(struct PrismaPlayer *pplayer, ULONG size);
void Prisma_QueueBuffer(struct PrismaPlayer *pplayer, struct ReplayBuffer *buffer);
struct ReplayBuffer *Prisma_GetFreeBuffer(struct PrismaPlayer *pplayer);
void Prisma_AddBuffer(struct PrismaPlayer *pplayer, struct ReplayBuffer *buffer);
UWORD Prisma_GetPlayerState(struct PrismaPlayer *pplayer);
void Prisma_SetEqualizer(struct PrismaPlayer *pplayer, struct PrismaEqualizer *pequalizer);
void Prisma_EnableEqualizer(struct PrismaPlayer *pplayer, BOOL enable);

/* added after the 2019 release */
ULONG Prisma_GetPlayerAttr(struct PrismaPlayer *pplayer, ULONG attr);
BOOL Prisma_SetPlayerAttr(struct PrismaPlayer *pplayer, ULONG attr, ULONG value);
ULONG Prisma_GetCardAttr(struct PrismaCard *card, ULONG attr);
UWORD Prisma_Record(struct PrismaPlayer *pplayer, BPTR fh);
void Prisma_QueueRecordBuffer(struct PrismaPlayer *pplayer, struct ReplayBuffer *buffer);
struct ReplayBuffer *Prisma_GetRecordBuffer(struct PrismaPlayer *pplayer);
BOOL Prisma_SetCardAttr(struct PrismaCard *card, ULONG attr, ULONG value);

#endif
