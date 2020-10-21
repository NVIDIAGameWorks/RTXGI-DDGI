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
#include "Geometry.h"
#include "Shaders.h"

#include <rtxgi/ddgi/DDGIVolume.h>

using namespace DirectX;

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include <pix3.h>
#endif

static const D3D12_HEAP_PROPERTIES defaultHeapProperties =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
* Create a bottom level acceleration structure (BLAS) for the scene.
*/
bool CreateBLAS(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, Scene &scene)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Describe the BLAS goemetries. Each mesh primitive populates a BLAS.
    dxr.BLASes.resize(scene.numGeometries);
    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); meshIndex++)
    {
        // Get the mesh
        const Mesh mesh = scene.meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            // Get the mesh primitive
            const MeshPrimitive primitive = mesh.primitives[primitiveIndex];

            // Describe the geometry
            D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
            desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            desc.Triangles.VertexBuffer.StartAddress = resources.sceneVBs[primitive.index]->GetGPUVirtualAddress();
            desc.Triangles.VertexBuffer.StrideInBytes = resources.sceneVBViews[primitive.index].StrideInBytes;
            desc.Triangles.VertexCount = static_cast<UINT>(primitive.vertices.size());
            desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            desc.Triangles.IndexBuffer = resources.sceneIBs[primitive.index]->GetGPUVirtualAddress();
            desc.Triangles.IndexFormat = resources.sceneIBViews[primitive.index].Format;
            desc.Triangles.IndexCount = static_cast<UINT>(primitive.indices.size());
            if (primitive.opaque) desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            // Describe the acceleration structure inputs
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
            ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            ASInputs.pGeometryDescs = &desc;
            ASInputs.NumDescs = 1;
            ASInputs.Flags = buildFlags;

            // Get the size requirements for the BLAS buffer
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
            d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

            ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
            ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

            // Create the BLAS scratch buffer
            D3D12BufferInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
            if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.BLASes[primitive.index].pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
            std::string str = "DXR BLASes Scratch: ";
            str.append(mesh.name.c_str());
            str.append(", Primitive: ");
            str.append(std::to_string(primitiveIndex));
            std::wstring name = std::wstring(str.begin(), str.end());
            dxr.BLASes[primitive.index].pScratch->SetName(name.c_str());
#endif

            // Create the BLAS buffer
            bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
            bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.BLASes[primitive.index].pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
            str = "DXR BLASes: ";
            str.append(mesh.name.c_str());
            str.append(", Primitive: ");
            str.append(std::to_string(primitiveIndex));
            name = std::wstring(str.begin(), str.end());
            dxr.BLASes[primitive.index].pResult->SetName(name.c_str());
#endif

            // Describe and build the BLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = ASInputs;
            buildDesc.ScratchAccelerationStructureData = dxr.BLASes[primitive.index].pScratch->GetGPUVirtualAddress();
            buildDesc.DestAccelerationStructureData = dxr.BLASes[primitive.index].pResult->GetGPUVirtualAddress();

            d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

            // Wait for the BLAS build to complete
            D3D12_RESOURCE_BARRIER uavBarrier;
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = dxr.BLASes[primitive.index].pResult;
            uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            d3d.cmdList->ResourceBarrier(1, &uavBarrier);
        }
    }
    return true;
}

