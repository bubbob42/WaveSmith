#ifndef WAVEGFX_GFX_H
#define WAVEGFX_GFX_H
#define FB_W 640
#define FB_H 256
struct GfxClip { long x0, y0, x1, y1; };
extern unsigned char fb[FB_H][FB_W];
extern unsigned long palette[256];
extern long gfx_out_of_box, gfx_inverted, gfx_rects, gfx_texts;
extern long gfx_vecs, gfx_pens, gfx_objs, gfx_libs, gfx_bitmaps, gfx_blits;
extern long gfx_winrects, gfx_winblits;
extern int gfx_standard;		/* GetBitMapAttr says BMF_STANDARD: planar in chip RAM */
extern long gfx_templates, gfx_chipread;	/* BltTemplate()s; reads outside a MEMF_CHIP block */
extern unsigned long gfx_pointer;
extern struct GfxClip gfx_clip;
void gfx_reset(void);
void gfx_dump(const char *path, int w, int h);
#endif
