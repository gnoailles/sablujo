#include "dx12_renderer.h"

#include "sablujo.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <D3DCompiler.h>
#include <vector>
#include <queue>

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

#include <directxmath.h>
using namespace DirectX;
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normals;
};

global_variable dx12_present_synchronization Synchronization;
global_variable ComPtr<IDXGISwapChain4> SwapChain;
global_variable ComPtr<ID3D12CommandQueue> CommandQueue;
global_variable ComPtr<ID3D12Device8> Device;
global_variable ComPtr<ID3D12DescriptorHeap> RTVHeap;
global_variable ComPtr<ID3D12Resource> RenderTargets[FrameCount];
global_variable ComPtr<ID3D12CommandAllocator> CommandAllocators[FrameCount][2];


global_variable ComPtr<ID3D12RootSignature> RootSignature;
global_variable ComPtr<ID3D12PipelineState> PipelineState;
global_variable ComPtr<ID3D12GraphicsCommandList> CommandList;
global_variable ComPtr<ID3D12GraphicsCommandList> LoadingCommandList;

global_variable std::queue<mesh_handle> RenderList;

global_variable D3D12_VIEWPORT Viewport;
global_variable D3D12_RECT ScissorRect;

global_variable XMMATRIX CurrentViewProjection;

struct vertex_buffer
{
    vertex_buffer() : MainBuffer{}, StagingBuffer{}, BufferView{} {};
    
    vertex_buffer(vertex_buffer&& Other) : 
    MainBuffer{std::move(Other.MainBuffer)}
    , StagingBuffer{std::move(Other.StagingBuffer)} 
    , BufferView{std::move(Other.BufferView)}
    , VertexCount{Other.VertexCount}
    {};
    
    ComPtr<ID3D12Resource> MainBuffer;
    ComPtr<ID3D12Resource> StagingBuffer;
    D3D12_VERTEX_BUFFER_VIEW BufferView;
    uint32_t VertexCount {0};
};

struct index_buffer
{
    index_buffer() : MainBuffer{}, StagingBuffer{}, BufferView{} {};
    
    index_buffer(index_buffer&& Other) : 
    MainBuffer{std::move(Other.MainBuffer)}
    , StagingBuffer{std::move(Other.StagingBuffer)} 
    , BufferView{std::move(Other.BufferView)}
    , IndexCount{Other.IndexCount}
    {};
    
    ComPtr<ID3D12Resource> MainBuffer;
    ComPtr<ID3D12Resource> StagingBuffer;
    D3D12_INDEX_BUFFER_VIEW BufferView;
    uint32_t IndexCount {0};
};

global_variable std::vector<vertex_buffer> VertexBuffers;
global_variable std::vector<index_buffer> IndexBuffers;

