#include "windows.h" // IWYU pragma: keep
#include "Main.h"
#include <stdarg.h>
#include <stdio.h>

static Main* g_pScriptHook = NULL;

static HMODULE g_realDinput8;

static FARPROC pDirectInput8Create;
static FARPROC pDllCanUnloadNow;
static FARPROC pDllGetClassObject;
static FARPROC pDllRegisterServer;
static FARPROC pDllUnregisterServer;

static void ensure_real(void)
{
    WCHAR sysdir[MAX_PATH], full[MAX_PATH];

    if (g_realDinput8 != NULL)
        return;

    if (GetSystemDirectoryW(sysdir, MAX_PATH) == 0)
        return;

    _snwprintf(full, MAX_PATH, L"%s\\dinput8.dll", sysdir);
    full[MAX_PATH - 1] = L'\0';

    g_realDinput8 = LoadLibraryW(full);
    if (g_realDinput8 == NULL)
        return;

    pDirectInput8Create  = GetProcAddress(g_realDinput8, "DirectInput8Create");
    pDllCanUnloadNow     = GetProcAddress(g_realDinput8, "DllCanUnloadNow");
    pDllGetClassObject   = GetProcAddress(g_realDinput8, "DllGetClassObject");
    pDllRegisterServer   = GetProcAddress(g_realDinput8, "DllRegisterServer");
    pDllUnregisterServer = GetProcAddress(g_realDinput8, "DllUnregisterServer");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);

		// ensure_real();

		g_pScriptHook = new Main();
		if (g_pScriptHook)
		{

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
