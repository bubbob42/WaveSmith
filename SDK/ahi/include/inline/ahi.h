/*
 * GCC inline calls for ahi.device, generated from fd/ahi_lib.fd and
 * clib/ahi_protos.h (AHI 4.18 kit) with: fd2pragma -s 40
 */
#ifndef _INLINE_AHI_H
#define _INLINE_AHI_H

#ifndef CLIB_AHI_PROTOS_H
#define CLIB_AHI_PROTOS_H
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
#ifndef  DEVICES_AHI_H
#include <devices/ahi.h>
#endif

#ifndef AHI_BASE_NAME
#define AHI_BASE_NAME AHIBase
#endif

#define AHI_AllocAudioA(tagList) \
	LP1(0x2a, struct AHIAudioCtrl *, AHI_AllocAudioA, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_AllocAudio(tags...) \
	({ULONG _tags[] = {tags}; AHI_AllocAudioA((struct TagItem *) _tags);})
#endif

#define AHI_FreeAudio(AudioCtrl) \
	LP1NR(0x30, AHI_FreeAudio, struct AHIAudioCtrl *, AudioCtrl, a2, \
	, AHI_BASE_NAME)

#define AHI_KillAudio() \
	LP0NR(0x36, AHI_KillAudio, \
	, AHI_BASE_NAME)

#define AHI_ControlAudioA(AudioCtrl, tagList) \
	LP2(0x3c, ULONG, AHI_ControlAudioA, struct AHIAudioCtrl *, AudioCtrl, a2, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_ControlAudio(AudioCtrl, tags...) \
	({ULONG _tags[] = {tags}; AHI_ControlAudioA((AudioCtrl), (struct TagItem *) _tags);})
#endif

#define AHI_SetVol(Channel, Volume, Pan, AudioCtrl, Flags) \
	LP5NR(0x42, AHI_SetVol, UWORD, Channel, d0, Fixed, Volume, d1, sposition, Pan, d2, struct AHIAudioCtrl *, AudioCtrl, a2, ULONG, Flags, d3, \
	, AHI_BASE_NAME)

#define AHI_SetFreq(Channel, Freq, AudioCtrl, Flags) \
	LP4NR(0x48, AHI_SetFreq, UWORD, Channel, d0, ULONG, Freq, d1, struct AHIAudioCtrl *, AudioCtrl, a2, ULONG, Flags, d2, \
	, AHI_BASE_NAME)

#define AHI_SetSound(Channel, Sound, Offset, Length, AudioCtrl, Flags) \
	LP6NR(0x4e, AHI_SetSound, UWORD, Channel, d0, UWORD, Sound, d1, ULONG, Offset, d2, LONG, Length, d3, struct AHIAudioCtrl *, AudioCtrl, a2, ULONG, Flags, d4, \
	, AHI_BASE_NAME)

#define AHI_SetEffect(Effect, AudioCtrl) \
	LP2(0x54, ULONG, AHI_SetEffect, APTR, Effect, a0, struct AHIAudioCtrl *, AudioCtrl, a2, \
	, AHI_BASE_NAME)

#define AHI_LoadSound(Sound, Type, Info, AudioCtrl) \
	LP4(0x5a, ULONG, AHI_LoadSound, UWORD, Sound, d0, ULONG, Type, d1, APTR, Info, a0, struct AHIAudioCtrl *, AudioCtrl, a2, \
	, AHI_BASE_NAME)

#define AHI_UnloadSound(Sound, Audioctrl) \
	LP2NR(0x60, AHI_UnloadSound, UWORD, Sound, d0, struct AHIAudioCtrl *, Audioctrl, a2, \
	, AHI_BASE_NAME)

#define AHI_NextAudioID(Last_ID) \
	LP1(0x66, ULONG, AHI_NextAudioID, ULONG, Last_ID, d0, \
	, AHI_BASE_NAME)

#define AHI_GetAudioAttrsA(ID, Audioctrl, tagList) \
	LP3(0x6c, BOOL, AHI_GetAudioAttrsA, ULONG, ID, d0, struct AHIAudioCtrl *, Audioctrl, a2, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_GetAudioAttrs(ID, Audioctrl, tags...) \
	({ULONG _tags[] = {tags}; AHI_GetAudioAttrsA((ID), (Audioctrl), (struct TagItem *) _tags);})
#endif

#define AHI_BestAudioIDA(tagList) \
	LP1(0x72, ULONG, AHI_BestAudioIDA, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_BestAudioID(tags...) \
	({ULONG _tags[] = {tags}; AHI_BestAudioIDA((struct TagItem *) _tags);})
#endif

#define AHI_AllocAudioRequestA(tagList) \
	LP1(0x78, struct AHIAudioModeRequester *, AHI_AllocAudioRequestA, struct TagItem *, tagList, a0, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_AllocAudioRequest(tags...) \
	({ULONG _tags[] = {tags}; AHI_AllocAudioRequestA((struct TagItem *) _tags);})
#endif

#define AHI_AudioRequestA(Requester, tagList) \
	LP2(0x7e, BOOL, AHI_AudioRequestA, struct AHIAudioModeRequester *, Requester, a0, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_AudioRequest(Requester, tags...) \
	({ULONG _tags[] = {tags}; AHI_AudioRequestA((Requester), (struct TagItem *) _tags);})
#endif

#define AHI_FreeAudioRequest(Requester) \
	LP1NR(0x84, AHI_FreeAudioRequest, struct AHIAudioModeRequester *, Requester, a0, \
	, AHI_BASE_NAME)

#define AHI_PlayA(Audioctrl, tagList) \
	LP2NR(0x8a, AHI_PlayA, struct AHIAudioCtrl *, Audioctrl, a2, struct TagItem *, tagList, a1, \
	, AHI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define AHI_Play(Audioctrl, tags...) \
	({ULONG _tags[] = {tags}; AHI_PlayA((Audioctrl), (struct TagItem *) _tags);})
#endif

#define AHI_SampleFrameSize(SampleType) \
	LP1(0x90, ULONG, AHI_SampleFrameSize, ULONG, SampleType, d0, \
	, AHI_BASE_NAME)

#define AHI_AddAudioMode(a0arg) \
	LP1(0x96, ULONG, AHI_AddAudioMode, struct TagItem *, a0arg, a0, \
	, AHI_BASE_NAME)

#define AHI_RemoveAudioMode(d0arg) \
	LP1(0x9c, ULONG, AHI_RemoveAudioMode, ULONG, d0arg, d0, \
	, AHI_BASE_NAME)

#define AHI_LoadModeFile(a0arg) \
	LP1(0xa2, ULONG, AHI_LoadModeFile, STRPTR, a0arg, a0, \
	, AHI_BASE_NAME)

#endif /*  _INLINE_AHI_H  */
