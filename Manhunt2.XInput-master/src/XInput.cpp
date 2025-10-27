#pragma once
#include "pch.h"
#include "XInput.h"
#include "MemoryMgr.h"

#ifndef MH2_XINPUT_GLUE_H
#define MH2_XINPUT_GLUE_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <wchar.h>
#include "CInput.h"
#include "keyboard.h"

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


// --- BEGIN PATCH: Scoped globals & knobs (weapon-type based) ---
// Scoped aim deltas (degrees per frame), produced in input update and consumed in camera
static float g_ScopedYawDeltaDeg = 0.0f;
static float g_ScopedPitchDeltaDeg = 0.0f;

// Configurable clamp for aim; already loaded from INI
static float AIM_PITCH_MIN = -29.0f;
static float AIM_PITCH_MAX = 42.0f;

// New: edge/state helpers for scoped entry/exit and a one-frame delta mute
static bool  g_WasScopedZoom = false;
static int   g_MuteScopedMouseFrames = 0; // when >0, skip emitting OS mouse dx/dy this frame

// Optional: small bias to apply once on scoped entry (deg). 0 by default.
static float g_ScopedPitchBiasDeg = 0.0f;
// --- END PATCH: Scoped globals & knobs ---


// ===== triplet table scanned by the keyboard provider =====
static volatile int* const gTripBase = (int*)0x0076BEA0; // [code, pressed, extra] x N
static volatile int* const gTripEnd = (int*)0x0076BF18;

// ===== mapping tables (primary/secondary) =====
static KeyCode* const g_ActionMapPrimary = (KeyCode*)0x0076E1C8;
static KeyCode* const g_ActionMapSecondary = (KeyCode*)0x0076E2A0;

// ===== Config (INI) =====
static bool   g_InvertLookX = false; // RS.x
static bool   g_InvertLookY = false; // RS.y
static wchar_t g_IniPath[MAX_PATH] = L"";

// === NEW: General config flags ===
static bool g_ModEnabled = true;            // [General] EnableMod
static bool g_BlockOnConflict = true;       // [General] BlockIfConflicts
static bool g_LogToConsole = true;          // [General] LogToConsole
// -1 = ignore (do nothing), 0 = force cursor HIDDEN when pad connected, 1 = force VISIBLE when pad connected
static int  g_MouseCursorWithGamepad = -1;  // [General] MouseCursorWithGamepad


// === NEW: console logging ===
static bool g_ConsoleReady = false;
static void MH2_EnsureConsole() {
    if (g_LogToConsole && !g_ConsoleReady) {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        g_ConsoleReady = true;
    }
}
static void MH2_Log(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list args; va_start(args, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
    if (g_LogToConsole && g_ConsoleReady) {
        wprintf(L"%s\n", buf);
    }
}

// === NEW: small helpers ===
static bool FileExistsW(const wchar_t* path) {
    DWORD a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// === NEW: cursor control when gamepad connects ===
static int  g_LastCursorTarget = -2;      // -2 = unknown, 0 = hidden, 1 = shown
static bool g_LastGamepadConnected = false;

static void EnsureCursorVisible(bool show) {
    // Drive ShowCursor's internal counter to the desired state
    if (show) { while (ShowCursor(TRUE) < 0) {} }
    else { while (ShowCursor(FALSE) >= 0) {} }
}


// ===== XInput ====
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, MH2_XINPUT_STATE*);
typedef VOID(WINAPI* PFN_XInputEnable)(BOOL);
static HMODULE            g_hXInput = nullptr;
static PFN_XInputGetState g_XIGetState = nullptr;
static PFN_XInputEnable   g_XIEnable = nullptr;
static bool               g_XIInited = false;
static bool               g_XIEnabled = true;

extern "C" void __cdecl MH2_XInputEnableShim(uint8_t enable) { g_XIEnabled = (enable != 0); }

// ===== Crawl → mouse delta (no INI) =====
static const float CRAWL_MOUSE_X_SENS = 1500.0f; // pixels/sec at full RS deflection
static float s_CrawlMouseXAcc = 0.0f;           // fractional accumulator for dx

// Aim → mouse delta (for firearms, e.g., sniper scope)
static const float AIM_MOUSE_X_SENS = 2000.0f;   // pixels/sec at full RS deflection
static const float AIM_MOUSE_Y_SENS = 2000.0f;
static float s_AimMouseXAcc = 0.0f;
static float s_AimMouseYAcc = 0.0f;

// Camera sensitivity constants (configurable via INI)
static float CAM_YAW_SENS = 20.0f; // deg/sec scale
static float CAM_PITCH_SENS = 20.0f;
static float AIM_MOUSE_X_SENS_CONFIG = 1500.0f; // pixels/sec at full RS deflection
static float AIM_MOUSE_Y_SENS_CONFIG = 1000.0f;

// ==== QTM (stealth minigame) detection & control ====
static bool  g_QtmActive = false;   // live flag
static bool  g_QtmWasActive = false;   // edge tracking
static int   g_QtmSeenFrames = 0;       // countdown refreshed by "QTMGO" lookups
static float QTM_MOUSE_X_SENS = 1800.0f; // pixels/sec at full RS deflection
static float QTM_MOUSE_Y_SENS = 1800.0f;
static float s_QtmMouseXAcc = 0.0f;    // subpixel accumulators
static float s_QtmMouseYAcc = 0.0f;


static volatile int* const gTripPressedBase = (int*)0x0076BEA4;
static volatile int* const gTripEndGuard = (int*)0x0076BF18;
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

static inline void WriteJump(void* at, void* to) {
    DWORD old; VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    uintptr_t rel = (uintptr_t)to - ((uintptr_t)at + 5);
    uint8_t jmp[5] = { 0xE9, (uint8_t)rel, (uint8_t)(rel >> 8), (uint8_t)(rel >> 16), (uint8_t)(rel >> 24) };
    memcpy(at, jmp, 5);
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
}
static inline void WriteBytes(void* at, const uint8_t* bytes, size_t n) {
    DWORD old; VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

static void MH2_LoadConfig()
{
    if (!g_IniPath[0]) {
        wchar_t dllDir[MAX_PATH];
        GetDllFolder(dllDir, MAX_PATH);

        // Always create/read INI in DLL folder
        wcscpy_s(g_IniPath, dllDir);
        wcscat_s(g_IniPath, L"Manhunt2.XInput.ini");
    }

    // Create ini with defaults if missing
    DWORD attr = GetFileAttributesW(g_IniPath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        // General defaults
        WritePrivateProfileStringW(L"General", L"EnableMod", L"1", g_IniPath);
        WritePrivateProfileStringW(L"General", L"BlockIfConflicts", L"1", g_IniPath);
        WritePrivateProfileStringW(L"General", L"LogToConsole", L"0", g_IniPath);
        // -1 ignore, 0 = hide cursor on pad connect, 1 = show cursor on pad connect
        WritePrivateProfileStringW(L"General", L"MouseCursorWithGamepad", L"0", g_IniPath);

        // Existing camera defaults
        WritePrivateProfileStringW(L"Camera", L"InvertLookX", L"1", g_IniPath);
        WritePrivateProfileStringW(L"Camera", L"InvertLookY", L"1", g_IniPath);
        
        // Sensitivity defaults
        WritePrivateProfileStringW(L"Camera", L"YawSensitivity", L"20.0", g_IniPath);
        WritePrivateProfileStringW(L"Camera", L"PitchSensitivity", L"20.0", g_IniPath);
        WritePrivateProfileStringW(L"Aim", L"MouseXSensitivity", L"400.0", g_IniPath);
        WritePrivateProfileStringW(L"Aim", L"MouseYSensitivity", L"800.0", g_IniPath);

        // Aim pitch clamp defaults
        WritePrivateProfileStringW(L"Aim", L"PitchMinDeg", L"-29.0", g_IniPath);
        WritePrivateProfileStringW(L"Aim", L"PitchMaxDeg", L"82.0", g_IniPath);
        WritePrivateProfileStringW(L"Aim", L"ScopedPitchBiasDeg", L"0.0", g_IniPath);

        // in the "create ini with defaults" block:
        WritePrivateProfileStringW(L"QTM", L"MouseXSensitivity", L"100.0", g_IniPath);
        WritePrivateProfileStringW(L"QTM", L"MouseYSensitivity", L"100.0", g_IniPath);


    }

    // Read values
    g_ModEnabled = GetPrivateProfileIntW(L"General", L"EnableMod", 1, g_IniPath) != 0;
    g_BlockOnConflict = GetPrivateProfileIntW(L"General", L"BlockIfConflicts", 1, g_IniPath) != 0;
    g_LogToConsole = GetPrivateProfileIntW(L"General", L"LogToConsole", 1, g_IniPath) != 0;
    g_MouseCursorWithGamepad = GetPrivateProfileIntW(L"General", L"MouseCursorWithGamepad", -1, g_IniPath);

    g_InvertLookX = GetPrivateProfileIntW(L"Camera", L"InvertLookX", 0, g_IniPath) != 0;
    g_InvertLookY = GetPrivateProfileIntW(L"Camera", L"InvertLookY", 0, g_IniPath) != 0;
    
    // Read sensitivity values
    wchar_t sensBuf[32];
    GetPrivateProfileStringW(L"Camera", L"YawSensitivity", L"20.0", sensBuf, 32, g_IniPath);
    CAM_YAW_SENS = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Camera", L"PitchSensitivity", L"20.0", sensBuf, 32, g_IniPath);
    CAM_PITCH_SENS = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Aim", L"MouseXSensitivity", L"400.0", sensBuf, 32, g_IniPath);
    AIM_MOUSE_X_SENS_CONFIG = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Aim", L"MouseYSensitivity", L"800.0", sensBuf, 32, g_IniPath);
    AIM_MOUSE_Y_SENS_CONFIG = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Aim", L"PitchMinDeg", L"-29.0", sensBuf, 32, g_IniPath);
    AIM_PITCH_MIN = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Aim", L"PitchMaxDeg", L"82.0", sensBuf, 32, g_IniPath);
    AIM_PITCH_MAX = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"Aim", L"ScopedPitchBiasDeg", L"0.0", sensBuf, 32, g_IniPath);
    g_ScopedPitchBiasDeg = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"QTM", L"MouseXSensitivity", L"100.0", sensBuf, 32, g_IniPath);
    QTM_MOUSE_X_SENS = (float)_wtof(sensBuf);
    GetPrivateProfileStringW(L"QTM", L"MouseYSensitivity", L"100.0", sensBuf, 32, g_IniPath);
    QTM_MOUSE_Y_SENS = (float)_wtof(sensBuf);


}

