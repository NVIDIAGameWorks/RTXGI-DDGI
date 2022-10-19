/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef DESCRIPTORS_HLSL
#define DESCRIPTORS_HLSL

#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../../include/graphics/Types.h"
#include "Platform.hlsl"

#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

#ifndef RTXGI_BINDLESS_TYPE
#error Required define RTXGI_BINDLESS_TYPE is not defined!
#endif

struct TLASInstance
{
#pragma pack_matrix(row_major)
    float3x4 transform;
#pragma pack_matrix(column_major)
    uint     instanceID24_Mask8;
    uint     instanceContributionToHitGroupIndex24_Flags8;
    uint2    blasAddress;
};

// Global Root / Push Constants ------------------------------------------------------------------------------------

VK_PUSH_CONST ConstantBuffer<GlobalConstants> GlobalConst : register(b0, space0);

#define GetGlobalConst(x, y) (GlobalConst.x##_##y)

uint GetPTSamplesPerPixel() { return (GetGlobalConst(pt, samplesPerPixel) & 0x7FFFFFFF); }
uint GetPTAntialiasing() { return (GetGlobalConst(pt, samplesPerPixel) & 0x80000000); }

uint HasDirectionalLight() { return GetGlobalConst(lighting, hasDirectionalLight); }
uint GetNumPointLights() { return GetGlobalConst(lighting, numPointLights); }
uint GetNumSpotLights() { return GetGlobalConst(lighting, numSpotLights); }

//----------------------------------------------------------------------------------------------------------------
// Root Signature Descriptors and Mappings
// ---------------------------------------------------------------------------------------------------------------

#if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

// Samplers -------------------------------------------------------------------------------------------------

VK_BINDING(0, 0) SamplerState                                Samplers[]          : register(s0, space0);

// Constant Buffers -----------------------------------------------------------------------------------------

VK_BINDING(1, 0) ConstantBuffer<Camera>                      CameraCB            : register(b1, space0);

// Structured Buffers ---------------------------------------------------------------------------------------

VK_BINDING(2, 0) StructuredBuffer<Light>                     Lights              : register(t2, space0);
VK_BINDING(3, 0) StructuredBuffer<Material>                  Materials           : register(t3, space0);
VK_BINDING(4, 0) StructuredBuffer<TLASInstance>              TLASInstances       : register(t4, space0);
VK_BINDING(5, 0) StructuredBuffer<DDGIVolumeDescGPUPacked>   DDGIVolumes         : register(t5, space0);
VK_BINDING(6, 0) StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless  : register(t6, space0);

VK_BINDING(7, 0) RWStructuredBuffer<TLASInstance>            RWTLASInstances     : register(u5, space0);

// Bindless Resources ---------------------------------------------------------------------------------------

VK_BINDING(8, 0) RWTexture2D<float4>                         RWTex2D[]           : register(u6, space0);
VK_BINDING(9, 0) RWTexture2DArray<float4>                    RWTex2DArray[]      : register(u6, space1);
VK_BINDING(10, 0) RaytracingAccelerationStructure            TLAS[]              : register(t7, space0);
VK_BINDING(11, 0) Texture2D                                  Tex2D[]             : register(t7, space1);
VK_BINDING(12, 0) Texture2DArray                             Tex2DArray[]        : register(t7, space2);
VK_BINDING(13, 0) ByteAddressBuffer                          ByteAddrBuffer[]    : register(t7, space3);

// Defines for Convenience ----------------------------------------------------------------------------------

#define PT_OUTPUT_INDEX 0
#define PT_ACCUMULATION_INDEX 1
#define GBUFFERA_INDEX 2
#define GBUFFERB_INDEX 3
#define GBUFFERC_INDEX 4
#define GBUFFERD_INDEX 5
#define RTAO_OUTPUT_INDEX 6
#define RTAO_RAW_INDEX 7
#define DDGI_OUTPUT_INDEX 8

#define SCENE_TLAS_INDEX 0
#define DDGIPROBEVIS_TLAS_INDEX 1

#define BLUE_NOISE_INDEX 0

#define SPHERE_INDEX_BUFFER_INDEX 0
#define SPHERE_VERTEX_BUFFER_INDEX 1
#define MATERIAL_INDICES_INDEX 2
#define GEOMETRY_BUFFERS_INDEX 3

// Sampler Accessor Functions ------------------------------------------------------------------------------

SamplerState GetBilinearWrapSampler() { return Samplers[0]; }
SamplerState GetPointClampSampler() { return Samplers[1]; }
SamplerState GetAnisoWrapSampler() { return Samplers[2]; }

// Resource Accessor Functions ------------------------------------------------------------------------------

#define GetCamera() CameraCB

StructuredBuffer<Light> GetLights() { return Lights; }

Material GetMaterial(uint index) { return Materials[index]; }
uint GetMaterialIndex(uint meshIndex) { return ByteAddrBuffer[MATERIAL_INDICES_INDEX].Load(meshIndex * 4); }

StructuredBuffer<DDGIVolumeDescGPUPacked> GetDDGIVolumeConstants(uint index) { return DDGIVolumes; }
StructuredBuffer<DDGIVolumeResourceIndices> GetDDGIVolumeResourceIndices(uint index) { return DDGIVolumeBindless; }

