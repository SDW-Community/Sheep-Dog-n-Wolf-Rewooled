/*
 * Input contexts and actions (input_actions.h; the plan: docs/input-contexts-plan.md).
 *
 * A context is data: its actions' names, their default bindings and a few flags (InputContextDef). Its run-time
 * state is the bindings in use and one bit per action for this frame and the last. Input_UpdateActions reads every
 * binding of the context on top of the stack from the devices InputMgr polled this frame (keyboard, mouse,
 * controller); the other contexts read nothing.
 *
 * While the game's readers still use g_pad, the contexts whose actions already exist there FOLLOW the game's own
 * bindings (InputContextDef.followsSlot): every frame two FOLLOWED bindings per action are rebuilt from the
 * PlayStation button the action is on - the keyboard / mouse binding of that button (InputMgr keyTable) and the
 * controller's button - so this layer always agrees with what the game does, whatever layout (new, legacy, rebound,
 * a preset of the options screen) is in use. They are kept apart from the context's own bindings (all
 * INPUT_MAX_BINDINGS of them, from its defaults table), and both count. A context stops following once its readers
 * have moved over and its bindings are edited and saved here.
 */
#include "sdw_types.h"
#include "sdw_enums.h"
#include "sdw_classes.h"
#include "../../sdk/crt.h"
#include "../../sdk/dinput.h"

#include "../draw2d.h"
#include "../input.h"
#include "input_actions.h"
#include "../../platform/platform.h"
#include "../../platform/save_store.h"

extern u16 g_inputMap[16]; /* the actions' PS1 masks (input.cpp): g_inputMap[4 + n] is g_padMasks[n] */
extern s32 g_dtRawMs;
extern u8 g_camInvertJoystick; /* objects/camera.cpp: the right stick's left / right inverted */
extern s32 g_rawTimeMs;        /* the real time in ms, unscaled, unpaused (time.cpp) */

#define KEY(k) {SRC_KEY, k}
#define MOUSE(b) {SRC_MOUSE, b}
#define PAD(slot) {SRC_PAD_BUTTON, slot}                    /* on every controller */
#define PAD_ON(family, slot) {SRC_PAD_BUTTON, slot, family} /* only on that family (INPUT_FAMILY_*) */
#define DPAD(d) {SRC_PAD_DPAD, d}
#define AXIS_POS(a) {SRC_PAD_AXIS_POS, a}
#define AXIS_NEG(a) {SRC_PAD_AXIS_NEG, a}
#define NONE {SRC_NONE, 0}

/* where a following context's action is on the PlayStation pad */
enum {
    FOLLOW_NONE,
    FOLLOW_SLOT, /* a fixed slot (INPUT_BIND_*) */
    FOLLOW_MASK, /* the slot g_inputMap[arg] currently points at (the remappable actions) */
};
struct InputFollow {
    u8 kind;
    u8 arg;
};

struct InputContextDef {
    const char *name;
    u8 actionCount;
    const char *const *actionNames;
    const InputBindingRef (*defaults)[INPUT_MAX_BINDINGS];
    const InputFollow *followsSlot; /* per action, or 0: the context has its own bindings only */
    u8 autoRepeat;                  /* held actions repeat (Input_Repeat) */
    u8 axisCount;
    const char *const *axisNames;
    const InputAxisBindingRef (*axisDefaults)[INPUT_MAX_AXIS_BINDINGS];
    s8 followsMoveKeys; /* the axis whose binding 0 is rebuilt from the game's movement keys (keyTable 12..15); -1 */
    /* custom logic after the bindings are read (and before the edges, the swallowing and the repeats): it may set or
     * clear action bits in *down, actions no single binding can express. Called every frame, also when the context is
     * not on top (*down is 0 then): a chance to forget its state. 0 for most contexts */
    void (*postProcess)(u8 ctx, u32 *down);
    /* the defaults with Legacy Controls on (the disc's layout); 0: the same as `defaults` */
    const InputBindingRef (*legacyDefaults)[INPUT_MAX_BINDINGS];
    const InputAxisBindingRef (*legacyAxisDefaults)[INPUT_MAX_AXIS_BINDINGS];
    /* the CfgGame value its bindings are saved in (Input_SaveBindings); 0: not saved */
    const char *saveName;
};

#define AXIS_KEYS(u, r, d, l) {AXSRC_KEYS, {u, r, d, l}}
#define AXIS(src) {src, {0, 0, 0, 0}}
#define AXIS_NONE AXIS(AXSRC_NONE)

/* ---- the contexts ---- */

static const char *const s_gameplayNames[GA_COUNT] = {"JUMP",  "RUN",   "ACTION", "SNEAK", "LOOK", "WHEEL",
                                                      "CAM-L", "CAM-R", "MAP",    "PAUSE", "STOP"};
/* the context's own bindings, besides the game's that it follows (s_gameplayFollow) */
static const InputBindingRef s_gameplayDefaults[GA_COUNT][INPUT_MAX_BINDINGS] = {
    /* the new controls' layout (the keyboard as InputMgr::ApplyDefaultKeyChanges sets it; the controller as the
     * PlayStation game: buttons by position) */
    /* GA_JUMP      */ {INPUT_KEY(DIK_SPACE), INPUT_PAD(INPUT_PAD_WEST), NONE, NONE},
    /* GA_RUN       */ {INPUT_KEY(DIK_Z), INPUT_PAD(INPUT_PAD_EAST), NONE, NONE},
    /* GA_ACTION    */ {INPUT_MOUSE(0), INPUT_KEY(DIK_X), INPUT_PAD(INPUT_PAD_SOUTH), NONE},
    /* GA_SNEAK     */ {INPUT_KEY(DIK_LSHIFT), INPUT_PAD(INPUT_BIND_R1), NONE, NONE},
    /* GA_LOOK      */ {INPUT_KEY(DIK_C), INPUT_PAD(INPUT_PAD_NORTH), NONE, NONE},
    /* GA_ITEMWHEEL */ {INPUT_KEY(DIK_LMENU), INPUT_PAD(INPUT_BIND_L1), NONE, NONE},
    /* GA_CAM_LEFT  */ {INPUT_KEY(DIK_Q), INPUT_PAD(INPUT_BIND_L2), NONE, NONE},
    /* GA_CAM_RIGHT */ {INPUT_KEY(DIK_E), INPUT_PAD(INPUT_BIND_R2), NONE, NONE},
    /* GA_MAP       */ {INPUT_KEY(DIK_TAB), INPUT_PAD(INPUT_BIND_SELECT), NONE, NONE},
    /* GA_PAUSE     */ {INPUT_KEY(DIK_ESCAPE), INPUT_PAD(INPUT_BIND_START), NONE, NONE},
    /* GA_STOP_RUN  */ {NONE, NONE, NONE, NONE}, /* pressed by Gameplay_PostProcess only */
};
/* Legacy Controls: the disc's keyboard layout (DefKConf.BSC; sneak on C, the port's change: macOS takes Ctrl + the
 * arrows), the arrows to move. The controller as in the new controls */
