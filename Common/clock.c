/*
 * clock.c -- see clock.h
 */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>

#include <proto/exec.h>
#include <proto/timer.h>

#include "clock.h"

struct Device *TimerBase = NULL;

static struct MsgPort *port = NULL;
static struct timerequest *eclock = NULL, *wake = NULL;
static BOOL eclock_open = FALSE, wake_open = FALSE, waiting = FALSE, pinned = FALSE;
static ULONG freq, last_lo, sec, rem, pinned_ms;

BOOL Clock_Open(BOOL pin)
{
	struct EClockVal ev;

	pinned = pin;
	pinned_ms = 0;
	if (!(port = CreateMsgPort()) ||
	    !(eclock = (struct timerequest *) CreateIORequest(port, sizeof(struct timerequest))) ||
	    !(wake = (struct timerequest *) CreateIORequest(port, sizeof(struct timerequest))))
		goto fail;
	if (OpenDevice((CONST_STRPTR) TIMERNAME, UNIT_ECLOCK, (struct IORequest *) eclock, 0) != 0) goto fail;
	eclock_open = TRUE;
	/* MICROHZ, not VBLANK: a vertical blank's 20 ms steps round every wait
	 * up, and frames fall behind their timetable (DeckBench: 16 a second
	 * where 25 were asked for) */
	if (OpenDevice((CONST_STRPTR) TIMERNAME, UNIT_MICROHZ, (struct IORequest *) wake, 0) != 0) goto fail;
	wake_open = TRUE;
	TimerBase = eclock->tr_node.io_Device;
	freq = ReadEClock(&ev);
	last_lo = ev.ev_lo;
	sec = rem = 0;
	return TRUE;

fail:
	Clock_Close();
	return FALSE;
}

void Clock_Close(void)
{
	if (waiting) {
		AbortIO((struct IORequest *) wake);
		WaitIO((struct IORequest *) wake);
		waiting = FALSE;
	}
	if (wake_open) CloseDevice((struct IORequest *) wake);
	if (eclock_open) CloseDevice((struct IORequest *) eclock);
	wake_open = eclock_open = FALSE;
	TimerBase = NULL;
	if (wake) DeleteIORequest((struct IORequest *) wake);
	if (eclock) DeleteIORequest((struct IORequest *) eclock);
	wake = eclock = NULL;
	if (port) DeleteMsgPort(port);
	port = NULL;
}

BOOL Clock_Pinned(void)
{
	return pinned;
}

/* Whole seconds and the ticks of the one under way kept apart: no 32-bit
 * overflow however long it runs. Called often enough that the E-clock's
 * low word does not wrap twice between two calls (1.7 h on PAL). */
ULONG Clock_Ms(void)
{
	struct EClockVal ev;

	if (pinned) return pinned_ms;
	if (!TimerBase) return 0;
	ReadEClock(&ev);
	rem += ev.ev_lo - last_lo;
	last_lo = ev.ev_lo;
	sec += rem / freq;
	rem %= freq;
	/* (rem / (freq / 1000) rounded the divisor down: 715909 Hz, NTSC, gave
	 * 1001 ms at the end of a second, the next one 1000 -- time went back
	 * a millisecond. rem < freq < 2^20: the product fits) */
	return sec * 1000UL + (rem * 1000UL) / freq;
}

void Clock_Step(ULONG ms)
{
	if (pinned) pinned_ms += ms;
}

ULONG Clock_Ticks(void)
{
	struct EClockVal ev;

	if (!TimerBase) return 0;
	ReadEClock(&ev);
	return ev.ev_lo;
}

ULONG Clock_TickHz(void)
{
	return freq;
}

ULONG Clock_Signal(void)
{
	return port ? 1UL << port->mp_SigBit : 0;
}

void Clock_Wake(ULONG ms)
{
	if (!wake_open || waiting) return;
	wake->tr_node.io_Command = TR_ADDREQUEST;
	wake->tr_time.tv_secs = ms / 1000UL;
	wake->tr_time.tv_micro = (ms % 1000UL) * 1000UL;
	SendIO((struct IORequest *) wake);
	waiting = TRUE;
}

BOOL Clock_Woke(void)
{
	if (!waiting || !CheckIO((struct IORequest *) wake)) return FALSE;
	WaitIO((struct IORequest *) wake);
	waiting = FALSE;
	return TRUE;
}