RWStructuredBuffer<TLASInstance> GetDDGIProbeVisTLASInstances() { return RWTLASInstances; }

RaytracingAccelerationStructure GetAccelerationStructure(uint index) { return TLAS[index]; }

ByteAddressBuffer GetSphereIndexBuffer() { return ByteAddrBuffer[SPHERE_INDEX_BUFFER_INDEX]; }
ByteAddressBuffer GetSphereVertexBuffer() { return ByteAddrBuffer[SPHERE_VERTEX_BUFFER_INDEX]; }

ByteAddressBuffer GetIndexBuffer(uint meshIndex) { return ByteAddrBuffer[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2)]; }
ByteAddressBuffer GetVertexBuffer(uint meshIndex) { return ByteAddrBuffer[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2) + 1]; }

// Bindless Resource Array Accessors ------------------------------------------------------------------------

RWTexture2D<float4> GetRWTex2D(uint index) { return RWTex2D[index]; }
Texture2D<float4> GetTex2D(uint index) { return Tex2D[index]; }

RWTexture2DArray<float4> GetRWTex2DArray(uint index) { return RWTex2DArray[index]; }
Texture2DArray<float4> GetTex2DArray(uint index) { return Tex2DArray[index]; }

#elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP

// Defines for Convenience ----------------------------------------------------------------------------------

#define CAMERA_INDEX 0
#define LIGHTS_INDEX 1
#define MATERIALS_INDEX 2
#define SCENE_TLAS_INSTANCES_INDEX 3
#define DDGIPROBEVIS_TLAS_INSTANCES_INDEX 6

#define PT_OUTPUT_INDEX 7
#define PT_ACCUMULATION_INDEX 8
#define GBUFFERA_INDEX 9
#define GBUFFERB_INDEX 10
#define GBUFFERC_INDEX 11
#define GBUFFERD_INDEX 12
#define RTAO_OUTPUT_INDEX 13
#define RTAO_RAW_INDEX 14
#define DDGI_OUTPUT_INDEX 15

#define SCENE_TLAS_INDEX 40
#define DDGIPROBEVIS_TLAS_INDEX 41

#define BLUE_NOISE_INDEX 42

#define SPHERE_INDEX_BUFFER_INDEX 368
#define SPHERE_VERTEX_BUFFER_INDEX 369
#define MATERIAL_INDICES_INDEX 370
#define GEOMETRY_BUFFERS_INDEX 371

// Sampler Accessor Functions ------------------------------------------------------------------------------

SamplerState GetBilinearWrapSampler() { return SamplerDescriptorHeap[0]; }
SamplerState GetPointClampSampler() { return SamplerDescriptorHeap[1]; }
SamplerState GetAnisoWrapSampler() { return SamplerDescriptorHeap[2]; }

// Resource Accessor Functions ------------------------------------------------------------------------------

#define GetCamera() ConstantBuffer<Camera>(ResourceDescriptorHeap[CAMERA_INDEX])

StructuredBuffer<Light> GetLights() { return StructuredBuffer<Light>(ResourceDescriptorHeap[LIGHTS_INDEX]); }

Material GetMaterial(uint index) { return StructuredBuffer<Material>(ResourceDescriptorHeap[MATERIALS_INDEX]).Load(index); }
uint GetMaterialIndex(uint meshIndex) { return ByteAddressBuffer(ResourceDescriptorHeap[MATERIAL_INDICES_INDEX]).Load(meshIndex * 4); }

StructuredBuffer<DDGIVolumeDescGPUPacked> GetDDGIVolumeConstants(uint index) { return ResourceDescriptorHeap[index]; }
StructuredBuffer<DDGIVolumeResourceIndices> GetDDGIVolumeResourceIndices(uint index) { return ResourceDescriptorHeap[index]; }

RWStructuredBuffer<TLASInstance> GetDDGIProbeVisTLASInstances() { return ResourceDescriptorHeap[DDGIPROBEVIS_TLAS_INSTANCES_INDEX]; }

RaytracingAccelerationStructure GetAccelerationStructure(uint index) { return ResourceDescriptorHeap[index];}

RWTexture2D<float4> GetRWTex2D(uint index) { return ResourceDescriptorHeap[index]; }
Texture2D<float4> GetTex2D(uint index) { return ResourceDescriptorHeap[index]; }

RWTexture2DArray<float4> GetRWTex2DArray(uint index) { return ResourceDescriptorHeap[index]; }
Texture2DArray<float4> GetTex2DArray(uint index) { return ResourceDescriptorHeap[index]; }

ByteAddressBuffer GetSphereIndexBuffer() { return ResourceDescriptorHeap[SPHERE_INDEX_BUFFER_INDEX]; }
ByteAddressBuffer GetSphereVertexBuffer() { return ResourceDescriptorHeap[SPHERE_VERTEX_BUFFER_INDEX]; }

ByteAddressBuffer GetIndexBuffer(uint meshIndex) { return ResourceDescriptorHeap[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2)]; }
ByteAddressBuffer GetVertexBuffer(uint meshIndex) { return ResourceDescriptorHeap[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2) + 1]; }

#endif // RTXGI_BINDLESS_TYPE

#endif // DESCRIPTORS_HLSL
