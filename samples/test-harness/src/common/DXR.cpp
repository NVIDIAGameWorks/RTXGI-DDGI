/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Common.h"
#include "Shaders.h"

#include <rtxgi/ddgi/DDGIVolume.h>

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include <pix3.h>
#endif

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
* Create the ray tracing output buffers.
*/
bool CreateRTOutput(D3D12Info &d3d, D3D12Resources &resources)
{
    // Describe the RT output resources (GBuffer textures)
    D3D12_RESOURCE_DESC desc = {};
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    // Create the GBufferA resource
    HRESULT hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTGBufferA));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTGBufferA->SetName(L"RT GBufferA Buffer");
#endif

    // Create the GBufferB resource
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTGBufferB));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTGBufferB->SetName(L"RT GBufferB Buffer");
#endif

    // Create the GBufferC resource
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTGBufferC));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTGBufferC->SetName(L"RT GBufferC Buffer");
#endif

    // Create the GBufferD resource
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTGBufferD));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTGBufferD->SetName(L"RT GBufferD Buffer");
#endif

    // Create the RTAO Raw resource
    desc.Format = DXGI_FORMAT_R8_UNORM;

    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTAORaw));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTAORaw->SetName(L"RTAO Raw");
#endif

    // Create the RTAO Filtered resource
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTAOFiltered));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTAOFiltered->SetName(L"RTAO Filtered");
#endif

    // Create the UAVs on the descriptor heap
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    handle.ptr += (handleIncrement * 3);        // RTGBufferA is 4th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTGBufferA, nullptr, &uavDesc, handle); 

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    handle.ptr += handleIncrement;              // RTGBufferB is 5th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTGBufferB, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // RTGBufferC is 6th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTGBufferC, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // RTGBufferD is 7th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTGBufferD, nullptr, &uavDesc, handle);

    // AO Resources
    uavDesc.Format = DXGI_FORMAT_R8_UNORM;

    handle.ptr += handleIncrement;              // RTAORaw is 8th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTAORaw, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // RTAOFiltered is 9th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTAOFiltered, nullptr, &uavDesc, handle);

    return true;
}

/**
 * Create the path tracing output and accumulation buffers.
 */
bool CreatePTOutput(D3D12Info &d3d, D3D12Resources &resources)
{
    // Describe the PT output buffer
    // Initialize as a copy source, since we will copy this buffer's contents to the back buffer
    D3D12_RESOURCE_DESC desc = {};
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    // Create the PT output buffer
    HRESULT hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&resources.PTOutput));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.PTOutput->SetName(L"PT Output Buffer");
#endif

    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    
    // Create the PT accumulation buffer
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.PTAccumulation));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.PTAccumulation->SetName(L"PT Accumulation Buffer");
#endif

    // Create the UAVs on the descriptor heap
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    handle.ptr += (handleIncrement * 10);        // PTOutput is 11th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.PTOutput, nullptr, &uavDesc, handle);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    handle.ptr += handleIncrement;              // PTAccumluation is 12th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.PTAccumulation, nullptr, &uavDesc, handle);

    return true;
}

/**
* Create a bottom level acceleration structure for the scene (Cornell Box or loaded scene).
*/
bool CreateBLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources)
{
    // Describe the geometry that goes in the bottom acceleration structures
    vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
    for (UINT geometryIndex = 0; geometryIndex < resources.vertexBuffers.size(); geometryIndex++)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
        desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        desc.Triangles.VertexBuffer.StartAddress = resources.vertexBuffers[geometryIndex]->GetGPUVirtualAddress();
        desc.Triangles.VertexBuffer.StrideInBytes = resources.vertexBufferViews[geometryIndex].StrideInBytes;
        desc.Triangles.VertexCount = (resources.vertexBufferViews[geometryIndex].SizeInBytes / resources.vertexBufferViews[geometryIndex].StrideInBytes);
        desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        desc.Triangles.IndexBuffer = resources.indexBuffers[geometryIndex]->GetGPUVirtualAddress();
        desc.Triangles.IndexFormat = resources.indexBufferViews[geometryIndex].Format;
        desc.Triangles.IndexCount = (resources.indexBufferViews[geometryIndex].SizeInBytes / sizeof(UINT));
        desc.Triangles.Transform3x4 = 0;
        desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        geometryDescs.push_back(desc);
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the BLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.pGeometryDescs = geometryDescs.data();
    ASInputs.NumDescs = (UINT)geometryDescs.size();
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

    // Create the BLAS scratch buffer
    D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.BLAS.pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.BLAS.pScratch->SetName(L"DXR BLAS Scratch");
#endif

    // Create the BLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.BLAS.pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.BLAS.pResult->SetName(L"DXR BLAS");
#endif

    // Describe and build the bottom level acceleration structure
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = dxr.BLAS.pScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = dxr.BLAS.pResult->GetGPUVirtualAddress();

    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the BLAS build to complete
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.BLAS.pResult;

    d3d.cmdList->ResourceBarrier(1, &barrier);

    return true;
}

