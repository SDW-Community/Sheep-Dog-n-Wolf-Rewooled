/*
 * The controller's own button icons in the prompts. The text code turns $B_...$ into glyphs of the game font, whose
 * PlayStation button glyphs come from the .DAV (Text_ExpandButtonToken); Text_DrawString asks ButtonIcons_ForGlyph
 * before drawing one and, when it answers, draws the icon of the button the controller in use has there instead. The
 * lookup is made at every draw, so the prompts follow the device in use and the controller plugged in.
 *
 * The sheet, res/controller.png (embedded by CMakeLists.txt as g_sdwControllerPng): 15 x 15 icons on a 16-pixel grid
 * from (9, 4), one controller family a row, the four face buttons in columns by POSITION (west, north, east, south),
 * as the fixed controller mapping (platform_sdl.cpp, s_padMapping) is by position. The rest of the image (the rows'
 * names) is not read. Only the face buttons have icons for now: the shoulders, triggers, Select, Start, the d-pad and
 * the keyboard keep the font's glyphs.
 *
 * Used only with the new controls (not g_legacyControls) and while the controller is the device in use.
 */
#include "sdw_types.h"
#include "sdw_enums.h"
#include "sdw_classes.h"
#include "../sdk/crt.h"

#include "draw2d.h"
#include "input.h"
#include "button_icons.h"
#include "../platform/platform.h"

#ifdef SDW_HAVE_CONTROLLER_ICONS
#include "sdw_controller_png.h" /* g_sdwControllerPng[] (generated in the build folder) */
#endif

/* the sheet's grid */
#define ICON_SHEET_X0 9
#define ICON_SHEET_Y0 4
#define ICON_SHEET_PITCH 16
#define ICON_SIZE 15
#define ICON_PAGE_SIZE 256

/* its rows */
enum {
    ICON_ROW_DUALSHOCK = 0,
    ICON_ROW_DUALSENSE_LIGHT = 1,
    ICON_ROW_DUALSENSE_DARK = 2,
    ICON_ROW_NINTENDO = 3,
    ICON_ROW_GAMECUBE = 4,
    ICON_ROW_JOYCON = 5,
    ICON_ROW_DREAMCAST = 6,
    ICON_ROW_XBOX = 7,
    ICON_ROW_XBOX_ORIGINAL = 8,
    ICON_ROW_XBOX_360 = 9,
    ICON_ROW_XBOX_ONE = 10,
};

/* the blink (Text_BlinkButtonGlyphs) dims the icon for its reverse third, as the font's reverse glyphs are */
#define ICON_RGB 0xffffff
#define ICON_RGB_BLINK 0xa0a0a0

s32 g_buttonIconPage = -1;

u8 *ButtonIcons_BuildPage(u32 *width, u32 *height)
{
#ifdef SDW_HAVE_CONTROLLER_ICONS
    int w = 0, h = 0, row;
    unsigned char *sheet = Platform_DecodeImageRGBA(g_sdwControllerPng, (int)sizeof(g_sdwControllerPng), &w, &h);
    u8 *page;
    if (!sheet)
        return 0;
    if (w > ICON_PAGE_SIZE || h > ICON_PAGE_SIZE) {
        Platform_Log("controller icons: the sheet is %dx%d, more than one %d-texel page: not used", w, h,
                     ICON_PAGE_SIZE);
        Platform_FreeImage(sheet);
        return 0;
    }
    page = new u8[ICON_PAGE_SIZE * ICON_PAGE_SIZE * 4];
    memset(page, 0, ICON_PAGE_SIZE * ICON_PAGE_SIZE * 4); /* transparent around the sheet */
    for (row = 0; row < h; row++)
        memcpy(page + row * ICON_PAGE_SIZE * 4, sheet + row * w * 4, w * 4);
    Platform_FreeImage(sheet);
    *width = *height = ICON_PAGE_SIZE;
    return page;
#else
    (void)width;
    (void)height;
    return 0;
#endif
}

/* the sheet row of the open controller's family */
static s32 ButtonIcons_Row(s32 family)
{
    switch (family) {
        case PLATFORM_PAD_PS3:
        case PLATFORM_PAD_PS4:
            return ICON_ROW_DUALSHOCK;
        case PLATFORM_PAD_PS5:
            return ICON_ROW_DUALSENSE_DARK;
        case PLATFORM_PAD_SWITCH_PRO:
        case PLATFORM_PAD_JOYCON_PAIR:
            return ICON_ROW_NINTENDO;
        case PLATFORM_PAD_JOYCON_SINGLE:
            return ICON_ROW_JOYCON;
        case PLATFORM_PAD_GAMECUBE:
            return ICON_ROW_GAMECUBE;
        case PLATFORM_PAD_XBOX360:
            return ICON_ROW_XBOX_360;
        case PLATFORM_PAD_XBOXONE:
            return ICON_ROW_XBOX_ONE;
        default:
            return ICON_ROW_XBOX;
    }
}

/* the PlayStation button of a face-button glyph (Text_ExpandButtonToken: 0x5c square, 0x5d triangle, 0x5e circle,
 * 0x5f cross) or of its blink variants (0x7c.. forward, 0x9c.. reverse: s_glyphVariantFwdTable / RevTable, which map
 * no other character there): the INPUT_BIND_* slot, or -1; *blink is set for the reverse variant */
static s32 ButtonIcons_FaceSlot(u8 glyph, u8 *blink)
{
    static const s32 slots[4] = {INPUT_BIND_SQUARE, INPUT_BIND_TRIANGLE, INPUT_BIND_CIRCLE, INPUT_BIND_CROSS};
    *blink = 0;
    if (glyph >= 0x5c && glyph <= 0x5f)
        return slots[glyph - 0x5c];
    if (glyph >= 0x7c && glyph <= 0x7f)
        return slots[glyph - 0x7c];
    if (glyph >= 0x9c && glyph <= 0x9f) {
        *blink = 1;
        return slots[glyph - 0x9c];
    }
    return -1;
}

s32 ButtonIcons_ForGlyph(u8 glyph, ButtonIcon *out)
{
    u8 blink;
    s32 slot, face, family;
    if (g_buttonIconPage < 0 || g_legacyControls)
        return 0;
    if ((slot = ButtonIcons_FaceSlot(glyph, &blink)) < 0)
        return 0;
    if (!g_inputMgr.joystick || g_inputMgr.current != (InputDevice *)g_inputMgr.joystick)
        return 0;
    if ((family = Platform_JoystickFamily()) == PLATFORM_PAD_NONE)
        return 0;
    if ((face = Platform_JoystickFaceButton(slot)) < 0)
        return 0;
    out->page = (u32)g_buttonIconPage;
    out->u = ICON_SHEET_X0 + face * ICON_SHEET_PITCH;
    out->v = ICON_SHEET_Y0 + ButtonIcons_Row(family) * ICON_SHEET_PITCH;
    out->w = out->h = ICON_SIZE;
    out->rgb = blink ? ICON_RGB_BLINK : ICON_RGB;
    return 1;
}
