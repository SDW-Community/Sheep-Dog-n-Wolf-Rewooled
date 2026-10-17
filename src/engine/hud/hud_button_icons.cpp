/*
 * What each button looks like on the HUD (hud_button_icons.h). One entry per binding and controller family; an
 * INPUT_FAMILY_ANY entry is the fallback for a family that has none. A missing PNG falls back the same way, so the
 * PNGs can be added one at a time: put them in the game's data folder at the path given (any case). The pads'
 * icons are animated: three PNGs each, <path>_1.png .. <path>_3.png (hud_icons.h, HUD_ANIM).
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
    {INPUT_KEY(DIK_A), HUD_ANIM("hud/icons/keyboard/a")},
    {INPUT_KEY(DIK_D), HUD_ANIM("hud/icons/keyboard/d")},
    {INPUT_KEY(DIK_S), HUD_ANIM("hud/icons/keyboard/s")},
    {INPUT_KEY(DIK_W), HUD_ANIM("hud/icons/keyboard/w")},
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
    {INPUT_MOUSE(0), HUD_ANIM("hud/icons/keyboard/lmb")},
    {INPUT_MOUSE(1), HUD_ANIM("hud/icons/keyboard/rmb")},
    {INPUT_MOUSE(2), HUD_TEXT("MMB")},
    {INPUT_MOUSE(3), HUD_TEXT("X1")},

    /* ---- PlayStation ---- */
    {INPUT_PAD_ON(PS, INPUT_PAD_SOUTH), HUD_ANIM("hud/icons/playstation/cross")},
    {INPUT_PAD_ON(PS, INPUT_PAD_EAST), HUD_ANIM("hud/icons/playstation/circle")},
    {INPUT_PAD_ON(PS, INPUT_PAD_WEST), HUD_ANIM("hud/icons/playstation/square")},
    {INPUT_PAD_ON(PS, INPUT_PAD_NORTH), HUD_ANIM("hud/icons/playstation/triangle")},
    {INPUT_PAD_ON(PS, INPUT_BIND_L1), HUD_ANIM("hud/icons/playstation/l1")},
    {INPUT_PAD_ON(PS, INPUT_BIND_R1), HUD_ANIM("hud/icons/playstation/r1")},
    {INPUT_PAD_ON(PS, INPUT_BIND_L2), HUD_ANIM("hud/icons/playstation/l2")},
    {INPUT_PAD_ON(PS, INPUT_BIND_R2), HUD_ANIM("hud/icons/playstation/r2")},
    {INPUT_PAD_ON(PS, INPUT_BIND_SELECT), HUD_ANIM("hud/icons/playstation/share")},
    {INPUT_PAD_ON(PS, INPUT_BIND_START), HUD_ANIM("hud/icons/playstation/options")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_UP), HUD_ANIM("hud/icons/playstation/dpad_up")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_DOWN), HUD_ANIM("hud/icons/playstation/dpad_down")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_LEFT), HUD_ANIM("hud/icons/playstation/dpad_left")},
    {INPUT_DPAD_ON(PS, PLATFORM_DPAD_RIGHT), HUD_ANIM("hud/icons/playstation/dpad_right")},

    /* ---- Xbox ---- */
    {INPUT_PAD_ON(XBOX, INPUT_PAD_SOUTH), HUD_ANIM("hud/icons/xbox/a")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_EAST), HUD_ANIM("hud/icons/xbox/b")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_WEST), HUD_ANIM("hud/icons/xbox/x")},
    {INPUT_PAD_ON(XBOX, INPUT_PAD_NORTH), HUD_ANIM("hud/icons/xbox/y")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_L1), HUD_ANIM("hud/icons/xbox/lb")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_R1), HUD_ANIM("hud/icons/xbox/rb")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_L2), HUD_ANIM("hud/icons/xbox/lt")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_R2), HUD_ANIM("hud/icons/xbox/rt")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_SELECT), HUD_ANIM("hud/icons/xbox/view")},
    {INPUT_PAD_ON(XBOX, INPUT_BIND_START), HUD_ANIM("hud/icons/xbox/menu")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_UP), HUD_ANIM("hud/icons/xbox/dpad_up")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_DOWN), HUD_ANIM("hud/icons/xbox/dpad_down")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_LEFT), HUD_ANIM("hud/icons/xbox/dpad_left")},
    {INPUT_DPAD_ON(XBOX, PLATFORM_DPAD_RIGHT), HUD_ANIM("hud/icons/xbox/dpad_right")},

    /* ---- Nintendo ---- */
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_SOUTH), HUD_ANIM("hud/icons/nintendo/b")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_EAST), HUD_ANIM("hud/icons/nintendo/a")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_WEST), HUD_ANIM("hud/icons/nintendo/y")},
    {INPUT_PAD_ON(NINTENDO, INPUT_PAD_NORTH), HUD_ANIM("hud/icons/nintendo/x")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_L1), HUD_ANIM("hud/icons/nintendo/l")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_R1), HUD_ANIM("hud/icons/nintendo/r")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_L2), HUD_ANIM("hud/icons/nintendo/zl")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_R2), HUD_ANIM("hud/icons/nintendo/zr")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_SELECT), HUD_ANIM("hud/icons/nintendo/minus")},
    {INPUT_PAD_ON(NINTENDO, INPUT_BIND_START), HUD_ANIM("hud/icons/nintendo/plus")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_UP), HUD_ANIM("hud/icons/nintendo/dpad_up")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_DOWN), HUD_ANIM("hud/icons/nintendo/dpad_down")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_LEFT), HUD_ANIM("hud/icons/nintendo/dpad_left")},
    {INPUT_DPAD_ON(NINTENDO, PLATFORM_DPAD_RIGHT), HUD_ANIM("hud/icons/nintendo/dpad_right")},

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