/**
* Create a bottom level acceleration structure (BLAS) for the probe visualization spheres.
*/
bool CreateProbeBLAS(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources)
{
    // Create the sphere resources
    if(!Geometry::CreateSphere(d3d, resources)) return false;

    // Describe the BLAS geometries
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = resources.sphereVB->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = resources.sphereVBView.StrideInBytes;
    geometryDesc.Triangles.VertexCount = (resources.sphereVBView.SizeInBytes / resources.sphereVBView.StrideInBytes);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexBuffer = resources.sphereIB->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = resources.sphereIBView.Format;
    geometryDesc.Triangles.IndexCount = (resources.sphereIBView.SizeInBytes / sizeof(UINT));
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
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

    // Create the BLAS scratch buffer
    D3D12BufferInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.probeBLAS.pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.probeBLAS.pScratch->SetName(L"DXR Probe BLASes Scratch");
#endif

    // Create the BLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.probeBLAS.pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.probeBLAS.pResult->SetName(L"DXR Probe BLASes");
#endif

    // Describe and build the BLAS
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
* Create a top level acceleration structure (TLAS) for the scene.
*/
bool CreateTLAS(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, Scene &scene)
{
    // Describe the TLAS instance(s)
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
    for (size_t instanceIndex = 0; instanceIndex < scene.instances.size(); instanceIndex++)
    {
        const Instance instance = scene.instances[instanceIndex];
        const Mesh mesh = scene.meshes[instance.mesh];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            const MeshPrimitive primitive = mesh.primitives[primitiveIndex];

            // Describe the instance
            D3D12_RAYTRACING_INSTANCE_DESC desc = {};
            desc.InstanceID = 0;
            desc.InstanceContributionToHitGroupIndex = primitive.index; // one shader record per BLAS
            desc.InstanceMask = 0xFF;
            desc.AccelerationStructure = dxr.BLASes[primitive.index].pResult->GetGPUVirtualAddress();
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
#endif

            // Disable front or back face culling for meshes with double sided materials
            if (scene.materials[primitive.material].data.doubleSided)
            {
                desc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
            }

            // Write the instance transform
            memcpy(desc.Transform, instance.transform, sizeof(XMFLOAT4) * 3);

            instances.push_back(desc);
        }
    }

    // Create the TLAS instance buffer
    D3D12BufferInfo instanceBufferInfo;
    instanceBufferInfo.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size();
    instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
    instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
    instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
    if (!D3D12::CreateBuffer(d3d, instanceBufferInfo, &dxr.TLAS.pInstanceDesc)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.TLAS.pInstanceDesc->SetName(L"DXR TLAS Instance Descriptors");
#endif

    // Copy the instance data to the upload buffer
    UINT8* pData = nullptr;
    dxr.TLAS.pInstanceDesc->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    memcpy(pData, instances.data(), instanceBufferInfo.size);
    dxr.TLAS.pInstanceDesc->Unmap(0, nullptr);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = dxr.TLAS.pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = (UINT)instances.size();
    ASInputs.Flags = buildFlags;

    // Get the size requirements for the TLAS buffers
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    D3D12BufferInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
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
bool CreateGlobalRootSignature(D3D12Global &d3d, DXRGlobal &dxr)
{
    D3D12_DESCRIPTOR_RANGE ranges[7];
    UINT rangeIndex = 0;

    // Camera and light constant buffers (b1, b2)
    ranges[rangeIndex].BaseShaderRegister = 1;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::CAMERA_OFFSET;
    rangeIndex++;

    // GBufferA, GBufferB, GBufferC, BufferD, RTAORaw, RTAOFiltered
    // PTOutput, PTAccumulation (u0, u1, u2, u3, u4, u5, u6, u7)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 8;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::RT_GBUFFER_OFFSET;
    rangeIndex++;
     
    // VisTLAS Instances (u0, u1, u2, u3, u4, u5, u6, u7, u8)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 3;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VIS_TLAS_OFFSET;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------
    // SRV array
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;

    // float array
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;

    // uint array
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 2;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;

    // Blue Noise RGB SRV (t5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::BLUE_NOISE_OFFSET;
    //ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 40;

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
    param4.Constants.Num32BitValues = 12;
    param4.Constants.ShaderRegister = 4;
    param4.Constants.RegisterSpace = 0;

    // VisTLAS Update root constants (b5)
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

    // Multi volume select root constant (b0, space1)
    D3D12_ROOT_PARAMETER param7 = {};
    param7.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param7.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param7.Constants.Num32BitValues = 1;
    param7.Constants.ShaderRegister = 0;
    param7.Constants.RegisterSpace = 1;

    D3D12_ROOT_PARAMETER rootParams[8] = { param0, param1, param2, param3, param4, param5, param6, param7 };

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
* Load and compile the DXR Ray Generation Shaders.
*/
bool CreateRayGenPrograms(D3D12Global &d3d, DXRGlobal &dxr, ShaderCompiler &compiler)
{
    std::wstring path = std::wstring(compiler.root.begin(), compiler.root.end());
    std::wstring file;

    // Load and compile the probe ray trace ray generation shader
    file = path + L"shaders\\ProbeTraceRGS.hlsl";
    dxr.probeRGS.filepath = file.c_str();
    dxr.probeRGS.entryPoint = L"RayGen";
    dxr.probeRGS.exportName = L"ProbeRGS";
    if (!Shaders::Compile(compiler, dxr.probeRGS, true)) return false;

    // Load and compile the primary ray trace ray generation shader
    file = path + L"shaders\\PrimaryTraceRGS.hlsl";
    dxr.primaryRGS.filepath = file.c_str();
    dxr.primaryRGS.entryPoint = L"RayGen";
    dxr.primaryRGS.exportName = L"PrimaryRGS";
    if (!Shaders::Compile(compiler, dxr.primaryRGS, true)) return false;

    // Load and compile the ambient occlusion ray generation shader
    file = path + L"shaders\\AOTraceRGS.hlsl";
    dxr.ambientOcclusionRGS.filepath = file.c_str();
    dxr.ambientOcclusionRGS.entryPoint = L"RayGen";
    dxr.ambientOcclusionRGS.exportName = L"AORGS";
    if (!Shaders::Compile(compiler, dxr.ambientOcclusionRGS, true)) return false;

    // Load and compile the probe visualization ray trace ray generation shader
    file = path + L"shaders\\VisDDGIProbes.hlsl";
    dxr.probeVisRGS.filepath = file.c_str();
    dxr.probeVisRGS.entryPoint = L"RayGen";
    dxr.probeVisRGS.exportName = L"ProbeVisRGS";
    if (!Shaders::Compile(compiler, dxr.probeVisRGS, true)) return false;

    // Load and compile the path tracing ray trace ray generation shader
    file = path + L"shaders\\PathTraceRGS.hlsl";
    dxr.pathTraceRGS.filepath = file.c_str();
    dxr.pathTraceRGS.entryPoint = L"RayGen";
    dxr.pathTraceRGS.exportName = L"PathTraceRGS";
    return Shaders::Compile(compiler, dxr.pathTraceRGS, true);
}

/**
* Load and compile the DXR Miss program.
*/
bool CreateMissProgram(D3D12Global &d3d, DXRGlobal &dxr, ShaderCompiler &compiler)
{
    std::wstring file = std::wstring(compiler.root.begin(), compiler.root.end());
    file.append(L"shaders\\Miss.hlsl");

    // Load and compile the miss shader
    dxr.miss.filepath = file.c_str();
    dxr.miss.exportName = L"Miss";
    dxr.miss.entryPoint = L"Miss";
    return Shaders::Compile(compiler, dxr.miss, true);
}

/**
* Create the DXR hit group.
* Loads and compiles the hit group's CHS and AHS.
* Creates the hit group's local root signature.
*/
bool CreateHitGroup(D3D12Global &d3d, DXRGlobal &dxr, ShaderCompiler &compiler, Scene &scene)
{
    // Set the hit group export name
    dxr.hit.exportName = L"HitGroup";

    // Load and compile the hit group's shaders
    {
        std::wstring directory = std::wstring(compiler.root.begin(), compiler.root.end());

        // Load and compile the Closest Hit Shader
        std::wstring file = directory;
        file.append(L"shaders\\CHS.hlsl");

        dxr.hit.chs.filepath = file.c_str();
        dxr.hit.chs.exportName = dxr.hit.chs.entryPoint = L"CHS";
        if (!Shaders::Compile(compiler, dxr.hit.chs, true)) return false;

        // Load and compile the Any Hit Shader
        file = directory;
        file.append(L"shaders\\AHS.hlsl");
        dxr.hit.ahs.exportName = dxr.hit.ahs.entryPoint = L"AHS";
        dxr.hit.ahs.filepath = file.c_str();
        if (!Shaders::Compile(compiler, dxr.hit.ahs, true)) return false;
    }

    // Describe and create the hit group's root signature
    {
        // 0: MeshPrimitive Material data (b3)
        D3D12_ROOT_PARAMETER param0 = {};
        param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param0.Constants.Num32BitValues = sizeof(GPUMaterial) / sizeof(float);
        param0.Constants.RegisterSpace = 0;
        param0.Constants.ShaderRegister = 3;

        // 1: Index buffer SRV (t3)
        D3D12_ROOT_PARAMETER param1 = {};
        param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param1.Descriptor.RegisterSpace = 0;
        param1.Descriptor.ShaderRegister = 3;

        // 2: Vertex buffer SRV (t4)
        D3D12_ROOT_PARAMETER param2 = {};
        param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param2.Descriptor.RegisterSpace = 0;
        param2.Descriptor.ShaderRegister = 4;

        // Textures in descriptor table
        D3D12_DESCRIPTOR_RANGE ranges[1];
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].RegisterSpace = 0;
        ranges[0].BaseShaderRegister = 6;
        ranges[0].NumDescriptors = max((UINT)scene.textures.size(), 1);     // use a single descriptor if no textures exist
        ranges[0].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::SCENE_TEXTURE_OFFSET;

        // 3: Textures (t6)
        D3D12_ROOT_PARAMETER param3 = {};
        param3.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param3.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param3.DescriptorTable.NumDescriptorRanges = _countof(ranges);
        param3.DescriptorTable.pDescriptorRanges = ranges;

        D3D12_ROOT_PARAMETER rootParams[4] = { param0, param1, param2, param3 };

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = _countof(rootParams);
        rootDesc.pParameters = rootParams;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        // Create the root signature
        dxr.hit.pRootSignature = D3D12::CreateRootSignature(d3d, rootDesc);
        if (dxr.hit.pRootSignature == nullptr) return false;
#if RTXGI_NAME_D3D_OBJECTS
        dxr.hit.pRootSignature->SetName(L"DXR Hit Group Local Root Signature");
#endif
    }

    return true;
}

/**
* Load and compile the compute shader that updates the visualization TLAS instances.
*/
bool CreateVisUpdateTLASProgram(D3D12Global &d3d, DXRGlobal &dxr, ShaderCompiler &compiler)
{
    std::wstring file = std::wstring(compiler.root.begin(), compiler.root.end());
    file.append(L"shaders\\VisUpdateTLASCS.hlsl");

    ShaderProgram shader;
    shader.filepath = file.c_str();
    shader.entryPoint = L"VisUpdateTLASCS";
    shader.targetProfile = L"cs_6_0";
    if (!Shaders::Compile(compiler, shader, true)) return false;

    dxr.visUpdateTLASCS = shader.bytecode;

    return true;
}

/**
* Create the compute PSO for the Vis TLAS update pass.
*/
bool CreateVisUpdateTLASPSO(D3D12Global &d3d, DXRGlobal &dxr)
{
    return D3D12::CreateComputePSO(d3d.device, dxr.globalRootSig, dxr.visUpdateTLASCS, &dxr.visUpdateTLASPSO);
}

/**
* Create the DXR ray tracing pipeline state object (RTPSO).
*/
bool CreatePipelineStateObject(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources)
{
    // Need 15 subobjects:
    // 1 for probe trace RGS
    // 1 for primary trace RGS
    // 1 for ambient occlusion trace RGS
    // 1 for probe vis trace RGS
    // 1 for path trace RGS
    // 1 for miss shader
    // 1 for CHS
    // 1 for AHS
    // 1 for Hit Group
    // 2 for Hit Group Local Root Signature (root signature and association)
    // 2 for Shader Config (config and association)
    // 1 for Global Root Signature
    // 1 for Pipeline Config
    UINT index = 0;
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.resize(15);

    // Add state subobject for the probe ray trace RGS
    D3D12_EXPORT_DESC probeRGSExportDesc = {};
    probeRGSExportDesc.Name = dxr.probeRGS.exportName;
    probeRGSExportDesc.ExportToRename = dxr.probeRGS.entryPoint;
    probeRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC probeRGSLibDesc = {};
    probeRGSLibDesc.DXILLibrary.BytecodeLength = dxr.probeRGS.bytecode->GetBufferSize();
    probeRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.probeRGS.bytecode->GetBufferPointer();
    probeRGSLibDesc.NumExports = 1;
    probeRGSLibDesc.pExports = &probeRGSExportDesc;

    D3D12_STATE_SUBOBJECT probeRGS = {};
    probeRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    probeRGS.pDesc = &probeRGSLibDesc;

    subobjects[index++] = probeRGS;

    // Add state subobject for the primary ray trace RGS
    D3D12_EXPORT_DESC primaryRGSExportDesc = {};
    primaryRGSExportDesc.Name = dxr.primaryRGS.exportName;
    primaryRGSExportDesc.ExportToRename = dxr.primaryRGS.entryPoint;
    primaryRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC primaryRGSLibDesc = {};
    primaryRGSLibDesc.DXILLibrary.BytecodeLength = dxr.primaryRGS.bytecode->GetBufferSize();
    primaryRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.primaryRGS.bytecode->GetBufferPointer();
    primaryRGSLibDesc.NumExports = 1;
    primaryRGSLibDesc.pExports = &primaryRGSExportDesc;

    D3D12_STATE_SUBOBJECT primaryRGS = {};
    primaryRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    primaryRGS.pDesc = &primaryRGSLibDesc;

    subobjects[index++] = primaryRGS;

    // Add state subobject for the AO ray trace RGS
    D3D12_EXPORT_DESC AORGSExportDesc = {};
    AORGSExportDesc.Name = dxr.ambientOcclusionRGS.exportName;
    AORGSExportDesc.ExportToRename = dxr.ambientOcclusionRGS.entryPoint;
    AORGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC AORGSLibDesc = {};
    AORGSLibDesc.DXILLibrary.BytecodeLength = dxr.ambientOcclusionRGS.bytecode->GetBufferSize();
    AORGSLibDesc.DXILLibrary.pShaderBytecode = dxr.ambientOcclusionRGS.bytecode->GetBufferPointer();
    AORGSLibDesc.NumExports = 1;
    AORGSLibDesc.pExports = &AORGSExportDesc;

    D3D12_STATE_SUBOBJECT AORGS = {};
    AORGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    AORGS.pDesc = &AORGSLibDesc;

    subobjects[index++] = AORGS;

    // Add state subobject for the probe vis ray trace RGS
    D3D12_EXPORT_DESC probeVisRGSExportDesc = {};
    probeVisRGSExportDesc.Name = dxr.probeVisRGS.exportName;
    probeVisRGSExportDesc.ExportToRename = dxr.probeVisRGS.entryPoint;
    probeVisRGSExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC probeVisRGSLibDesc = {};
    probeVisRGSLibDesc.DXILLibrary.BytecodeLength = dxr.probeVisRGS.bytecode->GetBufferSize();
    probeVisRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.probeVisRGS.bytecode->GetBufferPointer();
    probeVisRGSLibDesc.NumExports = 1;
    probeVisRGSLibDesc.pExports = &probeVisRGSExportDesc;

    D3D12_STATE_SUBOBJECT probeVisRGS = {};
    probeVisRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    probeVisRGS.pDesc = &probeVisRGSLibDesc;

    subobjects[index++] = probeVisRGS;

    // Add state subobject for the path trace RGS
    D3D12_EXPORT_DESC pathTraceRGSExportDesc = {};
    pathTraceRGSExportDesc.Name = dxr.pathTraceRGS.exportName;
    pathTraceRGSExportDesc.ExportToRename = dxr.pathTraceRGS.entryPoint;

    D3D12_DXIL_LIBRARY_DESC pathTraceRGSLibDesc = {};
    pathTraceRGSLibDesc.DXILLibrary.BytecodeLength = dxr.pathTraceRGS.bytecode->GetBufferSize();
    pathTraceRGSLibDesc.DXILLibrary.pShaderBytecode = dxr.pathTraceRGS.bytecode->GetBufferPointer();
    pathTraceRGSLibDesc.NumExports = 1;
    pathTraceRGSLibDesc.pExports = &pathTraceRGSExportDesc;

    D3D12_STATE_SUBOBJECT pathTraceRGS = {};
    pathTraceRGS.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    pathTraceRGS.pDesc = &pathTraceRGSLibDesc;

    subobjects[index++] = pathTraceRGS;

    // Add state subobject for the Miss shader
    D3D12_EXPORT_DESC msExportDesc = {};
    msExportDesc.Name = dxr.miss.exportName;
    msExportDesc.ExportToRename = dxr.miss.entryPoint;

    D3D12_DXIL_LIBRARY_DESC msLibDesc = {};
    msLibDesc.DXILLibrary.BytecodeLength = dxr.miss.bytecode->GetBufferSize();
    msLibDesc.DXILLibrary.pShaderBytecode = dxr.miss.bytecode->GetBufferPointer();
    msLibDesc.NumExports = 1;
    msLibDesc.pExports = &msExportDesc;

    D3D12_STATE_SUBOBJECT ms = {};
    ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    ms.pDesc = &msLibDesc;

    subobjects[index++] = ms;

    // Add a state subobject for the hit group's Closest Hit Shader
    D3D12_EXPORT_DESC chsExportDesc = {};
    chsExportDesc.Name = dxr.hit.chs.exportName;
    chsExportDesc.ExportToRename = dxr.hit.chs.entryPoint;

    D3D12_DXIL_LIBRARY_DESC chsLibDesc = {};
    chsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.chs.bytecode->GetBufferSize();
    chsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.chs.bytecode->GetBufferPointer();
    chsLibDesc.NumExports = 1;
    chsLibDesc.pExports = &chsExportDesc;

    D3D12_STATE_SUBOBJECT chs = {};
    chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    chs.pDesc = &chsLibDesc;

    subobjects[index++] = chs;

    // Add a state subobject for the hit group's Any Hit Shader
    D3D12_EXPORT_DESC ahsExportDesc = {};
    ahsExportDesc.Name = dxr.hit.ahs.exportName;
    ahsExportDesc.ExportToRename = dxr.hit.ahs.entryPoint;

    D3D12_DXIL_LIBRARY_DESC ahsLibDesc = {};
    ahsLibDesc.DXILLibrary.BytecodeLength = dxr.hit.ahs.bytecode->GetBufferSize();
    ahsLibDesc.DXILLibrary.pShaderBytecode = dxr.hit.ahs.bytecode->GetBufferPointer();
    ahsLibDesc.NumExports = 1;
    ahsLibDesc.pExports = &ahsExportDesc;

    D3D12_STATE_SUBOBJECT ahs = {};
    ahs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    ahs.pDesc = &ahsLibDesc;

    subobjects[index++] = ahs;

    // Add a state subobject for the hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.ClosestHitShaderImport = dxr.hit.chs.exportName;
    hitGroupDesc.AnyHitShaderImport = dxr.hit.ahs.exportName;
    hitGroupDesc.HitGroupExport = dxr.hit.exportName;

    D3D12_STATE_SUBOBJECT hitGroup = {};
    hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroup.pDesc = &hitGroupDesc;

    subobjects[index++] = hitGroup;

    // Add a state subobject for the hit group's local root signature 
    D3D12_STATE_SUBOBJECT hitGroupRootSigObject = {};
    hitGroupRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    hitGroupRootSigObject.pDesc = &dxr.hit.pRootSignature;

    subobjects[index++] = hitGroupRootSigObject;

    // Create a list of the shader export names that use the local root signature
    const WCHAR* rootSigExports[] = { dxr.hit.exportName };

    // Add a state subobject for the association between the hit group and its local root signature
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rootSigAssociation = {};
    rootSigAssociation.NumExports = _countof(rootSigExports);
    rootSigAssociation.pExports = rootSigExports;
    rootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

    D3D12_STATE_SUBOBJECT rootSigAssociationObject = {};
    rootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    rootSigAssociationObject.pDesc = &rootSigAssociation;

    subobjects[index++] = rootSigAssociationObject;

    // Add a state subobject for the shader payload configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
    shaderDesc.MaxPayloadSizeInBytes = sizeof(float) * 10;  //sizeof(RTCommon.hlsl::PackedPayload)
    shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

    D3D12_STATE_SUBOBJECT shaderConfigObject = {};
    shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigObject.pDesc = &shaderDesc;

    subobjects[index++] = shaderConfigObject;

    // Create a list of the shader export names that use the payload
    const WCHAR* shaderExports[] =
    {
        dxr.probeRGS.exportName,
        dxr.primaryRGS.exportName,
        dxr.ambientOcclusionRGS.exportName,
        dxr.probeVisRGS.exportName,
        dxr.pathTraceRGS.exportName,
        dxr.miss.exportName,
        dxr.hit.exportName
    };

    // Add a state subobject for the association between shaders and the payload
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
    shaderPayloadAssociation.NumExports = _countof(shaderExports);
    shaderPayloadAssociation.pExports = shaderExports;
    shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

    D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
    shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

    subobjects[index++] = shaderPayloadAssociationObject;

    // Add a state subobject for the global root signature
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
#if RTXGI_NAME_D3D_OBJECTS
    dxr.rtpso->SetName(L"RTPSO");
#endif

    // Get the RTPSO properties
    hr = dxr.rtpso->QueryInterface(IID_PPV_ARGS(&dxr.rtpsoInfo));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.rtpso->SetName(L"RTPSO Info");
#endif

    return true;
}

/**
* Create the DXR shader table.
*/
bool CreateShaderTable(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, Scene &scene)
{
    /*
    The Shader Table layout is as follows:
        Entry 0: Probe Ray Trace Ray Generation Shader
        Entry 1: Primary Ray Trace Ray Generation Shader
        Entry 2: Ambient Occlusion Ray Generation Shader
        Entry 3: Probe Vis Ray Trace Ray Generation Shader
        Entry 4: Path Trace Ray Generation Shader
        Entry 5: Miss Shader
        Entry 6+: Hit Groups
    All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
    The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT.
    The CHS requires the largest entry:
        32 bytes for the program identifier
      + 64 bytes for material constants data
      +  8 bytes for index buffer VA
      +  8 bytes for vertex buffer VA
      +  8 bytes for descriptor table VA
      +  8 bytes for sampler descriptor table VA
      = 128 bytes ->> aligns to 128 bytes
    */

    UINT shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    UINT shaderTableSize = 0;

    dxr.shaderTableRecordSize = shaderIdSize;
    dxr.shaderTableRecordSize += sizeof(GPUMaterial);   // material constants data
    dxr.shaderTableRecordSize += 8;                     // index buffer GPUVA
    dxr.shaderTableRecordSize += 8;                     // vertex buffer GPUVA
    dxr.shaderTableRecordSize += 8;                     // descriptor table GPUVA
    dxr.shaderTableRecordSize += 8;                     // sampler descriptor table GPUVA
    dxr.shaderTableRecordSize = RTXGI_ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.shaderTableRecordSize);

    // 7 default shader records in the table + a record for each mesh
    shaderTableSize = (dxr.shaderTableRecordSize * (7 + scene.numGeometries));
    shaderTableSize = RTXGI_ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);

    // Create the shader table resource
    D3D12BufferInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.shaderTable)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.shaderTable->SetName(L"DXR Shader Table");
