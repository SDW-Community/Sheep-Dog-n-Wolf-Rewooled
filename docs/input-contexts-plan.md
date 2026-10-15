# Input contexts: a refactor plan

A design plan. It describes how to give menus, gameplay and minigames (the mine defusal, and later the telescope,
cannon, catapult, fishing rod) their own control schemes, with the button prompts drawn from the same bindings.
[input-system.md](input-system.md) describes how input works today.

**Status:** migration step 2 (the action layer) is scaffolded in `src/engine/input/input_actions.h` / `.cpp`. It updates
every frame at the end of `Input_Poll`, but **no reader uses it yet**. To watch it, press F3 (the overlay's last line:
stack depth, top context, held actions) and F6 (cycles the stack: Gameplay → + Menu → + Defuse → back). Differences
from the sketch below:
- the contexts are `ICTX_*`, because the game already has a `CTX_*` enum (the Wolf's action contexts);
- 4 bindings per action, and a d-pad source (`SRC_PAD_DPAD`);
- gameplay and defuse **follow** the game's own bindings while their readers still use `g_pad`: every frame two
  *followed* bindings per action are rebuilt from the PS1 button the action is on (`keyTable` + the controller
  button, through `g_inputMap` for the remappable actions), so the layer always matches the game. They are kept in a
  separate array: every slot of a context's defaults table is its own, and both count (defuse: the face buttons
  *and* the arrow keys);
- **axes** for analog input (`Input_Axis2D`, `Input_AxisDelta`): gameplay has `GX_MOVE` (the game's movement keys,
  rebuilt every frame from `keyTable` slots 12..15, + the left stick + the d-pad) and `GX_LOOK` (the right stick +
  the mouse). `Input_Axis2D` returns a vector in -256..256 (+y forward) and a strength 0..256, built exactly as
  `ScnControllable::ReadPad` builds `stickX / stickY / stickMag` (the stick through `Pad_AnalogToStick`'s round dead
  zone; four directions as ±256, ±181 on a diagonal); the strongest binding wins. `Input_AxisDelta` is the mouse's
  motion in pixels, kept apart as the orbit camera uses it. The F3 line shows Move as `MOVE x y`;
- **per-family controller bindings:** `InputBindingRef.family` (`INPUT_FAMILY_ANY` by default, or `XBOX`,
  `PLAYSTATION`, `NINTENDO`, `GENERIC`); face buttons are named by position (`INPUT_PAD_SOUTH/EAST/WEST/NORTH`).
  When an action has controller bindings for the family in use, only those count; otherwise its `ANY` ones do
  (example: the menu confirms with east (A) and goes back with south (B) on Nintendo pads). `Input_PadFamily()` tells
  the HUD which family is in use; the F3 line shows it (`XB`, `PS`, `NIN`, `PAD`);
- **HUD prompts** (`src/engine/hud/`): `hud_prompt` draws an action's prompt from its binding on the device in use:
  the binding's icon for the controller family (`hud_button_icons.cpp`: a PNG in the game's data folder under
  `hud/icons/<family>/`, or text in the HUD font), else the context's placeholder (Defuse on a controller: `●`), else a
  label. `hud_draw` is the only file there that uses the old engine. The mine draws them over its old sprites;
- the **`postProcess` hook** (`InputContextDef.postProcess`): per-context logic run after the bindings are read and
  before the edges / swallowing / repeats. Gameplay's (`Gameplay_PostProcess`) does tap-to-run - a movement key
  tapped 3 times and held presses `GA_RUN` for one frame every 400 ms, as the game's run rhythm wants - and presses
  `GA_STOP_RUN` when the opposite movement key is pressed during a run. It reads the Move axis's keys, and is off with
  Legacy Controls. `InputMgr_TapRun` / `Input_TakeRunCancel` still feed `g_pad` until `ReadPad` reads the actions;
  remove them in that change;
- `DA_ABORT` is not there yet.

## The problem today

Every input reader looks at the same thing: the PS1 button word in `g_pad`. Nothing reads "actions". So there are
three schemes, wired three different ways, and two separate ways of showing buttons:

