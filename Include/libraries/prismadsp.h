#ifndef LIBRARIES_PRISMADSP_H
#define LIBRARIES_PRISMADSP_H

/*
 * prismadsp.library -- routines of our own on the Prisma Megamix's
 * VS1063a DSP, next to prisma.library (which it needs, 3.1 or later).
 *
 * Version 1: encoding a file. Its samples go through the card's SDI into
 * the chip's encoder -- not through the line input -- and come out as an
 * MP3 (or any other format the encoder writes but FLAC), the chip doing
 * the work. Each job runs in a process of its own; the caller gets
 * progress and its end as messages, or asks.
 *
 * The files read (1.1 on; 1.0: 16-bit PCM WAV and AIFF only):
 *   WAV        PCM 8 (unsigned), 16, 24 and 32 bits, IEEE float (32, 64),
 *              A-law, mu-law, IMA ADPCM, Microsoft ADPCM; and
 *              WAVE_FORMAT_EXTENSIBLE's PCM, float, A-law and mu-law
 *   AIFF/AIFC  PCM 1 to 32 bits, AIFC NONE twos sowt raw in24 in32 42ni
 *              23ni fl32 fl64 ulaw alaw ima4
 *   Sun AU     PCM 8 to 32 bits, float, double, mu-law, A-law
 *   IFF        8SVX (8 bits, plain or Fibonacci delta; mono or stereo;
 *              the first octave), 16SV, MAUD (8 and 16 bits, A-law,
 *              mu-law)
 *   FLAC       4 to 24 bits, up to 8 channels (an ID3v2 tag before it
 *              too); decoded on the Amiga, both CRCs checked
 *   raw        PDSPA_Raw: samples without a header
 * and (1.1 on, with the card) what the chip decodes: MP3, MP2 (an ID3v2
 * tag before them passed over), Ogg Vorbis, AAC (ADTS, ADIF, MP4/M4A --
 * the index, moov, before the audio, mdat), WMA, and WAVs of MP3 or
 * G.722 (one or two channels, up to 48000 Hz). Those go through the
 * chip twice: its decoder first, the samples read back into a temporary
 * WAV (PDSPA_TempDir; PDSPJS_DECODING), then that WAV into its encoder
 * as any other. PDSPA_ChipDecode sends any file that way.
 * 16-bit PCM in one or two channels goes in as it is; anything else is
 * decoded by the job first, more than two channels (up to 8) mixed down
 * to stereo -- the front left and right as they are, centre and
 * surrounds at -3 dB, the bass channel left out, scaled not to clip.
 *
 *   job = PDSP_EncodeFileA(tags);	starts it (NULL: PDSPA_Error says why)
 *   ... PDSPMSG_PROGRESS, then one PDSPMSG_END, each to be replied ...
 *   PDSP_DeleteJob(job);		after the END message came back
 *
 * PDSP_EncodeFileA() must be called from a process: it opens the files
 * (names relative to the caller's current directory) and reads the WAV's
 * header before it answers, so those errors come at once. Without a
 * reply port, poll PDSP_GetJobAttr(job, PDSPJA_State) until it is DONE,
 * FAILED or ABORTED. PDSP_DeleteJob() on a running job aborts it and
 * waits for its end (no END message then); with a reply port, never while
 * one of its messages is unreplied -- they live in the job.
 *
 * The chip is fed at 44100 Hz (for a recording at 44100 or 22050 Hz) or
 * 48000 Hz (for 48000 Hz). A file at another rate (1.1 on: 4000 to 192000
 * Hz; 1.0: none) is resampled to it by the job first (PDSPA_Resample);
 * mono is doubled to both sides. One job at a time; not while the card
 * plays or records for someone else.
 *
 * (c) 2016-2026 Christian Euler & Marcus Gerards
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_PORTS_H
#include <exec/ports.h>
#endif
#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

#define PRISMADSPNAME		"prismadsp.library"
#define PRISMADSP_VERSION	1

/* a job (opaque) */
struct PDSPJob;

/* ---- PDSP_EncodeFileA() tags ---- */
#define PDSPA_Dummy		(TAG_USER + 0x5D500)
#define PDSPA_From		(PDSPA_Dummy + 1)	/* STRPTR: the WAV or AIFF to read (the
							   "WAV" below: either) */
#define PDSPA_FromHandle	(PDSPA_Dummy + 2)	/* BPTR: or an open one, at its start (the
							   caller's: not closed) */
