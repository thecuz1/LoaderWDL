# Watch_Dogs: Legion Scripthook

A Lua "scripthook" for Watch Dogs Legion, loaded into the game via a `dinput8` proxy shim. It hooks the game's Lua (`luaL_loadbuffer` / `lua_pcall` in `DuniaDemo_clang_64_dx12.dll`) to capture the live `lua_State`, and ships an ImGui menu for running Lua against it.

- Uses Lua **5.1** because WDL uses it internally — write Lua 5.1, not 5.2+.

## Install

- Deploy **both** `build/scripthook.dll` (payload) and `build/dinput8.dll` (shim) next to the game exe (`bin/`).
- Logs: `boot.log` is written by the game's exe, `scripthook.log` is in the game's working dir.
- If you're on Linux/Proton add `WINEDLLOVERRIDES="dinput8=n,b"` to your launch options.

## Usage

- `F1` toggles the menu. `F2` refreshes the script list.
- Directory View: click a script name to run it (no Run buttons).
- The inline terminal in the Run View runs the pasted script with the Run button.
- You must be **in-game** (not the main menu) before scripts run — `context_lua_state` is NULL until the game actually runs Lua.
- Scripts are listed from `scripts/` **relative to the game's working dir** (the game install dir, not the repo) — copy your scripts there or the menu won't see them.

## Building

Cross-compiled on Linux for Windows (MinGW). Requires `bear` and `x86_64-w64-mingw32-g++`.

```
make clean && bear -- make -j
```

Artifacts: `build/scripthook.dll` and `build/dinput8.dll`. Both link `-static` — do not ship MinGW runtime DLLs. Verify with `x86_64-w64-mingw32-objdump -p build/*.dll | grep "DLL Name"`.

## Notice

Many of the scripts in the scripts folder were graciously provided by EncryptedStudios.
