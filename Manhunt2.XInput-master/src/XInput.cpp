#pragma once
#include "pch.h"
#include "XInput.h"
#include "MemoryMgr.h"

#ifndef MH2_XINPUT_GLUE_H
#define MH2_XINPUT_GLUE_H

#include <windows.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

// ====== CONFIG ======
// Use bumpers/triggers to drive OS-level mouse buttons only.
// RB/RT -> LMB, LB/LT -> RMB
#define RMB_FALLBACK_USE_RB 0  // kept for compatibility (unused for logic now)

// Your live keyboard-mapper tables & action indices
#include "C:\\Users\\Hellwire\\Desktop\\Nostalgia Modpack 1.6\\Source Code\\Manhunt2.XInput-master\\CInput.h"
#include "C:\\Users\\Hellwire\\Desktop\\Nostalgia Modpack 1.6\\Source Code\\Manhunt2.XInput-master\\keyboard.h"

// =========================
// XInput shims / structures
// =========================
struct MH2_XINPUT_GAMEPAD {
    uint16_t wButtons;
    uint8_t  bLeftTrigger;
    uint8_t  bRightTrigger;
    int16_t  sThumbLX;
    int16_t  sThumbLY;
    int16_t  sThumbRX;
    int16_t  sThumbRY;
};
struct MH2_XINPUT_STATE {
    uint32_t           dwPacketNumber;
    MH2_XINPUT_GAMEPAD Gamepad;
};

enum eWeaponType { WEAPONTYPE_NONE = 0, WEAPONTYPE_2HFIREARM, WEAPONTYPE_1HFIREARM, WEAPONTYPE_MELEE, WEAPONTYPE_LURE };
enum ePedState { PEDSTATE_NORMAL = 3, PEDSTATE_CRAWL = 5, PEDSTATE_AIM = 8, PEDSTATE_BLOCK = 9, PEDSTATE_WALLSQASH = 10, PEDSTATE_PEEK = 13, PEDSTATE_CROUCH = 14, PEDSTATE_CROUCHAIM = 17, PEDSTATE_DRAGDEADBODY = 18, PEDSTATE_CLIMB = 20 };

static const uint16_t XI_DPAD_UP = 0x0001;
static const uint16_t XI_DPAD_DOWN = 0x0002;
static const uint16_t XI_DPAD_LEFT = 0x0004;
static const uint16_t XI_DPAD_RIGHT = 0x0008;
static const uint16_t XI_START = 0x0010;
static const uint16_t XI_BACK = 0x0020;
static const uint16_t XI_LEFT_THUMB = 0x0040;   // L3
static const uint16_t XI_RIGHT_THUMB = 0x0080;  // R3
static const uint16_t XI_LEFT_SHOULDER = 0x0100;  // LB
static const uint16_t XI_RIGHT_SHOULDER = 0x0200;  // RB
static const uint16_t XI_A = 0x1000;
static const uint16_t XI_B = 0x2000;
static const uint16_t XI_X = 0x4000;
static const uint16_t XI_Y = 0x8000;

static const uint8_t  TRIGGER_THRESHOLD = 30;
static const int      INPUT_SENTINEL = 0xFF;

// ===== Engine pad/cam globals =====
static volatile float* const g_LS_X = (float*)0x0076BE7C;
static volatile float* const g_LS_Y = (float*)0x0076BE80;
static volatile float* const g_RS_X = (float*)0x0076BE84;
static volatile float* const g_RS_Y = (float*)0x0076BE88;
static volatile uint32_t* const g_PadFlags = (uint32_t*)0x0076BE8C;   // LS sign/activity bits
static volatile uint32_t* const g_ImmFlags = (uint32_t*)0x0076BE6C;   // immediate bits (mouse/aim/fire)
static volatile int* const g_PadMode = (int*)0x006B17C4;

static const uint32_t IM_MOUSE1_HOLD = 0x01;
static const uint32_t IM_MOUSE1_EDGE = 0x02;
static const uint32_t IM_MOUSE2_HOLD = 0x04;
static const uint32_t IM_MOUSE2_EDGE = 0x08;
static const uint32_t IM_RS_PRESENT = 0x400;  // case 0x0F
static const uint32_t IF_AIM = 0x800;  // case 0x10
static const uint32_t IF_FIRE = 0x1000; // case 0x11

