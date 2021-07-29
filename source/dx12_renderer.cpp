#include "dx12_renderer.h"

#include "sablujo.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <D3DCompiler.h>

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

#define TRIANGLE_EXAMPLE 0
#if TRIANGLE_EXAMPLE
#include <directxmath.h>
using namespace DirectX;
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};
#endif

global_variable dx12_present_synchronization Synchronization;
global_variable ComPtr<IDXGISwapChain4> SwapChain;
global_variable ComPtr<ID3D12CommandQueue> CommandQueue;
global_variable ComPtr<ID3D12Device8> Device;
global_variable ComPtr<ID3D12DescriptorHeap> RTVHeap;
global_variable ComPtr<ID3D12Resource> RenderTargets[FrameCount];
global_variable ComPtr<ID3D12CommandAllocator> CommandAllocators[FrameCount];


global_variable ComPtr<ID3D12RootSignature> RootSignature;
global_variable ComPtr<ID3D12PipelineState> PipelineState;
global_variable ComPtr<ID3D12GraphicsCommandList> CommandList;

global_variable D3D12_VIEWPORT Viewport;
global_variable D3D12_RECT ScissorRect;


// App resources.

#if TRIANGLE_EXAMPLE
global_variable ComPtr<ID3D12Resource> VertexBuffer;
global_variable D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
#else
global_variable ComPtr<ID3D12Resource> BackBuffer;
global_variable D3D12_SUBRESOURCE_FOOTPRINT BackBufferFootPrint;
#endif
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


