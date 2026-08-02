#include "MinHook/MinHook.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include "Logger.h"
#include "hookD3D12.h"
#include "menu.h"
#include "winProc.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <wrl/client.h>

#include <vector>

using Microsoft::WRL::ComPtr;

constexpr size_t kPresentIndex = 8;				 // Present
constexpr size_t kResizeBuffersIndex = 13;		 // ResizeBuffers
constexpr size_t kExecuteCommandListsIndex = 10; // ExecuteCommandLists

static HWND hDummyWindow = nullptr;
static const wchar_t *dummyClassName = L"DummyWndClass";

static ComPtr<IDXGISwapChain3> pSwapChain = nullptr;
static ComPtr<ID3D12Device> pDevice = nullptr;
static ComPtr<ID3D12CommandQueue> pCommandQueue = nullptr;

struct FrameContext {
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12Resource> renderTarget;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
};
static std::vector<FrameContext> gFrameContexts;

static ComPtr<ID3D12CommandQueue> gCommandQueue = nullptr;
static ComPtr<ID3D12DescriptorHeap> gHeapRTV = nullptr;
static ComPtr<ID3D12DescriptorHeap> gHeapSRV = nullptr;
static ComPtr<ID3D12GraphicsCommandList> gCommandList = nullptr;
static UINT gBufferCount = 0;

#define HOOK(name, ret, ...)                                                                       \
	typedef ret(APIENTRY *name##D3D12)(__VA_ARGS__); /* Function hook type */                      \
	LPVOID p##name##Target = nullptr;				 /* Target function */                         \
	static name##D3D12 o##name##D3D12 = nullptr;	 /* Original function */
LIST_OF_D3D12HOOKS
#undef HOOK

static bool gInitialized = false;
static bool gShutdown = false;

static HRESULT createDummyObjects() {
	WNDCLASSEXW wc = {sizeof(WNDCLASSEXW),
					  CS_CLASSDC,
					  DefWindowProcW,
					  0,
					  0,
					  GetModuleHandleW(nullptr),
					  nullptr,
					  nullptr,
					  nullptr,
					  nullptr,
					  dummyClassName,
					  nullptr};

	if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		Logger::LogMessage("[UI/d3d12] RegisterClassExW failed: %u\n", GetLastError());
		return E_FAIL;
	}

	hDummyWindow = CreateWindowExW(0, dummyClassName, L"Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
								   nullptr, nullptr, wc.hInstance, nullptr);
	if (!hDummyWindow) {
		Logger::LogMessage("[UI/d3d12] CreateWindowExW failed: %u\n", GetLastError());
		return E_FAIL;
	}

	ComPtr<IDXGIFactory4> factory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		Logger::LogMessage("[UI/d3d12] CreateDXGIFactory1 failed: 0x%08X\n", hr);
		return hr;
	}

	hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice));
	if (FAILED(hr)) {
		Logger::LogMessage("[UI/d3d12] D3D12CreateDevice failed: 0x%08X\n", hr);
		return hr;
	}

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = pDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&pCommandQueue));
	if (FAILED(hr)) {
		Logger::LogMessage("[UI/d3d12] CreateCommandQueue failed: 0x%08X\n", hr);
		return hr;
	}

	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.BufferCount = 2;
	scDesc.Width = 1;
	scDesc.Height = 1;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain1;
	hr = factory->CreateSwapChainForHwnd(pCommandQueue.Get(), hDummyWindow, &scDesc, nullptr,
										 nullptr, &swapChain1);
	if (FAILED(hr)) {
		Logger::LogMessage("[UI/d3d12] CreateSwapChainForHwnd failed: 0x%08X\n", hr);
		return hr;
	}

	hr = swapChain1.As(&pSwapChain);
	if (FAILED(hr)) {
		Logger::LogMessage("[UI/d3d12] QueryInterface IDXGISwapChain3 failed: 0x%08X\n", hr);
		return hr;
	}

	return S_OK;
}

static void CleanupDummyObjects() {
	if (hDummyWindow) {
		DestroyWindow(hDummyWindow);
		hDummyWindow = nullptr;
	}

	UnregisterClassW(dummyClassName, GetModuleHandle(nullptr));

	pSwapChain.Reset();
	pDevice.Reset();
	pCommandQueue.Reset();
}