/**
* Create a bottom level acceleration structure for the probe visualization spheres.
*/
bool CreateProbeBLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = resources.sphereVertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = resources.sphereVertexBufferView.StrideInBytes;
    geometryDesc.Triangles.VertexCount = (resources.sphereVertexBufferView.SizeInBytes / resources.sphereVertexBufferView.StrideInBytes);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexBuffer = resources.sphereIndexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = resources.sphereIndexBufferView.Format;
    geometryDesc.Triangles.IndexCount = (resources.sphereIndexBufferView.SizeInBytes / sizeof(UINT));
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the BLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.pGeometryDescs = &geometryDesc;
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

    // Create the BLAS scratch buffer
    D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.probeBLAS.pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.probeBLAS.pScratch->SetName(L"DXR Probe BLAS Scratch");
#endif

    // Create the BLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.probeBLAS.pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.probeBLAS.pResult->SetName(L"DXR Probe BLAS");
#endif

    // Describe and build the bottom level acceleration structure
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = dxr.probeBLAS.pScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = dxr.probeBLAS.pResult->GetGPUVirtualAddress();

    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the BLAS build to complete
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.probeBLAS.pResult;

    d3d.cmdList->ResourceBarrier(1, &barrier);

    return true;
}

/**
* Create a top level acceleration structure for the scene (Cornell Box or loaded scene).
*/
bool CreateTLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources)
{
    // Describe the TLAS geometry instance for the scene (Cornell Box or loaded scene)
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.AccelerationStructure = dxr.BLAS.pResult->GetGPUVirtualAddress();
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
    instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    
    // Create the TLAS instance buffer
    D3D12BufferCreateInfo instanceBufferInfo(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, instanceBufferInfo, &dxr.TLAS.pInstanceDesc)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.TLAS.pInstanceDesc->SetName(L"DXR TLAS Instances");
#endif

    // Copy the instance data to the buffer
    UINT8* pData;
    dxr.TLAS.pInstanceDesc->Map(0, nullptr, (void**)&pData);
    memcpy(pData, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    dxr.TLAS.pInstanceDesc->Unmap(0, nullptr);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = dxr.TLAS.pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.TLAS.pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.TLAS.pScratch->SetName(L"DXR TLAS Scratch");
#endif

    // Create the TLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.TLAS.pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.TLAS.pResult->SetName(L"DXR TLAS");
#endif

    // Describe and build the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = dxr.TLAS.pScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = dxr.TLAS.pResult->GetGPUVirtualAddress();

    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the TLAS build to complete
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.TLAS.pResult;

    d3d.cmdList->ResourceBarrier(1, &barrier);

    return true;
}

/**
* Create the global DXR root signature.
*/
bool CreateGlobalRootSignature(D3D12Info &d3d, DXRInfo &dxr)
{
    D3D12_DESCRIPTOR_RANGE ranges[6];
    UINT rangeIndex = 0;

    // Camera, material, and light constant buffers (b1, b2, b3)
    ranges[rangeIndex].BaseShaderRegister = 1;
    ranges[rangeIndex].NumDescriptors = 3;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 0;
    rangeIndex++;

    // RTGBufferA, RTGBufferB, RTGBufferC, RTBufferD, RTAORaw, RTAOFiltered
    // TLAS Instances, PTOutput, PTAccumulation (u0, u1, u2, u3, u4, u5, u6, u7, u8)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 9;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 3;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------

    // RTXGI DDGIVolume RT probe radiance (u0, space1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 12;
    rangeIndex++;

    // RTXGI DDGIVolume probe offsets, probe states (u3, u4, space1)
    ranges[rangeIndex].BaseShaderRegister = 3;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 15;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------

    // RTXGI DDGIVolume probe irradiance and distance SRV (t0, t1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 17;
    rangeIndex++;

    // Blue Noise RGB SRV (t5)
    ranges[rangeIndex].BaseShaderRegister = 5;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 19;

    // Samplers (s0, s1)
    D3D12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.BaseShaderRegister = 0;
    samplerRange.NumDescriptors = 2;
    samplerRange.RegisterSpace = 0;
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.OffsetInDescriptorsFromTableStart = 0;

    // Volume Constant Buffer (b1, space1)
    D3D12_ROOT_PARAMETER param0;
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.Descriptor.RegisterSpace = 1;
    param0.Descriptor.ShaderRegister = 1;

    // TLAS SRV
    D3D12_ROOT_PARAMETER param1 = {};
    param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param1.Descriptor.RegisterSpace = 0;
    param1.Descriptor.ShaderRegister = 2;

    // CBV/SRV/UAV descriptor table
    D3D12_ROOT_PARAMETER param2 = {};
    param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param2.DescriptorTable.NumDescriptorRanges = _countof(ranges);
    param2.DescriptorTable.pDescriptorRanges = ranges;

    // Sampler descriptor table
    D3D12_ROOT_PARAMETER param3 = {};
    param3.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param3.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param3.DescriptorTable.NumDescriptorRanges = 1;
    param3.DescriptorTable.pDescriptorRanges = &samplerRange;

    // Noise Root Constants (b4)
    D3D12_ROOT_PARAMETER param4 = {};
    param4.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param4.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param4.Constants.Num32BitValues = 8;
    param4.Constants.ShaderRegister = 4;
    param4.Constants.RegisterSpace = 0;

    // Vis TLAS Update root constants (b5)
    D3D12_ROOT_PARAMETER param5 = {};
    param5.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param5.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param5.Constants.Num32BitValues = 4;
    param5.Constants.ShaderRegister = 5;
    param5.Constants.RegisterSpace = 0;

    // Path Tracer Root Constants (b6)
    D3D12_ROOT_PARAMETER param6 = {};
    param6.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param6.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param6.Constants.Num32BitValues = 4;
    param6.Constants.ShaderRegister = 6;
    param6.Constants.RegisterSpace = 0;

    D3D12_ROOT_PARAMETER rootParams[7] = { param0, param1, param2, param3, param4, param5, param6 };

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // Create the global root signature
    dxr.globalRootSig = D3D12::CreateRootSignature(d3d, desc);
    if (dxr.globalRootSig == nullptr) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.globalRootSig->SetName(L"DXR Global Root Signature");
#endif

    return true;
}

/**
* Load and create the DXR Ray Generation programs.
*/
bool CreateRayGenPrograms(D3D12Info &d3d, DXRInfo &dxr, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring path = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    std::wstring file;
    
    // Load and compile the probe ray trace ray generation shader
    file = path + L"shaders\\ProbeTraceRGS.hlsl";
    dxr.probeRGS = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    if (!Shaders::Compile(shaderCompiler, dxr.probeRGS)) return false;

    // Load and compile the primary ray trace ray generation shader
    file = path + L"shaders\\PrimaryTraceRGS.hlsl";
    dxr.primaryRGS = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    if (!Shaders::Compile(shaderCompiler, dxr.primaryRGS)) return false;

    // Load and compile the ambient occlusion ray generation shader
    file = path + L"shaders\\AOTraceRGS.hlsl";
    dxr.ambientOcclusionRGS = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    if (!Shaders::Compile(shaderCompiler, dxr.ambientOcclusionRGS)) return false;

    // Load and compile the probe visualization ray trace ray generation shader
    file = path + L"shaders\\VisDDGIProbes.hlsl";
    dxr.probeVisRGS = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    if (!Shaders::Compile(shaderCompiler, dxr.probeVisRGS)) return false;

    // Load and compile the path tracing ray trace ray generation shader
    file = path + L"shaders\\PathTraceRGS.hlsl";
    dxr.pathTraceRGS = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    return Shaders::Compile(shaderCompiler, dxr.pathTraceRGS);
}

/**
* Load and create the DXR Miss program.
*/
bool CreateMissProgram(D3D12Info &d3d, DXRInfo &dxr, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\Miss.hlsl");

    // Load and compile the miss shader
    dxr.miss = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    return Shaders::Compile(shaderCompiler, dxr.miss);
}

/**
* Load and create the DXR Closest Hit program and root signature.
*/
bool CreateClosestHitProgram(D3D12Info &d3d, DXRInfo &dxr, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\ClosestHit.hlsl");

    // Load and compile the Closest Hit shader
    dxr.hit = HitProgram(L"Hit");
    dxr.hit.chs = RtProgram(D3D12ShaderInfo(file.c_str(), L"", L"lib_6_3"));
    if (!Shaders::Compile(shaderCompiler, dxr.hit.chs)) return false;

    // Index buffer SRV (t3)
    D3D12_ROOT_PARAMETER param0 = {};
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.Descriptor.RegisterSpace = 0;
    param0.Descriptor.ShaderRegister = 3;

    // Vertex buffer SRV (t4)
    D3D12_ROOT_PARAMETER param1 = {};
    param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param1.Descriptor.RegisterSpace = 0;
    param1.Descriptor.ShaderRegister = 4;

    // Per-mesh material data (b2, space2)
    D3D12_ROOT_PARAMETER param2 = {};
    param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param2.Constants.Num32BitValues = 4;
    param2.Constants.RegisterSpace = 2;
    param2.Constants.ShaderRegister = 2;

    D3D12_ROOT_PARAMETER rootParams[3] = { param0, param1, param2 };

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParams);
    rootDesc.pParameters = rootParams;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    // Create the root signature
    dxr.hit.chs.pRootSignature = D3D12::CreateRootSignature(d3d, rootDesc);
    if (dxr.hit.chs.pRootSignature == nullptr) return false;

#if RTXGI_NAME_D3D_OBJECTS
    dxr.hit.chs.pRootSignature->SetName(L"DXR CHS Local Root Signature");
#endif

    return true;
}

