#include "dx12_renderer.h"

#include "sablujo.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

global_variable const uint32_t FrameCount = 2;
global_variable uint32_t CurrentFrame;


// TODO(Gouzi): Temporary globals
struct dx12_present_synchronization
{
    HANDLE FenceEvent;
    ComPtr<ID3D12Fence> Fence;
    uint64_t FenceValues[FrameCount];
};

global_variable dx12_present_synchronization Synchronization;
global_variable ComPtr<IDXGISwapChain4> SwapChain;
global_variable ComPtr<ID3D12CommandQueue> CommandQueue;

inline void
ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        Assert(false);
    }
}

// Wait for pending GPU work to complete.
void DX12WaitForGpu()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(CommandQueue->Signal(Synchronization.Fence.Get(), Synchronization.FenceValues[CurrentFrame]));
    
    // Wait until the fence has been processed.
    ThrowIfFailed(Synchronization.Fence->SetEventOnCompletion(Synchronization.FenceValues[CurrentFrame], 
                                                              Synchronization.FenceEvent));
    WaitForSingleObjectEx(Synchronization.FenceEvent, INFINITE, FALSE);
    
    // Increment the fence value for the current frame.
    Synchronization.FenceValues[CurrentFrame]++;
}

void
DX12InitRenderer(HWND Window, win32_window_dimension Dimension)
{
    uint32_t DXGIFactoryFlags = 0;
#if SABLUJO_INTERNAL
    ComPtr<ID3D12Debug> DebugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
    {
        DebugController->EnableDebugLayer();
        DXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    ComPtr<IDXGIFactory7> Factory;
    ThrowIfFailed(CreateDXGIFactory2(DXGIFactoryFlags, IID_PPV_ARGS(&Factory)));
    
    ComPtr<IDXGIAdapter4> HardwareAdapter;
    ComPtr<ID3D12Device8> Device;
    for (UINT adapterIndex = 0; 
         DXGI_ERROR_NOT_FOUND != Factory->EnumAdapterByGpuPreference(adapterIndex,
                                                                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                     IID_PPV_ARGS(&HardwareAdapter));
         ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 AdapterDesc;
        HardwareAdapter->GetDesc1(&AdapterDesc);
        
        if (AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }
        
        // Check to see whether the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(HardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&Device))))
        {
            break;
        }
    }
    
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    QueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue)));
    
    ComPtr<IDXGISwapChain1> NewSwapChain;
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Width = Dimension.Width;
    SwapChainDesc.Height = Dimension.Height;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //SwapChainDesc.Stero = FALSE;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = FrameCount;
    //SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    //SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    //SwapChainDesc.Flags = 0;
    
    ThrowIfFailed(Factory->CreateSwapChainForHwnd(CommandQueue.Get(), 
                                                  Window, 
                                                  &SwapChainDesc, 
                                                  nullptr, 
                                                  nullptr, 
                                                  &NewSwapChain));
    
    ThrowIfFailed(NewSwapChain.As(&SwapChain));
    
    CurrentFrame = SwapChain->GetCurrentBackBufferIndex();
    
    
    // Create descriptor heaps.
    // Describe and create a render target view (RTV) descriptor heap.
    ComPtr<ID3D12DescriptorHeap> RTVHeap;
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.NumDescriptors = FrameCount;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(Device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&RTVHeap)));
    
    uint32_t RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    
    // Create frame resources.
    
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
    // Create a RTV for each frame.
    ComPtr<ID3D12Resource> RenderTargets[FrameCount];
    for (uint32_t n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(SwapChain->GetBuffer(n, IID_PPV_ARGS(&RenderTargets[n])));
        Device->CreateRenderTargetView(RenderTargets[n].Get(), nullptr, RTVHandle);
        RTVHandle.ptr += RTVDescriptorSize;
    }
    
    
    ComPtr<ID3D12CommandAllocator> CommandAllocator;
    ThrowIfFailed(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)));
    
    
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(Device->CreateFence(Synchronization.FenceValues[CurrentFrame], 
                                          D3D12_FENCE_FLAG_NONE, 
                                          IID_PPV_ARGS(&Synchronization.Fence)));
        Synchronization.FenceValues[CurrentFrame]++;
        
        // Create an event handle to use for frame synchronization.
        Synchronization.FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (Synchronization.FenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        
        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        DX12WaitForGpu();
    }
}

void
DX12Present()
{
    ThrowIfFailed(SwapChain->Present(0, 0));
    
    // Schedule a Signal command in the queue.
    uint64_t CurrentFenceValue = Synchronization.FenceValues[CurrentFrame];
    ThrowIfFailed(CommandQueue->Signal(Synchronization.Fence.Get(), CurrentFenceValue));
    
    // Update the frame index.
    CurrentFrame = SwapChain->GetCurrentBackBufferIndex();
    
    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (Synchronization.Fence->GetCompletedValue() < Synchronization.FenceValues[CurrentFrame])
    {
        ThrowIfFailed(Synchronization.Fence->SetEventOnCompletion(Synchronization.FenceValues[CurrentFrame], Synchronization.FenceEvent));
        WaitForSingleObjectEx(Synchronization.FenceEvent, INFINITE, FALSE);
    }
    
    // Set the fence value for the next frame.
    Synchronization.FenceValues[CurrentFrame] = CurrentFenceValue + 1;
}

void
DX12ShutdownRenderer()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    DX12WaitForGpu();
    
    CloseHandle(Synchronization.FenceEvent);
}