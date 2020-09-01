/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RASTER_ROOT_SIGNATURE_HLSL
#define RASTER_ROOT_SIGNATURE_HLSL

#include "../../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
#include "../../include/Lights.h"
#include "RSCommon.hlsl"

// ---- CBV/SRV/UAV Descriptor Heap ------------------------------------------------

cbuffer CameraCB : register(b1)
{
    float3    cameraPosition;
    float     cameraAspect;
    float3    cameraUp;
    float     cameraTanHalfFovY;
    float3    cameraRight;
    float     cameraFov;
    float3    cameraForward;
    float     cameraPad1;
};

cbuffer LightsCB : register(b2)
{
    uint  lightMask;
    uint3 lightCounts;
    DirectionalLightDescGPU directionalLight;
    PointLightDescGPU pointLight;
    SpotLightDescGPU spotLight;
};

RWTexture2D<float4>                    GBufferA                  : register(u0);
RWTexture2D<float4>                    GBufferB                  : register(u1);
RWTexture2D<float4>                    GBufferC                  : register(u2);
RWTexture2D<float4>                    GBufferD                  : register(u3);
RWTexture2D<float>                     RTAORaw                   : register(u4);
RWTexture2D<float>                     RTAOFiltered              : register(u5);

Texture2D<float4>                      BlueNoiseRGB              : register(t0);

// --- Sampler Descriptor Heap------------------------------------------------------

SamplerState                           BilinearSampler           : register(s0);
SamplerState                           PointSampler              : register(s1);

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

cbuffer RasterRootConstants : register(b5)
{
    uint  UseDDGI;
    float VizProbeRadius;
    float VizIrradianceScale;
    float VizDistanceScale;
    float VizRadianceScale;
    float VizOffsetScale;
    float VizStateScale;
    float VizDistanceDivisor;
};

#endif /* RASTER_ROOT_SIGNATURE_HLSL */