/**
* Load and compile the compute shader that updates the visualization TLAS instances.
*/
bool CreateVisUpdateTLASProgram(D3D12Info &d3d, DXRInfo &dxr, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\VisUpdateTLASCS.hlsl");

    D3D12ShaderInfo shader;
    shader.filename = file.c_str();
    shader.entryPoint = L"VisUpdateTLASCS";
    shader.targetProfile = L"cs_6_0";
    if (!Shaders::Compile(shaderCompiler, shader)) return false;

    dxr.visUpdateTLASCS = shader.bytecode;

    return true;
}

/**
* Create the compute PSO for the Vis TLAS update pass.
*/
bool CreateVisUpdateTLASPSO(D3D12Info &d3d, DXRInfo &dxr)
{
    return D3D12::CreateComputePSO(d3d.device, dxr.globalRootSig, dxr.visUpdateTLASCS, &dxr.visUpdateTLASPSO);
}

/**
* Create the DXR pipeline state object.
*/
bool CreatePipelineStateObjects(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources)
{
    // Need 14 subobjects:
    // 1 for probe trace RGS program
    // 1 for primary trace RGS program
    // 1 for ambient occlusion trace RGS program
    // 1 for probe vis trace RGS program
    // 1 for path trace RGS program
    // 1 for Miss program
    // 1 for CHS program
    // 1 for Hit Group
    // 2 for CHS Local Root Signature (root-signature and association)
    // 2 for Shader Config (config and association)
    // 1 for Global Root Signature
    // 1 for Pipeline Config    
    UINT index = 0;
    vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.resize(14);

    // Add state subobject for the probe ray trace RGS
    D3D12_EXPORT_DESC probeRGSExportDesc = {};
    probeRGSExportDesc.Name = L"ProbeRGS";
    probeRGSExportDesc.ExportToRename = L"RayGen";
    probeRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC probeRGSLibDesc = {};
    probeRGSLibDesc.DXILLibrary.BytecodeLength = dxr.probeRGS.info.bytecode->GetBufferSize();
    probeRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.probeRGS.info.bytecode->GetBufferPointer();
    probeRGSLibDesc.NumExports = 1;
    probeRGSLibDesc.pExports = &probeRGSExportDesc;

    D3D12_STATE_SUBOBJECT probeRGS = {};
    probeRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    probeRGS.pDesc = &probeRGSLibDesc;

    subobjects[index++] = probeRGS;

    // Add state subobject for the primary ray trace RGS
    D3D12_EXPORT_DESC primaryRGSExportDesc = {};
    primaryRGSExportDesc.Name = L"PrimaryRGS";
    primaryRGSExportDesc.ExportToRename = L"RayGen";
    primaryRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC primaryRGSLibDesc = {};
    primaryRGSLibDesc.DXILLibrary.BytecodeLength = dxr.primaryRGS.info.bytecode->GetBufferSize();
    primaryRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.primaryRGS.info.bytecode->GetBufferPointer();
    primaryRGSLibDesc.NumExports = 1;
    primaryRGSLibDesc.pExports = &primaryRGSExportDesc;

    D3D12_STATE_SUBOBJECT primaryRGS = {};
    primaryRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    primaryRGS.pDesc = &primaryRGSLibDesc;

    subobjects[index++] = primaryRGS;

    // Add state subobject for the AO ray trace RGS
    D3D12_EXPORT_DESC AORGSExportDesc = {};
    AORGSExportDesc.Name = L"AORGS";
    AORGSExportDesc.ExportToRename = L"RayGen";
    AORGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC AORGSLibDesc = {};
    AORGSLibDesc.DXILLibrary.BytecodeLength = dxr.ambientOcclusionRGS.info.bytecode->GetBufferSize();
    AORGSLibDesc.DXILLibrary.pShaderBytecode = dxr.ambientOcclusionRGS.info.bytecode->GetBufferPointer();
    AORGSLibDesc.NumExports = 1;
    AORGSLibDesc.pExports = &AORGSExportDesc;

    D3D12_STATE_SUBOBJECT AORGS = {};
    AORGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    AORGS.pDesc = &AORGSLibDesc;

    subobjects[index++] = AORGS;

    // Add state subobject for the probe vis ray trace RGS
    D3D12_EXPORT_DESC probeVisRGSExportDesc = {};
    probeVisRGSExportDesc.Name = L"ProbeVisRGS";
    probeVisRGSExportDesc.ExportToRename = L"RayGen";
    probeVisRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC probeVisRGSLibDesc = {};
    probeVisRGSLibDesc.DXILLibrary.BytecodeLength = dxr.probeVisRGS.info.bytecode->GetBufferSize();
    probeVisRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.probeVisRGS.info.bytecode->GetBufferPointer();
    probeVisRGSLibDesc.NumExports = 1;
    probeVisRGSLibDesc.pExports = &probeVisRGSExportDesc;

    D3D12_STATE_SUBOBJECT probeVisRGS = {};
    probeVisRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    probeVisRGS.pDesc = &probeVisRGSLibDesc;

    subobjects[index++] = probeVisRGS;

    // Add state subobject for the path trace RGS
    D3D12_EXPORT_DESC pathTraceRGSExportDesc = {};
    pathTraceRGSExportDesc.Name = L"PathTraceRGS";
    pathTraceRGSExportDesc.ExportToRename = L"RayGen";
    pathTraceRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC pathTraceRGSLibDesc = {};
    pathTraceRGSLibDesc.DXILLibrary.BytecodeLength = dxr.pathTraceRGS.info.bytecode->GetBufferSize();
    pathTraceRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.pathTraceRGS.info.bytecode->GetBufferPointer();
    pathTraceRGSLibDesc.NumExports = 1;
    pathTraceRGSLibDesc.pExports = &pathTraceRGSExportDesc;

    D3D12_STATE_SUBOBJECT pathTraceRGS = {};
    pathTraceRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    pathTraceRGS.pDesc = &pathTraceRGSLibDesc;

    subobjects[index++] = pathTraceRGS;

    // Add state subobject for the Miss shader
    D3D12_EXPORT_DESC msExportDesc = {};
    msExportDesc.Name = L"Miss";
    msExportDesc.ExportToRename = L"Miss";
    msExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC msLibDesc = {};
    msLibDesc.DXILLibrary.BytecodeLength = dxr.miss.info.bytecode->GetBufferSize();
    msLibDesc.DXILLibrary.pShaderBytecode = dxr.miss.info.bytecode->GetBufferPointer();
    msLibDesc.NumExports = 1;
    msLibDesc.pExports = &msExportDesc;

    D3D12_STATE_SUBOBJECT ms = {};
    ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    ms.pDesc = &msLibDesc;

    subobjects[index++] = ms;

    // Add state subobject for the Closest Hit shader
    D3D12_EXPORT_DESC chsExportDesc = {};
    chsExportDesc.Name = L"ClosestHit";
    if (resources.isGeometryProcedural)
    {
        chsExportDesc.ExportToRename = L"ClosestHitManual";
    }
    else
    {
        chsExportDesc.ExportToRename = L"ClosestHit";
    }
    chsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC chsLibDesc = {};
    chsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.chs.info.bytecode->GetBufferSize();
    chsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.chs.info.bytecode->GetBufferPointer();
    chsLibDesc.NumExports = 1;
    chsLibDesc.pExports = &chsExportDesc;

    D3D12_STATE_SUBOBJECT chs = {};
    chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    chs.pDesc = &chsLibDesc;

    subobjects[index++] = chs;

    // Add a state subobject for the hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";
    hitGroupDesc.HitGroupExport = L"HitGroup";

    D3D12_STATE_SUBOBJECT hitGroup = {};
    hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroup.pDesc = &hitGroupDesc;

    subobjects[index++] = hitGroup;

    // Add a state subobject for the shader payload configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
    shaderDesc.MaxPayloadSizeInBytes = sizeof(float) * 12;  //sizeof(PayloadData)
    shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

    D3D12_STATE_SUBOBJECT shaderConfigObject = {};
    shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigObject.pDesc = &shaderDesc;

    subobjects[index++] = shaderConfigObject;

    // Create a list of the shader export names that use the payload
    const WCHAR* shaderExports[] = { L"ProbeRGS", L"PrimaryRGS", L"AORGS", L"ProbeVisRGS", L"PathTraceRGS", L"Miss", L"HitGroup" };

    // Add a state subobject for the association between shaders and the payload
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
    shaderPayloadAssociation.NumExports = _countof(shaderExports);
    shaderPayloadAssociation.pExports = shaderExports;
    shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

    D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
    shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

    subobjects[index++] = shaderPayloadAssociationObject;

    // Add a state subobject for the chs local root signature 
    D3D12_STATE_SUBOBJECT chsRootSigObject = {};
    chsRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    chsRootSigObject.pDesc = &dxr.hit.chs.pRootSignature;

    subobjects[index++] = chsRootSigObject;

    // Create a list of the shader export names that use the local root signature
    const WCHAR* rootSigExports[] = { L"HitGroup" };

    // Add a state subobject for the association between the CHS and the local root signature
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION chsRootSigAssociation = {};
    chsRootSigAssociation.NumExports = _countof(rootSigExports);
    chsRootSigAssociation.pExports = rootSigExports;
    chsRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

    D3D12_STATE_SUBOBJECT chsRootSigAssociationObject = {};
    chsRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    chsRootSigAssociationObject.pDesc = &chsRootSigAssociation;

    subobjects[index++] = chsRootSigAssociationObject;

    D3D12_STATE_SUBOBJECT globalRootSig;
    globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSig.pDesc = &dxr.globalRootSig;

    subobjects[index++] = globalRootSig;

    // Add a state subobject for the ray tracing pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
    pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigObject.pDesc = &pipelineConfig;

    subobjects[index++] = pipelineConfigObject;

    // Describe the Ray Tracing Pipeline State Object
    D3D12_STATE_OBJECT_DESC pipelineDesc = {};
    pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
    pipelineDesc.pSubobjects = subobjects.data();

    // Create the RT Pipeline State Object (RTPSO)
    HRESULT hr = d3d.device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&dxr.rtpso));
    if (FAILED(hr)) return false;

    // Get the RTPSO properties
    hr = dxr.rtpso->QueryInterface(IID_PPV_ARGS(&dxr.rtpsoInfo));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.rtpso->SetName(L"DXR Pipeline State Object");
