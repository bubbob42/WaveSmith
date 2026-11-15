/*
 * gfx.c -- the host render harness's AmigaOS (stub/amiga.h): drawing into
 * a frame buffer of pen numbers, every rectangle bounds-checked, text as
 * boxes 8 pixels a character, pens obtained as new numbers with their
 * colours kept, BOOPSI as far as one class over gadgetclass needs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "stub/amiga.h"
#include "gfx.h"

UBYTE fb[FB_H][FB_W] = { { 0 } };
ULONG palette[256] = { 0 };
static int npens = 8;
long gfx_out_of_box = 0, gfx_inverted = 0, gfx_rects = 0, gfx_texts = 0;
long gfx_vecs = 0, gfx_pens = 0, gfx_objs = 0, gfx_libs = 0, gfx_bitmaps = 0;	/* held now */
long gfx_blits = 0;
long gfx_winrects = 0, gfx_winblits = 0;	/* into the window (the frame buffer) only */
int gfx_standard = 0;
long gfx_templates = 0, gfx_chipread = 0;
ULONG gfx_pointer = 0;		/* the last GA_PointerType handed to gadgetclass */
struct GfxClip gfx_clip = { 0, 0, FB_W - 1, FB_H - 1 };

void gfx_reset(void)
{
	memset(fb, 0, sizeof(fb));
	palette[0] = 0xAAAAAA; palette[1] = 0x000000; palette[2] = 0xFFFFFF; palette[3] = 0x000000;
	palette[4] = 0x6688BB; palette[5] = 0x6688BB; palette[6] = 0x000000; palette[7] = 0xFFFFFF;
}

/* the MEMF_CHIP blocks held: what the blitter may read (BltTemplate) */
#define CHIP_BLOCKS 16
static struct { UBYTE *at; ULONG size; } chip[CHIP_BLOCKS];

APTR AllocVec(ULONG size, ULONG flags)
{
	APTR p = calloc(1, size ? size : 1);
	int k;

	if (p) gfx_vecs++;
	if (p && (flags & MEMF_CHIP)) {
		for (k = 0; k < CHIP_BLOCKS && chip[k].at; k++) {
		}
		if (k < CHIP_BLOCKS) {
			chip[k].at = (UBYTE *) p;
			chip[k].size = size;
		}
	}
	return p;
}

void FreeVec(APTR p)
{
	int k;

	if (p) gfx_vecs--;
	for (k = 0; k < CHIP_BLOCKS; k++)
		if (chip[k].at == (UBYTE *) p) chip[k].at = NULL;
	free(p);
}

void CopyMem(APTR from, APTR to, ULONG n) { memmove(to, from, n); }

void Signal(struct Task *t, ULONG s) { (void) t; (void) s; }
struct Task *FindTask(CONST_STRPTR name) { static struct Task me; (void) name; return &me; }
void Disable(void) { }
void Enable(void) { }
void Forbid(void) {}
void Permit(void) {}
/* every library a class library: bevel.image's base carries its class,
   which NewObject() knows (the public name "bevel.image" does not
   resolve on OS 3.2: a class must take the class from the base) */
static Class bevel_class;
static struct ClassLibrary class_lib;

struct Library *OpenLibrary(CONST_STRPTR name, ULONG v)
{
	(void) name;
	(void) v;
	class_lib.cl_Class = &bevel_class;
	gfx_libs++;
	return &class_lib.cl_Lib;
}
void CloseLibrary(struct Library *l) { if (l) gfx_libs--; }

struct TagItem *NextTagItem(struct TagItem **tp)
{
	struct TagItem *t;

	for (;;) {
		t = *tp;
		if (!t) return NULL;
		switch (t->ti_Tag) {
		case TAG_DONE: *tp = NULL; return NULL;
		case TAG_IGNORE: (*tp)++; continue;
		case TAG_MORE: *tp = (struct TagItem *) t->ti_Data; continue;
		case TAG_SKIP: *tp += t->ti_Data + 1; continue;
		default: (*tp)++; return t;
		}
	}
}