inline void
ThrowIfFailed(HRESULT hr, ID3DBlob* error = nullptr)
{
    if (FAILED(hr))
    {
        if(error)
        {
            OutputDebugStringA((LPCSTR)error->GetBufferPointer());
        }
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

mesh_handle
DX12CreateVertexBuffer(float* Vertices, 
                       uint32_t* Indices, 
                       uint32_t VertexSize,
                       uint32_t VerticesCount,
                       uint32_t IndicesCount)
{
    mesh_handle Result{(uint16_t)((uint8_t)(VertexBuffers.size() << 8) | (uint8_t)IndexBuffers.size())};
    const uint32_t VertexBufferSize = VertexSize * VerticesCount;
    const uint32_t IndexBufferSize = sizeof(uint32_t) * IndicesCount;
    
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
    
    D3D12_RESOURCE_DESC IndexBufferDesc = VertexBufferDesc;
    IndexBufferDesc.Width = IndexBufferSize;
    
    vertex_buffer VertexBuffer;
    index_buffer IndexBuffer;
    ThrowIfFailed(Device->CreateCommittedResource(&UploadHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &VertexBufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr,
                                                  IID_PPV_ARGS(&VertexBuffer.StagingBuffer)));
    
    ThrowIfFailed(Device->CreateCommittedResource(&UploadHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &IndexBufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr,
                                                  IID_PPV_ARGS(&IndexBuffer.StagingBuffer)));
    
    D3D12_HEAP_PROPERTIES DefaultHeapProperties = {};
    DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    ThrowIfFailed(Device->CreateCommittedResource(&DefaultHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &VertexBufferDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(&VertexBuffer.MainBuffer)));
    
    ThrowIfFailed(Device->CreateCommittedResource(&DefaultHeapProperties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &IndexBufferDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(&IndexBuffer.MainBuffer)));
    
    /*
    D3D12_SUBRESOURCE_DATA SubresourceData = {};
    SubresourceData.pData = Vertices;
    SubresourceData.RowPitch = VertexBufferSize;
    SubresourceData.SlicePitch = SubresourceData.RowPitch;
    
    UpdateSubresources(LoadingCommandList.Get(), 
                       VertexBuffer.MainBuffer.Get(),
                       VertexBuffer.StagingBuffer.Get(),
                       0, 0, 1,
                       &SubresourceData);
    */
    
    uint8_t* VertexDataBegin;
    D3D12_RANGE ReadRange = {0, 0}; // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(VertexBuffer.StagingBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin)));
    memcpy(VertexDataBegin, Vertices, VertexBufferSize);
    VertexBuffer.StagingBuffer->Unmap(0, nullptr);
    
    LoadingCommandList->CopyBufferRegion(VertexBuffer.MainBuffer.Get(), 0, 
                                         VertexBuffer.StagingBuffer.Get(), 0, 
                                         VertexBufferSize);
    
    // Copy the data to the index buffer.
    uint8_t* IndexDataBegin;
    ThrowIfFailed(IndexBuffer.StagingBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&IndexDataBegin)));
    memcpy(IndexDataBegin, Indices, IndexBufferSize);
    IndexBuffer.StagingBuffer->Unmap(0, nullptr);
    
    LoadingCommandList->CopyBufferRegion(IndexBuffer.MainBuffer.Get(), 0, 
                                         IndexBuffer.StagingBuffer.Get(), 0, 
                                         IndexBufferSize);
    
    // Initialize the vertex buffer view.
    VertexBuffer.BufferView.BufferLocation = VertexBuffer.MainBuffer->GetGPUVirtualAddress();
    VertexBuffer.BufferView.StrideInBytes = VertexSize;
    VertexBuffer.BufferView.SizeInBytes = VertexBufferSize;
    VertexBuffer.VertexCount = VerticesCount;
    
    // Initialize the index buffer view.
    IndexBuffer.BufferView.BufferLocation = IndexBuffer.MainBuffer->GetGPUVirtualAddress();
    IndexBuffer.BufferView.Format = DXGI_FORMAT_R32_UINT;
    IndexBuffer.BufferView.SizeInBytes = IndexBufferSize;
    IndexBuffer.IndexCount = IndicesCount;
    
    VertexBuffers.emplace_back(std::move(VertexBuffer));
    IndexBuffers.emplace_back(std::move(IndexBuffer));
    return Result;
}

