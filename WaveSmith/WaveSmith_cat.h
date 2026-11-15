#ifndef WAVESMITH_CAT_H
#define WAVESMITH_CAT_H

/*
 * WaveSmith_cat.h -- CatComp message ids + GetString() accessor.
 *
 * GENERATED. Host: tools/gen_catalog.py gen. Amiga (native, no host
 * tools): catcomp catalogs/WaveSmith.cd CFILE WaveSmith_cat.h.
 * The two are interface-compatible -- identical WS_NAME ids (the .cd
 * carries no explicit ids, so both number 0..N in file order) and the
 * same struct LocaleInfo / GetString() contract. Do not hand-edit.
 *
 * Include with CATCOMP_CODE defined in exactly ONE .c (the locale
 * module) to instantiate GetString() and the default table; include
 * with CATCOMP_NUMBERS (or neither) elsewhere for ids and prototype.
 */

#include <exec/types.h>

struct LocaleInfo
{
    APTR li_LocaleBase;
    APTR li_Catalog;
};

STRPTR GetString(struct LocaleInfo *li, LONG stringNum);

/* Numbers are needed by the redirect header AND by the code section. */
#if defined(CATCOMP_NUMBERS) || defined(CATCOMP_CODE)
#define WS_SIG_CATALOG 0
#define WS_TXT_TITLE 1
#define WS_TXT_TITLE_FILE 2
#define WS_TXT_MONO 3
#define WS_TXT_STEREO 4
#define WS_TXT_IN 5
#define WS_TXT_OUT 6
#define WS_TXT_LISTEN 7
#define WS_TXT_LENGTH 8
#define WS_TXT_CANCEL 9
#define WS_TXT_ZOOMIN 10
#define WS_TXT_ZOOMOUT 11
#define WS_TXT_ALL 12
#define WS_TXT_NEXT 13
#define WS_TXT_LOOP 14
#define WS_TXT_HINT_IN 15
#define WS_TXT_HINT_OUT 16
#define WS_TXT_HINT_BACK 17
#define WS_TXT_HINT_ON 18
#define WS_TXT_HINT_INLISTEN 19
#define WS_TXT_HINT_OUTLISTEN 20
#define WS_TXT_HINT_CANCEL 21
#define WS_TXT_HINT_ZOOMIN 22
#define WS_TXT_HINT_ZOOMOUT 23
#define WS_TXT_HINT_ALL 24
#define WS_TXT_HINT_ATIN 25
#define WS_TXT_HINT_ATOUT 26
#define WS_TXT_HINT_LOOP 27
#define WS_TXT_ST_NOFILE 28
#define WS_TXT_ST_NOFILE_WAV 29
#define WS_TXT_ST_READING 30
#define WS_TXT_ST_FOUND 31
#define WS_TXT_ST_PART 32
#define WS_TXT_ST_PART_PERIOD 33
#define WS_TXT_CLICKS_NONE 34
#define WS_TXT_CLICKS_ONE 35
#define WS_TXT_CLICKS_MANY 36
#define WS_TXT_ST_NOMUSIC 37
#define WS_TXT_ST_SNAPPED_IN 38
#define WS_TXT_ST_SNAPPED_OUT 39
#define WS_TXT_ST_PUT_IN 40
#define WS_TXT_ST_PUT_OUT 41
#define WS_TXT_ST_PREFS_SAVED 42
#define WS_TXT_ST_PREFS_PROGDIR 43
#define WS_TXT_ST_MARGINS 44
#define WS_TXT_ST_MARKED 45
#define WS_TXT_ST_MARK_WHERE 46
#define WS_TXT_ST_MARKS_FULL 47
#define WS_TXT_ST_MARK_NOSEL 48
#define WS_TXT_ST_MARK_PICKED 49
#define WS_TXT_ST_UNMARKED 50
#define WS_TXT_ST_MEASURING 51
#define WS_TXT_ST_FIND_CANCELLED 52
#define WS_TXT_ST_NOTRACKS 53
#define WS_TXT_ST_TRACKS 54
#define WS_TXT_ST_SPLITMARKS_WHERE 55
#define WS_TXT_ST_SPLITMARKS 56
#define WS_TXT_ST_MARGINS_KEPT 57
#define WS_TXT_MG_TITLE 58
#define WS_TXT_MG_LEAD 59
#define WS_TXT_MG_TAIL 60
#define WS_TXT_MG_MS 61
#define WS_TXT_MG_USE 62
#define WS_TXT_MG_CANCEL 63
#define WS_TXT_ST_SAVING 64
#define WS_TXT_ST_SAVED 65
#define WS_TXT_ST_NONEXT 66
#define WS_TXT_ST_NOHELP 67
#define WS_TXT_ST_BADTIME 68
#define WS_TXT_ST_PLAY_BUSY 69
#define WS_TXT_ST_PLAY_NOCARD 70
#define WS_TXT_ST_PLAY_NOLIB 71
#define WS_TXT_ST_PLAY_NOAHI 72
#define WS_TXT_ST_PLAY_AHIMODE 73
#define WS_TXT_ST_PLAY_NOMEM 74
#define WS_TXT_ST_PLAY_AHIBUSY 75
#define WS_TXT_ST_PLAY_AHIJOB 76
#define WS_TXT_ST_AHIMODE 77
#define WS_TXT_AHI_TITLE 78
#define WS_TXT_AHI_DEFAULT 79
#define WS_TXT_ST_PLAY_ERR 80
#define WS_TXT_ST_PLAYING 81
#define WS_TXT_ST_PLAYED 82
#define WS_TXT_ST_STOPPED 83
#define WS_TXT_ST_ATEND 84
#define WS_TXT_LEFT_PRE 85
#define WS_TXT_LEFT_POST 86
#define WS_TXT_ST_READ_CANCELLED 87
#define WS_TXT_ST_DECODE_CANCELLED 88
#define WS_TXT_ST_SAVE_CANCELLED 89
#define WS_TXT_ST_NOSEL 90
#define WS_TXT_ST_PAUSED 91
#define WS_TXT_ST_DECODING 92
#define WS_TXT_ST_DECODE_FAILED 93
#define WS_TXT_ST_SAVED_AS 94
#define WS_TXT_ST_UNDONE 95
#define WS_TXT_ST_REDONE 96
#define WS_TXT_ST_NOUNDO 97
#define WS_TXT_ST_CUT_JOINED 98
#define WS_TXT_ST_CUT_SILENCE 99
#define WS_TXT_ST_CUTFULL 100
#define WS_TXT_ST_CUTREADING 101
#define WS_TXT_ST_SPLIT_WHERE 102
#define WS_TXT_ST_SPLIT 103
#define WS_TXT_ST_SPLIT_CANCELLED 104
#define WS_TXT_ASK_SPLIT_OVER 105
#define WS_TXT_ASK_SPLIT_OVER_GADS 106
#define WS_TXT_ASK_CUT 107
#define WS_TXT_ASK_CUT_GADS 108
#define WS_TXT_E_NOTWAV 109
#define WS_TXT_E_NOTPCM16 110
#define WS_TXT_ASK_UNSAVED 111
#define WS_TXT_ASK_UNSAVED_GADS 112
#define WS_TXT_ASK_REVERT 113
#define WS_TXT_ASK_REVERT_GADS 114
#define WS_TXT_ASK_SPLITMARKS_OVER 115
#define WS_TXT_ASK_REPLACE 116
#define WS_TXT_ASK_REPLACE_GADS 117
#define WS_TXT_E_NEEDDSP 118
#define WS_TXT_E_NOMEM 119
#define WS_TXT_E_EMPTY 120
#define WS_TXT_E_DOS 121
#define WS_TXT_E_SAVE 122
#define WS_TXT_DECIMAL 123
#define WS_TXT_THOUSANDS 124
#define WS_TXT_E_SAVE_EMPTY 125
#define WS_TXT_E_KEPT 126
#define WS_TXT_E_KEPT_BAK 127
#define WS_TXT_E_PREFS 128
#define WS_TXT_E_CUE 129
#define WS_TXT_E_MARGINS 130
#define WS_TXT_M_PROJECT 131
#define WS_TXT_M_OPEN 132
#define WS_TXT_M_NEXT 133
#define WS_TXT_M_SAVE 134
#define WS_TXT_M_SAVEAS 135
#define WS_TXT_M_REVERT 136
#define WS_TXT_M_SPLIT 137
#define WS_TXT_M_SPLITMARKS 138
#define WS_TXT_M_ADDMARK 139
#define WS_TXT_M_DELMARK 140
#define WS_TXT_M_FINDTRACKS 141
#define WS_TXT_M_EFFECTS 142
#define WS_TXT_M_FADEIN 143
#define WS_TXT_M_FADEOUT 144
#define WS_TXT_SHAPE_LINEAR 145
#define WS_TXT_SHAPE_EVEN 146
#define WS_TXT_SHAPE_SCURVE 147
#define WS_TXT_ST_FADED_IN 148
#define WS_TXT_ST_FADED_OUT 149
#define WS_TXT_LENGTH_FADEIN 150
#define WS_TXT_LENGTH_FADEOUT 151
#define WS_TXT_LENGTH_FADES 152
#define WS_TXT_ST_ENDFADE_IN 153
#define WS_TXT_ST_ENDFADE_OUT 154
#define WS_TXT_M_GAIN 155
#define WS_TXT_M_NORMALISE 156
#define WS_TXT_M_PLAIN 157
#define WS_TXT_GN_TITLE 158
#define WS_TXT_GN_GAIN 159
#define WS_TXT_GN_DB 160
#define WS_TXT_GN_PEAK 161
#define WS_TXT_GN_SILENT 162
#define WS_TXT_GN_PEAK_GONE 163
#define WS_TXT_GN_CLIPS 164
#define WS_TXT_GN_NORMALISE 165
#define WS_TXT_GN_TO 166
#define WS_TXT_ST_GAINED 167
#define WS_TXT_ST_GAINED_CLIPS 168
#define WS_TXT_ST_GAIN_KEPT 169
#define WS_TXT_ST_NORMALISED 170
#define WS_TXT_ST_NORM_THERE 171
#define WS_TXT_ST_NORM_SILENT 172
#define WS_TXT_ST_PLAINED 173
#define WS_TXT_ST_NOTHING_PLAIN 174
#define WS_TXT_E_GAINWIN 175
#define WS_TXT_K_RETURN 176
#define WS_TXT_K_DEL 177
#define WS_TXT_M_HELP 178
#define WS_TXT_K_HELP 179
#define WS_TXT_M_ABOUT 180
#define WS_TXT_M_QUIT 181
#define WS_TXT_M_EDIT 182
#define WS_TXT_M_UNDO 183
#define WS_TXT_M_REDO 184
#define WS_TXT_M_CUTOUT 185
#define WS_TXT_M_SELALL 186
#define WS_TXT_M_SELNONE 187
#define WS_TXT_M_VIEW 188
#define WS_TXT_M_ZOOMIN 189
#define WS_TXT_M_ZOOMOUT 190
#define WS_TXT_M_ALL 191
#define WS_TXT_M_ZOOMSEL 192
#define WS_TXT_M_ATIN 193
#define WS_TXT_M_ATOUT 194
#define WS_TXT_M_PREVMARK 195
#define WS_TXT_M_NEXTMARK 196
#define WS_TXT_K_F1 197
#define WS_TXT_K_F2 198
#define WS_TXT_ST_AT_MARK 199
#define WS_TXT_ST_AT_IN 200
#define WS_TXT_ST_AT_OUT 201
#define WS_TXT_ST_NO_MARK_BEFORE 202
#define WS_TXT_ST_NO_MARK_AFTER 203
#define WS_TXT_M_SCALE 204
#define WS_TXT_M_PLAYMENU 205
#define WS_TXT_M_PLAY 206
#define WS_TXT_M_PLAYSEL 207
#define WS_TXT_M_STOP 208
#define WS_TXT_M_LISTENIN 209
#define WS_TXT_M_LISTENOUT 210
#define WS_TXT_M_LOOP 211
#define WS_TXT_K_SPACE 212
#define WS_TXT_K_SHIFTSPACE 213
#define WS_TXT_M_SETTINGS 214
#define WS_TXT_M_PLAYWAY 215
#define WS_TXT_M_WAYAUTO 216
#define WS_TXT_M_WAYCARD 217
#define WS_TXT_M_WAYAHI 218
#define WS_TXT_M_AHIMODE 219
#define WS_TXT_M_SNAP 220
#define WS_TXT_M_MARGINS 221
#define WS_TXT_M_BACKUP 222
#define WS_TXT_M_ICONS 223
#define WS_TXT_M_SAVEPREFS 224
#define WS_TXT_ABOUT 225
#define WS_TXT_ABOUT_ART 226
#define WS_TXT_OK 227
#define WS_TXT_SAVEAS_TITLE 228
#define WS_TXT_OPEN_TITLE 229
#define WS_TXT_NOPLAYER 230
#define WS_TXT_M_TRIMWIN 231
#define WS_TXT_TRIM_TITLE 232
#define WS_TXT_SHOW 233
#define WS_TXT_HINT_FADELEN 234
#define WS_TXT_FADE_NONE 235
#define WS_TXT_HINT_DECK 236
#define WS_TXT_HINT_TRIMWIN 237
#define WS_TXT_HINT_FLAGIN 238
#define WS_TXT_HINT_FLAGOUT 239
#endif /* CATCOMP_NUMBERS || CATCOMP_CODE */