// ===== triplet table scanned by the keyboard provider =====
static volatile int* const gTripBase = (int*)0x0076BEA0; // [code, pressed, extra] x N
static volatile int* const gTripEnd = (int*)0x0076BF18;

// ===== mapping tables (primary/secondary) =====
static KeyCode* const g_ActionMapPrimary = (KeyCode*)0x0076E1C8;
static KeyCode* const g_ActionMapSecondary = (KeyCode*)0x0076E2A0;

// ===== XInput ====
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, MH2_XINPUT_STATE*);
typedef VOID(WINAPI* PFN_XInputEnable)(BOOL);
static HMODULE            g_hXInput = nullptr;
static PFN_XInputGetState g_XIGetState = nullptr;
static PFN_XInputEnable   g_XIEnable = nullptr;
static bool               g_XIInited = false;
static bool               g_XIEnabled = true;

extern "C" void __cdecl MH2_XInputEnableShim(uint8_t enable) { g_XIEnabled = (enable != 0); }

// ==== Mouse-delta injection for aim ====
static float AIM_MOUSE_SENS_X = 1400.0f;
static float AimMouseAccumX = 0.0f;

static inline void EmitSystemMouseMove(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;                 // relative pixels
    in.mi.dy = dy;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(in));
}

// === INPUT HELPERS: firearm test + mouse-delta + wheel-block hook ===

// Firearm types helper (used to decide when to send mouse delta)
static inline bool HasFirearm(int w) {
    return (w == WEAPONTYPE_1HFIREARM || w == WEAPONTYPE_2HFIREARM);
}

// --------- ALWAYS-ON (when game focused) MOUSE WHEEL BLOCK ---------
// We swallow WM_MOUSEWHEEL/WM_MOUSEHWHEEL at the OS low-level hook
// whenever THIS process' window is foreground. This prevents any
// wheel-driven weapon cycling while playing. Wheel still works in
// other apps when they are focused.
static inline bool IsGameForeground() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static HHOOK g_hMouseLL = nullptr;
static LRESULT CALLBACK WheelEat_LL(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if ((wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) && IsGameForeground()) {
            return 1; // swallow wheel
        }
    }
    return CallNextHookEx(g_hMouseLL, nCode, wParam, lParam);
}

static void EnsureMouseLLHook() {
    if (!g_hMouseLL) {
        g_hMouseLL = SetWindowsHookExW(WH_MOUSE_LL, WheelEat_LL, GetModuleHandleW(NULL), 0);
    }
}

