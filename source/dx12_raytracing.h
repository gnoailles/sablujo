#ifndef DX12_RAYTRACING_H

#include "win32_sablujo.h"

#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <D3DCompiler.h>

#include <dxcapi.h>

#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"

using Microsoft::WRL::ComPtr;

struct dx12_renderer;

// #DXR
struct acceleration_structure_buffers
{ 
    ComPtr<ID3D12Resource> Scratch; // Scratch memory for AS builder 
    ComPtr<ID3D12Resource> Result; // Where the AS is 
    ComPtr<ID3D12Resource> InstanceDesc; // Hold the matrices of the instances
};

#include <directxmath.h>
struct raytracing_data
{
    ComPtr<ID3D12Resource> BottomLevelAS; // Storage for the bottom Level AS
    ComPtr<ID3D12Resource> OutputResource;
    // Ray tracing pipeline state
    ComPtr<ID3D12StateObject> StateObject;
    
    ComPtr<ID3D12DescriptorHeap> SrvUavHeap;
    
    // Shader Binding Table
    nv_helpers_dx12::ShaderBindingTableGenerator SbtHelper;
    ComPtr<ID3D12Resource> SbtStorage;
    
    //TODO: Move these to cpp
    acceleration_structure_buffers TopLevelASBuffers;
    std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> Instances;
    
    // Ray tracing pipeline state properties, retaining the shader identifiers
    // to use in the Shader Binding Table
    ComPtr<ID3D12StateObjectProperties> StateObjectProps;
};

void CreateRaytracingPipeline(dx12_renderer* Renderer);
void CreateRaytracingOutputBuffer(dx12_renderer* Renderer, win32_window_dimension Dimension);
void CreateAccelerationStructures(dx12_renderer* Renderer);
void CreateShaderResourceHeap(dx12_renderer* Renderer);
void CreateShaderBindingTable(dx12_renderer* Renderer);

#define DX12_RAYTRACING_H
#endif //DX12_RAYTRACING_H
