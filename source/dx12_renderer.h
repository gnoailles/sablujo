#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"

struct renderer_config
{
    bool VSync;
    bool AllowTearing;
    bool UseRaytracing;
    bool UseComputeRaytracing;
    win32_window_dimension OutputDimensions;
};

void DX12InitRenderer(HWND Window, renderer_config* Config);

void DX12Present();
//mesh_handle DX12CreateVertexBuffer(vector3* Vertices, vector3* Normals, uint32_t VerticesCount);
mesh_handle DX12CreateVertexBuffer(float* Vertices, uint32_t* Indices,
                                   uint32_t VertexCount, uint32_t VerticesCount, uint32_t IndicesCount);

// 
void DX12SetViewProjection(float* ViewProjection);
void DX12SubmitForRender(mesh_handle Mesh);

void DX12Render(renderer_config Config);

void DX12Present(renderer_config Config);

void DX12ShutdownRenderer();
#define DX12_RENDERER_H
#endif