#endif

    // Map the shader table buffer
    UINT8* pData = nullptr;
    HRESULT hr = dxr.shaderTable->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    if (FAILED(hr)) return false;

    // Shader Record 0 - Probe Ray Trace Ray Generation program (no local root parameter data)
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.probeRGS.exportName), shaderIdSize);

    // Shader Record 1 - Primary Ray Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.primaryRGS.exportName), shaderIdSize);

    // Shader Record 2 - Ambient Occlusion Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.ambientOcclusionRGS.exportName), shaderIdSize);

    // Shader Record 3 - Probe Vis Ray Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.probeVisRGS.exportName), shaderIdSize);
    
    // Shader Record 4 - Path Trace Ray Generation program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.pathTraceRGS.exportName), shaderIdSize);

    // Shader Record 5 - Miss program (no local root parameter data)
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.miss.exportName), shaderIdSize);

    // Shader Record 6 - Hit Group program (visualization hits) and local root parameter data
    pData += dxr.shaderTableRecordSize;
    memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.hit.exportName), shaderIdSize);

    GPUMaterial defaultMaterial;
    memcpy(pData + shaderIdSize, &defaultMaterial, sizeof(GPUMaterial));   // bind a default material (no textures)
    *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize + sizeof(GPUMaterial)) = resources.sphereIB->GetGPUVirtualAddress();
    *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize + sizeof(GPUMaterial) + 8) = resources.sphereVB->GetGPUVirtualAddress();
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + sizeof(GPUMaterial) + 16) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

    // Shader Records 7+ - Hit Groups (for probe and primary hits) and local root parameter data
    // One Hit Group shader record for each MeshPrimitive / BLAS
    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); meshIndex++)
    {
        const Mesh mesh = scene.meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            UINT8 offset = 0;
            const MeshPrimitive primitive = mesh.primitives[primitiveIndex];
            const Material material = scene.materials[primitive.material];

            // Write the hit group shader identifier
            pData += dxr.shaderTableRecordSize;
            memcpy(pData, dxr.rtpsoInfo->GetShaderIdentifier(dxr.hit.exportName), shaderIdSize);

            // Write the material information (root constants)
            offset += shaderIdSize;
            memcpy(pData + offset, &material.data, sizeof(GPUMaterial));

            // Point to the index buffer
            offset += sizeof(GPUMaterial);
            *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + offset) = resources.sceneIBViews[primitive.index].BufferLocation;

            // Point to the vertex buffer
            offset += 8;
            *reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + offset) = resources.sceneVBViews[primitive.index].BufferLocation;

            // Point to the start of the CBV/SRV/UAV descriptor heap
            offset += 8;
            *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + offset) = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
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
bool Initialize(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, ShaderCompiler &compiler, Scene &scene)
{
    if (!CreateBLAS(d3d, dxr, resources, scene)) return false;
    if (!CreateProbeBLAS(d3d, dxr, resources)) return false;
    if (!CreateTLAS(d3d, dxr, resources, scene)) return false;
    if (!CreateGlobalRootSignature(d3d, dxr)) return false;
    if (!CreateRayGenPrograms(d3d, dxr, compiler)) return false;
    if (!CreateMissProgram(d3d, dxr, compiler)) return false;
    if (!CreateHitGroup(d3d, dxr, compiler, scene)) return false;
    if (!CreateVisUpdateTLASProgram(d3d, dxr, compiler)) return false;
    if (!CreateVisUpdateTLASPSO(d3d, dxr)) return false;
    if (!CreatePipelineStateObject(d3d, dxr, resources)) return false;
    if (!CreateShaderTable(d3d, dxr, resources, scene)) return false;
    return true;
}

