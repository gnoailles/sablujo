#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"
#include "sablujo_memory.h"

#include <vector>
#include <queue>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <D3DCompiler.h>

struct vertex_buffer;
struct index_buffer;
struct const_buffer;

struct mesh
{
    handle<vertex_buffer> VertexBuffer;
    handle<index_buffer> IndexBuffer;
};

struct renderer_config
{
    bool VSync;
    bool AllowTearing;
    bool UseImGUI;
    bool UseRaytracing;
    bool UseComputeRaytracing;
    win32_window_dimension OutputDimensions;
};


#define FRAME_COUNT 2
struct raytracing_data;
using Microsoft::WRL::ComPtr;

#include <directxmath.h>
using namespace DirectX;
struct vertex
{
    XMFLOAT4 Position;
    XMFLOAT3 Normal;
};


struct dx12_vertex_buffer
{
    ComPtr<ID3D12Resource> MainBuffer;
    ComPtr<ID3D12Resource> StagingBuffer;
    D3D12_VERTEX_BUFFER_VIEW BufferView;
    uint32_t VertexCount {0};
};

struct dx12_index_buffer
{
    ComPtr<ID3D12Resource> MainBuffer;
    ComPtr<ID3D12Resource> StagingBuffer;
    D3D12_INDEX_BUFFER_VIEW BufferView;
    uint32_t IndexCount {0};
};

struct dx12_const_buffer
{
    ComPtr<ID3D12Resource> MainBuffer;
    ComPtr<ID3D12DescriptorHeap> Heap;
    uint32_t Size;
};

struct dx12_present_synchronization
{
    HANDLE FenceEvent;
    ComPtr<ID3D12Fence> Fence;
    uint64_t FenceValues[FRAME_COUNT];
};

struct command_context
{
    ComPtr<ID3D12GraphicsCommandList> CommandList;
    ComPtr<ID3D12CommandAllocator> CommandAllocators[FRAME_COUNT];
    dx12_present_synchronization Synchronization;
};

struct dx12_renderer
{
    memory_area* MemArea;
    uint32_t FrameCount = FRAME_COUNT;
    uint32_t CurrentFrame;
    renderer_config SupportedConfig;
    
    ComPtr<ID3D12Device8> Device;
    ComPtr<ID3D12CommandQueue> CommandQueue;
    ComPtr<IDXGISwapChain4> SwapChain;
    
    raytracing_data* Raytracing;
    ComPtr<ID3D12DescriptorHeap> RTVHeap;
    ComPtr<ID3D12Resource> RenderTargets[FRAME_COUNT];
    
    command_context MainCommandContext;
    command_context LoadingCommandContext;
    
    // ImGui
    command_context ImGuiCommandContext;
    ComPtr<ID3D12DescriptorHeap> ImGuiSrvDescHeap;
    
    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3D12PipelineState> PipelineState;
    
    // Resources
    pool<mesh, mesh> Meshes;
    pool<dx12_vertex_buffer, vertex_buffer> VertexBuffers;
    pool<dx12_index_buffer, index_buffer> IndexBuffers;
    pool<dx12_const_buffer, const_buffer> ConstBuffers;
    
    // FrameData / Scene
    std::queue<mesh_instance> RenderList;
    
    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;
    handle<const_buffer> MVPBuffer;
    //XMMATRIX CurrentViewProjection;
    
};


void DX12InitRenderer(HWND Window, renderer_config* Config, memory_area* RendererArea);

void DX12Present();

handle<mesh> DX12CreateVertexBuffer(float* Vertices, uint32_t* Indices,
                                    uint32_t VertexCount, uint32_t VerticesCount, uint32_t IndicesCount);

handle<const_buffer> DX12CreateConstBuffer(uint32_t Size);
void DX12UpdateConstBuffer(handle<const_buffer> Buffer, void* Data, uint32_t Size);
// 
void DX12SetViewProjection(float* View, float* Projection);
void DX12SubmitForRender(mesh_instance MeshInstance);

void DX12Render(renderer_config Config);

void DX12Present(renderer_config Config);

void DX12ShutdownRenderer(renderer_config Config);
#define DX12_RENDERER_H
#endif
