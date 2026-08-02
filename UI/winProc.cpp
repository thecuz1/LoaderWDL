#include "imgui.h"

#include "Logger.h"
#include "menu.h"
#include "winProc.h"
#include "MinHook/MinHook.h"

#include <windows.h>

// Window input plumbing for the ImGui menu. Two jobs:
//  1. Subclass the game's window procedure (HookWindow2/hkWndProc) so ImGui
//     receives mouse/keyboard input and can swallow it while the menu is open.
//  2. Neutralize the game's mouse-look cursor trapping (SetCursorPos/ClipCursor)
//     while the menu is open so the cursor can move freely (hkSetCursorPos/
//     hkClipCursor/HookCursor).

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC       oWndProc = nullptr;
HWND          gameHWND = nullptr;

// Original user32.dll functions, filled in by HookCursor().
static BOOL(WINAPI* oSetCursorPos)(int X, int Y) = nullptr;
static BOOL(WINAPI* oClipCursor)(const RECT* lpRect) = nullptr;
static bool  gCursorHooked = false;

// Mouse-look games pin the cursor every frame: ClipCursor to a small center
// box and SetCursorPos to recenter. While the menu is open that traps ImGui's
// io.MousePos, so we swallow recentering and force an unclip.
BOOL WINAPI hkSetCursorPos(int X, int Y) {
    if (menuOpen) {
        return TRUE; // pretend the recenter succeeded; don't move the cursor
    }
    return oSetCursorPos(X, Y);
}

BOOL WINAPI hkClipCursor(const RECT* lpRect) {
    if (menuOpen) {
        return oClipCursor(nullptr); // unclip instead of trapping in a box
    }
    return oClipCursor(lpRect);
}

// Install the two MinHook hooks on user32.dll's exports (already loaded, so
// GetProcAddress is safe - no LoadLibrary). Failure is logged, not fatal.
void HookCursor() {
    if (gCursorHooked) {
        return;
    }

    const LPVOID pSetCursorPos = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleA("user32.dll"), "SetCursorPos"));
    const LPVOID pClipCursor = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleA("user32.dll"), "ClipCursor"));

    MH_STATUS mh;
    mh = MH_CreateHook(pSetCursorPos, (LPVOID)&hkSetCursorPos, (LPVOID*)&oSetCursorPos);
    if (mh != MH_OK) {
        Logger::LogMessage("[UI/winProc] MH_CreateHook SetCursorPos failed: %s\n", MH_StatusToString(mh));
        return;
    }
    mh = MH_CreateHook(pClipCursor, (LPVOID)&hkClipCursor, (LPVOID*)&oClipCursor);
    if (mh != MH_OK) {
        Logger::LogMessage("[UI/winProc] MH_CreateHook ClipCursor failed: %s\n", MH_StatusToString(mh));
        return;
    }

    if (MH_EnableHook(pSetCursorPos) != MH_OK ||
        MH_EnableHook(pClipCursor) != MH_OK) {
        Logger::LogMessage("[UI/winProc] MH_EnableHook cursor hooks failed\n");
        return;
    }

    gCursorHooked = true;
    Logger::LogMessage("[UI/winProc] Cursor release hooks installed\n");
}


// Replacement window procedure installed on the game window (see HookWindow2).
// Routes messages to ImGui first, swallows them while ImGui wants capture, and
// forwards the rest to the game's original WNDPROC.
LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!activelyHooked) {
        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    // Window is being destroyed: tear down all hooks and forward the message.
    switch (uMsg) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_NCDESTROY:
            unhookAll();
            return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    // Let ImGui consume input first (updates io.MousePos, keys, scroll).
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    // While ImGui has a focused widget / hovered window, swallow the message
    // so the game never sees it (e.g. no firing a weapon while typing).
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
        return true;
    }

    // Everything else goes to the game's real WNDPROC.
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// Restore the game's original WNDPROC.
void UnhookWindow() {
    if (gameHWND) {
        SetWindowLongPtr(gameHWND, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        gameHWND = nullptr;
        oWndProc = nullptr;
    }
}


// Replace the game window's WNDPROC with hkWndProc, saving the original.
void HookWindow2(HWND hWindow) {
    Logger::LogMessage("[UI/winProc] Starting window hook\n");

    if (gameHWND) {
        UnhookWindow();
    }

    oWndProc = (WNDPROC)SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    gameHWND = hWindow;
}
