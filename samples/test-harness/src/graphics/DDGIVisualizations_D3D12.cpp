/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/DDGIVisualizations.h"

#include "Geometry.h"

using namespace rtxgi;
using namespace rtxgi::d3d12;

using namespace Graphics::DDGI::Visualizations;

namespace Graphics
{
    namespace D3D12
    {
        namespace DDGI
        {
            namespace Visualizations
            {
                namespace DescriptorHeapOffsets
                {
                    const int CBV_CAMERA = 0;                               //   0:  1 CBV for the camera constant buffer
                    const int STB_DDGI_VOLUMES = CBV_CAMERA + 1;            //   1:  1 SRV for the DDGIVolume constants structured buffer
                    const int STB_INSTANCES = STB_DDGI_VOLUMES + 1;         //   2:  1 UAV for the TLAS instance descriptors structured buffer
                    const int UAV_GBUFFERA = STB_INSTANCES + 1;             //   3:  1 UAV for the GBufferA RWTexture
                    const int UAV_GBUFFERB = UAV_GBUFFERA + 1;              //   4:  1 UAV for the GBufferB RWTexture
                    const int SRV_INDICES = UAV_GBUFFERB + 1;               //   5:  1 SRV for Probe Sphere Mesh Index Buffers
                    const int SRV_VERTICES = SRV_INDICES + 1;               //   6:  1 SRV for Probe Sphere Mesh Vertex Buffers
                    const int SRV_TEX2D_START = SRV_VERTICES + 1;           //   7:  16 SRV for DDGIVolume Textures, 4 SRV per DDGIVolume (Ray Data, Irradiance, Distance, Probe Data)
                };

                //----------------------------------------------------------------------------------------------------------
                // Private Functions
                //----------------------------------------------------------------------------------------------------------

                void UpdateDescriptorHeap(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
                {
                    // Add the camera CBV to the descriptor heap
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, Scenes::Camera::GetGPUDataSize());
                    cbvDesc.BufferLocation = d3dResources.cameraCB->GetGPUVirtualAddress();

                    D3D12_CPU_DESCRIPTOR_HANDLE handle;
                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::CBV_CAMERA * resources.srvDescHeapEntrySize);
                    d3d.device->CreateConstantBufferView(&cbvDesc, handle);

                    // Add the constants structured buffer SRV to the descriptor heap
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    srvDesc.Buffer.NumElements = static_cast<UINT>(resources.volumes->size());
                    srvDesc.Buffer.StructureByteStride = sizeof(DDGIVolumeDescGPUPacked);
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_DDGI_VOLUMES * resources.srvDescHeapEntrySize);
                    d3d.device->CreateShaderResourceView(resources.constantsSTB, &srvDesc, handle);

                    // GBufferA (R8G8B8A8_UNORM) texture resource
                    TextureDesc desc = { static_cast<UINT>(d3d.width), static_cast<UINT>(d3d.height), 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };

                    // Add the GBufferA UAV to the descriptor heap
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    uavDesc.Format = desc.format;

                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERA * resources.srvDescHeapEntrySize);
                    d3d.device->CreateUnorderedAccessView(d3dResources.rt.GBufferA, nullptr, &uavDesc, handle);

                    // GBufferB (R32G32B32A32_FLOAT) texture resource
                    desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;

                    // Add the GBufferB UAV to the descriptor heap
                    uavDesc.Format = desc.format;
                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERB * resources.srvDescHeapEntrySize);
                    d3d.device->CreateUnorderedAccessView(d3dResources.rt.GBufferB, nullptr, &uavDesc, handle);

