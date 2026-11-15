#ifndef PRISMA_LIB_H
#define PRISMA_LIB_H

/*
 * prisma.library header file
 *
 * (c) 2016-2026 Christian Euler & Marcus Gerards
 *
 * Binary compatibility: every structure below that existed in 1.x keeps its
 * layout. The library allocates PrismaCard, PrismaPlayer and ReplayBuffer
 * itself and may append private data behind them, so never allocate or copy
 * these structures yourself; use the pointers the library hands out.
 */

#include <exec/exec.h>
#include <dos/dos.h>

#define PRISMALIB_NAME		"prisma.library"
#define PLAYERPROC_NAME		"PrismaPlayerProcess"
#define PLAYERPORT_NAME		"PrismaPlayerPort"
#define PLAYERINT_NAME		"PrismaPlayerInterrupt"

#define MIN_BUFFERS		2	/* Minimum replay buffer count */
#define MAX_BUFFERS		8	/* Maximum replay buffer count */

#define BUFFER_SIZE		65536	/* Buffer size used by the file player */


/*
 * Bus type defines
 */
#define PM_BUSTYPE_ZORRO	0x01	/* Zorro II mode */
#define PM_BUSTYPE_CLOCKPORT	0x02	/* Clockport mode */


/*
 * Player state (NONE -> IDLE -> PLAYING -> (EOF ->) STOPPING -> IDLE -> ...)
 */
#define PLAYER_NONE		0x00	/* Player is not initialised (no player task running) */
#define PLAYER_IDLE		0x01	/* Player is idle (task ready and waiting for action) */
#define PLAYER_INIT		0x02	/* Player is initialising (no longer used) */
#define PLAYER_PLAYING		0x03	/* Player is playing */
#define PLAYER_STOPPING		0x04	/* Player is ending the stream on the chip */
#define PLAYER_STOPPED		0x05	/* Player has stopped (no longer used) */
#define PLAYER_PAUSED		0x06	/* Player is paused */
#define PLAYER_LOADING		0x07	/* Player is loading data (not used) */
#define PLAYER_EOF		0x08	/* Player reached end of current file, queue still playing */
#define PLAYER_RECORDING	0x09	/* Player is recording (Prisma_Record(), since 3.x) */


/*
 * Player structure (read only for clients)
 */
struct PrismaPlayer {
	UWORD pp_State;					/* Global state of player (see above) */

	struct List pp_ReplayList;			/* Replay buffer queue */
	struct List pp_DirtyList;			/* Dirty buffer queue */
	UWORD pp_BufferCount;				/* Number of allocated buffers */
	struct ReplayBuffer *pp_ActiveBuffer;		/* Current active buffer */

	struct MsgPort *pp_MsgPort;			/* Player msg port */
	struct MsgPort *pp_TimerPort;			/* Timer msg port */
	struct MsgPort *pp_ClientPort;			/* Client msg port */
	struct Process *pp_Process;			/* Player process */
	struct PrismaCard *pp_Card;			/* Pointer to card state struct */

	BPTR pp_FileHandle;				/* Player file handle (or NULL for memory player) */

	struct Task *pp_SignalTask;			/* Task to signal for buffer swap */
	ULONG pp_SignalMask;				/* Buffer swap signal mask */

	UWORD pp_Volume;				/* Volume level (0 - 100) */
	UWORD pp_Panning;				/* Panning level (0 - Left, 50 - Center, 100 - Right) */

	BOOL pp_TaskRunning;				/* Player "task running" flag */
	BYTE pp_BufferSignal;				/* Buffer swap signal */
};


/*
 * 5 band equalizer control structure (layout of the chip's eq5Params)
 */
struct PrismaEqualizer {
	UWORD pe_Reserved;			/* Not used */

	WORD pe_BassLevel;			/* Bass level (-32 to +32) (+- 16dB in 0.5dB steps) */
	UWORD pe_BassCutoff;			/* Bass / Mid-Bass cutoff frequency (20-150 Hz) */

	WORD pe_MidBassLevel;			/* Mid-Bass level (-32 to +32) (+- 16dB in 0.5dB steps) */
	UWORD pe_MidBassCutoff;			/* Mid-Bass / Mid cutoff frequency (50-1000 Hz) */

	WORD pe_MidLevel;			/* Mid level (-32 to +32) (+- 16dB in 0.5dB steps) */
	UWORD pe_MidCutoff;			/* Mid / Mid-High cutoff frequency (1000-15000 Hz) */

	WORD pe_MidHighLevel;			/* Mid-High level (-32 to +32) (+- 16dB in 0.5dB steps) */
	UWORD pe_MidHighCutoff;			/* Mid-High / Treble cutoff frequency (2000-15000 Hz) */

	WORD pe_TrebleLevel;			/* Treble level (-32 to +32) (+- 16dB in 0.5dB steps) */
};


/*
 * PCA_INPUTSOURCE: the card's line input
 */
#define PM_INPUT_LINEIN		0x00
#define PM_INPUT_EXTIN		0x01


/*
 * The input's channels (PCA_INPUTCHANNELS) and, in ps_InputConfig, its rate
 * (the chip's adMixerConfig)
 */
#define PM_MIXER_RATE_192	(0<<4)
#define PM_MIXER_RATE_96	(1<<4)
#define PM_MIXER_RATE_48	(2<<4)
#define PM_MIXER_RATE_24	(3<<4)