#define PDSPA_To		(PDSPA_Dummy + 3)	/* STRPTR: the file to write */
#define PDSPA_ToHandle		(PDSPA_Dummy + 4)	/* BPTR: or an open one (the caller's) */
#define PDSPA_Format		(PDSPA_Dummy + 5)	/* PREC_#? (libraries/prisma.h); MP3 the
							   default; not PREC_FLAC */
#define PDSPA_Rate		(PDSPA_Dummy + 6)	/* ULONG: the recording's rate: 44100,
							   22050 or 48000. The default: the file's
							   when it is one of them, else 22050 for
							   a file at 24000 Hz or less, 44100 for
							   one at a multiple of 11025, 48000 for
							   the rest */
#define PDSPA_Channels		(PDSPA_Dummy + 7)	/* PRECCH_#?: stereo the default */
#define PDSPA_Quality		(PDSPA_Dummy + 8)	/* ULONG: as PPA_RECQUALITY; MP3: CBR 128
							   kbit/s the default */
#define PDSPA_ReplyPort		(PDSPA_Dummy + 9)	/* struct MsgPort *: progress and end
							   messages; none: ask PDSP_GetJobAttr() */
#define PDSPA_Priority		(PDSPA_Dummy + 10)	/* LONG: the job's process (0 the default;
							   kept below prisma.library's player, 10) */
#define PDSPA_Error		(PDSPA_Dummy + 11)	/* LONG *: PDSPERR_#? when no job comes */
/* 1.1 on */
#define PDSPA_Raw		(PDSPA_Dummy + 12)	/* BOOL: the input has no header -- its
							   samples as the tags below say, CD-DA's
							   unless they do: 44100 Hz, 16 bits,
							   stereo, little-endian; to its end */
#define PDSPA_RawRate		(PDSPA_Dummy + 13)	/* ULONG: its rate (44100) */
#define PDSPA_RawBits		(PDSPA_Dummy + 14)	/* ULONG: 8, 16, 24 or 32 (16) */
#define PDSPA_RawChannels	(PDSPA_Dummy + 15)	/* ULONG: 1 to 8 (2) */
#define PDSPA_RawBigEndian	(PDSPA_Dummy + 16)	/* BOOL: the Amiga's byte order (FALSE) */
#define PDSPA_RawUnsigned	(PDSPA_Dummy + 17)	/* BOOL: unsigned samples (FALSE) */
#define PDSPA_DecodeOnly	(PDSPA_Dummy + 18)	/* BOOL: no card, no encoder -- the samples
							   the chip would get written as a 16-bit
							   stereo WAV at the input's rate (the
							   channel mode done): to check a file, or
							   to convert one. At the rate the chip
							   would be fed (resampled: at that). A
							   file the chip decodes (1.1 on): the card
							   decodes it, the WAV its samples as they
							   come back -- the stream's own rate and
							   channels, PDSPA_Rate and PDSPA_Channels
							   not done */
#define PDSPA_Resample		(PDSPA_Dummy + 19)	/* ULONG: PDSPRS_#?, how a file at a rate
							   other than the chip's feed is resampled
							   (PDSPRS_GOOD the default) */
#define PDSPA_DataTypes		(PDSPA_Dummy + 20)	/* BOOL: the file (PDSPA_From) read through
							   datatypes even when a reader of the
							   library's would take it. Without it,
							   datatypes are asked only when none
							   does: a sound class that hands over the
							   whole sample (V47: 8 to 32 bits), copied
							   into memory first */

#define PDSPA_TempDir		(PDSPA_Dummy + 21)	/* STRPTR: the drawer for a file the chip
							   decodes: its samples go into a WAV there
							   first (about 10 MB a minute at 44.1 kHz
							   stereo), deleted at the job's end. "T:"
							   the default */
#define PDSPA_ChipDecode	(PDSPA_Dummy + 22)	/* BOOL: any file decoded by the chip, not
							   by the job (a FLAC on a slow Amiga). The
							   chip takes a WAV of PCM (8 to 32 bits),
							   float, A-law, mu-law or IMA ADPCM, and a
							   FLAC of up to 24 bits -- one or two
							   channels, up to 48000 Hz; any other file
							   is refused at once (PDSPERR_CHIPDECODE).
							   Not with PDSPA_Raw or PDSPA_DataTypes */

/* ---- PDSP_GetJobAttr() ---- */
#define PDSPJA_State		1	/* PDSPJS_#? */
#define PDSPJA_Error		2	/* PDSPERR_#? (once ended) */
#define PDSPJA_FramesDone	3	/* sample frames of the WAV into the encoder */
#define PDSPJA_FramesTotal	4	/* sample frames in the WAV */
#define PDSPJA_BytesOut		5	/* bytes the encoder has delivered */
#define PDSPJA_Overruns		6	/* prisma.library's recording buffers that came
					   too late (data lost: PPA_RECOVERRUNS) */
