#include "dx12_renderer.h"

#include "sablujo.h"
#include "dx12_raytracing.h"
#include "backends/imgui_impl_dx12.h"
#include <DXGIDebug.h>

global_variable dx12_renderer* Renderer;

inline void
ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        Assert(false);
    }
}

inline void
ThrowIfFailed(HRESULT hr, ID3DBlob* error)
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
void DX12WaitForCommandContext(command_context* CommandContext)
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(Renderer->CommandQueue->Signal(CommandContext->Synchronization.Fence.Get(), 
                                                 CommandContext->Synchronization.FenceValues[Renderer->CurrentFrame]));
    
    // Wait until the fence has been processed.
    ThrowIfFailed(CommandContext->Synchronization.Fence->SetEventOnCompletion(CommandContext->Synchronization.FenceValues[Renderer->CurrentFrame], 
                                                                              CommandContext->Synchronization.FenceEvent));
    WaitForSingleObjectEx(CommandContext->Synchronization.FenceEvent, INFINITE, FALSE);
    
    // Increment the fence value for the current frame.
    CommandContext->Synchronization.FenceValues[Renderer->CurrentFrame]++;
}

void InitCommandContextSynchronization(command_context* CommandContext)
{
    ThrowIfFailed(Renderer->Device->CreateFence(CommandContext->Synchronization.FenceValues[Renderer->CurrentFrame], 
                                                D3D12_FENCE_FLAG_NONE, 
                                                IID_PPV_ARGS(&CommandContext->Synchronization.Fence)));
    CommandContext->Synchronization.FenceValues[Renderer->CurrentFrame]++;
    
    // Create an event handle to use for frame synchronization.
    CommandContext->Synchronization.FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (CommandContext->Synchronization.FenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    
    // Wait for the command list to execute; we are reusing the same command 
    // list in our main loop but for now, we just want to wait for setup to 
    // complete before continuing.
    DX12WaitForCommandContext(CommandContext);
}

handle<mesh>
DX12CreateVertexBuffer(float* Vertices, 
                       uint32_t* Indices, 
                       uint32_t VertexSize,
                       uint32_t VerticesCount,
                       uint32_t IndicesCount)
{
    handle<vertex_buffer> VBHandle = AllocatePoolInstance(&Renderer->VertexBuffers);
    handle<index_buffer> IBHandle = AllocatePoolInstance(&Renderer->IndexBuffers);
    handle<mesh> Result = AllocatePoolInstance(&Renderer->Meshes);
    mesh* Mesh = GetPoolInstance(&Renderer->Meshes, Result);
    Mesh->VertexBuffer = VBHandle;
    Mesh->IndexBuffer = IBHandle;
    
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
    
    dx12_vertex_buffer* VertexBuffer = GetPoolInstance(&Renderer->VertexBuffers, VBHandle);
    dx12_index_buffer* IndexBuffer = GetPoolInstance(&Renderer->IndexBuffers, IBHandle);
    
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&UploadHeapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &VertexBufferDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&VertexBuffer->StagingBuffer)));
    
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&UploadHeapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &IndexBufferDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&IndexBuffer->StagingBuffer)));
    
    D3D12_HEAP_PROPERTIES DefaultHeapProperties = {};
    DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&DefaultHeapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &VertexBufferDesc,
                                                            D3D12_RESOURCE_STATE_COPY_DEST,
                                                            nullptr,
                                                            IID_PPV_ARGS(&VertexBuffer->MainBuffer)));
    
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&DefaultHeapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &IndexBufferDesc,
                                                            D3D12_RESOURCE_STATE_COPY_DEST,
                                                            nullptr,
                                                            IID_PPV_ARGS(&IndexBuffer->MainBuffer)));
    
    
    uint8_t* VertexDataBegin;
    D3D12_RANGE ReadRange = {0, 0}; // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(VertexBuffer->StagingBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin)));
    memcpy(VertexDataBegin, Vertices, VertexBufferSize);
    VertexBuffer->StagingBuffer->Unmap(0, nullptr);
    
    Renderer->LoadingCommandContext.CommandList->CopyBufferRegion(VertexBuffer->MainBuffer.Get(), 0, 
                                                                  VertexBuffer->StagingBuffer.Get(), 0, 
                                                                  VertexBufferSize);
    
    // Copy the data to the index buffer.
    uint8_t* IndexDataBegin;
    ThrowIfFailed(IndexBuffer->StagingBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&IndexDataBegin)));
    memcpy(IndexDataBegin, Indices, IndexBufferSize);
    IndexBuffer->StagingBuffer->Unmap(0, nullptr);
    
    Renderer->LoadingCommandContext.CommandList->CopyBufferRegion(IndexBuffer->MainBuffer.Get(), 0, 
                                                                  IndexBuffer->StagingBuffer.Get(), 0, 
                                                                  IndexBufferSize);
    
    // Initialize the vertex buffer view.
    VertexBuffer->BufferView.BufferLocation = VertexBuffer->MainBuffer->GetGPUVirtualAddress();
    VertexBuffer->BufferView.StrideInBytes = VertexSize;
    VertexBuffer->BufferView.SizeInBytes = VertexBufferSize;
    VertexBuffer->VertexCount = VerticesCount;
    
    // Initialize the index buffer view.
    IndexBuffer->BufferView.BufferLocation = IndexBuffer->MainBuffer->GetGPUVirtualAddress();
    IndexBuffer->BufferView.Format = DXGI_FORMAT_R32_UINT;
    IndexBuffer->BufferView.SizeInBytes = IndexBufferSize;
    IndexBuffer->IndexCount = IndicesCount;
    
    return Result;
}

