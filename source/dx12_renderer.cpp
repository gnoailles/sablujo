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
struct vertex
{
    XMFLOAT4 Position;
    XMFLOAT3 Normal;
};

//DXR Helpers
#include <vector>
#include <dxcapi.h>
#include "DXRHelper.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"

// #DXR
struct acceleration_structure_buffers
{ 
    ComPtr<ID3D12Resource> Scratch; // Scratch memory for AS builder 
    ComPtr<ID3D12Resource> Result; // Where the AS is 
    ComPtr<ID3D12Resource> InstanceDesc; // Hold the matrices of the instances
};

struct raytracing_data
{
    ComPtr<ID3D12Resource> BottomLevelAS; // Storage for the bottom Level AS
    nv_helpers_dx12::TopLevelASGenerator TopLevelASGenerator;
    acceleration_structure_buffers TopLevelASBuffers;
    std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> Instances;
    
    ComPtr<IDxcBlob> RayGenLibrary;
    ComPtr<IDxcBlob> HitLibrary;
    ComPtr<IDxcBlob> MissLibrary;
    ComPtr<ID3D12RootSignature> RayGenSignature;
    ComPtr<ID3D12RootSignature> HitSignature;
    ComPtr<ID3D12RootSignature> MissSignature;
    // Ray tracing pipeline state
    ComPtr<ID3D12StateObject> StateObject;
    // Ray tracing pipeline state properties, retaining the shader identifiers
    // to use in the Shader Binding Table
    ComPtr<ID3D12StateObjectProperties> StateObjectProps;
    
    
    ComPtr<ID3D12Resource> OutputResource;
    ComPtr<ID3D12DescriptorHeap> SrvUavHeap;
    
    // Shader Binding Table
    nv_helpers_dx12::ShaderBindingTableGenerator SbtHelper;
    ComPtr<ID3D12Resource> SbtStorage;
};

global_variable raytracing_data Raytracing;

global_variable dx12_present_synchronization Synchronization[2];
global_variable ComPtr<IDXGISwapChain4> SwapChain;
global_variable ComPtr<ID3D12CommandQueue> CommandQueue;
global_variable ComPtr<ID3D12Device8> Device;
global_variable ComPtr<ID3D12DescriptorHeap> RTVHeap;
global_variable ComPtr<ID3D12Resource> RenderTargets[FrameCount];
global_variable ComPtr<ID3D12CommandAllocator> CommandAllocators[FrameCount][2];


global_variable ComPtr<ID3D12RootSignature> RootSignature;
global_variable ComPtr<ID3D12PipelineState> PipelineState;
global_variable ComPtr<ID3D12GraphicsCommandList4> CommandList;
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


global_variable renderer_config SupportConfig;

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
void DX12WaitForCommandList(uint32_t CommandListIndex)
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(CommandQueue->Signal(Synchronization[CommandListIndex].Fence.Get(), Synchronization[CommandListIndex].FenceValues[CurrentFrame]));
    
    // Wait until the fence has been processed.
    ThrowIfFailed(Synchronization[CommandListIndex].Fence->SetEventOnCompletion(Synchronization[CommandListIndex].FenceValues[CurrentFrame], 
                                                                                Synchronization[CommandListIndex].FenceEvent));
    WaitForSingleObjectEx(Synchronization[CommandListIndex].FenceEvent, INFINITE, FALSE);
    
    // Increment the fence value for the current frame.
    Synchronization[CommandListIndex].FenceValues[CurrentFrame]++;
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

//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//

