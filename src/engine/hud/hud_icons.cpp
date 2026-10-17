/*
 * HUD icons (hud_icons.h): text drawn with the HUD font, or a PNG from the game's data folder.
 *
 * Where a PNG comes from: the game's data folder at the icon's path when there is one there (an override: a modder's
 * icon, or one being drawn, without rebuilding), else the copy embedded in the program from res/hud/icons/ at the
 * same path (CMakeLists.txt, g_sdwHudIcons).
 *
 * The atlas: at every level load (HudIcons_BuildPage, from PolyBatcher::LoadTexturePages) the PNGs the button icon
 * table names are read again (so an override added or changed shows from the next level on), each path once, and
 * packed in rows ("shelves", the tallest first) into one 1024 x 1024 page with a 2-pixel gap. A PNG that is missing,
 * or does not fit what is left, is left out: it is not ready, and the prompt falls back.
 *
 * An animation (HUD_ANIM) is HUDICON_FRAMES PNGs, a cell each; the frame drawn is the text's button-glyph blink
 * (Text_BlinkFrame), so the icons and the old glyphs change together; drawn still, it is the first frame.
 *
 * Uses the HUD font and hud_draw (the screen and the disk); nothing of the old engine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdw_types.h"
#include "sdw_hud_icons.h" /* g_sdwHudIcons[]: the PNGs of res/hud/icons/ (generated in the build folder) */
#include "../text.h"
#include "hud_draw.h"
#include "hud_font.h"
#include "hud_button_icons.h"
#include "hud_icons.h"

#define HUDICONS_ATLAS 1024
#define HUDICONS_GAP 2
#define HUDICONS_MAX 192
#define HUDICONS_PATH 96

s32 g_hudIconPage = -1;

/* the images in this level's atlas */
static struct HudIconCell {
    char path[HUDICONS_PATH];
    s32 w, h; /* in pixels */
    HudRect uv;
} s_cells[HUDICONS_MAX];
static u32 s_cellCount;

static const HudIconCell *HudIcons_Cell(const char *path)
{
    u32 i;
    for (i = 0; i < s_cellCount; i++)
        if (!strcmp(s_cells[i].path, path))
            return &s_cells[i];
    return 0;
}

/* the PNG of an image icon, or of an animation's frame (0 = the first) */
static const char *HudIcons_Path(const HudIcon *icon, u32 frame, char *buf, int size)
{
    if (icon->kind != HUDICON_ANIM)
        return icon->value;
    snprintf(buf, size, "%s_%u.png", icon->value, (unsigned)frame + 1);
    return buf;
}

/* an icon's PNG: the data folder's override, else the embedded copy; 0 when neither exists */
static u8 *HudIcons_LoadPng(const char *path, s32 *w, s32 *h)
{
    u32 i;
    u8 *rgba = HudDraw_LoadImage(path, w, h);
    if (rgba)
        return rgba;
    for (i = 0; g_sdwHudIcons[i].path; i++)
        if (!strcmp(g_sdwHudIcons[i].path, path))
            return HudDraw_DecodeImage(g_sdwHudIcons[i].png, g_sdwHudIcons[i].size, w, h);
    return 0;
}

/* tallest first, so each shelf wastes little */
static int HudIcons_CompareHeight(const void *a, const void *b)
{
    return ((const HudIconCell *)b)->h - ((const HudIconCell *)a)->h;
}