handle<const_buffer> DX12CreateConstBuffer(uint32_t Size)
{
    handle<const_buffer> BufferHandle = AllocatePoolInstance(&Renderer->ConstBuffers);
    dx12_const_buffer* ConstBuffer = GetPoolInstance(&Renderer->ConstBuffers, BufferHandle);
    ConstBuffer->Size = Size;
    
    D3D12_HEAP_PROPERTIES UploadHeapProperties = {};
    UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    /*
    // Specifies the default heap. This heap type experiences the most bandwidth for
    // the GPU, but cannot provide CPU access.
    static const D3D12_HEAP_PROPERTIES kDefaultHeapProps = {
        D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};
    */
    
    uint32_t AllocatedSize = MAX(Size, 256);
    Assert(AllocatedSize % 256 == 0);
    
    D3D12_RESOURCE_DESC BufferDesc = {};
    BufferDesc.Alignment = 0;
    BufferDesc.DepthOrArraySize = 1;
    BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    BufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    BufferDesc.Height = 1;
    BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    BufferDesc.MipLevels = 1;
    BufferDesc.SampleDesc.Count = 1;
    BufferDesc.SampleDesc.Quality = 0;
    BufferDesc.Width = AllocatedSize;
    
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&UploadHeapProperties, 
                                                            D3D12_HEAP_FLAG_NONE, 
                                                            &BufferDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ, 
                                                            nullptr, 
                                                            IID_PPV_ARGS(&ConstBuffer->MainBuffer)));
    
    D3D12_DESCRIPTOR_HEAP_DESC HeapDescription = {};
    HeapDescription.NumDescriptors = 1;
    HeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    HeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    ThrowIfFailed(Renderer->Device->CreateDescriptorHeap(&HeapDescription, IID_PPV_ARGS(&ConstBuffer->Heap)));
    
    // Describe and create the constant buffer view. 
    D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDescription = {}; 
    CBVDescription.BufferLocation = ConstBuffer->MainBuffer->GetGPUVirtualAddress(); 
    CBVDescription.SizeInBytes = AllocatedSize; 
    // Get a handle to the heap memory on the CPU side, to be able to write the 
    // descriptors directly 
    D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle = ConstBuffer->Heap->GetCPUDescriptorHandleForHeapStart(); 
    Renderer->Device->CreateConstantBufferView(&CBVDescription, SRVHandle);
    return BufferHandle;
}

void DX12UpdateConstBuffer(handle<const_buffer> BufferHandle, void* Data, uint32_t Size)
{
    dx12_const_buffer* ConstBuffer = GetPoolInstance(&Renderer->ConstBuffers, BufferHandle);
    Assert(Size <= ConstBuffer->Size);
    // Raytracing has to do the contrary of rasterization: rays are defined in 
    // camera space, and are transformed into world space. To do this, we need to 
    // store the inverse matrices as well. 
    
    /*XMVECTOR det; 
    matrices[2] = XMMatrixInverse(&det, matrices[0]); 
    matrices[3] = XMMatrixInverse(&det, matrices[1]); 
*/
    uint8_t *BufferData; 
    ThrowIfFailed(ConstBuffer->MainBuffer->Map(0, nullptr, (void **)&BufferData)); 
    memcpy(BufferData, Data, Size); 
    ConstBuffer->MainBuffer->Unmap(0, nullptr);
}