ULONG GetTagData(Tag tag, ULONG def, struct TagItem *list)
{
	struct TagItem *t;

	while ((t = NextTagItem(&list))) if (t->ti_Tag == tag) return t->ti_Data;
	return def;
}

ULONG CallHookPkt(struct Hook *h, APTR obj, APTR msg)
{
	return ((ULONG (*)(struct Hook *, APTR, APTR)) h->h_Entry)(h, obj, msg);
}

/* a pixel into an off-screen bitmap: outside it is memory that is not
   its on the Amiga -- counted as out of the box */
static void plot_bm(struct BitMap *bm, LONG x, LONG y, UBYTE pen)
{
	if (x < 0 || y < 0 || x >= bm->bm_W || y >= bm->bm_H) {
		gfx_out_of_box++;
		return;
	}
	bm->bm_Pix[y * bm->bm_W + x] = pen;
}

static void plot(struct RastPort *rp, LONG x, LONG y, BOOL bit)
{
	if (rp->BitMap && rp->BitMap->bm_Pix) {
		if (bit) plot_bm(rp->BitMap, x, y, rp->FgPen);
		else if (rp->DrawMode == JAM2) plot_bm(rp->BitMap, x, y, rp->BgPen);
		return;
	}
	if (x < gfx_clip.x0 || x > gfx_clip.x1 || y < gfx_clip.y0 || y > gfx_clip.y1) {
		gfx_out_of_box++;
		return;
	}
	if (bit) fb[y][x] = rp->FgPen;
	else if (rp->DrawMode == JAM2) fb[y][x] = rp->BgPen;
}

void RectFill(struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1)
{
	LONG x, y;
	UWORD rows, w;

	gfx_rects++;
	if (!rp->BitMap || !rp->BitMap->bm_Pix) gfx_winrects++;
	if (x1 < x0 || y1 < y0) {
		gfx_inverted++;
		return;
	}
	for (y = y0; y <= y1; y++) {
		for (x = x0; x <= x1; x++) {
			if (rp->AreaPtrn) {
				rows = (UWORD) (1 << rp->AreaPtSz);
				w = rp->AreaPtrn[y % rows];
				plot(rp, x, y, (BOOL) ((w >> (15 - (x & 15))) & 1));
			} else {
				plot(rp, x, y, TRUE);
			}
		}
	}
}

void SetAPen(struct RastPort *rp, ULONG pen) { rp->FgPen = (UBYTE) pen; }
void SetDrMd(struct RastPort *rp, ULONG mode) { rp->DrawMode = (UBYTE) mode; }
void Move(struct RastPort *rp, LONG x, LONG y) { rp->cp_x = (WORD) x; rp->cp_y = (WORD) y; }

LONG Text(struct RastPort *rp, CONST_STRPTR s, ULONG n)
{
	ULONG i;
	LONG x, y, top = rp->cp_y - (LONG) rp->TxBaseline;
	UWORD *pat = rp->AreaPtrn;

	gfx_texts++;
	rp->AreaPtrn = NULL;
	for (i = 0; i < n; i++) {
		if (s[i] == ' ') continue;
		/* a glyph's box, with a notch for digits so they read as text */
		for (y = top + 1; y <= top + 6; y++)
			for (x = rp->cp_x + (LONG) i * 8 + 1; x <= rp->cp_x + (LONG) i * 8 + 5; x++)
				if (y == top + 1 || y == top + 6 || x == rp->cp_x + (LONG) i * 8 + 1 ||
				    (s[i] >= 'A' && x == rp->cp_x + (LONG) i * 8 + 5))
					plot(rp, x, y, TRUE);
	}
	rp->AreaPtrn = pat;
	rp->cp_x = (WORD) (rp->cp_x + n * 8);
	return 0;
}