#define PM_MIXER_STEREO		(0<<6)
#define PM_MIXER_MONO		(1<<6)
#define PM_MIXER_LEFT		(2<<6)
#define PM_MIXER_RIGHT		(3<<6)


/*
 * Prisma settings structure (ENV:prisma.prefs holds exactly this)
 *
 * It grows at its end only (it is the last member of struct PrismaCard,
 * which only the library allocates): a shorter file from an older
 * version loads, the fields it lacks keep their defaults. Since 3.0 the
 * master layer follows -- a mixer's settings over whatever program plays
 * (Prisma_SetCardAttr(), PCA_MASTER#?). The library reads those from the
 * file once, when it first finds the card; later they change only through
 * Prisma_SetCardAttr(), so a program that starts does not undo them.
 */
struct PrismaSettings {
	UWORD ps_ConfigSize;			/* Size of this struct in bytes */
	ULONG ps_ConfigBits;			/* Config bits, see below */
	UWORD ps_DefaultVolume;			/* Default volume level (0 - 100) */
	UWORD ps_DefaultPanning;		/* Default panning (0 - 100) */
	struct PrismaEqualizer ps_DefaultEQ;	/* Default equalizer settings */
	UWORD ps_InputConfig;			/* Input mixer config (copy of VS1063 adMixerConfig register) */
	WORD  ps_InputGain;			/* Line mixer input gain in dB (-3 .. -31, 3 dB steps) */

	/* since 3.0: the master layer (PCA_MASTER#? has the units) */
	UWORD ps_MasterVolume;			/* 0..100, 100 = as the program plays */
	UWORD ps_MasterBalance;			/* 0..100, 50 = centre */
	BYTE  ps_MasterEQ[5];			/* bass .. treble, 0.5 dB steps -32..32 */
	BYTE  ps_MasterTreble;			/* 1.5 dB steps -8..7 */
	UBYTE ps_MasterBassBoost;		/* dB 0..15 */
	UBYTE ps_MasterBassTens;		/* the bass boost below Hz / 10, 2..15 */
	UBYTE ps_MasterTrebleKHz;		/* the treble from kHz, 1..15 */
	UBYTE ps_MasterMono;			/* 1: left and right averaged */
	UWORD ps_MasterEarSpeaker;		/* 0..65535, 0 = off */
};


/*
 * Prisma config bits
 */
#define PCONFBIT_CLOCKPORT_ENABLE	0	/* Clockport enabled (1) or disabled (0) */
#define PCONFBIT_CLOCKPORT_INTENA	1	/* Clockport interrupt enabled (1) or disabled (0) */
#define PCONFBIT_CLOCKPORT_USEINT6	2	/* Clockport interrupt routing (0) INT2 or (1) INT6 */
#define PCONFBIT_LINEIN_EXT		3	/* Input source (0) LineIn or (1) ExtIn/CDDA */
#define PCONFBIT_ADMIXER_ENABLE		4	/* Input mixer enabled (1) or disabled (0) */
#define PCONFBIT_VUMETER_ENABLE		5	/* VUMeter enabled (1) or disabled (0) */

#define PCONFIG_CLOCKPORT_ENABLE	(1L << PCONFBIT_CLOCKPORT_ENABLE)
#define PCONFIG_CLOCKPORT_INTENA	(1L << PCONFBIT_CLOCKPORT_INTENA)
#define PCONFIG_CLOCKPORT_USEINT6	(1L << PCONFBIT_CLOCKPORT_USEINT6)
#define PCONFIG_LINEIN_EXT		(1L << PCONFBIT_LINEIN_EXT)
#define PCONFIG_ADMIXER_ENABLE		(1L << PCONFBIT_ADMIXER_ENABLE)
#define PCONFIG_VUMETER_ENABLE		(1L << PCONFBIT_VUMETER_ENABLE)


/*
 * Replay buffer states (READY -> PLAYING -> DIRTY -> FILLING -> READY -> ...)
 */
#define BUFFER_READY	0x00     /* Buffer is ready to play */
#define BUFFER_PLAYING	0x01     /* Buffer is playing */
#define BUFFER_DIRTY	0x02     /* Buffer has finished playing and needs to be refilled */
#define BUFFER_FILLING	0x04     /* Buffer is refilling */


/*
 * Replay buffer structure
 *
 * Before Prisma_QueueBuffer() set rb_Data, rb_Size (bytes, odd sizes are
 * fine) and rb_Pos = rb_Data. rb_End is still honoured for clients built
 * against 1.x: when rb_Size is odd and rb_End == rb_Data + rb_Size / 2
 * words, the trailing byte is played too.
 */
struct ReplayBuffer {
	struct Node rb_Node;			/* Standard node structure */
	ULONG rb_Size;				/* Size of buffer in bytes */
	UWORD *rb_Data;				/* Pointer to buffer memory */
	UWORD *rb_Pos;				/* Current replay position in buffer */
	UWORD *rb_End;				/* End of buffer */
};


/*
 * Prisma card structure
 */