static void MH2_InitXInput() {
    if (g_XIInited) return;
    const wchar_t* dlls[] = { L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll" };
    for (auto dll : dlls) { g_hXInput = LoadLibraryW(dll); if (g_hXInput) break; }
    if (g_hXInput) {
        g_XIGetState = reinterpret_cast<PFN_XInputGetState>(GetProcAddress(g_hXInput, "XInputGetState"));
        g_XIEnable = reinterpret_cast<PFN_XInputEnable>(GetProcAddress(g_hXInput, "XInputEnable"));
        if (g_XIEnable) g_XIEnable(TRUE);
    }
    g_XIInited = true;

    // Install low-level wheel blocker once
    EnsureMouseLLHook();
}

static inline float MH2_NormStick(int16_t v, int dz) {
    int iv = (int)v;
    if (iv >= -dz && iv <= dz) return 0.0f;
    const int max = 32767;
    float sign = (iv < 0) ? -1.0f : 1.0f;
    float mag = (float)(abs(iv) - dz) / (float)(max - dz);
    if (mag < 0.0f) mag = 0.0f; if (mag > 1.0f) mag = 1.0f;
    return sign * mag;
}

// ==== Triplet utilities ====
static inline int TripletCapacity() {
    uintptr_t bytes = (uintptr_t)gTripEnd - (uintptr_t)gTripBase;
    return (int)(bytes / (sizeof(int) * 3)); // usually 10
}

// Internal mouse-wheel codes used by the game's keyboard provider
static const int MWHEEL_UP_CODE = 0x95;
static const int MWHEEL_DOWN_CODE = 0x96;

// Remove any wheel entries from (codes, pressed) in-place
static inline void StripWheelCodes(int& n, int* codes, int* pressed) {
    int j = 0;
    for (int i = 0; i < n; ++i) {
        const int c = codes[i];
        if (c == MWHEEL_UP_CODE || c == MWHEEL_DOWN_CODE) continue;
        if (j != i) { codes[j] = c; pressed[j] = pressed[i]; }
        ++j;
    }
    n = j;
}

// === Triplet wheel scrubber (kills any wheel codes provider may have queued) ===
static inline void ScrubMouseWheelTriplets()
{
    // The triplet buffer is [code, pressed, extra] repeated.
    // We don't touch 'code' (to avoid breaking sentinel scanning),
    // just force 'pressed' to 0 so the engine ignores the wheel this frame.
    volatile int* p = gTripBase;
    while (p < gTripEnd) {
        const int code = p[0];
        if (code == MWHEEL_UP_CODE || code == MWHEEL_DOWN_CODE) {
            p[1] = 0;  // not pressed
            p[2] = 0;  // clear any edge/extra just in case
        }
        p += 3;
    }
}

static void WriteTripletsDeterministic(const int* codes, const int* pressed, int count) {
    int cap = TripletCapacity();
    if (count > cap) count = cap;
    volatile int* p = gTripBase;
    for (int i = 0; i < count; ++i) { p[0] = codes[i]; p[1] = pressed[i] ? 1 : 0; p[2] = 0; p += 3; }
    if (count < cap) { p[0] = INPUT_SENTINEL; p[1] = 0; p[2] = 0; p += 3; }
    while (p < gTripEnd) { p[0] = INPUT_SENTINEL; p[1] = 0; p[2] = 0; p += 3; }
}

// ==== publish helper (filters out wheel; we push NO mouse button triplets) ====
static inline void PublishBoundCodesSafe(int actionIdx, int& n, int* codes, int* pressed) {
    auto push = [&](int c) {
        if (!c || c == INPUT_SENTINEL) return;
        if (c == MWHEEL_UP_CODE || c == MWHEEL_DOWN_CODE) return; // filter wheel up/down only
        // de-dup
        for (int i = 0; i < n; ++i) if (codes[i] == c) { pressed[i] = 1; return; }
        codes[n] = c; pressed[n] = 1; ++n;
        };
    const int c1 = (int)(uint8_t)g_ActionMapPrimary[actionIdx].Code;
    const int c2 = (int)(uint8_t)g_ActionMapSecondary[actionIdx].Code;
    push(c1); if (c2 && c2 != c1) push(c2);
}

// ==== OS-level mouse injection (SendInput) ====
static bool g_SysLMBDown = false;
static bool g_SysRMBDown = false;

static inline void EmitSystemLMB(bool down) {
    if (down == g_SysLMBDown) return; // edge only
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(in));
    g_SysLMBDown = down;
}
static inline void EmitSystemRMB(bool down) {
    if (down == g_SysRMBDown) return; // edge only
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    SendInput(1, &in, sizeof(in));
    g_SysRMBDown = down;
}

