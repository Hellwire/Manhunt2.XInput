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
#include <wchar.h>

// Your keyboard-mapper tables & action indices
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
static const uint16_t XI_LEFT_SHOULDER = 0x0100;   // LB
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
static volatile uint32_t* const g_PadFlags = (uint32_t*)0x0076BE8C;
static volatile uint32_t* const g_ImmFlags = (uint32_t*)0x0076BE6C;

static const uint32_t IM_RS_PRESENT = 0x400;  // case 0x0F

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

// ===== Config (INI) =====
static bool   g_InvertLookX = false; // RS.x
static bool   g_InvertLookY = false; // RS.y
static wchar_t g_IniPath[MAX_PATH] = L"";

// ===== Crawl → mouse delta (no INI) =====
static const float CRAWL_MOUSE_X_SENS = 1500.0f; // pixels/sec at full RS deflection
static float s_CrawlMouseXAcc = 0.0f;           // fractional accumulator for dx

// ===== LOAD CONFIG =====

// Returns the folder of the current DLL (.asi)
static void GetDllFolder(wchar_t* outPath, size_t size) {
    HMODULE hMod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetDllFolder, &hMod);

    GetModuleFileNameW(hMod, outPath, (DWORD)size);

    wchar_t* slash = wcsrchr(outPath, L'\\');
    if (slash) *(slash + 1) = L'\0'; else wcscpy_s(outPath, size, L".\\");
}

static void MH2_LoadConfig()
{
    if (!g_IniPath[0]) {
        wchar_t dllDir[MAX_PATH];
        GetDllFolder(dllDir, MAX_PATH);

        // Always create in DLL folder
        wcscpy_s(g_IniPath, dllDir);
        wcscat_s(g_IniPath, L"Manhunt2.XInput.ini");
    }

    // Create ini with defaults if missing
    DWORD attr = GetFileAttributesW(g_IniPath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        WritePrivateProfileStringW(L"Camera", L"InvertLookX", L"0", g_IniPath);
        WritePrivateProfileStringW(L"Camera", L"InvertLookY", L"0", g_IniPath);
    }

    g_InvertLookX = GetPrivateProfileIntW(L"Camera", L"InvertLookX", 0, g_IniPath) != 0;
    g_InvertLookY = GetPrivateProfileIntW(L"Camera", L"InvertLookY", 0, g_IniPath) != 0;
}

// ==== helpers ====
static inline float MH2_NormStick(int16_t v, int dz) {
    int iv = (int)v;
    if (iv >= -dz && iv <= dz) return 0.0f;
    const int max = 32767;
    float sign = (iv < 0) ? -1.0f : 1.0f;
    float mag = (float)(abs(iv) - dz) / (float)(max - dz);
    if (mag < 0.0f) mag = 0.0f; if (mag > 1.0f) mag = 1.0f;
    return sign * mag;
}
static inline bool HasFirearm(int w) {
    return (w == WEAPONTYPE_1HFIREARM || w == WEAPONTYPE_2HFIREARM);
}
// Simple rounding helper (no C99 dependency)
static inline int RoundToInt(float v) { return (int)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f)); }

// ===== Direct injection, identical to the engine's DI path =====
// thunk to FUN_0056F577(kind, payload)
typedef void(__cdecl* tEmitInput)(uint32_t kind, float* payload);
static tEmitInput g_EmitInput = reinterpret_cast<tEmitInput>(0x0056F577);

// Track which DIKs we’ve "pressed" so we can release them later.
static bool sKeyDown[256] = { false };

// NEVER emit wheel as a DIK; just in case it appears in binds
static const int MWHEEL_UP_CODE = 0x95;
static const int MWHEEL_DOWN_CODE = 0x96;

