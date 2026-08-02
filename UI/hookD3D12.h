#pragma once

#include <windows.h>

HRESULT initD3D12Hooks();
void UnhookD3D12();

#define LIST_OF_D3D12HOOKS                                                                         \
	HOOK(Present, HRESULT, IDXGISwapChain3 *pSwapChain, UINT SyncInterval, UINT Flags)             \
	HOOK(ExecuteCommandLists, void, ID3D12CommandQueue *_this, UINT NumCommandLists,               \
		 ID3D12CommandList *const *ppCommandLists)                                                 \
	HOOK(ResizeBuffers, HRESULT, IDXGISwapChain3 *pSwapChain, UINT BufferCount, UINT Width,        \
		 UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