static bool HasConflictingASI() {
    // Check already-loaded modules (case-insensitive names work)
    if (GetModuleHandleW(L"absolutecamera.asi") || GetModuleHandleW(L"AbsoluteCamera.asi") ||
        GetModuleHandleW(L"mh2patch.asi") || GetModuleHandleW(L"MH2Patch.asi"))
        return true;

    // Also check if the files just exist in the same folder as this ASI
    wchar_t dir[MAX_PATH]; GetDllFolder(dir, MAX_PATH);
    const wchar_t* names[] = { L"absolutecamera.asi", L"AbsoluteCamera.asi",
                               L"mh2patch.asi",       L"MH2Patch.asi" };
    wchar_t path[MAX_PATH];
    for (auto n : names) {
        wcscpy_s(path, dir); wcscat_s(path, n);
        if (FileExistsW(path)) return true;
    }
    return false;
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

// ==== RepeatGate (menus) ====
// One-frame pulses for DPAD; slightly longer pulse for A/X so menus never miss them.
struct RepeatGate { float tHeld; float nextAt; bool wasDown; };
static RepeatGate gRepeat[256] = {};          // repeat timing per DIK

// Hold counters (in "menu ticks") for one-shot pulses.
// 0 = not held. >0 = keep key down; decrement each MH2_MenuTick_DIOnly() then release at 0.
static uint8_t     sPulseHold[256] = {};      // replaces sPulseRelease[]
static bool        sEdgeDown[256] = {};      // edge detector for non-repeating keys

// Fast repeat values (snappy DPAD)
static const float REPEAT_INITIAL = 0.05f;    // seconds to first repeat
static const float REPEAT_RATE = 0.05f;    // seconds between repeats

// Default pulse lengths (in menu ticks). DPAD uses 1. Confirm buttons get a few ticks.
static const uint8_t PULSE_FRAMES_DEFAULT = 1;
static const uint8_t PULSE_FRAMES_CONFIRM = 3;  // RETURN/SPACE => A/X; small "stick" to survive sampling

// Emit a pulse and hold it for N menu ticks
static inline void PulseDIK(uint8_t dik, uint8_t frames = PULSE_FRAMES_DEFAULT) {
    if (!sKeyDown[dik]) EmitKeyDown_DIK(dik);
    // Extend hold if already down
    if (sPulseHold[dik] < frames) sPulseHold[dik] = frames;
}

// Decrement holds and release when they expire
static inline void HandlePulseReleases() {
    for (int c = 1; c < 256; ++c) {
        if (sPulseHold[c]) {
            if (--sPulseHold[c] == 0) {
                EmitKeyUp_DIK((uint8_t)c);
            }
        }
    }
}

// Repeating gate for arrows / nav (unchanged behavior)
static inline void UpdateRepeat(bool want, uint8_t dik, float dt) {
    RepeatGate& g = gRepeat[dik];
    if (want) {
        if (!g.wasDown) { g.wasDown = true; g.tHeld = 0.f; g.nextAt = REPEAT_INITIAL; PulseDIK(dik, PULSE_FRAMES_DEFAULT); }
        else {
            g.tHeld += dt;
            if (g.tHeld >= g.nextAt) { PulseDIK(dik, PULSE_FRAMES_DEFAULT); g.nextAt += REPEAT_RATE; }
        }
    }
    else {
        g.wasDown = false; g.tHeld = 0.f; g.nextAt = REPEAT_INITIAL;
    }
}

// One-shot edge (no repeat). Use default 1-tick unless caller overrides.
static inline void UpdateEdgeEx(bool want, uint8_t dik, uint8_t frames) {
    if (want && !sEdgeDown[dik]) { PulseDIK(dik, frames); sEdgeDown[dik] = true; }
    else if (!want && sEdgeDown[dik]) { sEdgeDown[dik] = false; }
}
static inline void UpdateEdge(bool want, uint8_t dik) {
    UpdateEdgeEx(want, dik, PULSE_FRAMES_DEFAULT);
}

// Reset menu gates when entering a menu (prevents sticky edges)
static inline void ResetMenuGates() {
    memset((void*)sEdgeDown, 0, sizeof(sEdgeDown));
    memset((void*)sPulseHold, 0, sizeof(sPulseHold));
    memset((void*)gRepeat, 0, sizeof(gRepeat));
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

/// MENU CONTROLS ///

// === NEW: DIK scan codes (if not already defined) ===
#ifndef DIK_ESCAPE
#define DIK_ESCAPE   0x01
#define DIK_BACK     0x0E
#define DIK_TAB      0x0F
#define DIK_RETURN   0x1C
#define DIK_SPACE    0x39
#define DIK_UP       0xC8
#define DIK_LEFT     0xCB
#define DIK_RIGHT    0xCD
#define DIK_DOWN     0xD0
#define DIK_PGUP     0xC9
#define DIK_PGDN     0xD1
#endif

// === NEW: state taps for robust menu detection ===
static volatile uint32_t* const g_GameState = (uint32_t*)0x006EC990; // main state machine
static volatile uint8_t* const g_MenuMgr = (uint8_t*)0x0075F110; // FUN_00562df0 result
static volatile uintptr_t* const g_BinkHandle = (uintptr_t*)0x0079E708; // nonzero if .bik is playing
static volatile float* const g_MouseDX = (float*)0x0076BE74;
static volatile float* const g_MouseDY = (float*)0x0076BE78;


static inline bool IsMenuActive() {
    const uint32_t st = *g_GameState;
    if (st == 1u || st == 2u || st == 7u || st == 0xCu) return true;
    if (g_MenuMgr && g_MenuMgr[0xF4] != 0) return true;
    if (*g_BinkHandle != 0) return true;
    return false;
}

// Remember last mode so we can release keys when switching
static bool sWasInMenu = false;

// ===== QTM HUD "is active" detour (no strings) =========================
static const uintptr_t ADDR_QTM_HUD = 0x00596FB0; // verify in your build

static uint8_t  g_QtmHud_OrigBytes[8] = { 0 };
static size_t   g_QtmHud_StolenLen = 0;
static uint8_t* g_QtmHud_Gateway = nullptr;
static bool     g_QtmHudHookInstalled = false;

static void __stdcall OnQtmHud(void* hudThis)
{
    // QTM HUD alpha gate (alpha > 0 => QTM visible this frame)
    const float alpha = *(float*)((uint8_t*)hudThis + 0x30);

    // Keep a heartbeat for the gameplay side (even if it lags)
    if (alpha > 0.0f) g_QtmSeenFrames = 6;

    // --- Robust dt (don’t rely on engine dt while HUD is drawing) ---
    static LARGE_INTEGER s_freq = {};
    static LARGE_INTEGER s_prev = {};
    if (!s_freq.QuadPart) {
        QueryPerformanceFrequency(&s_freq);
        QueryPerformanceCounter(&s_prev);
    }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    double dt = double(now.QuadPart - s_prev.QuadPart) / double(s_freq.QuadPart);
    s_prev = now;
    // Clamp dt to sane range so pauses/spikes don’t explode movement
    if (dt < 1.0 / 240.0) dt = 1.0 / 240.0;
    if (dt > 1.0 / 20.0)  dt = 1.0 / 20.0;

    // One-time HUD show/hide logs
    static bool s_hudShown = false;
    if (alpha > 0.0f) {
        if (!s_hudShown) {
            MH2_Log(L"[QTM] DEBUGINFO: HUD visible (alpha=%.3f)", alpha);
            s_hudShown = true;
        }
    }

    // If not visible this frame, nothing to drive.
    if (!(alpha > 0.0f) || !g_XIGetState) return;

    // Read pad
    MH2_XINPUT_STATE st{};
    if (g_XIGetState(0, &st) != ERROR_SUCCESS) return;

    // Normalize RS (same DZ as gameplay)
    auto Norm = [](int16_t v, int dz) -> float {
        int iv = (int)v;
        if (iv >= -dz && iv <= dz) return 0.0f;
        const int max = 32767;
        float sign = (iv < 0) ? -1.0f : 1.0f;
        float mag = (float)(abs(iv) - dz) / (float)(max - dz);
        if (mag < 0.0f) mag = 0.0f; if (mag > 1.0f) mag = 1.0f;
        return sign * mag;
        };
    const int DZ_R = 8689;
    const float rx = Norm(st.Gamepad.sThumbRX, DZ_R);
    const float ry = Norm(st.Gamepad.sThumbRY, DZ_R);

    // Use QTM sens + invert, independent of engine dt
    static float accX = 0.0f, accY = 0.0f;
    const float dxF = rx * (float)dt * QTM_MOUSE_X_SENS * (g_InvertLookX ? 1.0f : -1.0f);
    const float dyF = -ry * (float)dt * QTM_MOUSE_Y_SENS * (g_InvertLookY ? 1.0f : -1.0f);

    accX += dxF;
    accY += dyF;

    const int dx = (int)((accX >= 0.0f) ? (accX + 0.5f) : (accX - 0.5f));
    const int dy = (int)((accY >= 0.0f) ? (accY + 0.5f) : (accY - 0.5f));

    if (dx | dy) {
        // 1) Feed the game’s internal mouse-delta slots
        *g_MouseDX += (float)dx;
        *g_MouseDY += (float)dy;

        // 2) ALSO emit OS relative mouse movement (some QTM paths read OS input)
        INPUT in{}; in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_MOVE;
        in.mi.dx = dx; in.mi.dy = dy;
        SendInput(1, &in, sizeof(in));

        accX -= (float)dx;
        accY -= (float)dy;
    }
}



static void __declspec(naked) QtmHud_Hook()
{
    __asm {
        // Preserve state
        pushad
        pushfd

        mov     eax, ecx     // __thiscall 'this'
        push    eax
        call    OnQtmHud

        popfd
        popad

        jmp     g_QtmHud_Gateway
    }
}

static void InstallQtmHudHook()
{
    if (g_QtmHudHookInstalled) return;

    // Heuristic: detect common prologue
    const uint8_t* p = (const uint8_t*)ADDR_QTM_HUD;
    size_t steal = 5; // minimum for JMP

    // Typical: 55 8B EC 83 EC imm8   -> 6 bytes
    if (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC) {
        if (p[3] == 0x83 && p[4] == 0xEC) {
            steal = 6; // take full SUB too so we don't split it
        }
        else {
            steal = 5; // PUSH/MOV only; safe to take 5
        }
    }

    g_QtmHud_StolenLen = steal;
    memcpy(g_QtmHud_OrigBytes, (void*)ADDR_QTM_HUD, g_QtmHud_StolenLen);

    // Gateway = [orig bytes][jmp back]
    g_QtmHud_Gateway = (uint8_t*)VirtualAlloc(nullptr, g_QtmHud_StolenLen + 5,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(g_QtmHud_Gateway, g_QtmHud_OrigBytes, g_QtmHud_StolenLen);

    // jmp from gateway -> (ADDR + stolenLen)
    {
        const uintptr_t srcNext = (uintptr_t)(g_QtmHud_Gateway + g_QtmHud_StolenLen + 5);
        const uintptr_t dst = (ADDR_QTM_HUD + g_QtmHud_StolenLen);
        const int32_t   rel = (int32_t)(dst - srcNext);
        g_QtmHud_Gateway[g_QtmHud_StolenLen] = 0xE9;
        *(int32_t*)(g_QtmHud_Gateway + g_QtmHud_StolenLen + 1) = rel;
    }

    // Patch target with JMP hook (over exactly stolenLen bytes)
    DWORD oldProt;
    VirtualProtect((void*)ADDR_QTM_HUD, g_QtmHud_StolenLen, PAGE_EXECUTE_READWRITE, &oldProt);
    {
        // Fill with NOPs first (helps if stolenLen > 5)
        for (size_t i = 0; i < g_QtmHud_StolenLen; ++i) ((uint8_t*)ADDR_QTM_HUD)[i] = 0x90;

        const uintptr_t srcNext = ADDR_QTM_HUD + 5;
        const uintptr_t dst = (uintptr_t)&QtmHud_Hook;
        const int32_t   rel = (int32_t)(dst - srcNext);

        ((uint8_t*)ADDR_QTM_HUD)[0] = 0xE9;
        *(int32_t*)((uint8_t*)ADDR_QTM_HUD + 1) = rel;
    }
    VirtualProtect((void*)ADDR_QTM_HUD, g_QtmHud_StolenLen, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), (void*)ADDR_QTM_HUD, g_QtmHud_StolenLen);

    g_QtmHudHookInstalled = true;
    MH2_Log(L"[QTM] HUD hook installed @ 0x%08X (stole %u bytes)",
        (uint32_t)ADDR_QTM_HUD, (unsigned)g_QtmHud_StolenLen);
}


static uint8_t sTripHold[256] = {};   // frames left to hold per DIK

static inline int TripIndexForCode(uint8_t dik) {
    int* entry = (int*)gTripBase;
    int  idx = 0;
    for (;;) {
        if (*entry == 0xFF) break;                         // sentinel
        if (*entry == dik) return idx;                     // found row
        entry += 3; ++idx;
        if ((uintptr_t)entry > (uintptr_t)gTripEnd) break; // guard
    }
    return -1;
}

// Set pressed=1 for this DIK for `frames` menu ticks (sustained each tick)
static inline void TripForcePress(uint8_t dik, uint8_t frames) {
    int idx = TripIndexForCode(dik);
    if (idx < 0) return;
    gTripPressedBase[idx * 3] = 1;
    if (sTripHold[dik] < frames) sTripHold[dik] = frames;
}

// Sustain/expire triplet holds each menu tick
static inline void HandleTripletReleases() {
    for (int c = 1; c < 256; ++c) {
        if (!sTripHold[c]) continue;
        int idx = TripIndexForCode((uint8_t)c);
        if (idx >= 0) gTripPressedBase[idx * 3] = 1;   // keep pressed
        if (--sTripHold[c] == 0 && idx >= 0) {
            gTripPressedBase[idx * 3] = 0;            // release
        }
    }
}

/// END OF MENU CONTROLS ///

// --- Triplet force-press path (bypass aggregator) ---




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
    MH2_EnsureConsole();
    MH2_Log(L"[XInput] Config: EnableMod=%d, BlockIfConflicts=%d, LogToConsole=%d, MouseCursorWithGamepad=%d, InvertX=%d, InvertY=%d",
        (int)g_ModEnabled, (int)g_BlockOnConflict, (int)g_LogToConsole, g_MouseCursorWithGamepad,
        (int)g_InvertLookX, (int)g_InvertLookY);
    MH2_Log(L"[XInput] Sensitivity: Yaw=%.1f, Pitch=%.1f, AimX=%.1f, AimY=%.1f",
        CAM_YAW_SENS, CAM_PITCH_SENS, AIM_MOUSE_X_SENS_CONFIG, AIM_MOUSE_Y_SENS_CONFIG);
    // Mark init done whether or not XInput loaded (prevents re-running every frame)
    g_XIInited = true;
}

// ==========================
// Per-frame glue
// ==========================
static const int STICK_DEADZONE_L = 7849;
static const int STICK_DEADZONE_R = 8689;

static void MH2_UpdateXInputForPlayer(void* playerInst) {
    MH2_InitXInput();


    auto ZeroAnalogsAndPlayer = [&]() {
        if (playerInst) {
            float* L = (float*)((char*)playerInst + 1168);
            float* R = (float*)((char*)playerInst + 1184);
            L[0] = L[1] = R[0] = R[1] = 0.0f;
        }
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT);
        };

    if (!g_XIEnabled || !g_XIGetState) {
        ZeroAnalogsAndPlayer();
        EmitSystemLMB(false);
        EmitSystemRMB(false);
        ReleaseAllTrackedKeys();
        g_DDArmed = true;
        s_CrawlMouseXAcc = 0.0f;
        sWasInMenu = false;
        if (g_LastGamepadConnected) { g_LastGamepadConnected = false; MH2_Log(L"[XInput] Gamepad disconnected (module disabled or XI missing)"); }
        return;
    }

    MH2_XINPUT_STATE st{};
    if (g_XIGetState(0, &st) != ERROR_SUCCESS) {
        ZeroAnalogsAndPlayer();
        EmitSystemLMB(false);
        EmitSystemRMB(false);
        ReleaseAllTrackedKeys();
        g_DDArmed = true;
        s_CrawlMouseXAcc = 0.0f;
        sWasInMenu = false;
        if (g_LastGamepadConnected) { g_LastGamepadConnected = false; MH2_Log(L"[XInput] Gamepad disconnected"); }
        return;
    }

    if (!g_LastGamepadConnected) {
        g_LastGamepadConnected = true;
        MH2_Log(L"[XInput] Gamepad connected");
    }
    if (g_MouseCursorWithGamepad != -1) {
        if (g_LastCursorTarget != g_MouseCursorWithGamepad) {
            EnsureCursorVisible(g_MouseCursorWithGamepad == 1);
            g_LastCursorTarget = g_MouseCursorWithGamepad;
            MH2_Log(L"[XInput] Forcing cursor %s (per INI, on pad connect)",
                (g_MouseCursorWithGamepad == 1) ? L"VISIBLE" : L"HIDDEN");
        }
    }
    // --- sticks (LS/RS) ---
    float lx = MH2_NormStick(st.Gamepad.sThumbLX, STICK_DEADZONE_L);
    float ly = MH2_NormStick(st.Gamepad.sThumbLY, STICK_DEADZONE_L);
    float rx = MH2_NormStick(st.Gamepad.sThumbRX, STICK_DEADZONE_R);
    float ry = MH2_NormStick(st.Gamepad.sThumbRY, STICK_DEADZONE_R);

    // ===== QTM: heartbeat → active flag (do this BEFORE menus) =====
    if (g_QtmSeenFrames > 0) --g_QtmSeenFrames;
    const bool qtmNow = (g_QtmSeenFrames > 0);

    if (qtmNow != g_QtmWasActive) {
        if (qtmNow) {
            MH2_Log(L"[QTM] Exit");
            // clean edges: no sticky keys/buttons and clear accumulators
            ReleaseAllTrackedKeys();
            EmitSystemLMB(false);
            EmitSystemRMB(false);
            s_QtmMouseXAcc = s_QtmMouseYAcc = 0.0f;
        }
    }
    g_QtmActive = qtmNow;

    // ===== QTM: take over input while active (feed game mouse deltas, not OS) =====
    if (g_QtmActive) {
        const float dt = *(float*)0x6ECE68;
        const float dxF = rx * dt * QTM_MOUSE_X_SENS * (g_InvertLookX ? -1.0f : 1.0f);
        const float dyF = ry * dt * QTM_MOUSE_Y_SENS * (g_InvertLookY ? -1.0f : 1.0f);

        s_QtmMouseXAcc += dxF;
        s_QtmMouseYAcc += dyF;

        const int dx = RoundToInt(s_QtmMouseXAcc);
        const int dy = RoundToInt(s_QtmMouseYAcc);
        if (dx || dy) {
            *g_MouseDX += (float)dx;   // write into the game’s mouse-delta accumulators
            *g_MouseDY += (float)dy;

            // keep the desktop cursor still during QTM (uncomment to move OS cursor too)
            // EmitSystemMouseMoveDelta(dx, dy);

            s_QtmMouseXAcc -= (float)dx;
            s_QtmMouseYAcc -= (float)dy;
            // Optional debug:
            // MH2_Log(L"[QTM] dt=%.4f rx=%.3f ry=%.3f -> dx=%d dy=%d", dt, rx, ry, dx, dy);
        }

        // Silence everything else so the minigame has clean mouse-only input
        EmitSystemLMB(false);
        EmitSystemRMB(false);

        // Zero analogs to the engine/player
        if (playerInst) {
            float* L = (float*)((char*)playerInst + 1168);
            float* R = (float*)((char*)playerInst + 1184);
            L[0] = L[1] = R[0] = R[1] = 0.0f;
        }
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT);

        // No keyboard mapping while QTM is up
        bool noKeys[256] = { false };
        ApplyDesiredKeys(noKeys);

        // We handled this frame entirely; skip the rest of the glue.
        return;
    }

    // ===== Menus (only reached when NOT in QTM) =====
    const bool inMenu = IsMenuActive();
    if (inMenu) {
        if (!sWasInMenu) {
            // one-time cleanup on enter
            ReleaseAllTrackedKeys();
            EmitSystemLMB(false);
            EmitSystemRMB(false);
            s_CrawlMouseXAcc = 0.0f;
            g_DDArmed = true;
            ResetMenuGates(); // re-arm A/X/DPAD edges
            sWasInMenu = true;
        }
        // keep engine analogs quiescent while in menu
        *g_LS_X = *g_LS_Y = *g_RS_X = *g_RS_Y = 0.0f;
        *g_PadFlags = 0;
        *g_ImmFlags &= ~(IM_RS_PRESENT);
        return;
    }
    if (sWasInMenu) { ReleaseAllTrackedKeys(); sWasInMenu = false; }


    // crawl state (affects movement input path)
    bool isCrawling = false;
    if (playerInst) {
        int* PlrCurrState = (int*)((char*)playerInst + 956);
        isCrawling = (PlrCurrState && *PlrCurrState == PEDSTATE_CRAWL);
    }

    if (isCrawling) {
        const float dt = *(float*)0x6ECE68;
        const float dxF = rx * dt * CRAWL_MOUSE_X_SENS * (g_InvertLookX ? 1.0f : -1.0f);
        s_CrawlMouseXAcc += dxF;
        const int dx = RoundToInt(s_CrawlMouseXAcc);
        if (dx != 0) {
            EmitSystemMouseMoveDelta(dx, 0);
            s_CrawlMouseXAcc -= (float)dx;
        }
    }
    else {
        s_CrawlMouseXAcc = 0.0f;
    }

    // --- BEGIN PATCH: Aim detection (weapon-type), scoped edge handling, and delta mute ---
        // Detect firearm aim (sniper and other scoped modes)
    bool firearmAimNow = false;
    bool scopedZoom = false;

    float* PlrCurrYAngle = nullptr;
    int* PlrCurrState = nullptr;
    int* PlrCurrWpnType = nullptr;
    int* PlrLockOnHunter = nullptr;
    bool* PlrActionLockOn = nullptr;

    if (playerInst) {
        const bool rmbHeld = g_SysRMBDown;

        PlrCurrState = (int*)((char*)playerInst + 956);
        PlrCurrWpnType = (int*)((char*)playerInst + 968);
        PlrLockOnHunter = (int*)((char*)playerInst + 4500);
        PlrActionLockOn = (bool*)((char*)playerInst + 1352);
        PlrCurrYAngle = (float*)((char*)playerInst + 4400);

        const bool inAimState = (PlrCurrState && (*PlrCurrState == PEDSTATE_AIM || *PlrCurrState == PEDSTATE_CROUCHAIM)) ||
            (PlrLockOnHunter && *PlrLockOnHunter && PlrActionLockOn && *PlrActionLockOn);

        const bool hasFirearmNow = (PlrCurrWpnType) ? HasFirearm(*PlrCurrWpnType) : false;
        firearmAimNow = hasFirearmNow && (rmbHeld || inAimState);

        // Scoped when weapon is 2H firearm *and* we are aiming (RMB or aim state)
        scopedZoom = firearmAimNow && PlrCurrWpnType && (*PlrCurrWpnType == WEAPONTYPE_2HFIREARM);

        // Input-path scoped edge: only mute mouse deltas on the very first frame
        static bool sLastScopedInput = false;
        if (!sLastScopedInput && scopedZoom) {
            // First frame of scope: mute OS mouse delta once to avoid a dip
            g_MuteScopedMouseFrames = 1;
            s_AimMouseXAcc = 0.0f;
            s_AimMouseYAcc = 0.0f;
            MH2_Log(L"[XInput] Scoped entry (input) -> mute deltas 1f");
        }
        else if (sLastScopedInput && !scopedZoom) {
            MH2_Log(L"[XInput] Scoped exit (input)");
        }
        sLastScopedInput = scopedZoom;
        g_WasScopedZoom = scopedZoom;
    }

    if (firearmAimNow) {
        const float dt = *(float*)0x6ECE68;

        // Always drive yaw by mouse X while aiming (apply invert settings - flipped to match normal camera)
        const float dxF = rx * dt * AIM_MOUSE_X_SENS_CONFIG * (g_InvertLookX ? 1.0f : -1.0f);
        s_AimMouseXAcc += dxF;
        int dx = RoundToInt(s_AimMouseXAcc);
        int dy = 0;

        if (scopedZoom) {
            // In scoped/sniper, also drive pitch via mouse Y (the engine is in "mouse-only scope")
            const float dyF = -ry * dt * AIM_MOUSE_Y_SENS_CONFIG * (g_InvertLookY ? 1.0f : -1.0f); // RS up -> mouse up
            s_AimMouseYAcc += dyF;
            dy = RoundToInt(s_AimMouseYAcc);

            // Optional: track deltas in degrees (not consumed here, just informational)
            g_ScopedYawDeltaDeg += rx * dt * CAM_YAW_SENS * (g_InvertLookX ? 1.0f : -1.0f);
            g_ScopedPitchDeltaDeg += ry * dt * CAM_PITCH_SENS * (g_InvertLookY ? 1.0f : -1.0f);
        }
        else {
            // Non-scoped firearm aim: leave pitch to RS.y (avoid double input)
            s_AimMouseYAcc = 0.0f;
        }

        // Emit OS mouse deltas unless we’re intentionally muting on the very first scoped frame
        if (!(scopedZoom && g_MuteScopedMouseFrames > 0)) {
            if (dx != 0 || dy != 0) {
                EmitSystemMouseMoveDelta(dx, dy);
                s_AimMouseXAcc -= (float)dx;
                if (dy != 0) s_AimMouseYAcc -= (float)dy;
            }
        }
        else {
            // Consume/clear for the muted frame
            s_AimMouseXAcc -= (float)dx;
            s_AimMouseYAcc -= (float)dy;
        }
    }
    else {
        s_AimMouseXAcc = 0.0f;
        s_AimMouseYAcc = 0.0f;
    }

    // Clear the one-frame mute after we processed this frame
    if (g_MuteScopedMouseFrames > 0) --g_MuteScopedMouseFrames;

    *g_LS_X = (isCrawling || firearmAimNow) ? 0.0f : lx;
    *g_LS_Y = (isCrawling || firearmAimNow) ? 0.0f : ly;

    {
        const float TH = 0.5f;
        uint32_t A = 0;
        if (fabsf(lx) > 1e-5f || fabsf(ly) > 1e-5f) A |= 0x40000000;
        if (lx < -TH) A |= 0x00040000; else if (lx > TH) A |= 0x00080000;
        if (ly < -TH) A |= 0x00020000; else if (ly > TH) A |= 0x00010000;
        *g_PadFlags = (isCrawling || firearmAimNow) ? 0u : A;
    }

    *g_RS_X = 0.0f; *g_RS_Y = 0.0f;
    if (playerInst) {
        float* L = (float*)((char*)playerInst + 1168);
        float* R = (float*)((char*)playerInst + 1184);
        L[0] = (isCrawling || firearmAimNow) ? 0.0f : lx;
        L[1] = (isCrawling || firearmAimNow) ? 0.0f : ly;

        if (firearmAimNow) {
            if (scopedZoom) {
                // Scoped/sniper: let native mouse-only scope handle aiming.
                // Suppress BOTH sticks so the engine does not switch to pad-scope behavior.
                R[0] = 0.0f; R[1] = 0.0f;
                L[0] = 0.0f; L[1] = 0.0f;
                *g_LS_X = 0.0f; *g_LS_Y = 0.0f;

                // Make sure our explicit deltas are not used anywhere.
                g_ScopedYawDeltaDeg = 0.0f;
                g_ScopedPitchDeltaDeg = 0.0f;
            }
            else {
                // Non-scoped firearm aim: mouse drives yaw; keep RS.y for pitch
                R[0] = 0.0f;
                R[1] = ry;
            }
        }
        else {
            R[0] = isCrawling ? 0.0f : rx;
            R[1] = ry;
        }
    }

    if (firearmAimNow) {
        if (scopedZoom) {
            *g_ImmFlags &= ~IM_RS_PRESENT;
        }
        else {
            if (fabsf(ry) > 1e-4f) *g_ImmFlags |= IM_RS_PRESENT; else *g_ImmFlags &= ~IM_RS_PRESENT;
        }
    }
    else {
        if (fabsf(rx) > 1e-4f || fabsf(ry) > 1e-4f) *g_ImmFlags |= IM_RS_PRESENT; else *g_ImmFlags &= ~IM_RS_PRESENT;
    }
    // --- END PATCH: Aim detection (weapon-type), scoped edge handling, and delta mute ---

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

    if (DD && g_DDArmed) { EmitSystemWheelDownPulse(); g_DDArmed = false; }
    else if (!DD) { g_DDArmed = true; }

    bool desired[256] = { false };
    if (A)     DesireActionDIKs(ACTION_USE, desired);
    if (Y)     DesireActionDIKs(ACTION_WALLSQUASH, desired);
    if (DUp)   DesireActionDIKs(ACTION_RELOAD, desired);
    if (B)     DesireActionDIKs(ACTION_PICKUP, desired);
    if (Xb)    DesireActionDIKs(ACTION_BLOCK, desired);
    if (L3)    DesireActionDIKs(ACTION_LOOKAROUND, desired);
    if (R3)    DesireActionDIKs(ACTION_CROUCH, desired);
    if (DL)    DesireActionDIKs(ACTION_PEEKL, desired);
    if (DR)    DesireActionDIKs(ACTION_PEEKR, desired);
    if (RT)    DesireActionDIKs(ACTION_FIRE1, desired);
    if (LT)    DesireActionDIKs(ACTION_RUN, desired);
    if (Start) DesireActionDIKs(ACTION_MENU, desired);
    if (Back)  DesireActionDIKs(ACTION_LOOKBACK, desired);

    if (isCrawling) {
        DesireMovementFromStick(lx, ly, desired);
    }
    
    // When aiming firearms, use LS for WASD movement instead of analog movement
    if (firearmAimNow) {
        DesireMovementFromStick(lx, ly, desired);
    }

    ApplyDesiredKeys(desired);
}

