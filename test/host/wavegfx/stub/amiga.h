/*
 * amiga.h -- the AmigaOS the host render harness gives waveform.c: the
 * types (64-bit ULONG here: tags carry pointers), the structures and
 * constants the class uses, and the calls, stubbed in gfx.c over a frame
 * buffer of pen numbers. Not the NDK: only what the class reads.
 */
#ifndef WAVEGFX_AMIGA_H
#define WAVEGFX_AMIGA_H

#include <stddef.h>

typedef unsigned long ULONG;
typedef long LONG;
typedef unsigned short UWORD;
typedef short WORD;
typedef unsigned char UBYTE;
typedef signed char BYTE;
typedef short BOOL;
typedef char *STRPTR;
typedef const char *CONST_STRPTR;
typedef void *APTR;
typedef long BPTR;
typedef ULONG Tag;
#define TRUE 1
#define FALSE 0
#define VOID void

/* exec */
#define MEMF_ANY	0
#define MEMF_CLEAR	(1L << 16)
#define MEMF_PUBLIC	1
#define MEMF_CHIP	2
struct MinNode { struct MinNode *mln_Succ, *mln_Pred; };
struct Node { struct Node *ln_Succ, *ln_Pred; UBYTE ln_Type; BYTE ln_Pri; char *ln_Name; };
struct Task { struct Node tc_Node; };
struct Library { int lib_Dummy; };
APTR AllocVec(ULONG size, ULONG flags);
void FreeVec(APTR p);
void CopyMem(APTR from, APTR to, ULONG n);
void Signal(struct Task *t, ULONG sigs);
struct Task *FindTask(CONST_STRPTR name);
void Disable(void);
void Enable(void);
void Forbid(void);
void Permit(void);
struct Library *OpenLibrary(CONST_STRPTR name, ULONG v);
void CloseLibrary(struct Library *l);

/* utility */
struct TagItem { Tag ti_Tag; ULONG ti_Data; };
#define TAG_DONE	0UL
#define TAG_END		0UL
#define TAG_IGNORE	1UL
#define TAG_MORE	2UL
#define TAG_SKIP	3UL
#define TAG_USER	0x80000000UL
struct Hook { struct MinNode h_MinNode; ULONG (*h_Entry)(); ULONG (*h_SubEntry)(); APTR h_Data; };
struct TagItem *NextTagItem(struct TagItem **tp);
ULONG GetTagData(Tag t, ULONG def, struct TagItem *list);
ULONG CallHookPkt(struct Hook *h, APTR obj, APTR msg);

/* graphics */
struct TextFont { UWORD tf_YSize; UWORD tf_XSize; UWORD tf_Baseline; };
/* bm_Pix NULL: the window's (the frame buffer); else one of AllocBitMap()'s */
struct BitMap { UWORD Depth; WORD bm_W, bm_H; UBYTE *bm_Pix; };
struct ColorMap { int cm_Dummy; };
struct ViewPort { struct ColorMap *ColorMap; };
/* the layer: what waveform.c's paint log reads of it (Flags, the clip
   rects, the damage); the host's window is never covered, never damaged */
