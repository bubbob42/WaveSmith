#ifndef WAVESMITH_DECODE_H
#define WAVESMITH_DECODE_H

/*
 * decode.h -- a file that is not a 16-bit PCM WAV made one, once (concept
 * 5.2, D4): prismadsp.library's DecodeOnly job writes it beside the
 * original as <name>.wsmith.wav -- the codings and files PrismaEncode
 * reads, the card decoding what only the chip can (MP3, Ogg Vorbis, AAC,
 * WMA) -- and WaveSmith edits that. The original is never touched; the
 * work file goes when the doc does. The job runs on its own process: the
 * window answers meanwhile.
 *
 * What DecodeOnly gives: 16-bit stereo at the file's rate (resampled where
 * the chip could not be fed at it); a file the chip decodes at the
 * stream's own rate and channels.
 */

#include <exec/types.h>

/* the work file beside the original: <name> and this */
#define DECODE_SUFFIX	".wsmith.wav"

#define DECODE_RUNNING	0
#define DECODE_DONE	1
#define DECODE_FAILED	2	/* Decode_Error(): PDSPERR_#?, or -1 no library */

BOOL Decode_Open(void);		/* prismadsp.library 1.1, if it is there (again: TRUE) */
void Decode_Close(void);
BOOL Decode_There(void);
ULONG Decode_Signals(void);	/* to Wait() for */

/* the job for from started (its work file named): DECODE_RUNNING or
   DECODE_FAILED */
UWORD Decode_Start(CONST_STRPTR from);
/* the job's messages read: DECODE_RUNNING, DONE, FAILED */
UWORD Decode_Poll(void);
UWORD Decode_Percent(void);
CONST_STRPTR Decode_Work(void);	/* the work file's path */
LONG Decode_Error(void);
/* the job ended if it runs, the work file deleted */
void Decode_Forget(void);

#endif
