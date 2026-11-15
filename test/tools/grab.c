/*
 * grab.c -- DEV:Test/Grab: read a region of the Workbench screen and write
 * it as a binary PPM (RGB), so a host can LOOK at what an unattended test
 * drew.   Grab X/N,Y/N,W/N,H/N,TO/A,WINDOW/S,STEPS/K/N,PORT/K
 *   X Y W H   the region (negative X/Y: from the right/bottom edge; W/H 0:
 *             to the screen's edge)
 *   WINDOW    the active window instead of X Y W H
 *   STEPS n   an animation: n grabs, TO the name's start (TO<nnn>.ppm),
 *             and TOtimes.txt: when each grab began, ms after the first
 *     PORT    before each grab a message to this public port, whose owner
 *             replies once it has drawn the next frame (PrismaRec
 *             FRAMESTEP): frame-exact, however long a grab takes
 *     EVERY t without PORT: free-running, t ticks (1/50 s) between grabs
 *             (0, the default: back to back) -- for a program with no
 *             port; the times file says when each frame really was
 * Both a rectangle at a time: pen screens through ReadPixelArray8 and the
 * palette (GetRGB32), true colour through Picasso96API (p96ReadPixelArray);
 * without the memory for that, a pixel at a time.
 *
 * The region is read whole into memory under Forbid() (after WaitBlit()),
 * then written: read row by row between file writes, a gadget that redraws
 * 20 times a second (PrismaControl's level meter) TORE across the grab --
 * the top rows from one frame, the rest from the next (2026-10-09). Without
 * the memory for that it reads row by row as before.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/Picasso96.h>
#include <libraries/Picasso96.h>

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *P96Base = NULL;

static LONG args[9];

/* One row of the screen as RGB into out (w * 3 bytes). */
static void read_row(struct Screen *scr, BOOL tc, ULONG *pal, LONG x0, LONG y, LONG w, UBYTE *out)
{
    LONG x;
    ULONG c;

    for (x = 0; x < w; x++)
    {
        if (tc)
        {
            c = p96ReadPixel(&scr->RastPort, (UWORD)(x0 + x), (UWORD)y);
            out[x * 3] = (UBYTE)(c >> 16);
            out[x * 3 + 1] = (UBYTE)(c >> 8);
            out[x * 3 + 2] = (UBYTE)c;
        }
        else
        {
            c = (ULONG)ReadPixel(&scr->RastPort, x0 + x, y);
            if (c >= 256) c = 0;
            out[x * 3] = (UBYTE)(pal[c * 3] >> 24);
            out[x * 3 + 1] = (UBYTE)(pal[c * 3 + 1] >> 24);
            out[x * 3 + 2] = (UBYTE)(pal[c * 3 + 2] >> 24);
        }
    }
}

/* a region of a screen; its palette for a pen screen */
struct GrabArea {
    struct Screen *rg_Scr;
    BOOL rg_TC;
    ULONG *rg_Pal;
    LONG rg_X, rg_Y, rg_W, rg_H;
};

/* A pen screen's region in one call (ReadPixelArray8 through a one-row
 * bitmap, as its autodoc sets one up; each row comes padded to 16 pixels),
 * its pens through the palette. FALSE: no memory for it. */
static BOOL read_pens(const struct GrabArea *rg, UBYTE *rgb)
{
    struct RastPort tmp;
    struct BitMap *bm;
    UBYTE *pens;
    ULONG wide = ((ULONG)rg->rg_W + 15UL) & ~15UL, k, x, y, c;

    if ((pens = (UBYTE *)AllocVec(wide * (ULONG)rg->rg_H, MEMF_ANY)) == NULL) return FALSE;
    bm = AllocBitMap(wide, 1, GetBitMapAttr(rg->rg_Scr->RastPort.BitMap, BMA_DEPTH),
                     GetBitMapAttr(rg->rg_Scr->RastPort.BitMap, BMA_FLAGS) & BMF_INTERLEAVED, NULL);
    if (bm == NULL)
    {
        FreeVec(pens);
        return FALSE;
    }
    InitRastPort(&tmp);
    tmp.BitMap = bm;
    WaitBlit();
    Forbid();
    ReadPixelArray8(&rg->rg_Scr->RastPort, (UWORD)rg->rg_X, (UWORD)rg->rg_Y, (UWORD)(rg->rg_X + rg->rg_W - 1),
                    (UWORD)(rg->rg_Y + rg->rg_H - 1), pens, &tmp);
    Permit();
    for (y = 0, k = 0; y < (ULONG)rg->rg_H; y++)
        for (x = 0; x < (ULONG)rg->rg_W; x++, k += 3)
        {
            c = pens[y * wide + x];
            rgb[k] = (UBYTE)(rg->rg_Pal[c * 3] >> 24);
            rgb[k + 1] = (UBYTE)(rg->rg_Pal[c * 3 + 1] >> 24);
            rgb[k + 2] = (UBYTE)(rg->rg_Pal[c * 3 + 2] >> 24);
        }
    WaitBlit();
    FreeBitMap(bm);
    FreeVec(pens);
    return TRUE;
}

