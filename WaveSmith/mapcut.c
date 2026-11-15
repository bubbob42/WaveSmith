/*
 * mapcut.c -- a take's level map cut as the take was (mapcut.h)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "mapcut.h"
#include "levelmap.h"
#include "waveplan.h"
#include "beside.h"
#include "textutil.h"

#define NAME_LEN	256
#define MAP_MAX		4000000UL	/* bytes: 44 hours of looks; more is no map */

/* the look frame lies in: ms x 1000 / micros, without the product that
   wraps after 71 minutes */
static ULONG look_of(const struct MapCut *mc, ULONG frame, ULONG micros)
{
	ULONG ms = WavePlan_FramesToMs(frame, mc->mc_Rate);

	return (ms / micros) * 1000UL + (ms % micros) * 1000UL / micros;
}

void MapCut_Range(const struct MapCut *mc, ULONG micros, ULONG count, struct MapRange *mr)
{
	mr->mr_First = mr->mr_End = 0;
	if (!micros || !mc->mc_Rate || mc->mc_Out <= mc->mc_In) return;
	mr->mr_First = look_of(mc, mc->mc_In, micros);
	mr->mr_End = look_of(mc, mc->mc_Out - 1, micros) + 1;
	if (mr->mr_End > count) mr->mr_End = count;
	if (mr->mr_First > mr->mr_End) mr->mr_First = mr->mr_End;
}

/* the map's bytes: its header, then a byte a look; the looks kept */
struct Map {
	UBYTE *mp_Data;
	ULONG mp_Size;
	struct MapRange mp_Keep;
};

/* a take with cuts: its new map made from the old one (rm_To's data the
   caller frees) */
struct Remap {
	const struct Map *rm_From;
	struct Map *rm_To;
	ULONG rm_Micros;
};

/* the frames from a file's start to its look k's first: k x micros
   microseconds at the rate, without the product that wraps */
static ULONG look_start(const struct MapCut *mc, ULONG k, ULONG micros)
{
	ULONG ms = k * (micros / 1000UL) + k / 1000UL * (micros % 1000UL) + k % 1000UL * (micros % 1000UL) / 1000UL;

	return WavePlan_MsToFrames(ms, mc->mc_Rate);
}

/* half a look of micros, in frames */
static ULONG look_half(const struct MapCut *mc, ULONG micros)
{
	return WavePlan_MsToFrames(micros / 2000UL, mc->mc_Rate);
}

/* the new file's looks along the timeline: each the old look its first
   frame came from, silence LEVELMAP_NONE, at its level: FALSE without
   the memory */
static BOOL remap(const struct MapCut *mc, struct Remap *rm)
{
	struct TlLook tk;
	ULONG looks, k, src, count = rm->rm_From->mp_Size - LEVELMAP_HEADER;
	UBYTE *out;

	looks = look_of(mc, mc->mc_Out - 1 - mc->mc_In, rm->rm_Micros) + 1;
	if (!(out = (UBYTE *) AllocVec(LEVELMAP_HEADER + looks, MEMF_ANY))) return FALSE;
	for (k = 0; k < LEVELMAP_HEADER; k++) out[k] = rm->rm_From->mp_Data[k];
	tk.tk_Levels = mc->mc_Levels;
	for (k = 0; k < looks; k++) {
		out[LEVELMAP_HEADER + k] = LEVELMAP_NONE;
		tk.tk_First = mc->mc_In + look_start(mc, k, rm->rm_Micros);
		tk.tk_Mid = tk.tk_First + look_half(mc, rm->rm_Micros);
		if (!Tl_Look(mc->mc_Tl, &tk) || tk.tk_From == TL_SILENCE) continue;
		src = look_of(mc, tk.tk_From, rm->rm_Micros);
		if (src < count) out[LEVELMAP_HEADER + k] = Env_LookDb(rm->rm_From->mp_Data[LEVELMAP_HEADER + src], tk.tk_Level);
	}
	rm->rm_To->mp_Data = out;
	rm->rm_To->mp_Size = LEVELMAP_HEADER + looks;
	rm->rm_To->mp_Keep.mr_First = 0;
	rm->rm_To->mp_Keep.mr_End = looks;
	return TRUE;
}

/* the whole map from name into mp (the caller frees mp_Data): TRUE when
   it is one of ours */
static BOOL load_map(CONST_STRPTR name, struct Map *mp)
{
	BPTR fh;
	LONG size;
	BOOL ok = FALSE;

	mp->mp_Data = NULL;
	mp->mp_Size = 0;
	if (!(fh = Open((STRPTR) name, MODE_OLDFILE))) return FALSE;
	if (Seek(fh, 0, OFFSET_END) != -1 && (size = Seek(fh, 0, OFFSET_BEGINNING)) >= (LONG) LEVELMAP_HEADER &&
	    (ULONG) size <= MAP_MAX && (mp->mp_Data = (UBYTE *) AllocVec((ULONG) size, MEMF_ANY))) {
		mp->mp_Size = (ULONG) size;
		ok = (BOOL) (Read(fh, mp->mp_Data, size) == size && mp->mp_Data[0] == 'P' && mp->mp_Data[1] == 'R' &&
			     mp->mp_Data[2] == 'L' && mp->mp_Data[3] == '1');
	}
	Close(fh);
	if (!ok && mp->mp_Data) {
		FreeVec(mp->mp_Data);
		mp->mp_Data = NULL;
	}
	return ok;
}

