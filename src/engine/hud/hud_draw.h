/* The new HUD's one way out to the screen and the disk (hud_draw.cpp). Everything under src/engine/hud/ draws and
 * loads through this and nothing else of the old engine: the coordinate mapping, the polygon batcher and the game's
 * data folder stay behind these functions, so a new renderer only has to change hud_draw.cpp.
 *
 * Coordinates are HUD units: the 512 x 240 virtual screen the game's 2D code uses, through the HudElement anchoring in
 * force (a caller inside a HudElement scope keeps its alignment, also in widescreen). Layers are the game's draw
 * layers (g_screenLayerBase + n): a smaller n is in front. */
#ifndef SDW_HUD_DRAW_H
#define SDW_HUD_DRAW_H

#include "sdw_types.h"

struct HudRect {
    float x0, y0, x1, y1;
};

/* a rectangle of texture page `page` (u, v in 0..1 of that page) at `rect`, modulated by 0xRRGGBB; queued, drawn at
 * the end of the frame */
void HudDraw_TexturedRect(s32 page, const HudRect *rect, const HudRect *uv, u32 rgb, u32 *layer);

/* a PNG from the game's data folder (path relative to it, e.g. "hud/icons/xbox/a.png"; any case) as RGBA (the usual
 * opacity), or 0 when there is no such image. Free it with HudDraw_FreeImage */
u8 *HudDraw_LoadImage(const char *relativePath, s32 *width, s32 *height);
/* the same from a PNG in memory (an image embedded in the program) */
u8 *HudDraw_DecodeImage(const void *png, u32 size, s32 *width, s32 *height);
void HudDraw_FreeImage(u8 *rgba);

#endif