// ==========================
// Main per-frame glue
// ==========================
static void MH2_UpdateXInputForPlayer(void* playerInst) {
    // Scrub wheel immediately at frame start in case provider queued any
    ScrubMouseWheelTriplets();

    MH2_InitXInput();
    if (!g_XIEnabled || !g_XIGetState) return;

    MH2_XINPUT_STATE st{};
    DWORD xi = g_XIGetState(0, &st);

    int  codes[16];  int pressed[16]; int n = 0;

    if (xi != ERROR_SUCCESS) {
        // Sticks zero & flags clear
        if (playerInst) {
            float* L = (float*)((char*)playerInst + 1168);
            float* R = (float*)((char*)playerInst + 1184);
            L[0] = L[1] = R[0] = R[1] = 0.0f;
        }
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT); // don't touch mouse bits anymore

        // Ensure OS mouse buttons are released if device vanished
        EmitSystemLMB(false);
        EmitSystemRMB(false);

        WriteTripletsDeterministic(codes, pressed, 0);
        // Guarantee no wheel triplets remain on error path
        ScrubMouseWheelTriplets();
        return;
    }

    // sticks
    const int DZ_L = 7849, DZ_R = 8689;
    float lx = MH2_NormStick(st.Gamepad.sThumbLX, DZ_L);
    float ly = MH2_NormStick(st.Gamepad.sThumbLY, DZ_L);
    float rx = MH2_NormStick(st.Gamepad.sThumbRX, DZ_R);
    float ry = MH2_NormStick(st.Gamepad.sThumbRY, DZ_R);

    *g_LS_X = lx; *g_LS_Y = ly;
    {   // vanilla LS flags
        const float TH = 0.5f;
        uint32_t A = 0;
        if (fabsf(lx) > 1e-5f || fabsf(ly) > 1e-5f) A |= 0x40000000;
        if (lx < -TH) A |= 0x00040000; else if (lx > TH) A |= 0x00080000;
        if (ly < -TH) A |= 0x00020000; else if (ly > TH) A |= 0x00010000;
        *g_PadFlags = A;
    }

    // keep legacy RS globals zero; store RS on player for camera patch
    *g_RS_X = 0.0f; *g_RS_Y = 0.0f;
    if (playerInst) {
        float* L = (float*)((char*)playerInst + 1168);
        float* R = (float*)((char*)playerInst + 1184);
        L[0] = lx; L[1] = ly;
        R[0] = rx; R[1] = ry;
    }
    if (fabsf(rx) > 1e-4f || fabsf(ry) > 1e-4f) *g_ImmFlags |= IM_RS_PRESENT; else *g_ImmFlags &= ~IM_RS_PRESENT;

    // buttons
    const uint16_t wb = st.Gamepad.wButtons;
    const bool A = (wb & XI_A) != 0;
    const bool B = (wb & XI_B) != 0;
    const bool Xb = (wb & XI_X) != 0;
    const bool Y = (wb & XI_Y) != 0;

    const bool LB = (wb & XI_LEFT_SHOULDER) != 0;
    const bool RB = (wb & XI_RIGHT_SHOULDER) != 0;
    const bool L3 = (wb & XI_LEFT_THUMB) != 0;
    const bool DUp = (wb & XI_DPAD_UP) != 0;
    const bool DL = (wb & XI_DPAD_LEFT) != 0;
    const bool DR = (wb & XI_DPAD_RIGHT) != 0;
    const bool DD = (wb & XI_DPAD_DOWN) != 0;
    const bool Start = (wb & XI_START) != 0;
    const bool Back = (wb & XI_BACK) != 0;

    const bool LT = st.Gamepad.bLeftTrigger > TRIGGER_THRESHOLD;
    const bool RT = st.Gamepad.bRightTrigger > TRIGGER_THRESHOLD;

    // === OS-level mouse (bumpers only) ===
    // RB -> LMB, LB -> RMB
    const bool wantLMB = RB;
    const bool wantRMB = LB;
    EmitSystemLMB(wantLMB);
    EmitSystemRMB(wantRMB);

    // everything else => triplets (filtering wheel; do NOT emit mouse button triplets)
    if (A)     PublishBoundCodesSafe(ACTION_USE, n, codes, pressed);
    if (Y)     PublishBoundCodesSafe(ACTION_WALLSQUASH, n, codes, pressed);
    if (DD)    PublishBoundCodesSafe(ACTION_INVENTORY_SLOT_DOWN, n, codes, pressed); // D-pad Down
    if (DUp)   PublishBoundCodesSafe(ACTION_RELOAD, n, codes, pressed);         // D-pad Up
    if (B)     PublishBoundCodesSafe(ACTION_PICKUP, n, codes, pressed);
    if (Xb)    PublishBoundCodesSafe(ACTION_BLOCK, n, codes, pressed);
    if (L3)    PublishBoundCodesSafe(ACTION_TOGGLE_TORCH, n, codes, pressed);            // keep L3 as Run
    if (DL)    PublishBoundCodesSafe(ACTION_PEEKL, n, codes, pressed);
    if (DR)    PublishBoundCodesSafe(ACTION_PEEKR, n, codes, pressed);
    if (RT)    PublishBoundCodesSafe(ACTION_FIRE1, n, codes, pressed);          // Right Trigger
    if (LT)    PublishBoundCodesSafe(ACTION_RUN, n, codes, pressed);            // Left Trigger
    if (Start) PublishBoundCodesSafe(ACTION_MENU, n, codes, pressed);           // Start
    if (Back)  PublishBoundCodesSafe(ACTION_INVENT_SWAP, n, codes, pressed);    // Select/Back

    // Final safety: strip any possible mouse-wheel codes before publishing (prevents unintended weapon cycling)
    StripWheelCodes(n, codes, pressed);

    WriteTripletsDeterministic(codes, pressed, n);

    // Belt-and-suspenders — ensure no wheel triplets survived
    ScrubMouseWheelTriplets();
}