struct Rectangle { WORD MinX, MinY, MaxX, MaxY; };
struct Region { struct Rectangle bounds; };
struct ClipRect { struct ClipRect *Next; LONG obscured; };
struct Layer { int l_Dummy; struct ClipRect *ClipRect; UWORD Flags; struct Region *DamageList; };
#define LAYERUPDATING	0x10
#define LAYERREFRESH	0x80
struct RastPort {
	struct Layer *Layer;
	struct BitMap *BitMap;
	UWORD *AreaPtrn;
	UBYTE AreaPtSz;
	UBYTE FgPen, BgPen, DrawMode;
	WORD cp_x, cp_y;
	struct TextFont *Font;
	UWORD TxBaseline;
};
#define JAM1		0
#define JAM2		1
#define COMPLEMENT	2
#define BMA_DEPTH	4
#define BMA_FLAGS	5
#define BMF_INTERLEAVED	4
#define BMF_STANDARD	8
typedef UBYTE *PLANEPTR;
#define PRECISION_IMAGE	0
#define OBP_Precision	0x84000000UL
#define OBP_FailIfBad	0x84000001UL
#define SetAfPt(w, p, n) ((w)->AreaPtrn = (p), (w)->AreaPtSz = (UBYTE) (n))
void RectFill(struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1);
void SetAPen(struct RastPort *rp, ULONG pen);
void SetDrMd(struct RastPort *rp, ULONG mode);
void Move(struct RastPort *rp, LONG x, LONG y);
LONG Text(struct RastPort *rp, CONST_STRPTR s, ULONG n);
WORD TextLength(struct RastPort *rp, CONST_STRPTR s, ULONG n);
void SetFont(struct RastPort *rp, struct TextFont *f);
void InitRastPort(struct RastPort *rp);
ULONG GetBitMapAttr(struct BitMap *bm, ULONG a);
struct BitMap *AllocBitMap(ULONG w, ULONG h, ULONG depth, ULONG flags, struct BitMap *friend_bm);
void FreeBitMap(struct BitMap *bm);
void BltBitMapRastPort(struct BitMap *src, WORD sx, WORD sy, struct RastPort *rp, WORD dx, WORD dy, WORD w, WORD h, UBYTE minterm);
LONG BltBitMap(struct BitMap *src, LONG sx, LONG sy, struct BitMap *dst, LONG dx, LONG dy, LONG w, LONG h, ULONG minterm, ULONG mask, APTR temp);
void WaitBlit(void);
void BltTemplate(PLANEPTR src, LONG sx, LONG mod, struct RastPort *rp, LONG dx, LONG dy, LONG w, LONG h);
LONG ObtainBestPen(struct ColorMap *cm, ULONG r, ULONG g, ULONG b, ...);
void ReleasePen(struct ColorMap *cm, ULONG pen);
ULONG AttemptLockLayerRom(struct Layer *l);
void UnlockLayerRom(struct Layer *l);

