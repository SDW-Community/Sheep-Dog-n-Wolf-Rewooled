
#include "sdw_types.h"
#include "../engine/screen.h"
#include "sdw_enums.h"
#include "scenaric_props.h"
#define SDW_MEMBERS_ScnObject                           \
    static void *operator new(uptr);                     \
    void SetRotation(Vec3s *angle);                     \
    void StartCamera(u16, u16, u16, Vec3s *, u16, u32); \
    void SetUpdateMode(s32 mode);
#include "sdw_classes.h"
#define SDW_INLINE_SCNOBJECT_GETCLASSID 1
#define SDW_INLINE_SCNOBJECT_SETROTATION_VEC3S 1
#define SDW_INLINE_SCNOBJECT_SETVISIBLE_S32 1
#include "../engine/scenaric_inlines.h"
#undef SDW_INLINE_SCNOBJECT_GETCLASSID
#undef SDW_INLINE_SCNOBJECT_SETROTATION_VEC3S
#undef SDW_INLINE_SCNOBJECT_SETVISIBLE_S32
#define SDW_INLINE_SCNOBJECT_SETUPDATEMODE_S32 1
#include "../engine/scenaric_inlines.h"
#undef SDW_INLINE_SCNOBJECT_SETUPDATEMODE_S32
extern Wolf *g_pWolf;
#include "../engine/input.h"
#include "../app/app_main.h"
#include "../engine/scn_tools.h"
#include "camera.h"
#include "../engine/interface.h"
#include "../engine/time.h"
extern s32 g_dtMs;
extern u32 *g_screenLayerBase;

#define g_cullPlaneNormal (g_camera.viewMatS.rot + 6)

s32 Scenaric_FindByClass(u16, ScnObject **, s32);
void Camera_StartScripted(ScnObject *, Camera *, u16, u16, u16, Vec3s *, u16, u32, s32);
#define SDW_INLINE_SCNOBJECT_STARTCAMERA_U16_U16_U16_VEC3S_U16_U32 1
#include "../engine/scenaric_inlines.h"
#undef SDW_INLINE_SCNOBJECT_STARTCAMERA_U16_U16_U16_VEC3S_U16_U32
#define SDW_INLINE_FREE_READINTPROPERTY_VOID_U32 1
#include "../engine/scn_tools_inlines.h"
#undef SDW_INLINE_FREE_READINTPROPERTY_VOID_U32
#define SDW_INLINE_FREE_BOXCONTAINSXZ_BOX_VEC3S 1
#include "../engine/coll_box_inlines.h"
#undef SDW_INLINE_FREE_BOXCONTAINSXZ_BOX_VEC3S
#define SDW_INLINE_FREE_SCREENWIDTHS32 1
#include "../engine/screen_inlines.h"
#undef SDW_INLINE_FREE_SCREENWIDTHS32
#define SDW_INLINE_FREE_SCREENHEIGHTS32 1
#include "../engine/screen_inlines.h"
#undef SDW_INLINE_FREE_SCREENHEIGHTS32
#include "../engine/input/input_actions.h"
#include "../platform/platform.h"
#include "../engine/hud/hud_prompt.h"
#include "../engine/text.h"
extern u32 g_uiTintColor;
u16 Str_Length(const char *);
/* the mouse's aim speed, a share of the orbit camera's sensitivity (g_camMouseSensitivity, angle units per pixel):
 * aiming wants a finer hand than looking around */
