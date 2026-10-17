/* HUD icons (hud_icons.cpp): a piece of text in the HUD font, or a PNG from the game's data folder. The PNGs the
 * button icon table names (hud_button_icons.cpp) are loaded and packed into one atlas at every level load, which the
 * level's PolyBatcher gets as a texture page (g_hudIconPage); so a PNG added or changed shows from the next level on.
 * A PNG that is missing or does not fit is not ready, and the caller falls back to something else. */
#ifndef SDW_HUD_ICONS_H
#define SDW_HUD_ICONS_H

#include "sdw_types.h"

enum HudIconKind { HUDICON_NONE, HUDICON_TEXT, HUDICON_IMAGE, HUDICON_ANIM };

/* the frames of an animated image: <path>_1.png .. <path>_3.png, shown in step with the text's button-glyph blink
 * (Text_BlinkFrame) when drawn animated, else the first */
#define HUDICON_FRAMES 3

struct HudIcon {
    u8 kind;           /* HudIconKind */
    const char *value; /* the text (UTF-8), the PNG's path in the game's data folder, or an animation's path without
                        * its "_<frame>.png" */
};

/* icons written as they read, for tables: HUD_TEXT("←"), HUD_IMAGE("hud/icons/xbox/a.png"),
 * HUD_ANIM("hud/icons/xbox/a") for hud/icons/xbox/a_1.png .. a_3.png */
#define HUD_TEXT(text) {HUDICON_TEXT, text}
#define HUD_IMAGE(path) {HUDICON_IMAGE, path}
#define HUD_ANIM(path) {HUDICON_ANIM, path}

/* the batcher's page holding the PNGs, set by PolyBatcher::LoadTexturePages; -1 when there is none */
extern s32 g_hudIconPage;

/* (re)loads the table's PNGs and packs them: the atlas as RGBA (the usual opacity), new[]'d; 0 when there are none */
u8 *HudIcons_BuildPage(u32 *width, u32 *height);

/* the icon can be drawn: text always; an image when it was loaded into this level's atlas, an animation when its
 * first frame was (a missing later frame shows the first) */
u8 HudIcons_IsReady(const HudIcon *icon);

/* draws the icon centred at (x, y), `size` HUD units tall, on `layer`; an image keeps its proportions on the screen
 * (HudDraw_Image) and is filtered as the game's text is. A text icon is
 * in colour `rgb`, an image in its own colours; an animation shows its frame of the blink when `animate`, else its
 * first. 1 when it was drawn */
u8 HudIcons_Draw(const HudIcon *icon, float x, float y, float size, u32 rgb, u32 *layer, u8 animate = 0);

#endif
