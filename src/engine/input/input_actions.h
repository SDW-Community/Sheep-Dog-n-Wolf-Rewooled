/* Input contexts and actions (input_actions.cpp; the plan: docs/input-contexts-plan.md).
 *
 * Readers ask whether an ACTION of a CONTEXT is pressed, never which button: Input_Pressed(ICTX_MENU, MA_CONFIRM).
 * Only the context on top of the stack sees input. Filled once a frame from the devices InputMgr polled (end of
 * Input_Poll); nothing reads it yet - the game still reads g_pad - so it can be watched (F3 overlay, F6 cycles the
 * stack) before any reader moves over. */
#ifndef SDW_INPUT_ACTIONS_H
#define SDW_INPUT_ACTIONS_H

#include "sdw_types.h"

/* who owns the input */
enum InputContextId { ICTX_GAMEPLAY, ICTX_MENU, ICTX_DEFUSE, ICTX_CINEMATIC, ICTX_DANCE, ICTX_CANNON, ICTX_COUNT};

/* each context's own actions */
enum GameplayAction {
    GA_JUMP,
    GA_RUN,
    GA_ACTION,
    GA_SNEAK,
    GA_LOOK,
    GA_ITEMWHEEL,
    GA_CAM_LEFT,
    GA_CAM_RIGHT,
    GA_MAP,
    GA_PAUSE,
    GA_STOP_RUN, /* no binding: the gameplay hook presses it when the movement key opposite a tap-run is pressed */
    GA_COUNT
};
enum MenuAction { MA_UP, MA_DOWN, MA_LEFT, MA_RIGHT, MA_CONFIRM, MA_BACK, MA_COUNT };
enum DefuseAction { DA_TOP, DA_RIGHT, DA_LEFT, DA_BOTTOM, DA_COUNT }; /* the mine's four symbols, as on the pad */
enum CinematicAction { CA_SKIP, CA_COUNT };
enum DanceAction { DN_TOP, DN_RIGHT, DN_LEFT, DN_BOTTOM, DN_COUNT }; /* Level 12's dance steps (the ghosts' track), as on the pad */
enum CannonAction { CN_FIRE, CN_EXIT, CN_COUNT }; /* CanonSimple's aiming */

#define INPUT_MAX_ACTIONS 16
#define INPUT_MAX_BINDINGS 4 /* per action: e.g. two keys, a controller button and the d-pad */

/* one physical input */
enum InputSource {
    SRC_NONE,
    SRC_KEY,          /* code: DIK_* */
    SRC_MOUSE,        /* code: 0 left, 1 right, 2 middle, 3 X1 */
    SRC_PAD_BUTTON,   /* code: PlayStation slot INPUT_BIND_* (the controller's buttons come in that order) */
    SRC_PAD_DPAD,     /* code: PLATFORM_DPAD_* */
    SRC_PAD_AXIS_POS, /* code: 0 left stick X, 1 left stick Y (down), past half way */
    SRC_PAD_AXIS_NEG,
};
/* the controller families a binding can be limited to (InputBindingRef.family) */
enum InputPadFamily {
    INPUT_FAMILY_ANY,         /* every controller: the default */
    INPUT_FAMILY_XBOX,        /* Xbox 360, Xbox One / Series */
    INPUT_FAMILY_PLAYSTATION, /* DualShock, DualSense */
    INPUT_FAMILY_NINTENDO,    /* Switch Pro, Joy-Con, GameCube */
    INPUT_FAMILY_GENERIC,     /* a controller SDL knows without its maker's layout */
};

/* the face buttons by POSITION, for SRC_PAD_BUTTON: the same physical place on every family (south is Cross on a
 * PlayStation pad, A on an Xbox pad, B on a Nintendo pad) */
#define INPUT_PAD_SOUTH INPUT_BIND_CROSS
#define INPUT_PAD_EAST INPUT_BIND_CIRCLE
#define INPUT_PAD_WEST INPUT_BIND_SQUARE
#define INPUT_PAD_NORTH INPUT_BIND_TRIANGLE

struct InputBindingRef {
    u8 source; /* InputSource */
    u16 code;
    u8 family; /* controller sources only: INPUT_FAMILY_*. When an action has controller bindings for the family in
                * use, only those count; otherwise its INPUT_FAMILY_ANY ones do */
};

/* bindings written as they read, for tables: INPUT_KEY(DIK_LEFT), INPUT_PAD(INPUT_PAD_SOUTH),
 * INPUT_PAD_ON(INPUT_FAMILY_NINTENDO, INPUT_PAD_EAST), INPUT_DPAD(PLATFORM_DPAD_UP), INPUT_MOUSE(0) */
#define INPUT_KEY(dik) {SRC_KEY, dik, INPUT_FAMILY_ANY}
#define INPUT_MOUSE(button) {SRC_MOUSE, button, INPUT_FAMILY_ANY}
#define INPUT_PAD(slot) {SRC_PAD_BUTTON, slot, INPUT_FAMILY_ANY}
#define INPUT_PAD_ON(family, slot) {SRC_PAD_BUTTON, slot, family}
#define INPUT_DPAD(direction) {SRC_PAD_DPAD, direction, INPUT_FAMILY_ANY}
#define INPUT_DPAD_ON(family, direction) {SRC_PAD_DPAD, direction, family}