#define CS_MOUSE_SCALE 0.5f
/* the fractions of an angle unit the mouse moved and the aim has not taken yet; cleared when aiming starts */
static float s_mousePitch, s_mouseYaw;
/* the cannon whose aiming owns the input (ICTX_CANNON): pushed in CanonSimple::SetState, popped by CanonSimple_ReleaseInput */
static CanonSimple *s_cannonOwner;
/* leaving the aiming, whichever way (fired, left, frozen, reset): give the input back */
static void CanonSimple_ReleaseInput(CanonSimple *cannon)
{
    if (s_cannonOwner == cannon) {
        Input_PopContext(ICTX_CANNON);
        s_cannonOwner = 0;
    }
}
#define ABS_VALUE(v) ((v) >= 0 ? (v) : -(v))
void CanonSimple::PostLoadInit()
{
    void *props = record;
    {
        s32 b, a;
        s_cannonOwner = 0; /* a level left mid-aim: Input_ResetContexts already emptied the stack */
        dpadStep = 5;
        stickPitchStep = 0;
        stickYawStep = 0;
        stickRepeatMs = 0;
        dpadRepeatMs = 0;
        csFlags = 0;
        csFlags |= CSF_ANALOG_ACCEL;
        pipeCount = (u16)Scenaric_FindByClass(CLASSID_PIPE, pipes, 5);
        pipe2Count = (u16)Scenaric_FindByClass(CLASSID_PIPE2, pipes2, 5);
        state = CS_ST_IDLE;
        SnapToGround(0);
        basePos = pos;
        lastPos = basePos;
        activationBox = Scn_GetPropBox(props, 0);
        barrel = 0;
        barrel = Scn_GetPropObject(props, 0x10);
        barrel->SetPosition(&basePos);
        ball = 0;
        ball = Scn_GetPropObject(props, 0xc);
        ball->HandleMessage(this, MSG_CB_PLACE, &muzzlePos);
        MoveTo(&basePos);
        horzLimit = (u16)ReadIntProperty(props, 0x14);
        vertLimit = (u16)ReadIntProperty(props, 0x18);
        horzLimit = (horzLimit << 12) / 360;
        vertLimit = (vertLimit << 12) / 360;
        b = ReadIntProperty(props, 4);
        a = ReadIntProperty(props, 8);
        defaultAim.x = ((b * 0xfff) / 360) & 0xfff;
        defaultAim.y = ((a * 0xfff) / 360) & 0xfff;
        defaultAim.z = 0;
        aim = defaultAim;
        aimOffset.x = 0;
        aimOffset.y = 0;
        aimOffset.z = 0;
        ApplyAim(&aim);
        rumbleMotors[0] = 0xfe;
        rumbleMotors[1] = 0xff;
        SetUpdateMode(SCN_UPD_ALWAYS);
    }
}
void CanonSimple::Reset()
{
    CanonSimple_ReleaseInput(this);
    csFlags &= ~CSF_RUNTIME_MASK;
    csFlags &= ~(CSF_FIRE_ARMED | CSF_AIM_CAM_STARTED | CSF_AIM_CHANGED | CSF_WOLF_FROZEN);
    if (csFlags & CSF_RESET_AIM) {
        aim = defaultAim;
        aimOffset.x = 0;
        aimOffset.y = 0;
        aimOffset.z = 0;
        ApplyAim(&aim);
    }
    state = CS_ST_IDLE;
}
void CanonSimple::Update()
{
    switch (state) {
        case CS_ST_AIM:
            UpdateAim();
            Hud_DrawCannonMask();
            Hud_DrawCannonControls();
            break;
        case CS_ST_FIRING:
            fireDelayMs -= g_dtMs;
            if (fireDelayMs <= 0)
                SetState(CS_ST_FIRED);
            break;
    }
}