struct PrismaCard {
	UWORD *pc_CardBase;			/* Base address of Prisma card */
	ULONG pc_CardSerial;			/* Serial number of Prisma card */
	UWORD pc_ChipID;			/* Prisma Chip ID (6 for VS1063) */
	UWORD pc_Volume;			/* Current volume level */
	UWORD pc_VuMeter;			/* VU meter data (bits 8-15 left channel, 0-7 right channel, 3 dB steps 0-32) */
	UWORD pc_BusType;			/* Bus type (Zorro II or Clockport) */
	ULONG pc_PlayPosition;			/* Replay position (seconds, from SCI_DECODE_TIME) */
	UWORD pc_EndFillByte;			/* endFillByte value from parametric memory (see datasheet) */
	struct PrismaSettings pc_Settings;	/* Card configuration */
};


/*
 * Library base
 */
struct PrismaBase {
	struct Library pb_Library;
	UWORD pb_Pad;
	BPTR pb_SegList;
	struct Library *pb_SysBase;
	struct Library *pb_DosBase;

	struct SignalSemaphore *pb_Semaphore;	/* Private */
	struct PrismaCard *pb_Card;		/* Pointer to card status structure */
};


/*
 * Player task control message
 */
struct PlayerMsg {
	struct Message pm_Message;	/* Message body */
	UWORD pm_Code;			/* Message code */
	UWORD pm_Response;		/* Response code */
	ULONG pm_Data;			/* Message data */
};


/*
 * Player control codes (Client -> Player)
 */
#define PMSG_STARTUP		0x0000		/* Startup message, pm_Data = struct PrismaBase * */
#define PMSG_PLAY		0x0001		/* Play a file, pm_Data = File handle (BPTR) */
#define PMSG_STOP		0x0002		/* Stop player */
#define PMSG_PAUSE		0x0003		/* Pause player */
#define PMSG_RESUME		0x0004		/* Resume player */
#define PMSG_SETVOLUME		0x0005		/* Set volume */
#define PMSG_RECORD		0x0006		/* Record into a file, pm_Data = File handle (BPTR) */
#define PMSG_TERMINATE		0x0FFF		/* Terminate player */


/*
 * Player status codes (Player -> Client), sent to the client port given to
 * Prisma_CreatePlayer(). Reply every one of them. The player does
 * not wait for the reply, so it is safe to call the library while holding an
 * unreplied status message.
 *
 * PSTATUS_STOPPED follows every Prisma_Stop() and every end of file.
 * Since 3.0 its pm_Data holds the number of the play session that ended
 * (see PPA_SESSION), or 0 when a Prisma_Stop() found nothing playing. A
 * Prisma_Stop() that crosses the end of a file thus yields two STOPPED
 * messages, only one of them with the session number.
 * A Prisma_Stop() empties the replay queue in every state (since 3.0 also
 * when nothing plays): every queued buffer comes back through
 * Prisma_GetFreeBuffer().
 * PSTATUS_TERMINATED is the last message a player sends.
 */
#define PSTATUS_STOPPED		0x8001		/* Player has finished playing */
#define PSTATUS_TERMINATED	0x8FFF		/* Player process has terminated */


/*
 * Player response codes
 */
#define PMSG_OK			0x00		/* Response code: OK */
#define PMSG_ERROR		0x01		/* Response code: ERROR */
#define PMSG_BUSY		0x02		/* Response code: Player is busy */


/****************************************************************************
 * Additions after the 2019 release. They sit behind the old vectors, so
 * an old library does not have them: check the version before calling.
 ****************************************************************************/

/*
 * Prisma_GetPlayerAttr() / Prisma_SetPlayerAttr() attributes
 */
#define PPA_FLAGS		1	/* (get) PPF_#? flags below */
#define PPA_UNDERRUNS		2	/* (get) times the queue ran dry while playing */
#define PPA_WORDSSENT		3	/* (get) 16-bit words sent to the chip */
#define PPA_TICKS		4	/* (get) streaming timer ticks so far */
#define PPA_TICKMICROS		5	/* (get/set) streaming timer period, 2000..40000 us */
#define PPA_QUEUED		6	/* (get) buffers waiting in the replay queue */
#define PPA_SESSION		7	/* (get) number of the latest play session: counts
					   every Prisma_Play() of a new stream from 1 */
