#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"

void DX12InitRenderer(HWND Window, win32_window_dimension Dimension);

mesh_handle DX12CreateVertexBuffer(vector3* Vertices, vector3* Normals, uint32_t VerticesCount);

void DX12Render();

void DX12Present();

void DX12ShutdownRenderer();
#define DX12_RENDERER_H
#endif
