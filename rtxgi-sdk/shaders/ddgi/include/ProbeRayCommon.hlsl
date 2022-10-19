/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_PROBE_RAY_COMMON_HLSL
#define RTXGI_DDGI_PROBE_RAY_COMMON_HLSL

#include "Common.hlsl"

//------------------------------------------------------------------------
// Probe Ray Data Texture Write Helpers
//------------------------------------------------------------------------

void DDGIStoreProbeRayMiss(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float3 radiance)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        RayData[coords] = float4(radiance, 1e27f);
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        RayData[coords] = float4(asfloat(RTXGIFloat3ToUint(radiance)), 1e27f, 0.f, 0.f);
    }
}

void DDGIStoreProbeRayFrontfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float3 radiance, float hitT)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        // Store color components and hit distance as 32-bit float values.
        RayData[coords] = float4(radiance, hitT);
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        // Use R32G32_FLOAT format (don't use R32G32_UINT since hit distance needs to be negative sometimes).
        // Pack color as R10G10B10 in R32 and store hit distance in G32.
        static const float c_threshold = 1.f / 255.f;
        if (RTXGIMaxComponent(radiance.rgb) <= c_threshold) radiance.rgb = float3(0.f, 0.f, 0.f);
        RayData[coords] = float4(asfloat(RTXGIFloat3ToUint(radiance.rgb)), hitT, 0.f, 0.f);
    }
}

void DDGIStoreProbeRayFrontfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float hitT)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        RayData[coords].w = hitT;
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        RayData[coords].g = hitT;
    }
}

void DDGIStoreProbeRayBackfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float hitT)
{
    // Make the hit distance negative to mark a backface hit for blending, probe relocation, and probe classification.
    // Shorten the hit distance on a backface hit by 80% to decrease the influence of the probe during irradiance sampling.
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        RayData[coords].w = -hitT * 0.2f;
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        RayData[coords].g = -hitT * 0.2f;
    }
}

//------------------------------------------------------------------------
// Probe Ray Data Texture Read Helpers
//------------------------------------------------------------------------

float3 DDGILoadProbeRayRadiance(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return RayData[coords].rgb;
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        return RTXGIUintToFloat3(asuint(RayData[coords].r));
    }
    return float3(0.f, 0.f, 0.f);
}

float DDGILoadProbeRayDistance(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume)
{
    if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        return RayData[coords].a;
    }
    else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
    {
        return RayData[coords].g;
    }
    return 0.f;
}

//------------------------------------------------------------------------
// Probe Ray Direction
//------------------------------------------------------------------------

/**
 * Computes a spherically distributed, normalized ray direction for the given ray index in a set of ray samples.
 * Applies the volume's random probe ray rotation transformation to "non-fixed" ray direction samples.
 */
float3 DDGIGetProbeRayDirection(int rayIndex, DDGIVolumeDescGPU volume)
{
    bool isFixedRay = false;
    int sampleIndex = rayIndex;
    int numRays = volume.probeNumRays;

    if (volume.probeRelocationEnabled || volume.probeClassificationEnabled)
    {
        isFixedRay = (rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS);
        sampleIndex = isFixedRay ? rayIndex : (rayIndex - RTXGI_DDGI_NUM_FIXED_RAYS);
        numRays = isFixedRay ? RTXGI_DDGI_NUM_FIXED_RAYS : (numRays - RTXGI_DDGI_NUM_FIXED_RAYS);
    }

    // Get a ray direction on the sphere
    float3 direction = RTXGISphericalFibonacci(sampleIndex, numRays);

    // Don't rotate fixed rays so relocation/classification are temporally stable
    if (isFixedRay) return normalize(direction);

    // Apply a random rotation and normalize the direction
    return normalize(RTXGIQuaternionRotate(direction, RTXGIQuaternionConjugate(volume.probeRayRotation)));
}


#endif // RTXGI_DDGI_PROBE_RAY_COMMON_HLSL