static inline void EmitKeyDown_DIK(uint8_t dik) {
    if (sKeyDown[dik]) return;
    uint32_t u = dik; float f;
    memcpy(&f, &u, sizeof(f));
    g_EmitInput(0x12, &f);        // add/set pressed for this DIK
    sKeyDown[dik] = true;
}
static inline void EmitKeyUp_DIK(uint8_t dik) {
    if (!sKeyDown[dik]) return;
    uint32_t u = dik; float f;
    memcpy(&f, &u, sizeof(f));
    g_EmitInput(0x13, &f);        // mark event so aggregator consumes/removes entry next frame
    sKeyDown[dik] = false;
}
static inline void ReleaseAllTrackedKeys() {
    for (int c = 1; c < 256; ++c) if (sKeyDown[c]) EmitKeyUp_DIK((uint8_t)c);
}

// DIK selection helpers (respect user keybinds via action maps)
static inline void DesireDIK(uint8_t dik, bool* desired) {
    if (!dik || dik == INPUT_SENTINEL || dik == MWHEEL_UP_CODE || dik == MWHEEL_DOWN_CODE) return;
    desired[dik] = true;
}
static inline void DesireActionDIKs(int actionIdx, bool* desired) {
    const uint8_t c1 = (uint8_t)g_ActionMapPrimary[actionIdx].Code;
    const uint8_t c2 = (uint8_t)g_ActionMapSecondary[actionIdx].Code;
    DesireDIK(c1, desired);
    if (c2 && c2 != c1) DesireDIK(c2, desired);
}
static inline void ApplyDesiredKeys(bool* desired) {
    for (int c = 1; c < 256; ++c) {
        if (desired[c] && !sKeyDown[c]) EmitKeyDown_DIK((uint8_t)c);
        else if (!desired[c] && sKeyDown[c]) EmitKeyUp_DIK((uint8_t)c);
    }
}

// Convert LS to WASD (discrete) through binds
static inline void DesireMovementFromStick(float lx, float ly, bool* desired) {
    const float TH = 0.35f;
    if (ly > TH) DesireActionDIKs(ACTION_MOVE_FORWARD, desired); // W
    if (ly < -TH) DesireActionDIKs(ACTION_MOVE_BACKWARDS, desired); // S
    if (lx < -TH) DesireActionDIKs(ACTION_MOVE_LEFT, desired); // A
    if (lx > TH) DesireActionDIKs(ACTION_MOVE_RIGHT, desired); // D
}
// ==== OS-level mouse buttons & wheel ====
// RB->LMB, LB->RMB, DPAD-DOWN -> Wheel Down (single pulse per press)
static bool g_SysLMBDown = false;
static bool g_SysRMBDown = false;
static bool g_DDArmed = true; // re-arms when DPAD-DOWN is released

static inline void EmitSystemLMB(bool down) {
    if (down == g_SysLMBDown) return;
    INPUT in = {}; in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(in));
    g_SysLMBDown = down;
}
static inline void EmitSystemRMB(bool down) {
    if (down == g_SysRMBDown) return;
    INPUT in = {}; in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    SendInput(1, &in, sizeof(in));
    g_SysRMBDown = down;
}
static inline void EmitSystemWheelDownPulse() {
    INPUT in = {}; in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = (DWORD)(-WHEEL_DELTA); // negative = wheel down
    SendInput(1, &in, sizeof(in));
}
// OS-level relative mouse movement
static inline void EmitSystemMouseMoveDelta(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    INPUT in = {}; in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = dx; in.mi.dy = dy;
    SendInput(1, &in, sizeof(in));
}

static bool g_PrevDD = false;

