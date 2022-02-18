// dllmain.cpp : Defines the entry point for the DLL application.
#include <pe/module.h>
#include <xorstr.hpp>
#include "pluginsdk.h"
#include "searchers.h"
#include <iostream>
#include <Detours/src/detours.h>

// Offset to the user setting Skip Dungeon Cutscene
int SkipCutscene = 0;

uintptr_t* bSetting = NULL;
uintptr_t GetAddress(uintptr_t AddressOfCall, int index, int length)
{
	if (!AddressOfCall)
		return 0;

	long delta = *(long*)(AddressOfCall + index);
	return (AddressOfCall + delta + length);
}

bool(__fastcall* oUiStateGamePlayCinema)(__int64 UiStateCinema, __int64 cinemaId);
bool __fastcall hkUiStateGamePlayCinema(__int64 UiStateCinema, __int64 cinemaId) {
	// Check if the pointer for User Settings is available
	if (*bSetting) {
		// Is Skip Dungeon Cutscene enabled?
		if (*reinterpret_cast<bool*>(*bSetting + SkipCutscene))
			return 0;
	}
	else
		return 0; // Pointer was null but we won't let that stop us

	return oUiStateGamePlayCinema(UiStateCinema, cinemaId);
}

void __cdecl oep_notify([[maybe_unused]] const Version client_version)
{
	if (const auto module = pe::get_module()) {
		DetourTransactionBegin();
		DetourUpdateThread(NtCurrentThread());
		uintptr_t handle = module->handle();
		const auto sections2 = module->segments();
		const auto& s2 = std::find_if(sections2.begin(), sections2.end(), [](const IMAGE_SECTION_HEADER& x) {
			return x.Characteristics & IMAGE_SCN_CNT_CODE;
			});
		const auto data = s2->as_bytes();

		// This function checks if scene belongs to a dungeon, if so check if Skip Dungeon Cutscenes is enabled
		// I'm just getting the object pointer for the UserSettings from this
		auto sCheckSkipSetting = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 C7 45 C0 FE FF FF FF 48 89 58 08 48 89 70 10 48 89 78 20 4C 8B E2 4C 8B F9")));
		if (sCheckSkipSetting != data.end()) {
			memcpy(&SkipCutscene, &sCheckSkipSetting[0] + 0x23, 4); // Get the offset for Skip Dungeon Cutscene
			bSetting = (uintptr_t*)GetAddress((uintptr_t)&sCheckSkipSetting[0] + 0x1A, 3, 7); // Get pointer to User Settings object
		}

		// UiStateGame::playCinema
		auto sInitCinematicScene = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 33 C4 48 89 45 17 4C 8B E2 48 8B F1")));
		uintptr_t aInitCinematicScene = NULL;
		if (sInitCinematicScene != data.end()) {
			aInitCinematicScene = (uintptr_t)&sInitCinematicScene[0] - 0x2F;
			oUiStateGamePlayCinema = module->rva_to<std::remove_pointer_t<decltype(oUiStateGamePlayCinema)>>(aInitCinematicScene - handle);
			DetourAttach(&(PVOID&)oUiStateGamePlayCinema, &hkUiStateGamePlayCinema);
		}
		DetourTransactionCommit();
	}
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hInstance);
	}

	return TRUE;
}

bool __cdecl init([[maybe_unused]] const Version client_version)
{
	NtCurrentPeb()->BeingDebugged = FALSE;
	return true;
}

extern "C" __declspec(dllexport) PluginInfo GPluginInfo = {
  .hide_from_peb = true,
  .erase_pe_header = true,
  .init = init,
  .oep_notify = oep_notify,
  .priority = 1,
  .target_apps = L"BNSR.exe"
};