/* name out of the new map's way: deleted, or kept as <name>.bak (an
   older one replaced); TRUE when it is gone or was never there */
static BOOL set_aside(CONST_STRPTR name, BOOL backup)
{
	char bak[BESIDE_LEN];

	if (!backup) return (BOOL) (DeleteFile((STRPTR) name) || IoErr() == ERROR_OBJECT_NOT_FOUND);
	/* the backup's name cut to fit, as the take's (beside.h) */
	if (!Beside_BackupName(name, (STRPTR) bak)) return FALSE;
	if (!DeleteFile((STRPTR) bak) && IoErr() != ERROR_OBJECT_NOT_FOUND) return FALSE;
	return (BOOL) (Rename((STRPTR) name, (STRPTR) bak) || IoErr() == ERROR_OBJECT_NOT_FOUND);
}

/* the header and the looks first .. end - 1 into name, beside it under a
   temporary name, then renamed over whatever was there (kept as a
   backup when told): TRUE when done */
static BOOL write_map(const struct Map *mp, CONST_STRPTR name, BOOL backup)
{
	char tmp[BESIDE_LEN];
	BPTR fh;
	ULONG n = mp->mp_Keep.mr_End - mp->mp_Keep.mr_First;
	BOOL ok;

	/* a short temporary name of its own: "<name>.ped" could be cut by FFS
	   into the map's own name, and opening it would empty the map */
	if (!Beside_Temp(name, (STRPTR) tmp) || !(fh = Open((STRPTR) tmp, MODE_NEWFILE))) return FALSE;
	ok = (BOOL) (Write(fh, mp->mp_Data, LEVELMAP_HEADER) == (LONG) LEVELMAP_HEADER &&
		     Write(fh, mp->mp_Data + LEVELMAP_HEADER + mp->mp_Keep.mr_First, (LONG) n) == (LONG) n);
	if (!Close(fh)) ok = FALSE;
	if (ok && !set_aside(name, backup)) ok = FALSE;
	if (ok && !Rename((STRPTR) tmp, (STRPTR) name)) ok = FALSE;
	if (!ok) DeleteFile((STRPTR) tmp);
	return ok;
}

UWORD MapCut_Cut(const struct MapCut *mc)
{
	char from[NAME_LEN], to[NAME_LEN];
	struct Map mp, cut;
	struct Remap rm;
	ULONG micros;
	UWORD r = MAPCUT_REMOVED;

	if (!LevelMap_Name(mc->mc_From, (STRPTR) from, NAME_LEN) || !LevelMap_Name(mc->mc_To, (STRPTR) to, NAME_LEN))
		return MAPCUT_NONE;
	if (!load_map((CONST_STRPTR) from, &mp)) {
		/* none beside the source: a map of ours beside another file
		   written over now describes a sound that is not there */
		if (!Text_Same((CONST_STRPTR) from, (CONST_STRPTR) to) && load_map((CONST_STRPTR) to, &mp)) {
			FreeVec(mp.mp_Data);
			set_aside((CONST_STRPTR) to, mc->mc_Backup);
			return MAPCUT_REMOVED;
		}
		return MAPCUT_NONE;
	}
	micros = ((ULONG) mp.mp_Data[4] << 24) | ((ULONG) mp.mp_Data[5] << 16) | ((ULONG) mp.mp_Data[6] << 8) | mp.mp_Data[7];
	if (mc->mc_Tl && micros && mc->mc_Rate && mc->mc_Out > mc->mc_In) {
		/* cuts in the take: the looks along its timeline */
		rm.rm_From = &mp;
		rm.rm_To = &cut;
		rm.rm_Micros = micros;
		if (remap(mc, &rm)) {
			if (write_map(&cut, (CONST_STRPTR) to, mc->mc_Backup)) r = MAPCUT_CUT;
			FreeVec(cut.mp_Data);
		}
	} else {
		MapCut_Range(mc, micros, mp.mp_Size - LEVELMAP_HEADER, &mp.mp_Keep);
		if (mp.mp_Keep.mr_End > mp.mp_Keep.mr_First && write_map(&mp, (CONST_STRPTR) to, mc->mc_Backup)) r = MAPCUT_CUT;
	}
	FreeVec(mp.mp_Data);
	/* nothing left, or no new map: the old one beside the new take goes,
	   it would describe a file that is not there (a backup's map kept
	   beside the backup) */
	if (r != MAPCUT_CUT) set_aside((CONST_STRPTR) to, mc->mc_Backup);
	return r;
}