void
DX12LoadAssets()
{
    // Create a root signature.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    D3D12_ROOT_PARAMETER1 RootParameters[1];
    RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    RootParameters[0].Constants.ShaderRegister = 0;
    RootParameters[0].Constants.RegisterSpace = 0;
    RootParameters[0].Constants.Num32BitValues = sizeof(XMMATRIX) * 2 / 4;
    RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    // Create an empty root signature.
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC VersionedRootSignatureDesc;
    VersionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    VersionedRootSignatureDesc.Desc_1_1.NumParameters = 1;
    VersionedRootSignatureDesc.Desc_1_1.pParameters = RootParameters;
    VersionedRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
    VersionedRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
    VersionedRootSignatureDesc.Desc_1_1.Flags = 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        // D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
    
    // A single 32-bit constant root parameter that is used by the vertex shader.
    
    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&VersionedRootSignatureDesc, 
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
    
    const auto CompileResult = D3DCompileFromFile(L"resources/shaders/base_color_mvp.hlsl", 
                                                  nullptr, nullptr, 
                                                  "VS", "vs_5_1", 
                                                  CompileFlags, 0, 
                                                  &VertexShader, &Error);
    ThrowIfFailed(CompileResult, Error.Get());
    
    ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/base_color_mvp.hlsl", 
                                     nullptr, nullptr, 
                                     "FS", "ps_5_1", 
                                     CompileFlags, 0, 
                                     &PixelShader, &Error), Error.Get());
    
    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC InputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
    PSODesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    // PSODesc.DSVFormat;
    PSODesc.SampleDesc.Count = 1;
    // PSODesc.NodeMask;
    // PSODesc.CachedPSO;
    // PSODesc.Flags;
    
    ThrowIfFailed(Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PipelineState)));
    
    
    // Create the command list
    // Main command list is created in a closed state
    ThrowIfFailed(Device->CreateCommandList1(0, 
                                             D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                             D3D12_COMMAND_LIST_FLAG_NONE,
                                             IID_PPV_ARGS(&CommandList)));
    // Loading command list is created in an opened state 
    // since we expect loads to happen on first frames.
    ThrowIfFailed(Device->CreateCommandList(0, 
                                            D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                            CommandAllocators[CurrentFrame][0].Get(), 
                                            PipelineState.Get(),
                                            IID_PPV_ARGS(&LoadingCommandList)));
    
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    //ThrowIfFailed(CommandList->Close());
    
    //DX12CreateVertexBuffer();
    
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
DX12InitRenderer(HWND Window, win32_window_dimension Dimension)
{
    uint32_t DXGIFactoryFlags = 0;
#if SABLUJO_INTERNAL
    ComPtr<ID3D12Debug> DebugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
    {
        DebugController->EnableDebugLayer();
        DXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        
        ComPtr<ID3D12Debug1> DebugController1;
        DebugController->QueryInterface(IID_PPV_ARGS(&DebugController1));
        DebugController1->SetEnableGPUBasedValidation(true);
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
    
#if 0
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
        
        ThrowIfFailed(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocators[n][0])));
        ThrowIfFailed(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocators[n][1])));
    }
    
    VertexBuffers.reserve(255);
    IndexBuffers.reserve(255);
    DX12LoadAssets();
}

void 
DX12SetViewProjection(float* ViewProjection)
{
    CurrentViewProjection = XMMATRIX(ViewProjection);
}

void
DX12SubmitForRender(mesh_handle Mesh)
{
    RenderList.push(Mesh);
}

void
DX12Render()
{
    ThrowIfFailed(LoadingCommandList->Close());
    
    // Execute the loading command list.
    ID3D12CommandList* ppLoadingCommandLists[] = { LoadingCommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(ppLoadingCommandLists), ppLoadingCommandLists);
    ThrowIfFailed(LoadingCommandList->Reset(CommandAllocators[CurrentFrame][0].Get(), PipelineState.Get()));
    
    
    
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(CommandAllocators[CurrentFrame][1]->Reset());
    
    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(CommandList->Reset(CommandAllocators[CurrentFrame][1].Get(), PipelineState.Get()));
    
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
    
    CommandList->ResourceBarrier(1, &Barrier);
    
    uint32_t RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
    RTVHandle.ptr += CurrentFrame * RTVDescriptorSize;
    
    CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);
    
    // Record commands.
    float ClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
    
    // Set Camera
    
    XMMATRIX Model = XMMatrixIdentity();
    //XMMATRIX Model = XMMatrixTranslation(0.0f, 0.0f, -10.0f);
    CommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &Model, 0);
    CommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &CurrentViewProjection, sizeof(XMMATRIX) / 4);
    
    CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    while(!RenderList.empty())
    {
        const mesh_handle MeshHandle = RenderList.front();
        const uint8_t VertexBuffIndex = MeshHandle >> 8;
        const uint8_t IndexBuffIndex = MeshHandle & 0xFF;
        const vertex_buffer& VertexBuffer = VertexBuffers[VertexBuffIndex];
        const index_buffer& IndexBuffer = IndexBuffers[IndexBuffIndex];
        CommandList->IASetVertexBuffers(0, 1, &VertexBuffer.BufferView);
        CommandList->IASetIndexBuffer(&IndexBuffer.BufferView);
        CommandList->DrawIndexedInstanced(IndexBuffer.IndexCount, 1, 0, 0, 0);
        RenderList.pop();
    }
    
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    
    // Indicate that the back buffer will now be used to present.
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    CommandList->ResourceBarrier(1, &Barrier);
    
    ThrowIfFailed(CommandList->Close());
    
    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
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