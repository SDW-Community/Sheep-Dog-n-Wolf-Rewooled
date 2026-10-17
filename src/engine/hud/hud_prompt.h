/* Button prompts drawn from the input contexts (hud_prompt.cpp): what to press for an action, from its binding on the
 * device in use and the controller family - not from the .DAV's PlayStation button art. The picture is the binding's
 * icon (hud_button_icons.cpp: a PNG per family, or text), else the context's placeholder, else a label. It draws on
 * top of whatever is there, so it can sit over the old prompts while they are still drawn. */
#ifndef SDW_HUD_PROMPT_H
#define SDW_HUD_PROMPT_H

#include "sdw_types.h"

/* the plain label of an action's binding on the device in use (UTF-8: a key's name, a button's position); 0 when none */
u8 HudPrompt_Label(u8 ctx, u8 action, char *out, int size);

/* draws the prompt centred at (x, y) in HUD units (512 x 240), on `layer` (g_screenLayerBase + n: a smaller n is in
 * front). `animate`: an animated icon blinks with the text's button glyphs (as Text_PrintfStyled's `blink`), else
 * it stays on its first frame. Uses the HudElement anchoring in force, so a caller inside a HudElement scope keeps
 * its alignment. 1 when something was drawn. */
u8 HudPrompt_DrawAction(u8 ctx, u8 action, s32 x, s32 y, u32 *layer, u8 animate = 0);

#endif