// =====================================================================
// Camera patch – RS yaw+pitch (player follows yaw). LS never affects camera.
// =====================================================================
static float CAM_YAW_SENS = 20.0f; // RS.x -> player yaw
static float CAM_PITCH_SENS = 20.0f; // RS.y -> camera pitch

void* TheCamera = (void*)0x76F240;
CVector* CurrentCamLookAtOffsetFromPlayer = (CVector*)((char*)TheCamera + 1536);
CVector* CurrentCamOffsetFromPlayer = (CVector*)((char*)TheCamera + 1520);
CVector* CurrentRevCamOffsetFromPlayer = (CVector*)((char*)TheCamera + 1504);

// Camera vertical window (pitch accumulator)
static float CamMinYAngle = -29.0f;
static float CamMaxYAngle = 42.0f;
static float CamCurrYAngle = 15.0f;
static bool  CamInSyncWithPlrYAngle = false;
static float CAM_FREELOOK_CLAMP = 120.0f; // max left/right offset (deg)
static float CamYawOffset = 0.0f;         // camera yaw offset around player (deg)
static bool  WasAiming = false;           // track aim edge for smooth sync

static inline float Deg2Rad(float x) { return x * (3.14159265358979323846f / 180.0f); }
static inline float DegWrap(float a) {
    // wrap to [-180, 180]
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}
// Rotate a local-space offset vector around the (local) up axis (Y) by 'deg' degrees.
static inline void RotateYLocal(CVector* v, float deg) {
    float r = Deg2Rad(deg);
    float c = cosf(r), s = sinf(r);
    float x = v->x, z = v->z;
    v->x = c * x + s * z;
    v->z = -s * x + c * z;
}