#if TRIANGLE_EXAMPLE
void 
DX12CreateVertexBuffer()
{
    // Define the geometry for a triangle.
    Vertex TriangleVertices[] =
    {
        { {  0.0f,   0.25f * 1.7777777777f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { {  0.25f, -0.25f * 1.7777777777f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.25f, -0.25f * 1.7777777777f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    
    const UINT VertexBufferSize = sizeof(TriangleVertices);
    
    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    D3D12_HEAP_PROPERTIES UploadHeapProperties = {};
    UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    //UploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    //UploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    //UploadHeapProperties.CreationNodeMask = 1; Passing zero is equivalent to passing one, in order to simplify the usage of single-GPU adapters.
    //UploadHeapProperties.VisibleNodeMask = 1; Passing zero is equivalent to passing one, in order to simplify the usage of single-GPU adapters.
    
    D3D12_RESOURCE_DESC VertexBufferDesc = {};
    VertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    //VertexBufferDesc.Alignment = 0;
    VertexBufferDesc.Width = VertexBufferSize;
    VertexBufferDesc.Height = 1;
    VertexBufferDesc.DepthOrArraySize = 1;
    VertexBufferDesc.MipLevels = 1;
    VertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    VertexBufferDesc.SampleDesc.Count = 1;
    //VertexBufferDesc.SampleDesc.Quality = 0;
    VertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    VertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    ThrowIfFailed(Device->CreateCommittedResource(&UploadHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &VertexBufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr,
                                                  IID_PPV_ARGS(&VertexBuffer)));
    
    // Copy the triangle data to the vertex buffer.
    uint8_t* VertexDataBegin;
    D3D12_RANGE ReadRange = {0, 0}; // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(VertexBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin)));
    memcpy(VertexDataBegin, TriangleVertices, sizeof(TriangleVertices));
    VertexBuffer->Unmap(0, nullptr);
    
    // Initialize the vertex buffer view.
    VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(Vertex);
    VertexBufferView.SizeInBytes = VertexBufferSize;
}
#endif



void
DX12LoadAssets()
{
    // Create an empty root signature.
    D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
    RootSignatureDesc.NumParameters = 0;
    RootSignatureDesc.pParameters = nullptr;
    RootSignatureDesc.NumStaticSamplers = 0;
    RootSignatureDesc.pStaticSamplers = nullptr;
    RootSignatureDesc.Flags = 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
    
    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    ThrowIfFailed(D3D12SerializeRootSignature(&RootSignatureDesc, 
                                              D3D_ROOT_SIGNATURE_VERSION_1, 
                                              &Signature, 
                                              &Error));
    ThrowIfFailed(Device->CreateRootSignature(0, 
                                              Signature->GetBufferPointer(), 
                                              Signature->GetBufferSize(), 
                                              IID_PPV_ARGS(&RootSignature)));
    
    
    // Create the pipeline state, which includes compiling and loading shaders.
    ComPtr<ID3DBlob> VertexShader;
    ComPtr<ID3DBlob> PixelShader;
    
    UINT CompileFlags = 0;
#if SABLUJO_INTERNAL
    // Enable better shader debugging with the graphics debugging tools.
    CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    
    ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/base_color.hlsl", 
                                     nullptr, nullptr, 
                                     "VS", "vs_5_0", 
                                     CompileFlags, 0, 
                                     &VertexShader, &Error));
    
    ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/base_color.hlsl", 
                                     nullptr, nullptr, 
                                     "FS", "ps_5_0", 
                                     CompileFlags, 0, 
                                     &PixelShader, &Error));
    
    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC InputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    
    
    
    
    // Pipeline State Object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = RootSignature.Get();
    PSODesc.VS = D3D12_SHADER_BYTECODE{VertexShader->GetBufferPointer(), VertexShader->GetBufferSize()};
    PSODesc.PS = D3D12_SHADER_BYTECODE{PixelShader->GetBufferPointer(), PixelShader->GetBufferSize()};
    
    // PSODesc.DS;
    // PSODesc.HS;
    // PSODesc.GS;
    // PSODesc.StreamOutput;
    
    // Default blend state
    PSODesc.BlendState.AlphaToCoverageEnable = FALSE;
    PSODesc.BlendState.IndependentBlendEnable = FALSE;
    
    D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendDesc;
    DefaultRenderTargetBlendDesc.BlendEnable    = FALSE;
    DefaultRenderTargetBlendDesc.LogicOpEnable  = FALSE;
    DefaultRenderTargetBlendDesc.SrcBlend       = D3D12_BLEND_ONE;
    DefaultRenderTargetBlendDesc.DestBlend      = D3D12_BLEND_ZERO;
    DefaultRenderTargetBlendDesc.BlendOp        = D3D12_BLEND_OP_ADD;
    DefaultRenderTargetBlendDesc.SrcBlendAlpha  = D3D12_BLEND_ONE;
    DefaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    DefaultRenderTargetBlendDesc.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
    DefaultRenderTargetBlendDesc.LogicOp        = D3D12_LOGIC_OP_NOOP;
    DefaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        PSODesc.BlendState.RenderTarget[ i ] = DefaultRenderTargetBlendDesc;
    }
    
    PSODesc.SampleMask = UINT_MAX;
    
    // Default rasterizer state
    PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    PSODesc.RasterizerState.FrontCounterClockwise = FALSE;
    PSODesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    PSODesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    PSODesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    PSODesc.RasterizerState.DepthClipEnable = TRUE;
    PSODesc.RasterizerState.MultisampleEnable = FALSE;
    PSODesc.RasterizerState.AntialiasedLineEnable = FALSE;
    PSODesc.RasterizerState.ForcedSampleCount = 0;
    PSODesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    
    PSODesc.DepthStencilState.DepthEnable = FALSE;
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    PSODesc.InputLayout = { InputElementDescs, _countof(InputElementDescs) };
    
    PSODesc.IBStripCutValue;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // PSODesc.DSVFormat;
    PSODesc.SampleDesc.Count = 1;
    // PSODesc.NodeMask;
    // PSODesc.CachedPSO;
    // PSODesc.Flags;
    
    ThrowIfFailed(Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PipelineState)));
    
    
    // Create the command list
    ThrowIfFailed(Device->CreateCommandList(0, 
                                            D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                            CommandAllocators[CurrentFrame].Get(), 
                                            PipelineState.Get(), 
                                            IID_PPV_ARGS(&CommandList)));
    
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(CommandList->Close());
    
#if TRIANGLE_EXAMPLE
    DX12CreateVertexBuffer();
#endif
    
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

