/*
 * Button prompts drawn from the input contexts (hud_prompt.h; the plan: docs/input-contexts-plan.md, step 4).
 *
 * HudPrompt_DrawAction asks the input layer for the action's binding on the device in use and draws, centred on the
 * point given, the first of:
 *   1. the binding's icon for the controller family in use (hud_button_icons.cpp: a PNG, or text);
 *   2. the context's placeholder for that device (s_placeholders), while the icons are missing;
 *   3. a label: the key's name, or the controller button's position.
 * It uses the input layer, the HUD icons and font, and the platform's key names - nothing of the game's text, menus
 * or input code.
 */
#include <stdio.h>

#include "sdw_types.h"
#include "../input/input_actions.h"
#include "../../platform/platform.h"
#include "hud_button_icons.h"
#include "hud_icons.h"
#include "hud_prompt.h"

#define HUDPROMPT_SIZE 11.0f /* the prompt's height, in HUD units (the 240-line virtual screen) */
#define HUDPROMPT_RGB 0xffffff

/* the controller buttons by slot (INPUT_BIND_*), named by position: the label when a button has no icon */
static const char *const s_padNames[12] = {"N", "E", "S", "W", "L1", "R1", "L2", "R2", "SEL", "STA", "L3", "R3"};

/* a context's symbol for the buttons that have no icon yet on a device (the symbol must be in the HUD font's
 * s_symbols); a context without an entry shows the label */
enum { HUDPROMPT_KEYBOARD, HUDPROMPT_PAD };
static const struct {
    u8 ctx;
    u8 device;
    HudIcon placeholder;
} s_placeholders[] = {
    {ICTX_DEFUSE, HUDPROMPT_PAD, HUD_TEXT("●")},
    {ICTX_DANCE, HUDPROMPT_PAD, HUD_TEXT("●")},
};

/* the label of a binding with no icon */
static u8 HudPrompt_BindingLabel(const InputBindingRef *b, char *out, int size)
{
    char *c;
    out[0] = 0;
    switch (b->source) {
        case SRC_KEY:
            if (!Platform_KeyName(b->code, out, size))
                return 0;
            for (c = out; *c; c++)
                if (*c >= 'a' && *c <= 'z')
                    *c -= 'a' - 'A';
            return 1;
        case SRC_MOUSE:
            snprintf(out, size, "M%u", (unsigned)b->code + 1);
            return 1;
        case SRC_PAD_BUTTON:
            if (b->code >= 12)
                return 0;
            snprintf(out, size, "%s", s_padNames[b->code]);
            return 1;
        case SRC_PAD_DPAD:
            snprintf(out, size, "DPAD");
            return 1;
        case SRC_PAD_AXIS_POS:
        case SRC_PAD_AXIS_NEG:
            snprintf(out, size, "STICK");
            return 1;
    }
    return 0;
}

/* the action's binding on the device in use, or on the other one when it has nothing there; *pad: which */
static u8 HudPrompt_Binding(u8 ctx, u8 action, InputBindingRef *b, u8 *pad)
{
    *pad = Input_PadInUse();
    if (Input_FindBinding(ctx, action, *pad, b))
        return 1;
    *pad = !*pad;
    return Input_FindBinding(ctx, action, *pad, b);
}

static const HudIcon *HudPrompt_Placeholder(u8 ctx, u8 pad)
{
    u32 i;
    for (i = 0; i < sizeof(s_placeholders) / sizeof(s_placeholders[0]); i++)
        if (s_placeholders[i].ctx == ctx && s_placeholders[i].device == (pad ? HUDPROMPT_PAD : HUDPROMPT_KEYBOARD))
            return &s_placeholders[i].placeholder;
    return 0;
}

u8 HudPrompt_Label(u8 ctx, u8 action, char *out, int size)
{
    InputBindingRef b;
    u8 pad;
    return HudPrompt_Binding(ctx, action, &b, &pad) && HudPrompt_BindingLabel(&b, out, size);
}

u8 HudPrompt_DrawAction(u8 ctx, u8 action, s32 x, s32 y, u32 *layer, u8 animate)
{
    InputBindingRef b;
    HudIcon label;
    char text[32];
    const HudIcon *icon;
    u8 pad;
    if (!HudPrompt_Binding(ctx, action, &b, &pad))
        return 0;
    icon = HudButtonIcons_Find(&b, pad ? Input_PadFamily() : INPUT_FAMILY_ANY);
    if (!icon)
        icon = HudPrompt_Placeholder(ctx, pad);
    if (!icon) {
        if (!HudPrompt_BindingLabel(&b, text, sizeof(text)))
            return 0;
        label.kind = HUDICON_TEXT;
        label.value = text;
        icon = &label;
    }
    return HudIcons_Draw(icon, (float)x, (float)y, HUDPROMPT_SIZE, HUDPROMPT_RGB, layer, animate);
}
