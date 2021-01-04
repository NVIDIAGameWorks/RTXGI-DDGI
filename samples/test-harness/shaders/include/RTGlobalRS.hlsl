/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include "RSCommon.hlsl"

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
    float3    cameraPosition;
    float     cameraAspect;
    float3    cameraUp;
    float     cameraFov;
    float3    cameraRight;
    float     cameraTanHalfFovY;
    float3    cameraForward;
    int       cameraNumPaths;
};

cbuffer LightsCB : register(b2)
{
    uint  lightMask;
    uint3 lightCounts;
    DirectionalLightDescGPU directionalLight;
    PointLightDescGPU pointLight;
    SpotLightDescGPU spotLight;
};

RWTexture2D<float4>                      GBufferA                  : register(u0);
RWTexture2D<float4>                      GBufferB                  : register(u1);
RWTexture2D<float4>                      GBufferC                  : register(u2);
RWTexture2D<float4>                      GBufferD                  : register(u3);
RWTexture2D<float>                       RTAORaw                   : register(u4);
RWTexture2D<float>                       RTAOFiltered              : register(u5);
RWTexture2D<float4>                      PTOutput                  : register(u6);
RWTexture2D<float4>                      PTAccumulation            : register(u7);
RWStructuredBuffer<TLASInstance>         VisTLASInstances[]        : register(u0, space3);

Texture2D<float4>                        BlueNoiseRGB                : register(t0);
RaytracingAccelerationStructure          SceneBVH                    : register(t2);
//ByteAddressBuffer                      Indices                     : register(t3);        // not used, part of local root signature
//ByteAddressBuffer                      Vertices                    : register(t4);        // not used, part of local root signature

// --- Sampler Descriptor Heap------------------------------------------------------

SamplerState                             BilinearSampler              : register(s0);
SamplerState                             PointSampler                 : register(s1);

// ---- Root Constants -------------------------------------------------------------

cbuffer NoiseRootConstants : register(b4)
{
    uint  ResolutionX;
    uint  FrameNumber;
    uint  UseRTAO;
    uint  ViewAO;
    float AORadius;
    float AOPower;
    float AOBias;
    uint  UseTonemapping;
    uint  UseDithering;
    uint  UseExposure;
    float Exposure;
    float NoisePadding;
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
    float SkyIntensity;
};

#endif /* RT_GLOBAL_ROOT_SIGNATURE_HLSL */