/* ---- axes: movement and looking, analog ----
 * An action is on or off; an axis is a direction and a strength. Input_Axis2D gives the stick-like vector of the
 * strongest of the axis's bindings this frame - a stick through the game's round dead zone (Pad_AnalogToStick), or
 * four keys / the d-pad as ±256 (±181 on a diagonal), as ScnControllable::ReadPad builds it. Input_AxisDelta gives the
 * mouse's motion this frame for an axis bound to the mouse (the orbit camera's two inputs, kept apart). */
enum GameplayAxis { GX_MOVE, GX_LOOK, GX_COUNT };
/* the cannon: CX_AIM the sticks and the mouse (a speed / a distance), CX_STEP the movement keys and the d-pad (the
 * disc's d-pad aiming, which speeds up while held) */
enum CannonAxis { CX_AIM, CX_STEP, CX_COUNT };

#define INPUT_MAX_AXES 4
#define INPUT_MAX_AXIS_BINDINGS 3

enum InputAxisSource {
    AXSRC_NONE,
    AXSRC_PAD_LEFT_STICK,
    AXSRC_PAD_RIGHT_STICK, /* the camera's: left / right inverted with the Invert Camera Joystick setting */
    AXSRC_KEYS,            /* keys[4]: DIK_* up, right, down, left */
    AXSRC_PAD_DPAD,
    AXSRC_MOUSE,           /* Input_AxisDelta only */
};
struct InputAxisBindingRef {
    u8 source; /* InputAxisSource */
    u16 keys[4];
};

/* x, y in -256..256, +x right, +y forward / up (ReadPad's stickX / stickY); returns the strength 0..256 (stickMag).
 * 0 and 0, 0 inside the dead zone, or for a context not on top */
u16 Input_Axis2D(u8 ctx, u8 axis, s32 *x, s32 *y);
/* the mouse's motion this frame in pixels (+x right, +y down), for an axis with a mouse binding; 0, 0 otherwise */
void Input_AxisDelta(u8 ctx, u8 axis, s32 *dx, s32 *dy);
u8 Input_AxisCount(u8 ctx);
const char *Input_AxisName(u8 ctx, u8 axis);

/* the stack: the top context gets the input; what is held when a context is pushed (or uncovered by a pop) is
 * ignored by it until released, so the press that opened or closed something does nothing else */
void Input_PushContext(u8 ctx);
void Input_PopContext(u8 ctx); /* the topmost entry of ctx, wherever it is; nothing when it is not on the stack */
void Input_ResetContexts();    /* back to Gameplay alone: every level load (Game_ReloadLevel) */
u8 Input_IsTop(u8 ctx);
u8 Input_TopContext();

/* the controller is the device in use (the last one used): prompts show its buttons, else the keyboard's */
u8 Input_PadInUse();
/* the connected controller's family (INPUT_FAMILY_*); INPUT_FAMILY_ANY when there is none */
u8 Input_PadFamily();

/* this frame, for an action of a context (0 for a context that is not on top) */
u8 Input_Held(u8 ctx, u8 action);
u8 Input_Pressed(u8 ctx, u8 action);  /* the press edge */
u8 Input_Released(u8 ctx, u8 action); /* the release edge */
u8 Input_Repeat(u8 ctx, u8 action);   /* the press, then repeats while held (contexts with autoRepeat: menus) */

/* bindings: the first of an action's bindings on the keyboard / mouse (wantPad 0) or the controller (wantPad 1),
 * for prompts and a controls page; 1 when there is one */
u8 Input_FindBinding(u8 ctx, u8 action, u8 wantPad, InputBindingRef *out);
void Input_Rebind(u8 ctx, u8 action, u8 slot, InputBindingRef b);
void Input_ResetBindings(u8 ctx); /* back to the context's defaults (the legacy ones with Legacy Controls on) */

/* the player's keys (the pause menu's controls page): one keyboard / mouse binding per action, four movement keys;
 * a key already in use moves to the old key of the one rebound (a swap) */
u8 Input_KeyBinding(u8 ctx, u8 action, InputBindingRef *out); /* the action's keyboard / mouse binding */
u16 Input_MoveKey(u8 ctx, u8 direction);                       /* DIK_* of direction 0 up, 1 right, 2 down, 3 left */
void Input_RebindKey(u8 ctx, u8 action, InputBindingRef key);  /* key: SRC_KEY or SRC_MOUSE */
void Input_RebindMoveKey(u8 ctx, u8 direction, u16 dik);
/* the bindings saved in the save files (CfgGame), for the contexts that are saved (gameplay); loaded at start-up.
 * Forget: the save cleared and the defaults back */
u8 Input_SaveBindings(u8 ctx);
u8 Input_LoadBindings(u8 ctx);
void Input_ForgetBindings(u8 ctx);

/* a context's name and its actions' names (controls page, overlay) */
const char *Input_ContextName(u8 ctx);
u8 Input_ActionCount(u8 ctx);
const char *Input_ActionName(u8 ctx, u8 action);

void Input_UpdateActions();             /* once a frame, after InputMgr::Poll (Input_Poll) */
void Input_DebugLine(char *out, int size); /* the F3 overlay's line: the top context and its held actions */
void Input_DebugCycleContexts();        /* F6: Gameplay -> + Menu -> + Defuse -> back to Gameplay */

#endif
