#ifndef _INLINE_PRISMADSP_H
#define _INLINE_PRISMADSP_H

#ifndef CLIB_PRISMADSP_PROTOS_H
#define CLIB_PRISMADSP_PROTOS_H
#endif

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef  UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif
#ifndef  LIBRARIES_PRISMADSP_H
#include <libraries/prismadsp.h>
#endif

#ifndef PRISMADSP_BASE_NAME
#define PRISMADSP_BASE_NAME PrismaDSPBase
#endif

#define PDSP_EncodeFileA(tags) \
	LP1(0x1e, struct PDSPJob *, PDSP_EncodeFileA, struct TagItem *, tags, a0, \
	, PRISMADSP_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define PDSP_EncodeFile(...) \
	({ULONG _tags[] = {__VA_ARGS__}; PDSP_EncodeFileA((struct TagItem *) _tags);})
#endif /* !NO_INLINE_STDARG */

#define PDSP_AbortJob(job) \
	LP1(0x24, LONG, PDSP_AbortJob, struct PDSPJob *, job, a0, \
	, PRISMADSP_BASE_NAME)

#define PDSP_GetJobAttr(job, attr) \
	LP2(0x2a, ULONG, PDSP_GetJobAttr, struct PDSPJob *, job, a0, ULONG, attr, d0, \
	, PRISMADSP_BASE_NAME)

#define PDSP_DeleteJob(job) \
	LP1NR(0x30, PDSP_DeleteJob, struct PDSPJob *, job, a0, \
	, PRISMADSP_BASE_NAME)

#endif /*  _INLINE_PRISMADSP_H  */
