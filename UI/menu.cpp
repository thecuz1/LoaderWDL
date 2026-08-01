#include <cstdint>
#include <minwindef.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include "imgui.h"
#include "menu.h"
#include "Logger.h"
#include "Main.h"
#include "hookD3D12.h"

Main* mainInstance;

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
    // UnhookWindow();
}

DWORD MenuThread(Main* main) {
    mainInstance = main;
    if (GetModuleHandleA("d3d12.dll") && GetModuleHandleA("dxgi.dll")) {
        Logger::LogMessage("[UI/menu] DirectX 12 detected\n");
    } else if (GetModuleHandleA("d3d11.dll")) {
        Logger::LogMessage("[UI/menu] Mod menu doesn't support DirectX 11, shutting down\n");
        return 0;
    } else {
        Logger::LogMessage("[UI/menu] Unknown graphics library, mod menu shutting down\n");
        return 0;
    }

    Logger::LogMessage("[UI/menu] Thread started\n");
    struct CleanupGuard {
        ~CleanupGuard() { unhookAll(); Logger::LogMessage("[UI/menu] Shutting down thread\n"); }
    } cleanup;

    const BOOL failed = hookAll();
    if (failed) {
        return 0;
    }

    while (true) {
        Sleep(500);
        if (!activelyHooked) {
            Logger::LogMessage("[UI/menu] We are no longer hooked, cleaning up\n");
            break;
        }
    }
    return 0;
}

#define BIT(x) (1 << x)

uint32_t count = 0;

std::pair<bool, uint32_t> DirectoryTreeViewRecursive(const std::filesystem::path& path, uint32_t* count, int* selection_mask) {
	ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

	static bool any_node_clicked = false;
	static uint32_t node_clicked = 0;

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		ImGuiTreeNodeFlags node_flags = base_flags;
		const bool is_selected = (*selection_mask & BIT(*count)) != 0;
		if (is_selected) {
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}

		std::string name = entry.path().string();

		auto lastSlash = name.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		name = name.substr(lastSlash, name.size() - lastSlash);

		bool entryIsFile = !std::filesystem::is_directory(entry.path());
		if (entryIsFile) {
			node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			if (ImGui::Button("Run##%s"), name.c_str()) {
				std::ifstream scriptFile(name);
				if (!scriptFile.is_open()) {
				    Logger::LogMessage("[UI/menu] Error opening script file: %s\n", name.c_str());
				} else {
				    Logger::LogMessage("[UI/menu] Opening script file: %s\n", name.c_str());
				}
				Logger::LogMessage("[UI/menu] Running script...\n");
				std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), (std::istreambuf_iterator<char>()));
				scriptFile.close();
				mainInstance->Execute(context_lua_state, scriptContent.c_str());
			}
		}

		bool node_open = ImGui::TreeNodeEx(name.c_str(), node_flags);

		if (ImGui::IsItemClicked()) {
			node_clicked = *count;
			any_node_clicked = true;
		}

		(*count)--;

		if (!entryIsFile) {
			if (node_open) {

				auto clickState = DirectoryTreeViewRecursive(entry.path(), count, selection_mask);

				if (!any_node_clicked) {
					any_node_clicked = clickState.first;
					node_clicked = clickState.second;
				}

				ImGui::TreePop();
			} else {
				for (const auto& e : std::filesystem::recursive_directory_iterator(entry.path())) {
					(*count)--;
				}
			}
		}
	}

	return { any_node_clicked, node_clicked };
}

std::string directoryPath = "scripts";
char script[8192] = "";
bool f1_pressed = false;

void imguiInit() {
    // ImGui::ShowDemoWindow();
    // return;
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
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
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
    if (GetAsyncKeyState(VK_F1) & 0x8000) {
		if (!f1_pressed) {
			f1_pressed = true;
			Logger::LogMessage("\n[F1] Opening menu...\n");
            ImGui::Begin("ScriptHook", &isOpen, flags);
		}
    }
    if (ImGui::CollapsingHeader("Scripts")) {
        if (ImGui::TreeNode("Terminal")) {
            ImGui::InputTextMultiline("<", script, sizeof(script));
            if (ImGui::Button("Run")) {
                mainInstance->Execute(context_lua_state, script);
                Logger::LogMessage("[Lua] running script...\n}");

            }
            ImGui::TreePop();
        }
    }
   	if (ImGui::CollapsingHeader("Scripts2"))	{

  		for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
 			count++;
  		}

  		static int selection_mask = 0;

  		auto clickState = DirectoryTreeViewRecursive(directoryPath, &count, &selection_mask);

  		if (clickState.first) {
 			// Update selection state
 			// (process outside of tree loop to avoid visual inconsistencies during the clicking frame)
 			if (ImGui::GetIO().KeyCtrl) {
				selection_mask ^= BIT(clickState.second);               // CTRL+click to toggle
 			//} else if (!(selection_mask & (1 << clickState.second)))  // Depending on selection behavior you want, may want to preserve selection when clicking on item that is part of the selection
 			} else {
				selection_mask = BIT(clickState.second);                // Click to single-select
 			}
  		}
   	}
    if (GetAsyncKeyState(VK_F1) & 0x8000) {
       	if (!f1_pressed) {
      		f1_pressed = true;
      		Logger::LogMessage("\n[F1] Closing menu...\n");
      		ImGui::End();
       	}
    }
}