#endif

    return true;
}

/**
* Create the DXR shader table.
*/
bool CreateShaderTable(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources)
{
    /*
    The Shader Table layout is as follows:
        Entry 0: Probe Ray Trace Ray Generation Shader
        Entry 1: Primary Ray Trace Ray Generation Shader
        Entry 2: Ambient Occlusion Ray Generation Shader
        Entry 3: Probe Vis Ray Trace Ray Generation Shader
        Entry 4: Path Trace Ray Generation Shader
        Entry 5: Miss Shader
        Entry 6+: Hit Groups (Closest Hit Shaders)
    All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
    The CHS requires the largest entry:
        32 bytes for the program identifier
      +  8 bytes for a index buffer VA
      +  8 bytes for a vertex buffer VA
      + 16 bytes for material color (float3 + padding)
      = 64 bytes ->> aligns to 64 bytes
    The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
    */

    UINT shaderIdSize = 32;
    UINT shaderTableSize = 0;

    dxr.shaderTableRecordSize = shaderIdSize;
    dxr.shaderTableRecordSize += 8;                     // index buffer GPUVA
    dxr.shaderTableRecordSize += 8;                     // vertex buffer GPUVA
    dxr.shaderTableRecordSize += 16;                    // material color (float3)
    dxr.shaderTableRecordSize = RTXGI_ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.shaderTableRecordSize);

    // 7 default shader records in the table + a record for each mesh
    shaderTableSize = (dxr.shaderTableRecordSize * (7 + (UINT)resources.vertexBuffers.size()));
    shaderTableSize = RTXGI_ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);

    // Create the shader table buffer
    D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.shaderTable)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.shaderTable->SetName(L"DXR Shader Table");
