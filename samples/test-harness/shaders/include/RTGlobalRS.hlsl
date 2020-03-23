/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RT_GLOBAL_ROOT_SIGNATURE_HLSL
#define RT_GLOBAL_ROOT_SIGNATURE_HLSL

#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../../include/Lights.h"

struct TLASInstance
{
    float3x4    transform;
    uint        instanceID24_Mask8;
    uint        instanceContributionToHitGroupIndex24_Flags8;
    uint2       GPUAddress;
};

// ---- CBV/SRV/UAV Descriptor Heap ------------------------------------------------

cbuffer CameraCB : register(b1)
{
    float3    cameraOrigin;
    float     cameraAspect;
    float3    cameraUp;
    float     cameraTanHalfFovY;
    float3    cameraRight;
    float     cameraFov;
    float3    cameraForward;
    float     cameraPad1;
};

// Used by CHS when *not* loading a binary scene file
cbuffer MaterialCB : register(b2)
{
    float4 materialColors[18];
};

cbuffer LightsCB : register(b3)
{
    uint  lightMask;
    uint3 lightCounts;
    DirectionalLightDescGPU directionalLight;
    PointLightDescGPU pointLight;
    SpotLightDescGPU spotLight;
};

RWTexture2D<float4>                        RTGBufferA                : register(u0);
RWTexture2D<float4>                        RTGBufferB                : register(u1);
RWTexture2D<float4>                        RTGBufferC                : register(u2);
RWTexture2D<float4>                        RTGBufferD                : register(u3);
RWTexture2D<float>                         RTAORaw                   : register(u4);
RWTexture2D<float>                         RTAOFiltered              : register(u5);
RWStructuredBuffer<TLASInstance>           VisTLASInstances          : register(u6);
RWTexture2D<float4>                        PTOutput                  : register(u7);
RWTexture2D<float4>                        PTAccumulation            : register(u8);

// ---- DDGIVolume Entries -----------
ConstantBuffer<DDGIVolumeDescGPU>        DDGIVolume                  : register(b1, space1);

RWTexture2D<float4>                      DDGIProbeRTRadiance         : register(u0, space1);
//RWTexture2D<float4>                    DDGIProbeIrradianceUAV      : register(u1, space1);    // not used by app (SDK only)
//RWTexture2D<float4>                    DDGIProbeDistanceUAV        : register(u2, space1);    // not used by app (SDK only)
RWTexture2D<float4>                      DDGIProbeOffsets            : register(u3, space1);
RWTexture2D<uint>                        DDGIProbeStates             : register(u4, space1);
// -----------------------------------

Texture2D<float4>                        DDGIProbeIrradianceSRV      : register(t0);
Texture2D<float4>                        DDGIProbeDistanceSRV        : register(t1);
RaytracingAccelerationStructure          SceneBVH                    : register(t2);
//ByteAddressBuffer                      Indices                     : register(t3);        // not used, part of local root signature
//ByteAddressBuffer                      Vertices                    : register(t4);        // not used, part of local root signature
Texture2D<float4>                        BlueNoiseRGB                : register(t5);

// --- Sampler Descriptor Heap------------------------------------------------------

SamplerState                            TrilinearSampler             : register(s0);
SamplerState                            PointSampler                 : register(s1);

// ---- Root Constants -------------------------------------------------------------

cbuffer NoiseRootConstants : register(b4)
{
    uint  ResolutionX;
    uint  FrameNumber;
    float Exposure;
    uint  UseRTAO;
    uint  ViewAO;
    float AORadius;
    float AOPower;
    float AOBias;
};

cbuffer VisTLASUpdateRootConstants : register (b5)
{
    uint2 BLASGPUAddress;    // 64-bit gpu address
    float VizProbeRadius;
    float VisTLASPad;
};

cbuffer RTRootConstants : register(b6)
{
    float NormalBias;
    float ViewBias;
    uint  NumBounces;
    float RTPad;
};

#endif /* RT_GLOBAL_ROOT_SIGNATURE_HLSL */
