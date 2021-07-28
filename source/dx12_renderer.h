#if !defined(DX12_RENDERER_H)

#include "win32_sablujo.h"

void DX12InitRenderer(HWND Window, win32_window_dimension Dimension);

void DX12Present();

void DX12ShutdownRenderer();

#define DX12_RENDERER_H
#endif