| Who | How it reads input | How it shows buttons |
|---|---|---|
| Gameplay (`ScnControllable::ReadPad`, ~30 objects) | `g_pad` through the remappable masks `g_padMasks[]` (Jump = "whatever PS1 button slot 15 is") | `$B_JUMP$` etc. in text → PS1 font glyph via `g_inputMapAux` (+ the controller-icon hook, `src/engine/button_icons.cpp`) |
| Menus (`pause_menu.cpp`, `menu.cpp`, `map.cpp`) | `Pad_MenuHeld / Pressed / Repeat` with fixed PS1 bits (Cross = confirm, Triangle = back) + hard-coded Enter / Esc / arrows in `InputMgr::Poll` | `$B_VALID$`, `$B_CANCEL$` |
| Mine minigame (`objects/mine.cpp`) | its own table of raw PS1 bits (`g_defuseButtonTable`, line 132), press edges on `g_pad` (line 841) | its own DAV sprites `IMNPADT_/C_/S_/X_` (line 1044) |

Consequences: the action button means the same physical button everywhere; a press that closes a menu can leak into
gameplay; rebinding is one global table; every reader needs its own "are we paused / in a cutscene?" checks; and the
prompts can disagree with the real bindings.

## The target model

1. **Actions belong to a context, not to buttons.**

   ```
   Gameplay:  Move (axis), Jump, Run, Action, Sneak, Look, ItemWheel, CamLeft, CamRight, Map, Pause
   Menu:      Up, Down, Left, Right, Confirm, Back, PageLeft, PageRight
   Defuse:    Symbol1..4, Abort
   Cinematic: Skip
   ```

2. **Bindings are a table: context × action → physical inputs, per device.** The same key can be Jump in Gameplay
   and Confirm in Menu; conflicts only matter inside one context. Each context saves its own bindings. Legacy
   Controls becomes a different preset of the Gameplay context.

3. **A context stack decides who gets the input.** Opening the pause menu pushes Menu, the defusal pushes Defuse;
   only the top context sees input. When a context is pushed (or uncovered), whatever is held is swallowed until it
   is released, so the press that opened or closed something does nothing else (the Start → Esc retrigger that
   `buttonBlocked` works around is this bug).

4. **Readers ask about actions, never buttons:** `Input_Pressed(ICTX_DEFUSE, DA_TOP)`. Edges, auto-repeat and dead
   zones belong to the context (menus repeat, gameplay does not).

5. **Prompts resolve from the same table:** (context, action) → the binding on the device in use → icon (sheet
   cell) → fallback DAV glyph. Input and HUD can no longer drift apart.

### No class hierarchy

