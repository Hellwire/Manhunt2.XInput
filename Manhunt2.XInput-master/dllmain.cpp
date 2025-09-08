// dllmain.cpp : Определяет точку входа для приложения DLL.
#include "pch.h"
#include "src/XInput.h"
#include "src/MemoryMgr.h"

extern "C" void __cdecl MH2_XInputEnableShim(uint8_t enable);
extern "C" void MH2_XInputGlue_Install();
extern "C"
{
	__declspec(dllexport) void InitializeASI()
	{
		//Memory::VP::InjectHook(0x513DEA, InterpolateAimAngle);
		Memory::VP::InjectHook(0x5B0510, GetGlobalCameraPosFromPlayer);
		//Memory::VP::InjectHook(0x5455E6, ProcessCrosshair);
		//Memory::VP::InjectHook(0x414650, MH2_XInputEnableShim, PATCH_JUMP);

		MH2_XInputGlue_Install();
	}
}