u8 *HudIcons_BuildPage(u32 *width, u32 *height)
{
    u8 *atlas, *rgba;
    u32 i, n = 0, count = HudButtonIcons_Count();
    s32 x = 0, y = 0, shelf = 0, row;
    s_cellCount = 0;
    /* each PNG the table names (each frame of an animation), once */
    for (i = 0; i < count; i++) {
        const HudIcon *icon = HudButtonIcons_Icon(i);
        u32 f, k;
        if (icon->kind != HUDICON_IMAGE && icon->kind != HUDICON_ANIM)
            continue;
        for (f = 0; f < (icon->kind == HUDICON_ANIM ? HUDICON_FRAMES : 1u) && n < HUDICONS_MAX; f++) {
            char buf[HUDICONS_PATH];
            const char *path = HudIcons_Path(icon, f, buf, sizeof(buf));
            for (k = 0; k < n && strcmp(s_cells[k].path, path); k++)
                ;
            if (k < n)
                continue;
            snprintf(s_cells[n].path, sizeof(s_cells[n].path), "%s", path);
            n++;
        }
    }
    for (i = 0; i < n; i++) { /* their sizes first, to pack the tallest first; the pixels are read again below */
        s32 w = 0, h = 0;
        rgba = HudIcons_LoadPng(s_cells[i].path, &w, &h);
        s_cells[i].w = rgba ? w : 0;
        s_cells[i].h = rgba ? h : 0;
        if (rgba)
            HudDraw_FreeImage(rgba);
    }
    qsort(s_cells, n, sizeof(s_cells[0]), HudIcons_CompareHeight);
    atlas = 0;
    for (i = 0; i < n; i++) {
        s32 w, h;
        if (!s_cells[i].w)
            break; /* the missing ones sorted last */
        if (x + s_cells[i].w > HUDICONS_ATLAS) { /* the next shelf */
            x = 0;
            y += shelf + HUDICONS_GAP;
            shelf = 0;
        }
        if (s_cells[i].w > HUDICONS_ATLAS || y + s_cells[i].h > HUDICONS_ATLAS)
            break; /* the atlas is full: the rest are left out */
        if (!(rgba = HudIcons_LoadPng(s_cells[i].path, &w, &h)))
            break;
        if (!atlas) {
            atlas = new u8[HUDICONS_ATLAS * HUDICONS_ATLAS * 4];
            memset(atlas, 0, HUDICONS_ATLAS * HUDICONS_ATLAS * 4);
        }
        for (row = 0; row < h; row++)
            memcpy(atlas + ((y + row) * HUDICONS_ATLAS + x) * 4, rgba + row * w * 4, w * 4);
        HudDraw_FreeImage(rgba);
        s_cells[i].uv.x0 = (float)x / HUDICONS_ATLAS;
        s_cells[i].uv.y0 = (float)y / HUDICONS_ATLAS;
        s_cells[i].uv.x1 = (float)(x + w) / HUDICONS_ATLAS;
        s_cells[i].uv.y1 = (float)(y + h) / HUDICONS_ATLAS;
        x += w + HUDICONS_GAP;
        if (h > shelf)
            shelf = h;
        s_cellCount = i + 1;
    }
    if (!atlas)
        return 0;
    *width = *height = HUDICONS_ATLAS;
    return atlas;
}

u8 HudIcons_IsReady(const HudIcon *icon)
{
    if (!icon || icon->kind == HUDICON_NONE)
        return 0;
    if (icon->kind == HUDICON_TEXT)
        return 1;
    char buf[HUDICONS_PATH];
    return g_hudIconPage >= 0 && HudIcons_Cell(HudIcons_Path(icon, 0, buf, sizeof(buf))) != 0;
}

u8 HudIcons_Draw(const HudIcon *icon, float x, float y, float size, u32 rgb, u32 *layer, u8 animate)
{
    const HudIconCell *cell = 0;
    char buf[HUDICONS_PATH];
    if (!HudIcons_IsReady(icon))
        return 0;
    if (icon->kind == HUDICON_TEXT)
        return HudFont_Draw(icon->value, x, y, size, rgb, layer, HUDFONT_CENTRE);
    if (icon->kind == HUDICON_ANIM && animate)
        cell = HudIcons_Cell(HudIcons_Path(icon, Text_BlinkFrame(), buf, sizeof(buf)));
    if (!cell) /* a still image, an animation drawn still, or a frame that is missing: the first */
        cell = HudIcons_Cell(HudIcons_Path(icon, 0, buf, sizeof(buf)));
    HudDraw_Image(g_hudIconPage, x, y, size, cell->w, cell->h, &cell->uv, layer);
    return 1;
}
