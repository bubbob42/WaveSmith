#ifndef WAVESMITH_TIMELINE_H
#define WAVESMITH_TIMELINE_H

/*
 * timeline.h -- the sound as WaveSmith shows, plays and saves it: the
 * edit list (concept 3.1 and 5.3), pieces one after the other, each a
 * stretch of the file or silence. A file just opened is one piece, the
 * whole of it. A cut of a .. b - 1 splits the pieces at a and b and
 * drops what lies between (the parts joined) or puts silence of the same
 * length there. The file is not touched: Save writes the timeline.
 *
 * Frames are the timeline's, 0 .. tl_Frames - 1, but a piece's p_From,
 * which is the file's. Pieces that follow each other in the file are one
 * piece, and silences next to each other one silence: a timeline has a
 * single way to be written, so two can be compared piece by piece.
 *
 * A piece also carries its level (concept 3.2, envelope.h): a gain, and a
 * fade's curve with the stretch of it the piece holds. A gain or a fade
 * splits the pieces at the part's edges as a cut does; a fade cut through
 * later keeps its curve, each half its own stretch. Pieces that follow
 * each other in the file are one only when their levels run on.
 *
 * Pure but for Tl_Read() (dos.library): host-tested
 * (test/host/wavesmith/test_timeline.c).
 */

#include <exec/types.h>
#include <dos/dos.h>

#include "envelope.h"

#define TL_SILENCE	0xFFFFFFFFUL	/* p_From of a silence */
#define TL_NONE		0xFFFFFFFFUL	/* no such frame */
#define TL_MAXPIECES	512		/* a cut adds two at most */

/* p_Fade: a direction, and the shape in the low bits (ENV_SHAPE_#?) */
#define TL_FADE_NONE	0x00
#define TL_FADE_IN	0x10
#define TL_FADE_OUT	0x20
#define TL_FADE_DIR	0x30
#define TL_FADE_SHAPE	0x0F

struct Piece {
	ULONG p_From;			/* the file's first frame, or TL_SILENCE */
	ULONG p_Frames;
	UWORD p_Gain;			/* 4.12: ENV_UNITY as recorded (a silence's too) */
	UBYTE p_Fade;			/* TL_FADE_#?: NONE, or a direction and a shape */
	UBYTE p_Pad;
	UWORD p_U0, p_U1;		/* the piece's stretch of its fade's curve, its
					   first frame's place and its end's (ENV_U_MAX) */
};

struct Timeline {
	ULONG tl_Frames;		/* the pieces' together */
	UWORD tl_Count;
	UWORD tl_Pad;
	struct Piece tl_Pieces[TL_MAXPIECES];
};

/* one piece, the whole of a file of frames (0: no piece) */
void Tl_Init(struct Timeline *tl, ULONG frames);
/* the file as it is, its frames all: no piece (frames 0), or one from
   its first frame to its last */
BOOL Tl_Whole(const struct Timeline *tl, ULONG frames);
/* the same pieces; the same piece, its level too */
BOOL Tl_Same(const struct Timeline *a, const struct Timeline *b);
BOOL Tl_PieceSame(const struct Piece *a, const struct Piece *b);

/* where timeline frame t comes from */
struct TlAt {
	UWORD ta_Piece;
	ULONG ta_Start;			/* the piece's first timeline frame */
	ULONG ta_From;			/* t's frame in the file, or TL_SILENCE */
	ULONG ta_Run;			/* frames from t to the piece's end */
};
/* FALSE: t at or past the end */
BOOL Tl_At(const struct Timeline *tl, ULONG t, struct TlAt *at);

/* first .. end - 1 cut out, the rest joined: FALSE (the timeline as it
   was) when nothing lies there (first >= end, end past the end) or the
   pieces would be too many */
BOOL Tl_Cut(struct Timeline *tl, ULONG first, ULONG end);
/* the same, silence of end - first frames in its place */
BOOL Tl_Silence(struct Timeline *tl, ULONG first, ULONG end);

/* a part's level: tp_First .. tp_End - 1; FALSE (the timeline as it was)
   when nothing lies there or the pieces would be too many. A silence in
   it keeps its own: nothing. */
