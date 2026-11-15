/*
 * test_docsave.c -- WaveSmith's doc.c saving (Doc_Save in place, D6: no
 * backup; Doc_SaveAs), against dos.library on the host (doshost.c) that
 * refuses what AmigaDOS refuses and fails on demand: every file written
 * is held byte for byte against one built here from the original, and
 * after every failure the original must be as it was, the temporary file
 * gone, the doc still good where the contract says so.
 *   test_docsave <drawer>     (the drawer for the files, made beforehand)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "doc.h"
#include "mapcut.h"
#include "doshost.h"
#include "check.h"

#define RATE		44100UL
#define PATH_LEN	512

static struct Doc doc, doc2;
static const struct WaveSamples *fws;	/* the last fetch's */
static char dir[PATH_LEN];
static const struct DateStamp date0 = { 17000, 600, 1234 };

/* the test's WAV: its parts' sizes */
struct Layout {
	ULONG ly_Start;		/* the first sample's byte */
	ULONG ly_Data;		/* the samples' bytes */
	ULONG ly_Trailer;	/* the bytes after them */
	ULONG ly_Size;
	UWORD ly_Channels;
	ULONG ly_Frames;
};

struct Fixture {
	UWORD fx_Channels;
	ULONG fx_Frames;
	int fx_Before;		/* an odd LIST chunk between "fmt " and "data" */
	int fx_After;		/* an odd "id3 " chunk after the samples */
};

static void put16(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char) (v & 255);
	p[1] = (unsigned char) ((v >> 8) & 255);
}

static void put32(unsigned char *p, unsigned long v)
{
	put16(p, v & 0xFFFFUL);
	put16(p + 2, (v >> 16) & 0xFFFFUL);
}

static unsigned long sample(unsigned long frame, unsigned c)
{
	return (frame * 31UL + c * 7919UL + 5UL) & 0xFFFFUL;
}

/* a sample's top byte in the doc's picture, reckoned as doc.c does: the
   word as loaded (here the host's, little-endian like the file) put in
   the Amiga's order (LEWORD), then its top byte (TOP) -- on the host the
   picture is of the words swapped, the same on both sides of a check */
static BYTE doc_top(unsigned long v)
{
	UWORD w = (UWORD) v;
	LONG s = (LONG) (WORD) (UWORD) ((w << 8) | (w >> 8));

	return (BYTE) ((LONG) (((ULONG) (s + 32768L)) >> 8) - 128L);
}

static void path_of(char *buf, const char *name)
{
	sprintf(buf, "%s/%s", dir, name);
}

/* the fixture written to path, its layout into ly */
static int make_wav(const char *path, const struct Fixture *fx, struct Layout *ly)
{
	unsigned char h[64], w[4];
	FILE *f = fopen(path, "wb");
	unsigned long n = 0, i, align = fx->fx_Channels * 2UL;
	unsigned c;

	if (!f) return 0;
	ly->ly_Channels = fx->fx_Channels;
	ly->ly_Frames = fx->fx_Frames;
	ly->ly_Data = fx->fx_Frames * align;
	ly->ly_Start = 12 + 24 + (fx->fx_Before ? 14 : 0) + 8;
	ly->ly_Trailer = fx->fx_After ? 18 : 0;
	ly->ly_Size = ly->ly_Start + ly->ly_Data + ly->ly_Trailer;
	memcpy(h, "RIFF", 4);
	put32(h + 4, ly->ly_Size - 8);
	memcpy(h + 8, "WAVEfmt ", 8);
	put32(h + 16, 16);
	put16(h + 20, 1);
	put16(h + 22, fx->fx_Channels);
	put32(h + 24, RATE);
	put32(h + 28, RATE * align);
	put16(h + 32, align);
	put16(h + 34, 16);
	n = 36;
	if (fx->fx_Before) {
		memcpy(h + n, "LIST", 4);
		put32(h + n + 4, 5);
		memcpy(h + n + 8, "INFOx", 5);
		h[n + 13] = 0;
		n += 14;
	}
	memcpy(h + n, "data", 4);
	put32(h + n + 4, ly->ly_Data);
	n += 8;
	fwrite(h, 1, n, f);
	for (i = 0; i < fx->fx_Frames; i++) {
		for (c = 0; c < fx->fx_Channels; c++) {
			put16(w, sample(i, c));
			fwrite(w, 1, 2, f);
		}
	}
	if (fx->fx_After) fwrite("id3 \011\0\0\0ID3abcdef\0", 1, 18, f);
	fclose(f);
	return 1;
}

static unsigned char *load(const char *path, unsigned long *size)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;
	long n;

	*size = 0;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (!(b = malloc(n ? (size_t) n : 1)) || fread(b, 1, (size_t) n, f) != (size_t) n) {
		fclose(f);
		free(b);
		return NULL;
	}
	fclose(f);
	*size = (unsigned long) n;
	return b;
}

static int exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
}

/* the file at path is the bytes given, all of them */
static int file_is(const char *path, const unsigned char *b, unsigned long n)
{
	unsigned long size;
	unsigned char *f = load(path, &size);
	int same = f && size == n && memcmp(f, b, n) == 0;

	free(f);
	return same;
}

/* what a save of in..out must write, from the original's bytes: its
   header with the sizes made true, the kept samples, the trailer */
static unsigned char *expect(const unsigned char *orig, const struct Layout *ly, unsigned long in,
			     unsigned long out, unsigned long *n)
{
	unsigned long align = ly->ly_Channels * 2UL, bytes = (out - in) * align;
	unsigned char *e;

	*n = ly->ly_Start + bytes + ly->ly_Trailer;
	if (!(e = malloc(*n))) return NULL;
	memcpy(e, orig, ly->ly_Start);
	put32(e + 4, *n - 8);
	put32(e + ly->ly_Start - 4, bytes);
	memcpy(e + ly->ly_Start, orig + ly->ly_Start + in * align, bytes);
	memcpy(e + ly->ly_Start + bytes, orig + ly->ly_Start + ly->ly_Data, ly->ly_Trailer);
	return e;
}

static int saved_as(const char *path, const unsigned char *orig, const struct Layout *ly, unsigned long in,
		    unsigned long out)
{
	unsigned long n;
	unsigned char *e = expect(orig, ly, in, out, &n);
	int ok = e && file_is(path, e, n);

	free(e);
	return ok;
}

static int open_all(const char *path)
{
	if (Doc_Open(&doc, (CONST_STRPTR) path) != DOC_OK) {
		Doc_Close(&doc);
		return 0;
	}
	while (Doc_Read(&doc)) ;
	return 1;
}

/* 64 frames from first fetched: the samples the original has from
   base + first (in memory as the Amiga holds them: high byte first) */
