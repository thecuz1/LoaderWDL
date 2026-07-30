#include "menu.h"
#include <windows.h>
#include "Logger.h"
#include "Main.h"
#include "winProc.h"
#include "hookD3D12.h"
#include "imgui.h"

HRESULT hookAll() {
    // if (FAILED(HookWindow())) {
    //     return E_FAIL;
    // }
    if (FAILED(initD3D12Hooks())) {
        return E_FAIL;
    }
    activelyHooked = true;
    return S_OK;
}

void unhookAll() {
    activelyHooked = false;
    UnhookD3D12();
    UnhookWindow();
}

void MenuThread(Main* main) {
    if (GetModuleHandleA("d3d12.dll") && GetModuleHandleA("dxgi.dll")) {
        Logger::LogMessage("[UI/menu] DirectX 12 detected\n");
    } else if (GetModuleHandleA("d3d11.dll")) {
        Logger::LogMessage("[UI/menu] Mod menu doesn't support DirectX 11, shutting down\n");
        return;
    } else {
        Logger::LogMessage("[UI/menu] Unknown graphics library, mod menu shutting down\n");
        return;
    }

    Logger::LogMessage("[UI/menu] Thread started\n");
    struct CleanupGuard {
        ~CleanupGuard() { unhookAll(); Logger::LogMessage("[UI/menu] Shutting down thread\n"); }
    } cleanup;

    const BOOL failed = hookAll();
    if (failed) {
        return;
    }

    while (true) {
        Sleep(500);
        if (!activelyHooked) {
            Logger::LogMessage("[UI/menu] We are no longer hooked, cleaning up\n");
            return;
        }
    }
}

void imguiInit() {
    ImGui::Text("Hello World");
    // ImGui::ShowDemoWindow();
    return;
    bool isOpen = true;

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = isOpen;

    if (!isOpen) {
        return;
    }

    // Style setup (one-time)
    static bool styled = false;
    if (!styled) {
        ImGui::StyleColorsDark();
        ImVec4* colors = ImGui::GetStyle().Colors;
        // Custom color palette
        colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0.8f);
        colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 0.8f);
        colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.4f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
        styled = true;
        Logger::LogMessage("[UI/menu] Style applied\n");
    }

    // Window flags
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);

    ImGui::Begin("ImGui Menu", &isOpen, flags);

    if (ImGui::CollapsingHeader("MENU")) {
        if (ImGui::TreeNode("SUB MENU")) {
            ImGui::Text("Text Test");
            if (ImGui::Button("Button Test")) {
                Logger::LogMessage("[UI/menu] Button Test clicked.\n");
            }
            // if (ImGui::Checkbox("No Title Bar", &noTitleBar)) {
            //     Logger::LogMessage("[UI/menu] Checkbox No Title Bar toggled. flags=0x%X\n", flags);
            // }
            // ImGui::SliderFloat("Slider Test", &test, 1.0f, 100.0f);
            // ImGui::Text("Slider value=%.2f", test);
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
