// dllmain.cpp : Defines the entry point for the DLL application.
#include <pe/module.h>
#include <xorstr.hpp>
#include "pluginsdk.h"
#include "searchers.h"

void __cdecl oep_notify([[maybe_unused]] const Version client_version)
{
	if (const auto module = pe::get_module()) {
		uintptr_t handle = module->handle();
		const auto sections2 = module->segments();
		const auto& s2 = std::find_if(sections2.begin(), sections2.end(), [](const IMAGE_SECTION_HEADER& x) {
			return x.Characteristics & IMAGE_SCN_CNT_CODE;
			});
		const auto data = s2->as_bytes();

		// This function can be found by searching for String refs for: UiStateCinema::PlayCinema()
		// Go to the start of the function and change it to ret (0xC3)
		// x86 UE3 clients will not work this way and you need to change one of the first jump equals (JE) to a jmp (0xEB) otherwise client crashes
		auto sInitCinematicScene = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 89 5C 24 60 48 89 5C 24 68 C7 44 24 70 00 00 80 3F C7 44 24 74 00 00 80 3F 48 C7 44 24 78 00 00 80 3F")));
		uintptr_t aInitCinematicScene = NULL;
		if (sInitCinematicScene != data.end()) {
			aInitCinematicScene = (uintptr_t)&sInitCinematicScene[0] - 0x97;
			BYTE retCode[] = { 0xC3, 0x90 };
			DWORD oldProtect;
			VirtualProtect((LPVOID)aInitCinematicScene, sizeof(retCode), PAGE_EXECUTE_READWRITE, &oldProtect);
			memcpy((LPVOID)aInitCinematicScene, retCode, sizeof(retCode));
			VirtualProtect((LPVOID)aInitCinematicScene, sizeof(retCode), oldProtect, &oldProtect);

		}
	}
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		DisableThreadLibraryCalls(hInstance);

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