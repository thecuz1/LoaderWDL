#include <windows.h>
#include "Logger.h"
#include "imgui.h"
#include "menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC       oWndProc = nullptr;
HWND          gameHWND = nullptr;


LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // if (!oWndProc) {
    //     return 0;
    // }

    static BOOL hereFirst = true;
    if (hereFirst) {
        Logger::LogMessage("[UI/winProc] First WndProc call\n");
        hereFirst = !hereFirst;
    }

    if (!activelyHooked) {
        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_NCDESTROY:
            unhookAll();
            return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
    {
        return true;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
    DWORD processId;
    GetWindowThreadProcessId(handle, &processId);
    if (processId == GetCurrentProcessId() && GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle))
    {
        gameHWND = handle;
        return FALSE;
    }
    return TRUE;
}
HRESULT HookWindow()
{
    Logger::LogMessage("[UI/winProc] Starting window hook\n");
    EnumWindows(EnumWindowsCallback, 0);

    if (gameHWND)
    {
        oWndProc = (WNDPROC)SetWindowLongPtr(gameHWND, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
        Logger::LogMessage("[UI/winProc] Hooked game window\n");
        return S_OK;
    } else {
        Logger::LogMessage("[UI/winProc] Failed to hook game window\n");
        return E_FAIL;
    }
}
void UnhookWindow()
{
    if (gameHWND)
    {
        SetWindowLongPtr(gameHWND, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        gameHWND = nullptr;
        oWndProc = nullptr;
    }
}


void HookWindow2(HWND hWindow)
{
    Logger::LogMessage("[UI/winProc] Starting window hook\n");
    // EnumWindows(EnumWindowsCallback, 0);
    if (gameHWND) UnhookWindow();
    oWndProc = (WNDPROC)SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    gameHWND = hWindow;

    // if (gameHWND)
    // {
    //     oWndProc = (WNDPROC)SetWindowLongPtr(gameHWND, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    //     Logger::LogMessage("[UI/winProc] Hooked game window\n");
    //     return S_OK;
    // } else {
    //     Logger::LogMessage("[UI/winProc] Failed to hook game window\n");
    //     return E_FAIL;
    // }
}