sptr CanonSimple::HandleMessage(ScnObject *sender, u32 msg, void *)
{
    if (!sender)
        return 0;
    if (sender->GetClassId() == CLASSID_WOLF) {
        switch (msg) {
            case MSG_QUERY_ACTION:
                switch (state) {
                    case CS_ST_IDLE:
                        if (BoxContainsXZ(activationBox, &g_pWolf->pos))
                            return CTX_CANNON;
                        return CTX_NONE;
                    case CS_ST_AIM:
                        return CTX_NONE;
                    case CS_ST_FIRED:
                        return CTX_NONE;
                }
                break;
            case MSG_USE:
                csFlags &= ~(CSF_ABORTED | CSF_FIRE_ARMED);
                SetState(CS_ST_AIM);
                return 1;
            case MSG_FREEZE:
                csFlags |= CSF_ABORTED;
                SetState(CS_ST_IDLE);
                ball->HandleMessage(this, MSG_CB_RELEASE_CAMERA, 0);
                return 1;
        }
    } else if (sender->GetClassId() == CLASSID_CANNONBALL) {
        switch (msg) {
            case MSG_BALL_RETURNED:
                ball->HandleMessage(this, MSG_CB_PLACE, &muzzlePos);
                if (!(csFlags & CSF_ABORTED))
                    SetState(CS_ST_IDLE);
                break;
            case MSG_FREEZE:
                if (!(csFlags & CSF_ABORTED) && (csFlags & CSF_WOLF_FROZEN)) {
                    csFlags &= ~CSF_WOLF_FROZEN;
                    g_pWolf->HandleMessage(this, MSG_UNFREEZE, 0);
                }
                return 1;
        }
    }
    return 0;
}
void CanonSimple::SetState(u8 next)
{
    u16 i;
    state = next;
    if (next == CS_ST_AIM) {
        /* aiming owns the input until it leaves; what is held now (the Action press that got Ralph in) is ignored
         * until released, so it cannot fire or leave at once */
        if (!s_cannonOwner) {
            Input_PushContext(ICTX_CANNON);
            s_cannonOwner = this;
            s_mousePitch = s_mouseYaw = 0;
        }
    } else
        CanonSimple_ReleaseInput(this);
    switch (next) {
        case CS_ST_IDLE:
            if (!(csFlags & CSF_ABORTED) && (csFlags & CSF_WOLF_FROZEN)) {
                csFlags &= ~CSF_WOLF_FROZEN;
                g_pWolf->HandleMessage(this, MSG_UNFREEZE, 0);
            }
            Reset();
            StartCamera(0, aim.y & 0xfff, 0, &muzzlePos, 800, 0);
            Camera_ReleaseScripted(this);
            g_pWolf->HandleMessage(this, MSG_WOLF_SET_INVISIBLE, (void *)1);
            SetVisible(1);
            barrel->SetVisible(1);
            break;
        case CS_ST_AIM:
            pitchRemainder = yawRemainder = 0;
            ball->HandleMessage(this, MSG_CB_SET_LETHAL, 0);
            g_pWolf->HandleMessage(this, MSG_FREEZE, 0);
            csFlags |= CSF_WOLF_FROZEN;
            csFlags &= ~CSF_AIM_CAM_STARTED;
            g_pWolf->HandleMessage(this, MSG_WOLF_SET_INVISIBLE, 0);
            SetVisible(0);
            barrel->SetVisible(0);
            break;
        case CS_ST_FIRED:
            for (i = 0; i < pipeCount; i++)
                pipes[i]->HandleMessage(this, MSG_PIPE_ARM, 0);
            for (i = 0; i < pipe2Count; i++)
                pipes2[i]->HandleMessage(this, MSG_PIPE_ARM, 0);
            g_pad.Rumble_stub(250, rumbleMotors, 0x1000);
            ball->HandleMessage(this, MSG_CB_SET_LETHAL, 0);
            ball->HandleMessage(this, MSG_CB_LAUNCH, &aim);
            break;
        case CS_ST_FIRING:
            SetVisible(1);
            barrel->SetVisible(1);
            g_pWolf->HandleMessage(this, MSG_WOLF_SET_INVISIBLE, (void *)1);
            barrel->HandleMessage(this, MSG_CD_RECOIL, 0);
            ball->HandleMessage(this, MSG_CB_SET_OWNS_CAMERA, (void *)1);
            ball->HandleMessage(this, MSG_CB_LOAD, &aim);
            fireDelayMs = 400;
            break;
    }
}
/* the aiming crosshair: four white bars around the screen's centre */
static void CanonSimple_DrawCrosshair()
{
    HudElement hud(HudElement::Centre, HudElement::Centre, HudElement::CentreScreen);
    s32 cx = ScreenWidthS32() >> 1;
    s32 cy = ScreenHeightS32() >> 1;
    Ui_DrawFlatRect(g_screenLayerBase, cx - 7, cy - 1, cx - 3, cy + 1, 0xffffff); /* left */
    Ui_DrawFlatRect(g_screenLayerBase, cx + 3, cy - 1, cx + 7, cy + 1, 0xffffff); /* right */
    Ui_DrawFlatRect(g_screenLayerBase, cx - 1, cy - 5, cx + 1, cy - 2, 0xffffff); /* top */
    Ui_DrawFlatRect(g_screenLayerBase, cx - 1, cy + 2, cx + 1, cy + 5, 0xffffff); /* bottom */
}

