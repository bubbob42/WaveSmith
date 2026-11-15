/*
 * decode.c -- a file made a 16-bit PCM WAV, once (decode.h)
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <libraries/prismadsp.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/prismadsp.h>

#include "decode.h"
#include "doc.h"
#include "textutil.h"

#define DSP_REVISION	1		/* 1.1: DecodeOnly of what the chip decodes */

struct Library *PrismaDSPBase = NULL;

static struct MsgPort *port = NULL;
static struct PDSPJob *job = NULL;
static UWORD state = DECODE_FAILED;
static LONG err = 0;
static ULONG done = 0, total = 0;
static char work[DOC_PATHLEN];

/* the library from LIBS:, else from the drawer beside the program's
   (PROGDIR:/Libs, as the test builds lie) -- PrismaEncode's way */
BOOL Decode_Open(void)
{
	static const char *const names[2] = { PRISMADSPNAME, "PROGDIR:/Libs/" PRISMADSPNAME };
	UWORD i;

	if (PrismaDSPBase) return TRUE;		/* again, when a file needs it */
	for (i = 0; i < 2 && !PrismaDSPBase; i++) {
		if (!(PrismaDSPBase = OpenLibrary((STRPTR) names[i], PRISMADSP_VERSION))) continue;
		if (PrismaDSPBase->lib_Version == PRISMADSP_VERSION && PrismaDSPBase->lib_Revision < DSP_REVISION) {
			CloseLibrary(PrismaDSPBase);
			PrismaDSPBase = NULL;
		}
	}
	if (PrismaDSPBase && !(port = CreateMsgPort())) {
		CloseLibrary(PrismaDSPBase);
		PrismaDSPBase = NULL;
	}
	return (BOOL) (PrismaDSPBase != NULL);
}

void Decode_Close(void)
{
	Decode_Forget();
	if (port) DeleteMsgPort(port);
	port = NULL;
	if (PrismaDSPBase) CloseLibrary(PrismaDSPBase);
	PrismaDSPBase = NULL;
}

BOOL Decode_There(void)
{
	return (BOOL) (PrismaDSPBase != NULL);
}

ULONG Decode_Signals(void)
{
	return port ? 1UL << port->mp_SigBit : 0;
}

CONST_STRPTR Decode_Work(void)
{
	return (CONST_STRPTR) work;
}

LONG Decode_Error(void)
{
	return err;
}

UWORD Decode_Percent(void)
{
	if (!total) return 0;
	return (UWORD) (total >= 100 ? (done / (total / 100) > 100 ? 100 : done / (total / 100)) : done * 100 / total);
}

UWORD Decode_Start(CONST_STRPTR from)
{
	struct TagItem tags[8];
	STRPTR p;

	Decode_Forget();
	err = -1;
	state = DECODE_FAILED;
	if (!PrismaDSPBase) return state;
	if (Text_Length(from) + Text_Length((CONST_STRPTR) DECODE_SUFFIX) >= DOC_PATHLEN) {
		err = PDSPERR_ARGS;
		return state;
	}
	p = Text_Put((STRPTR) work, from);
	Text_Put(p, (CONST_STRPTR) DECODE_SUFFIX);
	tags[0].ti_Tag = PDSPA_From;
	tags[0].ti_Data = (ULONG) from;
	tags[1].ti_Tag = PDSPA_To;
	tags[1].ti_Data = (ULONG) work;
	tags[2].ti_Tag = PDSPA_DecodeOnly;
	tags[2].ti_Data = TRUE;
	tags[3].ti_Tag = PDSPA_ReplyPort;
	tags[3].ti_Data = (ULONG) port;
	tags[4].ti_Tag = PDSPA_Error;
	tags[4].ti_Data = (ULONG) &err;
	tags[5].ti_Tag = PDSPA_Resample;
	tags[5].ti_Data = PDSPRS_GOOD;
	tags[6].ti_Tag = TAG_DONE;
	tags[6].ti_Data = 0;
	err = PDSPERR_OK;
	done = total = 0;
	if (!(job = PDSP_EncodeFileA(tags))) {
		DeleteFile((STRPTR) work);
		work[0] = '\0';
		return state;
	}
	state = DECODE_RUNNING;
	return state;
}

UWORD Decode_Poll(void)
{
	struct PDSPMessage *m;

	if (!job || !port) return state;
	while ((m = (struct PDSPMessage *) GetMsg(port))) {
		if (m->pdm_FramesTotal) {
			done = m->pdm_FramesDone;
			total = m->pdm_FramesTotal;
		}
		if (m->pdm_Code == PDSPMSG_END) {
			err = m->pdm_Error;
			state = err == PDSPERR_OK ? DECODE_DONE : DECODE_FAILED;
		}
		ReplyMsg(&m->pdm_Message);
	}
	/* a file the chip decodes: its bytes in so far, of the file's */
	if (state == DECODE_RUNNING && PDSP_GetJobAttr(job, PDSPJA_State) == PDSPJS_DECODING &&
	    PDSP_GetJobAttr(job, PDSPJA_BytesTotal)) {
		done = PDSP_GetJobAttr(job, PDSPJA_BytesIn);
		total = PDSP_GetJobAttr(job, PDSPJA_BytesTotal);
	}
	if (state != DECODE_RUNNING) {
		PDSP_DeleteJob(job);
		job = NULL;
		if (state == DECODE_FAILED && work[0]) {
			DeleteFile((STRPTR) work);
			work[0] = '\0';
		}
	}
	return state;
}

void Decode_Forget(void)
{
	struct PDSPMessage *m;

	if (job) {
		/* the job aborted; its END comes, then it may go */
		PDSP_AbortJob(job);
		while (state == DECODE_RUNNING) {
			WaitPort(port);
			while ((m = (struct PDSPMessage *) GetMsg(port))) {
				if (m->pdm_Code == PDSPMSG_END) state = DECODE_FAILED;
				ReplyMsg(&m->pdm_Message);
			}
		}
		PDSP_DeleteJob(job);
		job = NULL;
	}
	if (work[0]) DeleteFile((STRPTR) work);
	work[0] = '\0';
	state = DECODE_FAILED;
}
