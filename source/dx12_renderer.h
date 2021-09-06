#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"

win32_offscreen_buffer DX12InitRenderer(HWND Window, win32_window_dimension Dimension);

mesh_handle DX12CreateVertexBuffer(vector3* Vertices, vector3* Normals, uint32_t VerticesCount);

void DX12Render(win32_offscreen_buffer* Buffer);

void DX12Present();

void DX12ShutdownRenderer();
#define DX12_RENDERER_H
#endif