                    // Add the DDGIVolume SRVs to the descriptor heap
                    srvDesc = {};
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    int heapSlot = DescriptorHeapOffsets::SRV_TEX2D_START;
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes->size()); volumeIndex++)
                    {
                        // Get the volume
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Ray Data
                        srvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volume->GetRayDataFormat());
                        handle.ptr = resources.srvDescHeapStart.ptr + (heapSlot * resources.srvDescHeapEntrySize);
                        d3d.device->CreateShaderResourceView(volume->GetProbeRayData(), &srvDesc, handle);
                        heapSlot++;

                        // Irradiance
                        srvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volume->GetIrradianceFormat());
                        handle.ptr = resources.srvDescHeapStart.ptr + (heapSlot * resources.srvDescHeapEntrySize);
                        d3d.device->CreateShaderResourceView(volume->GetProbeIrradiance(), &srvDesc, handle);
                        heapSlot++;

                        // Distance
                        srvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volume->GetDistanceFormat());
                        handle.ptr = resources.srvDescHeapStart.ptr + (heapSlot * resources.srvDescHeapEntrySize);
                        d3d.device->CreateShaderResourceView(volume->GetProbeDistance(), &srvDesc, handle);
                        heapSlot++;

                        // Probe Data
                        srvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volume->GetProbeDataFormat());
                        handle.ptr = resources.srvDescHeapStart.ptr + (heapSlot * resources.srvDescHeapEntrySize);
                        d3d.device->CreateShaderResourceView(volume->GetProbeData(), &srvDesc, handle);

                        heapSlot++;
                    }

                }

                bool UpdateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
                {
                    UINT shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                    // Write shader table records
                    UINT8* pData = nullptr;
                    D3D12_RANGE readRange = {};
                    if (FAILED(resources.shaderTableUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData)))) return false;

                    // Write shader table records for each shader permutation
                    D3D12_GPU_VIRTUAL_ADDRESS address = resources.shaderTable->GetGPUVirtualAddress();

                    // Entry 0: Ray Generation Shader and descriptor heap pointer
                    memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.rgs.exportName.c_str()), shaderIdSize);
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                    resources.shaderTableRGSStartAddress = address;

                    address += resources.shaderTableRecordSize;

                    // Entry 1: Miss Shader
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.miss.exportName.c_str()), shaderIdSize);
                    resources.shaderTableMissTableStartAddress = address;
                    resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                    address += resources.shaderTableMissTableSize;

                    // Entries 2+: Hit Groups and descriptor heap pointers
                    for (UINT hitGroupIndex = 0; hitGroupIndex < static_cast<UINT>(resources.rtShaders.hitGroups.size()); hitGroupIndex++)
                    {
                        pData += resources.shaderTableRecordSize;
                        memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.hitGroups[hitGroupIndex].exportName), shaderIdSize);
                        *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                        *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart();
                    }
                    resources.shaderTableHitGroupTableStartAddress = address;
                    resources.shaderTableHitGroupTableSize = static_cast<UINT>(resources.rtShaders.hitGroups.size()) * resources.shaderTableRecordSize;

                    // Unmap
                    resources.shaderTableUpload->Unmap(0, nullptr);

                    // Schedule a copy of the upload buffer to the device buffer
                    d3d.cmdList->CopyBufferRegion(resources.shaderTable, 0, resources.shaderTableUpload, 0, resources.shaderTableSize);

                    // Transition the default heap resource to generic read after the copy is complete
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = resources.shaderTable;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    return true;
                }

                bool UpdateInstances(Globals& d3d, Resources& resources)
                {
                    // Clear the instances
                    resources.probeInstances.clear();

                    // Gather the probe instances from volumes
                    UINT16 instanceOffset = 0;
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes->size()); volumeIndex++)
                    {
                        // Get the volume
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Skip this volume if its "Show Probes" flag is disabled
                        if (!volume->GetShowProbes()) continue;

                        // Add an instance for each probe
                        for (UINT probeIndex = 0; probeIndex < static_cast<UINT>(volume->GetNumProbes()); probeIndex++)
                        {
                            // Describe the probe instance
                            D3D12_RAYTRACING_INSTANCE_DESC desc = {};
                            desc.InstanceID = instanceOffset;                  // instance offset in first 16 bits
                            desc.InstanceID |= (UINT8)volume->GetIndex() << 16; // volume index in last 8 bits
                            desc.InstanceMask = 0xFF;
                            desc.AccelerationStructure = resources.blas.as->GetGPUVirtualAddress();
                        #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
                        #endif

                            // Initialize transform to identity, instance transforms are updated on the GPU
                            desc.Transform[0][0] = desc.Transform[1][1] = desc.Transform[2][2] = 1.f;

                            resources.probeInstances.push_back(desc);
                        }

                        // Increment the instance offset
                        instanceOffset += volume->GetNumProbes();
                    }

                    // Early out if no volumes want to visualize probes
                    if (resources.probeInstances.size() == 0) return true;

                    // Copy the instance data to the upload buffer
                    UINT8* pData = nullptr;
                    D3D12_RANGE readRange = {};
                    UINT size = static_cast<UINT>(resources.probeInstances.size()) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
                    D3DCHECK(resources.tlas.instancesUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
                    memcpy(pData, resources.probeInstances.data(), size);
                    resources.tlas.instancesUpload->Unmap(0, nullptr);

                    // Schedule a copy of the upload buffer to the device buffer
                    d3d.cmdList->CopyBufferRegion(resources.tlas.instances, 0, resources.tlas.instancesUpload, 0, size);

                    // Transition the default heap resource to generic read after the copy is complete
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = resources.tlas.instances;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    return true;
                }

                bool UpdateTLAS(Globals& d3d, Resources& resources, const Configs::Config& config)
                {
                #ifdef RTXGI_GFX_PERF_MARKERS
                    PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Update DDGI Visualizations TLAS");
                #endif

                    // Update the instances and copy them to the GPU
                    UpdateInstances(d3d, resources);

                    // Early out if no volumes want to visualize probes
                    if (resources.probeInstances.size() == 0) return true;

                    // Transition the instance buffer to unordered access
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = resources.tlas.instances;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                    // Wait for the transition to finish
                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    // Set the descriptor heap
                    ID3D12DescriptorHeap* ppHeaps[] = { resources.srvDescHeap };
                    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                    // Set the root signature and descriptor table
                    d3d.cmdList->SetComputeRootSignature(resources.rootSignature);
                    d3d.cmdList->SetComputeRootDescriptorTable(1, resources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());

                    // Set the compute PSO
                    d3d.cmdList->SetPipelineState(resources.updateTlasPSO);

                    UINT instanceOffset = 0;
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes->size()); volumeIndex++)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Skip this volume if the "Show Probes" flag is disabled
                        if (!volume->GetShowProbes()) continue;

                        // Update constants
                        resources.constants.volumeIndex = volume->GetIndex();
                        resources.constants.instanceOffset = instanceOffset;
                        resources.constants.probeRadius = config.ddgi.volumes[volumeIndex].probeRadius;

                        // Set root constants
                        d3d.cmdList->SetComputeRoot32BitConstants(3, resources.constants.GetNum32BitValues(), resources.constants.GetData(), 0);
                        
                        // Dispatch the compute shader
                        UINT numProbes = static_cast<UINT>(volume->GetNumProbes());
                        d3d.cmdList->Dispatch(numProbes, 1, 1);

                        // Increment the instance offset
                        instanceOffset += resources.volumes->at(volumeIndex)->GetNumProbes();
                    }

                    barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    barrier.UAV.pResource = resources.tlas.instances;

                    // Wait for the compute passes to finish
                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    // Transition the TLAS instances
                    barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = resources.tlas.instances;
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
                    ASInputs.InstanceDescs = resources.tlas.instances->GetGPUVirtualAddress();
                    ASInputs.NumDescs = static_cast<UINT>(resources.probeInstances.size());
                    ASInputs.Flags = buildFlags;

                    // Describe and build the TLAS
                    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
                    buildDesc.Inputs = ASInputs;
                    buildDesc.ScratchAccelerationStructureData = resources.tlas.scratch->GetGPUVirtualAddress();
                    buildDesc.DestAccelerationStructureData = resources.tlas.as->GetGPUVirtualAddress();

                    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

                    // Wait for the TLAS build to complete
                    barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    barrier.UAV.pResource = resources.tlas.as;

                    d3d.cmdList->ResourceBarrier(1, &barrier);

                #ifdef GFX_PERF_MARKERS
                    PIXEndEvent(d3d.cmdList);
                #endif

                    return true;
                }

                // --- Create -----------------------------------------------------------------------------------------

                bool CreateRootSignature(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    D3D12_DESCRIPTOR_RANGE ranges[5];
                    UINT rangeIndex = 0;

                    // Samplers
                    D3D12_DESCRIPTOR_RANGE samplerRange;
                    samplerRange.BaseShaderRegister = 0;
                    samplerRange.NumDescriptors = UINT_MAX;
                    samplerRange.RegisterSpace = 0;
                    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    samplerRange.OffsetInDescriptorsFromTableStart = 0;

                    // Sampler Descriptor Table
                    D3D12_ROOT_PARAMETER param0 = {};
                    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    param0.DescriptorTable.NumDescriptorRanges = 1;
                    param0.DescriptorTable.pDescriptorRanges = &samplerRange;

                    // Cameras Constant Buffer CBV (b1, space0)
                    ranges[rangeIndex].BaseShaderRegister = 1;
                    ranges[rangeIndex].NumDescriptors = 1;
                    ranges[rangeIndex].RegisterSpace = 0;
                    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::CBV_CAMERA;
                    rangeIndex++;

                    // DDGIVolume StructuredBuffer SRV (t1, space0)
                    ranges[rangeIndex].BaseShaderRegister = 1;
                    ranges[rangeIndex].NumDescriptors = 1;
                    ranges[rangeIndex].RegisterSpace = 0;
                    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_DDGI_VOLUMES;
                    rangeIndex++;

                    // TLAS Instances RWStructuredBuffer, GBufferA, GBufferB UAVs (u0-u2, space0)
                    ranges[rangeIndex].BaseShaderRegister = 0;
                    ranges[rangeIndex].NumDescriptors = 3;
                    ranges[rangeIndex].RegisterSpace = 0;
                    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_INSTANCES;
                    rangeIndex++;

                    // Bindless ByteAddressBuffer SRVs (t2, space0)
                    ranges[rangeIndex].BaseShaderRegister = 2;
                    ranges[rangeIndex].NumDescriptors = UINT_MAX;
                    ranges[rangeIndex].RegisterSpace = 0;
                    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_INDICES;
                    rangeIndex++;

                    // Bindless Texture2D SRVs (t2, space1)
                    ranges[rangeIndex].BaseShaderRegister = 2;
                    ranges[rangeIndex].NumDescriptors = UINT_MAX;
                    ranges[rangeIndex].RegisterSpace = 1;
                    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_TEX2D_START;
                    rangeIndex++;

                    // Resource Descriptor Table
                    D3D12_ROOT_PARAMETER param1 = {};
                    param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    param1.DescriptorTable.NumDescriptorRanges = _countof(ranges);
                    param1.DescriptorTable.pDescriptorRanges = ranges;

                    // RaytracingAccelerationStructure SRV (t0, space0)
                    D3D12_ROOT_PARAMETER param2 = {};
                    param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                    param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    param2.Descriptor.ShaderRegister = 0;
                    param2.Descriptor.RegisterSpace = 0;

                    // Root Constants (b0, space0)
                    D3D12_ROOT_PARAMETER param3 = {};
                    param3.Constants.ShaderRegister = 0;
                    param3.Constants.Num32BitValues = resources.constants.GetAlignedNum32BitValues();
                    param3.Constants.RegisterSpace = 0;
                    param3.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

                    D3D12_ROOT_PARAMETER rootParams[4] = { param0, param1, param2, param3 };

                    D3D12_ROOT_SIGNATURE_DESC desc = {};
                    desc.NumParameters = _countof(rootParams);
                    desc.pParameters = rootParams;
                    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

                    // Create the root signature
                    resources.rootSignature = CreateRootSignature(d3d, desc);
                    if (!resources.rootSignature)
                    {
                        log << "Failed to create DDGI visualizations root signature!\n";
                        return false;
                    }
                #ifdef GFX_NAME_OBJECTS
                    resources.rootSignature->SetName(L"DDGI Visualizations Root Signature");
                #endif
                    return true;
                }

                bool CreateDescriptorHeap(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    // See DescriptorHeapOffsets for the descriptor heap layout
                    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                    heapDesc.NumDescriptors = DescriptorHeapOffsets::SRV_TEX2D_START + (static_cast<UINT>(resources.volumes->size()) * GetDDGIVolumeNumSRVDescriptors());
                    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

                    // Create the descriptor heap
                    D3DCHECK(d3d.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&resources.srvDescHeap)));
                #ifdef GFX_NAME_OBJECTS
                    resources.srvDescHeap->SetName(L"DDGI Visualizations Descriptor Heap");
                #endif

                    resources.srvDescHeapStart = resources.srvDescHeap->GetCPUDescriptorHandleForHeapStart();
                    resources.srvDescHeapEntrySize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

                    UpdateDescriptorHeap(d3d, d3dResources, resources);

                    return true;
                }

                bool LoadAndCompileShaders(Globals& d3d, Resources& resources, std::ofstream& log)
                {
                    // Release existing shaders
                    resources.rtShaders.Release();
                    resources.textureVisCS.Release();
                    resources.updateTlasCS.Release();

                    std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                    // Load and compile the ray generation shader
                    {
                        resources.rtShaders.rgs.filepath = root + L"shaders/ddgi/visualizations/ProbesRGS.hlsl";
                        resources.rtShaders.rgs.entryPoint = L"RayGen";
                        resources.rtShaders.rgs.exportName = L"DDGIVisProbesRGS";

                        Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));

                        CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI Visualizations ray generation shader!\n", log);
                    }

                    // Load and compile the miss shader
                    {
                        resources.rtShaders.miss.filepath = root + L"shaders/ddgi/visualizations/ProbesMiss.hlsl";
                        resources.rtShaders.miss.entryPoint = L"Miss";
                        resources.rtShaders.miss.exportName = L"DDGIVisProbesMiss";

                        // Load and compile
                        CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.miss, true), "compile DDGI Visualizations miss shader!\n", log);
                    }

                    // Add the hit group
                    {
                        resources.rtShaders.hitGroups.emplace_back();

                        Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                        group.exportName = L"DDGIVisProbesHitGroup";

                        // Closest hit shader
                        group.chs.filepath = root + L"shaders/ddgi/visualizations/ProbesCHS.hlsl";
                        group.chs.entryPoint = L"CHS";
                        group.chs.exportName = L"DDGIVisProbesCHS";

                        // Load and compile
                        CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile DDGI Visualizations closest hit shader!\n", log);

                        // Set the payload size
                        resources.rtShaders.payloadSizeInBytes = sizeof(ProbesPayload);
                    }

                    // Load and compile the volume texture shader
                    {
                        resources.textureVisCS.filepath = root + L"shaders/ddgi/visualizations/VolumeTexturesCS.hlsl";
                        resources.textureVisCS.entryPoint = L"CS";
                        resources.textureVisCS.targetProfile = L"cs_6_0";

                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_X", L"8");
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_Y", L"4");

                        CHECK(Shaders::Compile(d3d.shaderCompiler, resources.textureVisCS, true), "compile DDGI Visualizations volume textures compute shader!\n", log);
                    }

                    // Load and compile the TLAS update compute shader
                    {
                        resources.updateTlasCS.filepath = root + L"shaders/ddgi/visualizations/ProbesUpdateCS.hlsl";
                        resources.updateTlasCS.entryPoint = L"CS";
                        resources.updateTlasCS.targetProfile = L"cs_6_0";

                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));

                        CHECK(Shaders::Compile(d3d.shaderCompiler, resources.updateTlasCS, true), "compile DDGI Visualizations probes update compute shader!\n", log);
                    }

                    return true;
                }

                bool CreatePSOs(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    // Release existing PSOs
                    SAFE_RELEASE(resources.rtpso);
                    SAFE_RELEASE(resources.rtpsoInfo);
                    SAFE_RELEASE(resources.texturesVisPSO);
                    SAFE_RELEASE(resources.updateTlasPSO);

                    // Create the probe visualization RTPSO
                    CHECK(CreateRayTracingPSO(
                        d3d.device,
                        resources.rootSignature,
                        resources.rtShaders,
                        &resources.rtpso,
                        &resources.rtpsoInfo),
                        "create DDGI Probe Visualization RTPSO!\n", log);

                #ifdef GFX_NAME_OBJECTS
                    resources.rtpso->SetName(L"DDGI Probe Visualization RTPSO");
                #endif

                    // Create the volume texture visualization PSO
                    CHECK(CreateComputePSO(
                        d3d.device,
                        resources.rootSignature,
                        resources.textureVisCS,
                        &resources.texturesVisPSO),
                        "create DDGI Volume Texture Visualization PSO!\n", log);

                #ifdef GFX_NAME_OBJECTS
                    resources.texturesVisPSO->SetName(L"DDGI Volume Texture Visualization PSO");
                #endif

                    // Create the probe update compute PSO
                    CHECK(CreateComputePSO(
                        d3d.device,
                        resources.rootSignature,
                        resources.updateTlasCS,
                        &resources.updateTlasPSO),
                        "create DDGI Visualization Probe Update Compute PSO!\n", log);

                #ifdef GFX_NAME_OBJECTS
                    resources.updateTlasPSO->SetName(L"DDGI Visualization Probe Update PSO");
                #endif

                    return true;
                }

                bool CreateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    // The Shader Table layout is as follows:
                    //    Entry 0:  Probe Vis Ray Generation Shader
                    //    Entry 1:  Probe Vis Miss Shader
                    //    Entry 2+: Probe Vis HitGroups
                    // All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
                    // The entries must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT.
                    // The CHS requires the largest entry:
                    //   32 bytes for the shader identifier
                    // +  8 bytes for descriptor table VA
                    // +  8 bytes for sampler descriptor table VA
                    // = 48 bytes ->> aligns to 64 bytes

                    // Release the existing shader table
                    resources.shaderTableSize = 0;
                    SAFE_RELEASE(resources.shaderTable);

                    UINT shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                    // Configure the shader record size (no shader record data)
                    resources.shaderTableRecordSize = shaderIdSize;
                    resources.shaderTableRecordSize += 8;              // descriptor table GPUVA
                    resources.shaderTableRecordSize += 8;              // sampler descriptor table GPUVA
                    resources.shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, resources.shaderTableRecordSize);

                    // Find the shader table size
                    resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.rtShaders.hitGroups.size())) * resources.shaderTableRecordSize;
                    resources.shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, resources.shaderTableSize);

                    // Create the shader table upload buffer resource
                    BufferDesc desc = { resources.shaderTableSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                    CHECK(CreateBuffer(d3d, desc, &resources.shaderTableUpload), "create DDGI Visualizations shader table upload buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    resources.shaderTableUpload->SetName(L"DDGI Visualizations Shader Table Upload");
                #endif

                    // Create the shader table buffer resource
                    desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE };
                    CHECK(CreateBuffer(d3d, desc, &resources.shaderTable), "create DDGI Visualizations shader table!", log);
                #ifdef GFX_NAME_OBJECTS
                    resources.shaderTable->SetName(L"DDGI Visualizations Shader Table");
                #endif

                    return true;
                }

                bool CreateGeometry(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    // Generate the sphere geometry
                    Geometry::CreateSphere(30, 30, resources.probe);

                    // Create the probe sphere's vertex and index buffers
                    CHECK(CreateIndexBuffer(d3d, resources.probe, &resources.probeIB, &resources.probeIBUpload, resources.probeIBView), "create probe index buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    resources.probeIB->SetName(L"IB: Probe Sphere, Primitive 0");
                #endif

                    CHECK(CreateVertexBuffer(d3d, resources.probe, &resources.probeVB, &resources.probeVBUpload, resources.probeVBView), "create probe vertex buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    resources.probeVB->SetName(L"VB: DDGI Probe Sphere, Primitive 0");
                #endif

                    // Add the index buffer SRV to the descriptor heap
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    srvDesc.Buffer.NumElements = resources.probeIBView.SizeInBytes / sizeof(UINT);
                    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    D3D12_CPU_DESCRIPTOR_HANDLE handle;
                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_INDICES * resources.srvDescHeapEntrySize);
                    d3d.device->CreateShaderResourceView(resources.probeIB, &srvDesc, handle);

                    // Add the vertex buffer SRV to the descriptor heap
                    srvDesc.Buffer.NumElements = resources.probeVBView.SizeInBytes / sizeof(UINT);

                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_VERTICES * resources.srvDescHeapEntrySize);
                    d3d.device->CreateShaderResourceView(resources.probeVB, &srvDesc, handle);

                    return true;
                }

                bool CreateBLAS(Globals& d3d, Resources& resources)
                {
                    // Describe the BLAS geometries
                    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
                    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                    geometryDesc.Triangles.VertexBuffer.StartAddress = resources.probeVB->GetGPUVirtualAddress();
                    geometryDesc.Triangles.VertexBuffer.StrideInBytes = resources.probeVBView.StrideInBytes;
                    geometryDesc.Triangles.VertexCount = (resources.probeVBView.SizeInBytes / resources.probeVBView.StrideInBytes);
                    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
                    geometryDesc.Triangles.IndexBuffer = resources.probeIB->GetGPUVirtualAddress();
                    geometryDesc.Triangles.IndexFormat = resources.probeIBView.Format;
                    geometryDesc.Triangles.IndexCount = (resources.probeIBView.SizeInBytes / sizeof(UINT));
                    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

                    // Describe the acceleration structure inputs
                    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
                    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
                    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
                    ASInputs.pGeometryDescs = &geometryDesc;
                    ASInputs.NumDescs = 1;
                    ASInputs.Flags = buildFlags;

                    // Get the size requirements for the BLAS buffers
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPreBuildInfo = {};
                    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &asPreBuildInfo);
                    asPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ScratchDataSizeInBytes);
                    asPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ResultDataMaxSizeInBytes);

                    // Create the BLAS scratch buffer
                    BufferDesc blasScratchDesc =
                    {
                        asPreBuildInfo.ScratchDataSizeInBytes,
                        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                        EHeapType::DEFAULT,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                    };
                    if (!CreateBuffer(d3d, blasScratchDesc, &resources.blas.scratch)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.blas.scratch->SetName(L"BLAS Scratch: DDGI Probe Visualization");
                #endif

                    // Create the BLAS buffer
                    BufferDesc blasDesc =
                    {
                        asPreBuildInfo.ResultDataMaxSizeInBytes,
                        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                        EHeapType::DEFAULT,
                        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                    };
                    if (!CreateBuffer(d3d, blasDesc, &resources.blas.as)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.blas.as->SetName(L"BLAS: DDGI Probe Visualization");
                #endif

                    // Describe and build the BLAS
                    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
                    buildDesc.Inputs = ASInputs;
                    buildDesc.ScratchAccelerationStructureData = resources.blas.scratch->GetGPUVirtualAddress();
                    buildDesc.DestAccelerationStructureData = resources.blas.as->GetGPUVirtualAddress();

                    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

                    // Wait for the BLAS build to complete
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    barrier.UAV.pResource = resources.blas.as;

                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    return true;
                }

                bool CreateInstances(Globals& d3d, Resources& resources)
                {
                    // Release the existing TLAS
                    resources.tlas.Release();

                    // Get the maximum number of probe instances from all volumes
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes->size()); volumeIndex++)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));
                        resources.maxProbeInstances += volume->GetNumProbes();
                    }

                    // Early out if no volumes or probes exist
                    if (resources.maxProbeInstances == 0) return true;

                    // Create the TLAS instance upload buffer resource
                    UINT size = resources.maxProbeInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
                    BufferDesc desc = { size, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                    if (!CreateBuffer(d3d, desc, &resources.tlas.instancesUpload)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.tlas.instancesUpload->SetName(L"TLAS Instance Descriptors Upload Buffer");
                #endif

                    // Create the TLAS instance device buffer resource
                    desc = { size, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                    if (!CreateBuffer(d3d, desc, &resources.tlas.instances)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.tlas.instances->SetName(L"TLAS Instance Descriptors Buffer");
                #endif

                    // Add the TLAS instances structured buffer UAV to the descriptor heap
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    uavDesc.Buffer.NumElements = resources.maxProbeInstances;
                    uavDesc.Buffer.StructureByteStride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

                    D3D12_CPU_DESCRIPTOR_HANDLE handle;
                    handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_INSTANCES * resources.srvDescHeapEntrySize);
                    d3d.device->CreateUnorderedAccessView(resources.tlas.instances, nullptr, &uavDesc, handle);

                    return true;
                }

                bool CreateTLAS(Globals& d3d, Resources& resources)
                {
                    if (!CreateInstances(d3d, resources)) return false;

                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

                    // Get the size requirements for the TLAS buffer
                    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
                    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
                    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
                    ASInputs.InstanceDescs = resources.tlas.instances->GetGPUVirtualAddress();
                    ASInputs.NumDescs = resources.maxProbeInstances;
                    ASInputs.Flags = buildFlags;

                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPreBuildInfo = {};
                    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &asPreBuildInfo);
                    asPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ResultDataMaxSizeInBytes);
                    asPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ScratchDataSizeInBytes);

                    // Create TLAS scratch buffer resource
                    BufferDesc desc =
                    {
                        asPreBuildInfo.ScratchDataSizeInBytes,
                        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                        EHeapType::DEFAULT,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                    };
                    if (!CreateBuffer(d3d, desc, &resources.tlas.scratch)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.tlas.scratch->SetName(L"TLAS Scratch: DDGI Probe Visualization");
                #endif

                    // Create the TLAS buffer resource
                    desc.size = asPreBuildInfo.ResultDataMaxSizeInBytes;
                    desc.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
                    if (!CreateBuffer(d3d, desc, &resources.tlas.as)) return false;
                #ifdef GFX_NAME_OBJECTS
                    resources.tlas.as->SetName(L"TLAS: DDGI Probe Visualization");
                #endif

                    return true;
                }

                //----------------------------------------------------------------------------------------------------------
                // Public Functions
                //----------------------------------------------------------------------------------------------------------

                /**
                 * Create resources used by the DDGI passes.
                 */
                bool Initialize(Globals& d3d, GlobalResources& d3dResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
                {
                    resources.volumes = &ddgiResources.volumes;
                    resources.constantsSTB = ddgiResources.constantsSTB;

                    if (!CreateRootSignature(d3d, d3dResources, resources, log)) return false;
                    if (!CreateDescriptorHeap(d3d, d3dResources, resources, log)) return false;
                    if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                    if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                    if (!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                    if (!CreateGeometry(d3d, d3dResources, resources, log)) return false;
                    if (!CreateBLAS(d3d, resources)) return false;
                    if (!CreateTLAS(d3d, resources)) return false;

                    if (!UpdateShaderTable(d3d, d3dResources, resources)) return false;

                    resources.cpuStat = perf.AddCPUStat("DDGIVis");
                    resources.gpuProbeStat = perf.AddGPUStat("DDGI Probe Vis");
                    resources.gpuTextureStat = perf.AddGPUStat("DDGI Texture Vis");

                    return true;
                }

                /**
                 * Reload and compile shaders, recreate PSOs, and recreate the shader table.
                 */
                bool Reload(Globals& d3d, GlobalResources& d3dResources, DDGI::Resources& ddgiResources, Resources& resources, std::ofstream& log)
                {
                    resources.volumes = &ddgiResources.volumes;
                    resources.constantsSTB = ddgiResources.constantsSTB;

                    log << "Reloading DDGI Visualization shaders...";

                    UpdateDescriptorHeap(d3d, d3dResources, resources);
                    if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                    if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                    if (!UpdateShaderTable(d3d, d3dResources, resources)) return false;

                    log << "done.\n";
                    log << std::flush;

                    return true;
                }

                /**
                 * Resize screen-space buffers.
                 */
                bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
                {
                    // nothing to do here
                    return true;
                }

                /**
                 * Update data before execute.
                 */
                void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
                {
                    CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                    // Update the show flags
                    resources.flags = VIS_FLAG_SHOW_NONE;
                    if (config.ddgi.showProbes) resources.flags |= VIS_FLAG_SHOW_PROBES;
                    if (config.ddgi.showTextures) resources.flags |= VIS_FLAG_SHOW_TEXTURES;

                    resources.enabled = config.ddgi.enabled;
                    if (resources.enabled)
                    {
                        Configs::DDGIVolume volume = config.ddgi.volumes[config.ddgi.selectedVolume];

                        if (resources.flags & VIS_FLAG_SHOW_PROBES)
                        {
                            // Update constants
                            resources.constants.probeType = volume.probeType;
                            resources.constants.probeRadius = volume.probeRadius;
                            resources.constants.probeAlpha = volume.probeAlpha;
                            resources.constants.distanceDivisor = volume.probeDistanceDivisor;

                            // Update the TLAS instances and rebuild
                            UpdateTLAS(d3d, resources, config);
                        }

                        if (resources.flags & VIS_FLAG_SHOW_TEXTURES)
                        {
                            // Update constants
                            resources.constants.volumeIndex = config.ddgi.selectedVolume;
                            resources.constants.distanceDivisor = volume.probeDistanceDivisor;

                            resources.constants.rayDataTextureScale = volume.probeRayDataScale;
                            resources.constants.irradianceTextureScale = volume.probeIrradianceScale;
                            resources.constants.distanceTextureScale = volume.probeDistanceScale;
                            resources.constants.relocationOffsetTextureScale = volume.probeRelocationOffsetScale;
                            resources.constants.classificationStateTextureScale = volume.probeClassificationStateScale;
                        }
                    }
                    CPU_TIMESTAMP_END(resources.cpuStat);
                }

                /**
                 * Record the graphics workload to the global command list.
                 */
                void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
                {
                    CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                    if (resources.enabled)
                    {
                        // Render probes
                        if (resources.flags & VIS_FLAG_SHOW_PROBES)
                        {
                            if (resources.probeInstances.size() > 0)
                            {
                            #ifdef GFX_PERF_MARKERS
                                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Vis: DDGIVolume Probes");
                            #endif

                                // Set the descriptor heaps
                                ID3D12DescriptorHeap* ppHeaps[] = { resources.srvDescHeap, d3dResources.samplerDescHeap };
                                d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                                // Set the root signature
                                d3d.cmdList->SetComputeRootSignature(resources.rootSignature);

                                // Set the descriptor tables
                                d3d.cmdList->SetComputeRootDescriptorTable(0, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                                d3d.cmdList->SetComputeRootDescriptorTable(1, resources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());

                                // Set the TLAS SRV
                                d3d.cmdList->SetComputeRootShaderResourceView(2, resources.tlas.as->GetGPUVirtualAddress());

                                // Update the root constants
                                DDGIVisConstants consts = resources.constants;
                                d3d.cmdList->SetComputeRoot32BitConstants(3, consts.GetNum32BitValues(), consts.GetData(), 0);

                                // Dispatch rays
                                D3D12_DISPATCH_RAYS_DESC desc = {};
                                desc.RayGenerationShaderRecord.StartAddress = resources.shaderTableRGSStartAddress;
                                desc.RayGenerationShaderRecord.SizeInBytes = resources.shaderTableRecordSize;

                                desc.MissShaderTable.StartAddress = resources.shaderTableMissTableStartAddress;
                                desc.MissShaderTable.SizeInBytes = resources.shaderTableMissTableSize;
                                desc.MissShaderTable.StrideInBytes = resources.shaderTableRecordSize;

                                desc.HitGroupTable.StartAddress = resources.shaderTableHitGroupTableStartAddress;
                                desc.HitGroupTable.SizeInBytes = resources.shaderTableHitGroupTableSize;
                                desc.HitGroupTable.StrideInBytes = resources.shaderTableRecordSize;

                                desc.Width = d3d.width;
                                desc.Height = d3d.height;
                                desc.Depth = 1;

                                // Set the PSO
                                d3d.cmdList->SetPipelineState1(resources.rtpso);

                                // Dispatch rays
                                GPU_TIMESTAMP_BEGIN(resources.gpuProbeStat->GetQueryBeginIndex());
                                d3d.cmdList->DispatchRays(&desc);
                                GPU_TIMESTAMP_END(resources.gpuProbeStat->GetQueryEndIndex());

                                D3D12_RESOURCE_BARRIER barriers[2] = {};
                                barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                                barriers[0].UAV.pResource = d3dResources.rt.GBufferA;
                                barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                                barriers[1].UAV.pResource = d3dResources.rt.GBufferB;

                                // Wait for the ray trace to complete
                                d3d.cmdList->ResourceBarrier(2, barriers);

                            #ifdef GFX_PERF_MARKERS
                                PIXEndEvent(d3d.cmdList);
                            #endif
                            }
                        }

                        // Render volume textures
                        if (resources.flags & VIS_FLAG_SHOW_TEXTURES)
                        {
                        #ifdef GFX_PERF_MARKERS
                            PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Vis: DDGIVolume Textures");
                        #endif

                            // Set the descriptor heaps
                            ID3D12DescriptorHeap* ppHeaps[] = { resources.srvDescHeap, d3dResources.samplerDescHeap };
                            d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                            // Set the root signature
                            d3d.cmdList->SetComputeRootSignature(resources.rootSignature);

                            // Set the descriptor tables
                            d3d.cmdList->SetComputeRootDescriptorTable(0, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                            d3d.cmdList->SetComputeRootDescriptorTable(1, resources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());

                            // Update the root constants
                            DDGIVisConstants consts = resources.constants;
                            d3d.cmdList->SetComputeRoot32BitConstants(3, consts.GetNum32BitValues(), consts.GetData(), 0);

                            // Set the PSO
                            d3d.cmdList->SetPipelineState(resources.texturesVisPSO);

                            // Dispatch threads
                            UINT groupsX = DivRoundUp(d3d.width, 8);
                            UINT groupsY = DivRoundUp(d3d.height, 4);

                            GPU_TIMESTAMP_BEGIN(resources.gpuTextureStat->GetQueryBeginIndex());
                            d3d.cmdList->Dispatch(groupsX, groupsY, 1);
                            GPU_TIMESTAMP_END(resources.gpuTextureStat->GetQueryEndIndex());

                            // Wait for the compute pass to finish
                            D3D12_RESOURCE_BARRIER barrier = {};
                            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                            barrier.UAV.pResource = d3dResources.rt.GBufferA;
                            d3d.cmdList->ResourceBarrier(1, &barrier);

                        #ifdef GFX_PERF_MARKERS
                            PIXEndEvent(d3d.cmdList);
                        #endif
                        }

                    }
                    CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
                }

                /**
                 * Release resources.
                 */
                void Cleanup(Resources& resources)
                {
                    SAFE_RELEASE(resources.probeVB);
                    SAFE_RELEASE(resources.probeVBUpload);
                    SAFE_RELEASE(resources.probeIB);
                    SAFE_RELEASE(resources.probeIBUpload);

                    resources.blas.Release();
                    resources.tlas.Release();

                    SAFE_RELEASE(resources.shaderTable);
                    SAFE_RELEASE(resources.shaderTableUpload);

                    resources.rtShaders.Release();
                    SAFE_RELEASE(resources.rtpso);
                    SAFE_RELEASE(resources.rtpsoInfo);

                    resources.textureVisCS.Release();
                    SAFE_RELEASE(resources.texturesVisPSO);

                    resources.updateTlasCS.Release();
                    SAFE_RELEASE(resources.updateTlasPSO);

                    SAFE_RELEASE(resources.rootSignature);
                    SAFE_RELEASE(resources.srvDescHeap);

                    resources.shaderTableSize = 0;
                    resources.shaderTableRecordSize = 0;
                    resources.shaderTableMissTableSize = 0;
                    resources.shaderTableHitGroupTableSize = 0;
                }

            } // namespace Graphics::D3D12::DDGI::Visualizations

        } // namespace Graphics::D3D12::DDGI

    } // namespace Graphics::D3D12

    namespace DDGI::Visualizations
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Visualizations::Initialize(d3d, d3dResources, ddgiResources, resources, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, DDGI::Resources& ddgiResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Visualizations::Reload(d3d, d3dResources, ddgiResources, resources, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Visualizations::Resize(d3d, d3dResources, resources, log);
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::DDGI::Visualizations::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::DDGI::Visualizations::Execute(d3d, d3dResources, resources);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::DDGI::Visualizations::Cleanup(resources);
        }

    } // namespace Graphics::DDGIVis
}
