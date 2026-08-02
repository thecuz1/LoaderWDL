#include <windows.h>
#include <minwindef.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstdint>

#include "imgui.h"

#include "menu.h"
#include "Logger.h"
#include "Main.h"
#include "hookD3D12.h"
#include "winProc.h"

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

    HookCursor();

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

struct ScriptNode {
    std::string name;
    bool isDir;
    uint32_t bitIndex;
    std::vector<ScriptNode> children;
};

static std::vector<ScriptNode> g_scriptTree;
static bool g_treeDirty = true;

void BuildScriptTree(std::vector<ScriptNode>& nodes, const std::filesystem::path& path, uint32_t& idx) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) break;
        ScriptNode node;
        node.name = entry.path().filename().string();
        node.isDir = entry.is_directory(ec);
        if (node.isDir) BuildScriptTree(node.children, entry.path(), idx);
        node.bitIndex = idx++;
        nodes.push_back(std::move(node));
    }
}

std::pair<bool, uint32_t> RenderTree(const std::vector<ScriptNode>& nodes, int* selection_mask) {
	ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

	bool any_clicked = false;
	uint32_t clicked = 0;

	for (const auto& node : nodes) {
		ImGuiTreeNodeFlags node_flags = base_flags;
		const bool is_selected = (*selection_mask & BIT(node.bitIndex)) != 0;
		if (is_selected) {
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (!node.isDir) {
			node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			if (ImGui::Button(("Run##" + std::to_string(node.bitIndex) + node.name).c_str())) {
				std::ifstream scriptFile(node.name);
				if (!scriptFile.is_open()) {
				    Logger::LogMessage("[UI/menu] Error opening script file: %s\n", node.name.c_str());
				} else {
				    Logger::LogMessage("[UI/menu] Opening script file: %s\n", node.name.c_str());
				}
				Logger::LogMessage("[UI/menu] Running script...\n");
				std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), (std::istreambuf_iterator<char>()));
				scriptFile.close();
				mainInstance->Execute(context_lua_state, scriptContent.c_str());
			}
		}

		bool node_open = ImGui::TreeNodeEx(node.name.c_str(), node_flags);

		if (ImGui::IsItemClicked()) {
			clicked = node.bitIndex;
			any_clicked = true;
		}

		if (node.isDir) {
			if (node_open) {
				auto clickState = RenderTree(node.children, selection_mask);

				if (!any_clicked) {
					any_clicked = clickState.first;
					clicked = clickState.second;
				}

				ImGui::TreePop();
			}
		}
	}

	return { any_clicked, clicked };
}

std::string directoryPath = "scripts";
char script[8192] = "";

void imguiInit() {
    static bool menu_open = false;

    // F1: tap to toggle
    if (GetAsyncKeyState(VK_F1) & 0x0001) {
        menu_open = !menu_open;
        menuOpen = menu_open;
        Logger::LogMessage(menu_open ? "[F1] Opening menu...\n" : "[F1] Closing menu...\n");
        if (menu_open) {
            ClipCursor(nullptr);
        }
    }

    if (!menu_open) {
        return;
    }

    // F2: tap to refresh script list
    if (GetAsyncKeyState(VK_F2) & 0x0001) {
        g_treeDirty = true;
        Logger::LogMessage("[F2] Refreshing script list...\n");
    }

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = true;

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
    bool isOpen = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);
    ImGui::Begin("ScriptHook", &isOpen, flags);
    if (ImGui::CollapsingHeader("Run View")) {
        if (ImGui::TreeNode("Terminal")) {
            ImGui::InputTextMultiline(nullptr, script, sizeof(script));
            if (ImGui::Button("Run")) {
                if (!context_lua_state) {
                    Logger::LogMessage("[Lua] ERROR: context_lua_state not set! Are you in the main menu?\n");
                } else {
                    mainInstance->Execute(context_lua_state, script);
                    Logger::LogMessage("[Lua] running script...\n");
                }
            }
            ImGui::TreePop();
        }
    }
   	if (ImGui::CollapsingHeader("Directory View"))	{

        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Script selection:\n"
                "  Click      - single-select\n"
                "  CTRL+click - toggle selection\n\n"
                "Selection is applied after the tree is drawn\n"
                "(outside the render loop) to avoid flicker.");
        }

 		if (g_treeDirty) {
 			uint32_t idx = 0;
 			g_scriptTree.clear();
 			BuildScriptTree(g_scriptTree, directoryPath, idx);
 			g_treeDirty = false;
 		}

  		static int selection_mask = 0;

 		auto clickState = RenderTree(g_scriptTree, &selection_mask);

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
    ImGui::End();
    ImGui::PopStyleVar();
}