#define PPA_TICKTIME		8	/* (get/set) E-clock ticks spent inside the
					   streaming tick while timing is on (at
					   ReadEClock()'s rate). Set nonzero to reset it
					   and start timing, 0 to stop; off by default,
					   it costs two ReadEClock() calls per tick */

/* Latency, for PCM streams (WAV header + samples, as an AHI driver plays).
 * Without a fill target the streaming tick fills the chip's SDI FIFO; the
 * decoder moves that into its audio FIFO (2048 frames), so the chip alone
 * holds some 58 ms at 44.1 kHz 16-bit stereo. With one, the tick sends
 * only what keeps the chip at PPA_FILLTARGET frames. Set PPA_FRAMEBYTES
 * first: the tick counts the SDI FIFO's bytes in frames with it. */
#define PPA_FILLTARGET		9	/* (get/set) frames the chip may hold, audio
					   FIFO and SDI FIFO together; 0 (default) =
					   as many as fit. Needs PPA_FRAMEBYTES. Keep it
					   above what plays in two tick periods. Setting
					   it also sets PPA_FILLMAX to twice the value
					   and clears PPA_FILLRAISES; get returns the
					   target now, raised or not (PPA_FILLMAX) */
#define PPA_FRAMEBYTES		10	/* (get/set) bytes per frame of the stream
					   (channels x bytes per sample), 1..16; 0 =
					   unknown (default): no fill target, no
					   PPA_CHIPFRAMES */
#define PPA_CHIPFRAMES		11	/* (get) frames the chip holds after the last
					   tick (one with room in the SDI FIFO): what a
					   buffer queued now waits behind, up to a tick
					   early. Needs PPA_FRAMEBYTES */
#define PPA_CHIPMIN		12	/* (get/set) the fewest frames the chip held at
					   a tick since this was set (any value resets
					   it); ~0 until a tick has measured */
#define PPA_QUEUEDBYTES		13	/* (get) bytes in the replay queue not sent to
					   the chip yet, the buffer being sent included */
#define PPA_FILLMAX		14	/* (get/set) how far the tick may raise the
					   fill target: when a tick finds less than a
					   quarter of the target left although data was
					   queued (something held interrupts off), the
					   target grows by a third, once per stall, up to
					   this. Equal to PPA_FILLTARGET: never raised.
					   Set it after PPA_FILLTARGET */
#define PPA_FILLRAISES		15	/* (get) times the tick raised the target */

/*
 * Recording: Prisma_Record(player, fh) records into a file what the chip's
 * ADC sees -- the card's line input, which PCA_INPUTSOURCE switches
 * between the Amiga's own audio (PM_INPUT_LINEIN) and the external input
 * (PM_INPUT_EXTIN). The chip encodes and writes the file header itself.
 * Needs the VLSI patch package (DEVS:vs1063a-patches.plg) loaded.
 *
 * Set the PPA_REC settings first; the chip reads them when the encoder
 * starts. Prisma_Record() returns PMSG_OK, PMSG_BUSY (the player is not
 * idle) or PMSG_ERROR (PPA_RECERROR says why). Prisma_Stop() ends the
 * recording: the rest of the stream is written, a WAV header (or FLAC's
 * STREAMINFO) gets its sizes, then PSTATUS_STOPPED arrives -- close the
 * file only after it. A
 * write error ends the recording too (PSTATUS_STOPPED, PRECERR_WRITE).
 * While recording, Prisma_SetVolume() sets how loud the input is heard
 * (the chip's monitor); pausing is not possible. Afterwards the chip is
 * reset (quietly) and plays again as before.
 *
 * Write nothing else into the file handle meanwhile. A WAV file's sizes
 * are fixed with Seek(), from where the file stood at Prisma_Record(); on
 * a handle that cannot seek (a pipe) they stay $FFFFFFFF, which players
 * read as "to the end". FLAC into a file: every frame's CRC is checked on
 * its way, bytes the encoder now and then puts between two frames are left
 * out (PPA_RECDROPPED), and STREAMINFO gets the smallest and largest frame
 * size and the total samples, which the encoder writes as 0 ("unknown" --
 * macOS's decoder then reads no frame past ~4 KB); on a pipe they stay 0.
 *
 * Without a file (fh = 0) the recording goes into buffers the client
 * queues with Prisma_QueueRecordBuffer() -- made by Prisma_CreateBuffer(),
 * an even size -- and takes back filled with Prisma_GetRecordBuffer():
 * (UBYTE *) rb_End - (UBYTE *) rb_Data bytes, from rb_Data on. A buffer
 * comes back once it is full, the last one at the end. Both calls may come
 * from an interrupt; nobody is signalled, poll. While no buffer is queued,
 * nothing is read: the chip drops what it records, and what it still held
 * is dropped when buffers come again.
 *
 * With PRECF_CODEC in PPA_RECFLAGS the chip plays at the same time (codec
 * mode, full duplex): what the client queued with Prisma_QueueBuffer() --
 * before Prisma_Record(), and on -- streams to the chip while it records.
 * Prisma_Play() is not called then; Prisma_Stop() ends both. The input is
 * not heard in codec mode (no monitor, no pass-through), and the chip
 * holds little of the playback: its 2 KB input FIFO, 21 ms at 96 KB/s --
 * keep the queue ahead. While recording the tick runs every 5 ms at most;
 * a shorter PPA_TICKMICROS set before Prisma_Record() stays (2 ms keeps
 * more in the FIFO at 16-bit PCM, but stalled mu-law at 48 kHz stereo).
 */
#define PPA_RECFORMAT		16	/* (get/set) PREC_#? below; default PREC_MP3 */
#define PPA_RECRATE		17	/* (get/set) sample rate 8000..48000 Hz, default
					   44100. MP3 knows only 8000, 11025, 12000,
					   16000, 22050, 24000, 32000, 44100, 48000 */
#define PPA_RECCHANNELS		18	/* (get/set) PRECCH_#? below; default stereo */
#define PPA_RECQUALITY		19	/* (get/set) MP3 and Ogg Vorbis: PREC_CBR(),
					   PREC_VBR() or PREC_QUALITY(); FLAC:
					   PREC_FLAC_#? (the low 3 bits count);
					   default PREC_CBR(128) */
#define PPA_RECGAIN		20	/* (get/set) input gain, 1024 = 1x (0 dB),
					   0 = automatic (AGC); default 1024. Takes
					   effect at once while recording */
#define PPA_RECMAXGAIN		21	/* (get/set) the most the AGC amplifies, 1024 =
					   1x; 0 (default) = 64x */
#define PPA_RECBYTES		22	/* (get) bytes the chip has delivered in this
					   (or the last) recording */
#define PPA_RECPEAK		23	/* (get/set) the most words waiting in the
					   chip's encoder buffer at a tick. It holds
					   3712 and empties itself when full: near that,
					   data was lost. Any set clears it */
#define PPA_RECOVERRUNS		24	/* (get) ticks that found no empty buffer for
					   what the chip had (the file writes fell
					   behind) */
#define PPA_RECLEVEL		25	/* (get) input peak since the previous look
					   (PPA_RECLEVELMICROS, a quarter second unless
					   set), left << 16 | right, 0..32767 (mono
					   formats: left only) */
#define PPA_RECLEVELMAX		26	/* (get/set) the same, the most since it was
					   set; any set clears it */
#define PPA_RECERROR		27	/* (get) PRECERR_#? below */
#define PPA_RECFLAGS		28	/* (get/set) PRECF_#? below; default 0 */

/*
 * The chip's sound processing beyond volume and the equalizer, set by the
 * card's owner like the volume (datasheet 10.2, 10.8.3, 11.10.7, 11.10.8).
 * Every new owner finds them off; a recording keeps them. Values outside
 * a range are clamped to it.
 *
 * Bass boost and treble (both in SCI_BASS) replace the 5-band equalizer
 * while either is on: the chip runs the equalizer only when both are off.
 * The tempo (speed shifter: faster or slower, the pitch kept) overrides
 * EarSpeaker, and is held back while recording (the chip must not run it
 * in codec mode); the tempo set meanwhile comes into effect afterwards.
 * Mono averages left and right.
 */
#define PPA_BASSBOOST		29	/* (get/set) bass enhancer, dB 0..15; 0 = off */
#define PPA_BASSFREQ		30	/* (get/set) boosted below this, Hz 20..150 in
					   10 Hz steps; default 60 */
#define PPA_TREBLE		31	/* (get/set) treble in 1.5 dB steps, -8..7
					   (a LONG: -12 dB .. +10.5 dB); 0 = off */
#define PPA_TREBLEFREQ		32	/* (get/set) treble at and above this, Hz
					   1000..15000 in 1 kHz steps; default 10000 */
#define PPA_EARSPEAKER		33	/* (get/set) headphone spatialiser, 0..65535;
					   0 = off; PEARSPEAKER_#? below */
#define PPA_MONO		34	/* (get/set) 1: left and right averaged */
#define PPA_TEMPO		35	/* (get/set) tempo, the pitch kept: 16384 = 1.0x
					   (PTEMPO_NORMAL), PTEMPO_MIN..PTEMPO_MAX
					   (0.68x..1.64x); 0 = normal */
#define PPA_RECDROPPED		36	/* (get) FLAC into a file: bytes the chip's
					   encoder put between two frames, left out
					   of the file (0 mostly; two now and then).
					   The library checks every frame's CRC on
					   its way to the file and drops only what
					   lies between a whole frame and the next.
					   Without a file the client's buffers get
					   the stream as the chip makes it */

/*
 * Level looks (after 3.0; a library without them answers FALSE to the
 * sets, which is how a client tells).
 * While recording, the tick looks at the input peaks -- the largest value
 * per channel since the previous look, which the chip then clears -- once
 * every PPA_RECLEVELMICROS, timed on the E-clock: the period holds however
 * long the tick's own work takes. The looks sit on a fixed grid from the
 * start of the recording, so look n holds the peak of the time up to n
 * periods, and the count of looks is the recording's clock. The first look
 * comes a quarter second after the start (the encoder must have read its
 * settings first) and stands for every period that has passed by then.
 *
 * A client that wants every look -- a level map, a gap detector -- gives the
 * library a ring of its own with PPA_RECLEVELRING: the tick writes each look
 * into it (left << 16 | right, as PPA_RECLEVEL) and counts it in lr_Looks.
 * Look k (from 0) sits at lr_Levels[k % lr_Size]; read the looks from your
 * last count up to lr_Looks, and when lr_Looks has run more than lr_Size
 * ahead of you, the oldest of them are gone. Make the ring hold a few times
 * what arrives between two of your reads. A tick held off past several
 * periods writes its one look that many times, so the count stays the
 * clock; held off for more than a second, the grid starts again from that
 * moment and lr_Stalls counts it (the count no longer matches the time).
 *
 * Attaching the ring and Prisma_Record() set lr_Looks and lr_Stalls to 0.
 * The ring stays the client's memory and attached until PPA_RECLEVELRING
 * is set to 0 or the player is deleted; once that set returns, the library
 * no longer touches it.
 */
#define PPA_RECLEVELMICROS	37	/* (get/set) microseconds between two looks,
					   10000..1000000 (clamped); default 250000.
					   A change while recording takes effect at
					   once, the grid starting anew */
#define PPA_RECLEVELRING	38	/* (get/set) a struct PrismaLevelRing of the
					   client's, or 0; FALSE for a ring without
					   lr_Levels or lr_Size */

struct PrismaLevelRing {
	ULONG *lr_Levels;			/* lr_Size entries, the client's */
	ULONG lr_Size;
	volatile ULONG lr_Looks;		/* looks written (the library) */
	volatile ULONG lr_Stalls;		/* grid restarts (the library) */
};

/*
 * Playing a file from the middle. Prisma_Play() reads a file from where
 * its handle stands: MP3, Ogg Vorbis and FLAC find their sync there, but a
 * WAV file needs its header, which a handle Seek()ed past it leaves out.
 * With PPA_PLAYHEADER set, the next Prisma_Play() of a file first sends
 * the file's first n bytes (its header, up to the data), then goes on from
 * where the handle stands -- once; then it is 0 again. The client seeks
 * the handle before Prisma_Play() and keeps the offset on a block of the
 * format (WAV: nBlockAlign).
 */
#define PPA_PLAYHEADER		39	/* (get/set) bytes, 0..4096; FALSE above */

/*
 * MP3 into a file (since 3.1): the library counts the frames on their way.
 * A VBR or quality recording (PPA_RECQUALITY not PREC_CBR()) into a file
 * that can seek starts with a silent frame, which becomes a Xing frame at
 * the end -- the frames, the bytes and a seek table, so that players show
 * the length and the bitrate (the chip writes bare frames: a player took
 * the first frame's bitrate for the whole file). PPA_RECGAPS counts where
 * the frame chain broke: data lost on the way.
 */
#define PPA_RECGAPS		40	/* (get) MP3 into a file: gaps in the frame
					   chain of this (or the last) recording */

#define PPF_UNDERRUN		(1L<<0)	/* queue is empty while playing */
#define PPF_PAUSED		(1L<<1)
#define PPF_EOF			(1L<<2)	/* file end reached, queue still playing */
#define PPF_STOPPING		(1L<<3)
#define PPF_RECORDING		(1L<<4)
#define PPF_RACING		(1L<<5)	/* the stream races: PCA_RACING (since 3.0) */

/* PPA_RECFORMAT: the chip's encoders. All but Ogg, MP3 and FLAC are RIFF
 * WAV. FLAC (lossless; since 3.0) is in VLSI's encoding-only patch variant,
 * DEVS:vs1063a-encpatchesflac.plg: the recording starts with a quiet reset
 * into it, and its end loads the full patch package again, as after every
 * recording. That variant lacks the exact 44.1 kHz rate: it samples at
 * ~44200 Hz there, so the file plays ~0.2 % slow; 48 kHz is exact. */
#define PREC_IMA		0	/* IMA ADPCM, 4 bits per sample */
#define PREC_PCM		1	/* 16-bit linear PCM */
#define PREC_ULAW		2	/* G.711 mu-law, 8 bits */
#define PREC_ALAW		3	/* G.711 A-law, 8 bits */
#define PREC_G722		4	/* G.722 ADPCM */
#define PREC_OGG		5	/* Ogg Vorbis */
#define PREC_MP3		6	/* MP3 */
#define PREC_FLAC		7	/* FLAC, lossless (DEVS:vs1063a-encpatchesflac.plg) */

/* PPA_RECCHANNELS: which inputs, how many channels in the file */
#define PRECCH_STEREO		0	/* stereo, one gain (AGC) for both */
#define PRECCH_DUAL		1	/* stereo, AGC per channel */
#define PRECCH_LEFT		2	/* mono, the left input */
#define PRECCH_RIGHT		3	/* mono, the right input */
#define PRECCH_MONO		4	/* mono, left and right mixed */
#define PRECCH_SWAPPED		5	/* stereo, left and right swapped */
#define PRECCH_LEFTBOTH		6	/* stereo file, the left input on both */
#define PRECCH_RIGHTBOTH	7	/* stereo file, the right input on both */

/* PPA_RECQUALITY. Ogg Vorbis works in quality mode; it turns bitrates into
 * a quality. Quality 5 is meant to be near CD quality at the rate. */
#define PREC_QUALITY(q)		((ULONG) (q))			/* 0..9 */
#define PREC_VBR(kbit)		(0x6000UL | (ULONG) (kbit))	/* variable, about kbit/s (1..511) */
#define PREC_CBR(kbit)		(0xE000UL | (ULONG) (kbit))	/* constant kbit/s (1..511) */
/* FLAC: how it chooses the channel coding (patch manual 1.10) */
#define PREC_FLAC_FAST		0UL	/* always mid/side: the fastest (larger files
						   when the channels differ much) */
