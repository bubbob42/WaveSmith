/*
 * cuesheet.c -- see cuesheet.h
 */
#include <exec/types.h>

#include "cuesheet.h"
#include "textutil.h"

#define LINE_MAX	160		/* a longer line is none of a sheet's we read */

/* ---- writing ------------------------------------------------------------ */

/* two digits */
static STRPTR put2(STRPTR p, ULONG n)
{
	*p++ = (char) ('0' + n / 10 % 10);
	*p++ = (char) ('0' + n % 10);
	*p = '\0';
	return p;
}

/* frame as mm:ss:ff -- the seconds first, the rest after: frame x 75
   would wrap past 21 minutes at 44.1 kHz */
struct Msf {
	ULONG ms_Frame, ms_Rate;
};

static STRPTR put_msf(STRPTR p, const struct Msf *m)
{
	ULONG secs = m->ms_Frame / m->ms_Rate, rem = m->ms_Frame % m->ms_Rate;

	if (secs / 60 > 99) p = Text_PutNumber(p, secs / 60);
	else p = put2(p, secs / 60);
	p = Text_Put(p, (CONST_STRPTR) ":");
	p = put2(p, secs % 60);
	p = Text_Put(p, (CONST_STRPTR) ":");
	return put2(p, rem * 75 / m->ms_Rate);
}

ULONG Cue_Write(const struct CueSheet *cs, STRPTR buf, ULONG size)
{
	STRPTR p = buf;
	struct Msf m;
	UWORD i, n = cs->cs_Count > CUE_MARKS_MAX ? CUE_MARKS_MAX : cs->cs_Count;
	ULONG k;

	/* bounded: a name of CUE_NAMELEN, 99 tracks of 66 bytes at most */
	if (size < CUE_TEXTMAX || !cs->cs_Rate) return 0;
	p = Text_Put(p, (CONST_STRPTR) "REM COMMENT \"WaveSmith\"\n");
	p = Text_Put(p, (CONST_STRPTR) "FILE \"");
	/* a quote would end the name early: an apostrophe for it */
	for (k = 0; cs->cs_File[k] && k < CUE_NAMELEN; k++) *p++ = cs->cs_File[k] == '"' ? '\'' : cs->cs_File[k];
	p = Text_Put(p, (CONST_STRPTR) "\" WAVE\n");
	m.ms_Rate = cs->cs_Rate;
	for (i = 0; i <= n; i++) {
		p = Text_Put(p, (CONST_STRPTR) "  TRACK ");
		p = put2(p, (ULONG) i + 1);
		p = Text_Put(p, (CONST_STRPTR) " AUDIO\n");
		m.ms_Frame = i ? cs->cs_Marks[i - 1] : 0;
		if (i) {
			p = Text_Put(p, (CONST_STRPTR) "    REM FRAME ");
			p = Text_PutNumber(p, m.ms_Frame);
			p = Text_Put(p, (CONST_STRPTR) "\n");
		}
		p = Text_Put(p, (CONST_STRPTR) "    INDEX 01 ");
		p = put_msf(p, &m);
		p = Text_Put(p, (CONST_STRPTR) "\n");
	}
	return (ULONG) (p - buf);
}

/* ---- reading ------------------------------------------------------------ */

/* the next word of *s (blanks first passed over) is word, either case */
static BOOL word_is(CONST_STRPTR *s, CONST_STRPTR word)
{
	CONST_STRPTR p = *s;

	while (*p == ' ' || *p == '\t') p++;
	if (!Text_Prefix(&p, word)) return FALSE;
	if (*p && *p != ' ' && *p != '\t') return FALSE;
	*s = p;
	return TRUE;
}

static BOOL number_at(CONST_STRPTR *s, ULONG *n)
{
	while (**s == ' ' || **s == '\t') (*s)++;
	return Text_GetNumber(s, n);
}

