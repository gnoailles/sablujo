#include "dx12_raytracing.h"
#include "dx12_renderer.h"

#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include "DXRHelper.h"



//-----------------------------------------------------------------------------
// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
//
ComPtr<ID3D12RootSignature> CreateRayGenSignature(dx12_renderer* Renderer)
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
                                                },
                                                {
                                                    0 /*b0*/, 
                                                    1, 
                                                    0, 
                                                    D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 
                                                    2
                                                }
                                            });
    return RootSignatureGen.Generate(Renderer->Device.Get(), true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
//
ComPtr<ID3D12RootSignature> CreateMissSignature(dx12_renderer* Renderer)
{ 
    nv_helpers_dx12::RootSignatureGenerator RootSignatureGen; 
    return RootSignatureGen.Generate(Renderer->Device.Get(), true);
}

//-----------------------------------------------------------------------------
// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
//
ComPtr<ID3D12RootSignature> CreateHitSignature(dx12_renderer* Renderer)
{ 
    nv_helpers_dx12::RootSignatureGenerator RootSignatureGen; 
    RootSignatureGen.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);
    return RootSignatureGen.Generate(Renderer->Device.Get(), true);
}


//-----------------------------------------------------------------------------
//
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
//
//
void CreateRaytracingPipeline(dx12_renderer* Renderer)
{ 
    nv_helpers_dx12::RayTracingPipelineGenerator Pipeline(Renderer->Device.Get()); 
    // The pipeline contains the DXIL code of all the shaders potentially executed 
    // during the raytracing process. This section compiles the HLSL code into a 
    // set of DXIL libraries. We chose to separate the code in several libraries 
    // by semantic (ray generation, hit, miss) for clarity. Any code layout can be 
    // used. 
    ComPtr<IDxcBlob> RayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/RayGen.hlsl"); 
    ComPtr<IDxcBlob> MissLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/Miss.hlsl"); 
    ComPtr<IDxcBlob> HitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"resources/shaders/raytracing/Hit.hlsl");
    
    
    Pipeline.AddLibrary(RayGenLibrary.Get(), {L"RayGen"});
    Pipeline.AddLibrary(MissLibrary.Get(), {L"Miss"});
    Pipeline.AddLibrary(HitLibrary.Get(), {L"ClosestHit"});
    
    Pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // As described at the beginning of this section, to each shader corresponds a root signature defining
    // its external inputs.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // To be used, each DX12 shader needs a root signature defining which 
    // parameters and buffers will be accessed. 
    ComPtr<ID3D12RootSignature> RayGenSignature = CreateRayGenSignature(Renderer); 
    ComPtr<ID3D12RootSignature> MissSignature = CreateMissSignature(Renderer); 
    ComPtr<ID3D12RootSignature> HitSignature = CreateHitSignature(Renderer);
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // To be used, each shader needs to be associated to its root signature. A shaders imported from the DXIL libraries needs to be associated with exactly one root signature. The shaders comprising the hit groups need to share the same root signature, which is associated to the hit group (and not to the shaders themselves). Note that a shader does not have to actually access all the resources declared in its root signature, as long as the root signature defines a superset of the resources the shader needs.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // The following section associates the root signature to each shader. Note 
    // that we can explicitly show that some shaders share the same root signature 
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred 
    // to as hit groups, meaning that the underlying intersection, any-hit and 
    // closest-hit shaders share the same root signature. 
    Pipeline.AddRootSignatureAssociation(RayGenSignature.Get(), {L"RayGen"}); 
    Pipeline.AddRootSignatureAssociation(MissSignature.Get(), {L"Miss"}); 
    Pipeline.AddRootSignatureAssociation(HitSignature.Get(), {L"HitGroup"});
    
    Pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance
    
    Pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates
    
    Pipeline.SetMaxRecursionDepth(1);
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // The pipeline now has all the information it needs. We generate the pipeline by calling the `Generate`
    // method of the helper, which creates the array of subobjects and calls
    // `ID3D12Device5::CreateStateObject`.
    Renderer->Raytracing->StateObject = Pipeline.Generate();
    Renderer->Raytracing->StateObject->QueryInterface(IID_PPV_ARGS(&Renderer->Raytracing->StateObjectProps)); 
}

