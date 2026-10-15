/* The HUD font (hud_font.cpp): a TrueType font (DejaVu Sans Bold, embedded by CMakeLists.txt) for the new HUD, with
 * the symbols the game's bitmap fonts lack (arrows, return, shift, backspace). Rasterised once at start-up into an
 * atlas that every level's PolyBatcher gets as a texture page (g_hudFontPage). Text is UTF-8. */
#ifndef SDW_HUD_FONT_H
#define SDW_HUD_FONT_H

#include "sdw_types.h"

/* the batcher's page holding the atlas, set by PolyBatcher::LoadTexturePages; -1 when there is none */
extern s32 g_hudFontPage;

/* the atlas as RGBA (white, the alpha the coverage, the usual opacity), new[]'d, rasterising it the first time; 0
 * when the build has no font */
u8 *HudFont_BuildPage(u32 *width, u32 *height);

enum { HUDFONT_LEFT, HUDFONT_CENTRE, HUDFONT_RIGHT };

/* the width of a string at `size` (its line height, in HUD units: the 512 x 240 virtual screen) */
float HudFont_Measure(const char *utf8, float size);

/* queues a string: (x, y) is its left edge / centre / right edge (align) and its vertical centre, in HUD units,
 * through the HudElement anchoring in force; colour 0xRRGGBB; layer g_screenLayerBase + n (a smaller n in front).
 * 0 when nothing can be drawn (no font page in this level) */
u8 HudFont_Draw(const char *utf8, float x, float y, float size, u32 rgb, u32 *layer, u8 align);

#endif