#define CS_PROMPT_GAP 6 /* as ACTION_PROMPT_ICON_GAP (scn_controllable.cpp): icon cell to label */

/* the cannons' controls (this one's aiming, CanonSheep's with Ralph inside): two crayon panels top-left, one under
 * the other, drawn as Ralph's action prompt (the button icon in the label's leading space, the text, the purple
 * panel behind) */
void Hud_DrawCannonControls()
{
    static const struct { u8 action; const char *label; } rows[2] = {{CN_FIRE, " FIRE"}, {CN_EXIT, " EXIT"}};
    HudElement hud(HudElement::Start, HudElement::Start);
    s32 i, top, width;
    Text_SetFont(FONT_GAME);
    for (i = 0; i < 2; i++) {
        top = 16 + i * 40; /* a 31-tall panel, 9 between */
        width = Str_Length(rows[i].label) * g_pCurFont->glyphWidth + CS_PROMPT_GAP;
        Text_SetWindow(g_screenLayerBase + 8, 32 + CS_PROMPT_GAP, top + 8, width + 20, 40, 1);
        Text_PrintFmt(rows[i].label);
        Hud_EndBox_stub();
        HudPrompt_DrawAction(ICTX_CANNON, rows[i].action, 32 + g_pCurFont->glyphWidth / 2,
                             top + 8 + g_pCurFont->lineHeight / 2, g_screenLayerBase + 7, 1);
        g_spriteCrayon2.Draw(g_screenLayerBase + 9, 16, top, width + 48, top + 31, g_uiTintColor, 0);
    }
}

/* the aim step of one stick axis this frame, for a value past the dead zone (-256..256, not 0). With
 * CSF_ANALOG_ACCEL it grows with the tilt (value / 30 just past the dead zone, value / 15 at full tilt, so it can
 * round to 0) and is only recomputed every 150 ms or when there is none cached; between, the cached step is used.
 * Without it the step is 1 in the stick's direction */
static s32 CanonSimple_StickStep(s32 value, s32 *cachedStep, u32 csFlags, u32 repeatMs)
{
    if (!(csFlags & CSF_ANALOG_ACCEL))
        return value >= 0 ? 1 : -1;
    if (repeatMs > 150 || !*cachedStep)
        *cachedStep = value / (30 - ABS_VALUE(value) * 15 / 256);
    return *cachedStep;
}

/* the aiming mode (CS_ST_AIM): the crosshair, the stick, the d-pad and the mouse turn the barrel, Fire shoots, Exit leaves
 * (ICTX_CANNON) */