HRESULT APIENTRY hookPresentD3D12(IDXGISwapChain3 *pSwapChain, UINT SyncInterval, UINT Flags) {
	if (!activelyHooked) {
		return oPresentD3D12(pSwapChain, SyncInterval, Flags);
	}

	if (!gCommandQueue) {
		Logger::LogMessage("[UI/d3d12] CommandQueue not yet captured, skipping frame\n");
		return oPresentD3D12(pSwapChain, SyncInterval, Flags);
	}

	if (!gInitialized) {
		Logger::LogMessage("[UI/d3d12] Initializing ImGui on first Present.\n");
		ID3D12Device *gDevice;

		if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void **)&gDevice))) {
			Logger::LogMessage("[UI/d3d12] GetDevice: hr=0x%08X\n", E_FAIL);
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		// Swap Chain description
		DXGI_SWAP_CHAIN_DESC desc = {};
		pSwapChain->GetDesc(&desc);
		gBufferCount = desc.BufferCount;
		Logger::LogMessage("[UI/d3d12] BufferCount=%u\n", gBufferCount);

		// Create descriptor heaps
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NumDescriptors = gBufferCount;
		if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapRTV)))) {
			Logger::LogMessage("[UI/d3d12] CreateDescriptorHeap RTV: hr=0x%08X\n", E_FAIL);
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapSRV)))) {
			Logger::LogMessage("[UI/d3d12] CreateDescriptorHeap SRV: hr=0x%08X\n", E_FAIL);
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		gFrameContexts.clear();
		gFrameContexts.resize(gBufferCount);

		// Create command allocator for each frame
		for (UINT i = 0; i < gBufferCount; ++i) {
			if (FAILED(gDevice->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gFrameContexts[i].allocator)))) {
				Logger::LogMessage("[UI/d3d12] CreateCommandAllocator: hr=0x%08X\n", E_FAIL);
				return oPresentD3D12(pSwapChain, SyncInterval, Flags);
			}
		}

		const UINT rtvSize =
			gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = gHeapRTV->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < gBufferCount; ++i) {
			gFrameContexts[i].rtvHandle = rtvHandle;
			pSwapChain->GetBuffer(i, IID_PPV_ARGS(&gFrameContexts[i].renderTarget));
			gDevice->CreateRenderTargetView(gFrameContexts[i].renderTarget.Get(), nullptr,
											rtvHandle);
			rtvHandle.ptr += rtvSize;
		}

		if (gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
									   gFrameContexts[0].allocator.Get(), nullptr,
									   IID_PPV_ARGS(&gCommandList)) != S_OK ||
			gCommandList->Close() != S_OK) {
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		// ImGui setup
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		// Don't let ImGui set/hide the OS cursor - the game manages it during gameplay (it hides it
		// for mouse-look). We draw ImGui's own cursor via io.MouseDrawCursor while the menu is open
		// instead.
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(desc.OutputWindow);

		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = gDevice;
		init_info.CommandQueue = gCommandQueue.Get();
		init_info.NumFramesInFlight = gBufferCount;
		init_info.RTVFormat = desc.BufferDesc.Format;
		init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		init_info.UserData = nullptr;

		init_info.SrvDescriptorHeap = gHeapSRV.Get();
		init_info.LegacySingleSrvCpuDescriptor = gHeapSRV->GetCPUDescriptorHandleForHeapStart();
		init_info.LegacySingleSrvGpuDescriptor = gHeapSRV->GetGPUDescriptorHandleForHeapStart();

		ImGui_ImplDX12_Init(&init_info);

		Logger::LogMessage("[UI/d3d12] ImGui initialized.\n");
		HookWindow2(desc.OutputWindow);

		gInitialized = true;
		gDevice->Release();
	}

	if (!gShutdown) {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		imguiInit();

		UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
		FrameContext &ctx = gFrameContexts[frameIdx];

		// Reset allocator and command list using frame-specific allocator
		HRESULT hr = ctx.allocator->Reset();
		if (FAILED(hr)) {
			Logger::LogMessage("[d3d12hook] CommandAllocator->Reset: hr=0x%08X\n", hr);
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		hr = gCommandList->Reset(ctx.allocator.Get(), nullptr);
		if (FAILED(hr)) {
			Logger::LogMessage("[d3d12hook] CommandList->Reset: hr=0x%08X\n", hr);
			return oPresentD3D12(pSwapChain, SyncInterval, Flags);
		}

		// Transition to render target
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = ctx.renderTarget.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gCommandList->ResourceBarrier(1, &barrier);

		gCommandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
		ID3D12DescriptorHeap *rawHeap = gHeapSRV.Get();
		gCommandList->SetDescriptorHeaps(1, &rawHeap);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gCommandList.Get());

		// Transition back to present
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		gCommandList->ResourceBarrier(1, &barrier);
		gCommandList->Close();

		// Execute
		if (!gCommandQueue) {
			Logger::LogMessage("[UI/d3d12] CommandQueue not set, skipping ExecuteCommandLists.\n");
		} else {
			ID3D12CommandList *cmdList = gCommandList.Get();
			gCommandQueue->ExecuteCommandLists(1, &cmdList);
		}
	}

	return oPresentD3D12(pSwapChain, SyncInterval, Flags);
}

