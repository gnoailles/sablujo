#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"

void DX12InitRenderer(HWND Window, win32_window_dimension Dimension);

mesh_handle DX12CreateVertexBuffer(float* Vertices, uint32_t* Indices,
                                   uint32_t VertexCount, uint32_t VerticesCount, uint32_t IndicesCount);

// 
void DX12SetViewProjection(float* ViewProjection);
void DX12SubmitForRender(mesh_handle Mesh);
void DX12Render();

void DX12Present();

void DX12ShutdownRenderer();
#define DX12_RENDERER_H
#endif