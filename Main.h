#pragma once

#include <windows.h> // IWYU pragma: keep
#include <minwindef.h>

#include <filesystem>

extern "C" {
    #include "lua.h"
}


#define MODULE_NAME "DuniaDemo_clang_64_dx12.dll"

#define LUA_OK          0
#define LUA_ERRSYNTAX   3

// function offsets
#ifdef OFFSETS_156
    #define lual_loadbuffer_t_off 0x6904AC0 // -0x2870
    #define lua_pcall_t_off 0x690cd20  // -0x2870
    #define lua_tolstring_t_off 0x690b080 // -0x2870
    #define lua_newstate_t_off 0x6904b90 // -0x2870

    #define lua_gettop_t_off 0x6904d00      // Unknown
    #define lua_settop_t_off 0x6904d20      // Unknown
#else
    #define lual_loadbuffer_t_off 0x6907330
    #define lua_pcall_t_off 0x690F590
    #define lua_tolstring_t_off 0x690D8F0
    #define lua_newstate_t_off 0x6907400

    #define lua_gettop_t_off 0x6907570
    #define lua_settop_t_off 0x6907590
#endif


typedef int(__cdecl* lua_gettop_t)(void* lua_state);
typedef void(__cdecl* lua_settop_t)(void* lua_state, int index);

typedef int(__cdecl* luaL_loadbuffer_t)(void* lua_state, const char* buff, size_t sz, const char* name);
typedef int(__cdecl* lua_pcall_t)(void* lua_state, int nargs, int nresults, int errfunc);
typedef const char* (__cdecl* lua_tolstring_t)(void* lua_state, int index, size_t* len);
typedef const char* (__cdecl* lua_tostring_t)(void* lua_state, int index);

class Main {
public:
	void StartThread();
	int Execute(lua_State* L, const char* scriptData);
	void ExecuteFile(lua_State* L, const std::filesystem::path& filepath);
	void Unload();

private:
	static int luaL_loadbuffer_t_trampoline(lua_State* lua_state, const char* buff, size_t sz, const char* name);
	static int luaL_pcall_t_trampoline(lua_State* L, int nargs, int nresults, int errfunc);
	uintptr_t GetGameBaseAddress();
	void InstallHook();
	static DWORD Entry(Main* main);
};

extern lua_State* context_lua_state;
