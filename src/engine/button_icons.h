/* The controller's own button icons in the prompts, in place of the PlayStation glyphs of the .DAV font
 * (button_icons.cpp). */
#ifndef SDW_BUTTON_ICONS_H
#define SDW_BUTTON_ICONS_H

#include "sdw_types.h"

/* one icon to draw: its texture page in the PolyBatcher, its rectangle there in texels of a 256-texel page, and the
 * colour to modulate it with (0xRRGGBB) */
struct ButtonIcon {
    u32 page;
    s32 u, v, w, h;
    u32 rgb;
};

/* the batcher's page holding the icons, set by PolyBatcher::LoadTexturePages; -1 when there is none */
extern s32 g_buttonIconPage;

/* the icon sheet on a 256 x 256 RGBA page (bytes R, G, B, A, the usual opacity), new[]'d; 0 when the build has no
 * sheet or it does not fit */
u8 *ButtonIcons_BuildPage(u32 *width, u32 *height);

/* the icon for game-font glyph `glyph` when it is a PlayStation button glyph (or one of its blink variants) and the
 * controller in use has an icon for that button: 1, with *out filled; 0 to draw the font's glyph */
s32 ButtonIcons_ForGlyph(u8 glyph, ButtonIcon *out);

#endif