#define PDSPJA_Rate		7	/* the recording's rate */
#define PDSPJA_StopMillis	8	/* how long the recording took to end: the
					   encoder's last frame, the chip's reset */
#define PDSPJA_OutPeak		9	/* the most words that waited in the
					   encoder's output buffer (PPA_RECPEAK): it
					   holds 3712 and empties itself when full --
					   near that, data was lost. The job holds the
					   feed while 1024 wait */
#define PDSPJA_WavRate		10	/* the WAV's sample rate */
#define PDSPJA_WavChannels	11	/* the input's channels: 1 to 8 (more than two
					   mixed down) */
#define PDSPJA_StartMillis	12	/* from the job's start to the WAV's first
					   word: the card, the encoder's start (its
					   output's silent head) */
#define PDSPJA_FeedMillis	13	/* from the WAV's first word on: the WAV, the
					   tail, the ring run empty (counting while
					   it feeds) */
#define PDSPJA_Gaps		14	/* MP3 into a file: where the frame chain
					   broke, data lost on the way (PPA_RECGAPS;
					   once ended) */
#define PDSPJA_SciMode		15	/* PDSPERR_ENCODER: SCI_MODE then (bit 12,
					   SM_ENCODE: the chip was encoding) ... */
#define PDSPJA_OutWaiting	16	/* ... and the words waiting in its output
					   (SCI_HDAT1): above 0, nobody read them */
#define PDSPJA_Holds		17	/* times the feed held because 1024 words or
					   more waited in the encoder's output: the
					   library's reader late (0 for MP3 mostly;
					   PCM holds all the time) ... */
#define PDSPJA_HoldMillis	18	/* ... the longest hold, ms ... */
#define PDSPJA_HoldChecks	19	/* ... and the output's checks during it: a
					   check a millisecond or so means the reader
					   stalled; a few, the job was not running */
/* 1.1 on */
#define PDSPJA_InCoding		20	/* the input's coding: PDSPIC_#? */
#define PDSPJA_InBits		21	/* ... its bits a sample as stored (ADPCM 4) */
#define PDSPJA_FeedRate		22	/* the rate the chip is fed at: 44100 or
					   48000 (PDSPA_DecodeOnly's WAV's) */
#define PDSPJA_Resample		23	/* PDSPRS_#? the file is resampled with,
					   PDSPRS_NONE when it is at the feed's rate */
#define PDSPJA_DataType		24	/* STRPTR: the datatype the file was read
					   through, NULL when a reader of ours read it */
#define PDSPJA_ChipDecode	25	/* BOOL: the file goes through the chip's
					   decoder first (the job states
					   PDSPJS_DECODING, then as for any file).
					   Until that pass ends, the WAV attributes
					   (WavRate, WavChannels, FramesTotal, Rate)
					   are 0: they are its output's */
#define PDSPJA_BytesIn		26	/* ... the file's bytes into the chip ... */
#define PDSPJA_BytesTotal	27	/* ... of so many (its size; 0: not known) */
#define PDSPJA_DecodeMillis	28	/* ... how long the decoding took */
#define PDSPJA_DecodeHolds	29	/* ... times the decoder waited for the Amiga
					   to read (its samples' buffer full: the
					   bus the limit) ... */
#define PDSPJA_DecodeStarved	30	/* ... and for the file (its input ran dry) */

/* ---- resampling (PDSPA_Resample): a windowed sinc, the ratio exact ---- */
#define PDSPRS_NONE		0	/* none: a file at another rate is refused
					   (PDSPERR_RATE) */
#define PDSPRS_FAST		1	/* 16 taps: flat to 0.30 of the lower rate,
					   -59 dB from 0.55 */
#define PDSPRS_GOOD		2	/* 32 taps: flat to 0.42, -76 dB from 0.55 */
#define PDSPRS_BEST		3	/* 64 taps: flat to 0.45, -94 dB from 0.55 */