#endif

    // Map the buffer
    UINT8* pData;
    HRESULT hr = dxr.shaderTable->Map(0, nullptr, (void**)&pData);
    if (FAILED(hr)) return false;

    // Shader Record 0 - Probe Ray Trace Ray Generation program (no local root parameter data)
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"ProbeRGS"), shaderIdSize);

    // Shader Record 1 - Primary Ray Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"PrimaryRGS"), shaderIdSize);

    // Shader Record 2 - Ambient Occlusion Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"AORGS"), shaderIdSize);

    // Shader Record 3 - Probe Vis Ray Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"ProbeVisRGS"), shaderIdSize);
    
    // Shader Record 4 - Path Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"PathTraceRGS"), shaderIdSize);

    // Shader Record 5 - Miss program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"Miss"), shaderIdSize);

    // Shader Record 6 - Closest Hit program (visualization hits) and local root parameter data
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);    // all CHS use the same shader program
    *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize) = resources.sphereIndexBuffer->GetGPUVirtualAddress();
    *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize + 8) = resources.sphereVertexBuffer->GetGPUVirtualAddress();

    // Shader Records 7+ - Closest Hit program (probe and primary hits) and local root parameter data
    vector<Material> materials = resources.geometry.materials;
    for (UINT resourceIndex = 0; resourceIndex < resources.vertexBuffers.size(); resourceIndex++)
    {
        pData += dxr.shaderTableRecordSize;
        memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);   // all CHS use the same shader program
        *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize) = resources.indexBuffers[resourceIndex]->GetGPUVirtualAddress();
        *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize + 8) = resources.vertexBuffers[resourceIndex]->GetGPUVirtualAddress();
        if(!resources.isGeometryProcedural)
        {
            *reinterpret_cast<XMFLOAT3*>(pData + shaderIdSize + 8 + 8) = materials[resources.geometry.meshes[resourceIndex].materialIndex].color;
        }
    }

    // Unmap
    dxr.shaderTable->Unmap(0, nullptr);

    return true;
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

