#include <minwindef.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "imgui.h"

#include "Logger.h"
#include "Main.h"
#include "hookD3D11.h"
#include "hookD3D12.h"
#include "menu.h"
#include "winProc.h"

Main *mainInstance;

HRESULT hookAll() {
	if (GetModuleHandleA("d3d12.dll") && GetModuleHandleA("dxgi.dll")) {
		if (FAILED(initD3D12Hooks())) {
			return E_FAIL;
		}
	} else if (GetModuleHandleA("d3d11.dll")) {
		if (FAILED(initD3D11Hooks())) {
			return E_FAIL;
		}
	} else {
		Logger::LogMessage("[UI/menu] No D3D12 or D3D11 runtime loaded, mod menu cannot hook\n");
		return E_FAIL;
	}
	activelyHooked = true;
	return S_OK;
}

void unhookAll() {
	activelyHooked = false;
	if (GetModuleHandleA("d3d12.dll") && GetModuleHandleA("dxgi.dll")) {
		UnhookD3D12();
	} else if (GetModuleHandleA("d3d11.dll")) {
		UnhookD3D11();
	}
}

DWORD MenuThread(Main *main) {
	mainInstance = main;
	if (GetModuleHandleA("d3d12.dll") && GetModuleHandleA("dxgi.dll")) {
		Logger::LogMessage("[UI/menu] DirectX 12 detected\n");
	} else if (GetModuleHandleA("d3d11.dll")) {
		Logger::LogMessage("[UI/menu] DirectX 11 detected\n");
	} else {
		Logger::LogMessage("[UI/menu] Unknown graphics library, mod menu shutting down\n");
		return 0;
	}

	Logger::LogMessage("[UI/menu] Thread started\n");
	struct CleanupGuard {
		~CleanupGuard() {
			unhookAll();
			Logger::LogMessage("[UI/menu] Shutting down thread\n");
		}
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

struct ScriptNode {
	std::string name;
	std::string path; // full path relative to the game CWD, used to open the file
	bool isDir;
	std::vector<ScriptNode> children;
};

static std::vector<ScriptNode> g_scriptTree;
static bool g_treeDirty = true;

void BuildScriptTree(std::vector<ScriptNode> &nodes, const std::filesystem::path &path) {
	std::error_code ec;
	for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
		if (ec) {
			break;
		}
		if (!entry.is_directory(ec) && entry.path().extension() != ".lua") {
			continue;
		}
		ScriptNode node;
		node.name = entry.path().filename().string();
		node.path = entry.path().string();
		node.isDir = entry.is_directory(ec);
		if (node.isDir) {
			BuildScriptTree(node.children, entry.path());
		}
		nodes.push_back(std::move(node));
	}
}

void RenderTree(const std::vector<ScriptNode> &nodes) {
	ImGuiTreeNodeFlags base_flags =
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

	for (const auto &node : nodes) {
		ImGuiTreeNodeFlags node_flags = base_flags;

		if (!node.isDir) {
			node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			ImGui::TreeNodeEx(node.name.c_str(), node_flags);
			if (ImGui::IsItemClicked()) {
				if (!context_lua_state) {
					Logger::LogMessage(
						"[UI/menu] Cannot run %s: context_lua_state not set (go in-game first)\n",
						node.name.c_str());
				} else {
					std::ifstream scriptFile(node.path);
					if (!scriptFile.is_open()) {
						Logger::LogMessage("[UI/menu] Error opening script file: %s\n",
										   node.name.c_str());
					} else {
						Logger::LogMessage("[UI/menu] Opening script file: %s\n",
										   node.name.c_str());
						std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)),
												  (std::istreambuf_iterator<char>()));
						scriptFile.close();
						Logger::LogMessage("[UI/menu] Running script...\n");
						mainInstance->Execute(context_lua_state, scriptContent.c_str());
					}
				}
			}
		} else {
			bool node_open = ImGui::TreeNodeEx(node.name.c_str(), node_flags);

			if (node_open) {
				RenderTree(node.children);
				ImGui::TreePop();
			}
		}
	}
}

std::string directoryPath = "scripts";
char script[8192] = "";

void imguiInit() {
	static bool menu_open = false;

	// F1: tap to toggle
	if (GetAsyncKeyState(VK_F1) & 0x0001) {
		menu_open = !menu_open;
		menuOpen = menu_open;
		ImGui::GetIO().MouseDrawCursor = menu_open;
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

	// Style setup (one-time)
	static bool styled = false;
	if (!styled) {
		ImGui::StyleColorsDark();
		ImVec4 *colors = ImGui::GetStyle().Colors;
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
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(25, 25), ImGuiCond_FirstUseEver);
	ImGui::Begin("ScriptHook", &isOpen, flags);
	// X button clicked: close the whole menu (not just the window) so it doesn't just re-open next
	// frame.
	if (!isOpen) {
		menu_open = false;
		menuOpen = false;
		ImGui::GetIO().MouseDrawCursor = false;
		Logger::LogMessage("[UI/menu] Menu closed via X\n");
	}
	if (ImGui::CollapsingHeader("Run View")) {
		if (ImGui::TreeNode("Terminal")) {
			ImGui::InputTextMultiline(nullptr, script, sizeof(script));
			if (ImGui::Button("Run")) {
				if (!context_lua_state) {
					Logger::LogMessage(
						"[Lua] ERROR: context_lua_state not set! Are you in the main menu?\n");
				} else {
					mainInstance->Execute(context_lua_state, script);
					Logger::LogMessage("[Lua] running script...\n");
				}
			}
			ImGui::TreePop();
		}
	}
	if (ImGui::CollapsingHeader("Directory View")) {

		ImGui::SetItemTooltip("Scripts:\n"
							  "  Click  - run the script\n");

		if (g_treeDirty) {
			g_scriptTree.clear();
			BuildScriptTree(g_scriptTree, directoryPath);
			g_treeDirty = false;
		}

		RenderTree(g_scriptTree);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