struct TlPart {
	ULONG tp_First, tp_End;
	UWORD tp_Gain;			/* Tl_Gain: the factor, 4.12 */
	UBYTE tp_Fade;			/* Tl_Fade: TL_FADE_IN or _OUT, and a shape */
	UBYTE tp_Pad;
};
/* every piece's gain there times tp_Gain (ENV_MAX at most) */
BOOL Tl_Gain(struct Timeline *tl, const struct TlPart *pt);
/* a fade across the part, its curve from its first frame to its end; a
   fade there before is replaced */
BOOL Tl_Fade(struct Timeline *tl, const struct TlPart *pt);
/* the part as recorded again: no gain, no fade */
BOOL Tl_Plain(struct Timeline *tl, const struct TlPart *pt);

/* the level from outside the pieces: the fades at In and Out (which go
   with their handles) and the dips at the cuts (a setting) */
struct TlLevels {
	ULONG lv_In, lv_Out;		/* the timeline's In and Out */
	ULONG lv_FadeIn, lv_FadeOut;	/* frames after In, before Out: 0 none */
	ULONG lv_Dip;			/* frames either side of a cut: 0 none */
	UWORD lv_Shape;			/* the end fades' shape (ENV_SHAPE_#?) */
	UWORD lv_Pad;
};

/* the level timeline frame t plays at (ENV_UNITY outside the timeline);
   lv NULL: the pieces' alone */
UWORD Tl_LevelAt(const struct Timeline *tl, const struct TlLevels *lv, ULONG t);
/* frames first .. first + n - 1 play as recorded: no gain, no fade, no
   end fade or dip reaching in -- the file's bytes, a copy will do */
BOOL Tl_Untouched(const struct Timeline *tl, const struct TlLevels *lv, ULONG first, ULONG n);
/* lv when it holds a level, else NULL: the end fades or a dip set */
const struct TlLevels *Tl_Levels(const struct TlLevels *lv);

/* the timeline frame that plays file frame f, TL_NONE when it was cut
   out (the first, when it plays more than once) */
ULONG Tl_Find(const struct Timeline *tl, ULONG f);

/* Where a look of a stretch cut from the timeline comes from (the .prl's
   cut, mapcut.c, and a saved doc's looks, doc.c, alike): the file frame
   its first frame plays and the level at its middle. FALSE past the end;
   tk_From TL_SILENCE in a silence */
struct TlLook {
	ULONG tk_First, tk_Mid;			/* in: timeline frames */
	const struct TlLevels *tk_Levels;	/* in: from outside the pieces (NULL none) */
	ULONG tk_From;				/* out: the file frame, or TL_SILENCE */
	UWORD tk_Level;				/* out: the level at tk_Mid */
	UWORD tk_Pad;
};
BOOL Tl_Look(const struct Timeline *tl, struct TlLook *tk);

/* the joins: the timeline frames where a piece of the file follows
   another it did not follow in the file, or silence begins or ends, at
   most max into out; how many */
UWORD Tl_Joins(const struct Timeline *tl, ULONG *out, UWORD max);

/* where a stretch of the file plays: tr_First .. tr_End - 1 of the file
   in, the timeline's stretches out (in the timeline's order) */
struct TlRange {
	ULONG tg_First, tg_End;
};
struct TlRanges {
	ULONG tr_First, tr_End;		/* the file's */
	struct TlRange *tr_Out;
	UWORD tr_Max;
};
UWORD Tl_Ranges(const struct Timeline *tl, struct TlRanges *tr);

/* timeline frames from a file: rd_Frames from rd_First into rd_Buf as
   the file holds them (its byte order), silence as zeros, at their level
   (16-bit samples: rd_Align / 2 a frame); how many -- fewer at the
   timeline's end or where the file fails. The handle is left where the
   last read left it. A piece as recorded is the file's bytes, untouched. */
struct TlRead {
	BPTR rd_FH;
	ULONG rd_Start;			/* the samples' first byte in the file */
	UWORD rd_Align;			/* bytes a frame */
	UWORD rd_Pad;
	ULONG rd_First, rd_Frames;
	UBYTE *rd_Buf;
	const struct TlLevels *rd_Levels;	/* NULL: the pieces' levels alone */
	ULONG *rd_Clipped;		/* NULL, or samples held at full scale added */
	BOOL rd_AsRecorded;		/* no level at all: the file's bytes (a 68000
					   playing: too slow to compute them, F10) */
};
/* every field cleared, the levels none: the caller fills in the rest */
void Tl_ReadInit(struct TlRead *rd);
ULONG Tl_Read(const struct Timeline *tl, const struct TlRead *rd);

#endif
