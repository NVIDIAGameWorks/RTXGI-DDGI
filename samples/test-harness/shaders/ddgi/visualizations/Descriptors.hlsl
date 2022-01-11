/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef DDGI_VIS_DESCRIPTORS_HLSL
#define DDGI_VIS_DESCRIPTORS_HLSL

#include "../../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIConstants.h"
#include "../../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../../../include/graphics/Types.h"
#include "../../include/Platform.hlsl"

struct GeometryInstance
{
#pragma pack_matrix(row_major)
    float3x4 transform;
#pragma pack_matrix(column_major)
    uint     instanceID24_Mask8;
    uint     instanceContributionToHitGroupIndex24_Flags8;
    uint2    blasAddress;
};

VK_BINDING(0, 0) SamplerState                              Samplers[]    : register(s0, space0);

VK_PUSH_CONST    ConstantBuffer<DDGIVisConstants>          Global        : register(b0, space0);

VK_BINDING(1, 0) ConstantBuffer<Camera>                    Camera        : register(b1, space0);

VK_BINDING(2, 0) RaytracingAccelerationStructure           SceneBVH      : register(t0, space0);
VK_BINDING(3, 0) StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes   : register(t1, space0);

VK_BINDING(4, 0) RWStructuredBuffer<GeometryInstance>      Instances     : register(u0, space0);
VK_BINDING(5, 0) RWTexture2D<float4>                       GBufferA      : register(u1, space0);
VK_BINDING(6, 0) RWTexture2D<float4>                       GBufferB      : register(u2, space0);

VK_BINDING(7, 0) ByteAddressBuffer                         RawBuffer[]   : register(t2, space0);

VK_BINDING(8, 0) Texture2D                                 Tex2D[]       : register(t2, space1);

// Defines for Convenience -------------------------------------------------------------------

#define BilinearWrapSampler Samplers[0]
#define PointClampSampler   Samplers[1]

#define RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE 0
#define RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE 1

// Global Constants / Push Constant Accessors ------------------------------------------------

#define GetGlobalConst(x, y) (Global.x##_##y)

// Resource Accessor Functions ---------------------------------------------------------------

SamplerState GetBilinearWrapSampler()
{
    return BilinearWrapSampler;
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

Texture2D GetDDGIVolumeRayDataSRV(uint volumeIndex)
{
    uint index = (volumeIndex * 4);
    return Tex2D[index];
}

Texture2D GetDDGIVolumeIrradianceSRV(uint volumeIndex)
{
    uint index = (volumeIndex * 4) + 1;
    return Tex2D[index];
}

Texture2D GetDDGIVolumeDistanceSRV(uint volumeIndex)
{
    uint index = (volumeIndex * 4) + 2;
    return Tex2D[index];
}

Texture2D GetDDGIVolumeProbeDataSRV(uint volumeIndex)
{
    uint index = (volumeIndex * 4) + 3;
    return Tex2D[index];
}

#endif // DDGI_VIS_DESCRIPTORS_HLSL