/* the region whole into memory, then into the file */
static BOOL grab_to(const struct GrabArea *rg, STRPTR path)
{
    struct RenderInfo ri;
    BPTR fh;
    UBYTE *whole, *row;
    LONG a[2], y;
    ULONG size = (ULONG)rg->rg_W * (ULONG)rg->rg_H * 3UL;

    if ((fh = Open(path, MODE_NEWFILE)) == 0) return FALSE;
    a[0] = rg->rg_W; a[1] = rg->rg_H;
    VFPrintf(fh, (STRPTR)"P6\n%ld %ld\n255\n", a);
    Flush(fh);
    if ((whole = (UBYTE *)AllocVec(size, MEMF_ANY)) != NULL)
    {
        if (rg->rg_TC)
        {
            /* the board's rectangle in one call: R8G8B8 is a PPM's own */
            ri.Memory = whole;
            ri.BytesPerRow = (WORD)(rg->rg_W * 3);
            ri.pad = 0;
            ri.RGBFormat = RGBFB_R8G8B8;
            p96ReadPixelArray(&ri, 0, 0, &rg->rg_Scr->RastPort, (UWORD)rg->rg_X, (UWORD)rg->rg_Y,
                              (UWORD)rg->rg_W, (UWORD)rg->rg_H);
        }
        else if (!read_pens(rg, whole))
        {
            WaitBlit();
            Forbid();
            for (y = 0; y < rg->rg_H; y++)
                read_row(rg->rg_Scr, FALSE, rg->rg_Pal, rg->rg_X, rg->rg_Y + y, rg->rg_W, whole + (ULONG)y * (ULONG)rg->rg_W * 3UL);
            Permit();
        }
        FWrite(fh, whole, 1, size);
        FreeVec(whole);
    }
    else if ((row = (UBYTE *)AllocVec(rg->rg_W * 3, MEMF_ANY)) != NULL)
    {
        for (y = 0; y < rg->rg_H; y++)
        {
            read_row(rg->rg_Scr, rg->rg_TC, rg->rg_Pal, rg->rg_X, rg->rg_Y + y, rg->rg_W, row);
            FWrite(fh, row, 1, rg->rg_W * 3);
        }
        FreeVec(row);
    }
    Close(fh);
    return TRUE;
}

/* "times.txt" at the end of a name */
static void strcat_times(STRPTR at)
{
    const char *t = "times.txt";

    while ((*at++ = *t++) != 0) ;
}

/* TO<nnn>.ppm */
static void frame_name(STRPTR out, STRPTR prefix, LONG i)
{
    LONG k = 0;

    while (prefix[k] && k < 240) { out[k] = prefix[k]; k++; }
    out[k++] = (char)('0' + (i / 100) % 10);
    out[k++] = (char)('0' + (i / 10) % 10);
    out[k++] = (char)('0' + i % 10);
    out[k++] = '.'; out[k++] = 'p'; out[k++] = 'p'; out[k++] = 'm';
    out[k] = 0;
}

/* TOtimes.txt: a line a frame, the ms after the first grab it began */
static void write_times(STRPTR prefix, LONG *times, LONG n)
{
    char path[256];
    BPTR fh;
    LONG k = 0, i;

    while (prefix[k] && k < 240) { path[k] = prefix[k]; k++; }
    path[k] = 0;
    strcat_times(path + k);
    if ((fh = Open((STRPTR)path, MODE_NEWFILE)) == 0) return;
    for (i = 0; i < n; i++) VFPrintf(fh, (STRPTR)"%ld\n", &times[i]);
    Close(fh);
}

/* the next frame drawn: a message to the port, back once it is */
static BOOL step(STRPTR name, struct MsgPort *reply)
{
    struct Message msg;
    struct MsgPort *port;

    msg.mn_Node.ln_Type = NT_MESSAGE;
    msg.mn_Length = sizeof(msg);
    msg.mn_ReplyPort = reply;
    Forbid();
    if ((port = FindPort(name)) != NULL) PutMsg(port, &msg);
    Permit();
    if (port == NULL) return FALSE;
    WaitPort(reply);
    GetMsg(reply);
    return TRUE;
}

