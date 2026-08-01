#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

typedef HRESULT (__stdcall *DirectInput8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID *);
typedef HRESULT (__stdcall *DllCanUnloadNowFn)(void);
typedef HRESULT (__stdcall *DllGetClassObjectFn)(REFCLSID, REFIID, LPVOID *);
typedef HRESULT (__stdcall *DllRegisterFn)(void);

static HMODULE g_realDinput8;
static HMODULE g_payload;
static FILE   *g_log;

static DirectInput8CreateFn  pDirectInput8Create;
static DllCanUnloadNowFn     pDllCanUnloadNow;
static DllGetClassObjectFn   pDllGetClassObject;
static DllRegisterFn         pDllRegisterServer;
static DllRegisterFn         pDllUnregisterServer;

static const char *g_payloads[] = {
    "scripthook.dll",
};

static void log_msg(const char *fmt, ...) {
    SYSTEMTIME st;
    char buf[1024];
    int len;
    va_list ap;

    GetLocalTime(&st);
    len = snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03d] ",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    if (len < 0 || len >= static_cast<int>(sizeof(buf)))
        return;

    va_start(ap, fmt);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
    va_end(ap);

    if (g_log != nullptr) {
        fputs(buf, g_log);
        fflush(g_log);
    }
    fputs(buf, stdout);
    fflush(stdout);
}

// WDL is built against the Universal CRT so its Lua `print` writes to UCRT stdout. Redirect UCRT's std streams to the console
static void redirect_game_crt() {
    HMODULE ucrt = LoadLibraryA("ucrtbase.dll");
    if (ucrt == nullptr)
        return;

    typedef void* (__cdecl *AcrtIobFunc)(int);
    typedef FILE* (__cdecl *FreopenFn)(const char *, const char *, FILE *);

    AcrtIobFunc iob = reinterpret_cast<AcrtIobFunc>(reinterpret_cast<void*>(GetProcAddress(ucrt, "__acrt_iob_func")));
    FreopenFn fre   = reinterpret_cast<FreopenFn>(reinterpret_cast<void*>(GetProcAddress(ucrt, "freopen")));
    if (iob == nullptr || fre == nullptr)
        return;

    fre("CONOUT$", "w", static_cast<FILE *>(iob(1))); // stdout
    fre("CONOUT$", "w", static_cast<FILE *>(iob(2))); // stderr
    fre("CONIN$",  "r", static_cast<FILE *>(iob(0))); // stdin
}

// Load the real dinput8 from the system directory and resolve its exports.
static void ensure_real() {
    WCHAR sysdir[MAX_PATH], full[MAX_PATH];

    if (g_realDinput8 != nullptr)
        return;

    if (GetSystemDirectoryW(sysdir, MAX_PATH) == 0)
        return;

    _snwprintf(full, MAX_PATH, L"%s\\dinput8.dll", sysdir);
    full[MAX_PATH - 1] = L'\0';

    g_realDinput8 = LoadLibraryW(full);
    if (g_realDinput8 == nullptr)
        return;

    pDirectInput8Create  = reinterpret_cast<DirectInput8CreateFn>(reinterpret_cast<void*>(GetProcAddress(g_realDinput8, "DirectInput8Create")));
    pDllCanUnloadNow     = reinterpret_cast<DllCanUnloadNowFn>(reinterpret_cast<void*>(GetProcAddress(g_realDinput8, "DllCanUnloadNow")));
    pDllGetClassObject   = reinterpret_cast<DllGetClassObjectFn>(reinterpret_cast<void*>(GetProcAddress(g_realDinput8, "DllGetClassObject")));
    pDllRegisterServer   = reinterpret_cast<DllRegisterFn>(reinterpret_cast<void*>(GetProcAddress(g_realDinput8, "DllRegisterServer")));
    pDllUnregisterServer = reinterpret_cast<DllRegisterFn>(reinterpret_cast<void*>(GetProcAddress(g_realDinput8, "DllUnregisterServer")));
}