static void MH2_MenuTick_DIOnly()
{
    if (!g_XIGetState) { ReleaseAllTrackedKeys(); return; }

    MH2_XINPUT_STATE st{};
    if (g_XIGetState(0, &st) != ERROR_SUCCESS) {
        ReleaseAllTrackedKeys();
        return;
    }

    const float dt = *(float*)0x6ECE68;

    // release any one-frame pulses from last tick
    HandlePulseReleases();
    HandleTripletReleases();   // sustain/expire direct triplet presses


    // Sticks + DPAD
    const float lx = MH2_NormStick(st.Gamepad.sThumbLX, STICK_DEADZONE_L);
    const float ly = MH2_NormStick(st.Gamepad.sThumbLY, STICK_DEADZONE_L);

    const uint16_t wb = st.Gamepad.wButtons;
    const bool DUp = (wb & XI_DPAD_UP) != 0;
    const bool DDown = (wb & XI_DPAD_DOWN) != 0;
    const bool DLeft = (wb & XI_DPAD_LEFT) != 0;
    const bool DRight = (wb & XI_DPAD_RIGHT) != 0;

    const bool A = (wb & XI_A) != 0;
    const bool B = (wb & XI_B) != 0;
    const bool Xb = (wb & XI_X) != 0;
    const bool Y = (wb & XI_Y) != 0;
    const bool Start = (wb & XI_START) != 0;
    const bool Back = (wb & XI_BACK) != 0;
    const bool LB = (wb & XI_LEFT_SHOULDER) != 0;
    const bool RB = (wb & XI_RIGHT_SHOULDER) != 0;
    const bool LT = st.Gamepad.bLeftTrigger > TRIGGER_THRESHOLD;
    const bool RT = st.Gamepad.bRightTrigger > TRIGGER_THRESHOLD;

    // Direction wants (DPAD or LS with threshold)
    const float TH = 0.55f;
    const bool wantUp = DUp || (ly > TH);
    const bool wantDown = DDown || (ly < -TH);
    const bool wantLeft = DLeft || (lx < -TH);
    const bool wantRight = DRight || (lx > TH);

    // Repeating arrows (keyboard-like)
    UpdateRepeat(wantUp, DIK_UP, dt);
    UpdateRepeat(wantDown, DIK_DOWN, dt);
    UpdateRepeat(wantLeft, DIK_LEFT, dt);
    UpdateRepeat(wantRight, DIK_RIGHT, dt);

    // === Make the menu see the keys reliably by writing to the triplet table ===
// Give confirm a few frames so UI can sample it even at high FPS.
    if (A || Start) TripForcePress(DIK_RETURN, PULSE_FRAMES_CONFIRM);
    if (Xb)         TripForcePress(DIK_SPACE, PULSE_FRAMES_CONFIRM);

    // Mild stickiness for the rest is harmless and can help on flaky screens:
    if (B || Back) TripForcePress(DIK_ESCAPE, 1);
    if (LB || LT)   TripForcePress(DIK_PGUP, 1);
    if (RB || RT)   TripForcePress(DIK_PGDN, 1);


    // One-shot edges
    // A/Start -> RETURN and X -> SPACE get a *few* ticks so the engine never misses them.
    // Everything else keeps the 1-tick pulse (DPAD-like).
    UpdateEdgeEx((A || Start), DIK_RETURN, PULSE_FRAMES_CONFIRM); // confirm
    UpdateEdge((B || Back), DIK_ESCAPE);                        // cancel/back
    UpdateEdge((LB || LT), DIK_PGUP);                          // page up
    UpdateEdge((RB || RT), DIK_PGDN);                          // page down
    UpdateEdgeEx(Xb, DIK_SPACE, PULSE_FRAMES_CONFIRM);  // X -> Space
    UpdateEdge(Y, DIK_BACK);                           // Y -> Backspace

}