void
DX12LoadAssets()
{
    // Create a root signature.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
    FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(Renderer->Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
    {
        FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    
    /*
    D3D12_ROOT_PARAMETER1 RootParameters[1];
    RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    RootParameters[0].Constants.ShaderRegister = 0;
    RootParameters[0].Constants.RegisterSpace = 0;
    RootParameters[0].Constants.Num32BitValues = sizeof(XMMATRIX) * 2 / 4;
    RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    */
    
    D3D12_ROOT_PARAMETER1 RootParameters[2];
    RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    RootParameters[0].Constants.ShaderRegister = 0;
    RootParameters[0].Constants.RegisterSpace = 0;
    RootParameters[0].Constants.Num32BitValues = sizeof(XMMATRIX) / 4;
    RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    D3D12_DESCRIPTOR_RANGE1 DescriptorRanges[1];
    DescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    DescriptorRanges[0].NumDescriptors = 1;
    DescriptorRanges[0].BaseShaderRegister = 1;
    DescriptorRanges[0].RegisterSpace = 0;
    DescriptorRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    DescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;
    
    RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    RootParameters[1].DescriptorTable.NumDescriptorRanges = ArrayCount(DescriptorRanges);
    RootParameters[1].DescriptorTable.pDescriptorRanges = DescriptorRanges;
    RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    // Create an empty root signature.
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC VersionedRootSignatureDesc;
    VersionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    VersionedRootSignatureDesc.Desc_1_1.NumParameters = ArrayCount(RootParameters);
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
    
    ComPtr<ID3DBlob> Signature;
    ComPtr<ID3DBlob> Error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&VersionedRootSignatureDesc, 
                                                       &Signature, 
                                                       &Error));
    ThrowIfFailed(Renderer->Device->CreateRootSignature(0, 
                                                        Signature->GetBufferPointer(), 
                                                        Signature->GetBufferSize(), 
                                                        IID_PPV_ARGS(&Renderer->RootSignature)));
    
    
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
    PSODesc.pRootSignature = Renderer->RootSignature.Get();
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
    PSODesc.RasterizerState.FrontCounterClockwise = TRUE;
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
    
    ThrowIfFailed(Renderer->Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&Renderer->PipelineState)));
    
    
    // Create the command list
    // Main command list is created in a closed state
    ThrowIfFailed(Renderer->Device->CreateCommandList1(0, 
                                                       D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                                       D3D12_COMMAND_LIST_FLAG_NONE,
                                                       IID_PPV_ARGS(&Renderer->MainCommandContext.CommandList)));
    // Loading command list is created in an opened state 
    // since we expect loads to happen on first frames.
    ThrowIfFailed(Renderer->Device->CreateCommandList(0, 
                                                      D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                                      Renderer->LoadingCommandContext.CommandAllocators[Renderer->CurrentFrame].Get(), 
                                                      Renderer->PipelineState.Get(),
                                                      IID_PPV_ARGS(&Renderer->LoadingCommandContext.CommandList)));
    
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    InitCommandContextSynchronization(&Renderer->MainCommandContext);
    InitCommandContextSynchronization(&Renderer->LoadingCommandContext);
}