/* ---- the input's codings (PDSPJA_InCoding) ---- */
#define PDSPIC_PCM		1	/* integers */
#define PDSPIC_FLOAT		2	/* IEEE 754 */
#define PDSPIC_ULAW		3	/* G.711 */
#define PDSPIC_ALAW		4
#define PDSPIC_IMA		5	/* IMA ADPCM, a WAV's */
#define PDSPIC_IMA4		6	/* IMA ADPCM, Apple's (AIFC ima4) */
#define PDSPIC_MSADPCM		7	/* Microsoft's ADPCM */
#define PDSPIC_FIBONACCI	8	/* 8SVX's Fibonacci delta */
#define PDSPIC_FLAC		9	/* FLAC (1.1 on) */
/* decoded by the chip (1.1 on): PDSPJA_InBits 0 */
#define PDSPIC_MP3		10	/* MPEG audio layer III */
#define PDSPIC_MP2		11	/* ... layer II */
#define PDSPIC_MP1		12	/* ... layer I */
#define PDSPIC_AAC		13	/* ADTS, ADIF, MP4 (M4A) */
#define PDSPIC_VORBIS		14	/* Ogg Vorbis */
#define PDSPIC_WMA		15	/* Windows Media Audio */
#define PDSPIC_G722		16	/* G.722 ADPCM in a WAV */

/* ---- job states ---- */
#define PDSPJS_STARTING		0	/* the recording starts, the plugin goes in */
#define PDSPJS_ENCODING		1	/* the WAV goes in */
#define PDSPJS_ENDING		2	/* the last of it, then the recording stops */
#define PDSPJS_DONE		3
#define PDSPJS_FAILED		4
#define PDSPJS_ABORTED		5
#define PDSPJS_DECODING		6	/* (1.1 on) the chip decodes the file into the
					   temporary WAV; PDSPJS_STARTING next */

/* ---- errors ---- */
#define PDSPERR_OK		0
#define PDSPERR_NOMEM		1
#define PDSPERR_PRISMALIB	2	/* no prisma.library 3.1 or later */
#define PDSPERR_CARD		3	/* no card, or someone else has it */
#define PDSPERR_PATCH		4	/* VLSI's patch package (2.6) not loaded: the
					   plugin's place is measured beside it */
#define PDSPERR_OPENIN		5	/* the input cannot be opened */
#define PDSPERR_WAVFORMAT	6	/* not a file of a kind and coding read here
					   (above), or more than 8 channels */
#define PDSPERR_RATE		7	/* the input's rate is not read here (4000 to
					   192000 Hz), the recording's is not
					   44100, 22050 or 48000, or PDSPRS_NONE
					   and the two do not match */
#define PDSPERR_FORMAT		8	/* a format route A cannot write (FLAC) */
#define PDSPERR_OPENOUT		9	/* the output cannot be opened */
#define PDSPERR_RECORD		10	/* prisma.library refused the recording */
#define PDSPERR_ENCODER		11	/* the encoder never delivered */
#define PDSPERR_PLUGIN		12	/* the plugin read back different */
#define PDSPERR_RING		13	/* the encoder's input did not empty */
#define PDSPERR_READ		14	/* reading the input failed */
#define PDSPERR_LOST		15	/* the chip was reset while encoding */
#define PDSPERR_ABORTED		16	/* PDSP_AbortJob() */
#define PDSPERR_BUSY		17	/* another job runs */
#define PDSPERR_PROCESS		18	/* no process for the job */
#define PDSPERR_ARGS		19	/* no input or no output named (or
					   PDSPA_ChipDecode with PDSPA_Raw or
					   PDSPA_DataTypes) */
#define PDSPERR_WRITE		20	/* PDSPA_DecodeOnly: writing the WAV failed */
#define PDSPERR_DAMAGED		21	/* a FLAC frame failed its CRC (or no frame
					   where one must be) */
#define PDSPERR_CHIPDECODE	22	/* the chip could not decode the file: no
					   sound came back (not a kind it decodes, a
					   protected WMA, an MP4 with its index after
					   the audio, or damaged) */
#define PDSPERR_TEMP		23	/* the temporary WAV cannot be written (no
					   PDSPA_TempDir drawer, or it is full) */

/* ---- messages (PDSPA_ReplyPort): reply each one ---- */
struct PDSPMessage {
	struct Message pdm_Message;
	struct PDSPJob *pdm_Job;
	ULONG pdm_Code;			/* PDSPMSG_#? */
	ULONG pdm_FramesDone;
	ULONG pdm_FramesTotal;
	LONG pdm_Error;			/* PDSPMSG_END: PDSPERR_#? */
	/* 1.1 on */
	ULONG pdm_State;		/* PDSPJS_#? when it was sent */
	ULONG pdm_BytesIn;		/* PDSPJS_DECODING: the file's bytes into the chip
					   (FramesDone: the frames back, FramesTotal 0) ... */
	ULONG pdm_BytesTotal;		/* ... of so many */
};

#define PDSPMSG_PROGRESS	1	/* a few a second while encoding */
#define PDSPMSG_END		2	/* once: done, failed or aborted */

#endif /* LIBRARIES_PRISMADSP_H */
