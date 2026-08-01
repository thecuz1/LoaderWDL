#include "Main.h"

#include <windows.h> // IWYU pragma: keep

#include <stdarg.h>
#include <stdio.h>

static Main* g_pScriptHook = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);

		g_pScriptHook = new Main();
		if (g_pScriptHook) {

			g_pScriptHook->StartThread();
		}
		break;
	case DLL_PROCESS_DETACH:
		g_pScriptHook->Unload();
		delete g_pScriptHook;
		break;
	}

	return TRUE;
}