/**
* Create the top level acceleration structure (TLAS) for the probe visualization.
*/
bool CreateVisTLAS(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, int numProbes, size_t index)
{
    // Release the visualization TLAS, if one already exists
    dxr.visTLASes[index].Release();

    // Create the TLAS instance buffer
    UINT64 size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * numProbes;
    D3D12BufferInfo instanceBufferInfo(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, instanceBufferInfo, &dxr.visTLASes[index].pInstanceDesc)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLASes[index].pInstanceDesc->SetName(L"DXR Vis TLAS Instances");
#endif

    // Create the view
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the VisTLAS instance data UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC visTLASInstanceDataUAVDesc = {};
    visTLASInstanceDataUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    visTLASInstanceDataUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    visTLASInstanceDataUAVDesc.Buffer.FirstElement = 0;
    visTLASInstanceDataUAVDesc.Buffer.NumElements = numProbes;
    visTLASInstanceDataUAVDesc.Buffer.StructureByteStride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    visTLASInstanceDataUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    visTLASInstanceDataUAVDesc.Buffer.CounterOffsetInBytes = 0;

    handle.ptr += (resources.cbvSrvUavDescSize * (DescriptorHeapConstants::VIS_TLAS_OFFSET + index));        // Vis TLAS instances are 10th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(dxr.visTLASes[index].pInstanceDesc, nullptr, &visTLASInstanceDataUAVDesc, handle);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = dxr.visTLASes[index].pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = numProbes;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = RTXGI_ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    dxr.visTlasSizes[index] = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    D3D12BufferInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.visTLASes[index].pScratch)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLASes[index].pScratch->SetName(L"DXR Vis TLAS Scratch");
#endif

    // Create the TLAS buffer
    bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
    bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &dxr.visTLASes[index].pResult)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    dxr.visTLASes[index].pResult->SetName(L"DXR Vis TLAS");