void APIENTRY hookExecuteCommandListsD3D12(ID3D12CommandQueue *queue, UINT NumCommandLists,
										   ID3D12CommandList *const *ppCommandLists) {
	if (!gCommandQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
		gCommandQueue = queue;
	}

	oExecuteCommandListsD3D12(queue, NumCommandLists, ppCommandLists);
}

void resetState() {
	if (gInitialized) {
		gInitialized = false;
		ImGui_ImplWin32_Shutdown();
		ImGui_ImplDX12_Shutdown();
	}
	gCommandQueue = nullptr;
	gFrameContexts.clear();
	gBufferCount = 0;
	gCommandList = nullptr;
	gHeapRTV = nullptr;
	gHeapSRV = nullptr;
}

HRESULT APIENTRY hookResizeBuffersD3D12(IDXGISwapChain3 *pSwapChain, UINT BufferCount, UINT Width,
										UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
	Logger::LogMessage("[UI/d3d12] ResizeBuffers called: %ux%u Buffers=%u\n", Width, Height,
					   BufferCount);

	resetState();
	return oResizeBuffersD3D12(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT initD3D12Hooks() {
	Logger::LogMessage("[UI/d3d12] Starting d3d12 hook\n");

	struct CleanupGuard {
		~CleanupGuard() { CleanupDummyObjects(); }
	} cleanup;

	if (FAILED(createDummyObjects())) {
		Logger::LogMessage("[UI/d3d12] Failed to create dummy device/swapchain\n");
		return E_FAIL;
	}
	Logger::LogMessage("[UI/d3d12] Created dummy devices\n");

	auto scVTable = *reinterpret_cast<void ***>(pSwapChain.Get());
	auto cqVTable = *reinterpret_cast<void ***>(pCommandQueue.Get());

	pPresentTarget = reinterpret_cast<LPVOID>(scVTable[kPresentIndex]);
	pResizeBuffersTarget = reinterpret_cast<LPVOID>(scVTable[kResizeBuffersIndex]);
	pExecuteCommandListsTarget = reinterpret_cast<LPVOID>(cqVTable[kExecuteCommandListsIndex]);

	MH_STATUS mh;

#define HOOK(name, ...)                                                                            \
	mh = MH_CreateHook(p##name##Target, reinterpret_cast<LPVOID>(hook##name##D3D12),               \
					   reinterpret_cast<LPVOID *>(&o##name##D3D12));                               \
	if (mh != MH_OK) {                                                                             \
		Logger::LogMessage("[UI/d3d12] MH_CreateHook %s failed: %s\n", #name,                      \
						   MH_StatusToString(mh));                                                 \
		return E_FAIL;                                                                             \
	}
	LIST_OF_D3D12HOOKS
#undef HOOK
#define HOOK(name, ...)                                                                            \
	mh = MH_EnableHook(p##name##Target);                                                           \
	if (mh != MH_OK) {                                                                             \
		Logger::LogMessage("[UI/d3d12] MH_EnableHook %s failed: %s\n", #name,                      \
						   MH_StatusToString(mh));                                                 \
		return E_FAIL;                                                                             \
	}
	LIST_OF_D3D12HOOKS
#undef HOOK

	Logger::LogMessage("[UI/d3d12] Finished hooking\n");
	return S_OK;
}

void UnhookD3D12() {
	Logger::LogMessage("[UI/d3d12] Releasing resources and hooks\n");

#define HOOK(name, ...)                                                                            \
	if (p##name##Target) {                                                                         \
		MH_DisableHook(p##name##Target);                                                           \
		MH_RemoveHook(p##name##Target);                                                            \
		p##name##Target = nullptr;                                                                 \
	}
	LIST_OF_D3D12HOOKS
#undef HOOK

	Sleep(100);

	gShutdown = true;
	UnhookWindow();

	resetState();
	if (ImGui::GetCurrentContext()) {
		ImGui::DestroyContext();
	}
	Sleep(1000);
	return;
}