void CanonSimple::UpdateAim()
{
    s32 stickX = 0, stickY = 0; /* after the stick block: this frame's analog steps, not the raw stick */
    s32 dpadPitch = 0;          /* -1 up, +1 down */
    s32 dpadYaw = 0;            /* -1 right, +1 left */
    s32 mouseX, mouseY;
    s32 stepX, stepY;
    float lookX, lookY;

    CanonSimple_DrawCrosshair();
    dpadRepeatMs += g_dtMs;
    stickRepeatMs += g_dtMs;

    /* the sticks (ICTX_CANNON's CX_AIM, Input_Axis2D: -256..256, +y up): Y pitches, X yaws (right turns the yaw down).
     * The disc's steps (CanonSimple_StickStep), so its y is turned to the pad's +y down */
    if (Input_Axis2D(ICTX_CANNON, CX_AIM, &stickX, &stickY))
        stickY = -stickY;
    if (!stickX && !stickY)
        stickRepeatMs = 0;
    if (stickY) {
        stickY = CanonSimple_StickStep(stickY, &stickPitchStep, csFlags, stickRepeatMs);
        AddAimOffset(&aimOffset.x, (s16)stickY, CS_AIM_PITCH);
        csFlags |= CSF_AIM_CHANGED;
    } else
        stickPitchStep = 0;
    if (stickX) {
        stickX = CanonSimple_StickStep(stickX, &stickYawStep, csFlags, stickRepeatMs);
        AddAimOffset(&aimOffset.y, (s16)-stickX, CS_AIM_YAW);
        csFlags |= CSF_AIM_CHANGED;
    } else
        stickYawStep = 0;
    if (stickRepeatMs > 150) /* the 150 ms are up (the steps were recomputed above): start the next ones */
        stickRepeatMs = 0;

    /* the mouse (ICTX_CANNON's CX_AIM): a distance, not a speed, so added as it is - not through AddAimOffset's
     * frame-time scaling - in the d-pad's directions (down aims down, right turns the yaw down); the fractions kept
     * for the next frames, so a slow hand still moves it */
    Input_AxisDelta(ICTX_CANNON, CX_AIM, &mouseX, &mouseY);
    if (mouseX || mouseY) {
        s_mousePitch += mouseY * g_camMouseSensitivity * CS_MOUSE_SCALE;
        s_mouseYaw -= mouseX * g_camMouseSensitivity * CS_MOUSE_SCALE;
        aimOffset.x += (s16)s_mousePitch;
        aimOffset.y += (s16)s_mouseYaw;
        s_mousePitch -= (s16)s_mousePitch;
        s_mouseYaw -= (s16)s_mouseYaw;
        ClampAimOffset(&aimOffset.x, CS_AIM_PITCH);
        ClampAimOffset(&aimOffset.y, CS_AIM_YAW);
        csFlags |= CSF_AIM_CHANGED;
    }
    /* the orbit camera keeps its own count of the mouse's motion (Platform_TakeMouseLook): emptied while aiming, or
     * it would turn the camera by all of it when Ralph gets out */
    Platform_TakeMouseLook(&lookX, &lookY);

    /* the movement keys and the d-pad (ICTX_CANNON's CX_STEP, Input_Axis2D: +x right, +y up): a direction each way,
     * up winning over down and right over left (Input_Digital) */
    Input_Axis2D(ICTX_CANNON, CX_STEP, &stepX, &stepY);
    dpadPitch = stepY > 0 ? -1 : stepY < 0 ? 1 : 0;
    dpadYaw = stepX > 0 ? -1 : stepX < 0 ? 1 : 0;
    /* while the stick gives no step, holding the d-pad speeds it up: 5, then +2 every 150 ms, up to 25; letting go
     * starts it over at 5. (A stick step that rounded to 0 counts as no stick) */
    if (!stickX && !stickY) {
        if (dpadPitch || dpadYaw) {
            if (dpadRepeatMs > 150) {
                dpadStep += 2;
                if (dpadStep > 25)
                    dpadStep = 25;
                dpadRepeatMs = 0;
            }
        } else {
            dpadRepeatMs = 0;
            dpadStep = 5;
        }
    }
    if (dpadPitch) {
        AddAimOffset(&aimOffset.x, (s16)(dpadPitch * dpadStep), CS_AIM_PITCH);
        csFlags |= CSF_AIM_CHANGED;
    }
    if (dpadYaw) {
        AddAimOffset(&aimOffset.y, (s16)(dpadYaw * dpadStep), CS_AIM_YAW);
        csFlags |= CSF_AIM_CHANGED;
    }

    /* the aim moved (or the first frame): turn the barrel and put the camera behind it */
    if ((csFlags & CSF_AIM_CHANGED) || !(csFlags & CSF_AIM_CAM_STARTED)) {
        aim.x = aimOffset.x + defaultAim.x;
        aim.y = aimOffset.y + defaultAim.y;
        aim.z = aimOffset.z + defaultAim.z;
        StartCamera(aim.x & 0xfff, aim.y & 0xfff, 0, &muzzlePos, 800, 0);
        ApplyAim(&aim);
        csFlags &= ~CSF_AIM_CHANGED;
        csFlags |= CSF_AIM_CAM_STARTED;
    }

    /* Fire shoots along the camera's forward vector (the view matrix's third row), Exit leaves. Fire only once a
     * frame has gone by with neither pressed (CSF_FIRE_ARMED, the disc's guard against the press that started the
     * aiming; ICTX_CANNON's push already ignores that press) */
    if (Input_Pressed(ICTX_CANNON, CN_FIRE) && (csFlags & CSF_FIRE_ARMED)) {
        aim.x = g_cullPlaneNormal[0];
        aim.y = g_cullPlaneNormal[1];
        aim.z = g_cullPlaneNormal[2];
        SetState(CS_ST_FIRING);
    } else if (Input_Pressed(ICTX_CANNON, CN_EXIT))
        SetState(CS_ST_IDLE);
    else
        csFlags |= CSF_FIRE_ARMED;
}
void CanonSimple::AddAimOffset(s16 *offset, s16 step, u8 axis)
{
    *offset += (s16)Time_ScaleStep(step, axis == CS_AIM_PITCH ? pitchRemainder : yawRemainder);
    ClampAimOffset(offset, axis);
}
/* the aim's limits: pitch at most 40 down and within ±vertLimit, yaw within ±horzLimit (PostLoadInit) */
void CanonSimple::ClampAimOffset(s16 *offset, u8 axis)
{
    switch (axis) {
        case CS_AIM_PITCH:
            if (*offset > 40)
                *offset = 40;
            else if (ABS_VALUE(*offset) > vertLimit) {
                if (*offset > 0)
                    *offset = vertLimit;
                else
                    *offset = -vertLimit;
            }
            break;
        case CS_AIM_YAW:
            if (ABS_VALUE(*offset) > horzLimit) {
                if (*offset > 0)
                    *offset = horzLimit;
                else
                    *offset = -horzLimit;
            }
            break;
    }
}
void CanonSimple::MoveTo(Vec3s *at)
{
    Vec3s delta;
    delta.x = at->x - lastPos.x;
    delta.y = at->y - lastPos.y;
    delta.z = at->z - lastPos.z;
    activationBox->min[0] += delta.x;
    activationBox->min[1] += delta.y;
    activationBox->min[2] += delta.z;
    activationBox->max[0] += delta.x;
    activationBox->max[1] += delta.y;
    activationBox->max[2] += delta.z;
    muzzlePos = *at;
    lastPos = muzzlePos;
    muzzlePos.y -= 80;
    muzzlePosCopy = muzzlePos;
    SetPosition(at);
    barrel->SetPosition(&muzzlePos);
    if (state != CS_ST_FIRED)
        ball->HandleMessage(this, MSG_CB_PLACE, &muzzlePos);
}
void CanonSimple::Translate(Vec3s *delta)
{
    ScnObject::Translate(delta);
    barrel->Translate(delta);
    ball->Translate(delta);
    activationBox->min[0] += delta->x;
    activationBox->min[1] += delta->y;
    activationBox->min[2] += delta->z;
    activationBox->max[0] += delta->x;
    activationBox->max[1] += delta->y;
    activationBox->max[2] += delta->z;
    lastPos = pos;
    muzzlePos = barrel->pos;
    muzzlePosCopy = muzzlePos;
}
void CanonSimple::ApplyAim(Vec3s *target)
{
    Vec3s barrelAim;
    {
        Vec3s baseAim;
        barrelAim.z = (-target->x) & 0xfff;
        barrelAim.x = 0;
        barrelAim.y = (-target->y + 0x400) & 0xfff;
        baseAim = barrelAim;
        baseAim.z = 0;
        SetRotation(&baseAim);
        barrel->SetRotation(&barrelAim);
    }
}
ScnObject *CanonSimple_Create(void *record)
{
    CanonSimple *object = new CanonSimple;
    object = (CanonSimple *)object->Init(record, 0);
    return object;
}
