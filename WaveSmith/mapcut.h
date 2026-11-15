#ifndef WAVESMITH_MAPCUT_H
#define WAVESMITH_MAPCUT_H

/*
 * mapcut.h -- the level map beside a take (PrismaRec's <name>.prl,
 * levelmap.h) cut as the take was (concept 5.4): the looks from In's to
 * the last kept frame's, so that the take list's picture and the gap
 * detector see the file as it is now. A map is an extra (lmapfile.h):
 * one that cannot be cut is removed -- a stale map is worse than none; a
 * file that is no PRL1 map is left alone.
 *
 * A map cut once is exact. Its first look then starts before the new
 * file does (In falls inside a look), and the PRL1 header has no place
 * for that offset: a take trimmed again is cut on the file's own grid,
 * up to one look (40 ms) off at either end.
 *
 * A take with cuts in it (mc_Tl, timeline.h): each look of the new file
 * the old look its first frame came from, a silence's LEVELMAP_NONE --
 * a look off at each join at most. With a level (concept 3.2: a gain, a
 * fade, mc_Levels' end fades and dips), each look's dB moved by the level
 * at its middle frame; a level of nothing makes it LEVELMAP_NONE.
 */

#include <exec/types.h>

#include "timeline.h"

struct MapCut {
	CONST_STRPTR mc_From;		/* the take the map was made for */
	CONST_STRPTR mc_To;		/* the take written (mc_From in place) */
	ULONG mc_In, mc_Out;		/* the frames kept: mc_In .. mc_Out - 1 */
	ULONG mc_Rate;			/* frames a second */
	const struct Timeline *mc_Tl;	/* the frames' timeline; NULL: the file's
					   own (no cut in it, no level) */
	const struct TlLevels *mc_Levels;	/* with mc_Tl: the end fades and
					   dips (NULL: none) */
	BOOL mc_Backup;			/* the map replaced kept as <name>.bak, as
					   Doc_Save keeps the take (Doc_SetBackup) */
};

/* MapCut_Cut()'s answers */
#define MAPCUT_NONE	0		/* no map beside mc_From (or none of ours) */
#define MAPCUT_CUT	1		/* the cut map beside mc_To */
#define MAPCUT_REMOVED	2		/* it could not be cut, or nothing was left */

/* The looks a cut keeps of count looks of micros each: first .. end - 1
   (pure; end == first: none) */
struct MapRange {
	ULONG mr_First, mr_End;
};
void MapCut_Range(const struct MapCut *mc, ULONG micros, ULONG count, struct MapRange *mr);

/* the map beside mc_From cut into the one beside mc_To: MAPCUT_#? */
UWORD MapCut_Cut(const struct MapCut *mc);

#endif
