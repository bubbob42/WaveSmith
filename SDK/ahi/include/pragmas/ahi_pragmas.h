#ifndef PRAGMAS_AHI_PRAGMAS_H
#define PRAGMAS_AHI_PRAGMAS_H

#ifndef CLIB_AHI_PROTOS_H
#include <clib/ahi_protos.h>
#endif

#pragma libcall AHIBase AHI_AllocAudioA      02A 901
#pragma libcall AHIBase AHI_FreeAudio        030 A01
#pragma libcall AHIBase AHI_KillAudio        036 00
#pragma libcall AHIBase AHI_ControlAudioA    03C 9A02
#pragma libcall AHIBase AHI_SetVol           042 3A21005
#pragma libcall AHIBase AHI_SetFreq          048 2A1004
#pragma libcall AHIBase AHI_SetSound         04E 4A321006
#pragma libcall AHIBase AHI_SetEffect        054 A802
#pragma libcall AHIBase AHI_LoadSound        05A A81004
#pragma libcall AHIBase AHI_UnloadSound      060 A002
#pragma libcall AHIBase AHI_NextAudioID      066 001
#pragma libcall AHIBase AHI_GetAudioAttrsA   06C 9A003
#pragma libcall AHIBase AHI_BestAudioIDA     072 901
#pragma libcall AHIBase AHI_AllocAudioRequestA 078 801
#pragma libcall AHIBase AHI_AudioRequestA    07E 9802
#pragma libcall AHIBase AHI_FreeAudioRequest 084 801
#pragma libcall AHIBase AHI_PlayA            08A 9A02
#pragma libcall AHIBase AHI_SampleFrameSize  090 001
#pragma libcall AHIBase AHI_AddAudioMode     096 801
#pragma libcall AHIBase AHI_RemoveAudioMode  09C 001
#pragma libcall AHIBase AHI_LoadModeFile     0A2 801
/* "ahi.device" */
/* Requester functions look like ASL's (obsolete) file requester functions  */
/* --- New for V4 --- */
#ifdef __SASC_60
#pragma tagcall AHIBase AHI_AllocAudio       02A 901
#pragma tagcall AHIBase AHI_ControlAudio     03C 9A02
#pragma tagcall AHIBase AHI_GetAudioAttrs    06C 9A003
#pragma tagcall AHIBase AHI_BestAudioID      072 901
#pragma tagcall AHIBase AHI_AllocAudioRequest 078 801
#pragma tagcall AHIBase AHI_AudioRequest     07E 9802
#pragma tagcall AHIBase AHI_Play             08A 9A02
#endif

#endif	/*  PRAGMAS_AHI_PRAGMA_H  */