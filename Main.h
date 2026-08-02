#pragma once

#include <minwindef.h>
#include <windows.h> // IWYU pragma: keep

#include <filesystem>

extern "C" {
#include "lua.h"
}

#define LUA_OK		  0
#define LUA_ERRSYNTAX 3

// Lua C API offsets into the engine DLL, per build. Selected at runtime by DetectBuild() from the
// loaded module name (dx11 vs dx12), so no per-build compile flag is needed. OFFSETS_156 is a dx12
// version override (1.5.6).
#define LUA_DX12_LOADBUFFER 0x6907330
#define LUA_DX12_PCALL		0x690F590
#define LUA_DX12_TOLSTRING	0x690D8F0
#define LUA_DX12_NEWSTATE	0x6907400
#define LUA_DX12_GETTOP		0x6907570
#define LUA_DX12_SETTOP		0x6907590

#define LUA_DX12_156_LOADBUFFER 0x6904AC0 // -0x2870
#define LUA_DX12_156_PCALL		0x690CD20
#define LUA_DX12_156_TOLSTRING	0x690B080
#define LUA_DX12_156_NEWSTATE	0x6904B90
#define LUA_DX12_156_GETTOP		0x6904D00
#define LUA_DX12_156_SETTOP		0x6904D20

#define LUA_DX11_LOADBUFFER 0x6905F70
#define LUA_DX11_PCALL		0x690E1D0
#define LUA_DX11_TOLSTRING	0x690C530
#define LUA_DX11_NEWSTATE	0x6906040
#define LUA_DX11_GETTOP		0x69061B0
#define LUA_DX11_SETTOP		0x69061D0

// Runtime-selected engine module + Lua function offsets (set by DetectBuild()).
extern const char *g_engineModule;
extern uintptr_t lual_loadbuffer_t_off;
extern uintptr_t lua_pcall_t_off;
extern uintptr_t lua_tolstring_t_off;
extern uintptr_t lua_newstate_t_off;
extern uintptr_t lua_gettop_t_off;
extern uintptr_t lua_settop_t_off;

void DetectBuild();

typedef int(__cdecl *lua_gettop_t)(void *lua_state);
typedef void(__cdecl *lua_settop_t)(void *lua_state, int index);

typedef int(__cdecl *luaL_loadbuffer_t)(void *lua_state, const char *buff, size_t sz,
										const char *name);
typedef int(__cdecl *lua_pcall_t)(void *lua_state, int nargs, int nresults, int errfunc);
typedef const char *(__cdecl *lua_tolstring_t)(void *lua_state, int index, size_t *len);
typedef const char *(__cdecl *lua_tostring_t)(void *lua_state, int index);

class Main {
  public:
	void StartThread();
	int Execute(lua_State *L, const char *scriptData);
	void ExecuteFile(lua_State *L, const std::filesystem::path &filepath);
	void Unload();

  private:
	static int luaL_loadbuffer_t_trampoline(lua_State *lua_state, const char *buff, size_t sz,
											const char *name);
	static int luaL_pcall_t_trampoline(lua_State *L, int nargs, int nresults, int errfunc);
	uintptr_t GetGameBaseAddress();
	void InstallHook();
	static DWORD Entry(Main *main);
};

extern lua_State *context_lua_state;
