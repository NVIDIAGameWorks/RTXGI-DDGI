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

#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIConstants.h"
#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../../include/graphics/Types.h"
#include "Platform.hlsl"

struct GeometryInstance
{
#pragma pack_matrix(row_major)
    float3x4 transform;
#pragma pack_matrix(column_major)
    uint     instanceID24_Mask8;
    uint     instanceContributionToHitGroupIndex24_Flags8;
    uint2    blasAddress;
};

// Samplers ----------------------------------------------------------------------------------

VK_BINDING(0, 0) SamplerState                        Samplers[]        : register(s0, space0);

// Root / Push Constants ---------------------------------------------------------------------

VK_PUSH_CONST ConstantBuffer<GlobalConstants>        Global            : register(b0, space0);

#if !SPIRV
ConstantBuffer<DDGIConstants>                        DDGI              : register(b0, space1);
#endif

// Constant Buffers --------------------------------------------------------------------------

VK_BINDING(1, 0) ConstantBuffer<Camera>              Camera            : register(b1, space0);

// Structured Buffers ------------------------------------------------------------------------

VK_BINDING(2, 0) StructuredBuffer<Light>             Lights            : register(t2, space0);
VK_BINDING(3, 0) StructuredBuffer<Material>          Materials         : register(t3, space0);
VK_BINDING(4, 0) StructuredBuffer<GeometryInstance>  Instances         : register(t4, space0);
VK_BINDING(5, 0) StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes : register(t5, space0);

// Bindless Resources ------------------------------------------------------------------------

VK_BINDING(6, 0) RWTexture2D<float4>                 RWTex2D[]         : register(u6, space0);
VK_BINDING(7, 0) RaytracingAccelerationStructure     BVH[]             : register(t7, space0);
VK_BINDING(8, 0) Texture2D                           Tex2D[]           : register(t7, space1);
VK_BINDING(9, 0) ByteAddressBuffer                   RawBuffer[]       : register(t7, space2);

// Defines for Convenience -------------------------------------------------------------------

#define BilinearWrapSampler Samplers[0]
#define PointClampSampler   Samplers[1]
#define AnisoWrapSampler    Samplers[2]

#define PTOutput            RWTex2D[0]
#define PTAccumulation      RWTex2D[1]
#define GBufferA            RWTex2D[2]
#define GBufferB            RWTex2D[3]
#define GBufferC            RWTex2D[4]
#define GBufferD            RWTex2D[5]
#define RTAOOutput          RWTex2D[6]
#define RTAORaw             RWTex2D[7]
#define DDGIOutput          RWTex2D[8]

#define SceneBVH            BVH[0]

#define BlueNoise           Tex2D[0]

// Global Constants / Push Constant Accessors ------------------------------------------------

#define GetGlobalConst(x, y) (Global.x##_##y)

uint   GetPTSamplesPerPixel() { return (Global.pt_samplesPerPixel & 0x7FFFFFFF); }
uint   GetPTAntialiasing() { return (Global.pt_samplesPerPixel & 0x80000000); }

uint   HasDirectionalLight() { return Global.lighting_hasDirectionalLight; }
uint   GetNumPointLights() { return Global.lighting_numPointLights; }
uint   GetNumSpotLights() { return Global.lighting_numSpotLights; }

// Resource Accessor Functions ---------------------------------------------------------------

SamplerState GetBilinearWrapSampler()
{
    return BilinearWrapSampler;
}

uint GetMaterialIndex(uint meshIndex)
{
    return RawBuffer[0].Load(meshIndex * 4);
}

ByteAddressBuffer GetIndexBuffer(uint meshIndex)
{
    return RawBuffer[1 + (meshIndex * 2)];
}

ByteAddressBuffer GetVertexBuffer(uint meshIndex)
{
    return RawBuffer[1 + (meshIndex * 2) + 1];
}

// DDGI Resources Accessors ------------------------------------------------------------------

RWTexture2D<float4> GetDDGIVolumeRayDataUAV(uint volumeIndex)
{
#if SPIRV
    uint uavOffset = Global.ddgi_uavOffset;
#else
    uint uavOffset = DDGI.uavOffset;
#endif
    uint index = uavOffset + (volumeIndex * 4);
    return RWTex2D[index];
}

RWTexture2D<float4> GetDDGIVolumeIrradianceUAV(uint volumeIndex)
{
#if SPIRV
    uint uavOffset = Global.ddgi_uavOffset;
#else
    uint uavOffset = DDGI.uavOffset;
#endif
    uint index = uavOffset + (volumeIndex * 4) + 1;
    return RWTex2D[index];
}

RWTexture2D<float4> GetDDGIVolumeDistanceUAV(uint volumeIndex)
{
#if SPIRV
    uint uavOffset = Global.ddgi_uavOffset;
#else
    uint uavOffset = DDGI.uavOffset;
#endif
    uint index = uavOffset + (volumeIndex * 4) + 2;
    return RWTex2D[index];
}

RWTexture2D<float4> GetDDGIVolumeProbeDataUAV(uint volumeIndex)
{
#if SPIRV
    uint uavOffset = Global.ddgi_uavOffset;
#else
    uint uavOffset = DDGI.uavOffset;
#endif
    uint index = uavOffset + (volumeIndex * 4) + 3;
    return RWTex2D[index];
}

Texture2D GetDDGIVolumeRayDataSRV(uint volumeIndex)
{
#if SPIRV
    uint srvOffset = Global.ddgi_srvOffset;
#else
    uint srvOffset = DDGI.srvOffset;
#endif
    uint index = srvOffset + (volumeIndex * 4);
    return Tex2D[index];
}

Texture2D GetDDGIVolumeIrradianceSRV(uint volumeIndex)
{
#if SPIRV
    uint srvOffset = Global.ddgi_srvOffset;
#else
    uint srvOffset = DDGI.srvOffset;
#endif
    uint index = srvOffset + (volumeIndex * 4) + 1;
    return Tex2D[index];
}

Texture2D GetDDGIVolumeDistanceSRV(uint volumeIndex)
{
#if SPIRV
    uint srvOffset = Global.ddgi_srvOffset;
#else
    uint srvOffset = DDGI.srvOffset;
#endif
    uint index = srvOffset + (volumeIndex * 4) + 2;
    return Tex2D[index];
}

Texture2D GetDDGIVolumeProbeDataSRV(uint volumeIndex)
{
#if SPIRV
    uint srvOffset = Global.ddgi_srvOffset;
#else
    uint srvOffset = DDGI.srvOffset;
#endif
    uint index = srvOffset + (volumeIndex * 4) + 3;
    return Tex2D[index];
}

#endif // DESCRIPTORS_HLSL