/* "mm:ss:ff" as frames at rate */
static BOOL msf_at(CONST_STRPTR *s, ULONG rate, ULONG *frame)
{
	ULONG mm, ss, ff;

	if (!number_at(s, &mm) || **s != ':') return FALSE;
	(*s)++;
	if (!Text_GetNumber(s, &ss) || **s != ':' || ss > 59) return FALSE;
	(*s)++;
	if (!Text_GetNumber(s, &ff) || ff > 74 || mm > 9999) return FALSE;
	*frame = (mm * 60 + ss) * rate + ff * rate / 75;
	return TRUE;
}

/* a later track's start a marker: past the first track's, inside the
   sound, in its place among the rest (rising, each once) */
static void add_mark(struct CueRead *cr, ULONG f)
{
	UWORD i, k;

	if (f <= cr->cr_First || f >= cr->cr_Frames || cr->cr_Count >= CUE_MARKS_MAX) return;
	for (i = 0; i < cr->cr_Count && cr->cr_Marks[i] < f; i++) ;
	if (i < cr->cr_Count && cr->cr_Marks[i] == f) return;
	for (k = cr->cr_Count; k > i; k--) cr->cr_Marks[k] = cr->cr_Marks[k - 1];
	cr->cr_Marks[i] = f;
	cr->cr_Count++;
}

/* the track being read */
struct Track {
	UWORD tk_Seen;			/* TRACK lines so far */
	BOOL tk_In;			/* a TRACK line seen */
	BOOL tk_HasRem, tk_HasIndex;
	ULONG tk_Rem, tk_Index;
};

static void end_track(struct CueRead *cr, struct Track *tk)
{
	ULONG f = tk->tk_HasRem ? tk->tk_Rem : tk->tk_Index;

	if (tk->tk_In && (tk->tk_HasRem || tk->tk_HasIndex)) {
		/* the first: where the tracks begin (inside the sound) */
		if (tk->tk_Seen == 1) cr->cr_First = f < cr->cr_Frames ? f : 0;
		else add_mark(cr, f);
	}
	tk->tk_In = tk->tk_HasRem = tk->tk_HasIndex = FALSE;
}

UWORD Cue_Read(struct CueRead *cr, CONST_STRPTR text, ULONG len)
{
	char line[LINE_MAX];
	struct Track tk;
	CONST_STRPTR s;
	ULONG pos = 0, n, v;
	UWORD files = 0;
	BOOL cut;

	cr->cr_Count = 0;
	cr->cr_First = 0;
	tk.tk_Seen = 0;
	tk.tk_In = tk.tk_HasRem = tk.tk_HasIndex = FALSE;
	tk.tk_Rem = tk.tk_Index = 0;
	if (!cr->cr_Rate) return 0;
	while (pos < len) {
		n = 0;
		cut = FALSE;
		while (pos < len && text[pos] != '\n') {
			if (n + 1 < sizeof(line)) line[n++] = text[pos];
			else cut = TRUE;
			pos++;
		}
		pos++;
		if (cut) continue;
		line[n] = '\0';
		while (n && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = '\0';
		s = (CONST_STRPTR) line;
		if (word_is(&s, (CONST_STRPTR) "FILE")) {
			/* a second file's tracks are another sound's */
			if (++files > 1) break;
		} else if (word_is(&s, (CONST_STRPTR) "TRACK")) {
			end_track(cr, &tk);
			tk.tk_In = TRUE;
			tk.tk_Seen++;
		} else if (word_is(&s, (CONST_STRPTR) "REM")) {
			if (tk.tk_In && word_is(&s, (CONST_STRPTR) "FRAME") && number_at(&s, &v) && !*s) {
				tk.tk_Rem = v;
				tk.tk_HasRem = TRUE;
			}
		} else if (word_is(&s, (CONST_STRPTR) "INDEX")) {
			if (tk.tk_In && number_at(&s, &v) && v == 1 && msf_at(&s, cr->cr_Rate, &v)) {
				tk.tk_Index = v;
				tk.tk_HasIndex = TRUE;
			}
		}
	}
	end_track(cr, &tk);
	return cr->cr_Count;
}