static int fetch_ok(unsigned long first, unsigned long base)
{
	const struct WaveSamples *ws;
	const unsigned char *p;
	unsigned long i, v;
	unsigned c;

	if (!(ws = Doc_Fetch(&doc, first, 64))) return 0;
	if (ws->wsm_First != first || ws->wsm_Frames != 64) return 0;
	p = (const unsigned char *) ws->wsm_Data;
	for (i = 0; i < 64; i++) {
		for (c = 0; c < doc.d_Channels; c++, p += 2) {
			v = sample(base + first + i, c);
			if (p[0] != ((v >> 8) & 255) || p[1] != (v & 255)) return 0;
		}
	}
	return 1;
}

static void set_meta(const char *path)
{
	SetProtection((CONST_STRPTR) path, 0x51);
	SetComment((CONST_STRPTR) path, (CONST_STRPTR) "Side A, 1981");
	SetFileDate((CONST_STRPTR) path, &date0);
}

static int meta_kept(const char *path)
{
	struct FileInfoBlock fib;
	BPTR lock;
	int ok;

	if (!(lock = Lock((CONST_STRPTR) path, SHARED_LOCK))) return 0;
	ok = Examine(lock, &fib) && fib.fib_Protection == 0x51 && strcmp(fib.fib_Comment, "Side A, 1981") == 0 &&
	     fib.fib_Date.ds_Days == date0.ds_Days && fib.fib_Date.ds_Minute == date0.ds_Minute &&
	     fib.fib_Date.ds_Tick == date0.ds_Tick;
	UnLock(lock);
	return ok;
}

/* a save's progress as told: how often, the last, never back */
static int prog_calls = 0, prog_last = -1, prog_back = 0, prog_stop = 101;

/* TRUE: on; FALSE once prog_stop per cent is reached (the user's Cancel) */
static BOOL progress(UWORD pc)
{
	if ((int) pc <= prog_last) prog_back++;
	prog_last = (int) pc;
	prog_calls++;
	return (BOOL) ((int) pc < prog_stop);
}

static int held_nothing(void)
{
	if (dos_host.dh_Vecs || dos_host.dh_Handles || dos_host.dh_Locks || dos_host.dh_Objs)
		printf("     held: %ld blocks, %ld files, %ld locks, %ld objects\n", dos_host.dh_Vecs, dos_host.dh_Handles,
		       dos_host.dh_Locks, dos_host.dh_Objs);
	return !dos_host.dh_Vecs && !dos_host.dh_Handles && !dos_host.dh_Locks && !dos_host.dh_Objs;
}

static int closed(void)
{
	return doc.d_FH == 0 && doc.d_Path[0] == '\0' && doc.d_Head == NULL;
}

/* the doc as it was after a save that failed: open, read, fetching */
static int intact(void)
{
	return doc.d_FH != 0 && doc.d_Done && fetch_ok(5000, 0);
}

/* the doc a save kept (DOC_KEPT): open on the file saved, read, its
   timeline the whole file, no level from outside -- and its picture and
   plain peaks those a reading of that file makes, byte for byte */
static int kept_as_read(const char *path)
{
	UWORD l;
	ULONG k;
	int ok;

	if (!doc.d_FH || !doc.d_Done || !doc.d_PlanReady || doc.d_Tl.tl_Count != 1 || doc.d_Levels.lv_FadeIn ||
	    doc.d_Levels.lv_FadeOut)
		return 0;
	if (Doc_Open(&doc2, (CONST_STRPTR) path) != DOC_OK) {
		Doc_Close(&doc2);
		return 0;
	}
	while (Doc_Read(&doc2)) ;
	ok = doc.d_Frames == doc2.d_Frames && doc.d_Tl.tl_Frames == doc2.d_Frames && doc.d_Pic.dp_PeakCount == doc2.d_Pic.dp_PeakCount &&
	     doc.d_Wav.nw_DataStart == doc2.d_Wav.nw_DataStart && doc.d_Trailer == doc2.d_Trailer;
	for (l = 0; ok && l < WAVEPEAKS_LEVELS; l++)
		ok = doc.d_Pic.dp_Peaks.wp_Entries[l] == doc2.d_Pic.dp_Peaks.wp_Entries[l] &&
		     memcmp(doc.d_Pic.dp_Peaks.wp_Level[l], doc2.d_Pic.dp_Peaks.wp_Level[l], WavePeaks_Bytes(&doc2.d_Pic.dp_Peaks, l)) == 0;
	for (k = 0; ok && k < doc.d_Pic.dp_PeakCount; k++)
		ok = doc.d_Pic.dp_PeakLooks[k] == doc2.d_Pic.dp_PeakLooks[k] && doc.d_Pic.dp_PeakWords[k] == doc2.d_Pic.dp_PeakWords[k];
	if (!ok) printf("     kept: frames %lu/%lu peaks %lu/%lu\n", (unsigned long) doc.d_Frames, (unsigned long) doc2.d_Frames,
			(unsigned long) doc.d_Pic.dp_PeakCount, (unsigned long) doc2.d_Pic.dp_PeakCount);
	Doc_Close(&doc2);
	return ok;
}

/* a fresh Trim.wav from the fixture, its bytes and layout; the names */
static unsigned char *orig;
static unsigned long orig_n;
static struct Layout ly;
static char trim[PATH_LEN], temp[PATH_LEN];

static int fresh(const struct Fixture *fx)
{
	free(orig);
	orig = NULL;
	DosHost_Reset();
	path_of(trim, "Trim.wav");
	path_of(temp, "WaveSmith-0.tmp");	/* the first temporary name (doc.h) */
	remove(trim);
	remove(temp);
	if (!make_wav(trim, fx, &ly) || !(orig = load(trim, &orig_n))) return 0;
	set_meta(trim);
	return open_all(trim);
}