void
DX12InitRenderer(HWND Window, renderer_config* Config, memory_area* RendererArea)
{
    Renderer = Allocate<dx12_renderer>(RendererArea);
    Renderer->MemArea = RendererArea;
    Renderer->SupportedConfig = {};
    Renderer->SupportedConfig.VSync = true;
    Renderer->SupportedConfig.UseImGUI = true;
    Renderer->Raytracing = Allocate<raytracing_data>(RendererArea);
    
    Renderer->Meshes = AllocatePool<mesh, mesh>(RendererArea, 32);
    Renderer->VertexBuffers = AllocatePool<dx12_vertex_buffer, vertex_buffer>(RendererArea, 32);
    Renderer->IndexBuffers = AllocatePool<dx12_index_buffer, index_buffer>(RendererArea, 32);
    Renderer->ConstBuffers = AllocatePool<dx12_const_buffer, const_buffer>(RendererArea, 32);
    
    uint32_t DXGIFactoryFlags = 0;
#if SABLUJO_INTERNAL
    ComPtr<ID3D12Debug> DebugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
    {
        DebugController->EnableDebugLayer();
        
        ComPtr<ID3D12Debug1> DebugController1;
        DebugController->QueryInterface(IID_PPV_ARGS(&DebugController1));
        DebugController1->SetEnableGPUBasedValidation(true);
    }
    ComPtr<IDXGIInfoQueue> InfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&InfoQueue))))
    {
        DXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        InfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        InfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
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
        
        // Check to see whether the adapter supports Direct3D 12
        if (SUCCEEDED(D3D12CreateDevice(HardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&Renderer->Device))))
        {
            break;
        }
    }
    
    // Check Raytracing support;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options5 = {}; 
    if(SUCCEEDED(Renderer->Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Options5, sizeof(Options5))))
    {
        Renderer->SupportedConfig.UseRaytracing = Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
    }
    else
    {
        Renderer->SupportedConfig.UseRaytracing = false;
    }
    
    if(Config->UseRaytracing)
    {
        Config->UseRaytracing = Renderer->SupportedConfig.UseRaytracing;
    }
    
    
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    QueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(Renderer->Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&Renderer->CommandQueue)));
    
    ComPtr<IDXGISwapChain1> NewSwapChain;
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Width = Config->OutputDimensions.Width;
    SwapChainDesc.Height = Config->OutputDimensions.Height;
    SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    //SwapChainDesc.Stero = FALSE;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = Renderer->FrameCount;
    //SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    //SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    SwapChainDesc.Flags = 0;
    
    // Check tearing support
    {
        // Rather than create the DXGI 1.5 factory interface directly, we create the
        // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
        // graphics debugging tools which will not support the 1.5 factory interface 
        // until a future update.
        ComPtr<IDXGIFactory6> Factory6;
        HRESULT HR = CreateDXGIFactory1(IID_PPV_ARGS(&Factory6));
        if (SUCCEEDED(HR))
        {
            HR = Factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
                                               &Renderer->SupportedConfig.AllowTearing, 
                                               sizeof(Renderer->SupportedConfig.AllowTearing));
        }
        if (Config->AllowTearing)
        {
            Config->AllowTearing = Renderer->SupportedConfig.AllowTearing;
        }
        if (Config->AllowTearing)
        {
            Config->VSync = false;
            SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
    }
    
    ThrowIfFailed(Factory->CreateSwapChainForHwnd(Renderer->CommandQueue.Get(), 
                                                  Window, 
                                                  &SwapChainDesc, 
                                                  nullptr, 
                                                  nullptr, 
                                                  &NewSwapChain));
    
    ThrowIfFailed(NewSwapChain.As(&Renderer->SwapChain));
    Renderer->CurrentFrame = Renderer->SwapChain->GetCurrentBackBufferIndex();
    
    if (Renderer->SupportedConfig.UseRaytracing)
    {
        
        // Create the raytracing pipeline, associating the shader code to symbol names
        // and to their root signatures, and defining the amount of memory carried by
        // rays (ray payload)
        CreateRaytracingPipeline(Renderer); // #DXR
        
        // Allocate the buffer storing the raytracing output, with the same dimensions
        // as the target image
        CreateRaytracingOutputBuffer(Renderer, Config->OutputDimensions); // #DXR
    }
    
    Renderer->Viewport.TopLeftX = 0;
    Renderer->Viewport.TopLeftY = 0;
    Renderer->Viewport.Width = (float)Config->OutputDimensions.Width;
    Renderer->Viewport.Height = (float)Config->OutputDimensions.Height;
    Renderer->Viewport.MinDepth = D3D12_MIN_DEPTH;
    Renderer->Viewport.MaxDepth = D3D12_MAX_DEPTH;
    
    Renderer->ScissorRect.left = 0;
    Renderer->ScissorRect.top = 0;
    Renderer->ScissorRect.right = Config->OutputDimensions.Width;
    Renderer->ScissorRect.bottom = Config->OutputDimensions.Height;
    
    
    // Create descriptor heaps.
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.NumDescriptors = Renderer->FrameCount;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(Renderer->Device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&Renderer->RTVHeap)));
    
    
    // Create frame resources.
    uint32_t RTVDescriptorSize = Renderer->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = Renderer->RTVHeap->GetCPUDescriptorHandleForHeapStart();
    // Create a RTV for each frame.
    for (uint32_t n = 0; n < Renderer->FrameCount; n++)
    {
        ThrowIfFailed(Renderer->SwapChain->GetBuffer(n, IID_PPV_ARGS(&Renderer->RenderTargets[n])));
        Renderer->Device->CreateRenderTargetView(Renderer->RenderTargets[n].Get(), nullptr, RTVHandle);
        RTVHandle.ptr += RTVDescriptorSize;
        
        ThrowIfFailed(Renderer->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Renderer->MainCommandContext.CommandAllocators[n])));
        ThrowIfFailed(Renderer->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Renderer->LoadingCommandContext.CommandAllocators[n])));
    }
    
    if (Config->UseImGUI)
    {
        // ImGui Setup
        /*
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = FrameCount;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;
            ThrowIfFailed(Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ImGuiRtvDescHeap)));
            
            SIZE_T rtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = ImGuiRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < FrameCount; i++)
            {
                ImGuiRenderTargetDescriptors[i] = rtvHandle;
                rtvHandle.ptr += rtvDescriptorSize;
            }
        }
        */
        
        // Create the command list in a closed state
        ThrowIfFailed(Renderer->Device->CreateCommandList1(0, 
                                                           D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                                           D3D12_COMMAND_LIST_FLAG_NONE,
                                                           IID_PPV_ARGS(&Renderer->ImGuiCommandContext.CommandList)));
        
        for(uint32_t n = 0; n < Renderer->FrameCount; ++n)
        {
            ThrowIfFailed(Renderer->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Renderer->ImGuiCommandContext.CommandAllocators[n])));
        }
        InitCommandContextSynchronization(&Renderer->ImGuiCommandContext);
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(Renderer->Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&Renderer->ImGuiSrvDescHeap)));
        }
        /*
        for (UINT i = 0; i < FrameCount; i++)
        {
            SwapChain->GetBuffer(i, IID_PPV_ARGS(&ImGuiRenderTargetResources[i]));
            Device->CreateRenderTargetView(ImGuiRenderTargetResources[i].Get(), NULL, ImGuiRenderTargetDescriptors[i]);
        }
        */
        ImGui_ImplDX12_Init(Renderer->Device.Get(), Renderer->FrameCount,
                            DXGI_FORMAT_B8G8R8A8_UNORM, Renderer->ImGuiSrvDescHeap.Get(),
                            Renderer->ImGuiSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
                            Renderer->ImGuiSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
    }
    DX12LoadAssets();
    Renderer->MVPBuffer = DX12CreateConstBuffer(sizeof(XMMATRIX) * 3);
}

