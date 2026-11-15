/*
 * clock.h -- the program's one clock: milliseconds from the E-clock, and a
 * timer that wakes the main loop. The simulated card keeps its time by it
 * and the deck its frames, so an autotest can PIN it: then it moves only
 * by Clock_Step(), 40 ms a frame, and two runs draw the same pictures.
 */
#ifndef PRISMAREC_CLOCK_H
#define PRISMAREC_CLOCK_H

#include <exec/types.h>

#define CLOCK_FRAME_MS	40UL		/* a frame: 25 a second */

BOOL Clock_Open(BOOL pinned);
void Clock_Close(void);
BOOL Clock_Pinned(void);
/* Milliseconds since Clock_Open() (49 days before it turns over) */
ULONG Clock_Ms(void);
/* Pinned: the time moves on by ms (nothing otherwise) */
void Clock_Step(ULONG ms);
/* The E-clock itself, for measuring short spans (end - start, a few
 * minutes at most), and its ticks a second; never pinned */
ULONG Clock_Ticks(void);
ULONG Clock_TickHz(void);

/* The wake-up: Clock_Wake(ms) sends a timer request (one at a time; a
 * second call while one runs does nothing); its signal is
 * Clock_Signal(); Clock_Woke() takes the request back when it is done
 * and says so. Clock_Close() aborts one still running. */
ULONG Clock_Signal(void);
void Clock_Wake(ULONG ms);
BOOL Clock_Woke(void);

#endif
