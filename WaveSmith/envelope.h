/*
 * envelope.h -- the level of the sound (concept 3.2): a part's gain, a
 * fade's curve, the fades at In and Out, the dips at the cuts, all in
 * fixed point, 4.12 (ENV_UNITY, 4096, is the sound as recorded), and the
 * samples it is applied to: 16-bit little-endian, as a WAV holds them.
 *
 * A level holds ENV_BLOCK frames (0.36 ms at 44.1 kHz: no step the ear
 * hears); each sample then costs one 16 x 16 multiplication and a shift,
 * a 68000's MULS.W -- why a gain goes no higher than ENV_MAX, +18 dB.
 *
 * Pure: host-tested (test/host/wavesmith/test_envelope.c).
 */
#ifndef WAVESMITH_ENVELOPE_H
#define WAVESMITH_ENVELOPE_H

#include <exec/types.h>

#define ENV_UNITY	4096U		/* 4.12: as recorded, 0 dB */
#define ENV_MAX		32767U		/* +18.06 dB */
#define ENV_BLOCK	16UL		/* the frames a level holds */
#define ENV_U_MAX	65535U		/* a place on a curve: 0 its start, this its end */

/* the shapes (Settings, Fades...: question F2) */
#define ENV_SHAPE_LINEAR	0	/* the amplitude in a straight line */
#define ENV_SHAPE_EVEN		1	/* the amplitude squared: the loudness falls evenly */
#define ENV_SHAPE_SCURVE	2	/* (1 - cos) / 2: gentle at both ends */
#define ENV_SHAPES		3

/* a gain in tenths of a dB (F6) */
#define ENV_TENTHS_MIN	(-240)
#define ENV_TENTHS_MAX	180

/* The curve rising from 0 at u 0 to ENV_UNITY at ENV_U_MAX: a fade in
 * reads it forwards, a fade out from its end (ENV_U_MAX - u) */
UWORD Env_Curve(UWORD shape, UWORD u);

/* Tenths of a dB as a level (clamped to ENV_TENTHS_MIN .. _MAX) */
UWORD Env_FromTenths(WORD tenths);

/* A level as the nearest tenths of a dB, ENV_TENTHS_MIN at most down;
 * ENV_TENTHS_MIN - 1 for 0 (nothing at all) */
WORD Env_Tenths(UWORD level);

/* a times b, both 4.12, rounded: ENV_MAX at most */
UWORD Env_Mul(UWORD a, UWORD b);

/* A look's byte (dB below full scale, 0 .. 90; ENV_LOOK_NONE nothing --
 * levelmap.h's LEVELMAP_NONE) heard at a level: a gain lifts it, a fade
 * lowers it, in whole dB; a level of nothing makes it ENV_LOOK_NONE. The
 * .prl cut (mapcut.c) and a saved doc's looks (doc.c) alike. */
#define ENV_LOOK_NONE	255
UBYTE Env_LookDb(UBYTE look, UWORD level);

/* A stretch of frames at one level, in place: ea_Frames frames of
 * ea_Channels 16-bit little-endian samples at ea_Buf. A sample the level
 * takes past full scale is held there and counted into ea_Clipped (the
 * caller clears it); the level ENV_UNITY leaves every byte as it was. */
struct EnvApply {
	UBYTE *ea_Buf;
	ULONG ea_Frames;
	UWORD ea_Channels;
	UWORD ea_Level;
	ULONG ea_Clipped;
};
void Env_Apply(struct EnvApply *ea);

#endif
