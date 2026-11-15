#ifndef _VBCCINLINE_PRISMADSP_H
#define _VBCCINLINE_PRISMADSP_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

struct PDSPJob * __PDSP_EncodeFileA(__reg("a6") struct Library *, __reg("a0") struct TagItem * tags)="\tjsr\t-30(a6)";
#define PDSP_EncodeFileA(tags) __PDSP_EncodeFileA(PrismaDSPBase, (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
struct PDSPJob * __PDSP_EncodeFile(__reg("a6") struct Library *, Tag tag1, ...)="\tmove.l\ta0,-(a7)\n\tlea\t4(a7),a0\n\tjsr\t-30(a6)\n\tmovea.l\t(a7)+,a0";
#define PDSP_EncodeFile(...) __PDSP_EncodeFile(PrismaDSPBase, __VA_ARGS__)
#endif

LONG __PDSP_AbortJob(__reg("a6") struct Library *, __reg("a0") struct PDSPJob * job)="\tjsr\t-36(a6)";
#define PDSP_AbortJob(job) __PDSP_AbortJob(PrismaDSPBase, (job))

ULONG __PDSP_GetJobAttr(__reg("a6") struct Library *, __reg("a0") struct PDSPJob * job, __reg("d0") ULONG attr)="\tjsr\t-42(a6)";
#define PDSP_GetJobAttr(job, attr) __PDSP_GetJobAttr(PrismaDSPBase, (job), (attr))

void __PDSP_DeleteJob(__reg("a6") struct Library *, __reg("a0") struct PDSPJob * job)="\tjsr\t-48(a6)";
#define PDSP_DeleteJob(job) __PDSP_DeleteJob(PrismaDSPBase, (job))

#endif /*  _VBCCINLINE_PRISMADSP_H  */