// =====================================================================
// Camera patch – RS yaw+pitch
// =====================================================================

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

/// GET GLOBAL CAMERA ///


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
    
    // --- BEGIN PATCH: Camera uses weapon-type scoped detection with edge handling ---
     // Check if we're scoped - 2H firearm while actively aiming
    const bool isScoped =
        hasFirearmNow &&
        (*PlrCurrWpnType == WEAPONTYPE_2HFIREARM) &&
        (rmbHeld || inAimState);

    // Track generic aim state (freelook / ADS) separately from scope
    const bool wasAimingPrev = WasAiming;
    WasAiming = aimCamNow;

    // Scoped rising/falling edge handled ONLY here (camera side)
    static bool sWasScopedCam = false;
    if (!sWasScopedCam && isScoped) {
        // Entering scope: commit freelook yaw into engine once, zero yaw offset,
        // then initialize engine aim pitch from our freelook pitch + optional bias.
        *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
        CamYawOffset = 0.0f;

        *PlrCurrYAngle = CamCurrYAngle + g_ScopedPitchBiasDeg;
        MH2_Log(L"[Cam] Scoped entry -> yaw committed, pitch baseline=%.2f (bias=%.2f)", CamCurrYAngle, g_ScopedPitchBiasDeg);
    }
    else if (sWasScopedCam && !isScoped) {
        // Exiting scope: bring engine aim pitch back to freelook
        CamCurrYAngle = *PlrCurrYAngle;
        MH2_Log(L"[Cam] Scoped exit -> freelook pitch=%.2f", CamCurrYAngle);
    }
    sWasScopedCam = isScoped;

    if (isScoped) {
        // While scoped, engine mouse handling owns the camera.
        CamYawOffset = 0.0f;
        CamCurrYAngle = *PlrCurrYAngle;

        // (Keep deltas clear — not used while scoped.)
        g_ScopedYawDeltaDeg = 0.0f;
        g_ScopedPitchDeltaDeg = 0.0f;
    }
    else {
        // IMPORTANT: do not run the generic aim yaw-commit on scope enter/exit.
        // Only commit on non-scoped aim enter/exit:
        if (aimCamNow && !wasAimingPrev) {
            *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
            CamYawOffset = 0.0f;
        }
        else if (!aimCamNow && wasAimingPrev) {
            *PlrCurrYAngle = DegWrap(*PlrCurrYAngle + CamYawOffset);
            CamYawOffset = 0.0f;
        }

        // RS.x -> yaw offset
        if (fabsf(RSx) > 0.0005f) {
            CamYawOffset = DegWrap(CamYawOffset + RSx * dt * CAM_YAW_SENS);
            if (CamYawOffset > CAM_FREELOOK_CLAMP) CamYawOffset = CAM_FREELOOK_CLAMP;
            if (CamYawOffset < -CAM_FREELOOK_CLAMP) CamYawOffset = -CAM_FREELOOK_CLAMP;
        }

        // RS.y -> pitch (use aim-vs-freelook clamps)
        if (fabsf(RSy) > 0.0005f) {
            CamCurrYAngle += RSy * dt * CAM_PITCH_SENS;
            const float minLim = aimCamNow ? AIM_PITCH_MIN : CamMinYAngle;
            const float maxLim = aimCamNow ? AIM_PITCH_MAX : CamMaxYAngle;
            if (CamCurrYAngle > maxLim) CamCurrYAngle = maxLim;
            if (CamCurrYAngle < minLim) CamCurrYAngle = minLim;
        }

        // Auto-recenter when moving and not aiming
        CVector2d* LS = (CVector2d*)((char*)This + 1168);
        float moveMag = (LS) ? sqrtf(LS->x * LS->x + LS->y * LS->y) : 0.0f;
        if (!aimCamNow && moveMag > 0.15f && fabsf(RSx) < 0.05f && fabsf(CamYawOffset) > 1e-3f) {
            const float CAM_RECENTER_SPEED = 70.0f; // deg/sec
            float step = CAM_RECENTER_SPEED * dt;
            if (fabsf(CamYawOffset) <= step) CamYawOffset = 0.0f;
            else CamYawOffset += (CamYawOffset > 0.0f ? -step : step);
        }
    }
    // --- END PATCH: Camera uses weapon-type scoped detection with edge handling ---


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