CVector* __fastcall GetGlobalCameraPosFromPlayer(void* This, void*,
    CVector* camPos, CVector* plrPos, CMatrix* camRot, CMatrix* plrRot, CVector* gCamPosFromPlr)
{
    MH2_UpdateXInputForPlayer(This);

    float* PlrCurrYAngle = (float*)((char*)This + 4400); // player yaw
    bool* PlrActionLookBack = (bool*)((char*)This + 1232);
    bool* PlrActionLockOn = (bool*)((char*)This + 1352);
    int* PlrCurrState = (int*)((char*)This + 956);
    int* PlrCurrWpnType = (int*)((char*)This + 968);
    int* PlrLockOnHunter = (int*)((char*)This + 4500);
    CVector2d* RS = (CVector2d*)((char*)This + 1184);
    const float dt = *(float*)0x6ECE68;

    // RMB (LB) should always work, but only use mouse-delta when a firearm is equipped.
    const bool rmbHeld = g_SysRMBDown; // LB -> RMB via SendInput
    const bool inAimState = (*PlrCurrState == PEDSTATE_AIM) || (*PlrCurrState == PEDSTATE_CROUCHAIM) ||
        (*PlrLockOnHunter && *PlrActionLockOn);
    const bool hasFirearmNow = HasFirearm(*PlrCurrWpnType);

    // Only send mouse delta if we truly have firearm aim.
    const bool doMouseDelta = hasFirearmNow && (rmbHeld || inAimState);

    // If holding LB with no firearm, rotate the player directly (no mouse injection).
    const bool lbRotateNoGun = rmbHeld && !hasFirearmNow;

    // Unified "aim camera" mode flag
    const bool aimCamNow = (doMouseDelta || lbRotateNoGun);

    // On aim edge: snap player to current camera heading, then zero freelook & pixel accumulator.
    if (aimCamNow && !WasAiming) {
        *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
        CamYawOffset = 0.0f;
        AimMouseAccumX = 0.0f;
    }
    WasAiming = aimCamNow;

    if (doMouseDelta) {
        // Firearm aiming: feed horizontal MOUSEMOVE only (no wheel)
        *g_ImmFlags |= IF_AIM;

        float px = RS->x * AIM_MOUSE_SENS_X * dt;   // px/s * s = px
        AimMouseAccumX += px;
        int dx = (AimMouseAccumX >= 0.0f) ? (int)floorf(AimMouseAccumX + 0.5f)
            : (int)ceilf(AimMouseAccumX - 0.5f);
        AimMouseAccumX -= (float)dx;

        EmitSystemMouseMove(dx, 0);

        // Make sure no other path consumes RS while on mouse path
        *g_RS_X = 0.0f;
        *g_RS_Y = 0.0f;

        // Also scrub the game's triplet buffer so any DI/raw wheel the game queued is neutralized
        ScrubMouseWheelTriplets();
    }
    else {
        if (lbRotateNoGun) {
            // No firearm: holding LB rotates player directly
            if (fabsf(RS->x) > 0.0005f) {
                *PlrCurrYAngle += RS->x * dt * CAM_YAW_SENS;
            }
        }
        else {
            // Not aiming: RS.x = free-look yaw only
            if (fabsf(RS->x) > 0.0005f) {
                CamYawOffset = DegWrap(CamYawOffset + RS->x * dt * CAM_YAW_SENS);
                if (CamYawOffset > CAM_FREELOOK_CLAMP) CamYawOffset = CAM_FREELOOK_CLAMP;
                if (CamYawOffset < -CAM_FREELOOK_CLAMP) CamYawOffset = -CAM_FREELOOK_CLAMP;
            }
        }
    }

    // RS.y -> camera pitch accumulator
    if (fabsf(RS->y) > 0.0005f) {
        CamCurrYAngle += RS->y * dt * CAM_PITCH_SENS;
        if (CamCurrYAngle > CamMaxYAngle) CamCurrYAngle = CamMaxYAngle;
        if (CamCurrYAngle < CamMinYAngle) CamCurrYAngle = CamMinYAngle;
    }

    CVector CamPosOffsetFromPlayer;
    ((CVector * (__thiscall*)(void*, CVector*, CMatrix*, bool))0x5AF950)(This, plrPos, plrRot, false);

    if (!*(bool*)0x789819 && *PlrActionLookBack)
    {
        *(int*)0x76F244 = 0; *(int*)0x76F24C = 0;
        if (*(bool*)0x76F4FD && *(bool*)0x76F4FE) { CamPosOffsetFromPlayer = *CurrentRevCamOffsetFromPlayer; CamCurrYAngle = -CamCurrYAngle; }
        else { *(bool*)0x76F4FD = true; CamPosOffsetFromPlayer = *CurrentCamOffsetFromPlayer; }
    }
    else if (*PlrCurrState == PEDSTATE_CRAWL || ((bool(__cdecl*)(void*))0x515260)(This))
    {
        RslElementGroup* elementGroup = (RslElementGroup*)((char*)This + 184);
        CamPosOffsetFromPlayer.x = 0.0f;
        CamPosOffsetFromPlayer.y = (float)fabsf(sinf(elementGroup->parentNode->ltm.pos.x * 4.0f + elementGroup->parentNode->ltm.pos.z) * 0.05f);
        CamPosOffsetFromPlayer.z = -0.001f;
    }
    else {
        CamPosOffsetFromPlayer = *CurrentCamOffsetFromPlayer;
    }

    if ((*PlrCurrState == PEDSTATE_AIM && *PlrCurrWpnType == WEAPONTYPE_LURE && !*(bool*)0x75B348) ||
        *PlrCurrState == PEDSTATE_CRAWL || *PlrCurrState == PEDSTATE_CLIMB ||
        (*PlrLockOnHunter && *PlrActionLockOn) || *(bool*)0x76F860)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.35f;
        CurrentCamLookAtOffsetFromPlayer->y = -0.18f;
        CurrentCamLookAtOffsetFromPlayer->z = 0.12f;
        if (CamInSyncWithPlrYAngle || *PlrCurrState == PEDSTATE_CLIMB || *PlrCurrState == PEDSTATE_CRAWL ||
            (*PlrLockOnHunter && *PlrActionLockOn) || *(bool*)0x76F860 || !*(bool*)0x75B348)
        {
            CamCurrYAngle = *PlrCurrYAngle;
            CurrentCamLookAtOffsetFromPlayer->x = 0.03f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
        }
        else { CamInSyncWithPlrYAngle = true; *PlrCurrYAngle = CamCurrYAngle; }
    }
    else if (*PlrCurrState == PEDSTATE_NORMAL)
    {
        CamInSyncWithPlrYAngle = false;
        CurrentCamLookAtOffsetFromPlayer->x = 0.03f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }
    else if (*PlrCurrState == PEDSTATE_PEEK && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM)
    {
        CamInSyncWithPlrYAngle = false;
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.020f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_PEEK && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348)
    {
        CamInSyncWithPlrYAngle = false;
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.020f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_AIM && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.041f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.03f;
    }
    else if (*PlrCurrState == PEDSTATE_AIM && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.041f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.03f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCH && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCH && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCHAIM && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.02f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.02f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCHAIM && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348)
    {
        CurrentCamLookAtOffsetFromPlayer->x = -0.02f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.02f;
    }
    else if (*PlrCurrState == PEDSTATE_WALLSQASH)
    {
        CurrentCamLookAtOffsetFromPlayer->x = 0.01f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }
    else {
        CamInSyncWithPlrYAngle = false;
        CurrentCamLookAtOffsetFromPlayer->x = 0.01f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }

    // Build camera pos from offsets
    CMatrix CamPitchRot;
    ((CMatrix * (__thiscall*)(CMatrix*, CVector*, float))0x57E440)(&CamPitchRot, (CVector*)0x6B3218, CamCurrYAngle);
    ((CVector * (__thiscall*)(CVector*, CVector*, CMatrix*))0x580390)(&CamPosOffsetFromPlayer, &CamPosOffsetFromPlayer, &CamPitchRot);

    // apply free-look yaw around player's up axis in local space
    if (!aimCamNow && fabsf(CamYawOffset) > 1e-4f) {
        RotateYLocal(&CamPosOffsetFromPlayer, CamYawOffset);
    }

    ((CVector * (__thiscall*)(CVector*, CVector*, CMatrix*))0x580390)(&CamPosOffsetFromPlayer, &CamPosOffsetFromPlayer, plrRot);
    ((CVector * (__cdecl*)(CVector*, CVector*, CVector*))0x60D800)(gCamPosFromPlr, plrPos, &CamPosOffsetFromPlayer);

    return camPos;
}


