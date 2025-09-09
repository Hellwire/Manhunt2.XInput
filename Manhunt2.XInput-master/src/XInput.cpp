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

static void MH2_LoadConfig()
{
    if (!g_IniPath[0]) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wchar_t* slash = wcsrchr(exe, L'\\');
        if (slash) *(slash + 1) = L'\0'; else wcscpy_s(exe, L".\\");
        wcscpy_s(g_IniPath, exe);
        wcscat_s(g_IniPath, L"Manhunt2.XInput.ini");
    }
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
static inline int TripletCapacity() {
    uintptr_t bytes = (uintptr_t)gTripEnd - (uintptr_t)gTripBase;
    return (int)(bytes / (sizeof(int) * 3)); // usually 10
}

// Internal mouse wheel codes
static const int MWHEEL_UP_CODE = 0x95;
static const int MWHEEL_DOWN_CODE = 0x96;

// Scrub any wheel triplets in-place
static inline void ScrubMouseWheelTriplets() {
    volatile int* p = gTripBase;
    while (p < gTripEnd) {
        const int code = p[0];
        if (code == MWHEEL_UP_CODE || code == MWHEEL_DOWN_CODE) {
            p[1] = 0; p[2] = 0;
        }
        p += 3;
    }
}
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
static void WriteTripletsDeterministic(const int* codes, const int* pressed, int count) {
    int cap = TripletCapacity();
    if (count > cap) count = cap;
    volatile int* p = gTripBase;
    for (int i = 0; i < count; ++i) { p[0] = codes[i]; p[1] = pressed[i] ? 1 : 0; p[2] = 0; p += 3; }
    if (count < cap) { p[0] = INPUT_SENTINEL; p[1] = 0; p[2] = 0; p += 3; }
    while (p < gTripEnd) { p[0] = INPUT_SENTINEL; p[1] = 0; p[2] = 0; p += 3; }
}
static inline void PublishBoundCodesSafe(int actionIdx, int& n, int* codes, int* pressed) {
    auto push = [&](int c) {
        if (!c || c == INPUT_SENTINEL) return;
        if (c == MWHEEL_UP_CODE || c == MWHEEL_DOWN_CODE) return;
        for (int i = 0; i < n; ++i) if (codes[i] == c) { pressed[i] = 1; return; }
        codes[n] = c; pressed[n] = 1; ++n;
        };
    const int c1 = (int)(uint8_t)g_ActionMapPrimary[actionIdx].Code;
    const int c2 = (int)(uint8_t)g_ActionMapSecondary[actionIdx].Code;
    push(c1); if (c2 && c2 != c1) push(c2);
}
static inline void ScrubActionTriplets(int actionIdx) {
    const int c1 = (int)(uint8_t)g_ActionMapPrimary[actionIdx].Code;
    const int c2 = (int)(uint8_t)g_ActionMapSecondary[actionIdx].Code;
    volatile int* p = gTripBase;
    while (p < gTripEnd) {
        const int code = p[0];
        if (code == c1 || code == c2) { p[1] = 0; p[2] = 0; }
        p += 3;
    }
}

// Convert LS direction into pressed keyboard actions (respects user's keybinds via action maps)
static inline void PublishMovementFromStickAsKeyboard(float lx, float ly, int& n, int* codes, int* pressed)
{
    // deadzone / threshold for discrete WASD
    const float TH = 0.35f;

    if (ly > TH) PublishBoundCodesSafe(ACTION_MOVE_FORWARD, n, codes, pressed); // W
    if (ly < -TH) PublishBoundCodesSafe(ACTION_MOVE_BACKWARDS, n, codes, pressed); // S
    if (lx < -TH) PublishBoundCodesSafe(ACTION_MOVE_LEFT, n, codes, pressed); // A
    if (lx > TH) PublishBoundCodesSafe(ACTION_MOVE_RIGHT, n, codes, pressed); // D
}


// ==== OS-level mouse buttons (bumpers only): RB->LMB, LB->RMB ====
static bool g_SysLMBDown = false;
static bool g_SysRMBDown = false;
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

// ==========================
// Init
// ==========================
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
    MH2_LoadConfig();
}