// === Triplet helpers (you already have the base version) ===


static inline uint32_t Orig_TranslateKeytoAction_body(int keycode)
{
    if (*(uint32_t*)0x006B17C4 == 0) return 0; // input enabled?

    int* entry = (int*)gTripBase;
    int  idx = 0;

    for (;;)
    {
        if (((keycode == 0x2A) || (keycode == 0x36)) && (*entry == 0x95))
            return gTripPressedBase[idx * 3];

        if (*entry == keycode)
            return gTripPressedBase[idx * 3];

        if (*entry == 0xFF) break;

        entry += 3;
        ++idx;

        if ((uintptr_t)entry > (uintptr_t)gTripEndGuard)
            return 0;
    }
    return 0;
}

// put this at file scope once (remove any duplicate you already had)
static __declspec(thread) bool g_inTKA = false;

extern "C" uint32_t __cdecl TranslateKeytoAction_Hook(int keycode)
{
    static bool prevMenu = false;
    const bool inMenu = IsMenuActive();

    if (!g_inTKA) {
        g_inTKA = true;
        if (inMenu) {
            MH2_MenuTick_DIOnly();     // run for FE *and* pause
        }
        else if (prevMenu) {
            ReleaseAllTrackedKeys();   // ensure nothing sticks after leaving menu
        }
        g_inTKA = false;
        prevMenu = inMenu;
    }

    return Orig_TranslateKeytoAction_body(keycode);
}


