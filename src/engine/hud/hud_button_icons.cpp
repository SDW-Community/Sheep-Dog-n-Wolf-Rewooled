/*
 * What each button looks like on the HUD (hud_button_icons.h). One entry per binding and controller family; an
 * INPUT_FAMILY_ANY entry is the fallback for a family that has none. A missing PNG falls back the same way, so the
 * PNGs can be added one at a time: put them in the game's data folder at the path given (any case).
 *
 * Controller buttons are named by POSITION (INPUT_PAD_SOUTH...): the same place on every pad, with each family's own
 * picture - south is Cross on a PlayStation pad, A on an Xbox pad, B on a Nintendo pad.
 *
 * A text icon's symbols must be in the HUD font's s_symbols (hud_font.cpp).
 */
#include "sdw_types.h"
#include "sdw_enums.h"
#include "../../sdk/dinput.h"
#include "../../platform/platform.h"
#include "hud_button_icons.h"

#define PS INPUT_FAMILY_PLAYSTATION
#define XBOX INPUT_FAMILY_XBOX
#define NINTENDO INPUT_FAMILY_NINTENDO

static const HudButtonIcon s_buttonIcons[] = {
    /* ---- the keyboard: keys whose own name is long (the others show their name) ---- */
    {INPUT_KEY(DIK_UP), HUD_TEXT("↑")},
    {INPUT_KEY(DIK_DOWN), HUD_TEXT("↓")},
    {INPUT_KEY(DIK_LEFT), HUD_TEXT("←")},
    {INPUT_KEY(DIK_RIGHT), HUD_TEXT("→")},
    {INPUT_KEY(DIK_RETURN), HUD_TEXT("↵")},
    {INPUT_KEY(DIK_BACK), HUD_TEXT("⌫")},
    {INPUT_KEY(DIK_LSHIFT), HUD_TEXT("⇧")},
    {INPUT_KEY(DIK_RSHIFT), HUD_TEXT("⇧")},
    {INPUT_KEY(DIK_LCONTROL), HUD_TEXT("CTRL")},
    {INPUT_KEY(DIK_RCONTROL), HUD_TEXT("CTRL")},
    {INPUT_KEY(DIK_LMENU), HUD_TEXT("ALT")},
    {INPUT_KEY(DIK_RMENU), HUD_TEXT("ALT")},
    {INPUT_KEY(DIK_ESCAPE), HUD_TEXT("ESC")},
    {INPUT_KEY(DIK_TAB), HUD_TEXT("TAB")},
    {INPUT_KEY(DIK_SPACE), HUD_TEXT("SPACE")},

    /* ---- the mouse ---- */
    {INPUT_MOUSE(0), HUD_TEXT("LMB")},
    {INPUT_MOUSE(1), HUD_TEXT("RMB")},
    {INPUT_MOUSE(2), HUD_TEXT("MMB")},
    {INPUT_MOUSE(3), HUD_TEXT("X1")},

    /* ---- PlayStation ---- */
    {INPUT_PAD_ON(PS, INPUT_PAD_SOUTH), HUD_IMAGE("hud/icons/playstation/cross.png")},
    {INPUT_PAD_ON(PS, INPUT_PAD_EAST), HUD_IMAGE("hud/icons/playstation/circle.png")},
    {INPUT_PAD_ON(PS, INPUT_PAD_WEST), HUD_IMAGE("hud/icons/playstation/square.png")},
    {INPUT_PAD_ON(PS, INPUT_PAD_NORTH), HUD_IMAGE("hud/icons/playstation/triangle.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_L1), HUD_IMAGE("hud/icons/playstation/l1.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_R1), HUD_IMAGE("hud/icons/playstation/r1.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_L2), HUD_IMAGE("hud/icons/playstation/l2.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_R2), HUD_IMAGE("hud/icons/playstation/r2.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_SELECT), HUD_IMAGE("hud/icons/playstation/share.png")},
    {INPUT_PAD_ON(PS, INPUT_BIND_START), HUD_IMAGE("hud/icons/playstation/options.png")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_UP), HUD_IMAGE("hud/icons/playstation/dpad_up.png")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_DOWN), HUD_IMAGE("hud/icons/playstation/dpad_down.png")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_LEFT), HUD_IMAGE("hud/icons/playstation/dpad_left.png")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_RIGHT), HUD_IMAGE("hud/icons/playstation/dpad_right.png")},

    /* ---- Xbox ---- */
    {INPUT_PAD_ON(XBOX, INPUT_PAD_SOUTH), HUD_IMAGE("hud/icons/xbox/a.png")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_EAST), HUD_IMAGE("hud/icons/xbox/b.png")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_WEST), HUD_IMAGE("hud/icons/xbox/x.png")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_NORTH), HUD_IMAGE("hud/icons/xbox/y.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_L1), HUD_IMAGE("hud/icons/xbox/lb.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_R1), HUD_IMAGE("hud/icons/xbox/rb.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_L2), HUD_IMAGE("hud/icons/xbox/lt.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_R2), HUD_IMAGE("hud/icons/xbox/rt.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_SELECT), HUD_IMAGE("hud/icons/xbox/view.png")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_START), HUD_IMAGE("hud/icons/xbox/menu.png")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_UP), HUD_IMAGE("hud/icons/xbox/dpad_up.png")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_DOWN), HUD_IMAGE("hud/icons/xbox/dpad_down.png")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_LEFT), HUD_IMAGE("hud/icons/xbox/dpad_left.png")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_RIGHT), HUD_IMAGE("hud/icons/xbox/dpad_right.png")},

    /* ---- Nintendo ---- */
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_SOUTH), HUD_IMAGE("hud/icons/nintendo/b.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_EAST), HUD_IMAGE("hud/icons/nintendo/a.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_WEST), HUD_IMAGE("hud/icons/nintendo/y.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_NORTH), HUD_IMAGE("hud/icons/nintendo/x.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_L1), HUD_IMAGE("hud/icons/nintendo/l.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_R1), HUD_IMAGE("hud/icons/nintendo/r.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_L2), HUD_IMAGE("hud/icons/nintendo/zl.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_R2), HUD_IMAGE("hud/icons/nintendo/zr.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_SELECT), HUD_IMAGE("hud/icons/nintendo/minus.png")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_START), HUD_IMAGE("hud/icons/nintendo/plus.png")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_UP), HUD_IMAGE("hud/icons/nintendo/dpad_up.png")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_DOWN), HUD_IMAGE("hud/icons/nintendo/dpad_down.png")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_LEFT), HUD_IMAGE("hud/icons/nintendo/dpad_left.png")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_RIGHT), HUD_IMAGE("hud/icons/nintendo/dpad_right.png")},

    /* ---- any controller (and a family whose PNG is missing): the D-pad as an arrow ---- */
    {INPUT_DPAD(PLATFORM_DPAD_UP), HUD_TEXT("D↑")},
    {INPUT_DPAD(PLATFORM_DPAD_DOWN), HUD_TEXT("D↓")},
    {INPUT_DPAD(PLATFORM_DPAD_LEFT), HUD_TEXT("D←")},
    {INPUT_DPAD(PLATFORM_DPAD_RIGHT), HUD_TEXT("D→")},
};

#define HUDBUTTONICONS_COUNT (sizeof(s_buttonIcons) / sizeof(s_buttonIcons[0]))

const HudIcon *HudButtonIcons_Find(const InputBindingRef *binding, u8 family)
{
    const HudIcon *any = 0;
    u32 i;
    for (i = 0; i < HUDBUTTONICONS_COUNT; i++) {
        const HudButtonIcon *e = &s_buttonIcons[i];
        if (e->binding.source != binding->source || e->binding.code != binding->code ||
            !HudIcons_IsReady(&e->icon))
            continue;
        if (e->binding.family == family && family != INPUT_FAMILY_ANY)
            return &e->icon;
        if (e->binding.family == INPUT_FAMILY_ANY && !any)
            any = &e->icon;
    }
    return any;
}

u32 HudButtonIcons_Count()
{
    return HUDBUTTONICONS_COUNT;
}

const HudIcon *HudButtonIcons_Icon(u32 index)
{
    return index < HUDBUTTONICONS_COUNT ? &s_buttonIcons[index].icon : 0;
}