// ==========================
// Per-frame glue
// ==========================
static void MH2_UpdateXInputForPlayer(void* playerInst) {
    ScrubMouseWheelTriplets();
    MH2_InitXInput();
    if (!g_XIEnabled || !g_XIGetState) return;

    MH2_XINPUT_STATE st{}; DWORD xi = g_XIGetState(0, &st);

    bool hasFirearmForThisFrame = false;
    if (playerInst) {
        int* PlrCurrWpnType = (int*)((char*)playerInst + 968);
        hasFirearmForThisFrame = PlrCurrWpnType && HasFirearm(*PlrCurrWpnType);
    }

    // Triplet buffers (we'll also add movement into these when crawling)
    int  codes[16];  int pressed[16]; int n = 0;

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
        WriteTripletsDeterministic(codes, pressed, 0);
        ScrubMouseWheelTriplets();
        return;
    }

    // --- sticks (LS/RS) ---
    const int DZ_L = 7849, DZ_R = 8689;
    float lx = MH2_NormStick(st.Gamepad.sThumbLX, DZ_L);
    float ly = MH2_NormStick(st.Gamepad.sThumbLY, DZ_L);
    float rx = MH2_NormStick(st.Gamepad.sThumbRX, DZ_R);
    float ry = MH2_NormStick(st.Gamepad.sThumbRY, DZ_R);

    // Determine crawl state
    bool isCrawling = false;
    int* PlrCurrState = nullptr;
    if (playerInst) {
        PlrCurrState = (int*)((char*)playerInst + 956); // same offset you use elsewhere
        isCrawling = (PlrCurrState && *PlrCurrState == PEDSTATE_CRAWL);
    }

    // Publish LS and flags normally, BUT override while crawling
    *g_LS_X = lx;
    *g_LS_Y = ly;

    {   // LS flags (used by engine); clear them when crawling
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
        // If crawling, zero LS to fully disable analog move for the engine path
        L[0] = isCrawling ? 0.0f : lx;
        L[1] = isCrawling ? 0.0f : ly;
        R[0] = rx;
        R[1] = ry;
    }
    if (fabsf(rx) > 1e-4f || fabsf(ry) > 1e-4f) *g_ImmFlags |= IM_RS_PRESENT; else *g_ImmFlags &= ~IM_RS_PRESENT;

    // --- buttons → actions ---
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

    // bumpers → OS mouse buttons
    EmitSystemLMB(RB);
    EmitSystemRMB(LB);

    // --- PUBLISH ACTIONS ---
    if (A)     PublishBoundCodesSafe(ACTION_USE, n, codes, pressed);
    if (Y)     PublishBoundCodesSafe(ACTION_WALLSQUASH, n, codes, pressed);
    if (DD)    PublishBoundCodesSafe(ACTION_INVENTORY_SLOT2, n, codes, pressed);
    if (DUp)   PublishBoundCodesSafe(ACTION_RELOAD, n, codes, pressed);
    if (B)     PublishBoundCodesSafe(ACTION_PICKUP, n, codes, pressed);
    if (Xb)    PublishBoundCodesSafe(ACTION_BLOCK, n, codes, pressed);
    if (L3)    PublishBoundCodesSafe(ACTION_GRAPPLE, n, codes, pressed);
    if (DL)    PublishBoundCodesSafe(ACTION_PEEKL, n, codes, pressed);
    if (DR)    PublishBoundCodesSafe(ACTION_PEEKR, n, codes, pressed);
    if (RT)    PublishBoundCodesSafe(ACTION_LOOKBACK, n, codes, pressed);
    if (LT)    PublishBoundCodesSafe(ACTION_RUN, n, codes, pressed);
    if (Start) PublishBoundCodesSafe(ACTION_MENU, n, codes, pressed);
    if (Back)  PublishBoundCodesSafe(ACTION_INVENT_SWAP, n, codes, pressed);

    // --- CRAWL OVERRIDE: LS → WASD via keyboard provider ---
    if (isCrawling) {
        // Ensure analog is fully ignored for movement
        *g_LS_X = 0.0f;
        *g_LS_Y = 0.0f;

        // Convert LS axes to movement key actions (uses user’s keybinds)
        PublishMovementFromStickAsKeyboard(lx, ly, n, codes, pressed);
    }

    // Finalize triplets (deterministic, wheel scrub like before)
    StripWheelCodes(n, codes, pressed);
    WriteTripletsDeterministic(codes, pressed, n);
    ScrubMouseWheelTriplets();
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