namespace DXR
{

/*
* Initialize DXR.
*/
bool Initialize(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler)
{
    if (!CreateRTOutput(d3d, resources)) return false;
    if (!CreatePTOutput(d3d, resources)) return false;
    if (!CreateBLAS(d3d, dxr, resources)) return false;
    if (!CreateProbeBLAS(d3d, dxr, resources)) return false;
    if (!CreateTLAS(d3d, dxr, resources)) return false;
    if (!CreateGlobalRootSignature(d3d, dxr)) return false;
    if (!CreateRayGenPrograms(d3d, dxr, shaderCompiler)) return false;
    if (!CreateMissProgram(d3d, dxr, shaderCompiler)) return false;
    if (!CreateClosestHitProgram(d3d, dxr, shaderCompiler)) return false;
    if (!CreateVisUpdateTLASProgram(d3d, dxr, shaderCompiler)) return false;
    if (!CreateVisUpdateTLASPSO(d3d, dxr)) return false;
    if (!CreatePipelineStateObjects(d3d, dxr, resources)) return false;
    if (!CreateShaderTable(d3d, dxr, resources)) return false;
    return true;
}

/**
* Create the top level acceleration structure for the probe visualization.
*/
bool CreateVisTLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, int numProbes)
{
    // Release the visualization TLAS, if one already exists
    dxr.visTLAS.Release();

    // Create the TLAS instance buffer
    UINT64 size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numProbes;
    D3D12BufferCreateInfo instanceBufferInfo(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, instanceBufferInfo, &dxr.visTLAS.pInstanceDesc)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLAS.pInstanceDesc->SetName(L"DXR Vis TLAS Instances");
#endif

    // Create the view
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the Vis TLAS instance data UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC visTLASInstanceDataUAVDesc = {};
    visTLASInstanceDataUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    visTLASInstanceDataUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    visTLASInstanceDataUAVDesc.Buffer.FirstElement = 0;
    visTLASInstanceDataUAVDesc.Buffer.NumElements = numProbes;
    visTLASInstanceDataUAVDesc.Buffer.StructureByteStride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    visTLASInstanceDataUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    visTLASInstanceDataUAVDesc.Buffer.CounterOffsetInBytes = 0;

    handle.ptr += (resources.cbvSrvUavDescSize * 9);        // Vis TLAS instances are 10th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(dxr.visTLAS.pInstanceDesc, nullptr, &visTLASInstanceDataUAVDesc, handle);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = dxr.visTLAS.pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = numProbes;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    dxr.visTlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.visTLAS.pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLAS.pScratch->SetName(L"DXR Vis TLAS Scratch");
#endif

    // Create the TLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.visTLAS.pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLAS.pResult->SetName(L"DXR Vis TLAS");
#endif

    // Write instance descriptions and build the acceleration structure
    UpdateVisTLAS(d3d, dxr, resources, numProbes, 1.f);

    return true;
}