// ==========================
// Init
// ==========================
static void MH2_InitXInput() {
    if (g_XIInited) return;

    const wchar_t* dlls[] = { L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll" };
    for (auto dll : dlls) {
        g_hXInput = LoadLibraryW(dll);
        if (g_hXInput) break;
    }

    if (g_hXInput) {
        g_XIGetState = reinterpret_cast<PFN_XInputGetState>(GetProcAddress(g_hXInput, "XInputGetState"));
        g_XIEnable = reinterpret_cast<PFN_XInputEnable>(GetProcAddress(g_hXInput, "XInputEnable"));
        if (g_XIEnable) g_XIEnable(TRUE);
        g_PrevDD = false; // reset DPAD-down edge state
    }

    // Always load/create INI once
    MH2_LoadConfig();

    // Mark init done whether or not XInput loaded (prevents re-running every frame)
    g_XIInited = true;
}

// ==========================
// Per-frame glue
// ==========================
static void MH2_UpdateXInputForPlayer(void* playerInst) {
    MH2_InitXInput();
    if (!g_XIEnabled || !g_XIGetState) {
        // No pad: zero sticks, release keys and system mouse buttons
        if (playerInst) {
            float* L = (float*)((char*)playerInst + 1168);
            float* R = (float*)((char*)playerInst + 1184);
            L[0] = L[1] = R[0] = R[1] = 0.0f;
        }
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT);
        EmitSystemLMB(false);
        EmitSystemRMB(false);
        ReleaseAllTrackedKeys();
        g_DDArmed = true; // re-arm DPAD-DOWN wheel
        s_CrawlMouseXAcc = 0.0f;
        return;
    }

    MH2_XINPUT_STATE st{};
    DWORD xi = g_XIGetState(0, &st);
    if (xi != ERROR_SUCCESS) {
        if (playerInst) {
            float* L = (float*)((char*)playerInst + 1168);
            float* R = (float*)((char*)playerInst + 1184);
            L[0] = L[1] = R[0] = R[1] = 0.0f;
        }
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT);
        EmitSystemLMB(false);
        EmitSystemRMB(false);
        ReleaseAllTrackedKeys();
        g_DDArmed = true; // re-arm DPAD-DOWN wheel
        s_CrawlMouseXAcc = 0.0f;
        return;
    }

    // --- sticks (LS/RS) ---
    const int DZ_L = 7849, DZ_R = 8689;
    float lx = MH2_NormStick(st.Gamepad.sThumbLX, DZ_L);
    float ly = MH2_NormStick(st.Gamepad.sThumbLY, DZ_L);
    float rx = MH2_NormStick(st.Gamepad.sThumbRX, DZ_R);
    float ry = MH2_NormStick(st.Gamepad.sThumbRY, DZ_R);

    // crawl state (affects movement input path)
    bool isCrawling = false;
    if (playerInst) {
        int* PlrCurrState = (int*)((char*)playerInst + 956);
        isCrawling = (PlrCurrState && *PlrCurrState == PEDSTATE_CRAWL);
    }

    // While crawling, drive rotation via OS mouse delta from RS.x
    if (isCrawling) {
        const float dt = *(float*)0x6ECE68;           // engine delta time
        const float dxF = rx * dt * CRAWL_MOUSE_X_SENS;
        s_CrawlMouseXAcc += dxF;
        const int dx = RoundToInt(s_CrawlMouseXAcc);
        if (dx != 0) {
            EmitSystemMouseMoveDelta(dx, 0);
            s_CrawlMouseXAcc -= (float)dx;
        }
    }
    else {
        s_CrawlMouseXAcc = 0.0f; // reset accumulator when leaving crawl
    }

    // Publish LS (zeroed when crawling so engine analog move path is disabled)
    *g_LS_X = isCrawling ? 0.0f : lx;
    *g_LS_Y = isCrawling ? 0.0f : ly;

    // Engine LS/RS flag bits (preserved behavior)
    {
        const float TH = 0.5f;
        uint32_t A = 0;
        if (fabsf(lx) > 1e-5f || fabsf(ly) > 1e-5f) A |= 0x40000000;
        if (lx < -TH) A |= 0x00040000; else if (lx > TH) A |= 0x00080000;
        if (ly < -TH) A |= 0x00020000; else if (ly > TH) A |= 0x00010000;
        *g_PadFlags = isCrawling ? 0u : A;
    }

    // Mirror to per-player LS/RS storages
    *g_RS_X = 0.0f; *g_RS_Y = 0.0f;
    if (playerInst) {
        float* L = (float*)((char*)playerInst + 1168);
        float* R = (float*)((char*)playerInst + 1184);
        L[0] = isCrawling ? 0.0f : lx;
        L[1] = isCrawling ? 0.0f : ly;
        // Suppress RS.x when crawling so internal yaw patch doesn't also turn
        R[0] = isCrawling ? 0.0f : rx;
        R[1] = ry;
    }
    if (fabsf(rx) > 1e-4f || fabsf(ry) > 1e-4f) *g_ImmFlags |= IM_RS_PRESENT; else *g_ImmFlags &= ~IM_RS_PRESENT;

    // --- buttons → desired DIKs this frame ---
    const uint16_t wb = st.Gamepad.wButtons;
    const bool A = (wb & XI_A) != 0;
    const bool B = (wb & XI_B) != 0;
    const bool Xb = (wb & XI_X) != 0;
    const bool Y = (wb & XI_Y) != 0;
    const bool LB = (wb & XI_LEFT_SHOULDER) != 0;
    const bool RB = (wb & XI_RIGHT_SHOULDER) != 0;
    const bool L3 = (wb & XI_LEFT_THUMB) != 0;
    const bool R3 = (wb & XI_RIGHT_THUMB) != 0;
    const bool DUp = (wb & XI_DPAD_UP) != 0;
    const bool DL = (wb & XI_DPAD_LEFT) != 0;
    const bool DR = (wb & XI_DPAD_RIGHT) != 0;
    const bool DD = (wb & XI_DPAD_DOWN) != 0;
    const bool Start = (wb & XI_START) != 0;
    const bool Back = (wb & XI_BACK) != 0;
    const bool LT = st.Gamepad.bLeftTrigger > TRIGGER_THRESHOLD;
    const bool RT = st.Gamepad.bRightTrigger > TRIGGER_THRESHOLD;

    // bumpers → OS mouse buttons
    EmitSystemLMB(RB);
    EmitSystemRMB(LB);

    // DPAD Down -> one-shot OS mouse wheel down; must release to re-arm
    if (DD && g_DDArmed) {
        EmitSystemWheelDownPulse();
        g_DDArmed = false;
    }
    else if (!DD) {
        g_DDArmed = true;
    }

    bool desired[256] = { false };

    // Actions mapped to keyboard (respect user binds)
    if (A)     DesireActionDIKs(ACTION_USE, desired);
    if (Y)     DesireActionDIKs(ACTION_WALLSQUASH, desired);
    if (DD)    DesireActionDIKs(ACTION_INVENTORY_SLOT_DOWN, desired);
    if (DUp)   DesireActionDIKs(ACTION_RELOAD, desired);
    if (B)     DesireActionDIKs(ACTION_PICKUP, desired);
    if (Xb)    DesireActionDIKs(ACTION_BLOCK, desired);
    if (L3)    DesireActionDIKs(ACTION_TOGGLE_TORCH, desired);
    if (R3)    DesireActionDIKs(ACTION_CROUCH, desired);
    if (DL)    DesireActionDIKs(ACTION_PEEKL, desired);
    if (DR)    DesireActionDIKs(ACTION_PEEKR, desired);
    if (RT)    DesireActionDIKs(ACTION_LOOKBACK, desired);
    if (LT)    DesireActionDIKs(ACTION_RUN, desired);
    if (Start) DesireActionDIKs(ACTION_MENU, desired);
    if (Back)  DesireActionDIKs(ACTION_INVENT_SWAP, desired);

    // While crawling: convert LS to discrete movement keys via binds
    if (isCrawling) {
        DesireMovementFromStick(lx, ly, desired);
    }

    // Apply DIK downs/ups based on current desired set
    ApplyDesiredKeys(desired);
}

