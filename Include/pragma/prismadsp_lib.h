#ifndef _INCLUDE_PRAGMA_PRISMADSP_LIB_H
#define _INCLUDE_PRAGMA_PRISMADSP_LIB_H

#ifndef CLIB_PRISMADSP_PROTOS_H
#include <clib/prismadsp_protos.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(PrismaDSPBase,0x01e,PDSP_EncodeFileA(a0))
#pragma amicall(PrismaDSPBase,0x024,PDSP_AbortJob(a0))
#pragma amicall(PrismaDSPBase,0x02a,PDSP_GetJobAttr(a0,d0))
#pragma amicall(PrismaDSPBase,0x030,PDSP_DeleteJob(a0))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall PrismaDSPBase PDSP_EncodeFileA       01e 801
#pragma  libcall PrismaDSPBase PDSP_AbortJob          024 801
#pragma  libcall PrismaDSPBase PDSP_GetJobAttr        02a 0802
#pragma  libcall PrismaDSPBase PDSP_DeleteJob         030 801
#endif
#ifdef __SASC_60
#pragma  tagcall PrismaDSPBase PDSP_EncodeFile        01e 801
#endif

#endif	/*  _INCLUDE_PRAGMA_PRISMADSP_LIB_H  */