// ===== FE "Confirm" handler hook =====
// void __fastcall FE_Menu_HandleConfirm(int menuCtx /*ECX*/);
static const uintptr_t ADDR_FE_CONFIRM = 0x0055D7B0; // <-- TODO: put your real address here

using tFEConfirm = void(__fastcall*)(int);
static tFEConfirm g_FEConfirm_Orig = (tFEConfirm)ADDR_FE_CONFIRM;
static uint8_t    g_FEConfirmOrigBytes[5] = { 0 };
static bool       g_FEConfirmHookInstalled = false;


// Our hook: pump XInput->triplets just before FE reads "confirm", then call original.
static void __fastcall FE_Menu_HandleConfirm_Hook(int menuCtx /*ECX*/)
{
    // Feed RETURN/SPACE/ESC/PGUP/PGDN pulses into the FE triplet table
    // so FUN_0055d7a0() will see them as keyboard keys.
    MH2_MenuTick_DIOnly();

    // Temporarily restore original, call it, then reapply our JMP
    WriteBytes((void*)ADDR_FE_CONFIRM, g_FEConfirmOrigBytes, 5);
    g_FEConfirm_Orig(menuCtx);
    WriteJump((void*)ADDR_FE_CONFIRM, (void*)&FE_Menu_HandleConfirm_Hook);
}