// =====================================================================
// Camera patch – RS yaw+pitch
// =====================================================================
static float CAM_YAW_SENS = 20.0f; // deg/sec scale
static float CAM_PITCH_SENS = 20.0f;

void* TheCamera = (void*)0x76F240;
CVector* CurrentCamLookAtOffsetFromPlayer = (CVector*)((char*)TheCamera + 1536);
CVector* CurrentCamOffsetFromPlayer = (CVector*)((char*)TheCamera + 1520);
CVector* CurrentRevCamOffsetFromPlayer = (CVector*)((char*)TheCamera + 1504);

static float CamMinYAngle = -29.0f;
static float CamMaxYAngle = 42.0f;
static float CamCurrYAngle = 15.0f;
static float CAM_FREELOOK_CLAMP = 120.0f;
static float CamYawOffset = 0.0f;
static bool  WasAiming = false;

static inline float Deg2Rad(float x) { return x * (3.14159265358979323846f / 180.0f); }
static inline float DegWrap(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}
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

    float* PlrCurrYAngle = (float*)((char*)This + 4400);
    bool* PlrActionLookBack = (bool*)((char*)This + 1232);
    bool* PlrActionLockOn = (bool*)((char*)This + 1352);
    int* PlrCurrState = (int*)((char*)This + 956);
    int* PlrCurrWpnType = (int*)((char*)This + 968);
    int* PlrLockOnHunter = (int*)((char*)This + 4500);
    CVector2d* RS = (CVector2d*)((char*)This + 1184);
    const float dt = *(float*)0x6ECE68;

    const float RSx = RS->x * (g_InvertLookX ? -1.0f : 1.0f);
    const float RSy = RS->y * (g_InvertLookY ? -1.0f : 1.0f);

    const bool rmbHeld = g_SysRMBDown;
    const bool inAimState = (*PlrCurrState == PEDSTATE_AIM) || (*PlrCurrState == PEDSTATE_CROUCHAIM) ||
        (*PlrLockOnHunter && *PlrActionLockOn);
    const bool hasFirearmNow = HasFirearm(*PlrCurrWpnType);

    const bool firearmAimNoMouse = hasFirearmNow && (rmbHeld || inAimState);
    const bool lbRotateNoGun = rmbHeld && !hasFirearmNow;
    const bool aimCamNow = (firearmAimNoMouse || lbRotateNoGun);

    // Aim edge handling: commit yaw on enter AND exit to prevent drift
    const bool wasAimingPrev = WasAiming;
    WasAiming = aimCamNow;
    if (aimCamNow && !wasAimingPrev) {
        *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
        CamYawOffset = 0.0f;
    }
    else if (!aimCamNow && wasAimingPrev) {
        *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
        CamYawOffset = 0.0f;
    }

    // RS.x -> yaw offset (used both in aim and freelook)
    if (fabsf(RSx) > 0.0005f) {
        CamYawOffset = DegWrap(CamYawOffset + RSx * dt * CAM_YAW_SENS);
        if (CamYawOffset > CAM_FREELOOK_CLAMP) CamYawOffset = CAM_FREELOOK_CLAMP;
        if (CamYawOffset < -CAM_FREELOOK_CLAMP) CamYawOffset = -CAM_FREELOOK_CLAMP;
    }

    // RS.y -> pitch
    if (fabsf(RSy) > 0.0005f) {
        CamCurrYAngle += RSy * dt * CAM_PITCH_SENS;
        if (CamCurrYAngle > CamMaxYAngle) CamCurrYAngle = CamMaxYAngle;
        if (CamCurrYAngle < CamMinYAngle) CamCurrYAngle = CamMinYAngle;
    }

    // Auto-recenter behind player when moving (not aiming) and not steering RS.x
    CVector2d* LS = (CVector2d*)((char*)This + 1168);
    float moveMag = (LS) ? sqrtf(LS->x * LS->x + LS->y * LS->y) : 0.0f;
    if (!aimCamNow && moveMag > 0.15f && fabsf(RSx) < 0.05f && fabsf(CamYawOffset) > 1e-3f) {
        const float CAM_RECENTER_SPEED = 240.0f; // deg/sec
        float step = CAM_RECENTER_SPEED * dt;
        if (fabsf(CamYawOffset) <= step) CamYawOffset = 0.0f;
        else CamYawOffset += (CamYawOffset > 0.0f ? -step : step);
    }

    // Build base camera offset
    CVector CamPosOffsetFromPlayer;
    ((CVector * (__thiscall*)(void*, CVector*, CMatrix*, bool))0x5AF950)(This, plrPos, plrRot, false);

    if (!*(bool*)0x789819 && *PlrActionLookBack) {
        *(int*)0x76F244 = 0; *(int*)0x76F24C = 0;
        if (*(bool*)0x76F4FD && *(bool*)0x76F4FE) { CamPosOffsetFromPlayer = *CurrentRevCamOffsetFromPlayer; CamCurrYAngle = -CamCurrYAngle; }
        else { *(bool*)0x76F4FD = true; CamPosOffsetFromPlayer = *CurrentCamOffsetFromPlayer; }
    }
    else if (*PlrCurrState == PEDSTATE_CRAWL || ((bool(__cdecl*)(void*))0x515260)(This)) {
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
        if (*PlrCurrState == PEDSTATE_CLIMB || *PlrCurrState == PEDSTATE_CRAWL ||
            (*PlrLockOnHunter && *PlrActionLockOn) || *(bool*)0x76F860 || !*(bool*)0x75B348)
        {
            CamCurrYAngle = *PlrCurrYAngle;
            CurrentCamLookAtOffsetFromPlayer->x = 0.03f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
        }
        else {
            *PlrCurrYAngle = CamCurrYAngle;
        }
    }
    else if (*PlrCurrState == PEDSTATE_NORMAL) {
        CurrentCamLookAtOffsetFromPlayer->x = 0.03f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }
    else if (*PlrCurrState == PEDSTATE_PEEK && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.020f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_PEEK && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.020f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_AIM && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.041f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.03f;
    }
    else if (*PlrCurrState == PEDSTATE_AIM && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.041f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.03f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCH && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCH && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.036f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.05f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCHAIM && *PlrCurrWpnType == WEAPONTYPE_1HFIREARM) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.02f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.02f;
    }
    else if (*PlrCurrState == PEDSTATE_CROUCHAIM && *PlrCurrWpnType == WEAPONTYPE_2HFIREARM && !*(bool*)0x75B348) {
        CurrentCamLookAtOffsetFromPlayer->x = -0.02f; CurrentCamLookAtOffsetFromPlayer->y = -0.012f; CurrentCamLookAtOffsetFromPlayer->z = 0.02f;
    }
    else if (*PlrCurrState == PEDSTATE_WALLSQASH) {
        CurrentCamLookAtOffsetFromPlayer->x = 0.01f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }
    else {
        CurrentCamLookAtOffsetFromPlayer->x = 0.01f; CurrentCamLookAtOffsetFromPlayer->y = -0.31f; CurrentCamLookAtOffsetFromPlayer->z = 0.55f;
    }

    // Pitch rotate, then apply yaw offset around player
    CMatrix CamPitchRot;
    ((CMatrix * (__thiscall*)(CMatrix*, CVector*, float))0x57E440)(&CamPitchRot, (CVector*)0x6B3218, CamCurrYAngle);
    ((CVector * (__thiscall*)(CVector*, CVector*, CMatrix*))0x580390)(&CamPosOffsetFromPlayer, &CamPosOffsetFromPlayer, &CamPitchRot);

    if (fabsf(CamYawOffset) > 1e-4f) {
        RotateYLocal(&CamPosOffsetFromPlayer, CamYawOffset);
    }

    ((CVector * (__thiscall*)(CVector*, CVector*, CMatrix*))0x580390)(&CamPosOffsetFromPlayer, &CamPosOffsetFromPlayer, plrRot);
    ((CVector * (__cdecl*)(CVector*, CVector*, CVector*))0x60D800)(gCamPosFromPlr, plrPos, &CamPosOffsetFromPlayer);

    return camPos;
}

// Keep instant aim interpolation
void __cdecl InterpolateAimAngle(float* curAngle, float* /*angleDelta*/, float targetAngle, float /*speed*/, float /*minDelta*/) {
    *curAngle = targetAngle;
}

// Crosshair (unchanged)
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

// Entry (publisher path drives keyboard provider)
extern "C" void MH2_XInputGlue_Install() {}

#endif // MH2_XINPUT_GLUE_H