No `InputContext` base class with `MenuInputContext`, `MineInputContext`… subclasses. The contexts differ only in
**data** (which actions, their default keys, a few flags), not in behaviour. One definition type plus one table per
context keeps rebinding, saving and the controls page generic: they loop over `actionNames` and `bindings` for any
context. The one piece of real custom logic (gameplay's tap-to-run) fits as an optional hook function.

## Code sketch

### `engine/input/input_actions.h`

```cpp
#ifndef SDW_INPUT_ACTIONS_H
#define SDW_INPUT_ACTIONS_H
#include "sdw_types.h"

/* who owns the input: only the top of the stack (and what it lets through) sees it */
enum InputContextId { ICTX_GAMEPLAY, ICTX_MENU, ICTX_DEFUSE, ICTX_CINEMATIC, ICTX_COUNT };

/* each context's own actions */
enum GameplayAction { GA_JUMP, GA_RUN, GA_ACTION, GA_SNEAK, GA_LOOK, GA_ITEMWHEEL, GA_CAM_LEFT, GA_CAM_RIGHT,
                      GA_MAP, GA_PAUSE, GA_COUNT };
enum MenuAction { MA_UP, MA_DOWN, MA_LEFT, MA_RIGHT, MA_CONFIRM, MA_BACK, MA_COUNT };
enum DefuseAction { DA_TOP, DA_RIGHT, DA_LEFT, DA_BOTTOM, DA_ABORT, DA_COUNT };
enum CinematicAction { CA_SKIP, CA_COUNT };

#define INPUT_MAX_ACTIONS 16
#define INPUT_MAX_BINDINGS 3 /* per action: e.g. a key, a mouse button and a controller button */

/* one physical input */
enum InputSource { SRC_NONE, SRC_KEY, SRC_MOUSE, SRC_PAD_BUTTON, SRC_PAD_AXIS_POS, SRC_PAD_AXIS_NEG };
struct InputBindingRef {
    u8 source; /* InputSource */
    u16 code;  /* DIK_* key, mouse button 0..3, PS1 slot (INPUT_BIND_*), or axis index */
};

/* a context: data only */
struct InputContextDef {
    const char *name;               /* save key and controls-page title */
    u8 actionCount;
    const char *const *actionNames; /* the controls page lists these */
    const InputBindingRef (*defaults)[INPUT_MAX_BINDINGS];
    u8 autoRepeat;                  /* held actions repeat (menus) */
    u8 passThrough;                 /* the contexts below still get input (none of ours do) */
    void (*postProcess)(u32 *down); /* optional custom logic, e.g. gameplay's tap-to-run; 0 for most */
};

void Input_PushContext(u8 ctx);
void Input_PopContext(u8 ctx);
u8 Input_IsTop(u8 ctx);

u8 Input_Held(u8 ctx, u8 action);
u8 Input_Pressed(u8 ctx, u8 action);  /* this frame's press edge */
u8 Input_Released(u8 ctx, u8 action);
u8 Input_Repeat(u8 ctx, u8 action);   /* press, then repeats while held (autoRepeat contexts) */

/* for prompts and the controls page: the action's binding on a device (keyboard / controller) */
u8 Input_FindBinding(u8 ctx, u8 action, u8 wantPad, InputBindingRef *out);
void Input_Rebind(u8 ctx, u8 action, u8 slot, InputBindingRef b);

void Input_UpdateActions(); /* once a frame, after InputMgr::Poll */
#endif
```

### `engine/input/input_actions.cpp`

The default tables reproduce today's layout, so switching a reader over changes nothing for the player.

```cpp
#define KEY(k) {SRC_KEY, k}
#define MOUSE(b) {SRC_MOUSE, b}
#define PAD(slot) {SRC_PAD_BUTTON, slot}
#define NONE {SRC_NONE, 0}

static const char *const s_gameplayNames[GA_COUNT] = {"Jump", "Run", "Action", "Sneak", "Look", "Item wheel",
                                                      "Camera left", "Camera right", "Map", "Pause"};
static const InputBindingRef s_gameplayDefaults[GA_COUNT][INPUT_MAX_BINDINGS] = {
    /* GA_JUMP      */ {KEY(DIK_SPACE), NONE, PAD(INPUT_BIND_SQUARE)},
    /* GA_RUN       */ {KEY(DIK_Z), NONE, PAD(INPUT_BIND_CIRCLE)},
    /* GA_ACTION    */ {KEY(DIK_X), MOUSE(0), PAD(INPUT_BIND_CROSS)},
    /* GA_SNEAK     */ {KEY(DIK_LSHIFT), NONE, PAD(INPUT_BIND_R1)},
    /* GA_LOOK      */ {KEY(DIK_C), NONE, PAD(INPUT_BIND_TRIANGLE)},
    /* GA_ITEMWHEEL */ {KEY(DIK_LMENU), NONE, PAD(INPUT_BIND_L1)},
    /* GA_CAM_LEFT  */ {KEY(DIK_Q), NONE, PAD(INPUT_BIND_L2)},
    /* GA_CAM_RIGHT */ {KEY(DIK_E), NONE, PAD(INPUT_BIND_R2)},
    /* GA_MAP       */ {KEY(DIK_TAB), NONE, PAD(INPUT_BIND_SELECT)},
    /* GA_PAUSE     */ {KEY(DIK_ESCAPE), NONE, PAD(INPUT_BIND_START)},
};
static const char *const s_menuNames[MA_COUNT] = {"Up", "Down", "Left", "Right", "Confirm", "Back"};
static const InputBindingRef s_menuDefaults[MA_COUNT][INPUT_MAX_BINDINGS] = {
    {KEY(DIK_UP), KEY(DIK_W), {SRC_PAD_AXIS_NEG, 1}},
    {KEY(DIK_DOWN), KEY(DIK_S), {SRC_PAD_AXIS_POS, 1}},
    {KEY(DIK_LEFT), KEY(DIK_A), {SRC_PAD_AXIS_NEG, 0}},
    {KEY(DIK_RIGHT), KEY(DIK_D), {SRC_PAD_AXIS_POS, 0}},
    {KEY(DIK_RETURN), MOUSE(0), PAD(INPUT_BIND_CROSS)},
    {KEY(DIK_ESCAPE), MOUSE(1), PAD(INPUT_BIND_TRIANGLE)},
};
static const char *const s_defuseNames[DA_COUNT] = {"Top symbol", "Right symbol", "Left symbol", "Bottom symbol",
                                                    "Give up"};
static const InputBindingRef s_defuseDefaults[DA_COUNT][INPUT_MAX_BINDINGS] = {
    {KEY(DIK_W), KEY(DIK_UP), PAD(INPUT_BIND_TRIANGLE)},
    {KEY(DIK_D), KEY(DIK_RIGHT), PAD(INPUT_BIND_CIRCLE)},
    {KEY(DIK_A), KEY(DIK_LEFT), PAD(INPUT_BIND_SQUARE)},
    {KEY(DIK_S), KEY(DIK_DOWN), PAD(INPUT_BIND_CROSS)},
    {KEY(DIK_ESCAPE), NONE, PAD(INPUT_BIND_START)},
};
/* ... ICTX_CINEMATIC likewise */

static const InputContextDef s_contexts[ICTX_COUNT] = {
    {"Gameplay", GA_COUNT, s_gameplayNames, s_gameplayDefaults, 0, 0, Gameplay_TapToRun},
    {"Menu", MA_COUNT, s_menuNames, s_menuDefaults, 1, 0, 0},
    {"Defuse", DA_COUNT, s_defuseNames, s_defuseDefaults, 0, 0, 0},
    {"Cinematic", CA_COUNT, s_cineNames, s_cineDefaults, 0, 0, 0},
};

/* per-context run-time state */
static struct {
    InputBindingRef bindings[INPUT_MAX_ACTIONS][INPUT_MAX_BINDINGS]; /* defaults, then the saved ones */
    u32 cur, prev;  /* one bit per action */
    u32 swallowed;  /* held when the context was pushed or uncovered: ignored until released */
    s32 repeatMs[INPUT_MAX_ACTIONS];
} s_state[ICTX_COUNT];

static u8 s_stack[8] = {ICTX_GAMEPLAY};
static u8 s_depth = 1;

/* one physical input, from the devices InputMgr already polled */
static u8 Input_PhysicalDown(const InputBindingRef *b)
{
    switch (b->source) {
        case SRC_KEY:
            return g_inputMgr.keyboard->buttonDown[b->code];
        case SRC_MOUSE:
            return g_inputMgr.device3 && g_inputMgr.device3->buttonDown[b->code];
        case SRC_PAD_BUTTON: /* the controller's buttons are already in PS1 slot order */
            return g_inputMgr.joystick && g_inputMgr.joystick->buttonDown[b->code];
        case SRC_PAD_AXIS_POS:
            return g_inputMgr.joystick && Pad_Axis(b->code) > 0.5f;
        case SRC_PAD_AXIS_NEG:
            return g_inputMgr.joystick && Pad_Axis(b->code) < -0.5f;
    }
    return 0;
}

void Input_UpdateActions()
{
    for (u8 c = 0; c < ICTX_COUNT; c++) {
        u32 down = 0;
        if (Input_IsTop(c))
            for (u8 a = 0; a < s_contexts[c].actionCount; a++)
                for (u8 k = 0; k < INPUT_MAX_BINDINGS; k++)
                    if (Input_PhysicalDown(&s_state[c].bindings[a][k]))
                        down |= 1u << a;
        if (s_contexts[c].postProcess)
            s_contexts[c].postProcess(&down);
        s_state[c].swallowed &= down; /* let go: no longer swallowed */
        s_state[c].prev = s_state[c].cur;
        s_state[c].cur = down & ~s_state[c].swallowed;
        /* + the repeat timers for autoRepeat contexts, from g_dtRawMs */
    }
}

void Input_PushContext(u8 ctx)
{
    s_stack[s_depth++] = ctx;
    s_state[ctx].swallowed = ~0u; /* whatever is held now belongs to the context below */
    s_state[ctx].cur = s_state[ctx].prev = 0;
}

void Input_PopContext(u8 ctx)
{
    if (s_depth > 1 && s_stack[s_depth - 1] == ctx)
        s_depth--;
    s_state[s_stack[s_depth - 1]].swallowed = ~0u; /* the Confirm that closed the menu does not also jump */
}

u8 Input_IsTop(u8 ctx) { return s_stack[s_depth - 1] == ctx; }
u8 Input_Held(u8 ctx, u8 a) { return (s_state[ctx].cur >> a) & 1; }
u8 Input_Pressed(u8 ctx, u8 a) { return ((s_state[ctx].cur & ~s_state[ctx].prev) >> a) & 1; }
u8 Input_Released(u8 ctx, u8 a) { return ((~s_state[ctx].cur & s_state[ctx].prev) >> a) & 1; }
```

## Using it

### The mine minigame (`objects/mine.cpp`)

Before (line 132, line 841):

```cpp
const DefuseButtonData g_defuseButtonTable[] = {{(u16)~PAD_TRIANGLE, 0, -18, 0, 0x004000}, ...};
...
if (!(g_padCurButtons & ~g_defuseButtonTable[s].mask) && (g_padPrevButtons & ~g_defuseButtonTable[s].mask)) {
```

After:

```cpp
const DefuseButtonData g_defuseButtonTable[] = {{DA_TOP, 0, -18, 0, 0x004000}, {DA_RIGHT, 21, -7, 0, 0x000030},
                                                {DA_LEFT, -21, -7, 0, 0x400040}, {DA_BOTTOM, 0, 4, 0, 0xff0000}};
...
if (Input_Pressed(ICTX_DEFUSE, g_defuseButtonTable[s].action)) {
```

Push in the state that starts the minigame, pop where it ends:

```cpp
case DM_ST_PLAYBACK:            /* entering the minigame */
    Input_PushContext(ICTX_DEFUSE);
...
/* defused, exploded or given up */
Input_PopContext(ICTX_DEFUSE);
```

The button sprites ask the same table instead of using the hard-wired PS1 art (`g_defusePadSprites[q]`):

```cpp
ButtonIcons_DrawAction(ICTX_DEFUSE, g_defuseButtonTable[q].action, x, y, g_screenLayerBase + 8);
```

`ButtonIcons_DrawAction` is the controller-icon hook extended: `Input_FindBinding` for the device in use → sheet
cell → draw, or the old sprite when there is no icon.

### Menus and gameplay

```cpp
void PauseMenu_Open()  { Input_PushContext(ICTX_MENU); ... }
void PauseMenu_Close() { Input_PopContext(ICTX_MENU); ... }

/* Pad_MenuPressed((u16)~PAD_CROSS)  becomes */  Input_Pressed(ICTX_MENU, MA_CONFIRM)
/* Pad_MenuRepeat((u16)~PAD_DOWN)    becomes */  Input_Repeat(ICTX_MENU, MA_DOWN)

/* ScnControllable::ReadPad: the WOLF_ACT_* bits from actions instead of g_padMasks */
if (Input_Pressed(ICTX_GAMEPLAY, GA_JUMP))  bits |= WOLF_ACT_JUMP;
if (Input_Held(ICTX_GAMEPLAY, GA_JUMP))     bits |= WOLF_ACT_JUMP_HELD;
```

While the pause menu is open, `ICTX_GAMEPLAY` is not on top, so every gameplay query returns 0: no reader needs its
own "are we paused?" check.

### Prompts: the `$B_...$` tokens become actions

The texts in the level files (`.MLT`) contain `$B_ACTION$`, `$B_VALID$`, `$B_CANCEL$`... and cannot easily be
edited, so the tokens stay and are mapped to actions:

```cpp
static const struct { const char *token; u8 ctx, action; } s_promptTokens[] = {
    {"ACTION", ICTX_GAMEPLAY, GA_ACTION},     {"JUMP", ICTX_GAMEPLAY, GA_JUMP},     {"RUN", ICTX_GAMEPLAY, GA_RUN},
    {"SNEAK", ICTX_GAMEPLAY, GA_SNEAK},       {"INTVIEW", ICTX_GAMEPLAY, GA_LOOK},  {"QUICKINV", ICTX_GAMEPLAY, GA_ITEMWHEEL},
    {"CAM_LEFT", ICTX_GAMEPLAY, GA_CAM_LEFT}, {"CAM_RIGHT", ICTX_GAMEPLAY, GA_CAM_RIGHT},
    {"VALID", ICTX_MENU, MA_CONFIRM},         {"CANCEL", ICTX_MENU, MA_BACK},
};
```

`Text_ExpandButtonToken` looks the token up here instead of in `g_inputMapAux`, and the icon hook draws that
action's current binding. Rebinding Jump in the gameplay controls page changes every `$B_JUMP$`; the menu's Confirm
icon is unaffected.

### Custom logic: the post-process hook

Tap-to-run and the stick dead zone are gameplay-only behaviour. Instead of a subclass, the gameplay context's
`postProcess` runs after its bindings are read:

```cpp
/* a movement key tapped three times and held adds GA_RUN (InputMgr_TapRun moves here) */
static void Gameplay_TapToRun(u32 *down);
```

## Migration order

Each step leaves the game working.

1. **Inventory the readers.** Grep `g_pad`, `g_padMasks`, `Pad_Menu*`, raw `PAD_*` bits and `escPressed`; label each
   hit with its context. Expect roughly 40 files. The telescope, cannon, catapult and fishing rod read the pad
   directly: they are minigames too and get their own contexts.
2. **Add the action layer underneath.** `Input_UpdateActions` runs every frame next to the existing `g_pad`, which
   `InputMgr::Poll` keeps filling. Nothing reads the new layer yet; nothing changes.
3. **Move readers one context at a time:**
   - the **mine** first: one file, one table, one sprite set; it proves input and HUD now come from one place;
   - the **menus** (`pause_menu.cpp`, `menu.cpp`, `map.cpp`);
   - **gameplay**: `ScnControllable::ReadPad` (Wolf, Robot) first, then the ~30 objects that read `g_pad`.
4. **Prompts:** `Text_ExpandButtonToken` maps tokens to (context, action); the icon hook draws the resolver's answer.
5. **Rebinding per context:** one controls page per context, saved under new keys (not the 2001 `UserConf` blob).
6. **Remove** `g_pad`, `g_padMasks`, `g_inputMap`, `Pad_Menu*` once nothing reads them.

## Decisions to make early

- **Cinematics are a context too** (Skip). Most of the "is a cutscene running?" checks scattered around
  `Game_Frame_2` become "is Cinematic on top?".
- **Keyboard and controller at once:** every context keeps bindings for both devices; prompts pick icons by the
  last-used device, which `InputMgr` already tracks (`g_inputMgr.current`).
- **Icon width:** keyboard keys (Space, Shift, Enter) want wider icons than the one-cell glyph slot. Decide at step 4
  whether the token expansion reserves two cells for them.
- **Input before update:** `Main_Loop` reads input *after* the game update (one frame of lag). Once every reader goes
  through this API, moving `Input_Poll` + `Input_UpdateActions` before `Game_Frame_2` is a single change.
