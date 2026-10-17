/*
 * The new HUD's one way out to the screen and the disk (hud_draw.h): the only file under src/engine/hud/ that uses the
 * old engine.
 *
 * HUD units to screen pixels: Screen::ScaleX / ScaleY, which apply the HudElement anchoring in force. They take whole
 * units, too coarse for glyphs a few units wide, so the mapping is sampled at two points and used as the linear
 * function it is. The rectangle is queued in the PolyBatcher as two textured triangles (Draw2D_TexRect).
 *
 * The game's data folder: the one g_dirReference ("<data>/References/") is in, wherever --game-dir put it.
 */
#include "sdw_types.h"
#include "sdw_enums.h"
#include "sdw_classes.h"

#include "../../sdk/crt.h"
#include "../screen.h"
#include "../fixed_math.h"
#include "../draw2d.h"
#include "../../platform/platform.h"
#include "hud_draw.h"

void Draw2D_TexRect(float z, float x0, float y0, float x1, float y1, s32 texIndex, float uTL, float vTL, u32 cTL,
                    float uBL, float vBL, u32 cBL, float uTR, float vTR, u32 cTR, float uBR, float vBR,
                    u32 cBR); /* draw2d.cpp */

/* the HUD-to-screen mapping in force: screen = origin + unit * scale */
static void HudDraw_Mapping(float *ox, float *sx, float *oy, float *sy)
{
    *ox = g_screen.ScaleX(0);
    *sx = (g_screen.ScaleX(512) - *ox) / 512.0f;
    *oy = g_screen.ScaleY(0);
    *sy = (g_screen.ScaleY(240) - *oy) / 240.0f;
}

void HudDraw_TexturedRect(s32 page, const HudRect *rect, const HudRect *uv, u32 rgb, u32 *layer)
{
    float ox, sx, oy, sy;
    u32 c = Color_RgbToBgr(rgb);
    if (page < 0)
        return;
    HudDraw_Mapping(&ox, &sx, &oy, &sy);
    Draw2D_TexRect(g_screen.Draw2D_LayerToZ(layer), ox + rect->x0 * sx, oy + rect->y0 * sy, ox + rect->x1 * sx,
                   oy + rect->y1 * sy, page, uv->x0, uv->y0, c, uv->x0, uv->y1, c, uv->x1, uv->y0, c, uv->x1, uv->y1,
                   c);
}

void HudDraw_Image(s32 page, float x, float y, float height, s32 texW, s32 texH, const HudRect *uv, u32 *layer)
{
    float ox, sx, oy, sy, w, h;
    if (page < 0 || texW <= 0 || texH <= 0)
        return;
    HudDraw_Mapping(&ox, &sx, &oy, &sy);
    /* in pixels: `height` units tall, square texels */
    h = height * sy;
    w = h * texW / texH;
    x = ox + x * sx;
    y = oy + y * sy;
    /* the batcher's textured pages are MODULATE2X (texture x colour x 2): 0x808080 is the image's own colours, as for
     * the game's sprites (white would double them, and clip the light ones together) */
    Draw2D_TexRect(g_screen.Draw2D_LayerToZ(layer), x - w / 2, y - h / 2, x + w / 2, y + h / 2, page, uv->x0, uv->y0,
                   0x808080, uv->x0, uv->y1, 0x808080, uv->x1, uv->y0, 0x808080, uv->x1, uv->y1, 0x808080);
}

u8 *HudDraw_LoadImage(const char *relativePath, s32 *width, s32 *height)
{
    char path[SDW_PATH_MAX];
    int w = 0, h = 0;
    size_t n = strlen(g_dirReference);
    u8 *rgba;
    /* "<data>/References/" -> "<data>/" */
    if (n && (g_dirReference[n - 1] == '/' || g_dirReference[n - 1] == '\\'))
        n--;
    while (n && g_dirReference[n - 1] != '/' && g_dirReference[n - 1] != '\\')
        n--;
    snprintf(path, sizeof(path), "%.*s%s", (int)n, g_dirReference, relativePath);
    if (!(rgba = Platform_LoadImageRGBA(path, &w, &h)))
        return 0;
    *width = w;
    *height = h;
    return rgba;
}

u8 *HudDraw_DecodeImage(const void *png, u32 size, s32 *width, s32 *height)
{
    int w = 0, h = 0;
    u8 *rgba = Platform_DecodeImageRGBA(png, (int)size, &w, &h);
    if (!rgba)
        return 0;
    *width = w;
    *height = h;
    return rgba;
}

void HudDraw_FreeImage(u8 *rgba)
{
    Platform_FreeImage(rgba);
}
