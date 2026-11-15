/*
 * editprefs.c -- see editprefs.h
 */
#include <exec/types.h>

#include "editprefs.h"
#include "textutil.h"

#define LINE_MAX	64		/* a longer line is no setting of ours */

void EditPrefs_Defaults(struct EditPrefs *ep)
{
	ep->ep_Snap = TRUE;
	ep->ep_LeadMs = EDITPREFS_LEAD;
	ep->ep_TailMs = EDITPREFS_TAIL;
	ep->ep_Backup = FALSE;
	ep->ep_Icons = TRUE;
	ep->ep_Overview = EDITPREFS_OVER;
	ep->ep_Play = EDITPREFS_PLAY_AUTO;
	ep->ep_AhiID = 0;
	ep->ep_AhiFreq = 0;
	ep->ep_FadeShape = 1;		/* Even */
	ep->ep_Normalise = EDITPREFS_NORMALISE;
}

static CONST_STRPTR const play_words[3] = { "AUTO", "CARD", "AHI" };
static CONST_STRPTR const shape_words[EDITPREFS_SHAPES] = { "LINEAR", "EVEN", "SCURVE" };

/* ---- reading ------------------------------------------------------------ */

static BOOL yes_no(CONST_STRPTR v, BOOL *b)
{
	if (Text_Same(v, "YES") || Text_Same(v, "ON") || Text_Same(v, "1"))
		*b = TRUE;
	else if (Text_Same(v, "NO") || Text_Same(v, "OFF") || Text_Same(v, "0"))
		*b = FALSE;
	else return FALSE;
	return TRUE;
}

/* a value, and the numbers it may be */
struct Range {
	CONST_STRPTR r_Text;
	ULONG r_Lo, r_Hi;
};

static BOOL number(const struct Range *r, ULONG *n)
{
	CONST_STRPTR v = r->r_Text;
	ULONG x;

	if (!Text_GetNumber(&v, &x) || *v || x < r->r_Lo || x > r->r_Hi) return FALSE;
	*n = x;
	return TRUE;
}

struct KeyValue {
	CONST_STRPTR kv_Key;
	CONST_STRPTR kv_Value;
};

static BOOL setting(struct EditPrefs *ep, const struct KeyValue *kv)
{
	CONST_STRPTR key = kv->kv_Key, v = kv->kv_Value;
	struct Range r;
	ULONG n;

	r.r_Text = v;
	r.r_Lo = 0;
	if (Text_Same(key, "SNAP")) return yes_no(v, &ep->ep_Snap);
	if (Text_Same(key, "BACKUP")) return yes_no(v, &ep->ep_Backup);
	if (Text_Same(key, "ICONS")) return yes_no(v, &ep->ep_Icons);
	if (Text_Same(key, "LEAD")) {
		r.r_Hi = EDITPREFS_LEAD_MAX;
		if (!number(&r, &n)) return FALSE;
		ep->ep_LeadMs = n;
	} else if (Text_Same(key, "TAIL")) {
		r.r_Hi = EDITPREFS_TAIL_MAX;
		if (!number(&r, &n)) return FALSE;
		ep->ep_TailMs = n;
	} else if (Text_Same(key, "PLAY")) {
		for (n = 0; n < 3 && !Text_Same(v, play_words[n]); n++) ;
		if (n == 3) return FALSE;
		ep->ep_Play = (UWORD) n;
	} else if (Text_Same(key, "FADESHAPE")) {
		for (n = 0; n < EDITPREFS_SHAPES && !Text_Same(v, shape_words[n]); n++) ;
		if (n == EDITPREFS_SHAPES) return FALSE;
		ep->ep_FadeShape = (UWORD) n;
	} else if (Text_Same(key, "AHIMODE") || Text_Same(key, "AHIFREQ")) {
		r.r_Hi = 0xFFFFFFFFUL;
		if (!number(&r, &n)) return FALSE;
		if (Text_Same(key, "AHIMODE")) ep->ep_AhiID = n;
		else ep->ep_AhiFreq = n;
	} else if (Text_Same(key, "NORMALISE")) {
		r.r_Hi = EDITPREFS_NORMALISE_MAX;
		if (!number(&r, &n)) return FALSE;
		ep->ep_Normalise = (UWORD) n;
	} else if (Text_Same(key, "OVERVIEW")) {
		r.r_Lo = EDITPREFS_OVER_MIN;
		r.r_Hi = EDITPREFS_OVER_MAX;
		if (!number(&r, &n)) return FALSE;
		ep->ep_Overview = (UWORD) n;
	} else {
		return FALSE;
	}
	return TRUE;
}

UWORD EditPrefs_Read(struct EditPrefs *ep, CONST_STRPTR text, ULONG len)
{
	char line[LINE_MAX];
	struct KeyValue kv;
	ULONG pos = 0, n;
	STRPTR key, value, k;
	BOOL cut;
	UWORD taken = 0;

	while (pos < len) {
		/* a line; one longer than the buffer is no setting */
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

		key = line;
		while (*key == ' ' || *key == '\t') key++;
		if (!*key || *key == ';' || *key == '#') continue;
		for (value = key; *value && *value != '='; value++) ;
		if (!*value) continue;
		*value++ = '\0';
		for (k = value - 1; k > key && (k[-1] == ' ' || k[-1] == '\t'); k--) k[-1] = '\0';
		while (*value == ' ' || *value == '\t') value++;
		kv.kv_Key = key;
		kv.kv_Value = value;
		if (setting(ep, &kv)) taken++;
	}
	return taken;
}

/* ---- writing ------------------------------------------------------------ */

static STRPTR put_yes_no(STRPTR p, CONST_STRPTR key, BOOL b)
{
	p = Text_Put(Text_Put(p, key), "=");
	return Text_Put(p, b ? "YES\n" : "NO\n");
}

static STRPTR put_number(STRPTR p, CONST_STRPTR key, ULONG n)
{
	p = Text_Put(Text_Put(p, key), "=");
	p = Text_PutNumber(p, n);
	return Text_Put(p, "\n");
}

ULONG EditPrefs_Write(const struct EditPrefs *ep, STRPTR buf, ULONG size)
{
	STRPTR p = buf;

	/* every value a word or a number of at most 10 digits: the worst
	   case fits EDITPREFS_TEXTMAX (the host test writes it) */
	if (size < EDITPREFS_TEXTMAX) return 0;
	p = Text_Put(p, "; WaveSmith settings\n");
	p = put_yes_no(p, "SNAP", ep->ep_Snap);
	p = put_number(p, "LEAD", ep->ep_LeadMs);
	p = put_number(p, "TAIL", ep->ep_TailMs);
	p = put_yes_no(p, "BACKUP", ep->ep_Backup);
	p = put_yes_no(p, "ICONS", ep->ep_Icons);
	p = put_number(p, "OVERVIEW", ep->ep_Overview);
	p = Text_Put(Text_Put(p, "PLAY="), ep->ep_Play < 3 ? play_words[ep->ep_Play] : play_words[0]);
	p = Text_Put(p, "\n");
	p = put_number(p, "AHIMODE", ep->ep_AhiID);
	p = put_number(p, "AHIFREQ", ep->ep_AhiFreq);
	p = Text_Put(Text_Put(p, "FADESHAPE="), ep->ep_FadeShape < EDITPREFS_SHAPES ? shape_words[ep->ep_FadeShape] : shape_words[1]);
	p = Text_Put(p, "\n");
	p = put_number(p, "NORMALISE", ep->ep_Normalise);
	return (ULONG) (p - buf);
}
