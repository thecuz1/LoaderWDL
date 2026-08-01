#include "Main.h"
#include "Logger.h"
#include "windows.h"
#include <cstdio>
#include "MinHook/MinHook.h"
#include <minwindef.h>
#include <string>
#include <filesystem>
#include <winnt.h>
#include "menu.h"

extern "C" {
    #include "lua.h"
}

using namespace std;

LPVOID luaL_loadbuffer_t_addr;
LPVOID luaL_pcall_t_addr;
lua_State* context_lua_state;
static CRITICAL_SECTION luaEngine_loadLock;

bool hasConsole = false;
DWORD Main::Entry(Main* main) // static
{
    HWND hGameWindow = NULL;
    while (hGameWindow == NULL)
    {
        hGameWindow = FindWindowA(NULL, "Watch Dogs Legion");

        if (hGameWindow == NULL)
        {
            Sleep(500);
        } else {
            break;
        }
    }
    Sleep(2000);

	InitializeCriticalSection(&luaEngine_loadLock);
	MH_Initialize();
	main->InstallHook();

	HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MenuThread, main, 0, 0);
    if (thread) CloseHandle(thread);
    else Logger::LogMessage("[Main] Failed to create MenuThread thread: %d\n", GetLastError());
    return 0;
}

uintptr_t Main::GetGameBaseAddress()
{
	return reinterpret_cast<uintptr_t>(GetModuleHandleA(MODULE_NAME));
}

int Main::luaL_pcall_t_trampoline(lua_State* L, int nargs, int nresults, int errfunc) {
   	if (L != NULL && context_lua_state == NULL) {
        Logger::LogMessage("Captured lua_state from pcall!\n");
		context_lua_state = L;
    }

	lua_pcall_t luaL_pcall_t_call = reinterpret_cast<lua_pcall_t>(luaL_pcall_t_addr);
	return luaL_pcall_t_call(L, nargs, nresults, errfunc);
}

int Main::luaL_loadbuffer_t_trampoline(lua_State* lua_state, const char* buff, size_t sz, const char* name) {
	if (lua_state != NULL)
		context_lua_state = lua_state;
	else {
	    Logger::LogMessage("=== ERROR: failed to set lua_state because it is NULL ===\n");
	}

	Logger::LogMessage("=== Lua Script Loaded ===\n");

	// Printa o nome do arquivo/script
	if (name != NULL) {
		Logger::LogMessage("Script Name: %s\n", name);
	}
	else {
		Logger::LogMessage("Script Name: [unnamed]\n");
	}

	// Printa o tamanho do buffer
	Logger::LogMessage("Buffer Size: %zu bytes\n", sz);

	// Printa o conte�do do script Lua
	if (buff != NULL && sz > 0) {
		Logger::LogMessage("Script Content:\n");
		Logger::LogMessage("------------------------\n");

		// Cria uma string tempor�ria para garantir null-termination
		std::string script_content(buff, sz);
		Logger::LogMessage("%s\n", script_content.c_str());

		Logger::LogMessage("------------------------\n");
	}

	Logger::LogMessage("\n");
	// call the original function to avoid breaking the game
	luaL_loadbuffer_t luaL_loadbuffer_t_call = reinterpret_cast<luaL_loadbuffer_t>(luaL_loadbuffer_t_addr);
	return luaL_loadbuffer_t_call(lua_state, buff, sz, name);
}

int Main::Execute(lua_State* L, const char* scriptData)
{
	lua_tolstring_t lua_tolstring_t_call = reinterpret_cast<lua_tolstring_t>((GetGameBaseAddress() + lua_tolstring_t_off));
	lua_pcall_t lua_pcall_t_call = reinterpret_cast<lua_pcall_t>((GetGameBaseAddress() + lua_pcall_t_off));

	luaL_loadbuffer_t luaL_loadbuffer_t_call = reinterpret_cast<luaL_loadbuffer_t>(luaL_loadbuffer_t_addr);
	int loadResult = luaL_loadbuffer_t_call(L, scriptData, strlen(scriptData), "exec");

	if (loadResult != LUA_OK)
	{
	    const char* err = nullptr;
		if (lua_isstring(L, -1))
        {
            err = lua_tolstring_t_call(L, -1, NULL);
        }
		Logger::LogMessage("Compilation error: %s\n", err ? err : "nil");
		lua_pop(L, 1);
		return LUA_ERRSYNTAX;
	}

	// Leave critical section because call below might never return (loop)
	LeaveCriticalSection(&luaEngine_loadLock);

	int result = lua_pcall_t_call(L, 0, 0, 0);
	EnterCriticalSection(&luaEngine_loadLock);
	if (result != LUA_OK)
	{
		const char* err = lua_tolstring_t_call(L, -1, NULL);
		Logger::LogMessage("Execution error: %s\n", err);
		lua_pop(L, 1);
	}
	// const int value = lua_gettop(L);
	// Logger::LogMessage("Stack is size: %d\n", value);

	return result;
}

void Main::ExecuteFile(lua_State* L, const std::filesystem::path& filepath) {
    struct FileDeleter {
        void operator()(FILE* f) const {
            if (f) std::fclose(f);
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
	PBYTE load_buffer_func = (PBYTE)(baseAddress + lual_loadbuffer_t_off);
	PBYTE lua_pcall_func = (PBYTE)(baseAddress + lua_pcall_t_off);

	Logger::LogMessage("Base Address: %p\n", baseAddress);
	Logger::LogMessage("Call Address (lual_loadbuffer): %p\n", load_buffer_func);
	Logger::LogMessage("Call Address (lua_pcall): %p\n", lua_pcall_func);

	if (MH_CreateHook(load_buffer_func, (LPVOID)&luaL_loadbuffer_t_trampoline, (LPVOID*)&luaL_loadbuffer_t_addr) != MH_OK) {
		Logger::LogMessage("Failed to create hook! (lual_loadbuffer)\n");
		return;
	}

	if (MH_CreateHook(lua_pcall_func, (LPVOID)&luaL_pcall_t_trampoline, (LPVOID*)&luaL_pcall_t_addr) != MH_OK) {
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
	// print t_addr
	Logger::LogMessage("Original Hook address (lual_loadbuffer): %p\n", luaL_loadbuffer_t_addr);
	Logger::LogMessage("Original Hook address (lua_pcall): %p\n", luaL_pcall_t_addr);
}

void Main::StartThread()
{
	Logger::Initialize("Teste!");
	Logger::LogMessage("\n");

	hasConsole = AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "wb", stdout);

	HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Main::Entry, this, 0, 0);
    if (thread) CloseHandle(thread);
    else Logger::LogMessage("[Main] Failed to create main thread: %d\n", GetLastError());
}

void Main::Unload()
{
	Logger::Shutdown();
	if (hasConsole)
	{
		fclose(stdout);
		hasConsole = !FreeConsole();
	}
}
