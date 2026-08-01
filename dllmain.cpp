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

		ensure_real();

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

HRESULT __stdcall DirectInput8Create(HINSTANCE hinst, DWORD dwVersion,
                                     REFIID riid, LPVOID *ppvOut)
{
    ensure_real();
    if (pDirectInput8Create == NULL)
        return 0x80004005;
    return ((HRESULT (__stdcall *)(HINSTANCE, DWORD, REFIID, LPVOID *))
            pDirectInput8Create)(hinst, dwVersion, riid, ppvOut);
}

HRESULT __stdcall DllCanUnloadNow(void)
{
    ensure_real();
    if (pDllCanUnloadNow == NULL)
        return 1;
    return ((HRESULT (__stdcall *)(void))pDllCanUnloadNow)();
}

HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    ensure_real();
    if (pDllGetClassObject == NULL)
        return 0x80040111; /* CLASS_E_CLASSNOTAVAILABLE */
    return ((HRESULT (__stdcall *)(REFCLSID, REFIID, LPVOID *))
            pDllGetClassObject)(rclsid, riid, ppv);
}

HRESULT __stdcall DllRegisterServer(void)
{
    ensure_real();
    if (pDllRegisterServer == NULL)
        return 0x80004005;
    return ((HRESULT (__stdcall *)(void))pDllRegisterServer)();
}

HRESULT __stdcall DllUnregisterServer(void)
{
    ensure_real();
    if (pDllUnregisterServer == NULL)
        return 0x80004005;
    return ((HRESULT (__stdcall *)(void))pDllUnregisterServer)();
}
