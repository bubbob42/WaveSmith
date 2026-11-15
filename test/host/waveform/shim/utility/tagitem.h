/*
 * utility/tagitem.h for the host tests: only what gadgets/waveform.h uses
 */
#ifndef UTILITY_TAGITEM_H
#define UTILITY_TAGITEM_H

#include <exec/types.h>

typedef ULONG Tag;
#define TAG_DONE	0UL
#define TAG_USER	0x80000000UL

struct TagItem {
	Tag ti_Tag;
	ULONG ti_Data;
};

#endif