void __cdecl InterpolateAimAngle(float* curAngle, float* /*angleDelta*/, float targetAngle, float /*speed*/, float /*minDelta*/) {
    *curAngle = targetAngle;
}

// Crosshair
void ProcessCrosshair() {
    *(char*)0x79D0E4 = 0;
    *(float*)0x79D0E8 = *(float*)0x76DEE0;
    *(float*)0x79D0EC = *(float*)0x75B124;

    if (*(int*)0x76DECC) {
        void* PlayerInst = *(void**)0x789490;
        float fDefaultSpriteScale = 0.05f;
        float fLineLength = 0.2f * fDefaultSpriteScale;
        float fDefaultSpread = 0.1f * fDefaultSpriteScale;
        float fMaxSpread = 0.5f * fDefaultSpriteScale;
        float fLineOffset = 0.01f * fDefaultSpriteScale;
        if (PlayerInst && ((bool(__thiscall*)(void*))0x599290)(PlayerInst)) {
            CVector2d* RS = (CVector2d*)((char*)PlayerInst + 1184);
            CVector2d* LS = (CVector2d*)((char*)PlayerInst + 1168);
            float fSpreadAimMove = RS->Magnitude() * 0.005f + LS->Magnitude() * 0.01f + fDefaultSpread;
            float fSpread = (fSpreadAimMove <= fMaxSpread) ? fSpreadAimMove : fMaxSpread;
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(-fLineOffset, fSpread, -fLineOffset, fSpread + fLineLength, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(fLineOffset, fSpread, fLineOffset, fSpread + fLineLength, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(-fLineOffset, -fSpread, -fLineOffset, -fSpread - fLineLength, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(fLineOffset, -fSpread, fLineOffset, -fSpread - fLineLength, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(-fSpread, -fLineOffset, -fSpread - fLineLength, -fLineOffset, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(-fSpread, fLineOffset, -fSpread - fLineLength, fLineOffset, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(fSpread, -fLineOffset, fSpread + fLineLength, -fLineOffset, 160, 160, 160, 255, 1);
            ((int(__cdecl*)(float, float, float, float, int, int, int, int, int))0x60AF80)(fSpread, fLineOffset, fSpread + fLineLength, fLineOffset, 160, 160, 160, 255, 1);
        }
    }

    *(float*)0x79D0EC = 0.0f;
    *(char*)0x79D0E4 = 0;
    *(float*)0x79D0E8 = 0.0f;
}

// No global hook needed; publisher path drives the keyboard provider
extern "C" void MH2_XInputGlue_Install() {}

#endif // MH2_XINPUT_GLUE_H