/* intuition */
struct IBox { WORD Left, Top, Width, Height; };
struct Screen { struct ViewPort ViewPort; struct RastPort RastPort; };
#define BACKGROUNDPEN	0
#define TEXTPEN		1
#define SHINEPEN	2
#define SHADOWPEN	3
#define FILLPEN		4
#define BLOCKPEN	5
#define FILLTEXTPEN	6
#define NUMDRIPENS	12
struct DrawInfo { UWORD *dri_Pens; struct TextFont *dri_Font; };
struct GadgetInfo { struct Screen *gi_Screen; struct DrawInfo *gi_DrInfo; struct RastPort *gi_RastPort; };
struct Gadget { struct Gadget *NextGadget; WORD LeftEdge, TopEdge, Width, Height; UWORD Flags; };
struct ExtGadget { struct Gadget g; ULONG MoreFlags; };
#define GFLG_DISABLED	0x0100
#define GFLG_EXTENDED	0x8000
struct Window { struct RastPort *RPort; struct Layer *WLayer; };
typedef struct IClass {
	struct Hook cl_Dispatcher;
	ULONG cl_InstOffset;
	ULONG cl_InstSize;
	struct IClass *cl_Super;
} Class;
typedef ULONG Object;
/* a class library's base (intuition/classes.h): OpenLibrary of one gives it */
struct ClassLibrary { struct Library cl_Lib; UWORD cl_Pad; Class *cl_Class; };
typedef struct { ULONG MethodID; } *Msg;
#define INST_DATA(cl, o)	((APTR) ((UBYTE *) (o) + (cl)->cl_InstOffset))
#define GADGETCLASS	"gadgetclass"
#define OM_NEW		0x101
#define OM_DISPOSE	0x102
#define OM_SET		0x103
#define OM_GET		0x104
#define OM_UPDATE	0x107
#define GM_HITTEST	0
#define GM_RENDER	1
#define GM_GOACTIVE	2
#define GM_HANDLEINPUT	3
#define GM_GOINACTIVE	4
#define GM_HELPTEST	5
#define GM_LAYOUT	6
#define GM_DOMAIN	7
#define GM_QUERYHOVERED	15
struct opSet { ULONG MethodID; struct TagItem *ops_AttrList; struct GadgetInfo *ops_GInfo; };
struct opGet { ULONG MethodID; ULONG opg_AttrID; ULONG *opg_Storage; };
struct gpRender { ULONG MethodID; struct GadgetInfo *gpr_GInfo; struct RastPort *gpr_RPort; LONG gpr_Redraw; };
#define GREDRAW_UPDATE	2
#define GREDRAW_REDRAW	1
struct gpLayout { ULONG MethodID; struct GadgetInfo *gpl_GInfo; ULONG gpl_Initial; };
struct gpDomain { ULONG MethodID; struct GadgetInfo *gpd_GInfo; struct RastPort *gpd_RPort; LONG gpd_Which; struct IBox gpd_Domain; struct TagItem *gpd_Attrs; };
#define GDOMAIN_MINIMUM	0
#define GDOMAIN_NOMINAL	1
#define GDOMAIN_MAXIMUM	2
struct AmigaTimeVal { ULONG tv_secs, tv_micro; };
struct InputEvent { struct InputEvent *ie_NextEvent; UBYTE ie_Class; UBYTE ie_SubClass; UWORD ie_Code; UWORD ie_Qualifier; struct AmigaTimeVal ie_TimeStamp; };
#define IECLASS_RAWKEY	1
#define IECLASS_RAWMOUSE 2
#define IECLASS_TIMER	6
#define IECODE_UP_PREFIX 0x80
#define IECODE_LBUTTON	0x68
#define IECODE_RBUTTON	0x69
#define IECODE_NOBUTTON	0xFF
#define SELECTDOWN	IECODE_LBUTTON
#define SELECTUP	(IECODE_LBUTTON | IECODE_UP_PREFIX)
#define MENUDOWN	IECODE_RBUTTON
#define IEQUALIFIER_LSHIFT 1
#define IEQUALIFIER_RSHIFT 2
struct gpInput { ULONG MethodID; struct GadgetInfo *gpi_GInfo; struct InputEvent *gpi_IEvent; LONG *gpi_Termination; struct { WORD X, Y; } gpi_Mouse; struct TabletData *gpi_TabletData; };
struct gpHitTest { ULONG MethodID; struct GadgetInfo *gpht_GInfo; struct { WORD X, Y; } gpht_Mouse; };
struct gpGoInactive { ULONG MethodID; struct GadgetInfo *gpgi_GInfo; ULONG gpgi_Abort; };
#define GMR_GADGETHIT	4
#define GMR_MEACTIVE	0
#define GMR_NOREUSE	2
#define GMR_REUSE	4
#define GMR_VERIFY	8
#define GA_Dummy	(TAG_USER + 0x30000)
#define GA_Disabled	(GA_Dummy + 14)
#define GA_CustomMousePointer (GA_Dummy + 60)
#define GA_PointerType	(GA_Dummy + 61)
#define GA_GadgetHelpText (GA_Dummy + 45)
#define GMR_NOHELPHIT	0x00000000UL
#define GMR_HELPCODE	0x00010000UL
#define GMR_HELPHIT	0xFFFFFFFFUL
#define POINTERTYPE_NORMAL 0
#define POINTERTYPE_EASTWESTRESIZE 10
#define POINTERTYPE_HAND 11
#define POINTERTYPE_TEXT 30
#define IM_DRAWFRAME	0x207
#define IM_FRAMEBOX	0x20A
#define IDS_NORMAL	0
#define FRAMEF_MINIMAL	2
struct impDraw { ULONG MethodID; struct RastPort *imp_RPort; struct { WORD X, Y; } imp_Offset; ULONG imp_State; struct DrawInfo *imp_DrInfo; struct { WORD Width, Height; } imp_Dimensions; };
#define BEVEL_Style	(TAG_USER + 0x4000001)
#define BVS_NONE	6
#define BVS_FIELD	3
Class *MakeClass(CONST_STRPTR id, CONST_STRPTR super, Class *superp, ULONG size, ULONG flags);
BOOL FreeClass(Class *cl);
APTR NewObject(Class *cl, CONST_STRPTR id, ...);
void DisposeObject(APTR o);
struct RastPort *ObtainGIRPort(struct GadgetInfo *gi);
void ReleaseGIRPort(struct RastPort *rp);
BOOL DoubleClick(ULONG s1, ULONG m1, ULONG s2, ULONG m2);
ULONG DoMethodA(Object *o, Msg m);
ULONG DoMethod(Object *o, ULONG id, ...);
ULONG DoSuperMethodA(Class *cl, Object *o, Msg m);

#endif
