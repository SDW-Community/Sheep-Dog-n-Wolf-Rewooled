/* What each button looks like on the HUD (hud_button_icons.cpp): binding x controller family -> icon. Used by every
 * context's prompts; a context only says how it shows them (hud_prompt.cpp). */
#ifndef SDW_HUD_BUTTON_ICONS_H
#define SDW_HUD_BUTTON_ICONS_H

#include "sdw_types.h"
#include "../input/input_actions.h"
#include "hud_icons.h"

struct HudButtonIcon {
    InputBindingRef binding; /* the key / mouse button / controller button; its family: the controllers it is for */
    HudIcon icon;
};

/* the icon of a binding on a controller family (INPUT_FAMILY_ANY for the keyboard and mouse): the entry for that
 * family when it is ready, else the entry for any family when it is ready; 0 when there is none */
const HudIcon *HudButtonIcons_Find(const InputBindingRef *binding, u8 family);

/* every entry, for the atlas (hud_icons.cpp) */
u32 HudButtonIcons_Count();
const HudIcon *HudButtonIcons_Icon(u32 index);

#endif
