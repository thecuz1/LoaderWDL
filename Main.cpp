#include "MinHook/MinHook.h"

#include "Logger.h"
#include "Main.h"
#include "menu.h"

#include <minwindef.h>
#include <windows.h>
#include <winnt.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
#include "lua.h"
}

using namespace std;

LPVOID luaL_loadbuffer_t_addr;
LPVOID luaL_pcall_t_addr;
lua_State *context_lua_state;

const char *g_engineModule = "DuniaDemo_clang_64_dx12.dll";
uintptr_t lual_loadbuffer_t_off = LUA_DX12_LOADBUFFER;
uintptr_t lua_pcall_t_off = LUA_DX12_PCALL;
uintptr_t lua_tolstring_t_off = LUA_DX12_TOLSTRING;
uintptr_t lua_newstate_t_off = LUA_DX12_NEWSTATE;
uintptr_t lua_gettop_t_off = LUA_DX12_GETTOP;
uintptr_t lua_settop_t_off = LUA_DX12_SETTOP;

// Absolute Lua C API addresses recovered by AOB scan (0 = not found; fall back to the offset
// tables).
static uintptr_t g_luaLoadbuffer = 0;
static uintptr_t g_luaPcall = 0;
static uintptr_t g_luaTolstring = 0;
static uintptr_t g_luaGettop = 0;
static uintptr_t g_luaSettop = 0;

// AOB signatures for the Lua C API functions, cross-validated against both the dx12 1.6.3 and dx11
// builds (unique match in each).
// `??` bytes are wildcards: they cover RIP-relative displacement fields that differ per build.
static const uint8_t sig_luaL_loadbuffer[] = {
	0x56, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x04, 0x48, 0x31, 0xE0, 0x48,
	0x89, 0x44, 0x24, 0x38, 0x48, 0x89, 0x54, 0x24, 0x28, 0x4C, 0x89, 0x44, 0x24, 0x30, 0x48, 0x8D,
	0x15, 0x2B, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x44, 0x24, 0x28, 0xE8, 0xA1, 0x84, 0x00, 0x00, 0x89};