static const InputBindingRef s_gameplayLegacyDefaults[GA_COUNT][INPUT_MAX_BINDINGS] = {
    /* GA_JUMP      */ {INPUT_KEY(DIK_SPACE), INPUT_PAD(INPUT_PAD_WEST), NONE, NONE},
    /* GA_RUN       */ {INPUT_KEY(DIK_Z), INPUT_PAD(INPUT_PAD_EAST), NONE, NONE},
    /* GA_ACTION    */ {INPUT_KEY(DIK_X), INPUT_PAD(INPUT_PAD_SOUTH), NONE, NONE},
    /* GA_SNEAK     */ {INPUT_KEY(DIK_C), INPUT_PAD(INPUT_BIND_R1), NONE, NONE},
    /* GA_LOOK      */ {INPUT_KEY(DIK_LSHIFT), INPUT_PAD(INPUT_PAD_NORTH), NONE, NONE},
    /* GA_ITEMWHEEL */ {INPUT_KEY(DIK_LMENU), INPUT_PAD(INPUT_BIND_L1), NONE, NONE},
    /* GA_CAM_LEFT  */ {INPUT_KEY(DIK_A), INPUT_PAD(INPUT_BIND_L2), NONE, NONE},
    /* GA_CAM_RIGHT */ {INPUT_KEY(DIK_S), INPUT_PAD(INPUT_BIND_R2), NONE, NONE},
    /* GA_MAP       */ {INPUT_KEY(DIK_F12), INPUT_PAD(INPUT_BIND_SELECT), NONE, NONE},
    /* GA_PAUSE     */ {INPUT_KEY(DIK_ESCAPE), INPUT_PAD(INPUT_BIND_START), NONE, NONE},
    /* GA_STOP_RUN  */ {NONE, NONE, NONE, NONE},
};

/* the remappable actions through g_inputMap (ScnControllable::ReadPad's g_padMasks indices: 4 camera left .. 11
 * jump); Select and Start are fixed */
static const InputFollow s_gameplayFollow[GA_COUNT] = {
    {FOLLOW_MASK, 4 + 11}, {FOLLOW_MASK, 4 + 9}, {FOLLOW_MASK, 4 + 10}, {FOLLOW_MASK, 4 + 7}, {FOLLOW_MASK, 4 + 8},
    {FOLLOW_MASK, 4 + 6},  {FOLLOW_MASK, 4 + 4}, {FOLLOW_MASK, 4 + 5},  {FOLLOW_SLOT, INPUT_BIND_SELECT},
    {FOLLOW_SLOT, INPUT_BIND_START}, {FOLLOW_NONE, 0},
};

static const char *const s_menuNames[MA_COUNT] = {"UP", "DOWN", "LEFT", "RIGHT", "OK", "BACK"};
/* the menus' own keys (InputMgr::Poll reads the arrows, Enter and Esc outside the binding table), the d-pad and the
 * left stick, Cross / Triangle as Pad_MenuPressed tests them */
static const InputBindingRef s_menuDefaults[MA_COUNT][INPUT_MAX_BINDINGS] = {
    /* MA_UP      */ {KEY(DIK_UP), DPAD(PLATFORM_DPAD_UP), AXIS_NEG(1), NONE},
    /* MA_DOWN    */ {KEY(DIK_DOWN), DPAD(PLATFORM_DPAD_DOWN), AXIS_POS(1), NONE},
    /* MA_LEFT    */ {KEY(DIK_LEFT), DPAD(PLATFORM_DPAD_LEFT), AXIS_NEG(0), NONE},
    /* MA_RIGHT   */ {KEY(DIK_RIGHT), DPAD(PLATFORM_DPAD_RIGHT), AXIS_POS(0), NONE},
    /* Nintendo pads confirm with A (east) and go back with B (south); the others as the PlayStation game */
    /* MA_CONFIRM */ {KEY(DIK_RETURN), PAD(INPUT_PAD_SOUTH), PAD_ON(INPUT_FAMILY_NINTENDO, INPUT_PAD_EAST), NONE},
    /* MA_BACK    */ {KEY(DIK_ESCAPE), PAD(INPUT_PAD_NORTH), PAD_ON(INPUT_FAMILY_NINTENDO, INPUT_PAD_SOUTH), NONE},
};

static const char *const s_defuseNames[DA_COUNT] = {"TOP", "RIGHT", "LEFT", "BOTTOM"};

static const InputBindingRef s_defuseDefaults[DA_COUNT][INPUT_MAX_BINDINGS] = {
        /* DA_TOP    */ {NONE, KEY(DIK_W), PAD(INPUT_BIND_TRIANGLE), DPAD(PLATFORM_DPAD_UP)},
        /* DA_RIGHT  */ {NONE, KEY(DIK_D), PAD(INPUT_BIND_CIRCLE), DPAD(PLATFORM_DPAD_RIGHT)},
        /* DA_LEFT   */ {NONE, KEY(DIK_A), PAD(INPUT_BIND_SQUARE), DPAD(PLATFORM_DPAD_LEFT)},
        /* DA_BOTTOM */ {NONE, KEY(DIK_S), PAD(INPUT_BIND_CROSS), DPAD(PLATFORM_DPAD_DOWN)},
};



/* Move: binding 0 is the game's movement keys (rebuilt every frame), then the left stick and the d-pad (the game
 * reads no d-pad in play: this context's own). Look: the right stick and the mouse */
