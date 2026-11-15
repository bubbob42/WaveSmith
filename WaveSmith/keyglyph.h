/*
 * keyglyph.h -- the drawn marks on the bar's keys (concept Panel, 10 and
 * 11): Loop's repeat sign and the three lenses, zoom out, zoom in, show
 * all. An imageclass that DRAWS its mark, as AmiMatters' glyph.c does:
 * no artwork, the screen's own text pen, any size -- the marks are shares
 * of the box they are drawn in, so they follow the screen's font as
 * tapedeck.gadget's symbols beside them do.
 *
 * Given to a button as GA_Image the image stays the maker's: the button
 * draws it and never disposes it.
 */
#ifndef WAVESMITH_KEYGLYPH_H
#define WAVESMITH_KEYGLYPH_H

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/screens.h>

#define KEYGLYPH_TAGBASE	(TAG_USER + 0x4B470000UL)	/* 'KG' */
#define KEYGLYPH_Kind		(KEYGLYPH_TAGBASE + 1)	/* (ULONG) KG_#?, OM_NEW */
#define KEYGLYPH_Size		(KEYGLYPH_TAGBASE + 2)	/* (UWORD) its height, OM_NEW */
#define KEYGLYPH_Screen		(KEYGLYPH_TAGBASE + 3)	/* (struct Screen *) for the pixels'
							   shape and the word's width, OM_NEW
							   only (not kept) */
#define KEYGLYPH_Text		(KEYGLYPH_TAGBASE + 4)	/* (CONST_STRPTR) a flag's word, kept:
							   it must live as long as the image */
#define KEYGLYPH_PenPtr		(KEYGLYPH_TAGBASE + 5)	/* (const LONG *) a flag's pen, read at
							   each draw (-1: none yet, the pennant
							   drawn empty) -- the
							   waveform's handle pen, known only once
							   the gadget has laid its pens out */
#define KEYGLYPH_Width		(KEYGLYPH_TAGBASE + 6)	/* (UWORD) the key's inside width,
							   OM_NEW: Loop's sign fills it but for a
							   flag's margin each side (0: 11 to 9) */

#define KG_LOOP		0		/* two arrows chasing round a rectangle */
#define KG_ZOOMOUT	1		/* a lens, a minus in it */
#define KG_ZOOMIN	2		/* a lens, a plus */
#define KG_ALL		3		/* a lens, a frame: everything in view */
#define KG_FLAG		4		/* a handle's pennant in its pen, its word beside */

/* intuition and graphics open; NULL without memory (the caller labels
   its keys with words then) */
Class *KeyGlyph_MakeClass(void);
void KeyGlyph_FreeClass(Class *cl);

/* TRUE when the screen's pixels are about square (laced hires, lores,
   RTG): rows are cheap there, and a bar key can be taller */
BOOL KeyGlyph_SquarePixels(struct Screen *scr);
/* how many pixels wide a square h pixels tall is on this screen (the
   About picture's width) */
WORD KeyGlyph_SquareWidth(struct Screen *scr, WORD h);

#endif