static const uint8_t mask_luaL_loadbuffer[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t sig_lua_pcall[] = {
	0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x44, 0x89, 0xC7, 0x48, 0x89, 0xCE, 0x48, 0x8B, 0x05,
	0x00, 0x00, 0x00, 0x04, 0x48, 0x31, 0xE0, 0x48, 0x89, 0x44, 0x24, 0x38, 0x45, 0x85, 0xC9, 0x74,
	0x28, 0x7E, 0x31, 0x48, 0x8B, 0x46, 0x18, 0x49, 0x63, 0xC9, 0x48, 0xC1, 0xE1, 0x04, 0x48, 0x01};
static const uint8_t mask_lua_pcall[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t sig_lua_tolstring[] = {
	0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x4C, 0x89, 0xC6, 0x89, 0xD3, 0x48, 0x89, 0xCF, 0x85,
	0xD2, 0x7E, 0x45, 0x48, 0x8B, 0x47, 0x18, 0x48, 0x63, 0xCB, 0x48, 0xC1, 0xE1, 0x04, 0x48, 0x01,
	0xC8, 0x48, 0x83, 0xC0, 0xF0, 0x48, 0x3B, 0x47, 0x10, 0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x03};
static const uint8_t mask_lua_tolstring[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF};
static const uint8_t sig_lua_gettop[] = {
	0x8B, 0x44, 0x24, 0x20, 0x44, 0x31, 0xE8, 0x0F, 0xB7, 0x4C, 0x24, 0x24, 0x83, 0xF1, 0x0A, 0x09,
	0xC1, 0x0F, 0x84, 0xAC, 0x00, 0x00, 0x00, 0x4C, 0x89, 0xE1, 0xE8, 0x00, 0x00, 0x00, 0x03, 0x48,
	0x89, 0xF1, 0x4C, 0x89, 0xE2, 0x49, 0x89, 0xC0, 0x4D, 0x89, 0xF1, 0xE8, 0x90, 0xFD, 0xFF, 0xFF};
static const uint8_t mask_lua_gettop[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t sig_lua_settop[] = {
	0x89, 0xF1, 0x4C, 0x89, 0xE2, 0x49, 0x89, 0xC0, 0x4D, 0x89, 0xF1, 0xE8, 0x90, 0xFD, 0xFF, 0xFF,
	0x85, 0xC0, 0x75, 0x14, 0x48, 0x89, 0xF1, 0x31, 0xD2, 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9, 0xE8,
	0xDC, 0x7F, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x3A, 0xB9, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xD7, 0x48};
static const uint8_t mask_lua_settop[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Scan a wildcard pattern over the loaded engine module. Returns the absolute  address of the
// match, or 0. Anchored on the first signature byte (always concrete in our signatures) via memchr
// so it's near-linear time.
static uintptr_t ScanPattern(const uint8_t *sig, const uint8_t *mask, size_t len, uintptr_t base,
							 size_t size) {
	if (size < len) {
		return 0;
	}
	const uint8_t *mem = reinterpret_cast<const uint8_t *>(base);
	const uint8_t *end = mem + size - len;
	for (const uint8_t *p = mem; p < end;) {
		const uint8_t *a =
			reinterpret_cast<const uint8_t *>(memchr(p, sig[0], static_cast<size_t>(end - p)));
		if (!a) {
			break;
		}
		p = a + 1;
		bool ok = true;
		for (size_t j = 0; j < len; ++j) {
			if (mask[j] && a[j] != sig[j]) {
				ok = false;
				break;
			}
		}
		if (ok) {
			return base + static_cast<uintptr_t>(a - mem);
		}
	}
	return 0;
}

// Pick the engine module + Lua offsets from which DLL the game actually loaded, then AOB-scan it
// for the Lua C API (version-independent). dx11 auto-detected; dx12 is the default (OFFSETS_156
// -> 1.5.6).
void DetectBuild() {
	if (GetModuleHandleA("DuniaDemo_clang_64_dx11.dll") != NULL) {
		g_engineModule = "DuniaDemo_clang_64_dx11.dll";
		lual_loadbuffer_t_off = LUA_DX11_LOADBUFFER;
		lua_pcall_t_off = LUA_DX11_PCALL;
		lua_tolstring_t_off = LUA_DX11_TOLSTRING;
		lua_newstate_t_off = LUA_DX11_NEWSTATE;
		lua_gettop_t_off = LUA_DX11_GETTOP;
		lua_settop_t_off = LUA_DX11_SETTOP;
	} else {
		g_engineModule = "DuniaDemo_clang_64_dx12.dll";
#ifdef OFFSETS_156
		lual_loadbuffer_t_off = LUA_DX12_156_LOADBUFFER;
		lua_pcall_t_off = LUA_DX12_156_PCALL;
		lua_tolstring_t_off = LUA_DX12_156_TOLSTRING;
		lua_newstate_t_off = LUA_DX12_156_NEWSTATE;
		lua_gettop_t_off = LUA_DX12_156_GETTOP;
		lua_settop_t_off = LUA_DX12_156_SETTOP;
#else
		lual_loadbuffer_t_off = LUA_DX12_LOADBUFFER;
		lua_pcall_t_off = LUA_DX12_PCALL;
		lua_tolstring_t_off = LUA_DX12_TOLSTRING;
		lua_newstate_t_off = LUA_DX12_NEWSTATE;
		lua_gettop_t_off = LUA_DX12_GETTOP;
		lua_settop_t_off = LUA_DX12_SETTOP;
#endif
	}

	// AOB-scan the loaded module for the Lua C API functions.
	HMODULE mod = GetModuleHandleA(g_engineModule);
	if (!mod) {
		return;
	}
	PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
	PIMAGE_NT_HEADERS nt =
		reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE *>(mod) + dos->e_lfanew);
	uintptr_t base = reinterpret_cast<uintptr_t>(mod);
	size_t size = nt->OptionalHeader.SizeOfImage;

	g_luaLoadbuffer = ScanPattern(sig_luaL_loadbuffer, mask_luaL_loadbuffer,
								  sizeof(sig_luaL_loadbuffer), base, size);
	g_luaPcall = ScanPattern(sig_lua_pcall, mask_lua_pcall, sizeof(sig_lua_pcall), base, size);
	g_luaTolstring =
		ScanPattern(sig_lua_tolstring, mask_lua_tolstring, sizeof(sig_lua_tolstring), base, size);
	g_luaGettop = ScanPattern(sig_lua_gettop, mask_lua_gettop, sizeof(sig_lua_gettop), base, size);
	g_luaSettop = ScanPattern(sig_lua_settop, mask_lua_settop, sizeof(sig_lua_settop), base, size);

	int found = (g_luaLoadbuffer ? 1 : 0) + (g_luaPcall ? 1 : 0) + (g_luaTolstring ? 1 : 0) +
				(g_luaGettop ? 1 : 0) + (g_luaSettop ? 1 : 0);
	Logger::LogMessage("[Main] Lua AOB scan: %d/5 patterns found, %d fallbacks\n", found,
					   5 - found);
}

bool hasConsole = false;
DWORD Main::Entry(Main *main) { // static
	HWND hGameWindow = NULL;
	while (hGameWindow == NULL) {
		hGameWindow = FindWindowA(NULL, "Watch Dogs Legion");

		if (hGameWindow == NULL) {
			Sleep(500);
		} else {
			break;
		}
	}
	Sleep(2000);

	MH_Initialize();
	main->InstallHook();

	HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MenuThread, main, 0, 0);
	if (thread) {
		CloseHandle(thread);
	} else {
		Logger::LogMessage("[Main] Failed to create MenuThread thread: %d\n", GetLastError());
	}
	return 0;
}

uintptr_t Main::GetGameBaseAddress() {
	return reinterpret_cast<uintptr_t>(GetModuleHandleA(g_engineModule));
}

int Main::luaL_pcall_t_trampoline(lua_State *L, int nargs, int nresults, int errfunc) {
	if (L != NULL && context_lua_state == NULL) {
		Logger::LogMessage("[Main] Captured lua_state from pcall!\n");
		context_lua_state = L;
	}

	lua_pcall_t luaL_pcall_t_call = reinterpret_cast<lua_pcall_t>(luaL_pcall_t_addr);
	return luaL_pcall_t_call(L, nargs, nresults, errfunc);
}

int Main::luaL_loadbuffer_t_trampoline(lua_State *lua_state, const char *buff, size_t sz,
									   const char *name) {
	if (lua_state != NULL) {
		context_lua_state = lua_state;
	} else {
		Logger::LogMessage("[Error] Failed to set lua_state because it is NULL!\n");
	}

	Logger::LogMessage("[Lua] Script loaded.\n");

	// Print file/script name
	if (name != NULL) {
		Logger::LogMessage("Script name: %s\n", name);
	} else {
		Logger::LogMessage("Script name: [unnamed]\n");
	}

	// Print buffer size
	Logger::LogMessage("Buffer Size: %zu bytes\n", sz);

	// Print lua script contents
	if (buff != NULL && sz > 0) {
		Logger::LogMessage("Script Content:\n");
		Logger::LogMessage("------------------------\n");

		// luaL_loadbuffer's buffer isn't null-terminated (length passed separately), so copy it
		// into a std::string to safely use with %s
		std::string script_content(buff, sz);
		Logger::LogMessage("%s\n", script_content.c_str());

		Logger::LogMessage("------------------------\n");
	}

	Logger::LogMessage("\n");
	// call the original function to avoid breaking the game
	luaL_loadbuffer_t luaL_loadbuffer_t_call =
		reinterpret_cast<luaL_loadbuffer_t>(luaL_loadbuffer_t_addr);
	return luaL_loadbuffer_t_call(lua_state, buff, sz, name);
}
int Main::Execute(lua_State *L, const char *scriptData) {
	if (L == nullptr) {
		Logger::LogMessage(
			"[Main] Execute called with NULL lua_state (game Lua not loaded yet - go in-game)\n");
		return LUA_ERRSYNTAX;
	}

	uintptr_t engineBase = GetGameBaseAddress();
	uintptr_t tolstringAddr = g_luaTolstring ? g_luaTolstring : engineBase + lua_tolstring_t_off;
	uintptr_t pcallAddr = g_luaPcall ? g_luaPcall : engineBase + lua_pcall_t_off;

	lua_tolstring_t lua_tolstring_t_call = reinterpret_cast<lua_tolstring_t>(tolstringAddr);
	lua_pcall_t lua_pcall_t_call = reinterpret_cast<lua_pcall_t>(pcallAddr);

	// Wrap the game's Lua `print` once per state so script output shows up with a [Console] prefix
	static const char *kPrintPrefix =
		"if type(_G.print) == \"function\" and not _G.__console_print_patched then\n"
		"  local _op = _G.print;\n"
		"  _G.print = function(...)\n"
		"    local t = {};\n"
		"    for i = 1, select(\"#\", ...) do t[i] = tostring(select(i, ...)); end;\n"
		"    _op(\"[Console] \" .. table.concat(t, \"\\t\"));\n"
		"  end;\n"
		"  _G.__console_print_patched = true;\n"
		"end;\n";

	std::string script = std::string(kPrintPrefix) + scriptData;

	luaL_loadbuffer_t luaL_loadbuffer_t_call =
		reinterpret_cast<luaL_loadbuffer_t>(luaL_loadbuffer_t_addr);
	int loadResult = luaL_loadbuffer_t_call(L, script.c_str(), script.size(), "exec");

	if (loadResult != LUA_OK) {
		const char *err = nullptr;
		if (lua_isstring(L, -1)) {
			err = lua_tolstring_t_call(L, -1, NULL);
		}
		Logger::LogMessage("Compilation error: %s\n", err ? err : "nil");
		lua_pop(L, 1);
		return LUA_ERRSYNTAX;
	}

	int result = lua_pcall_t_call(L, 0, 0, 0);
	if (result != LUA_OK) {
		const char *err = lua_tolstring_t_call(L, -1, NULL);
		Logger::LogMessage("Execution error: %s\n", err);
		lua_pop(L, 1);
	}

	return result;
}

void Main::ExecuteFile(lua_State *L, const std::filesystem::path &filepath) {
	struct FileDeleter {
		void operator()(FILE *f) const {
			if (f)
				std::fclose(f);
		}
	};

	std::unique_ptr<FILE, FileDeleter> file(_wfopen(filepath.c_str(), L"rb"));
	std::string narrowPath = filepath.string();

	if (!file) {
		Logger::LogMessage("Failed to open file: %s\n", narrowPath.c_str());
		return;
	}

	std::string scriptData;
	std::fseek(file.get(), 0, SEEK_END);
	long size = std::ftell(file.get());
	std::rewind(file.get());

	if (size < 0) {
		Logger::LogMessage("Failed to get file size: %s\n", narrowPath.c_str());
		return;
	}

	scriptData.resize(size);
	size_t bytesRead = std::fread(scriptData.data(), 1, size, file.get());

	if (bytesRead == static_cast<size_t>(size)) {
		Main::Execute(L, scriptData.c_str());
	} else {
		Logger::LogMessage("Failed to read file contents: %s\n", narrowPath.c_str());
	}
}

void Main::InstallHook() {
	uintptr_t baseAddress = GetGameBaseAddress();
	PBYTE load_buffer_func =
		(PBYTE)(g_luaLoadbuffer ? g_luaLoadbuffer : baseAddress + lual_loadbuffer_t_off);
	PBYTE lua_pcall_func = (PBYTE)(g_luaPcall ? g_luaPcall : baseAddress + lua_pcall_t_off);

	if (MH_CreateHook(load_buffer_func, (LPVOID)&luaL_loadbuffer_t_trampoline,
					  (LPVOID *)&luaL_loadbuffer_t_addr) != MH_OK) {
		Logger::LogMessage("Failed to create hook! (lual_loadbuffer)\n");
		return;
	}

	if (MH_CreateHook(lua_pcall_func, (LPVOID)&luaL_pcall_t_trampoline,
					  (LPVOID *)&luaL_pcall_t_addr) != MH_OK) {
		Logger::LogMessage("Failed to create hook! (lua_pcall)\n");
		return;
	}

	Logger::LogMessage("Hooks created!\n");

	if (MH_EnableHook(load_buffer_func) != MH_OK) {
		Logger::LogMessage("Failed to enable hook! (lual_loadbuffer)\n");
		return;
	}

	if (MH_EnableHook(lua_pcall_func) != MH_OK) {
		Logger::LogMessage("Failed to enable hook! (lua_pcall)\n");
		return;
	}

	Logger::LogMessage("Hooks enabled!\n");
}

void Main::StartThread() {
	Logger::Initialize("scripthook.log");
	DetectBuild();
	Logger::LogMessage("\n");
	Logger::LogMessage("[Main] Engine module: %s (loadbuffer @ 0x%X)\n", g_engineModule,
					   lual_loadbuffer_t_off);

	hasConsole = AllocConsole();
	FILE *dummy;
	freopen_s(&dummy, "CONOUT$", "wb", stdout);

	HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Main::Entry, this, 0, 0);
	if (thread)
		CloseHandle(thread);
	else
		Logger::LogMessage("[Main] Failed to create main thread: %d\n", GetLastError());
}

void Main::Unload() {
	Logger::Shutdown();
	if (hasConsole) {
		fclose(stdout);
		hasConsole = !FreeConsole();
	}
}