//-----------------------------------------------------------------------------
//
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
//
void CreateRaytracingOutputBuffer(dx12_renderer* Renderer, win32_window_dimension Dimension)
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
    ThrowIfFailed(Renderer->Device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &ResDesc,
                                                            D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                            nullptr,
                                                            IID_PPV_ARGS(&Renderer->Raytracing->OutputResource)));
}


//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//

acceleration_structure_buffers 
CreateBottomLevelAS(dx12_renderer* Renderer)
{ 
    nv_helpers_dx12::BottomLevelASGenerator BottomLevelAS; 
    // Adding all vertex buffers and not transforming their position. 
    for (uint32_t BuffIndex = 0; BuffIndex < Renderer->VertexBuffers.Count; ++BuffIndex) 
    { 
        BottomLevelAS.AddVertexBuffer(Renderer->VertexBuffers.Data[BuffIndex].MainBuffer.Get(), 0, Renderer->VertexBuffers.Data[BuffIndex].VertexCount, sizeof(vertex), 
                                      Renderer->IndexBuffers.Data[BuffIndex].MainBuffer.Get(), 0, Renderer->IndexBuffers.Data[BuffIndex].IndexCount,
                                      0, 0); 
    } 
    // The AS build requires some scratch space to store temporary information. 
    // The amount of scratch memory is dependent on the scene complexity. 
    uint64_t ScratchSizeInBytes = 0; 
    // The final AS also needs to be stored in addition to the existing vertex 
    // buffers. It size is also dependent on the scene complexity. 
    uint64_t ResultSizeInBytes = 0; 
    BottomLevelAS.ComputeASBufferSizes(Renderer->Device.Get(), false, &ScratchSizeInBytes, &ResultSizeInBytes); 
    // Once the sizes are obtained, the application is responsible for allocating 
    // the necessary buffers. Since the entire generation will be done on the GPU, 
    // we can directly allocate those on the default heap 
    acceleration_structure_buffers Buffers; 
    Buffers.Scratch = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), 
                                                    ScratchSizeInBytes, 
                                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                    D3D12_RESOURCE_STATE_COMMON, 
                                                    nv_helpers_dx12::kDefaultHeapProps); 
    Buffers.Result = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), 
                                                   ResultSizeInBytes, 
                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                   D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
                                                   nv_helpers_dx12::kDefaultHeapProps); 
    // Build the acceleration structure. Note that this call integrates a barrier 
    // on the generated AS, so that it can be used to compute a top-level AS right 
    // after this method. 
    BottomLevelAS.Generate((ID3D12GraphicsCommandList4*)Renderer->MainCommandContext.CommandList.Get(), 
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
void CreateTopLevelAS(dx12_renderer* Renderer, const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> &Instances)
{ 
    nv_helpers_dx12::TopLevelASGenerator TopLevelASGenerator;
    // Gather all the instances into the builder helper 
    for(size_t i = 0; i < Instances.size(); i++)
    { 
        TopLevelASGenerator.AddInstance(Instances[i].first.Get(), 
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
    TopLevelASGenerator.ComputeASBufferSizes(Renderer->Device.Get(), 
                                             true, 
                                             &ScratchSize, 
                                             &ResultSize, 
                                             &InstanceDescsSize); 
    
    // Create the scratch and result buffers. Since the build is all done on GPU, 
    // those can be allocated on the default heap 
    Renderer->Raytracing->TopLevelASBuffers.Scratch = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), 
                                                                                    ScratchSize, 
                                                                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
                                                                                    nv_helpers_dx12::kDefaultHeapProps); 
    Renderer->Raytracing->TopLevelASBuffers.Result = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), 
                                                                                   ResultSize, 
                                                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
                                                                                   D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
                                                                                   nv_helpers_dx12::kDefaultHeapProps); 
    
    // The buffer describing the instances: ID, shader binding information, 
    // matrices ... Those will be copied into the buffer by the helper through 
    // mapping, so the buffer has to be allocated on the upload heap. 
    Renderer->Raytracing->TopLevelASBuffers.InstanceDesc = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), 
                                                                                         InstanceDescsSize, 
                                                                                         D3D12_RESOURCE_FLAG_NONE, 
                                                                                         D3D12_RESOURCE_STATE_GENERIC_READ, 
                                                                                         nv_helpers_dx12::kUploadHeapProps);
    
    // After all the buffers are allocated, or if only an update is required, we 
    // can build the acceleration structure. Note that in the case of the update 
    // we also pass the existing AS as the 'previous' AS, so that it can be 
    // refitted in place. 
    TopLevelASGenerator.Generate((ID3D12GraphicsCommandList4*)Renderer->MainCommandContext.CommandList.Get(), 
                                 Renderer->Raytracing->TopLevelASBuffers.Scratch.Get(), 
                                 Renderer->Raytracing->TopLevelASBuffers.Result.Get(), 
                                 Renderer->Raytracing->TopLevelASBuffers.InstanceDesc.Get());
}


