#pragma once

#include <windows.h>

HRESULT initD3D11Hooks();
void UnhookD3D11();

#define LIST_OF_D3D11HOOKS \
	HOOK(Present, HRESULT, IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) \
	HOOK(ResizeBuffers, HRESULT, IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