int main(void)
{
    struct RDArgs *rda;
    struct GrabArea rg;
    struct Window *active;
    struct MsgPort *reply = NULL;
    char path[256];
    struct DateStamp first, now;
    LONG ncol, n, i, every;
    LONG *times = NULL;
    LONG rc;

    rc = 20;
    rg.rg_Scr = NULL;
    rg.rg_Pal = NULL;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    P96Base = OpenLibrary((STRPTR)"Picasso96API.library", 2);
    rda = ReadArgs((STRPTR)"X/N,Y/N,W/N,H/N,TO/A,WINDOW/S,STEPS/K/N,PORT/K,EVERY/K/N", args, NULL);
    if (rda == NULL || IntuitionBase == NULL || GfxBase == NULL) goto done;
    if (!args[5] && (!args[0] || !args[1] || !args[2] || !args[3])) goto done;
    rg.rg_Scr = LockPubScreen((STRPTR)"Workbench");
    if (rg.rg_Scr == NULL) goto done;
    if (args[5] && (active = IntuitionBase->ActiveWindow) != NULL)
    {
        rg.rg_X = active->LeftEdge; rg.rg_Y = active->TopEdge;
        rg.rg_W = active->Width; rg.rg_H = active->Height;
    }
    else if (args[5])
    {
        goto done;
    }
    else
    {
        rg.rg_X = *(LONG *)args[0]; rg.rg_Y = *(LONG *)args[1];
        rg.rg_W = *(LONG *)args[2]; rg.rg_H = *(LONG *)args[3];
    }
    if (rg.rg_X < 0) rg.rg_X += rg.rg_Scr->Width;
    if (rg.rg_Y < 0) rg.rg_Y += rg.rg_Scr->Height;
    /* W or H 0: to the screen's edge, so one test grabs any screen whole;
     * and never past it (ReadPixel there reads nothing) */
    if (rg.rg_W <= 0 || rg.rg_X + rg.rg_W > rg.rg_Scr->Width) rg.rg_W = rg.rg_Scr->Width - rg.rg_X;
    if (rg.rg_H <= 0 || rg.rg_Y + rg.rg_H > rg.rg_Scr->Height) rg.rg_H = rg.rg_Scr->Height - rg.rg_Y;
    rg.rg_TC = (BOOL)(P96Base != NULL && p96GetBitMapAttr(rg.rg_Scr->RastPort.BitMap, P96BMA_ISP96) &&
                      p96GetBitMapAttr(rg.rg_Scr->RastPort.BitMap, P96BMA_RGBFORMAT) != RGBFB_CLUT);
    ncol = 1L << GetBitMapAttr(rg.rg_Scr->RastPort.BitMap, BMA_DEPTH);
    if (ncol > 256) ncol = 256;
    if ((rg.rg_Pal = (ULONG *)AllocVec(256 * 3 * 4, MEMF_ANY | MEMF_CLEAR)) == NULL) goto done;
    if (!rg.rg_TC) GetRGB32(rg.rg_Scr->ViewPort.ColorMap, 0, ncol, rg.rg_Pal);
    if (!args[6])
    {
        if (grab_to(&rg, (STRPTR)args[4])) rc = 0;
        goto done;
    }
    /* an animation: (a frame drawn,) a grab, n times; when each began */
    n = *(LONG *)args[6];
    every = args[8] ? *(LONG *)args[8] : 0;
    if (n <= 0 || n > 999) goto done;
    if (args[7] && (reply = CreateMsgPort()) == NULL) goto done;
    if ((times = (LONG *)AllocVec((ULONG)n * sizeof(LONG), MEMF_ANY)) == NULL) goto done;
    DateStamp(&first);
    for (i = 0; i < n; i++)
    {
        if (reply != NULL) { if (!step((STRPTR)args[7], reply)) goto done; }
        else if (i > 0 && every > 0) Delay(every);
        DateStamp(&now);
        times[i] = ((now.ds_Days - first.ds_Days) * 1440L + now.ds_Minute - first.ds_Minute) * 60000L +
                   (now.ds_Tick - first.ds_Tick) * 20L;
        frame_name(path, (STRPTR)args[4], i);
        if (!grab_to(&rg, (STRPTR)path)) goto done;
    }
    write_times((STRPTR)args[4], times, n);
    rc = 0;
done:
    FreeVec(times);
    if (reply != NULL) DeleteMsgPort(reply);
    FreeVec(rg.rg_Pal);
    if (rg.rg_Scr != NULL) UnlockPubScreen(NULL, rg.rg_Scr);
    if (rda != NULL) FreeArgs(rda);
    CloseLibrary(P96Base);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return (int)rc;
}