void 
DX12SetViewProjection(float* View, float* Projection)
{
    XMMATRIX ProjectionMat = XMMATRIX(Projection);
    XMMATRIX Matrices[3];
    Matrices[0] = XMMATRIX(View);
    Matrices[1] = XMMatrixInverse(nullptr, Matrices[0]);
    Matrices[2] = XMMatrixInverse(nullptr, ProjectionMat);
    Matrices[0] = Matrices[0] * ProjectionMat;
    DX12UpdateConstBuffer(Renderer->MVPBuffer, Matrices, sizeof(float) * 48);
    //Renderer->CurrentViewProjection = XMMATRIX(ViewProjection);
}

void
DX12SubmitForRender(mesh_instance MeshInstance)
{
    Renderer->RenderList.push(MeshInstance);
}

void
DX12Render(renderer_config Config)
{
    ThrowIfFailed(Renderer->LoadingCommandContext.CommandList->Close());
    
    // Execute the loading command list.
    ID3D12CommandList* ppLoadingCommandLists[] = { Renderer->LoadingCommandContext.CommandList.Get() };
    Renderer->CommandQueue->ExecuteCommandLists(_countof(ppLoadingCommandLists), ppLoadingCommandLists);
    
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(Renderer->MainCommandContext.CommandAllocators[Renderer->CurrentFrame]->Reset());
    
    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(Renderer->MainCommandContext.CommandList->Reset(Renderer->MainCommandContext.CommandAllocators[Renderer->CurrentFrame].Get(), 
                                                                  Renderer->PipelineState.Get()));
    
    ComPtr<ID3D12GraphicsCommandList4> GraphicsCommandList4;
    ThrowIfFailed(Renderer->MainCommandContext.CommandList.As(&GraphicsCommandList4));
    
    if(Config.UseRaytracing)
    {
        if(!Renderer->Raytracing->BottomLevelAS)
        {
            // Setup the acceleration structures (AS) for raytracing. When setting up 
            // geometry, each bottom-level AS has its own transform matrix. 
            CreateAccelerationStructures(Renderer); 
            
            // Command lists are created in the recording state, but there is 
            // nothing to record yet. The main loop expects it to be closed, so 
            // close it now. 
            //ThrowIfFailed(CommandList->Close());
            
            // Create the buffer containing the raytracing result (always output in a
            // UAV), and create the heap referencing the resources used by the raytracing,
            // such as the acceleration structure
            CreateShaderResourceHeap(Renderer); // #DXR
            
            // Create the shader binding table and indicating which shaders
            // are invoked for each instance in the AS
            CreateShaderBindingTable(Renderer);
        }
        
        // Bind the raytracing pipeline
        GraphicsCommandList4->SetPipelineState1(Renderer->Raytracing->StateObject.Get());
    }
    else
    {
        // Set necessary state.
        Renderer->MainCommandContext.CommandList->SetGraphicsRootSignature(Renderer->RootSignature.Get());
    }
    
    Renderer->MainCommandContext.CommandList->RSSetViewports(1, &Renderer->Viewport);
    Renderer->MainCommandContext.CommandList->RSSetScissorRects(1, &Renderer->ScissorRect);
    
    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER Barrier = {};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    Barrier.Transition.pResource = Renderer->RenderTargets[Renderer->CurrentFrame].Get();
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
    
    uint32_t RTVDescriptorSize = Renderer->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = Renderer->RTVHeap->GetCPUDescriptorHandleForHeapStart();
    RTVHandle.ptr += Renderer->CurrentFrame * RTVDescriptorSize;
    
    Renderer->MainCommandContext.CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);
    
    // Record commands
    if(Config.UseRaytracing && Renderer->SupportedConfig.UseRaytracing)
    {
        // #DXR
        // Bind the descriptor heap giving access to the top-level acceleration
        // structure, as well as the raytracing output
        std::vector<ID3D12DescriptorHeap*> Heaps = {Renderer->Raytracing->SrvUavHeap.Get()};
        Renderer->MainCommandContext.CommandList->SetDescriptorHeaps((uint32_t)Heaps.size(), Heaps.data());
        
        // On the last frame, the raytracing output was used as a copy source, to
        // copy its contents into the render target. Now we need to transition it to
        // a UAV so that the shaders can write in it.
        Barrier.Transition.pResource = Renderer->Raytracing->OutputResource.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
        
        // Setup the raytracing task
        D3D12_DISPATCH_RAYS_DESC Desc = {};
        // The layout of the SBT is as follows: ray generation shader, miss
        // shaders, hit groups. As described in the CreateShaderBindingTable method,
        // all SBT entries of a given type have the same size to allow a fixed stride.
        // The ray generation shaders are always at the beginning of the SBT.
        
        uint32_t RayGenerationSectionSizeInBytes = Renderer->Raytracing->SbtHelper.GetRayGenSectionSize();
        Desc.RayGenerationShaderRecord.StartAddress = Renderer->Raytracing->SbtStorage->GetGPUVirtualAddress();
        Desc.RayGenerationShaderRecord.SizeInBytes = RayGenerationSectionSizeInBytes;
        
        // The miss shaders are in the second SBT section, right after the ray
        // generation shader. We have one miss shader for the camera rays and one
        // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
        // also indicate the stride between the two miss shaders, which is the size
        // of a SBT entry
        uint32_t MissSectionSizeInBytes = Renderer->Raytracing->SbtHelper.GetMissSectionSize();
        Desc.MissShaderTable.StartAddress = Renderer->Raytracing->SbtStorage->GetGPUVirtualAddress() + RayGenerationSectionSizeInBytes;
        Desc.MissShaderTable.SizeInBytes = MissSectionSizeInBytes;
        Desc.MissShaderTable.StrideInBytes = Renderer->Raytracing->SbtHelper.GetMissEntrySize();
        
        // The hit groups section start after the miss shaders. In this sample we
        // have one 1 hit group for the triangle
        uint32_t HitGroupsSectionSize = Renderer->Raytracing->SbtHelper.GetHitGroupSectionSize();
        Desc.HitGroupTable.StartAddress = Renderer->Raytracing->SbtStorage->GetGPUVirtualAddress() + RayGenerationSectionSizeInBytes + MissSectionSizeInBytes;
        Desc.HitGroupTable.SizeInBytes = HitGroupsSectionSize;
        Desc.HitGroupTable.StrideInBytes = Renderer->Raytracing->SbtHelper.GetHitGroupEntrySize();
        
        // Dimensions of the image to render, identical to a kernel launch dimension
        //TODO: Get the window dimensions here
        Desc.Width = 1280;
        Desc.Height = 720;
        Desc.Depth = 1;
        
        // Dispatch the rays and write to the raytracing output
        GraphicsCommandList4->DispatchRays(&Desc);
        
        // The raytracing output needs to be copied to the actual render target used
        // for display. For this, we need to transition the raytracing output from a
        // UAV to a copy source, and the render target buffer to a copy destination.
        // We can then do the actual copy, before transitioning the render target
        // buffer into a render target, that will be then used to display the image
        Barrier.Transition.pResource = Renderer->Raytracing->OutputResource.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
        
        Barrier.Transition.pResource = Renderer->RenderTargets[Renderer->CurrentFrame].Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
        
        Renderer->MainCommandContext.CommandList->CopyResource(Renderer->RenderTargets[Renderer->CurrentFrame].Get(), 
                                                               Renderer->Raytracing->OutputResource.Get());
        
        Barrier.Transition.pResource = Renderer->RenderTargets[Renderer->CurrentFrame].Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
        
        Renderer->RenderList = std::queue<mesh_instance>();
    }
    else
    {
        float ClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        Renderer->MainCommandContext.CommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
        
        // Set Camera
        //Renderer->MainCommandContext.CommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &Renderer->CurrentViewProjection, sizeof(XMMATRIX) / 4);
        // #DXR Extra: Perspective Camera 
        dx12_const_buffer* MVPBuffer = GetPoolInstance(&Renderer->ConstBuffers, Renderer->MVPBuffer);
        std::vector<ID3D12DescriptorHeap*> Heaps = { MVPBuffer->Heap.Get() }; 
        Renderer->MainCommandContext.CommandList->SetDescriptorHeaps((uint32_t)Heaps.size(), Heaps.data()); 
        // set the root descriptor table 0 to the constant buffer descriptor heap 
        Renderer->MainCommandContext.CommandList->SetGraphicsRootDescriptorTable(1, MVPBuffer->Heap->GetGPUDescriptorHandleForHeapStart());
        
        
        Renderer->MainCommandContext.CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        while(!Renderer->RenderList.empty())
        {
            mesh_instance MeshInstance = Renderer->RenderList.front();
            Renderer->MainCommandContext.CommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &MeshInstance.Transform, 0);
            mesh* Mesh = GetPoolInstance(&Renderer->Meshes, MeshInstance.Mesh);
            dx12_vertex_buffer* VertexBuffer = GetPoolInstance(&Renderer->VertexBuffers, Mesh->VertexBuffer);
            dx12_index_buffer* IndexBuffer = GetPoolInstance(&Renderer->IndexBuffers, Mesh->IndexBuffer);
            Renderer->MainCommandContext.CommandList->IASetVertexBuffers(0, 1, &VertexBuffer->BufferView);
            Renderer->MainCommandContext.CommandList->IASetIndexBuffer(&IndexBuffer->BufferView);
            Renderer->MainCommandContext.CommandList->DrawIndexedInstanced(IndexBuffer->IndexCount, 1, 0, 0, 0);
            Renderer->RenderList.pop();
        }
    }
    
    
    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // Indicate that the back buffer will now be used to present.
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    if(!Config.UseImGUI)
    {
        Renderer->MainCommandContext.CommandList->ResourceBarrier(1, &Barrier);
    }
    
    ThrowIfFailed(Renderer->MainCommandContext.CommandList->Close());
    // Execute the command list.
    std::vector<ID3D12CommandList*> CommandLists = {{ Renderer->MainCommandContext.CommandList.Get() }};
    if (Config.UseImGUI)
    {
        ThrowIfFailed(Renderer->ImGuiCommandContext.CommandList->Reset(Renderer->ImGuiCommandContext.CommandAllocators[Renderer->CurrentFrame].Get(), nullptr));
        /*
float ClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        ImGuiCommandList->ClearRenderTargetView(RTVHandle, ClearColor, 0, nullptr);
        uint32_t RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
        RTVHandle.ptr += CurrentFrame * RTVDescriptorSize;
*/
        Renderer->ImGuiCommandContext.CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);
        std::vector<ID3D12DescriptorHeap*> Heaps = {Renderer->ImGuiSrvDescHeap.Get()};
        Renderer->ImGuiCommandContext.CommandList->SetDescriptorHeaps((uint32_t)Heaps.size(), Heaps.data());
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Renderer->ImGuiCommandContext.CommandList.Get());
        
        Renderer->ImGuiCommandContext.CommandList->ResourceBarrier(1, &Barrier);
        ThrowIfFailed(Renderer->ImGuiCommandContext.CommandList->Close());
        CommandLists.push_back(Renderer->ImGuiCommandContext.CommandList.Get());
    }
    
    
    Renderer->CommandQueue->ExecuteCommandLists((uint32_t)CommandLists.size(), CommandLists.data());
}