#endif

    // Write instance descriptions and build the acceleration structure
    UpdateVisTLAS(d3d, dxr, resources, numProbes, 1.f);

    return true;
}

/**
* Update the TLAS instances for the visualization probes and rebuild the TLAS.
*
* Called every frame. If the number of probes changes, the caller is responsible
* for freeing the TLAS buffers and calling CreateVisTLAS() to reallocate them 
* before calling UpdateVisTLAS() again.
*/
bool UpdateVisTLAS(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, int numProbes, float probeRadius, size_t index)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(255, 255, 0), "Update VisTLAS");
#endif

    // Transition the instance buffer to unordered access
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxr.visTLASes[index].pInstanceDesc;
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
    UINT64 groupOffset = (UINT64(d3d.frameIndex) * resources.numVolumes) * GetDDGIVolumeConstantBufferSize();
    d3d.cmdList->SetComputeRootConstantBufferView(0, resources.volumeGroupCB->GetGPUVirtualAddress() + groupOffset);

    // Set descriptor tables
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set root constants
    d3d.cmdList->SetComputeRoot32BitConstants(5, 2, &blasHandle, 0);
    d3d.cmdList->SetComputeRoot32BitConstant(5, *(UINT *)&probeRadius, 2);
    d3d.cmdList->SetComputeRoot32BitConstant(7, static_cast<UINT>(index), 0);

    // Set the compute PSO and dispatch
    d3d.cmdList->SetPipelineState(dxr.visUpdateTLASPSO);
    d3d.cmdList->Dispatch(numProbes, 1, 1);

    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.visTLASes[index].pInstanceDesc;

    // Wait for the compute pass to finish
    d3d.cmdList->ResourceBarrier(1, &barrier);

    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxr.visTLASes[index].pInstanceDesc;
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
    ASInputs.InstanceDescs = dxr.visTLASes[index].pInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = numProbes;
    ASInputs.Flags = buildFlags;

    // Describe and build the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = dxr.visTLASes[index].pScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = dxr.visTLASes[index].pResult->GetGPUVirtualAddress();

    d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the TLAS build to complete
    barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = dxr.visTLASes[index].pResult;

    d3d.cmdList->ResourceBarrier(1, &barrier);

    return true;
}

/*
* Release DXR specific GPU resources.
*/
void Cleanup(DXRGlobal &dxr)
{
    // Release shader table
    RTXGI_SAFE_RELEASE(dxr.shaderTable);

    // Release shaders
    dxr.probeRGS.Release();
    dxr.primaryRGS.Release();
    dxr.ambientOcclusionRGS.Release();
    dxr.probeVisRGS.Release();
    dxr.pathTraceRGS.Release();
    dxr.miss.Release();
    dxr.hit.Release();
    dxr.visUpdateTLASCS->Release();

    // Release root signatures and PSOs
    RTXGI_SAFE_RELEASE(dxr.globalRootSig);
    RTXGI_SAFE_RELEASE(dxr.visUpdateTLASPSO);
    RTXGI_SAFE_RELEASE(dxr.rtpso);
    RTXGI_SAFE_RELEASE(dxr.rtpsoInfo);

    // Release acceleration structures
    for (size_t i = 0; i < dxr.BLASes.size(); i++)
    {
        dxr.BLASes[i].Release();
    }
    dxr.probeBLAS.Release();
    dxr.TLAS.Release();

    for (size_t i = 0; i < dxr.visTLASes.size(); i++)
    {
        dxr.visTLASes[i].Release();
    }
}

}