#define PREC_FLAC_MIDSIDE	2UL	/* mid/side or left/right, the better */
#define PREC_FLAC_BEST		4UL	/* all four ways: the smallest, the slowest */

/* PPA_EARSPEAKER: the datasheet's levels (11.10.8) */
#define PEARSPEAKER_OFF		0
#define PEARSPEAKER_SUBTLE	12000	/* music on headphones, very subtle */
#define PEARSPEAKER_NORMAL	38000	/* the sound source further away */
#define PEARSPEAKER_WIDE	50000	/* old or 'dry' recordings */

/* PPA_TEMPO */
#define PTEMPO_NORMAL		16384
#define PTEMPO_MIN		11141
#define PTEMPO_MAX		26869

/* PPA_RECFLAGS */
#define PRECF_CODEC		(1L<<0)	/* full duplex (codec mode): the replay queue
					   plays while the chip records. WAV formats
					   only (not Ogg, MP3), at 8000, 12000, 16000,
					   24000, 32000 or 48000 Hz */
#define PRECF_NOHEADER		(1L<<1)	/* no RIFF header before the samples; with
					   PRECF_CODEC the replay queue sends none
					   either: it plays in the recording's format,
					   rate and channels (16-bit PCM little-endian) */

/* PPA_RECERROR */
#define PRECERR_NONE		0
#define PRECERR_NOPATCH		1	/* the VLSI patch package is not loaded */
#define PRECERR_SETTINGS	2	/* a rate MP3 or codec mode does not have,
					   or Ogg/MP3 in codec mode */