win32_offscreen_buffer
DX12InitRenderer(HWND Window, win32_window_dimension Dimension)
{
    win32_offscreen_buffer Buffer = {};
    
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
    SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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
    
#if !TRIANGLE_EXAMPLE
    // Setup our software backbuffer
    D3D12_HEAP_PROPERTIES UploadHeapProperties = {};
    UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC BackBufferDesc = {};
    BackBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    //BackBufferDesc.Alignment = 0;
    BackBufferDesc.Width = SwapChainDesc.Width;
    BackBufferDesc.Height = SwapChainDesc.Height;
    BackBufferDesc.DepthOrArraySize = 1;
    BackBufferDesc.MipLevels = 1;
    BackBufferDesc.Format = SwapChainDesc.Format;
    BackBufferDesc.SampleDesc.Count = SwapChainDesc.SampleDesc.Count;
    BackBufferDesc.SampleDesc.Quality = SwapChainDesc.SampleDesc.Quality;
    BackBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    BackBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout;
    Device->GetCopyableFootprints(&BackBufferDesc, 0, 1, 0, &Layout, nullptr, nullptr, nullptr);
    BackBufferFootPrint = Layout.Footprint;
    
    Buffer.Width = (uint32_t)BackBufferDesc.Width;
    Buffer.Height = BackBufferDesc.Height;
    Buffer.Pitch = Layout.Footprint.RowPitch;
    Buffer.BytesPerPixel = Buffer.Pitch / Buffer.Width;
    
    BackBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    BackBufferDesc.Width = Layout.Footprint.RowPitch * Layout.Footprint.Height;
    BackBufferDesc.Height = 1;
    BackBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    ThrowIfFailed(Device->CreateCommittedResource(&UploadHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &BackBufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr,
                                                  IID_PPV_ARGS(&BackBuffer)));
    
    D3D12_RANGE ReadRange = {0, 0}; // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(BackBuffer->Map(0, &ReadRange, &Buffer.Memory));
    
#endif
    
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = (float)Dimension.Width;
    Viewport.Height = (float)Dimension.Height;
    Viewport.MinDepth = D3D12_MIN_DEPTH;
    Viewport.MaxDepth = D3D12_MAX_DEPTH;
    
    ScissorRect.left = 0;
    ScissorRect.top = 0;
    ScissorRect.right = Dimension.Width;
    ScissorRect.bottom = Dimension.Height;
    
    
    // Create descriptor heaps.
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.NumDescriptors = FrameCount;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(Device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&RTVHeap)));
    
    
    // Create frame resources.
    uint32_t RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
    // Create a RTV for each frame.
    for (uint32_t n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(SwapChain->GetBuffer(n, IID_PPV_ARGS(&RenderTargets[n])));
        Device->CreateRenderTargetView(RenderTargets[n].Get(), nullptr, RTVHandle);
        RTVHandle.ptr += RTVDescriptorSize;
        
        ThrowIfFailed(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocators[n])));
    }
    
    DX12LoadAssets();
    
    return Buffer;
}

void
DX12Render(win32_offscreen_buffer* Buffer)
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(CommandAllocators[CurrentFrame]->Reset());
    
    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(CommandList->Reset(CommandAllocators[CurrentFrame].Get(), PipelineState.Get()));
    
    // Set necessary state.
    CommandList->SetGraphicsRootSignature(RootSignature.Get());
    CommandList->RSSetViewports(1, &Viewport);
    CommandList->RSSetScissorRects(1, &ScissorRect);
    
    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER Barrier = {};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    Barrier.Transition.pResource = RenderTargets[CurrentFrame].Get();
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
#if TRIANGLE_EXAMPLE
    CommandList->ResourceBarrier(1, &Barrier);
    
    uint32_t RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
    RTVHandle.ptr += CurrentFrame * RTVDescriptorSize;
    
    CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);
    
    // Record commands.
    float ClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
    CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    CommandList->DrawInstanced(3, 1, 0, 0);
    
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
#else
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    CommandList->ResourceBarrier(1, &Barrier);
    
    BackBuffer->Unmap(0, nullptr);
    
    D3D12_TEXTURE_COPY_LOCATION DestLocation = {};
    DestLocation.pResource = RenderTargets[CurrentFrame].Get();
    DestLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    DestLocation.SubresourceIndex = 0;
    
    D3D12_TEXTURE_COPY_LOCATION SrcLocation = {};
    SrcLocation.pResource = BackBuffer.Get();
    SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    SrcLocation.PlacedFootprint.Offset = 0;
    SrcLocation.PlacedFootprint.Footprint = BackBufferFootPrint;
    
    CommandList->CopyTextureRegion(&DestLocation, 0,0,0,
                                   &SrcLocation, nullptr);
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
#endif
    
    // Indicate that the back buffer will now be used to present.
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    CommandList->ResourceBarrier(1, &Barrier);
    
    ThrowIfFailed(CommandList->Close());
    
    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    
#if !TRIANGLE_EXAMPLE
    D3D12_RANGE ReadRange = {0, 0}; // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(BackBuffer->Map(0, &ReadRange, &Buffer->Memory));
#endif
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