#ifdef CATCOMP_CODE

#include <libraries/locale.h>
#include <proto/locale.h>

struct WS_CatCompArrayType
{
    LONG   cca_ID;
    STRPTR cca_Str;
};

static const struct WS_CatCompArrayType WS_CatCompArray[] =
{
    { WS_SIG_CATALOG, (STRPTR)"WSCAT-builtin" },
    { WS_TXT_TITLE, (STRPTR)"WaveSmith" },
    { WS_TXT_TITLE_FILE, (STRPTR)"WaveSmith: %s  %s  %ld Hz %s" },
    { WS_TXT_MONO, (STRPTR)"mono" },
    { WS_TXT_STEREO, (STRPTR)"stereo" },
    { WS_TXT_IN, (STRPTR)"In" },
    { WS_TXT_OUT, (STRPTR)"Out" },
    { WS_TXT_LISTEN, (STRPTR)"Listen" },
    { WS_TXT_LENGTH, (STRPTR)"Length %s" },
    { WS_TXT_CANCEL, (STRPTR)"Cancel" },
    { WS_TXT_ZOOMIN, (STRPTR)"Zoom +" },
    { WS_TXT_ZOOMOUT, (STRPTR)"Zoom -" },
    { WS_TXT_ALL, (STRPTR)"All" },
    { WS_TXT_NEXT, (STRPTR)"Next" },
    { WS_TXT_LOOP, (STRPTR)"Loop" },
    { WS_TXT_HINT_IN, (STRPTR)"Where the kept part begins: type a time (m:ss.mmm)" },
    { WS_TXT_HINT_OUT, (STRPTR)"Where the kept part ends: type a time (m:ss.mmm)" },
    { WS_TXT_HINT_BACK, (STRPTR)"The handle a step back (the step follows the zoom)" },
    { WS_TXT_HINT_ON, (STRPTR)"The handle a step on (the step follows the zoom)" },
    { WS_TXT_HINT_INLISTEN, (STRPTR)"Listen from In, 3 s" },
    { WS_TXT_HINT_OUTLISTEN, (STRPTR)"Listen to the last 3 s before Out" },
    { WS_TXT_HINT_CANCEL, (STRPTR)"Call off the reading, decoding or saving (Esc too)" },
    { WS_TXT_HINT_ZOOMIN, (STRPTR)"Zoom in (Right-Amiga +, or the wheel)" },
    { WS_TXT_HINT_ZOOMOUT, (STRPTR)"Zoom out (Right-Amiga -)" },
    { WS_TXT_HINT_ALL, (STRPTR)"Show the whole file (Right-Amiga 0)" },
    { WS_TXT_HINT_ATIN, (STRPTR)"Show 3 s around In" },
    { WS_TXT_HINT_ATOUT, (STRPTR)"Show 3 s around Out" },
    { WS_TXT_HINT_LOOP, (STRPTR)"Again and again: the part marked, else In to Out (Play > Loop? too)" },
    { WS_TXT_ST_NOFILE, (STRPTR)"Open a sound file (Project/Open...): WAV, AIFF, FLAC, MP3, Ogg, AAC" },
    { WS_TXT_ST_NOFILE_WAV, (STRPTR)"Open a WAV file (Project/Open...); others need prismadsp.library" },
    { WS_TXT_ST_READING, (STRPTR)"Reading %s: %ld %%%s" },
    { WS_TXT_ST_FOUND, (STRPTR)"Found: lead-in %s; run-out %s" },
    { WS_TXT_ST_PART, (STRPTR)"%s s, %s" },
    { WS_TXT_ST_PART_PERIOD, (STRPTR)"%s s, a click every %s s" },
    { WS_TXT_CLICKS_NONE, (STRPTR)"no clicks" },
    { WS_TXT_CLICKS_ONE, (STRPTR)"1 click" },
    { WS_TXT_CLICKS_MANY, (STRPTR)"%ld clicks" },
    { WS_TXT_ST_NOMUSIC, (STRPTR)"No music found: the whole file is kept" },
    { WS_TXT_ST_SNAPPED_IN, (STRPTR)"In: %s, snapped to a zero crossing (frame %s)" },
    { WS_TXT_ST_SNAPPED_OUT, (STRPTR)"Out: %s, snapped to a zero crossing (frame %s)" },
    { WS_TXT_ST_PUT_IN, (STRPTR)"In: %s (frame %s)" },
    { WS_TXT_ST_PUT_OUT, (STRPTR)"Out: %s (frame %s)" },
    { WS_TXT_ST_PREFS_SAVED, (STRPTR)"Settings saved (ENVARC:WaveSmith.prefs)" },
    { WS_TXT_ST_PREFS_PROGDIR, (STRPTR)"Settings saved beside WaveSmith (PROGDIR:WaveSmith.prefs)" },
    { WS_TXT_ST_MARGINS, (STRPTR)"Lead %s, tail %s: for the files opened" },
    { WS_TXT_ST_MARKED, (STRPTR)"Track %ld starts at %s" },
    { WS_TXT_ST_MARK_WHERE, (STRPTR)"Click where the track starts first (or press Return while it plays)" },
    { WS_TXT_ST_MARKS_FULL, (STRPTR)"No more markers: 99 tracks is a cue sheet's most" },
    { WS_TXT_ST_MARK_NOSEL, (STRPTR)"Click a marker first: Del takes that one away" },
    { WS_TXT_ST_MARK_PICKED, (STRPTR)"Track %ld's marker: drag it, or Del takes it away" },
    { WS_TXT_ST_UNMARKED, (STRPTR)"Marker taken away" },
    { WS_TXT_ST_MEASURING, (STRPTR)"Measuring the level for Find tracks" },
    { WS_TXT_ST_FIND_CANCELLED, (STRPTR)"Find tracks cancelled: the markers as they were" },
    { WS_TXT_ST_NOTRACKS, (STRPTR)"No tracks found: put the markers by hand (Return at the play marker)" },
    { WS_TXT_ST_TRACKS, (STRPTR)"%ld tracks: a marker at each one's start (Undo takes them back)" },
    { WS_TXT_ST_SPLITMARKS_WHERE, (STRPTR)"Put markers between In and Out first (Return, or Edit/Find tracks)" },
    { WS_TXT_ST_SPLITMARKS, (STRPTR)"Split into %ld tracks: %s and on" },
    { WS_TXT_ST_MARGINS_KEPT, (STRPTR)"Lead %s, tail %s: from the next file on (In and Out were placed here)" },
    { WS_TXT_MG_TITLE, (STRPTR)"Lead and tail" },
    { WS_TXT_MG_LEAD, (STRPTR)"In: so long before the music" },
    { WS_TXT_MG_TAIL, (STRPTR)"Out: so long after the music" },
    { WS_TXT_MG_MS, (STRPTR)"ms" },
    { WS_TXT_MG_USE, (STRPTR)"_Use" },
    { WS_TXT_MG_CANCEL, (STRPTR)"_Cancel" },
    { WS_TXT_ST_SAVING, (STRPTR)"Saving %s: %ld %%%s" },
    { WS_TXT_ST_SAVED, (STRPTR)"Saved: %s kept" },
    { WS_TXT_ST_NONEXT, (STRPTR)"No sound file after this one in its drawer" },
    { WS_TXT_ST_NOHELP, (STRPTR)"The manual is not installed (WaveSmith.guide)" },
    { WS_TXT_ST_BADTIME, (STRPTR)"Not a time: type m:ss.mmm" },
    { WS_TXT_ST_PLAY_BUSY, (STRPTR)"The card is in use by %s" },
    { WS_TXT_ST_PLAY_NOCARD, (STRPTR)"No Prisma Megamix card found" },
    { WS_TXT_ST_PLAY_NOLIB, (STRPTR)"Nothing to play through: no prisma.library 3 (Settings, Play through)" },
    { WS_TXT_ST_PLAY_NOAHI, (STRPTR)"Playing through AHI needs ahi.device 4" },
    { WS_TXT_ST_PLAY_AHIMODE, (STRPTR)"AHI's mode is not there or in use (Settings, AHI mode...)" },
    { WS_TXT_ST_PLAY_NOMEM, (STRPTR)"Not enough memory to play" },
    { WS_TXT_ST_PLAY_AHIBUSY, (STRPTR)"Playing through AHI: the card is in use by %s" },
    { WS_TXT_ST_PLAY_AHIJOB, (STRPTR)"Through AHI: the card encodes; its input is silent until that ends" },
    { WS_TXT_ST_AHIMODE, (STRPTR)"AHI mode: %s" },
    { WS_TXT_AHI_TITLE, (STRPTR)"WaveSmith plays through" },
    { WS_TXT_AHI_DEFAULT, (STRPTR)"AHI's default" },
    { WS_TXT_ST_PLAY_ERR, (STRPTR)"The card would not play (%ld)" },
    { WS_TXT_ST_PLAYING, (STRPTR)"Playing %s to %s" },
    { WS_TXT_ST_PLAYED, (STRPTR)"Played %s to %s" },
    { WS_TXT_ST_STOPPED, (STRPTR)"Stopped at %s" },
    { WS_TXT_ST_ATEND, (STRPTR)"The play marker is at the end: |< or a click to play" },
    { WS_TXT_LEFT_PRE, (STRPTR)", about " },
    { WS_TXT_LEFT_POST, (STRPTR)" left" },
    { WS_TXT_ST_READ_CANCELLED, (STRPTR)"Reading %s cancelled" },
    { WS_TXT_ST_DECODE_CANCELLED, (STRPTR)"Decoding %s cancelled" },
    { WS_TXT_ST_SAVE_CANCELLED, (STRPTR)"Saving cancelled: %s is as it was" },
    { WS_TXT_ST_NOSEL, (STRPTR)"Mark a part first: drag over the sound" },
    { WS_TXT_ST_PAUSED, (STRPTR)"Paused at %s: Play or Space goes on" },
    { WS_TXT_ST_DECODING, (STRPTR)"Decoding %s: %ld %%%s" },
    { WS_TXT_ST_DECODE_FAILED, (STRPTR)"%s could not be decoded (prismadsp.library error %ld)" },
    { WS_TXT_ST_SAVED_AS, (STRPTR)"Saved as %s: %s kept" },
    { WS_TXT_ST_UNDONE, (STRPTR)"Undone: In %s, Out %s" },
    { WS_TXT_ST_REDONE, (STRPTR)"Done again: In %s, Out %s" },
    { WS_TXT_ST_NOUNDO, (STRPTR)"Not enough memory to keep the changes for Undo" },
    { WS_TXT_ST_CUT_JOINED, (STRPTR)"Cut out %s, the parts joined (Undo: Right-Amiga Z)" },
    { WS_TXT_ST_CUT_SILENCE, (STRPTR)"Cut out %s, silence in its place (Undo: Right-Amiga Z)" },
    { WS_TXT_ST_CUTFULL, (STRPTR)"Too many cuts: save first" },
    { WS_TXT_ST_CUTREADING, (STRPTR)"Wait until %s is read" },
    { WS_TXT_ST_SPLIT_WHERE, (STRPTR)"Put the play marker between In and Out first: the split goes there" },
    { WS_TXT_ST_SPLIT, (STRPTR)"Split into %s and %s" },
    { WS_TXT_ST_SPLIT_CANCELLED, (STRPTR)"Split cancelled: nothing written" },
    { WS_TXT_ASK_SPLIT_OVER, (STRPTR)"%s's parts (-1, -2) are there already.\nReplace them?" },
    { WS_TXT_ASK_SPLIT_OVER_GADS, (STRPTR)"_Replace|_Cancel" },
    { WS_TXT_ASK_CUT, (STRPTR)"Cut the marked part out of %s.\nJoin the parts on either side,\nor leave silence in its place?" },
    { WS_TXT_ASK_CUT_GADS, (STRPTR)"_Join|_Silence|_Cancel" },
    { WS_TXT_E_NOTWAV, (STRPTR)"%s is not a WAV file" },
    { WS_TXT_E_NOTPCM16, (STRPTR)"%s: only 16-bit PCM WAV files, mono or stereo, for now" },
    { WS_TXT_ASK_UNSAVED, (STRPTR)"%s has changes that are not saved." },
    { WS_TXT_ASK_UNSAVED_GADS, (STRPTR)"_Save|_Discard|_Cancel" },
    { WS_TXT_ASK_REVERT, (STRPTR)"Discard the changes to %s and read it again?" },
    { WS_TXT_ASK_REVERT_GADS, (STRPTR)"_Revert|_Cancel" },
    { WS_TXT_ASK_SPLITMARKS_OVER, (STRPTR)"Some of %s's tracks (-01, -02 ...) are there already.\nReplace them?" },
    { WS_TXT_ASK_REPLACE, (STRPTR)"A file of that name is there already.\nReplace it with the trimmed sound?" },
    { WS_TXT_ASK_REPLACE_GADS, (STRPTR)"_Replace|_Cancel" },
    { WS_TXT_E_NEEDDSP, (STRPTR)"%s is no 16-bit WAV: other formats need prismadsp.library 1.1" },
    { WS_TXT_E_NOMEM, (STRPTR)"Not enough memory for %s" },
    { WS_TXT_E_EMPTY, (STRPTR)"%s holds no sound" },
    { WS_TXT_E_DOS, (STRPTR)"%s: DOS error %ld" },
    { WS_TXT_E_SAVE, (STRPTR)"Saving failed: DOS error %ld (the file is as it was)" },
    { WS_TXT_DECIMAL, (STRPTR)"." },
    { WS_TXT_THOUSANDS, (STRPTR)"," },
    { WS_TXT_E_SAVE_EMPTY, (STRPTR)"Nothing to save: In and Out are at one place" },
    { WS_TXT_E_KEPT, (STRPTR)"Saving failed half-way: the trimmed sound is %s, the original gone" },
    { WS_TXT_E_KEPT_BAK, (STRPTR)"Saving failed half-way: the trimmed sound is %s, the original is %s" },
    { WS_TXT_E_PREFS, (STRPTR)"Settings not saved: DOS error %ld" },
    { WS_TXT_E_CUE, (STRPTR)"Saved, but not its cue sheet: DOS error %ld" },
    { WS_TXT_E_MARGINS, (STRPTR)"Lead and tail needs slider.gadget and integer.gadget (ReAction)" },
    { WS_TXT_M_PROJECT, (STRPTR)"Project" },
    { WS_TXT_M_OPEN, (STRPTR)"Open..." },
    { WS_TXT_M_NEXT, (STRPTR)"Open next" },
    { WS_TXT_M_SAVE, (STRPTR)"Save" },
    { WS_TXT_M_SAVEAS, (STRPTR)"Save as..." },
    { WS_TXT_M_REVERT, (STRPTR)"Revert" },
    { WS_TXT_M_SPLIT, (STRPTR)"Split at the cursor..." },
    { WS_TXT_M_SPLITMARKS, (STRPTR)"Split at markers..." },
    { WS_TXT_M_ADDMARK, (STRPTR)"Add marker" },
    { WS_TXT_M_DELMARK, (STRPTR)"Remove marker" },
    { WS_TXT_M_FINDTRACKS, (STRPTR)"Find tracks" },
    { WS_TXT_M_EFFECTS, (STRPTR)"Effects" },
    { WS_TXT_M_FADEIN, (STRPTR)"Fade in" },
    { WS_TXT_M_FADEOUT, (STRPTR)"Fade out" },
    { WS_TXT_SHAPE_LINEAR, (STRPTR)"Linear" },
    { WS_TXT_SHAPE_EVEN, (STRPTR)"Even" },
    { WS_TXT_SHAPE_SCURVE, (STRPTR)"S-curve" },
    { WS_TXT_ST_FADED_IN, (STRPTR)"Faded in %s to %s (%s). Undo: Right-Amiga Z" },
    { WS_TXT_ST_FADED_OUT, (STRPTR)"Faded out %s to %s (%s). Undo: Right-Amiga Z" },
    { WS_TXT_LENGTH_FADEIN, (STRPTR)"Length %s, faded in %s s" },
    { WS_TXT_LENGTH_FADEOUT, (STRPTR)"Length %s, faded out %s s" },
    { WS_TXT_LENGTH_FADES, (STRPTR)"Length %s, faded in %s s and out %s s" },
    { WS_TXT_ST_ENDFADE_IN, (STRPTR)"Faded in from In over %s s. Undo: Right-Amiga Z" },
    { WS_TXT_ST_ENDFADE_OUT, (STRPTR)"Faded out to Out over %s s. Undo: Right-Amiga Z" },
    { WS_TXT_M_GAIN, (STRPTR)"Gain..." },
    { WS_TXT_M_NORMALISE, (STRPTR)"Normalise" },
    { WS_TXT_M_PLAIN, (STRPTR)"Remove fades and gain" },
    { WS_TXT_GN_TITLE, (STRPTR)"Gain" },
    { WS_TXT_GN_GAIN, (STRPTR)"Gain" },
    { WS_TXT_GN_DB, (STRPTR)"dB" },
    { WS_TXT_GN_PEAK, (STRPTR)"The part's peak: %s dB now, %s dB after" },
    { WS_TXT_GN_SILENT, (STRPTR)"The part is silent" },
    { WS_TXT_GN_PEAK_GONE, (STRPTR)"The part's peak: %s dB now, silent after" },
    { WS_TXT_GN_CLIPS, (STRPTR)"Too loud: samples clip at full scale" },
    { WS_TXT_GN_NORMALISE, (STRPTR)"Normalise" },
    { WS_TXT_GN_TO, (STRPTR)"to" },
    { WS_TXT_ST_GAINED, (STRPTR)"Gain %s dB from %s to %s. Undo: Right-Amiga Z" },
    { WS_TXT_ST_GAINED_CLIPS, (STRPTR)"Gain %s dB from %s to %s: some samples clip" },
    { WS_TXT_ST_GAIN_KEPT, (STRPTR)"The gain left as it was" },
    { WS_TXT_ST_NORMALISED, (STRPTR)"Normalised to %s dB: gain %s dB. Undo: Right-Amiga Z" },
    { WS_TXT_ST_NORM_THERE, (STRPTR)"The part peaks at %s dB already" },
    { WS_TXT_ST_NORM_SILENT, (STRPTR)"Nothing to normalise: the part is silent" },
    { WS_TXT_ST_PLAINED, (STRPTR)"Fades and gain removed from %s to %s. Undo: Right-Amiga Z" },
    { WS_TXT_ST_NOTHING_PLAIN, (STRPTR)"No fade or gain there to remove" },
    { WS_TXT_E_GAINWIN, (STRPTR)"Gain needs slider.gadget and string.gadget (ReAction)" },
    { WS_TXT_K_RETURN, (STRPTR)"Return" },
    { WS_TXT_K_DEL, (STRPTR)"Del" },
    { WS_TXT_M_HELP, (STRPTR)"Help..." },
    { WS_TXT_K_HELP, (STRPTR)"Help" },
    { WS_TXT_M_ABOUT, (STRPTR)"About..." },
    { WS_TXT_M_QUIT, (STRPTR)"Quit" },
    { WS_TXT_M_EDIT, (STRPTR)"Edit" },
    { WS_TXT_M_UNDO, (STRPTR)"Undo" },
    { WS_TXT_M_REDO, (STRPTR)"Redo" },
    { WS_TXT_M_CUTOUT, (STRPTR)"Cut out..." },
    { WS_TXT_M_SELALL, (STRPTR)"Select all" },
    { WS_TXT_M_SELNONE, (STRPTR)"Select none" },
    { WS_TXT_M_VIEW, (STRPTR)"View" },
    { WS_TXT_M_ZOOMIN, (STRPTR)"Zoom in" },
    { WS_TXT_M_ZOOMOUT, (STRPTR)"Zoom out" },
    { WS_TXT_M_ALL, (STRPTR)"Show all" },
    { WS_TXT_M_ZOOMSEL, (STRPTR)"Zoom to selection" },
    { WS_TXT_M_ATIN, (STRPTR)"Go to In" },
    { WS_TXT_M_ATOUT, (STRPTR)"Go to Out" },
    { WS_TXT_M_PREVMARK, (STRPTR)"Previous marker" },
    { WS_TXT_M_NEXTMARK, (STRPTR)"Next marker" },
    { WS_TXT_K_F1, (STRPTR)"F1" },
    { WS_TXT_K_F2, (STRPTR)"F2" },
    { WS_TXT_ST_AT_MARK, (STRPTR)"Marker %ld: %s" },
    { WS_TXT_ST_AT_IN, (STRPTR)"In: %s" },
    { WS_TXT_ST_AT_OUT, (STRPTR)"Out: %s" },
    { WS_TXT_ST_NO_MARK_BEFORE, (STRPTR)"No marker before %s" },
    { WS_TXT_ST_NO_MARK_AFTER, (STRPTR)"No marker after %s" },
    { WS_TXT_M_SCALE, (STRPTR)"dB scale?" },
    { WS_TXT_M_PLAYMENU, (STRPTR)"Play" },
    { WS_TXT_M_PLAY, (STRPTR)"Play/Pause" },
    { WS_TXT_M_PLAYSEL, (STRPTR)"Play selection" },
    { WS_TXT_M_STOP, (STRPTR)"Stop" },
    { WS_TXT_M_LISTENIN, (STRPTR)"Listen at In" },
    { WS_TXT_M_LISTENOUT, (STRPTR)"Listen at Out" },
    { WS_TXT_M_LOOP, (STRPTR)"Loop?" },
    { WS_TXT_K_SPACE, (STRPTR)"Space" },
    { WS_TXT_K_SHIFTSPACE, (STRPTR)"Shift Space" },
    { WS_TXT_M_SETTINGS, (STRPTR)"Settings" },
    { WS_TXT_M_PLAYWAY, (STRPTR)"Play through" },
    { WS_TXT_M_WAYAUTO, (STRPTR)"Automatic" },
    { WS_TXT_M_WAYCARD, (STRPTR)"The Prisma card" },
    { WS_TXT_M_WAYAHI, (STRPTR)"AHI" },
    { WS_TXT_M_AHIMODE, (STRPTR)"AHI mode..." },
    { WS_TXT_M_SNAP, (STRPTR)"Snap to zero crossings?" },
    { WS_TXT_M_MARGINS, (STRPTR)"Lead and tail..." },
    { WS_TXT_M_BACKUP, (STRPTR)"Keep a backup?" },
    { WS_TXT_M_ICONS, (STRPTR)"Create icons?" },
    { WS_TXT_M_SAVEPREFS, (STRPTR)"Save settings" },
    { WS_TXT_ABOUT, (STRPTR)"WaveSmith %s\n\nTrims a recording's ends by eye and ear." },
    { WS_TXT_ABOUT_ART, (STRPTR)"Icons and picture by Martin \"Mason\" Merz" },
    { WS_TXT_OK, (STRPTR)"OK" },
    { WS_TXT_SAVEAS_TITLE, (STRPTR)"Save the trimmed sound as" },
    { WS_TXT_OPEN_TITLE, (STRPTR)"Open a sound file" },
    { WS_TXT_NOPLAYER, (STRPTR)"WaveSmith has nothing to play through:\nneither a Prisma Megamix card nor AHI\n(ahi.device 4) was found.\n\nFit the card or install AHI,\nthen start WaveSmith again." },
    { WS_TXT_M_TRIMWIN, (STRPTR)"Trim..." },
    { WS_TXT_TRIM_TITLE, (STRPTR)"Trim" },
    { WS_TXT_SHOW, (STRPTR)"Show" },
    { WS_TXT_HINT_FADELEN, (STRPTR)"The fade's length (Effects, Fade in and Fade out)" },
    { WS_TXT_FADE_NONE, (STRPTR)"none" },
    { WS_TXT_HINT_DECK, (STRPTR)"Rewind: the stop before (start, In, the markers, Out, end); Play; Forward: the next stop; Stop; Pause (Space too)" },
    { WS_TXT_HINT_TRIMWIN, (STRPTR)"The handles' times, nudges, fades and Listen in a window (Right-Amiga T)" },
    { WS_TXT_HINT_FLAGIN, (STRPTR)"In: the view to it, the handle the arrows move (a double-click on its flag on the sound: the Trim window)" },
    { WS_TXT_HINT_FLAGOUT, (STRPTR)"Out: the view to it, the handle the arrows move (a double-click on its flag on the sound: the Trim window)" },
};

STRPTR GetString(struct LocaleInfo *li, LONG stringNum)
{
    STRPTR builtIn;
    LONG   i;
    LONG   count;

    builtIn = (STRPTR)"";
    count = (LONG)(sizeof(WS_CatCompArray) / sizeof(WS_CatCompArray[0]));
    for (i = 0; i < count; i++)
    {
        if (WS_CatCompArray[i].cca_ID == stringNum)
        {
            builtIn = WS_CatCompArray[i].cca_Str;
            break;
        }
    }

    /* Consult the catalog only when locale.library is open AND a catalog is
     * actually loaded; otherwise the built-in English default is used. The
     * NULL check on the result additionally guards against a stub or broken
     * locale.library (e.g. an incomplete emulation) handing back NULL. */
    if (li != NULL && li->li_LocaleBase != NULL && li->li_Catalog != NULL)
    {
        STRPTR s;

        s = GetCatalogStr((struct Catalog *)li->li_Catalog, stringNum,
                          builtIn);
        if (s != NULL)
        {
            return s;
        }
    }
    return builtIn;
}

#endif /* CATCOMP_CODE */

#endif /* WAVESMITH_CAT_H */