/**
* Update the top level acceleration structure instances for the visualization
* probes and rebuild the TLAS.
*
* Currently called every frame to reflect changes from the probe position preprocess. If the
* number of probes changes, the caller is responsible for freeing the TLAS buffers
* and calling CreateVisTLAS() to reallocate them before calling UpdateVisTLAS() again.
*/
bool UpdateVisTLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, int numProbes, float probeRadius)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(255, 255, 0), "Update Vis TLAS");
#endif

    // Transition the instance buffer to unordered access
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxr.visTLAS.pInstanceDesc;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // Wait for the transition to finish
    d3d.cmdList->ResourceBarrier(1, &barrier);

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    UINT64 blasHandle = dxr.probeBLAS.pResult->GetGPUVirtualAddress();

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set constant buffer
    UINT64 offset = d3d.frameIndex * rtxgi::GetDDGIVolumeConstantBufferSize();
    d3d.cmdList->SetComputeRootConstantBufferView(0, resources.volumeCB->GetGPUVirtualAddress() + offset);

    // Set descriptor tables
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set root constants
    d3d.cmdList->SetComputeRoot32BitConstants(5, 2, &blasHandle, 0);
    d3d.cmdList->SetComputeRoot32BitConstant(5, *(UINT *)&probeRadius, 2);

    // Set the compute PSO and dispatch
    d3d.cmdList->SetPipelineState(dxr.visUpdateTLASPSO);
    d3d.cmdList->Dispatch(numProbes, 1, 1);

    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.visTLAS.pInstanceDesc;

    // Wait for the compute pass to finish
    d3d.cmdList->ResourceBarrier(1, &barrier);

    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxr.visTLAS.pInstanceDesc;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // Wait for the transition to finish
    d3d.cmdList->ResourceBarrier(1, &barrier);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = dxr.visTLAS.pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = numProbes;
    ASInputs.Flags = buildFlags;

    // Describe and build the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = dxr.visTLAS.pScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = dxr.visTLAS.pResult->GetGPUVirtualAddress();

    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the TLAS build to complete
    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.visTLAS.pResult;

    d3d.cmdList->ResourceBarrier(1, &barrier);

    return true;
}