WORD TextLength(struct RastPort *rp, CONST_STRPTR s, ULONG n) { (void) rp; (void) s; return (WORD) (n * 8); }
void SetFont(struct RastPort *rp, struct TextFont *f) { rp->Font = f; rp->TxBaseline = f ? f->tf_Baseline : 6; }
void InitRastPort(struct RastPort *rp) { memset(rp, 0, sizeof(*rp)); }

struct BitMap *AllocBitMap(ULONG w, ULONG h, ULONG depth, ULONG flags, struct BitMap *friend_bm)
{
	struct BitMap *bm = (struct BitMap *) calloc(1, sizeof(*bm));

	(void) flags;
	(void) friend_bm;
	if (!bm || !(bm->bm_Pix = (UBYTE *) calloc(w ? w : 1, h ? h : 1))) {
		free(bm);
		return NULL;
	}
	bm->Depth = (UWORD) depth;
	bm->bm_W = (WORD) w;
	bm->bm_H = (WORD) h;
	gfx_bitmaps++;
	return bm;
}

void FreeBitMap(struct BitMap *bm)
{
	if (!bm) return;
	free(bm->bm_Pix);
	free(bm);
	gfx_bitmaps--;
}

void WaitBlit(void) {}

/* the template's set bits in the pen (JAM1), each word read inside a
   MEMF_CHIP block -- the blitter reads nothing else -- else counted */
void BltTemplate(PLANEPTR src, LONG sx, LONG mod, struct RastPort *rp, LONG dx, LONG dy, LONG w, LONG h)
{
	LONG x, y, b;
	const UBYTE *p;
	int k;
	UBYTE dm = rp->DrawMode;

	gfx_templates++;
	if (!rp->BitMap || !rp->BitMap->bm_Pix) gfx_winrects++;
	if (sx < 0 || sx > 15 || w <= 0 || h <= 0 || ((size_t) src & 1)) {
		gfx_inverted++;
		return;
	}
	rp->DrawMode = JAM1;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			b = sx + x;
			p = src + y * mod + (b >> 4) * 2;
			for (k = 0; k < CHIP_BLOCKS; k++)
				if (chip[k].at && p >= chip[k].at && p + 2 <= chip[k].at + chip[k].size) break;
			if (k == CHIP_BLOCKS) {
				gfx_chipread++;
				continue;
			}
			/* a word as the class wrote it: the leftmost pixel its top bit */
			if ((*(const UWORD *) p >> (15 - (b & 15))) & 1) plot(rp, dx + x, dy + y, TRUE);
		}
	}
	rp->DrawMode = dm;
}

/* a copy (minterm 0xC0) from src into rp: every source pixel inside src,
   every target pixel inside the box the paint may touch */
void BltBitMapRastPort(struct BitMap *src, WORD sx, WORD sy, struct RastPort *rp, WORD dx, WORD dy, WORD w, WORD h, UBYTE minterm)
{
	LONG x, y;
	UBYTE fg = rp->FgPen, dm = rp->DrawMode;

	gfx_blits++;
	if (!rp->BitMap || !rp->BitMap->bm_Pix) gfx_winblits++;
	if (minterm != 0xC0 || w <= 0 || h <= 0) {
		gfx_inverted++;
		return;
	}
	rp->DrawMode = JAM1;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (sx + x < 0 || sy + y < 0 || sx + x >= src->bm_W || sy + y >= src->bm_H) {
				gfx_out_of_box++;
				continue;
			}
			rp->FgPen = src->bm_Pix[(sy + y) * src->bm_W + sx + x];
			plot(rp, dx + x, dy + y, TRUE);
		}
	}
	rp->FgPen = fg;
	rp->DrawMode = dm;
}
ULONG GetBitMapAttr(struct BitMap *bm, ULONG a)
{
	/* the host's: chunky, never interleaved -- or said planar in chip RAM,
	   the native screens' (the class's mask then) */
	if (a == BMA_FLAGS) return gfx_standard ? BMF_STANDARD : 0;
	return bm ? bm->Depth : 8;
}

/* a copy (minterm 0xC0) between two of AllocBitMap()'s: every pixel inside
   both, else counted out of the box */