static const char *const s_gameplayAxisNames[GX_COUNT] = {"MOVE", "LOOK"};
static const InputAxisBindingRef s_gameplayAxisDefaults[GX_COUNT][INPUT_MAX_AXIS_BINDINGS] = {
    /* GX_MOVE */ {AXIS_KEYS(DIK_W, DIK_D, DIK_S, DIK_A), AXIS(AXSRC_PAD_LEFT_STICK), AXIS(AXSRC_PAD_DPAD)},
    /* GX_LOOK */ {AXIS(AXSRC_PAD_RIGHT_STICK), AXIS(AXSRC_MOUSE), AXIS_NONE},
};

static void Gameplay_PostProcess(u8 ctx, u32 *down);

static const InputAxisBindingRef s_gameplayLegacyAxisDefaults[GX_COUNT][INPUT_MAX_AXIS_BINDINGS] = {
    /* GX_MOVE */ {AXIS_KEYS(DIK_UP, DIK_RIGHT, DIK_DOWN, DIK_LEFT), AXIS(AXSRC_PAD_LEFT_STICK), AXIS(AXSRC_PAD_DPAD)},
    /* GX_LOOK */ {AXIS(AXSRC_PAD_RIGHT_STICK), AXIS(AXSRC_MOUSE), AXIS_NONE},
};

static const char *const s_cineNames[CA_COUNT] = {"SKIP"};
static const InputBindingRef s_cineDefaults[CA_COUNT][INPUT_MAX_BINDINGS] = {
    {KEY(DIK_ESCAPE), PAD(INPUT_BIND_START), PAD(INPUT_BIND_CROSS), NONE}};