int main(int argc, char **argv)
{
	static const struct Fixture stereo = { 2, 120000, 1, 1 }, mono = { 1, 30001, 0, 0 };
	char other[PATH_LEN], othertemp[PATH_LEN];
	struct DocSave ds;
	LONG err;

	if (argc < 2) return 2;
	strncpy(dir, argv[1], PATH_LEN - 1);

	/* 1: the whole kept: the file as it was, byte for byte */
	CHECK(fresh(&stereo), "whole: the fixture opened and read");
	CHECK(doc.d_Frames == stereo.fx_Frames && doc.d_Trailer == 18, "whole: frames and trailer as made");
	doc.d_SaveProgress = progress;
	err = Doc_Save(&doc, 0, doc.d_Frames);
	CHECK(prog_calls >= 5 && prog_last == 100 && !prog_back, "whole: the progress told, rising, to 100");

	CHECK(err == DOC_KEPT, "whole: saved, the doc kept");
	CHECK(kept_as_read(trim), "whole: the kept doc's picture and peaks a reading's, nothing read again");
	CHECK(file_is(trim, orig, orig_n), "whole: the file byte for byte as it was");
	CHECK(meta_kept(trim), "whole: protection, comment, date kept");
	CHECK(!exists(temp), "whole: no temporary file left");
	Doc_Close(&doc);
	CHECK(held_nothing(), "whole: nothing held once closed");

	/* 1b: a save called off half-way (Cancel): the file and the doc as they were */
	CHECK(fresh(&stereo), "cancel: opened");
	doc.d_SaveProgress = progress;
	prog_stop = 50;
	prog_last = -1;
	err = Doc_Save(&doc, 1000, doc.d_Frames - 1000);
	prog_stop = 101;
	CHECK(err == DOC_E_CANCELLED, "cancel: the save called off");
	CHECK(prog_last >= 50 && prog_last < 100, "cancel: stopped where it was told");
	CHECK(file_is(trim, orig, orig_n), "cancel: the file as it was");
	CHECK(!exists(temp), "cancel: no temporary file left");
	CHECK(intact(), "cancel: the doc as it was, still reading its file");
	Doc_Close(&doc);
	CHECK(held_nothing(), "cancel: nothing held");

	/* 2: a stretch kept, the header's chunks and the trailer with it */
	CHECK(fresh(&stereo), "trim: opened");
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == DOC_KEPT, "trim: saved, the doc kept");
	CHECK(saved_as(trim, orig, &ly, 12345, 67890), "trim: header, the kept samples, the trailer, byte for byte");
	CHECK(meta_kept(trim), "trim: protection, comment, date kept");
	CHECK(!exists(temp), "trim: no temporary file left");
	CHECK(kept_as_read(trim), "trim: the kept doc's picture and peaks a reading's");
	CHECK(fetch_ok(0, 12345) && fetch_ok(1000, 12345), "trim: the kept doc fetches the saved file, the original's from In");
	Doc_Close(&doc);
	CHECK(held_nothing(), "trim: nothing held once closed");
	CHECK(open_all(trim) && doc.d_Frames == 67890 - 12345 && doc.d_Trailer == 18, "trim: opened again, its length");
	CHECK(fetch_ok(0, 12345) && fetch_ok(1000, 12345), "trim: its samples the original's from In");
	Doc_Close(&doc);

	/* 3: mono, no chunks besides; Out past the end taken as the end */
	CHECK(fresh(&mono), "mono: opened");
	err = Doc_Save(&doc, 1, mono.fx_Frames + 1000);
	CHECK(err == DOC_KEPT, "mono: saved, the doc kept");
	CHECK(saved_as(trim, orig, &ly, 1, mono.fx_Frames), "mono: byte for byte, Out at the end");
	CHECK(kept_as_read(trim), "mono: the kept doc's picture and peaks a reading's");
	Doc_Close(&doc);
	CHECK(held_nothing(), "mono: nothing held once closed");

	/* 3b: no memory for the new file's picture: saved all the same, the
	   doc closed -- the window opens and reads it again, as it did */
	CHECK(fresh(&stereo), "nomem: opened");
	dos_host.dh_FailVecIn = 1;
	err = Doc_Save(&doc, 12345, 67890);
	dos_host.dh_FailVecIn = 0;
	CHECK(err == 0 && closed(), "nomem: saved, the doc closed");
	CHECK(saved_as(trim, orig, &ly, 12345, 67890) && !exists(temp), "nomem: byte for byte, no temporary file");
	CHECK(held_nothing(), "nomem: nothing held");

	/* 3c: a timeline with a cut and a gain: the samples written at their
	   level, the picture made of them -- a reading's of the saved file */
	CHECK(fresh(&stereo), "edited: opened");
	{
		struct TlPart pt;

		pt.tp_First = 40000;
		pt.tp_End = 50000;
		pt.tp_Gain = Env_FromTenths(60);
		pt.tp_Fade = TL_FADE_NONE;
		pt.tp_Pad = 0;
		CHECK(Tl_Cut(&doc.d_Tl, 20000, 30000) && Tl_Gain(&doc.d_Tl, &pt), "edited: a cut, a gain");
		Doc_Changed(&doc);
		pt.tp_First = 60000;
		pt.tp_End = 70000;
		pt.tp_Fade = TL_FADE_OUT | ENV_SHAPE_EVEN;
		CHECK(Tl_Fade(&doc.d_Tl, &pt), "edited: a fade");
	}
	doc.d_Levels.lv_In = 1000;
	doc.d_Levels.lv_Out = 100000;
	doc.d_Levels.lv_FadeIn = 3000;
	doc.d_Levels.lv_Shape = ENV_SHAPE_EVEN;
	err = Doc_Save(&doc, 1000, 100000);
	CHECK(err == DOC_KEPT && doc.d_Frames == 99000, "edited: saved, the doc kept, its length");
	CHECK(kept_as_read(trim), "edited: the kept doc's picture and peaks a reading's of the file saved");
	CHECK(doc.d_Pic.dp_LookMax >= doc.d_Pic.dp_PeakCount && doc.d_LL.ll_OutCount == doc.d_Pic.dp_PeakCount, "edited: as many looks as plain peaks");
	Doc_Close(&doc);
	CHECK(held_nothing(), "edited: nothing held once closed");

	/* 4: the disk full part-way: the original untouched, the doc good; a
	   second Save then (the header the first one changed) is right */
	CHECK(fresh(&stereo), "full: opened");
	dos_host.dh_WriteLeft = 100000;
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == ERROR_DISK_FULL, "full: the disk's error");
	CHECK(file_is(trim, orig, orig_n) && meta_kept(trim), "full: the original as it was");
	CHECK(!exists(temp), "full: the part written gone");
	CHECK(intact(), "full: the doc open, read, fetching");
	CHECK(kept_as_read(trim), "full: the doc's own picture and peaks back, a reading's of the original");
	dos_host.dh_WriteLeft = -1;
	err = Doc_Save(&doc, 2000, 99000);
	CHECK(err == DOC_KEPT && saved_as(trim, orig, &ly, 2000, 99000), "full: saved again with room, byte for byte");
	CHECK(kept_as_read(trim), "full: the kept doc's picture and peaks a reading's");
	Doc_Close(&doc);
	CHECK(held_nothing(), "full: nothing held once closed");

	/* 5: a drawer under the first temporary name: the next name taken,
	   the drawer left alone */
	{
		char temp1[PATH_LEN];

		CHECK(fresh(&stereo), "drawer: opened");
		CHECK(mkdir(temp, 0755) == 0, "drawer: made");
		path_of(temp1, "WaveSmith-1.tmp");
		err = Doc_Save(&doc, 12345, 67890);
		CHECK(err == DOC_KEPT && saved_as(trim, orig, &ly, 12345, 67890), "drawer: saved through the next name");
		CHECK(exists(temp) && !exists(temp1), "drawer: the drawer left alone, no temporary file");
		Doc_Close(&doc);
		rmdir(temp);
		CHECK(held_nothing(), "drawer: nothing held");
	}

	/* 5b: FFS takes 30 characters: a name of 30 with a backup -- the
	   temporary and the backup's names fit, apart from each other and
	   from the original (cut alike they were one file: the edits lost) */
	{
		char longn[PATH_LEN], bakn[PATH_LEN];
		static const char nm[] = "Side_A_Dark_Side_Of_Moon01.wav";	/* 30 */
		CONST_STRPTR bp;

		CHECK(fresh(&stereo), "long: opened");
		Doc_Close(&doc);
		path_of(longn, nm);
		remove(longn);
		CHECK(rename(trim, longn) == 0 && open_all(longn), "long: a 30-character name opened");
		Doc_SetBackup(TRUE);
		err = Doc_Save(&doc, 12345, 67890);
		Doc_SetBackup(FALSE);
		bp = (CONST_STRPTR) strrchr((const char *) Doc_LastBackup(), '/');
		bp = bp ? bp + 1 : Doc_LastBackup();
		CHECK(err == DOC_KEPT && saved_as(longn, orig, &ly, 12345, 67890), "long: saved, byte for byte");
		CHECK(strlen((const char *) bp) <= 30 && strcmp((const char *) bp, nm) != 0, "long: the backup's name fits, not the original's");
		path_of(bakn, (const char *) bp);
		CHECK(file_is(bakn, orig, orig_n), "long: the original kept as the backup");
		Doc_Close(&doc);
		remove(bakn);
		remove(longn);
		CHECK(held_nothing(), "long: nothing held");
	}

	/* 6: the new file's Close fails (its last bytes not on the disk) */
	CHECK(fresh(&stereo), "close: opened");
	dos_host.dh_FailClose = 1;
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == ERROR_DISK_FULL, "close: its error");
	CHECK(file_is(trim, orig, orig_n) && !exists(temp), "close: the original as it was, the new one gone");
	CHECK(intact(), "close: the doc open, read, fetching");
	Doc_Close(&doc);
	CHECK(held_nothing(), "close: nothing held");

	/* 7: the original cannot be deleted: kept, the doc open on it again */
	CHECK(fresh(&stereo), "delete: opened");
	dos_host.dh_FailDelete = 1;
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == ERROR_DELETE_PROTECTED, "delete: its error");
	CHECK(file_is(trim, orig, orig_n) && meta_kept(trim) && !exists(temp), "delete: the original as it was, the new one gone");
	CHECK(intact(), "delete: the doc open again, still read, fetching");
	Doc_Close(&doc);
	CHECK(held_nothing(), "delete: nothing held");

	/* 8: ... and cannot be opened again: the doc without its file */
	CHECK(fresh(&stereo), "reopen: opened");
	dos_host.dh_FailDelete = 1;
	dos_host.dh_FailOpenOld = 1;
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == ERROR_DELETE_PROTECTED && doc.d_FH == 0, "reopen: the error, no handle");
	CHECK(file_is(trim, orig, orig_n) && !exists(temp), "reopen: the original as it was");
	Doc_Close(&doc);
	CHECK(held_nothing(), "reopen: nothing held once closed");

	/* 9: the rename fails: the original gone, the trimmed one kept */
	CHECK(fresh(&stereo), "rename: opened");
	dos_host.dh_FailRename = 1;
	err = Doc_Save(&doc, 12345, 67890);
	CHECK(err == DOC_E_KEPT, "rename: DOC_E_KEPT");
	CHECK(!exists(trim) && saved_as(temp, orig, &ly, 12345, 67890), "rename: the trimmed sound under the temporary name");
	CHECK(closed() && held_nothing(), "rename: the doc closed, nothing held");
	remove(temp);

	/* 10: before the reading is done; In at Out */
	CHECK(fresh(&stereo), "early: made");
	Doc_Close(&doc);
	CHECK(Doc_Open(&doc, (CONST_STRPTR) trim) == DOC_OK, "early: opened, not read");
	err = Doc_Save(&doc, 0, 1000);
	CHECK(err == ERROR_OBJECT_IN_USE && !exists(temp) && file_is(trim, orig, orig_n), "early: refused, nothing written");
	while (Doc_Read(&doc)) ;
	err = Doc_Save(&doc, 5000, 5000);
	CHECK(err == DOC_E_EMPTY && !exists(temp) && file_is(trim, orig, orig_n), "empty: In at Out refused");
	CHECK(intact(), "empty: the doc as it was");
	Doc_Close(&doc);
	CHECK(held_nothing(), "early: nothing held");

	/* 11: Save As: a new file, then over it; the doc stays */
	CHECK(fresh(&stereo), "as: opened");
	path_of(other, "Side-trim.wav");
	path_of(othertemp, "WaveSmith-0.tmp");
	remove(other);
	ds.ds_In = 100;
	ds.ds_Out = 2000;
	ds.ds_Path = (CONST_STRPTR) other;
	err = Doc_SaveAs(&doc, &ds);
	CHECK(err == 0 && saved_as(other, orig, &ly, 100, 2000), "as: a new file, byte for byte");
	CHECK(intact(), "as: the doc open, read, fetching");
	ds.ds_In = 50000;
	ds.ds_Out = stereo.fx_Frames + 5;
	err = Doc_SaveAs(&doc, &ds);
	CHECK(err == 0 && saved_as(other, orig, &ly, 50000, stereo.fx_Frames), "as: over the file there, byte for byte");
	CHECK(!exists(othertemp) && file_is(trim, orig, orig_n), "as: no temporary file, the doc's file untouched");
	/* the disk full: the file there before stays */
	dos_host.dh_WriteLeft = 1000;
	ds.ds_In = 0;
	err = Doc_SaveAs(&doc, &ds);
	CHECK(err == ERROR_DISK_FULL && saved_as(other, orig, &ly, 50000, stereo.fx_Frames) && !exists(othertemp),
	      "as: the disk full, the file there kept");
	dos_host.dh_WriteLeft = -1;
	CHECK(intact(), "as: the doc still good");
	Doc_Close(&doc);
	CHECK(held_nothing(), "as: nothing held");

	/* 12: the level map beside a take (mapcut.c) */
	{
		static unsigned char map[8 + 1000];
		struct MapCut mc;
		struct MapRange mr;
		char tprl[PATH_LEN], take[PATH_LEN], takeprl[PATH_LEN], asprl[PATH_LEN], aswav[PATH_LEN];
		unsigned long n, i;
		unsigned char *got;
		FILE *f;

		memcpy(map, "PRL1", 4);
		put32(map + 4, 0);
		map[4] = 0; map[5] = 0; map[6] = 0x9C; map[7] = 0x40;		/* 40000 us, big-endian */
		for (i = 0; i < 1000; i++) map[8 + i] = (unsigned char) ((i * 7) % 90);
		memset(&mc, 0, sizeof(mc));		/* mc_Tl NULL: no cut in the take */
		mc.mc_Rate = 44100;
		mc.mc_In = 158760;
		mc.mc_Out = 1582308;
		MapCut_Range(&mc, 40000, 1000, &mr);
		CHECK(mr.mr_First == 90 && mr.mr_End == 897, "map: In's look and the last kept frame's");
		mc.mc_In = 44100UL * 7200;
		mc.mc_Out = 44100UL * 7300;
		MapCut_Range(&mc, 40000, 200000, &mr);
		CHECK(mr.mr_First == 180000 && mr.mr_End == 182500, "map: two hours in, no product that wraps");
		mc.mc_In = 0;
		mc.mc_Out = 0;
		MapCut_Range(&mc, 40000, 1000, &mr);
		CHECK(mr.mr_First == 0 && mr.mr_End == 0, "map: nothing kept, no looks");

		DosHost_Reset();
		path_of(trim, "Trim.wav");
		path_of(tprl, "Trim.prl");
		f = fopen(trim, "wb"); fputs("x", f); fclose(f);
		f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
		mc.mc_From = mc.mc_To = (CONST_STRPTR) trim;
		mc.mc_Rate = 44100;
		mc.mc_In = 158760;
		mc.mc_Out = 1582308;
		CHECK(MapCut_Cut(&mc) == MAPCUT_CUT, "map in place: cut");
		got = load(tprl, &n);
		CHECK(got && n == 8 + 807 && memcmp(got, map, 8) == 0 && memcmp(got + 8, map + 8 + 90, 807) == 0,
		      "map in place: the header, the looks 90 .. 896, byte for byte");
		free(got);
		path_of(temp, "Trim.prl.ped");
		CHECK(!exists(temp) && held_nothing(), "map in place: no temporary file, nothing held");

		/* Save As: a take of another format (Take.mp3, its map Take.prl)
		   trimmed into Take-trim.wav: the map beside it, the original's kept */
		path_of(take, "Take.mp3");
		path_of(takeprl, "Take.prl");
		path_of(aswav, "Take-trim.wav");
		path_of(asprl, "Take-trim.prl");
		remove(asprl);
		f = fopen(takeprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
		mc.mc_From = (CONST_STRPTR) take;
		mc.mc_To = (CONST_STRPTR) aswav;
		mc.mc_In = 0;
		mc.mc_Out = 44100UL * 10;
		CHECK(MapCut_Cut(&mc) == MAPCUT_CUT, "map as: cut");
		got = load(asprl, &n);
		CHECK(got && n == 8 + 250 && memcmp(got + 8, map + 8, 250) == 0, "map as: the first 10 s beside the new file");
		free(got);
		CHECK(file_is(takeprl, map, sizeof(map)), "map as: the original's map as it was");

		/* none beside the take: the new file's old map (the one above)
		   describes a sound that is not there -- it goes; then none at
		   all, and one that is no map: left alone */
		remove(takeprl);
		CHECK(MapCut_Cut(&mc) == MAPCUT_REMOVED && !exists(asprl), "map: none beside the take, the stale one beside the new file gone");
		CHECK(MapCut_Cut(&mc) == MAPCUT_NONE, "map: none on either side");
		f = fopen(takeprl, "wb"); fputs("not a map at all", f); fclose(f);
		CHECK(MapCut_Cut(&mc) == MAPCUT_NONE && file_is(takeprl, (const unsigned char *) "not a map at all", 16),
		      "map: a file that is no PRL1 map left alone");

		/* nothing left: the stale map goes */
		f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
		mc.mc_From = mc.mc_To = (CONST_STRPTR) trim;
		mc.mc_In = 44100UL * 50;
		mc.mc_Out = 44100UL * 60;
		CHECK(MapCut_Cut(&mc) == MAPCUT_REMOVED && !exists(tprl), "map: past its end, nothing left: removed");

		/* the disk full: no half map, the stale one removed */
		f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
		mc.mc_In = 158760;
		mc.mc_Out = 1582308;
		dos_host.dh_WriteLeft = 100;
		CHECK(MapCut_Cut(&mc) == MAPCUT_REMOVED && !exists(tprl) && !exists(temp), "map: the disk full: removed, no half map");
		dos_host.dh_WriteLeft = -1;

		/* Keep a backup (mc_Backup, as Doc_Save's): the map replaced kept
		   as Trim.prl.bak, an older backup replaced; a map removed (nothing
		   left) kept as the backup too */
		{
			char bak[PATH_LEN];
			unsigned char *was;
			unsigned long was_n;

			path_of(bak, "Trim.prl.bak");
			remove(bak);
			f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
			mc.mc_Backup = TRUE;
			CHECK(MapCut_Cut(&mc) == MAPCUT_CUT && file_is(bak, map, sizeof(map)), "map backup: the old map kept as .bak");
			got = load(tprl, &n);
			CHECK(got && n == 8 + 807 && memcmp(got + 8, map + 8 + 90, 807) == 0, "map backup: the cut map in its place");
			free(got);
			was = load(tprl, &was_n);
			CHECK(MapCut_Cut(&mc) == MAPCUT_CUT && was && file_is(bak, was, was_n), "map backup again: the older backup replaced");
			free(was);
			/* a take with a cut in it: the map along its timeline, the old
			   one kept the same way */
			{
				static struct Timeline mtl;

				f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
				remove(bak);
				Tl_Init(&mtl, 44100UL * 40);
				CHECK(Tl_Cut(&mtl, 44100UL * 10, 44100UL * 20), "map backup, cut: the timeline cut");
				mc.mc_Tl = &mtl;
				mc.mc_In = 0;
				mc.mc_Out = 44100UL * 30;
				CHECK(MapCut_Cut(&mc) == MAPCUT_CUT && file_is(bak, map, sizeof(map)), "map backup, cut: the old map kept as .bak");
				mc.mc_Tl = NULL;
				mc.mc_In = 158760;
				mc.mc_Out = 1582308;
			}
			f = fopen(tprl, "wb"); fwrite(map, 1, sizeof(map), f); fclose(f);
			mc.mc_In = 44100UL * 50;
			mc.mc_Out = 44100UL * 60;
			CHECK(MapCut_Cut(&mc) == MAPCUT_REMOVED && !exists(tprl) && file_is(bak, map, sizeof(map)),
			      "map backup: nothing left, the map moved into the backup");
			CHECK(!exists(temp), "map backup: no temporary file");
			mc.mc_Backup = FALSE;
			remove(bak);
		}
		CHECK(held_nothing(), "map: nothing held");
		remove(take); remove(takeprl); remove(asprl); remove(aswav);
	}

	/* 13: the timeline (timeline.c through doc.c): a cut joined and a
	   silence -- the picture, the samples, Save As and the map along it */
	{
		static unsigned char map[8 + 60];
		static BYTE out[2 * 2 * 80];
		struct Fixture fx;
		struct Layout ly;
		struct WavePeakMsg q;
		struct DocSave ds;
		struct MapCut mc;
		ULONG (*hook)(struct Hook *, APTR, struct WavePeakMsg *);	/* the hook as the class calls it */
		char cut[PATH_LEN], cutwav[PATH_LEN], cutprl[PATH_LEN], srcprl[PATH_LEN];
		unsigned char *got, *e;
		unsigned long n, i, t, f, col, len, cols, align = 4, v;
		int ok, contains;
		unsigned c;
		BYTE mn, mx, top;
		FILE *fp;

		/* the model: timeline frame t's file frame, or -1 (silence) */
#define TL_CUT_A	8192UL
#define TL_CUT_B	16384UL
#define TL_SIL_A	24576UL
#define TL_SIL_B	32768UL
#define MODEL(t)	((t) >= TL_SIL_A && (t) < TL_SIL_B ? -1L : (long) ((t) < TL_CUT_A ? (t) : (t) + (TL_CUT_B - TL_CUT_A)))
		DosHost_Reset();
		path_of(cut, "Cut.wav");
		fx.fx_Channels = 2;
		fx.fx_Frames = 65536;
		fx.fx_Before = 0;
		fx.fx_After = 1;
		CHECK(make_wav(cut, &fx, &ly) && open_all(cut), "timeline: the fixture read");
		CHECK(Tl_Whole(&doc.d_Tl, doc.d_Frames) && Doc_Length(&doc) == 65536, "timeline: a fresh doc is the whole file");
		CHECK(Tl_Cut(&doc.d_Tl, TL_CUT_A, TL_CUT_B) && Tl_Silence(&doc.d_Tl, TL_SIL_A, TL_SIL_B), "timeline: cut and silence");
		Doc_Changed(&doc);
		len = 65536 - (TL_CUT_B - TL_CUT_A);
		CHECK(Doc_Length(&doc) == len && doc.d_Source.ws_Frames == len, "timeline: the length and the source's, 8192 frames fewer");

		/* the picture on the picture's own grid: each column exact */
		hook = (ULONG (*)(struct Hook *, APTR, struct WavePeakMsg *)) (void *) doc.d_Source.ws_Peaks.h_Entry;
		cols = len / 1024;
		q.wpm_First = 0;
		q.wpm_Step = 1024;
		q.wpm_Count = cols < 80 ? cols : 80;
		q.wpm_Out = out;
		ok = hook(&doc.d_Source.ws_Peaks, NULL, &q) == q.wpm_Count;
		for (col = 0; ok && col < q.wpm_Count; col++) {
			for (c = 0; ok && c < 2; c++) {
				mn = 127;
				mx = -128;
				for (t = col * 1024; t < (col + 1) * 1024; t++) {
					top = MODEL(t) < 0 ? 0 : doc_top(sample((unsigned long) MODEL(t), c));
					if (top < mn) mn = top;
					if (top > mx) mx = top;
				}
				ok = out[(col * 2 + c) * 2] == mn && out[(col * 2 + c) * 2 + 1] == mx;
				if (!ok) printf("  column %lu channel %u: %d %d, want %d %d\n", col, c, out[(col * 2 + c) * 2],
						out[(col * 2 + c) * 2 + 1], mn, mx);
			}
		}
		CHECK(ok, "timeline: the picture along it, column by column exact on 1024 frames");
		/* columns across the joins (3000 frames): each holds its frames' extremes */
		q.wpm_Step = 3000;
		q.wpm_Count = len / 3000;
		ok = hook(&doc.d_Source.ws_Peaks, NULL, &q) == q.wpm_Count;
		contains = 1;
		for (col = 0; ok && col < q.wpm_Count; col++) {
			for (c = 0; c < 2; c++) {
				mn = 127;
				mx = -128;
				for (t = col * 3000; t < (col + 1) * 3000; t++) {
					top = MODEL(t) < 0 ? 0 : doc_top(sample((unsigned long) MODEL(t), c));
					if (top < mn) mn = top;
					if (top > mx) mx = top;
				}
				if (out[(col * 2 + c) * 2] > mn || out[(col * 2 + c) * 2 + 1] < mx) contains = 0;
			}
		}
		CHECK(ok && contains, "timeline: columns across joins hold their frames' extremes");
		/* past the end: 0, 0 */
		q.wpm_First = len - 1024;
		q.wpm_Step = 1024;
		q.wpm_Count = 3;
		CHECK(hook(&doc.d_Source.ws_Peaks, NULL, &q) == 3 && out[4] == 0 && out[5] == 0 && out[8] == 0 && out[11] == 0,
		      "timeline: columns past its end 0, 0");

		/* the samples across the join and into the silence */
		fws = Doc_Fetch(&doc, TL_CUT_A - 32, 64);
		ok = fws && fws->wsm_Frames == 64;
		for (i = 0; ok && i < 64; i++) {
			const unsigned char *pp = (const unsigned char *) fws->wsm_Data + i * 4;

			f = (unsigned long) MODEL(TL_CUT_A - 32 + i);
			for (c = 0; ok && c < 2; c++) {
				v = sample(f, c);
				ok = pp[c * 2] == ((v >> 8) & 255) && pp[c * 2 + 1] == (v & 255);
			}
		}
		CHECK(ok, "timeline: fetched across the join, the file's frames each side");
		fws = Doc_Fetch(&doc, TL_SIL_A - 16, 64);
		ok = fws && fws->wsm_Frames == 64;
		for (i = 0; ok && i < 64; i++) {
			const unsigned char *pp = (const unsigned char *) fws->wsm_Data + i * 4;

			if (i < 16) ok = pp[0] == ((sample((unsigned long) MODEL(TL_SIL_A - 16 + i), 0) >> 8) & 255);
			else ok = pp[0] == 0 && pp[1] == 0 && pp[2] == 0 && pp[3] == 0;
		}
		CHECK(ok, "timeline: fetched into the silence, zeros");

		/* Save As: the timeline written, its header's sizes its own */
		path_of(cutwav, "Cut-saved.wav");
		ds.ds_In = 0;
		ds.ds_Out = len;
		ds.ds_Path = (CONST_STRPTR) cutwav;
		CHECK(Doc_SaveAs(&doc, &ds) == 0, "timeline: saved as");
		got = load(cutwav, &n);
		ok = got && n == ly.ly_Start + len * align + ly.ly_Trailer;
		if (ok) {
			e = malloc(len * align);
			for (t = 0; e && t < len; t++) {
				for (c = 0; c < 2; c++) {
					v = MODEL(t) < 0 ? 0 : sample((unsigned long) MODEL(t), c);
					e[t * 4 + c * 2] = (unsigned char) (v & 255);
					e[t * 4 + c * 2 + 1] = (unsigned char) (v >> 8);
				}
			}
			ok = e && memcmp(got + ly.ly_Start, e, len * align) == 0 &&
			     got[ly.ly_Start - 4] == ((len * align) & 255) && got[ly.ly_Start - 3] == (((len * align) >> 8) & 255);
			free(e);
		}
		CHECK(ok, "timeline: the saved samples are the timeline's, silence zeros, the sizes true");
		free(got);

		/* the map along the timeline: a look the old look its first frame
		   came from (1764 frames a look at 44.1 kHz), silence 255 */
		path_of(srcprl, "Cut.prl");
		path_of(cutprl, "Cut-saved.prl");
		memcpy(map, "PRL1", 4);
		map[4] = 0; map[5] = 0; map[6] = 0x9C; map[7] = 0x40;
		for (i = 0; i < 60; i++) map[8 + i] = (unsigned char) (10 + i);
		fp = fopen(srcprl, "wb"); fwrite(map, 1, sizeof(map), fp); fclose(fp);
		memset(&mc, 0, sizeof(mc));
		mc.mc_From = (CONST_STRPTR) cut;
		mc.mc_To = (CONST_STRPTR) cutwav;
		mc.mc_In = 0;
		mc.mc_Out = len;
		mc.mc_Rate = 44100;
		mc.mc_Tl = &doc.d_Tl;
		CHECK(MapCut_Cut(&mc) == MAPCUT_CUT, "timeline: the map cut along it");
		got = load(cutprl, &n);
		/* 57344 frames: 1300.3 ms, looks 0 .. 32 */
		ok = got && n == 8 + 33 && memcmp(got, map, 8) == 0;
		/* look 2 (frame 3528): the file's look 2; look 10 (frame 17640,
		   from 25832 = 585 ms): look 14; look 15 (frame 26460): silence;
		   look 20 (frame 35280, from 43472 = 985 ms): look 24 */
		if (ok) ok = got[8 + 2] == map[8 + 2] && got[8 + 10] == map[8 + 14] && got[8 + 15] == 255 && got[8 + 20] == map[8 + 24];
		if (got && !ok) printf("  map: %lu bytes, looks 2 %u, 10 %u, 15 %u, 20 %u\n", n, got[10], got[18], got[23], got[28]);
		CHECK(ok, "timeline: the map's looks from where their frames came, silence none");
		free(got);
		Doc_Close(&doc);
		CHECK(held_nothing(), "timeline: nothing held");

		/* a column across a silence's edge: the silence's 0 in it -- the
		   samples constant, channel 0's top byte +64, channel 1's -64 (as
		   doc.c reckons on this host: the words swapped) */
		CHECK(make_wav(cut, &fx, &ly), "timeline: the constant fixture");
		fp = fopen(cut, "r+b");
		fseek(fp, (long) ly.ly_Start, SEEK_SET);
		for (i = 0; i < fx.fx_Frames; i++) fwrite("\x40\x00\xC0\x00", 1, 4, fp);
		fclose(fp);
		CHECK(open_all(cut) && Tl_Silence(&doc.d_Tl, 8192, 16384), "timeline: read, a silence put in");
		Doc_Changed(&doc);
		q.wpm_First = 8192 - 2048;
		q.wpm_Step = 4096;
		q.wpm_Count = 3;
		q.wpm_Out = out;
		CHECK(hook(&doc.d_Source.ws_Peaks, NULL, &q) == 3, "timeline: three columns over the silence's edges");
		CHECK(out[0] == 0 && out[1] == 64 && out[2] == -64 && out[3] == 0, "timeline: into the silence, each channel to 0");
		CHECK(out[4] == 0 && out[5] == 0 && out[6] == 0 && out[7] == 0, "timeline: inside the silence, 0, 0");
		CHECK(out[8] == 0 && out[9] == 64 && out[10] == -64 && out[11] == 0, "timeline: out of the silence, each channel from 0");
		Doc_Close(&doc);
		remove(cut); remove(cutwav); remove(cutprl); remove(srcprl);
#undef MODEL
	}

	/* 14: Keep a backup (Doc_SetBackup): the file replaced kept as
	   Trim.wav.bak with its protection, comment and date; saved again,
	   the older backup replaced; an old backup that cannot be deleted, or
	   a rename to it that fails: the save refused, the original in its
	   place and the doc open on it */
	{
		char bak[PATH_LEN];
		unsigned char *was;
		unsigned long was_n;

		path_of(bak, "Trim.wav.bak");
		remove(bak);
		Doc_SetBackup(TRUE);
		CHECK(fresh(&stereo), "backup: opened");
		err = Doc_Save(&doc, 12345, 67890);
		CHECK(err == DOC_KEPT && saved_as(trim, orig, &ly, 12345, 67890), "backup: saved, byte for byte");
		CHECK(file_is(bak, orig, orig_n) && meta_kept(bak), "backup: the original kept as .bak, protection, comment, date");
		Doc_Close(&doc);
		CHECK(meta_kept(trim) && !exists(temp) && held_nothing(), "backup: the new file's metadata, no temporary file, nothing held");
		was = load(trim, &was_n);
		CHECK(was && open_all(trim), "backup again: opened");
		err = Doc_Save(&doc, 100, 50000);
		CHECK(err == DOC_KEPT && file_is(bak, was, was_n), "backup again: the older backup replaced by the file saved over");
		free(was);
		Doc_Close(&doc);
		CHECK(held_nothing(), "backup again: nothing held");

		CHECK(fresh(&stereo), "backup stuck: opened");
		dos_host.dh_FailDelete = 1;
		err = Doc_Save(&doc, 12345, 67890);
		CHECK(err == ERROR_DELETE_PROTECTED, "backup stuck: the old backup's error");
		CHECK(file_is(trim, orig, orig_n) && meta_kept(trim) && !exists(temp), "backup stuck: the original in its place, the new one gone");
		CHECK(intact(), "backup stuck: the doc open again, still read, fetching");
		Doc_Close(&doc);

		remove(bak);
		CHECK(fresh(&stereo), "backup rename: opened");
		dos_host.dh_FailRename = 1;
		err = Doc_Save(&doc, 12345, 67890);
		CHECK(err == ERROR_DISK_WRITE_PROTECTED, "backup rename: its error");
		CHECK(file_is(trim, orig, orig_n) && !exists(bak) && !exists(temp), "backup rename: the original in its place, no backup");
		CHECK(intact(), "backup rename: the doc open again");
		Doc_Close(&doc);
		Doc_SetBackup(FALSE);
		remove(bak);
		CHECK(held_nothing(), "backup: nothing held at the end");
	}

	/* a level (concept 3.2): +6 dB on frames 1000 .. 2999, saved as: the
	   rest the file's bytes, the part every sample at its level -- the
	   fixture's samples fill the 16 bits, so many hit full scale: held
	   there and counted */
	{
		struct TlPart pt;
		char lvwav[PATH_LEN];
		unsigned char *got, *e;
		unsigned long n, t, align = 4, len, clip = 0, i;
		UWORD g6 = Env_FromTenths(60);
		long s, w;
		int ok;
		unsigned c;

		CHECK(fresh(&stereo), "level: opened");
		len = doc.d_Tl.tl_Frames;
		/* a part's peak, as recorded: the looks wholly inside it from the
		   peaks the reading kept, the looks at its edges read again --
		   against every sample of it. The reading turns each word from
		   the file's order to the Amiga's (doc.c's LEWORD), so on this
		   little-endian host the kept peaks are those of the words turned
		   (doc_top's comment): the whole looks reckoned so, the edges as
		   the file has them; on the Amiga both are the file's */
		{
			unsigned long k, a2, b2, f2, want2, lf = doc.d_LL.ll_Frames, k0, k1, m2;
			long s2;
			int bad = 0;

			srand(41);
			for (k = 0; k < 200; k++) {
				a2 = (unsigned long) rand() % len;
				b2 = a2 + 1 + (unsigned long) rand() % (k < 100 ? 50000 : 3000);
				if (b2 > len) b2 = len;
				k0 = (a2 + lf - 1) / lf;
				k1 = b2 / lf;
				want2 = 0;
				for (f2 = a2; f2 < b2; f2++) {
					int whole = k0 < k1 && f2 >= k0 * lf && f2 < k1 * lf;

					for (c = 0; c < 2; c++) {
						unsigned short u = (unsigned short) sample(f2, c);

						s2 = whole ? (long) (short) (unsigned short) ((u << 8) | (u >> 8)) : (long) (short) u;
						m2 = (unsigned long) (s2 < 0 ? -s2 : s2);
						if (whole && m2 > 32767) m2 = 32767;	/* the looks keep -32768 as 32767 */
						if (m2 > want2) want2 = m2;
					}
				}
				if (Doc_PartPeak(&doc, a2, b2) != want2) {
					if (!bad) printf("  part %lu..%lu: %lu, want %lu (looks %lu of %lu frames, %lu kept)\n", a2, b2,
							 (unsigned long) Doc_PartPeak(&doc, a2, b2), want2, (unsigned long) doc.d_Pic.dp_PeakCount,
							 (unsigned long) doc.d_LL.ll_Frames, (unsigned long) doc.d_Pic.dp_LookMax);
					bad++;
				}
			}
			CHECK(!bad, "level: 200 parts' peaks as recorded, each its samples' loudest");
		}
		pt.tp_First = 1000;
		pt.tp_End = 3000;
		pt.tp_Gain = g6;
		pt.tp_Fade = TL_FADE_NONE;
		pt.tp_Pad = 0;
		CHECK(Tl_Gain(&doc.d_Tl, &pt), "level: +6 dB on 1000 .. 2999");
		Doc_Changed(&doc);
		/* a part across the gain: the gained samples' loudest (held at
		   full scale), the rest as recorded */
		{
			unsigned long f2, want2 = 0, a2 = 500, b2 = 4000;
			long s2, w2;

			for (f2 = a2; f2 < b2; f2++) {
				for (c = 0; c < 2; c++) {
					s2 = (long) (short) (unsigned short) sample(f2, c);
					w2 = s2;
					if (f2 >= 1000 && f2 < 3000) {
						w2 = (s2 * (long) g6 + 2048L) >> 12;
						if (w2 > 32767L) w2 = 32767L;
						if (w2 < -32768L) w2 = -32768L;
					}
					if ((unsigned long) (w2 < 0 ? -w2 : w2) > want2) want2 = (unsigned long) (w2 < 0 ? -w2 : w2);
				}
			}
			CHECK(Doc_PartPeak(&doc, a2, b2) == want2, "level: a part's peak across the gain, at its level");
		}
		/* the picture at its level: column 0 (frames 0 .. 1023) and 3 as
		   recorded, column 1 (1024 .. 2047, all of it gained) its
		   extremes times +6 dB, held at the picture's full scale */
		{
			static BYTE cols[2 * 2 * 4];
			struct WavePeakMsg q;
			ULONG (*hook)(struct Hook *, APTR, struct WavePeakMsg *) =
				(ULONG (*)(struct Hook *, APTR, struct WavePeakMsg *)) (void *) doc.d_Source.ws_Peaks.h_Entry;
			unsigned long col, f;
			BYTE mn, mx, top;
			long want[2];

			q.wpm_First = 0;
			q.wpm_Step = 1024;
			q.wpm_Count = 4;
			q.wpm_Out = cols;
			ok = hook(&doc.d_Source.ws_Peaks, NULL, &q) == 4;
			for (col = 0; ok && col < 4; col++) {
				if (col == 2) continue;		/* half gained: its highest level, checked by eye */
				for (c = 0; ok && c < 2; c++) {
					mn = 127;
					mx = -128;
					for (f = col * 1024; f < (col + 1) * 1024; f++) {
						top = doc_top(sample(f, c));
						if (top < mn) mn = top;
						if (top > mx) mx = top;
					}
					want[0] = mn;
					want[1] = mx;
					if (col == 1) {
						for (i = 0; i < 2; i++) {
							want[i] = (want[i] * (long) g6 + 2048L) >> 12;
							if (want[i] > 127) want[i] = 127;
							if (want[i] < -128) want[i] = -128;
						}
					}
					ok = cols[(col * 2 + c) * 2] == want[0] && cols[(col * 2 + c) * 2 + 1] == want[1];
					if (!ok) printf("  level picture column %lu channel %u: %d %d, want %ld %ld\n", col, c,
							cols[(col * 2 + c) * 2], cols[(col * 2 + c) * 2 + 1], want[0], want[1]);
				}
			}
			CHECK(ok, "level: the picture as recorded around the part, its extremes times +6 dB in it");
		}
		path_of(lvwav, "Level-saved.wav");
		ds.ds_In = 0;
		ds.ds_Out = len;
		ds.ds_Path = (CONST_STRPTR) lvwav;
		CHECK(Doc_SaveAs(&doc, &ds) == 0, "level: saved as");
		got = load(lvwav, &n);
		ok = got && n == orig_n;
		e = malloc(len * align);
		for (t = 0; ok && e && t < len; t++) {
			for (c = 0; c < 2; c++) {
				s = (long) (short) (unsigned short) sample(t, c);
				w = s;
				if (t >= 1000 && t < 3000) {
					w = (long) ((s * (long) g6 + 2048L) >> 12);
					if (w > 32767L) { w = 32767L; clip++; }
					if (w < -32768L) { w = -32768L; clip++; }
				}
				e[t * 4 + c * 2] = (unsigned char) (w & 255);
				e[t * 4 + c * 2 + 1] = (unsigned char) ((w >> 8) & 255);
			}
		}
		ok = ok && e && memcmp(got + ly.ly_Start, e, len * align) == 0;
		for (i = 0; ok && i < 1000UL * align; i++) ok = got[ly.ly_Start + i] == orig[ly.ly_Start + i];
		CHECK(ok, "level: as recorded outside the part, every sample of it times +6 dB");
		CHECK(clip > 100 && doc.d_Clipped == clip, "level: the samples held at full scale counted");
		CHECK(got && !memcmp(got, orig, ly.ly_Start) && !memcmp(got + ly.ly_Start + len * align, orig + ly.ly_Start + len * align,
								      ly.ly_Trailer), "level: the header and what follows the samples as they were");
		free(e);
		free(got);
		Doc_Close(&doc);
		remove(lvwav);
		CHECK(held_nothing(), "level: nothing held");
	}

	remove(trim);
	remove(other);
	free(orig);
	return CHECK_DONE("test_docsave");
}