LONG BltBitMap(struct BitMap *src, LONG sx, LONG sy, struct BitMap *dst, LONG dx, LONG dy, LONG w, LONG h, ULONG minterm, ULONG mask, APTR temp)
{
	LONG x, y;

	(void) mask;
	(void) temp;
	gfx_blits++;
	if (minterm != 0xC0 || w <= 0 || h <= 0 || !src->bm_Pix || !dst->bm_Pix) {
		gfx_inverted++;
		return 0;
	}
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (sx + x < 0 || sy + y < 0 || sx + x >= src->bm_W || sy + y >= src->bm_H ||
			    dx + x < 0 || dy + y < 0 || dx + x >= dst->bm_W || dy + y >= dst->bm_H) {
				gfx_out_of_box++;
				continue;
			}
			dst->bm_Pix[(dy + y) * dst->bm_W + dx + x] = src->bm_Pix[(sy + y) * src->bm_W + sx + x];
		}
	}
	return (LONG) w * h;
}

LONG ObtainBestPen(struct ColorMap *cm, ULONG r, ULONG g, ULONG b, ...)
{
	(void) cm;
	if (npens >= 255) return -1;
	palette[npens] = ((r >> 24) << 16) | ((g >> 24) << 8) | (b >> 24);
	gfx_pens++;
	return npens++;
}

void ReleasePen(struct ColorMap *cm, ULONG pen) { (void) cm; if (pen != ~0UL) gfx_pens--; }
ULONG AttemptLockLayerRom(struct Layer *l) { (void) l; return 1; }
void UnlockLayerRom(struct Layer *l) { (void) l; }

/* BOOPSI: one class over gadgetclass, and bevel.image as a frame; each
   object's class kept, for DisposeObject */
#define MAX_OBJS 16
static struct { int dummy; } bevel_obj;
static struct { APTR o; Class *cl; } objs[MAX_OBJS];

Class *MakeClass(CONST_STRPTR id, CONST_STRPTR super, Class *superp, ULONG size, ULONG flags)
{
	Class *cl = calloc(1, sizeof(Class));

	(void) id; (void) super; (void) superp; (void) flags;
	cl->cl_InstOffset = sizeof(struct ExtGadget);
	cl->cl_InstSize = size;
	gfx_objs++;
	return cl;
}

BOOL FreeClass(Class *cl)
{
	int i;

	for (i = 0; i < MAX_OBJS; i++) if (objs[i].o && objs[i].cl == cl) return FALSE;
	if (cl) gfx_objs--;
	free(cl);
	return TRUE;
}

APTR NewObject(Class *cl, CONST_STRPTR id, ...)
{
	va_list ap;
	struct TagItem tags[64];
	struct opSet ops;
	int n = 0;

	(void) id;
	if (!cl) return NULL;
	if (cl == &bevel_class) {
		gfx_objs++;
		return (APTR) &bevel_obj;
	}
	va_start(ap, id);
	for (;;) {
		tags[n].ti_Tag = va_arg(ap, ULONG);
		if (tags[n].ti_Tag == TAG_DONE) break;
		tags[n].ti_Data = va_arg(ap, ULONG);
		if (++n >= 63) break;
	}
	tags[n].ti_Tag = TAG_DONE;
	va_end(ap);
	ops.MethodID = OM_NEW;
	ops.ops_AttrList = tags;
	ops.ops_GInfo = NULL;
	return (APTR) CallHookPkt(&cl->cl_Dispatcher, (APTR) cl, &ops);
}

void DisposeObject(APTR o)
{
	struct opSet m;		/* any message: only its MethodID read */
	int i;

	if (!o) return;
	if (o == (APTR) &bevel_obj) {
		gfx_objs--;
		return;
	}
	for (i = 0; i < MAX_OBJS; i++) {
		if (objs[i].o != o) continue;
		memset(&m, 0, sizeof(m));
		m.MethodID = OM_DISPOSE;
		CallHookPkt(&objs[i].cl->cl_Dispatcher, o, &m);
		return;
	}
}
struct RastPort *ObtainGIRPort(struct GadgetInfo *gi) { return gi ? gi->gi_RastPort : NULL; }
void ReleaseGIRPort(struct RastPort *rp) { (void) rp; }
BOOL DoubleClick(ULONG s1, ULONG m1, ULONG s2, ULONG m2) { return (BOOL) ((s2 - s1) * 1000000UL + m2 - m1 < 500000UL); }