//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
void CreateAccelerationStructures(dx12_renderer* Renderer)
{ 
    // Build the bottom AS from the Triangle vertex buffer 
    acceleration_structure_buffers BottomLevelBuffers = CreateBottomLevelAS(Renderer); 
    
    // Just one instance for now 
    
    Renderer->Raytracing->Instances = {{BottomLevelBuffers.Result, XMMatrixIdentity()}}; 
    CreateTopLevelAS(Renderer, Renderer->Raytracing->Instances); 
    
    
    const uint32_t GraphicsCommandList = 1;
    // Flush the command list and wait for it to finish 
    Renderer->MainCommandContext.CommandList->Close(); 
    ID3D12CommandList *ppCommandLists[] = {Renderer->MainCommandContext.CommandList.Get()}; 
    Renderer->CommandQueue->ExecuteCommandLists(1, ppCommandLists); 
    
    dx12_present_synchronization* MainCommandsSync = &Renderer->MainCommandContext.Synchronization;
    MainCommandsSync->FenceValues[Renderer->CurrentFrame]++; 
    Renderer->CommandQueue->Signal(MainCommandsSync->Fence.Get(), 
                                   MainCommandsSync->FenceValues[Renderer->CurrentFrame]); 
    MainCommandsSync->Fence->SetEventOnCompletion(MainCommandsSync->FenceValues[Renderer->CurrentFrame], 
                                                  MainCommandsSync->FenceEvent); 
    WaitForSingleObject(MainCommandsSync->FenceEvent, INFINITE); 
    
    // Once the command list is finished executing, reset it to be reused for 
    // rendering 
    ThrowIfFailed(Renderer->MainCommandContext.CommandList->Reset(Renderer->MainCommandContext.CommandAllocators[Renderer->CurrentFrame].Get(), 
                                                                  Renderer->PipelineState.Get())); 
    // Store the AS buffers. The rest of the buffers will be released once we exit 
    // the function 
    Renderer->Raytracing->BottomLevelAS = BottomLevelBuffers.Result;
}