acceleration_structure_buffers 
CreateBottomLevelAS()
{ 
    nv_helpers_dx12::BottomLevelASGenerator BottomLevelAS; 
    // Adding all vertex buffers and not transforming their position. 
    for (uint32_t BuffIndex = 0; BuffIndex < VertexBuffers.size(); ++BuffIndex) 
    { 
        BottomLevelAS.AddVertexBuffer(VertexBuffers[BuffIndex].MainBuffer.Get(), 0, VertexBuffers[BuffIndex].VertexCount, sizeof(vertex), 
                                      IndexBuffers[BuffIndex].MainBuffer.Get(), 0, IndexBuffers[BuffIndex].IndexCount,
                                      0, 0); 
    } 
    // The AS build requires some scratch space to store temporary information. 
    // The amount of scratch memory is dependent on the scene complexity. 
    uint64_t ScratchSizeInBytes = 0; 
    // The final AS also needs to be stored in addition to the existing vertex 
    // buffers. It size is also dependent on the scene complexity. 
    uint64_t ResultSizeInBytes = 0; 
    BottomLevelAS.ComputeASBufferSizes(Device.Get(), false, &ScratchSizeInBytes, &ResultSizeInBytes); 
    // Once the sizes are obtained, the application is responsible for allocating 
    // the necessary buffers. Since the entire generation will be done on the GPU, 
    // we can directly allocate those on the default heap 
    acceleration_structure_buffers Buffers; 
    Buffers.Scratch = nv_helpers_dx12::CreateBuffer(Device.Get(), 
                                                    ScratchSizeInBytes, 
                                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                    D3D12_RESOURCE_STATE_COMMON, 
                                                    nv_helpers_dx12::kDefaultHeapProps); 
    Buffers.Result = nv_helpers_dx12::CreateBuffer(Device.Get(), 
                                                   ResultSizeInBytes, 
                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                   D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
                                                   nv_helpers_dx12::kDefaultHeapProps); 
    // Build the acceleration structure. Note that this call integrates a barrier 
    // on the generated AS, so that it can be used to compute a top-level AS right 
    // after this method. 
    BottomLevelAS.Generate(CommandList.Get(), 
                           Buffers.Scratch.Get(), 
                           Buffers.Result.Get(), 
                           false, 
                           nullptr); 
    return Buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//

// pair of bottom level AS and matrix of the instance
void CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> &Instances)
{ 
    // Gather all the instances into the builder helper 
    for(size_t i = 0; i < Instances.size(); i++)
    { 
        Raytracing.TopLevelASGenerator.AddInstance(Instances[i].first.Get(), 
                                                   Instances[i].second, 
                                                   static_cast<uint32_t>(i), 
                                                   static_cast<uint32_t>(0));
    } 
    
    // As for the bottom-level AS, the building the AS requires some scratch space 
    // to store temporary data in addition to the actual AS. In the case of the 
    // top-level AS, the instance descriptors also need to be stored in GPU 
    // memory. This call outputs the memory requirements for each (scratch, 
    // results, instance descriptors) so that the application can allocate the 
    // corresponding memory 
    uint64_t ScratchSize; 
    uint64_t ResultSize;
    uint64_t InstanceDescsSize;
    Raytracing.TopLevelASGenerator.ComputeASBufferSizes(Device.Get(), 
                                                        true, 
                                                        &ScratchSize, 
                                                        &ResultSize, 
                                                        &InstanceDescsSize); 
    
    // Create the scratch and result buffers. Since the build is all done on GPU, 
    // those can be allocated on the default heap 
    Raytracing.TopLevelASBuffers.Scratch = nv_helpers_dx12::CreateBuffer(Device.Get(), 
                                                                         ScratchSize, 
                                                                         D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
                                                                         nv_helpers_dx12::kDefaultHeapProps); 
    Raytracing.TopLevelASBuffers.Result = nv_helpers_dx12::CreateBuffer(Device.Get(), 
                                                                        ResultSize, 
                                                                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                                        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
                                                                        nv_helpers_dx12::kDefaultHeapProps); 
    
    // The buffer describing the instances: ID, shader binding information, 
    // matrices ... Those will be copied into the buffer by the helper through 
    // mapping, so the buffer has to be allocated on the upload heap. 
    Raytracing.TopLevelASBuffers.InstanceDesc = nv_helpers_dx12::CreateBuffer(Device.Get(), 
                                                                              InstanceDescsSize, 
                                                                              D3D12_RESOURCE_FLAG_NONE, 
                                                                              D3D12_RESOURCE_STATE_GENERIC_READ, 
                                                                              nv_helpers_dx12::kUploadHeapProps);
    
    // After all the buffers are allocated, or if only an update is required, we 
    // can build the acceleration structure. Note that in the case of the update 
    // we also pass the existing AS as the 'previous' AS, so that it can be 
    // refitted in place. 
    Raytracing.TopLevelASGenerator.Generate(CommandList.Get(), 
                                            Raytracing.TopLevelASBuffers.Scratch.Get(), 
                                            Raytracing.TopLevelASBuffers.Result.Get(), 
                                            Raytracing.TopLevelASBuffers.InstanceDesc.Get());
}

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
void CreateAccelerationStructures() 
{ 
    // Build the bottom AS from the Triangle vertex buffer 
    acceleration_structure_buffers BottomLevelBuffers = CreateBottomLevelAS(); 
    
    // Just one instance for now 
    
    Raytracing.Instances = {{BottomLevelBuffers.Result, XMMatrixIdentity()}}; 
    CreateTopLevelAS(Raytracing.Instances); 
    
    
    const uint32_t GraphicsCommandList = 1;
    // Flush the command list and wait for it to finish 
    CommandList->Close(); 
    ID3D12CommandList *ppCommandLists[] = {CommandList.Get()}; 
    CommandQueue->ExecuteCommandLists(1, ppCommandLists); 
    Synchronization[GraphicsCommandList].FenceValues[CurrentFrame]++; 
    CommandQueue->Signal(Synchronization[GraphicsCommandList].Fence.Get(), Synchronization[GraphicsCommandList].FenceValues[CurrentFrame]); 
    Synchronization[GraphicsCommandList].Fence->SetEventOnCompletion(Synchronization[GraphicsCommandList].FenceValues[CurrentFrame], Synchronization[GraphicsCommandList].FenceEvent); 
    WaitForSingleObject(Synchronization[GraphicsCommandList].FenceEvent, INFINITE); 
    
    // Once the command list is finished executing, reset it to be reused for 
    // rendering 
    ThrowIfFailed(CommandList->Reset(CommandAllocators[CurrentFrame][GraphicsCommandList].Get(), PipelineState.Get())); 
    // Store the AS buffers. The rest of the buffers will be released once we exit 
    // the function 
    Raytracing.BottomLevelAS = BottomLevelBuffers.Result;
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
    
    for(uint32_t CommandListIndex = 0; CommandListIndex < 2; ++ CommandListIndex)
    {
        ThrowIfFailed(Device->CreateFence(Synchronization[CommandListIndex].FenceValues[CurrentFrame], 
                                          D3D12_FENCE_FLAG_NONE, 
                                          IID_PPV_ARGS(&Synchronization[CommandListIndex].Fence)));
        Synchronization[CommandListIndex].FenceValues[CurrentFrame]++;
        
        // Create an event handle to use for frame synchronization.
        Synchronization[CommandListIndex].FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (Synchronization[CommandListIndex].FenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        
        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        DX12WaitForCommandList(CommandListIndex);
    }
}


//-----------------------------------------------------------------------------
// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
//
ComPtr<ID3D12RootSignature> CreateRayGenSignature()
{ 
    nv_helpers_dx12::RootSignatureGenerator RootSignatureGen; 
    RootSignatureGen.AddHeapRangesParameter({
                                                {
                                                    0 /*u0*/, 
                                                    1 /*1 descriptor */, 
                                                    0 /*use the implicit register space 0*/, 
                                                    D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/, 
                                                    0 /*heap slot where the UAV is defined*/
                                                }, 
                                                {
                                                    0 /*t0*/, 
                                                    1, 
                                                    0, 
                                                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 
                                                    1
                                                }
                                            });
    return RootSignatureGen.Generate(Device.Get(), true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
//
ComPtr<ID3D12RootSignature> CreateMissSignature()
{ 
    nv_helpers_dx12::RootSignatureGenerator RootSignatureGen; 
    return RootSignatureGen.Generate(Device.Get(), true);
}

//-----------------------------------------------------------------------------
// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
//
ComPtr<ID3D12RootSignature> CreateHitSignature()
{ 
    nv_helpers_dx12::RootSignatureGenerator RootSignatureGen; 
    RootSignatureGen.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);
    return RootSignatureGen.Generate(Device.Get(), true);
}

//-----------------------------------------------------------------------------
//
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
//
//
void CreateRaytracingPipeline()
{ 
    nv_helpers_dx12::RayTracingPipelineGenerator Pipeline(Device.Get()); 
    // The pipeline contains the DXIL code of all the shaders potentially executed 
    // during the raytracing process. This section compiles the HLSL code into a 
    // set of DXIL libraries. We chose to separate the code in several libraries 
    // by semantic (ray generation, hit, miss) for clarity. Any code layout can be 
    // used. 
    Raytracing.RayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/RayGen.hlsl"); 
    Raytracing.MissLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/Miss.hlsl"); 
    Raytracing.HitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/Hit.hlsl");
    
    
    Pipeline.AddLibrary(Raytracing.RayGenLibrary.Get(), {L"RayGen"});
    Pipeline.AddLibrary(Raytracing.MissLibrary.Get(), {L"Miss"});
    Pipeline.AddLibrary(Raytracing.HitLibrary.Get(), {L"ClosestHit"});
    
    Pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // As described at the beginning of this section, to each shader corresponds a root signature defining
    // its external inputs.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // To be used, each DX12 shader needs a root signature defining which 
    // parameters and buffers will be accessed. 
    Raytracing.RayGenSignature = CreateRayGenSignature(); 
    Raytracing.MissSignature = CreateMissSignature(); 
    Raytracing.HitSignature = CreateHitSignature();
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // To be used, each shader needs to be associated to its root signature. A shaders imported from the DXIL libraries needs to be associated with exactly one root signature. The shaders comprising the hit groups need to share the same root signature, which is associated to the hit group (and not to the shaders themselves). Note that a shader does not have to actually access all the resources declared in its root signature, as long as the root signature defines a superset of the resources the shader needs.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // The following section associates the root signature to each shader. Note 
    // that we can explicitly show that some shaders share the same root signature 
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred 
    // to as hit groups, meaning that the underlying intersection, any-hit and 
    // closest-hit shaders share the same root signature. 
    Pipeline.AddRootSignatureAssociation(Raytracing.RayGenSignature.Get(), {L"RayGen"}); 
    Pipeline.AddRootSignatureAssociation(Raytracing.MissSignature.Get(), {L"Miss"}); 
    Pipeline.AddRootSignatureAssociation(Raytracing.HitSignature.Get(), {L"HitGroup"});
    
    Pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance
    
    Pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates
    
    Pipeline.SetMaxRecursionDepth(1);
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // The pipeline now has all the information it needs. We generate the pipeline by calling the `Generate`
    // method of the helper, which creates the array of subobjects and calls
    // `ID3D12Device5::CreateStateObject`.
    Raytracing.StateObject = Pipeline.Generate();
    Raytracing.StateObject->QueryInterface(IID_PPV_ARGS(&Raytracing.StateObjectProps)); 
}

//-----------------------------------------------------------------------------
//
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
//
void CreateRaytracingOutputBuffer(win32_window_dimension Dimension)
{ 
    D3D12_RESOURCE_DESC ResDesc = {}; 
    ResDesc.DepthOrArraySize = 1; 
    ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; 
    // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB 
    // formats cannot be used with UAVs. For accuracy we should convert to sRGB 
    // ourselves in the shader 
    ResDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; 
    ResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; 
    ResDesc.Width = Dimension.Width; 
    ResDesc.Height = Dimension.Height; 
    ResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; 
    ResDesc.MipLevels = 1; 
    ResDesc.SampleDesc.Count = 1; 
    ThrowIfFailed(Device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &ResDesc,
                                                  D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                  nullptr,
                                                  IID_PPV_ARGS(&Raytracing.OutputResource)));
}

//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
void CreateShaderResourceHeap()
{ 
    // Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the 
    // raytracing output and 1 SRV for the TLAS 
    Raytracing.SrvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(Device.Get(), 
                                                                  2, 
                                                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
                                                                  true); 
    
    // Get a handle to the heap memory on the CPU side, to be able to write the 
    // descriptors directly 
    D3D12_CPU_DESCRIPTOR_HANDLE SrvHandle = Raytracing.SrvUavHeap->GetCPUDescriptorHandleForHeapStart(); 
    
    // Create the UAV. Based on the root signature we created it is the first 
    // entry. The Create*View methods write the view information directly into 
    // srvHandle 
    D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc = {}; 
    UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; 
    Device->CreateUnorderedAccessView(Raytracing.OutputResource.Get(), 
                                      nullptr, 
                                      &UavDesc, 
                                      SrvHandle); 
    
    // Add the Top Level AS SRV right after the raytracing output buffer 
    SrvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc; 
    SrvDesc.Format = DXGI_FORMAT_UNKNOWN; 
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE; 
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; 
    SrvDesc.RaytracingAccelerationStructure.Location = Raytracing.TopLevelASBuffers.Result->GetGPUVirtualAddress(); 
    
    // Write the acceleration structure view in the heap 
    Device->CreateShaderResourceView(nullptr, &SrvDesc, SrvHandle);
}

//-----------------------------------------------------------------------------
//
// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
// this is where the shader resources are bound to the shaders, in a way that
// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
// contains a series of shader IDs with their resource pointers. The SBT
// contains the ray generation shader, the miss shaders, then the hit groups.
// Using the helper class, those can be specified in arbitrary order.
//
void CreateShaderBindingTable() 
{ 
    // The SBT helper class collects calls to Add*Program. If called several 
    // times, the helper must be emptied before re-adding shaders. 
    Raytracing.SbtHelper.Reset(); 
    // The pointer to the beginning of the heap is the only parameter required by 
    // shaders without root parameters 
    D3D12_GPU_DESCRIPTOR_HANDLE SrvUavHeapHandle = Raytracing.SrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    uint64_t* HeapPointer = reinterpret_cast<uint64_t*>(SrvUavHeapHandle.ptr);
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // We can now add the various programs used in our example: according to its root signature, the ray generation shader needs to access
    // the raytracing output buffer and the top-level acceleration structure referenced in the heap. Therefore, it
    // takes a single resource pointer towards the beginning of the heap data. The miss shader and the hit group
    // only communicate through the ray payload, and do not require any resource, hence an empty resource array.
    // Note that the helper will group the shaders by types in the SBT, so it is possible to declare them in an
    // arbitrary order. For example, miss programs can be added before or after ray generation programs without
    // affecting the result.
    // However, within a given type (say, the hit groups), the order in which they are added
    // is important. It needs to correspond to the `InstanceContributionToHitGroupIndex` value used when adding
    // instances to the top-level acceleration structure: for example, an instance having `InstanceContributionToHitGroupIndex==0`
    // needs to have its hit group added first in the SBT.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    
    // The ray generation only uses heap data 
    Raytracing.SbtHelper.AddRayGenerationProgram(L"RayGen", {HeapPointer}); 
    
    // The miss and hit shaders do not access any external resources: instead they 
    // communicate their results through the ray payload 
    Raytracing.SbtHelper.AddMissProgram(L"Miss", {}); 
    
    // Adding the triangle hit shader 
    std::vector<void*> BuffersVirtualAddresses;
    BuffersVirtualAddresses.reserve(VertexBuffers.size());
    for(uint32_t i = 0; i < VertexBuffers.size(); ++i)
    {
        BuffersVirtualAddresses.push_back((void*)(VertexBuffers[i].MainBuffer->GetGPUVirtualAddress()));
    }
    Raytracing.SbtHelper.AddHitGroup(L"HitGroup", BuffersVirtualAddresses);
    
    // Create the SBT on the upload heap
    uint32_t SbtSize = 0;
    SbtSize = Raytracing.SbtHelper.ComputeSBTSize();
    Raytracing.SbtStorage = nv_helpers_dx12::CreateBuffer(Device.Get(), SbtSize,
                                                          D3D12_RESOURCE_FLAG_NONE, 
                                                          D3D12_RESOURCE_STATE_GENERIC_READ,
                                                          nv_helpers_dx12::kUploadHeapProps);
    Raytracing.SbtHelper.Generate(Raytracing.SbtStorage.Get(), Raytracing.StateObjectProps.Get());
}
void
DX12InitRenderer(HWND Window, renderer_config* Config)
{
    SupportConfig = {};
    SupportConfig.VSync = true;
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
    
    // Check Raytracing support;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options5 = {}; 
    if(SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Options5, sizeof(Options5))))
    {
        SupportConfig.UseRaytracing = Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
    }
    else
    {
        SupportConfig.UseRaytracing = false;
    }
    
    if(Config->UseRaytracing)
    {
        Config->UseRaytracing = SupportConfig.UseRaytracing;
    }
    
    
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    QueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue)));
    
    ComPtr<IDXGISwapChain1> NewSwapChain;
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Width = Config->OutputDimensions.Width;
    SwapChainDesc.Height = Config->OutputDimensions.Height;
    SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    //SwapChainDesc.Stero = FALSE;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = FrameCount;
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
            HR = Factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &SupportConfig.AllowTearing, sizeof(SupportConfig.AllowTearing));
        }
        if (Config->AllowTearing)
        {
            Config->AllowTearing = SupportConfig.AllowTearing;
        }
        if (Config->AllowTearing)
        {
            Config->VSync = false;
            SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }
    }
    
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
    
    if (SupportConfig.UseRaytracing)
    {
        
        // Create the raytracing pipeline, associating the shader code to symbol names
        // and to their root signatures, and defining the amount of memory carried by
        // rays (ray payload)
        CreateRaytracingPipeline(); // #DXR
        
        // Allocate the buffer storing the raytracing output, with the same dimensions
        // as the target image
        CreateRaytracingOutputBuffer(Config->OutputDimensions); // #DXR
    }
    
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = (float)Config->OutputDimensions.Width;
    Viewport.Height = (float)Config->OutputDimensions.Height;
    Viewport.MinDepth = D3D12_MIN_DEPTH;
    Viewport.MaxDepth = D3D12_MAX_DEPTH;
    
    ScissorRect.left = 0;
    ScissorRect.top = 0;
    ScissorRect.right = Config->OutputDimensions.Width;
    ScissorRect.bottom = Config->OutputDimensions.Height;
    
    
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
DX12Render(renderer_config Config)
{
    ThrowIfFailed(LoadingCommandList->Close());
    
    // Execute the loading command list.
    ID3D12CommandList* ppLoadingCommandLists[] = { LoadingCommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(ppLoadingCommandLists), ppLoadingCommandLists);
    
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(CommandAllocators[CurrentFrame][1]->Reset());
    
    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(CommandList->Reset(CommandAllocators[CurrentFrame][1].Get(), PipelineState.Get()));
    
    if(Config.UseRaytracing)
    {
        if(!Raytracing.BottomLevelAS)
        {
            // Setup the acceleration structures (AS) for raytracing. When setting up 
            // geometry, each bottom-level AS has its own transform matrix. 
            CreateAccelerationStructures(); 
            
            // Command lists are created in the recording state, but there is 
            // nothing to record yet. The main loop expects it to be closed, so 
            // close it now. 
            //ThrowIfFailed(CommandList->Close());
            
            // Create the buffer containing the raytracing result (always output in a
            // UAV), and create the heap referencing the resources used by the raytracing,
            // such as the acceleration structure
            CreateShaderResourceHeap(); // #DXR
            
            // Create the shader binding table and indicating which shaders
            // are invoked for each instance in the AS
            CreateShaderBindingTable();
        }
        
        // Bind the raytracing pipeline
        CommandList->SetPipelineState1(Raytracing.StateObject.Get());
    }
    else
    {
        // Set necessary state.
        CommandList->SetGraphicsRootSignature(RootSignature.Get());
    }
    
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
    
    // Record commands
    if(Config.UseRaytracing && SupportConfig.UseRaytracing)
    {
        // #DXR
        // Bind the descriptor heap giving access to the top-level acceleration
        // structure, as well as the raytracing output
        std::vector<ID3D12DescriptorHeap*> Heaps = {Raytracing.SrvUavHeap.Get()};
        CommandList->SetDescriptorHeaps((uint32_t)Heaps.size(), Heaps.data());
        
        // On the last frame, the raytracing output was used as a copy source, to
        // copy its contents into the render target. Now we need to transition it to
        // a UAV so that the shaders can write in it.
        Barrier.Transition.pResource = Raytracing.OutputResource.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        CommandList->ResourceBarrier(1, &Barrier);
        
        // Setup the raytracing task
        D3D12_DISPATCH_RAYS_DESC Desc = {};
        // The layout of the SBT is as follows: ray generation shader, miss
        // shaders, hit groups. As described in the CreateShaderBindingTable method,
        // all SBT entries of a given type have the same size to allow a fixed stride.
        // The ray generation shaders are always at the beginning of the SBT.
        
        uint32_t RayGenerationSectionSizeInBytes = Raytracing.SbtHelper.GetRayGenSectionSize();
        Desc.RayGenerationShaderRecord.StartAddress = Raytracing.SbtStorage->GetGPUVirtualAddress();
        Desc.RayGenerationShaderRecord.SizeInBytes = RayGenerationSectionSizeInBytes;
        
        // The miss shaders are in the second SBT section, right after the ray
        // generation shader. We have one miss shader for the camera rays and one
        // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
        // also indicate the stride between the two miss shaders, which is the size
        // of a SBT entry
        uint32_t MissSectionSizeInBytes = Raytracing.SbtHelper.GetMissSectionSize();
        Desc.MissShaderTable.StartAddress = Raytracing.SbtStorage->GetGPUVirtualAddress() + RayGenerationSectionSizeInBytes;
        Desc.MissShaderTable.SizeInBytes = MissSectionSizeInBytes;
        Desc.MissShaderTable.StrideInBytes = Raytracing.SbtHelper.GetMissEntrySize();
        
        // The hit groups section start after the miss shaders. In this sample we
        // have one 1 hit group for the triangle
        uint32_t HitGroupsSectionSize = Raytracing.SbtHelper.GetHitGroupSectionSize();
        Desc.HitGroupTable.StartAddress = Raytracing.SbtStorage->GetGPUVirtualAddress() + RayGenerationSectionSizeInBytes + MissSectionSizeInBytes;
        Desc.HitGroupTable.SizeInBytes = HitGroupsSectionSize;
        Desc.HitGroupTable.StrideInBytes = Raytracing.SbtHelper.GetHitGroupEntrySize();
        
        // Dimensions of the image to render, identical to a kernel launch dimension
        //TODO: Get the window dimensions here
        Desc.Width = 1280;
        Desc.Height = 720;
        Desc.Depth = 1;
        
        // Dispatch the rays and write to the raytracing output
        CommandList->DispatchRays(&Desc);
        
        // The raytracing output needs to be copied to the actual render target used
        // for display. For this, we need to transition the raytracing output from a
        // UAV to a copy source, and the render target buffer to a copy destination.
        // We can then do the actual copy, before transitioning the render target
        // buffer into a render target, that will be then used to display the image
        Barrier.Transition.pResource = Raytracing.OutputResource.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        CommandList->ResourceBarrier(1, &Barrier);
        
        Barrier.Transition.pResource = RenderTargets[CurrentFrame].Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        CommandList->ResourceBarrier(1, &Barrier);
        
        CommandList->CopyResource(RenderTargets[CurrentFrame].Get(), Raytracing.OutputResource.Get());
        
        Barrier.Transition.pResource = RenderTargets[CurrentFrame].Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        CommandList->ResourceBarrier(1, &Barrier);
        
        RenderList = std::queue<mesh_handle>();
    }
    else
    {
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
DX12Present(renderer_config Config)
{
    uint32_t SyncInterval = Config.VSync ? 1 : 0;
    uint32_t Flags = 0;
    if(Config.AllowTearing && !Config.VSync && SupportConfig.AllowTearing)
    {
        Flags |= DXGI_PRESENT_ALLOW_TEARING;
    }
    
    ThrowIfFailed(SwapChain->Present(SyncInterval, Flags));
    
    // Schedule a Signal command in the queue.
    const uint32_t GraphicsCommandList = 1;
    uint64_t CurrentFenceValue = Synchronization[GraphicsCommandList].FenceValues[CurrentFrame];
    ThrowIfFailed(CommandQueue->Signal(Synchronization[GraphicsCommandList].Fence.Get(), CurrentFenceValue));
    
    // Update the frame index.
    CurrentFrame = SwapChain->GetCurrentBackBufferIndex();
    
    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (Synchronization[GraphicsCommandList].Fence->GetCompletedValue() < Synchronization[GraphicsCommandList].FenceValues[CurrentFrame])
    {
        ThrowIfFailed(Synchronization[GraphicsCommandList].Fence->SetEventOnCompletion(Synchronization[GraphicsCommandList].FenceValues[CurrentFrame], Synchronization[GraphicsCommandList].FenceEvent));
        WaitForSingleObjectEx(Synchronization[GraphicsCommandList].FenceEvent, INFINITE, FALSE);
    }
    
    // Set the fence value for the next frame.
    Synchronization[GraphicsCommandList].FenceValues[CurrentFrame] = CurrentFenceValue + 1;
    
    DX12WaitForCommandList(0);
    ThrowIfFailed(CommandAllocators[CurrentFrame][0]->Reset());
    ThrowIfFailed(LoadingCommandList->Reset(CommandAllocators[CurrentFrame][0].Get(), PipelineState.Get()));
}

void
DX12ShutdownRenderer()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    DX12WaitForCommandList(0);
    CloseHandle(Synchronization[0].FenceEvent);
    DX12WaitForCommandList(1);
    CloseHandle(Synchronization[1].FenceEvent);
}