void
DX12Present(renderer_config Config)
{
    uint32_t SyncInterval = Config.VSync ? 1 : 0;
    uint32_t Flags = 0;
    if(Config.AllowTearing && !Config.VSync && Renderer->SupportedConfig.AllowTearing)
    {
        Flags |= DXGI_PRESENT_ALLOW_TEARING;
    }
    
    ThrowIfFailed(Renderer->SwapChain->Present(SyncInterval, Flags));
    
    // Schedule a Signal command in the queue.
    uint64_t CurrentFenceValue = Renderer->MainCommandContext.Synchronization.FenceValues[Renderer->CurrentFrame];
    ThrowIfFailed(Renderer->CommandQueue->Signal(Renderer->MainCommandContext.Synchronization.Fence.Get(), CurrentFenceValue));
    
    // Update the frame index.
    Renderer->CurrentFrame = Renderer->SwapChain->GetCurrentBackBufferIndex();
    
    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (Renderer->MainCommandContext.Synchronization.Fence->GetCompletedValue() < Renderer->MainCommandContext.Synchronization.FenceValues[Renderer->CurrentFrame])
    {
        ThrowIfFailed(Renderer->MainCommandContext.Synchronization.Fence->SetEventOnCompletion(Renderer->MainCommandContext.Synchronization.FenceValues[Renderer->CurrentFrame], 
                                                                                               Renderer->MainCommandContext.Synchronization.FenceEvent));
        WaitForSingleObjectEx(Renderer->MainCommandContext.Synchronization.FenceEvent, INFINITE, FALSE);
    }
    
    // Set the fence value for the next frame.
    Renderer->MainCommandContext.Synchronization.FenceValues[Renderer->CurrentFrame] = CurrentFenceValue + 1;
    
    // Wait and reset loading commands
    DX12WaitForCommandContext(&Renderer->LoadingCommandContext);
    ThrowIfFailed(Renderer->LoadingCommandContext.CommandAllocators[Renderer->CurrentFrame]->Reset());
    ThrowIfFailed(Renderer->LoadingCommandContext.CommandList->Reset(Renderer->LoadingCommandContext.CommandAllocators[Renderer->CurrentFrame].Get(), 
                                                                     Renderer->PipelineState.Get()));
}

void
DX12ShutdownRenderer(renderer_config Config)
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    DX12WaitForCommandContext(&Renderer->LoadingCommandContext);
    CloseHandle(Renderer->LoadingCommandContext.Synchronization.FenceEvent);
    DX12WaitForCommandContext(&Renderer->MainCommandContext);
    CloseHandle(Renderer->MainCommandContext.Synchronization.FenceEvent);
    if (Config.UseImGUI)
    {
        DX12WaitForCommandContext(&Renderer->ImGuiCommandContext);
        CloseHandle(Renderer->ImGuiCommandContext.Synchronization.FenceEvent);
        ImGui_ImplDX12_Shutdown();
    }
    
    Renderer->~dx12_renderer();
    Renderer = nullptr;
#if SABLUJO_INTERNAL
    {
        ComPtr<IDXGIDebug1> DXGIDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&DXGIDebug))))
        {
            DXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, 
                                         DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif
}
