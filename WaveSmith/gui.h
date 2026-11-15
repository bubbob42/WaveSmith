#ifndef WAVESMITH_GUI_H
#define WAVESMITH_GUI_H

/*
 * gui.h -- WaveSmith's window (window A of the concept): the overview,
 * the waveform with its In and Out handles, the scroller, the trim row,
 * the buttons, the status line; and the loop that reads the file a piece
 * at a time while the window answers.
 */

#include <exec/types.h>

/* the libraries and classes; FALSE: Gui_Missing() names the first one
   that was not there */
BOOL Gui_OpenLibs(void);
void Gui_CloseLibs(void);
CONST_STRPTR Gui_Missing(void);

/* the window until it is closed, path opened first (NULL: none); the
   return code */
LONG Gui_Run(CONST_STRPTR path, CONST_STRPTR pubscreen);

#endif