static const InputContextDef s_contexts[ICTX_COUNT] = {
    {"GAMEPLAY", GA_COUNT, s_gameplayNames, s_gameplayDefaults, 0, 0, GX_COUNT, s_gameplayAxisNames,
     s_gameplayAxisDefaults, -1, Gameplay_PostProcess, s_gameplayLegacyDefaults, s_gameplayLegacyAxisDefaults,
     "InputGameplay"},
    {"MENU", MA_COUNT, s_menuNames, s_menuDefaults, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0},
    {"DEFUSE", DA_COUNT, s_defuseNames, s_defuseDefaults, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
    {"CINEMATIC", CA_COUNT, s_cineNames, s_cineDefaults, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
};

/* ---- the state ---- */

#define INPUT_REPEAT_FIRST_MS 500 /* as the game's own repeat (Pad::Latch) */
#define INPUT_REPEAT_NEXT_MS 125
#define INPUT_STACK_MAX 8

static struct {
    InputBindingRef bindings[INPUT_MAX_ACTIONS][INPUT_MAX_BINDINGS]; /* the context's own: defaults, then the saved */
    InputBindingRef followed[INPUT_MAX_ACTIONS][2]; /* the game's, rebuilt every frame (Input_Follow); none if not */
    u32 cur, prev;  /* one bit per action */
    u32 swallowed;  /* held when the context was pushed or uncovered: ignored until released */
    u32 repeated;   /* the actions that repeat this frame */
    s32 repeatMs[INPUT_MAX_ACTIONS];
    InputAxisBindingRef axisBindings[INPUT_MAX_AXES][INPUT_MAX_AXIS_BINDINGS];
    struct {
        s32 x, y, dx, dy;
        u16 strength;
    } axis[INPUT_MAX_AXES];
    u8 initialised;
} s_state[ICTX_COUNT];

static u8 s_stack[INPUT_STACK_MAX] = {ICTX_GAMEPLAY};
static u8 s_padFamily; /* the connected controller's family this frame (Input_PadFamily) */
static u8 s_depth = 1;

/* the PS1 button bit of each slot (INPUT_BIND_TRIANGLE .. INPUT_BIND_R3), as InputMgr's s_slotPadBit */
static const u16 s_slotPadBit[12] = {PAD_TRIANGLE, PAD_CIRCLE, PAD_CROSS,  PAD_SQUARE, PAD_L1, PAD_R1,
                                     PAD_L2,       PAD_R2,     PAD_SELECT, PAD_START,  PAD_L3, PAD_R3};

static u8 Input_ValidContext(u8 ctx)
{
    return ctx < ICTX_COUNT;
}

/* the slot an active-low PS1 mask (g_inputMap) names; 0xff when none */
static u8 Input_SlotOfMask(u16 mask)
{
    u8 slot;
    for (slot = 0; slot < 12; slot++)
        if ((u16)~mask == s_slotPadBit[slot])
            return slot;
    return 0xff;
}

/* the keyboard / mouse binding the game has for a PS1 slot (InputMgr keyTable) */
static InputBindingRef Input_GameKeyFor(u8 slot)
{
    InputBindingRef b = NONE;
    const InputBinding *gb = &g_inputMgr.keyTable[slot];
    if (gb->dev && gb->dev == (InputDevice *)g_inputMgr.keyboard) {
        b.source = SRC_KEY;
        b.code = gb->code;
    } else if (gb->dev && gb->dev == (InputDevice *)g_inputMgr.device3) {
        b.source = SRC_MOUSE;
        b.code = gb->code;
    }
    return b;
}

/* a following context's followed bindings, rebuilt from the game's: the key / mouse button, the controller button */
static void Input_Follow(u8 ctx)
{
    const InputContextDef *def = &s_contexts[ctx];
    u8 a, slot;
    for (a = 0; a < def->actionCount; a++) {
        const InputFollow *f = &def->followsSlot[a];
        slot = f->kind == FOLLOW_SLOT ? f->arg : f->kind == FOLLOW_MASK ? Input_SlotOfMask(g_inputMap[f->arg]) : 0xff;
        if (slot == 0xff) {
            s_state[ctx].followed[a][0].source = s_state[ctx].followed[a][1].source = SRC_NONE;
            continue;
        }
        s_state[ctx].followed[a][0] = Input_GameKeyFor(slot);
        s_state[ctx].followed[a][1].source = SRC_PAD_BUTTON;
        s_state[ctx].followed[a][1].code = slot;
    }
}

/* the defaults of the controls in force: the legacy ones with Legacy Controls on, when the context has them */
void Input_ResetBindings(u8 ctx)
{
    const InputContextDef *def;
    u8 legacy;
    if (!Input_ValidContext(ctx))
        return;
    def = &s_contexts[ctx];
    legacy = g_legacyControls && def->legacyDefaults;
    memcpy(s_state[ctx].bindings, legacy ? def->legacyDefaults : def->defaults,
           def->actionCount * INPUT_MAX_BINDINGS * sizeof(InputBindingRef));
    if (def->axisCount)
        memcpy(s_state[ctx].axisBindings, legacy && def->legacyAxisDefaults ? def->legacyAxisDefaults : def->axisDefaults,
               def->axisCount * INPUT_MAX_AXIS_BINDINGS * sizeof(InputAxisBindingRef));
    s_state[ctx].initialised = 1;
}

/* one physical input, from the devices InputMgr polled this frame */
static u8 Input_PhysicalDown(const InputBindingRef *b)
{
    InputDevice *kb = (InputDevice *)g_inputMgr.keyboard;
    InputDevice *mouse = (InputDevice *)g_inputMgr.device3;
    InputDevice *pad = (InputDevice *)g_inputMgr.joystick;
    u8 padOk = pad && (g_inputMgr.presentMask & INPUTDEV_MASK_JOYSTICK);
    switch (b->source) {
        case SRC_KEY:
            return kb && b->code < kb->buttonCount && kb->buttonDown[b->code];
        case SRC_MOUSE:
            return mouse && (g_inputMgr.presentMask & INPUTDEV_MASK_MOUSE) && b->code < mouse->buttonCount &&
                   mouse->buttonDown[b->code];
        case SRC_PAD_BUTTON:
            return padOk && b->code < pad->buttonCount && pad->buttonDown[b->code];
        case SRC_PAD_DPAD:
            return padOk && (Platform_ReadJoystickDpad() & b->code) != 0;
        case SRC_PAD_AXIS_POS:
            return padOk && (b->code == 0 ? pad->axisX : pad->axisY) > 0.5f;
        case SRC_PAD_AXIS_NEG:
            return padOk && (b->code == 0 ? pad->axisX : pad->axisY) < -0.5f;
    }
    return 0;
}

/* a stick (-1..1 floats) as the game reads one: to the pad's bytes, then Pad_AnalogToStick's round dead zone; +y up */
static u16 Input_Stick(float ax, float ay, s32 *x, s32 *y)
{
    s32 bx = (s32)(ax * 128.0f) + 0x80, by = (s32)(ay * 128.0f) + 0x80;
    s32 strength;
    bx = bx < 0 ? 0 : bx > 0xff ? 0xff : bx;
    by = by < 0 ? 0 : by > 0xff ? 0xff : by;
    strength = Pad_AnalogToStick((u8)bx, (u8)by, x, y);
    *y = -*y;
    return (u16)(strength > 256 ? 256 : strength);
}

/* four directions as ReadPad turns them into a vector: right over left, up over down, ±181 on a diagonal */
static u16 Input_Digital(u8 up, u8 right, u8 down, u8 left, s32 *x, s32 *y)
{
    *x = right ? 256 : left ? -256 : 0;
    *y = up ? 256 : down ? -256 : 0;
    if (*x && *y) {
        *x = *x > 0 ? 181 : -181;
        *y = *y > 0 ? 181 : -181;
    }
    return (*x || *y) ? 256 : 0;
}

static u8 Input_KeyDown(u16 code)
{
    InputDevice *kb = (InputDevice *)g_inputMgr.keyboard;
    return kb && code && code < kb->buttonCount && kb->buttonDown[code];
}

/* one axis binding: its vector and strength (stick-like sources), or its motion (the mouse) */
static u16 Input_AxisBindingValue(const InputAxisBindingRef *b, s32 *x, s32 *y, s32 *dx, s32 *dy)
{
    InputDevice *pad = (InputDevice *)g_inputMgr.joystick;
    InputDevice *mouse = (InputDevice *)g_inputMgr.device3;
    u8 padOk = pad && (g_inputMgr.presentMask & INPUTDEV_MASK_JOYSTICK);
    unsigned dpad;
    *x = *y = 0;
    switch (b->source) {
        case AXSRC_PAD_LEFT_STICK:
            return padOk ? Input_Stick(pad->axisX, pad->axisY, x, y) : 0;
        case AXSRC_PAD_RIGHT_STICK:
            return padOk ? Input_Stick(g_camInvertJoystick ? -pad->axisRX : pad->axisRX, pad->axisRY, x, y) : 0;
        case AXSRC_KEYS:
            return Input_Digital(Input_KeyDown(b->keys[0]), Input_KeyDown(b->keys[1]), Input_KeyDown(b->keys[2]),
                                 Input_KeyDown(b->keys[3]), x, y);
        case AXSRC_PAD_DPAD:
            if (!padOk)
                return 0;
            dpad = Platform_ReadJoystickDpad();
            return Input_Digital(dpad & PLATFORM_DPAD_UP, dpad & PLATFORM_DPAD_RIGHT, dpad & PLATFORM_DPAD_DOWN,
                                 dpad & PLATFORM_DPAD_LEFT, x, y);
        case AXSRC_MOUSE:
            if (mouse && (g_inputMgr.presentMask & INPUTDEV_MASK_MOUSE)) {
                *dx += mouse->rawX;
                *dy += mouse->rawY;
            }
            return 0;
    }
    return 0;
}

/* a context's axes this frame: the strongest stick-like binding, the sum of the mouse ones */
static void Input_UpdateAxes(u8 c)
{
    const InputContextDef *def = &s_contexts[c];
    u8 ax, k, i;
    if (def->followsMoveKeys >= 0) { /* the game's movement keys: keyTable up, right, down, left */
        InputAxisBindingRef *b = &s_state[c].axisBindings[def->followsMoveKeys][0];
        b->source = AXSRC_KEYS;
        for (i = 0; i < 4; i++) {
            const InputBinding *gb = &g_inputMgr.keyTable[INPUT_BIND_MENU_UP + i];
            b->keys[i] = gb->dev == (InputDevice *)g_inputMgr.keyboard ? gb->code : 0;
        }
    }
    for (ax = 0; ax < def->axisCount; ax++) {
        s32 bestX = 0, bestY = 0, dx = 0, dy = 0, x, y;
        u16 best = 0, strength;
        if (Input_IsTop(c))
            for (k = 0; k < INPUT_MAX_AXIS_BINDINGS; k++) {
                strength = Input_AxisBindingValue(&s_state[c].axisBindings[ax][k], &x, &y, &dx, &dy);
                if (strength > best) {
                    best = strength;
                    bestX = x;
                    bestY = y;
                }
            }
        s_state[c].axis[ax].x = bestX;
        s_state[c].axis[ax].y = bestY;
        s_state[c].axis[ax].strength = best;
        s_state[c].axis[ax].dx = dx;
        s_state[c].axis[ax].dy = dy;
    }
}

/* ---- the gameplay context's custom logic ----
 * Running by repeated taps on a movement key (the new controls; not with Legacy Controls): the same key pressed
 * GP_RUN_TAPS times, each within GP_TAP_GAP_MS of the one before, the last one then held. The game keeps running
 * while the run button is TAPPED (Wolf::RunTapCheck: each tap 0.1 to 0.75 s after the last keeps the run, a faster one
 * cancels it, none ends it), so while the key is held GA_RUN is pressed for one frame every GP_RUN_TAP_MS; letting the
 * key go stops the taps and the Wolf, no longer steered, stops.
 * Stopping: while the key run with is held, or within the game's run window after it was let go, pressing the opposite
 * movement key presses GA_STOP_RUN (the Wolf brakes instead of turning round at a run) and counts as no tap.
 * The movement keys are the Move axis's (its binding 0: the game's, InputMgr keyTable 12..15), so rebinding Move moves
 * this too. (The same logic as InputMgr_TapRun, which feeds the game's g_pad until ScnControllable::ReadPad reads the
 * actions.) */
#define GP_RUN_TAPS 3       /* the presses that start a run */
#define GP_TAP_GAP_MS 300   /* the longest gap between two of them */
#define GP_RUN_TAP_MS 400   /* inside the game's window of 0.1 to 0.75 s (WolfMoveBank params[5], params[6]) */
#define GP_RUN_AFTER_MS 750 /* the game's run window: a run lasts this long after the last run tap */
#define GP_NONE 0xff

static struct {
    u8 wasDown[4];  /* the movement keys last frame: up, right, down, left (the Move axis's order) */
    u8 tapSlot;     /* the movement key tapped last, when, and its presses in a row so far */
    s32 tapTime;
    u8 tapCount;
    u8 runSlot;     /* the key held after its taps: running while it is held; GP_NONE none */
    s32 runTapTime; /* when GA_RUN was last pressed for it */
    u8 dirSlot;     /* the key the run is in, kept after the key is let go, for the stop */
    s32 dirTime;    /* when that key was last held */
} s_tapRun = {{0, 0, 0, 0}, GP_NONE, 0, 0, GP_NONE, 0, GP_NONE, 0};

static void Gameplay_ForgetTapRun()
{
    s_tapRun.tapSlot = s_tapRun.runSlot = s_tapRun.dirSlot = GP_NONE;
    s_tapRun.tapCount = 0;
}

static void Gameplay_PostProcess(u8 ctx, u32 *down)
{
    const InputAxisBindingRef *keys = &s_state[ctx].axisBindings[GX_MOVE][0];
    u8 slot, held;
    s32 now = g_rawTimeMs;
    if (!Input_IsTop(ctx) || g_legacyControls || keys->source != AXSRC_KEYS) {
        Gameplay_ForgetTapRun(); /* a menu or a minigame on top: no run carried past it */
        for (slot = 0; slot < 4; slot++)
            s_tapRun.wasDown[slot] = 0;
        return;
    }
    for (slot = 0; slot < 4; slot++) {
        held = Input_KeyDown(keys->keys[slot]);
        if (held && !s_tapRun.wasDown[slot]) {
            if (s_tapRun.dirSlot != GP_NONE && slot == ((s_tapRun.dirSlot + 2) & 3) && now >= s_tapRun.dirTime &&
                now - s_tapRun.dirTime <= GP_RUN_AFTER_MS) {
                *down |= 1u << GA_STOP_RUN; /* the opposite key: stop, and no tap counted from this press */
                Gameplay_ForgetTapRun();
            } else {
                if (slot == s_tapRun.tapSlot && now - s_tapRun.tapTime >= 0 && now - s_tapRun.tapTime <= GP_TAP_GAP_MS)
                    s_tapRun.tapCount++;
                else
                    s_tapRun.tapCount = 1; /* another key, or too late: counting again from this press */
                if (s_tapRun.tapCount >= GP_RUN_TAPS) {
                    s_tapRun.runSlot = s_tapRun.dirSlot = slot;
                    s_tapRun.runTapTime = now - GP_RUN_TAP_MS; /* the first tap at once */
                    s_tapRun.tapCount = 0;
                }
                s_tapRun.tapSlot = slot;
                s_tapRun.tapTime = now;
            }
        }
        s_tapRun.wasDown[slot] = held;
    }
    if (s_tapRun.runSlot != GP_NONE && !Input_KeyDown(keys->keys[s_tapRun.runSlot]))
        s_tapRun.runSlot = GP_NONE; /* let go: no more taps */
    if (s_tapRun.runSlot == GP_NONE)
        return;
    s_tapRun.dirTime = now; /* the opposite key stops the run until GP_RUN_AFTER_MS after this */
    if (now - s_tapRun.runTapTime >= GP_RUN_TAP_MS || now < s_tapRun.runTapTime) {
        s_tapRun.runTapTime = now;
        *down |= 1u << GA_RUN; /* one frame: a tap */
    }
}

static u8 Input_IsPadSource(u8 source)
{
    return source == SRC_PAD_BUTTON || source == SRC_PAD_DPAD || source == SRC_PAD_AXIS_POS ||
           source == SRC_PAD_AXIS_NEG;
}

/* the platform's controller kinds, as the families bindings name */
static u8 Input_FamilyOfPad(int platformFamily)
{
    switch (platformFamily) {
        case PLATFORM_PAD_XBOX360:
        case PLATFORM_PAD_XBOXONE:
            return INPUT_FAMILY_XBOX;
        case PLATFORM_PAD_PS3:
        case PLATFORM_PAD_PS4:
        case PLATFORM_PAD_PS5:
            return INPUT_FAMILY_PLAYSTATION;
        case PLATFORM_PAD_SWITCH_PRO:
        case PLATFORM_PAD_JOYCON_SINGLE:
        case PLATFORM_PAD_JOYCON_PAIR:
        case PLATFORM_PAD_GAMECUBE:
            return INPUT_FAMILY_NINTENDO;
        case PLATFORM_PAD_GENERIC:
            return INPUT_FAMILY_GENERIC;
        default:
            return INPUT_FAMILY_ANY;
    }
}

/* the action has controller bindings of its own for the family in use: they replace its INPUT_FAMILY_ANY ones */
static u8 Input_ActionHasFamilyBindings(u8 ctx, u8 action)
{
    u8 k;
    if (s_padFamily == INPUT_FAMILY_ANY)
        return 0;
    for (k = 0; k < INPUT_MAX_BINDINGS; k++) {
        const InputBindingRef *b = &s_state[ctx].bindings[action][k];
        if (Input_IsPadSource(b->source) && b->family == s_padFamily)
            return 1;
    }
    return 0;
}

/* whether a binding applies to the controller in use: keyboard and mouse ones always; a controller one when it is for
 * this family, or for any family and the action has none for this one */
static u8 Input_BindingApplies(const InputBindingRef *b, u8 familyOverride)
{
    if (!Input_IsPadSource(b->source))
        return 1;
    if (b->family == INPUT_FAMILY_ANY)
        return !familyOverride;
    return b->family == s_padFamily;
}

void Input_UpdateActions()
{
    u8 c, a, k, over;
    s_padFamily = Input_FamilyOfPad(Platform_JoystickFamily());
    for (c = 0; c < ICTX_COUNT; c++) {
        const InputContextDef *def = &s_contexts[c];
        u32 down = 0;
        if (!s_state[c].initialised) {
            Input_ResetBindings(c);
            Input_LoadBindings(c); /* the player's, when saved */
        }
        if (def->followsSlot)
            Input_Follow(c);
        Input_UpdateAxes(c);
        if (Input_IsTop(c))
            for (a = 0; a < def->actionCount; a++) {
                over = Input_ActionHasFamilyBindings(c, a);
                for (k = 0; k < 2; k++) /* the game's, any family */
                    if (Input_BindingApplies(&s_state[c].followed[a][k], over) &&
                        Input_PhysicalDown(&s_state[c].followed[a][k]))
                        down |= 1u << a;
                for (k = 0; k < INPUT_MAX_BINDINGS; k++)
                    if (Input_BindingApplies(&s_state[c].bindings[a][k], over) &&
                        Input_PhysicalDown(&s_state[c].bindings[a][k]))
                        down |= 1u << a;
            }
        if (def->postProcess)
            def->postProcess(c, &down);
        s_state[c].swallowed &= down; /* let go: no longer swallowed */
        s_state[c].prev = s_state[c].cur;
        s_state[c].cur = down & ~s_state[c].swallowed;
        s_state[c].repeated = 0;
        if (def->autoRepeat)
            for (a = 0; a < def->actionCount; a++) {
                if (!(s_state[c].cur >> a & 1)) {
                    s_state[c].repeatMs[a] = INPUT_REPEAT_FIRST_MS;
                } else if (!(s_state[c].prev >> a & 1)) {
                    s_state[c].repeated |= 1u << a; /* the press itself */
                    s_state[c].repeatMs[a] = INPUT_REPEAT_FIRST_MS;
                } else if ((s_state[c].repeatMs[a] -= g_dtRawMs) <= 0) {
                    s_state[c].repeated |= 1u << a;
                    s_state[c].repeatMs[a] += INPUT_REPEAT_NEXT_MS;
                }
            }
    }
}

/* ---- the stack ---- */

void Input_PushContext(u8 ctx)
{
    if (!Input_ValidContext(ctx) || s_depth >= INPUT_STACK_MAX)
        return;
    s_stack[s_depth++] = ctx;
    s_state[ctx].swallowed = ~0u; /* whatever is held now belongs to the context below */
    s_state[ctx].cur = s_state[ctx].prev = 0;
}

/* removes the topmost entry of ctx, wherever it is (a mine reset while the pause menu is above it); only uncovering
 * the top swallows what is held. Nothing when ctx is not on the stack, or is the bottom one. */
void Input_PopContext(u8 ctx)
{
    u8 i, j;
    for (i = s_depth - 1; i >= 1; i--)
        if (s_stack[i] == ctx)
            break;
    if (i < 1)
        return;
    for (j = i; j + 1 < s_depth; j++)
        s_stack[j] = s_stack[j + 1];
    s_depth--;
    if (i == s_depth)
        s_state[s_stack[s_depth - 1]].swallowed = ~0u; /* the press that closed it does nothing below */
}

void Input_ResetContexts()
{
    while (s_depth > 1)
        Input_PopContext(Input_TopContext());
}

u8 Input_PadInUse()
{
    return g_inputMgr.joystick && g_inputMgr.current == (InputDevice *)g_inputMgr.joystick;
}

u8 Input_TopContext()
{
    return s_stack[s_depth - 1];
}

u8 Input_IsTop(u8 ctx)
{
    return Input_TopContext() == ctx;
}

/* ---- queries ---- */

u8 Input_Held(u8 ctx, u8 a)
{
    return Input_ValidContext(ctx) && a < 32 && (s_state[ctx].cur >> a & 1);
}

u8 Input_Pressed(u8 ctx, u8 a)
{
    return Input_ValidContext(ctx) && a < 32 && ((s_state[ctx].cur & ~s_state[ctx].prev) >> a & 1);
}

u8 Input_Released(u8 ctx, u8 a)
{
    return Input_ValidContext(ctx) && a < 32 && ((~s_state[ctx].cur & s_state[ctx].prev) >> a & 1);
}

u8 Input_Repeat(u8 ctx, u8 a)
{
    return Input_ValidContext(ctx) && a < 32 && (s_state[ctx].repeated >> a & 1);
}

/* ---- bindings ---- */

u8 Input_PadFamily()
{
    return s_padFamily;
}

/* the context's own bindings first (its table: the new scheme), then the game's it follows (the bridge); for the
 * controller, the ones that apply to its family (the same rule as the input) */
u8 Input_FindBinding(u8 ctx, u8 action, u8 wantPad, InputBindingRef *out)
{
    u8 k, over;
    if (!Input_ValidContext(ctx) || action >= s_contexts[ctx].actionCount)
        return 0;
    over = Input_ActionHasFamilyBindings(ctx, action);
    for (k = 0; k < INPUT_MAX_BINDINGS; k++) {
        const InputBindingRef *b = &s_state[ctx].bindings[action][k];
        if (b->source != SRC_NONE && Input_IsPadSource(b->source) == (wantPad != 0) && Input_BindingApplies(b, over)) {
            *out = *b;
            return 1;
        }
    }
    for (k = 0; k < 2; k++) {
        const InputBindingRef *b = &s_state[ctx].followed[action][k];
        if (b->source != SRC_NONE && Input_IsPadSource(b->source) == (wantPad != 0) && Input_BindingApplies(b, over)) {
            *out = *b;
            return 1;
        }
    }
    return 0;
}

/* one of the context's own bindings (the followed ones are the game's, set in the game's controls) */
void Input_Rebind(u8 ctx, u8 action, u8 slot, InputBindingRef b)
{
    if (!Input_ValidContext(ctx) || action >= s_contexts[ctx].actionCount || slot >= INPUT_MAX_BINDINGS)
        return;
    if (!s_state[ctx].initialised)
        Input_ResetBindings(ctx);
    s_state[ctx].bindings[action][slot] = b;
}

/* ---- the player's keys: the controls page (pause_menu.cpp) ----
 * One keyboard / mouse binding per action and four movement keys, as the page shows them; the controller is not
 * rebound (its buttons are by position). A key already used by another action or direction moves to the old key of
 * the one rebound (a swap), so no two share a key and none is left without one. */

static u8 Input_IsKeySource(u8 source)
{
    return source == SRC_KEY || source == SRC_MOUSE;
}

u8 Input_KeyBinding(u8 ctx, u8 action, InputBindingRef *out)
{
    u8 k;
    if (!Input_ValidContext(ctx) || action >= s_contexts[ctx].actionCount)
        return 0;
    for (k = 0; k < INPUT_MAX_BINDINGS; k++)
        if (Input_IsKeySource(s_state[ctx].bindings[action][k].source)) {
            *out = s_state[ctx].bindings[action][k];
            return 1;
        }
    return 0;
}

/* the movement keys: the first four-key binding of the context's first axis (gameplay: GX_MOVE) */
static InputAxisBindingRef *Input_MoveKeys(u8 ctx)
{
    u8 k;
    if (!Input_ValidContext(ctx) || !s_contexts[ctx].axisCount)
        return 0;
    for (k = 0; k < INPUT_MAX_AXIS_BINDINGS; k++)
        if (s_state[ctx].axisBindings[0][k].source == AXSRC_KEYS)
            return &s_state[ctx].axisBindings[0][k];
    return 0;
}

u16 Input_MoveKey(u8 ctx, u8 direction)
{
    InputAxisBindingRef *keys = Input_MoveKeys(ctx);
    return keys && direction < 4 ? keys->keys[direction] : 0;
}

/* whoever holds `taken` (an action other than `exceptAction`, a direction other than `exceptDirection`) gets `old` */
static void Input_GiveOldKey(u8 ctx, InputBindingRef taken, InputBindingRef old, u8 exceptAction, u8 exceptDirection)
{
    InputAxisBindingRef *keys = Input_MoveKeys(ctx);
    u8 a, k;
    for (a = 0; a < s_contexts[ctx].actionCount; a++) {
        if (a == exceptAction)
            continue;
        for (k = 0; k < INPUT_MAX_BINDINGS; k++) {
            InputBindingRef *b = &s_state[ctx].bindings[a][k];
            if (b->source == taken.source && b->code == taken.code)
                *b = old;
        }
    }
    if (keys && taken.source == SRC_KEY)
        for (k = 0; k < 4; k++)
            if (k != exceptDirection && keys->keys[k] == taken.code)
                keys->keys[k] = old.source == SRC_KEY ? old.code : 0; /* a mouse button cannot be a movement key */
}

void Input_RebindKey(u8 ctx, u8 action, InputBindingRef key)
{
    InputBindingRef old = NONE;
    u8 k, placed = 0;
    if (!Input_ValidContext(ctx) || action >= s_contexts[ctx].actionCount || !Input_IsKeySource(key.source))
        return;
    key.family = INPUT_FAMILY_ANY;
    Input_KeyBinding(ctx, action, &old);
    if (old.source == key.source && old.code == key.code)
        return;
    Input_GiveOldKey(ctx, key, old, action, 0xff);
    /* the action's keys: the new one in place of the first, the others cleared (the page shows one) */
    for (k = 0; k < INPUT_MAX_BINDINGS; k++) {
        InputBindingRef *b = &s_state[ctx].bindings[action][k];
        if (!Input_IsKeySource(b->source))
            continue;
        if (!placed) {
            *b = key;
            placed = 1;
        } else {
            b->source = SRC_NONE;
        }
    }
    for (k = 0; k < INPUT_MAX_BINDINGS && !placed; k++)
        if (s_state[ctx].bindings[action][k].source == SRC_NONE) {
            s_state[ctx].bindings[action][k] = key;
            placed = 1;
        }
}

void Input_RebindMoveKey(u8 ctx, u8 direction, u16 dik)
{
    InputAxisBindingRef *keys = Input_MoveKeys(ctx);
    InputBindingRef taken = {SRC_KEY, dik, INPUT_FAMILY_ANY}, old = {SRC_KEY, 0, INPUT_FAMILY_ANY};
    if (!keys || direction >= 4 || keys->keys[direction] == dik)
        return;
    old.code = keys->keys[direction];
    if (!old.code)
        old.source = SRC_NONE;
    Input_GiveOldKey(ctx, taken, old, 0xff, direction);
    keys->keys[direction] = dik;
}

/* ---- saving the player's bindings: CfgGame\<saveName>, binary ----
 * u8 version, action count, bindings per action, axis count, bindings per axis; then every binding as u8 source,
 * u8 family, u16 code; then every axis binding as u8 source, u8 0, u16 keys[4]. A save that does not match the
 * context's shape (another version of the game) is ignored: the defaults stay. */
#define INPUT_SAVE_VERSION 1
#define INPUT_SAVE_MAX (5 + INPUT_MAX_ACTIONS * INPUT_MAX_BINDINGS * 4 + INPUT_MAX_AXES * INPUT_MAX_AXIS_BINDINGS * 10)

static void Input_PutU16(u8 *p, u16 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

static u16 Input_GetU16(const u8 *p)
{
    return (u16)(p[0] | p[1] << 8);
}

u8 Input_SaveBindings(u8 ctx)
{
    const InputContextDef *def;
    u8 data[INPUT_SAVE_MAX], *p = data, a, k, i;
    SaveKey *key;
    if (!Input_ValidContext(ctx) || !(def = &s_contexts[ctx])->saveName || !(key = Save_OpenKey("CfgGame")))
        return 0;
    *p++ = INPUT_SAVE_VERSION;
    *p++ = def->actionCount;
    *p++ = INPUT_MAX_BINDINGS;
    *p++ = def->axisCount;
    *p++ = INPUT_MAX_AXIS_BINDINGS;
    for (a = 0; a < def->actionCount; a++)
        for (k = 0; k < INPUT_MAX_BINDINGS; k++, p += 4) {
            p[0] = s_state[ctx].bindings[a][k].source;
            p[1] = s_state[ctx].bindings[a][k].family;
            Input_PutU16(p + 2, s_state[ctx].bindings[a][k].code);
        }
    for (a = 0; a < def->axisCount; a++)
        for (k = 0; k < INPUT_MAX_AXIS_BINDINGS; k++, p += 10) {
            p[0] = s_state[ctx].axisBindings[a][k].source;
            p[1] = 0;
            for (i = 0; i < 4; i++)
                Input_PutU16(p + 2 + i * 2, s_state[ctx].axisBindings[a][k].keys[i]);
        }
    Save_SetValue(key, def->saveName, 3 /* REG_BINARY */, data, (unsigned)(p - data));
    Save_CloseKey(key);
    return 1;
}

u8 Input_LoadBindings(u8 ctx)
{
    const InputContextDef *def;
    u8 data[INPUT_SAVE_MAX];
    const u8 *p = data;
    unsigned type = 0, size = sizeof(data);
    u8 a, k, i, ok;
    SaveKey *key;
    if (!Input_ValidContext(ctx) || !(def = &s_contexts[ctx])->saveName || !Save_KeyExists("CfgGame") ||
        !(key = Save_OpenKey("CfgGame")))
        return 0;
    ok = Save_QueryValue(key, def->saveName, &type, data, &size) == SAVE_OK && type == 3 && size >= 5 &&
         data[0] == INPUT_SAVE_VERSION && data[1] == def->actionCount && data[2] == INPUT_MAX_BINDINGS &&
         data[3] == def->axisCount && data[4] == INPUT_MAX_AXIS_BINDINGS &&
         size == 5u + def->actionCount * INPUT_MAX_BINDINGS * 4u + def->axisCount * INPUT_MAX_AXIS_BINDINGS * 10u;
    Save_CloseKey(key);
    if (!ok)
        return 0;
    p += 5;
    for (a = 0; a < def->actionCount; a++)
        for (k = 0; k < INPUT_MAX_BINDINGS; k++, p += 4) {
            s_state[ctx].bindings[a][k].source = p[0];
            s_state[ctx].bindings[a][k].family = p[1];
            s_state[ctx].bindings[a][k].code = Input_GetU16(p + 2);
        }
    for (a = 0; a < def->axisCount; a++)
        for (k = 0; k < INPUT_MAX_AXIS_BINDINGS; k++, p += 10) {
            s_state[ctx].axisBindings[a][k].source = p[0];
            for (i = 0; i < 4; i++)
                s_state[ctx].axisBindings[a][k].keys[i] = Input_GetU16(p + 2 + i * 2);
        }
    return 1;
}

void Input_ForgetBindings(u8 ctx)
{
    SaveKey *key;
    u8 none = 0; /* version 0: read as no save */
    if (!Input_ValidContext(ctx))
        return;
    if (s_contexts[ctx].saveName && (key = Save_OpenKey("CfgGame")) != 0) {
        Save_SetValue(key, s_contexts[ctx].saveName, 3 /* REG_BINARY */, &none, 1);
        Save_CloseKey(key);
    }
    Input_ResetBindings(ctx);
}

const char *Input_ContextName(u8 ctx)
{
    return Input_ValidContext(ctx) ? s_contexts[ctx].name : "?";
}

u8 Input_ActionCount(u8 ctx)
{
    return Input_ValidContext(ctx) ? s_contexts[ctx].actionCount : 0;
}

const char *Input_ActionName(u8 ctx, u8 action)
{
    return action < Input_ActionCount(ctx) ? s_contexts[ctx].actionNames[action] : "?";
}

u16 Input_Axis2D(u8 ctx, u8 axis, s32 *x, s32 *y)
{
    if (!Input_ValidContext(ctx) || axis >= s_contexts[ctx].axisCount) {
        *x = *y = 0;
        return 0;
    }
    *x = s_state[ctx].axis[axis].x;
    *y = s_state[ctx].axis[axis].y;
    return s_state[ctx].axis[axis].strength;
}

void Input_AxisDelta(u8 ctx, u8 axis, s32 *dx, s32 *dy)
{
    if (!Input_ValidContext(ctx) || axis >= s_contexts[ctx].axisCount) {
        *dx = *dy = 0;
        return;
    }
    *dx = s_state[ctx].axis[axis].dx;
    *dy = s_state[ctx].axis[axis].dy;
}

u8 Input_AxisCount(u8 ctx)
{
    return Input_ValidContext(ctx) ? s_contexts[ctx].axisCount : 0;
}

const char *Input_AxisName(u8 ctx, u8 axis)
{
    return axis < Input_AxisCount(ctx) ? s_contexts[ctx].axisNames[axis] : "?";
}

/* ---- debugging ---- */

void Input_DebugLine(char *out, int size)
{
    u8 top = Input_TopContext(), a;
    static const char *const familyTags[] = {"", " XB", " PS", " NIN", " PAD"};
    int n = snprintf(out, size, "IN %u:%s%s", (unsigned)s_depth, s_contexts[top].name,
                     s_padFamily < sizeof(familyTags) / sizeof(familyTags[0]) ? familyTags[s_padFamily] : "");
    if (s_contexts[top].axisCount && s_state[top].axis[0].strength)
        n += snprintf(out + n, size - n, " %s %d %d", s_contexts[top].axisNames[0], (int)s_state[top].axis[0].x,
                      (int)s_state[top].axis[0].y);
    for (a = 0; a < s_contexts[top].actionCount && n > 0 && n < size; a++)
        if (s_state[top].cur >> a & 1)
            n += snprintf(out + n, size - n, " %s", s_contexts[top].actionNames[a]);
}

void Input_DebugCycleContexts()
{
    switch (Input_TopContext()) {
        case ICTX_GAMEPLAY:
            Input_PushContext(ICTX_MENU);
            break;
        case ICTX_MENU:
            Input_PushContext(ICTX_DEFUSE);
            break;
        default:
            Input_ResetContexts();
    }
}