/*
* Free DXR resources.
*/
void Cleanup(DXRInfo &dxr, D3D12Resources &resources)
{
    RTXGI_SAFE_RELEASE(resources.RTGBufferA);
    RTXGI_SAFE_RELEASE(resources.RTGBufferB);
    RTXGI_SAFE_RELEASE(resources.RTGBufferC);
    RTXGI_SAFE_RELEASE(resources.RTGBufferD);
    RTXGI_SAFE_RELEASE(resources.RTAORaw);
    RTXGI_SAFE_RELEASE(resources.RTAOFiltered);
    RTXGI_SAFE_RELEASE(resources.PTOutput);
    RTXGI_SAFE_RELEASE(resources.PTAccumulation);
    RTXGI_SAFE_RELEASE(dxr.rtpso);
    RTXGI_SAFE_RELEASE(dxr.rtpsoInfo);
    RTXGI_SAFE_RELEASE(dxr.shaderTable);
    RTXGI_SAFE_RELEASE(dxr.globalRootSig);
    RTXGI_SAFE_RELEASE(dxr.visUpdateTLASPSO);

    dxr.probeRGS.Release();
    dxr.primaryRGS.Release();
    dxr.ambientOcclusionRGS.Release();
    dxr.probeVisRGS.Release();
    dxr.pathTraceRGS.Release();
    dxr.miss.Release();
    dxr.hit.Release();

    dxr.BLAS.Release();
    dxr.probeBLAS.Release();
    dxr.TLAS.Release();
    dxr.visTLAS.Release();
}

}