static void InstallFEConfirmHook()
{
    if (g_FEConfirmHookInstalled) return;
    memcpy(g_FEConfirmOrigBytes, (void*)ADDR_FE_CONFIRM, 5);
    WriteJump((void*)ADDR_FE_CONFIRM, (void*)&FE_Menu_HandleConfirm_Hook);
    g_FEConfirmHookInstalled = true;
}


// ===== Pause menu per-frame tick detour =====
static const uintptr_t ADDR_PAUSE_TICK = 0x004B1770; // FUN_004b1770
static uint8_t g_PauseOrigBytes[5] = { 0 };
static bool    g_PauseHookInstalled = false;


static void __cdecl PauseMenuTick_Hook() {
    // 1) Pump XInput → triplets/DIKs BEFORE original pause UI polls keys
    MH2_MenuTick_DIOnly();

    // 2) Temporarily restore original prologue and call it
    WriteBytes((void*)ADDR_PAUSE_TICK, g_PauseOrigBytes, 5);
    ((void(__cdecl*)())ADDR_PAUSE_TICK)();

    // 3) Re-apply our JMP for the next frame
    WriteJump((void*)ADDR_PAUSE_TICK, (void*)&PauseMenuTick_Hook);
}

static void InstallPauseMenuHook() {
    if (g_PauseHookInstalled) return;
    memcpy(g_PauseOrigBytes, (void*)ADDR_PAUSE_TICK, 5);
    WriteJump((void*)ADDR_PAUSE_TICK, (void*)&PauseMenuTick_Hook);
    g_PauseHookInstalled = true;
}




extern "C" void MH2_XInputGlue_Install() {
    MH2_InitXInput();

    if (!g_ModEnabled) {
        MH2_Log(L"[XInput] EnableMod=0 — not installing hooks.");
        return;
    }

    if (g_BlockOnConflict && HasConflictingASI()) {
        MH2_Log(L"[XInput] Conflicting ASI detected (AbsoluteCamera or MH2Patch) — not installing hooks.");
        return;
    }
    Memory::VP::InjectHook(0x513DEA, InterpolateAimAngle);
    Memory::VP::InjectHook(0x5B0510, GetGlobalCameraPosFromPlayer);
    Memory::VP::InjectHook(0x5455E6, ProcessCrosshair);
    Memory::VP::InjectHook(0x414650, MH2_XInputEnableShim, PATCH_JUMP);
    Memory::VP::InjectHook(0x004B0110, TranslateKeytoAction_Hook, PATCH_JUMP);
    InstallPauseMenuHook();
    InstallFEConfirmHook();
    InstallQtmHudHook();


    MH2_Log(L"[XInput] Hooks installed.");
}



#endif // MH2_XINPUT_GLUE_H