//-----------------------------------------------------------------------------
//
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
//
void CreateShaderResourceHeap(dx12_renderer* Renderer)
{ 
    // Create a SRV/UAV/CBV descriptor heap. We need 3 entries 
    // - 1 UAV for the raytracing output
    // - 1 SRV for the TLAS 
    // - 1 CBV for the view projection matrices
    Renderer->Raytracing->SrvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(Renderer->Device.Get(), 
                                                                             3, 
                                                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
                                                                             true); 
    
    // Get a handle to the heap memory on the CPU side, to be able to write the 
    // descriptors directly 
    D3D12_CPU_DESCRIPTOR_HANDLE SrvHandle = Renderer->Raytracing->SrvUavHeap->GetCPUDescriptorHandleForHeapStart(); 
    
    // Create the UAV. Based on the root signature we created it is the first 
    // entry. The Create*View methods write the view information directly into 
    // srvHandle 
    D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc = {}; 
    UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; 
    Renderer->Device->CreateUnorderedAccessView(Renderer->Raytracing->OutputResource.Get(), 
                                                nullptr, 
                                                &UavDesc, 
                                                SrvHandle); 
    
    // Add the Top Level AS SRV right after the raytracing output buffer 
    SrvHandle.ptr += Renderer->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc; 
    SrvDesc.Format = DXGI_FORMAT_UNKNOWN; 
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE; 
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; 
    SrvDesc.RaytracingAccelerationStructure.Location = Renderer->Raytracing->TopLevelASBuffers.Result->GetGPUVirtualAddress(); 
    // Write the acceleration structure view in the heap 
    Renderer->Device->CreateShaderResourceView(nullptr, &SrvDesc, SrvHandle);
    
    // #DXR Extra: Perspective Camera
    // Add the constant buffer for the camera after the TLAS
    SrvHandle.ptr += Renderer->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // Describe and create a constant buffer view for the camera
    dx12_const_buffer* MVPBuffer = GetPoolInstance(&Renderer->ConstBuffers, Renderer->MVPBuffer);
    
    uint32_t AllocatedSize = MAX(MVPBuffer->Size, 256);
    Assert(AllocatedSize % 256 == 0);
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC CbvDesc = {};
    CbvDesc.BufferLocation = MVPBuffer->MainBuffer->GetGPUVirtualAddress();
    CbvDesc.SizeInBytes = AllocatedSize;
    Renderer->Device->CreateConstantBufferView(&CbvDesc, SrvHandle);
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
void CreateShaderBindingTable(dx12_renderer* Renderer) 
{ 
    // The SBT helper class collects calls to Add*Program. If called several 
    // times, the helper must be emptied before re-adding shaders. 
    Renderer->Raytracing->SbtHelper.Reset(); 
    // The pointer to the beginning of the heap is the only parameter required by 
    // shaders without root parameters 
    D3D12_GPU_DESCRIPTOR_HANDLE SrvUavHeapHandle = Renderer->Raytracing->SrvUavHeap->GetGPUDescriptorHandleForHeapStart();
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
    Renderer->Raytracing->SbtHelper.AddRayGenerationProgram(L"RayGen", {HeapPointer}); 
    
    // The miss and hit shaders do not access any external resources: instead they 
    // communicate their results through the ray payload 
    Renderer->Raytracing->SbtHelper.AddMissProgram(L"Miss", {}); 
    
    // Adding the triangle hit shader 
    std::vector<void*> BuffersVirtualAddresses;
    BuffersVirtualAddresses.reserve(Renderer->VertexBuffers.Count);
    for(uint32_t i = 0; i < Renderer->VertexBuffers.Count; ++i)
    {
        BuffersVirtualAddresses.push_back((void*)(Renderer->VertexBuffers.Data[i].MainBuffer->GetGPUVirtualAddress()));
    }
    Renderer->Raytracing->SbtHelper.AddHitGroup(L"HitGroup", BuffersVirtualAddresses);
    
    // Create the SBT on the upload heap
    uint32_t SbtSize = 0;
    SbtSize = Renderer->Raytracing->SbtHelper.ComputeSBTSize();
    Renderer->Raytracing->SbtStorage = nv_helpers_dx12::CreateBuffer(Renderer->Device.Get(), SbtSize,
                                                                     D3D12_RESOURCE_FLAG_NONE, 
                                                                     D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                     nv_helpers_dx12::kUploadHeapProps);
    Renderer->Raytracing->SbtHelper.Generate(Renderer->Raytracing->SbtStorage.Get(), Renderer->Raytracing->StateObjectProps.Get());
}

