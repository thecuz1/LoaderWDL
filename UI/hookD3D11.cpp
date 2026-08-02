#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "MinHook/MinHook.h"
#include "Logger.h"
#include "menu.h"
#include "hookD3D11.h"
#include "winProc.h"

using Microsoft::WRL::ComPtr;

// IDXGISwapChain vtable slots (same positions as the D3D12 swap chain).
constexpr size_t kPresentIndex      = 8;  // Present
constexpr size_t kResizeBuffersIndex = 13; // ResizeBuffers

static HWND                       hDummyWindow = nullptr;
static const wchar_t* dummyClassName = L"DummyWndClass";

static ComPtr<IDXGISwapChain>        gSwapChain = nullptr;
static ComPtr<ID3D11Device>          gDevice = nullptr;
static ComPtr<ID3D11DeviceContext>   gContext = nullptr;
static ComPtr<ID3D11RenderTargetView> gMainRenderTargetView = nullptr;

#define HOOK(name, ret, ...) \
typedef ret (APIENTRY* name##D3D11)(__VA_ARGS__); /* Function hook type */ \
static LPVOID   p##name##Target = nullptr;        /* Target function */ \
static name##D3D11  o##name##D3D11 = nullptr;     /* Original function */
LIST_OF_D3D11HOOKS
#undef HOOK

static bool gInitialized = false;
static bool gShutdown    = false;

static HRESULT createDummyObjects() {
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        DefWindowProcW,
        0, 0,
        GetModuleHandleW(nullptr),
        nullptr, nullptr, nullptr, nullptr,
        dummyClassName,
        nullptr
    };

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::LogMessage("[UI/d3d11] RegisterClassExW failed: %u\n", GetLastError());
        return E_FAIL;
    }

    hDummyWindow = CreateWindowExW(
        0, dummyClassName, L"Dummy",
        WS_OVERLAPPEDWINDOW,
        0, 0, 1, 1,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    if (!hDummyWindow) {
        Logger::LogMessage("[UI/d3d11] CreateWindowExW failed: %u\n", GetLastError());
        return E_FAIL;
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 1;
    scDesc.BufferDesc.Width = 1;
    scDesc.BufferDesc.Height = 1;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hDummyWindow;
    scDesc.SampleDesc.Count = 1;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        &scDesc, &gSwapChain, &gDevice, nullptr, &gContext
    );
    if (FAILED(hr)) {
        Logger::LogMessage("[UI/d3d11] D3D11CreateDeviceAndSwapChain (hardware) failed: 0x%08X, trying WARP\n", hr);
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &scDesc, &gSwapChain, &gDevice, nullptr, &gContext
        );
    }
    if (FAILED(hr)) {
        Logger::LogMessage("[UI/d3d11] D3D11CreateDeviceAndSwapChain (WARP) failed: 0x%08X\n", hr);
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

    gSwapChain.Reset();
    gDevice.Reset();
    gContext.Reset();
    gMainRenderTargetView.Reset();
}

HRESULT APIENTRY hookPresentD3D11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!activelyHooked) {
        return oPresentD3D11(pSwapChain, SyncInterval, Flags);
    }

    if (!gInitialized) {
        Logger::LogMessage("[UI/d3d11] Initializing ImGui on first Present.\n");

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)device.GetAddressOf()))) {
            Logger::LogMessage("[UI/d3d11] GetDevice failed\n");
            return oPresentD3D11(pSwapChain, SyncInterval, Flags);
        }
        device->GetImmediateContext(context.GetAddressOf());

        gDevice = device;
        gContext = context;

        ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf()))) {
            Logger::LogMessage("[UI/d3d11] GetBuffer failed\n");
            return oPresentD3D11(pSwapChain, SyncInterval, Flags);
        }
        if (FAILED(gDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, gMainRenderTargetView.GetAddressOf()))) {
            Logger::LogMessage("[UI/d3d11] CreateRenderTargetView failed\n");
            return oPresentD3D11(pSwapChain, SyncInterval, Flags);
        }

        DXGI_SWAP_CHAIN_DESC desc = {};
        pSwapChain->GetDesc(&desc);

        // ImGui setup (mirrors the D3D12 hook).
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // Don't let ImGui set/hide the OS cursor - the game manages it during
        // gameplay. We draw ImGui's own cursor via io.MouseDrawCursor instead.
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(desc.OutputWindow);
        ImGui_ImplDX11_Init(gDevice.Get(), gContext.Get());

        Logger::LogMessage("[UI/d3d11] ImGui initialized.\n");
        HookWindow2(desc.OutputWindow);

        gInitialized = true;
    }

    if (!gShutdown) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        imguiInit();

        ImGui::Render();
        gContext->OMSetRenderTargets(1, gMainRenderTargetView.GetAddressOf(), nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresentD3D11(pSwapChain, SyncInterval, Flags);
}

void resetStateD3D11() {
    if (gInitialized) {
        gInitialized = false;
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
    }
    gMainRenderTargetView.Reset();
}

HRESULT APIENTRY hookResizeBuffersD3D11(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    Logger::LogMessage("[UI/d3d11] ResizeBuffers called: %ux%u Buffers=%u\n", Width, Height, BufferCount);

    resetStateD3D11();
    return oResizeBuffersD3D11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT initD3D11Hooks() {
    Logger::LogMessage("[UI/d3d11] Starting d3d11 hook\n");

    struct CleanupGuard {
        ~CleanupGuard() { CleanupDummyObjects(); }
    } cleanup;

    if (FAILED(createDummyObjects())) {
        Logger::LogMessage("[UI/d3d11] Failed to create dummy device/swapchain\n");
        return E_FAIL;
    }
    Logger::LogMessage("[UI/d3d11] Created dummy devices\n");

    auto scVTable = *reinterpret_cast<void***>(gSwapChain.Get());

    pPresentTarget = reinterpret_cast<LPVOID>(scVTable[kPresentIndex]);
    pResizeBuffersTarget = reinterpret_cast<LPVOID>(scVTable[kResizeBuffersIndex]);

    MH_STATUS mh;

    #define HOOK(name, ...) \
        mh = MH_CreateHook( \
            p##name##Target, \
            reinterpret_cast<LPVOID>(hook##name##D3D11), \
            reinterpret_cast<LPVOID*>(&o##name##D3D11) \
        ); \
        if (mh != MH_OK) { \
            Logger::LogMessage("[UI/d3d11] MH_CreateHook %s failed: %s\n", #name, MH_StatusToString(mh)); \
            return E_FAIL; \
        }
    LIST_OF_D3D11HOOKS
    #undef HOOK
    #define HOOK(name, ...) \
        mh = MH_EnableHook(p##name##Target); \
        if (mh != MH_OK) { \
            Logger::LogMessage("[UI/d3d11] MH_EnableHook %s failed: %s\n", #name, MH_StatusToString(mh)); \
            return E_FAIL; \
        }
    LIST_OF_D3D11HOOKS
    #undef HOOK

    Logger::LogMessage("[UI/d3d11] Finished hooking\n");
    return S_OK;
}

void UnhookD3D11() {
    Logger::LogMessage("[UI/d3d11] Releasing resources and hooks\n");

    #define HOOK(name, ...) \
    if (p##name##Target) { \
        MH_DisableHook(p##name##Target); \
        MH_RemoveHook(p##name##Target); \
        p##name##Target = nullptr; \
    }
    LIST_OF_D3D11HOOKS
    #undef HOOK

    Sleep(100);

    gShutdown = true;
    UnhookWindow();

    resetStateD3D11();
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }
    Sleep(1000);
    return;
}