// Try to load a payload DLL from the shim's own directory.
static DWORD WINAPI payload_thread(LPVOID param) {
    HMODULE hmod = static_cast<HMODULE>(param);
    CHAR path[MAX_PATH], full[MAX_PATH];
    DWORD len, i;

    len = GetModuleFileNameA(hmod, path, MAX_PATH);
    if (len == 0)
        return 1;

    for (i = len; i > 0; i--) {
        if (path[i - 1] == '\\' || path[i - 1] == '/')
            break;
    }
    if (i > 0)
        path[i] = '\0';

    for (i = 0; i < ARRAYSIZE(g_payloads); i++) {
        _snprintf(full, sizeof(full), "%s%s", path, g_payloads[i]);
        full[sizeof(full) - 1] = '\0';

        log_msg("[dinput8] Loading payload: %s", full);
        g_payload = LoadLibraryA(full);
        if (g_payload != nullptr) {
            log_msg("[dinput8] Payload loaded @ %p", g_payload);
            return 0;
        }

        DWORD err = GetLastError();
        const char *hint = "";
        switch (err) {
        case 5:   hint = "5 = access denied"; break;
        case 126: hint = "126 = module or dependency missing (file absent or VC++ runtime?)"; break;
        case 225: hint = "225 = blocked by Windows Defender / AV (add bin folder exclusion, restore quarantined DLL)"; break;
        }
        if (hint[0] != '\0')
            log_msg("[dinput8] Load failed (GetLastError=%lu) %s", err, hint);
        else
            log_msg("[dinput8] Load failed (GetLastError=%lu)", err);
    }

    log_msg("[dinput8] ERROR: no payload DLL found beside dinput8.dll");
    return 2;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        CHAR path[MAX_PATH];
        CHAR *slash;

        DisableThreadLibraryCalls(hinstDLL);

        AllocConsole();
        SetConsoleTitleA("Dev Console - Watch Dogs Legion");
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
        HWND hwnd = GetConsoleWindow();
        if (hwnd != nullptr)
            ShowWindow(hwnd, SW_SHOW);

        memset(path, 0, sizeof(path));
        if (GetModuleFileNameA(hinstDLL, path, MAX_PATH) != 0) {
            slash = strrchr(path, '\\');
            if (slash == nullptr)
                slash = strrchr(path, '/');
            if (slash != nullptr) {
                slash[1] = '\0';
                strncat(path, "boot.log",
                        sizeof(path) - strlen(path) - 1);
            } else {
                strncpy(path, "boot.log", sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            }
            fopen_s(&g_log, path, "at");
        }

        log_msg("========== EXE LAUNCH / dinput8 attached ==========");
        log_msg("[dinput8] Boot logger + console ready");

        redirect_game_crt();
        ensure_real();

        CreateThread(nullptr, 0, payload_thread, hinstDLL, 0, nullptr);
        return TRUE;
    }

    if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_log != nullptr) {
            fclose(g_log);
            g_log = nullptr;
        }
        if (g_payload != nullptr) {
            FreeLibrary(g_payload);
            g_payload = nullptr;
        }
        if (g_realDinput8 != nullptr) {
            FreeLibrary(g_realDinput8);
            g_realDinput8 = nullptr;
        }
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riid, LPVOID *ppvOut) {
    ensure_real();
    if (pDirectInput8Create == nullptr)
        return 0x80004005;
    return pDirectInput8Create(hinst, dwVersion, riid, ppvOut);
}

extern "C" HRESULT __stdcall DllCanUnloadNow(void) {
    ensure_real();
    if (pDllCanUnloadNow == nullptr)
        return 1;
    return pDllCanUnloadNow();
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    ensure_real();
    if (pDllGetClassObject == nullptr)
        return 0x80040111; /* CLASS_E_CLASSNOTAVAILABLE */
    return pDllGetClassObject(rclsid, riid, ppv);
}

extern "C" HRESULT __stdcall DllRegisterServer(void) {
    ensure_real();
    if (pDllRegisterServer == nullptr)
        return 0x80004005;
    return pDllRegisterServer();
}

extern "C" HRESULT __stdcall DllUnregisterServer(void) {
    ensure_real();
    if (pDllUnregisterServer == nullptr)
        return 0x80004005;
    return pDllUnregisterServer();
}