#define PRECERR_NOMEM		3	/* no memory for the buffers */
#define PRECERR_START		4	/* the chip did not start encoding */
#define PRECERR_WRITE		5	/* writing the file failed: recording ended */
#define PRECERR_NOFLAC		6	/* FLAC: DEVS:vs1063a-encpatchesflac.plg is
					   missing or not a VLSI plugin */
#define PRECERR_BUSRATE		7	/* (since 3.0) more data than the card's bus
					   carries. A clockport card reads 96,000
					   bytes a second at most, in codec mode
					   60,000 each way: 16-bit PCM up to 48 kHz
					   mono or 24 kHz stereo, FLAC stereo up to
					   32 kHz; mu-law, MP3 and Ogg always. The
					   Zorro II card takes every setting. */

/* The clockport's limit behind PRECERR_BUSRATE, bytes a second, against
 * what the encoder delivers: rate x channels x 2 for 16-bit PCM, x 1 for
 * mu-law and A-law, / 2 for IMA ADPCM, 8000 a channel for G.722, and
 * 60 % of PCM for FLAC; MP3 and Ogg Vorbis are never refused. Channels:
 * 1 for PRECCH_LEFT, PRECCH_RIGHT and PRECCH_MONO, else 2. */
#define PREC_CLOCKPORT_BYTES	96000UL	/* recording alone */
#define PREC_CLOCKPORT_DUPLEX	60000UL	/* each way, with PRECF_CODEC */

