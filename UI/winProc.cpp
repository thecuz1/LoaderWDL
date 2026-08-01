#include "imgui.h"

#include "Logger.h"
#include "menu.h"

#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC       oWndProc = nullptr;
HWND          gameHWND = nullptr;


LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
        return true;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void UnhookWindow() {
    if (gameHWND) {
        SetWindowLongPtr(gameHWND, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        gameHWND = nullptr;
        oWndProc = nullptr;
    }
}


void HookWindow2(HWND hWindow) {
    Logger::LogMessage("[UI/winProc] Starting window hook\n");

    if (gameHWND) {
        UnhookWindow();
    }

    oWndProc = (WNDPROC)SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    gameHWND = hWindow;
}