ULONG DoMethodA(Object *o, Msg m)
{
	struct impDraw *imp = (struct impDraw *) m;
	struct RastPort *rp;
	LONG x, y, w, h;

	if ((APTR) o != (APTR) &bevel_obj || m->MethodID != IM_DRAWFRAME) return 0;
	rp = imp->imp_RPort;
	x = imp->imp_Offset.X; y = imp->imp_Offset.Y; w = imp->imp_Dimensions.Width; h = imp->imp_Dimensions.Height;
	rp->AreaPtrn = NULL;
	SetAPen(rp, 1); RectFill(rp, x, y, x + w - 1, y); RectFill(rp, x, y, x, y + h - 1);
	SetAPen(rp, 2); RectFill(rp, x, y + h - 1, x + w - 1, y + h - 1); RectFill(rp, x + w - 1, y, x + w - 1, y + h - 1);
	SetAPen(rp, 2); RectFill(rp, x + 1, y + 1, x + w - 2, y + 1); RectFill(rp, x + 1, y + 1, x + 1, y + h - 2);
	SetAPen(rp, 1); RectFill(rp, x + 1, y + h - 2, x + w - 2, y + h - 2); RectFill(rp, x + w - 2, y + 1, x + w - 2, y + h - 2);
	return 0;
}

ULONG DoMethod(Object *o, ULONG id, ...)
{
	va_list ap;
	struct IBox *fb2;

	if ((APTR) o == (APTR) &bevel_obj && id == IM_FRAMEBOX) {
		va_start(ap, id);
		(void) va_arg(ap, ULONG);
		fb2 = (struct IBox *) va_arg(ap, ULONG);
		va_end(ap);
		fb2->Left = -2; fb2->Top = -2; fb2->Width = 4; fb2->Height = 4;
		return 1;
	}
	return 0;
}

ULONG DoSuperMethodA(Class *cl, Object *o, Msg m)
{
	struct opSet *ops = (struct opSet *) m;
	struct TagItem *t, *list;
	APTR p;
	int i;

	switch (m->MethodID) {
	case OM_NEW:
		for (i = 0; i < MAX_OBJS && objs[i].o; i++) ;
		if (i == MAX_OBJS || !(p = calloc(1, cl->cl_InstOffset + cl->cl_InstSize))) return 0;
		objs[i].o = p;
		objs[i].cl = cl;
		gfx_objs++;
		return (ULONG) p;
	case OM_DISPOSE:
		for (i = 0; i < MAX_OBJS; i++) {
			if (objs[i].o != (APTR) o) continue;
			objs[i].o = NULL;
			gfx_objs--;
			free(o);
		}
		return 0;
	case OM_SET:
		list = ops->ops_AttrList;
		while ((t = NextTagItem(&list))) if (t->ti_Tag == GA_PointerType) gfx_pointer = t->ti_Data;
		return 0;
	default:
		(void) o;
		return 0;
	}
}

/* the frame as a PPM, each line twice (PAL Hires's pixel shape) */
void gfx_dump(const char *path, int w, int h)
{
	FILE *f = fopen(path, "wb");
	int x, y, k;

	if (!f) return;
	fprintf(f, "P6\n%d %d\n255\n", w, h * 2);
	for (y = 0; y < h; y++)
		for (k = 0; k < 2; k++)
			for (x = 0; x < w; x++) {
				ULONG c = palette[fb[y][x]];
				fputc((int) (c >> 16) & 255, f); fputc((int) (c >> 8) & 255, f); fputc((int) c & 255, f);
			}
	fclose(f);
}