/*
 * Prisma_GetCardAttr() attributes (the first ten read only; since 3.0 also
 * Prisma_SetCardAttr(), see the master layer below)
 */
#define PCA_PATCHSTATUS		1	/* PPATCH_#? below */
#define PCA_PATCHWORDS		2	/* words in the loaded VLSI patch */
#define PCA_CLOCKF		3	/* value written to SCI_CLOCKF */
#define PCA_CHIPVERSION		4	/* SCI_STATUS SS_VER (6 = VS1063) */
#define PCA_TIMEOUTS		5	/* DREQ waits that timed out since ObtainCard */
#define PCA_DREQ		6	/* current DREQ state (1 = ready) */
#define PCA_HARDRESETS		7	/* hardware resets of the chip by the library
					   since the machine started (each one powers
					   the outputs off and on: a thump) */
#define PCA_SOFTRESETS		8	/* software resets since the machine started
					   (after each recording, and when a stream
					   would not cancel) */
#define PCA_BOOT		9	/* PBOOT_#? below: how the chip was found at
					   its last boot by the library */
#define PCA_BOOTCLOCKF		10	/* the SCI_CLOCKF it had then (0 = fresh) */

/*
 * The master layer (since 3.0): a mixer's settings over whatever program
 * plays, get and set (Prisma_SetCardAttr(), the 39th vector: lib_NegSize
 * >= 234). The chip gets the owner's settings combined with these:
 * volumes and balances add as attenuations; EQ levels add (the owner's
 * when its equalizer is on), clamped to +-16 dB, on the owner's
 * crossovers or else the master's fixed ones (126, 500, 2000, 8000 Hz);
 * bass boosts add (15 dB at most) and trebles add (-8..7), each at the
 * master's frequency where the master sets that part; mono if either
 * asks; EarSpeaker the master's unless off. The datasheet's rules hold on
 * the result: a bass boost or treble from either layer stops the 5-band
 * equalizer, the owner's tempo (speed shifter) stops EarSpeaker.
 * Values out of range are clamped. Kept in prisma.prefs (struct
 * PrismaSettings, ps_Master#?): SetPrisma saves them.
 *
 * For Prisma_GetCardAttr() and Prisma_SetCardAttr() card may be NULL:
 * the card whoever has it, or nobody -- a mixer must not hold the card
 * (players could not obtain it), and pb_Card is set only while someone
 * does. NULL needs the card found once since the library was loaded (any
 * Prisma_ObtainCard(); a mixer obtains and releases it at its start).
 */
#define PCA_MASTERVOLUME	11	/* 0..100 (default 100: as the program plays) */
#define PCA_MASTERBALANCE	12	/* 0..100 (default 50: centre) */
#define PCA_MASTEREQBASS	13	/* EQ levels, 0.5 dB steps -32..32 (LONG): */
#define PCA_MASTEREQMIDBASS	14	/*   the five bands, centred on 64, 250, */
#define PCA_MASTEREQMID		15	/*   1000, 4000 and 16000 Hz with the */
#define PCA_MASTEREQMIDHIGH	16	/*   master's crossovers */
#define PCA_MASTEREQTREBLE	17
#define PCA_MASTERBASSBOOST	18	/* bass enhancer, dB 0..15 (0 = off) */
#define PCA_MASTERBASSFREQ	19	/* boosted below this, Hz 20..150 (10 Hz steps) */
#define PCA_MASTERTREBLE	20	/* treble in 1.5 dB steps, -8..7 (LONG; 0 = off) */
#define PCA_MASTERTREBLEFREQ	21	/* treble from this, Hz 1000..15000 (1 kHz steps) */
#define PCA_MASTERMONO		22	/* 1: left and right averaged */
#define PCA_MASTEREARSPEAKER	23	/* headphone spatialiser, 0..65535: PEARSPEAKER_#? */

