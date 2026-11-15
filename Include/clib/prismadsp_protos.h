#ifndef CLIB_PRISMADSP_PROTOS_H
#define CLIB_PRISMADSP_PROTOS_H

/*
 * prismadsp.library protos
 *
 * (c) 2016-2026 Christian Euler & Marcus Gerards
 */

#include <exec/types.h>
#include <utility/tagitem.h>

#include <libraries/prismadsp.h>

struct PDSPJob *PDSP_EncodeFileA(struct TagItem *tags);
struct PDSPJob *PDSP_EncodeFile(Tag tag1, ...);
LONG PDSP_AbortJob(struct PDSPJob *job);
ULONG PDSP_GetJobAttr(struct PDSPJob *job, ULONG attr);
void PDSP_DeleteJob(struct PDSPJob *job);

#endif