/* Read only */
#define PCA_OWNER		24	/* (STRPTR) the program that has the card: its
					   command name, else its task's name; NULL
					   when nobody. A copy kept by the library,
					   valid until the next owner. */
#define PCA_VUMETER		25	/* the chip's VU meter now, as pc_VuMeter
					   (left in bits 15..8); 0 while it is off
					   (PCA_VUMETERON, PCONFIG_VUMETER_ENABLE)
					   and while no stream plays (stopped,
					   paused): it shows the stream */

/* The card's input and VU meter (get and set, card NULL allowed): what a
 * mixer needs, also while nobody plays and it has no card pointer. They
 * replace 1.x's Prisma_SetInputSource(), _SetMixerGain(), _EnableMixer(),
 * _SetMixerParams(), _EnableVUMeter() and _SetClockportInt(), whose
 * vectors stay for old programs. */
#define PCA_INPUTSOURCE		26	/* PM_INPUT_#?: the line input's source */
#define PCA_INPUTMIX		27	/* 1: the input passed through to the
					   output (the chip's AD mixer) */
#define PCA_INPUTGAIN		28	/* its gain, dB -31..-3 (LONG) */
#define PCA_INPUTCHANNELS	29	/* PM_MIXER_STEREO/MONO/LEFT/RIGHT (the
					   rate stays) */
#define PCA_VUMETERON		30	/* 1: the VU meter runs (pc_VuMeter,
					   PCA_VUMETER) */
#define PCA_CLOCKPORTINT	31	/* 2 or 6: the Zorro II card's clockport
					   interrupt, INT2 or INT6; a card on a
					   clockport answers 0 and refuses */
#define PCA_BUSTYPE		32	/* (read only) PM_BUSTYPE_#? */

/* Diagnostics (read only, since 3.0): every stream start compares the chip
 * with what the library left it -- SCI_MODE without a pending cancel or the
 * encoder, playMode, SCI_VOL -- and puts right what differs (something
 * wrote it behind the library's back). PrismaDiag shows these. */
#define PCA_REPAIRS		33	/* how many stream starts had to */
#define PCA_REPAIRWHAT		34	/* the last: (PREPAIR_#? << 16) | SCI_MODE found */
#define PCA_REPAIRPLAYMODE	35	/* the last: (playMode found << 16) | wanted */
#define PCA_REPAIRVOLUME	36	/* the last: (SCI_VOL found << 16) | wanted */

#define PREPAIR_MODE		0x0001	/* SM_CANCEL or SM_ENCODE set: soft reset */
#define PREPAIR_PLAYMODE	0x0002
#define PREPAIR_VOLUME		0x0004

/* (read only, since 3.0) How much quieter the card plays than the volumes
 * ask, in half-dB steps; 0: not at all. The chip limits each equalizer
 * band to level + attenuation <= 0 dB, so where the owner's and the
 * master's volume together leave less attenuation than the largest boost,
 * the library adds the rest -- the boosted band reaches 0 dB, nothing
 * clips, and turning the volume up further changes nothing. A mixer shows
 * this as "too loud for the equalizer". */
#define PCA_HEADROOM		37

/* (read only, since 3.0) A stream that races: the chip takes it far faster
 * than any stream of its kind plays -- an MP3 three times faster than the
 * fastest MP3, a WAV than its own header says -- for three seconds in a
 * row. Not a fault of the stream: the card's reads go wrong (DREQ read as
 * "room"), or the data reaches the chip damaged, and the chip throws away
 * what is no audio as fast as it comes. Either way nothing is heard and a
 * player runs through its file in seconds. Another card disturbing the
 * Prisma on the bus does that (an A4000D with an A4092 SCSI controller,
 * 2026-10-10: see PrismaDiag BUSCHECK). The start of a stream is not
 * judged: the chip skips cover art and headers as fast as that, rightly.
 * Unknown kinds are not judged at all. */
#define PCA_RACING		38	/* 1 while the stream playing races */
#define PCA_RACES		39	/* races since the library was loaded */

/* (read only, since 3.1) One more at every reset of the chip and every
 * load of a patch package -- the boot, the reset that ends a recording,
 * the FLAC variant. What a client put into the chip's RAM itself
 * (a plugin, an interrupt vector: prismadsp.library's) is gone once this
 * has moved: prismadsp.library loads its plugins again then. */
#define PCA_CHIPGENERATION	40

#define PBOOT_NONE		0	/* not known: a library older than PCA_BOOT
					   (once the card is obtained, this one has
					   always booted or taken over the chip) */
#define PBOOT_FRESH		1	/* straight out of the machine's reset */
#define PBOOT_SOFT		2	/* running, but not as booted (a warm reboot
					   keeps it running): software reset */
#define PBOOT_HARD		3	/* not answering: hardware reset (a thump) */

#define PPATCH_NONE		0	/* no patch file found */
#define PPATCH_LOADED		1	/* patch loaded and started */
#define PPATCH_BADFILE		2	/* patch file unreadable or malformed */

#endif
