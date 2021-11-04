/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// -------- FEATURE DEFINES -----------------------------------------------------------------------

// RTXGI_DDGI_PROBE_RELOCATION must be passed in as a define at shader compilation time.
// This define specifies if probe relocation is enabled or disabled.
// Ex: RTXGI_DDGI_PROBE_RELOCATION [0|1]
#ifndef RTXGI_DDGI_PROBE_RELOCATION
#error Required define RTXGI_DDGI_PROBE_RELOCATION is not defined for ProbeTraceRGS.hlsl!
#endif

// RTXGI_DDGI_PROBE_CLASSIFICATION must be passed in as a define at shader compilation time.
// This define specifies if probe classification is enabled or disabled.
// Ex: RTXGI_DDGI_PROBE_CLASSIFICATION [0|1]
#ifndef RTXGI_DDGI_PROBE_CLASSIFICATION
#error Required define RTXGI_DDGI_PROBE_CLASSIFICATION is not defined for ProbeTraceRGS.hlsl!
#endif

// RTXGI_DDGI_VOLUME_INFINITE_SCROLLING must be passed in as a define at shader compilation time.
// This define specifies if infinite scrolling volume functionality is enabled or disabled.
// Ex: RTXGI_DDGI_VOLUME_INFINITE_SCROLLING [0|1]
#ifndef RTXGI_DDGI_VOLUME_INFINITE_SCROLLING
#error Required define RTXGI_DDGI_VOLUME_INFINITE_SCROLLING is not defined for ProbeTraceRGS.hlsl!
#endif

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// RTXGI_DDGI_FORMAT_PROBE_RAY_DATA must be passed in as a define at shader compilation time.
// This define specifies the format of the probe ray data texture.
// Ex: RTXGI_DDGI_FORMAT_PROBE_RAY_DATA 0 => R32G32_FLOAT
// Ex: RTXGI_DDGI_FORMAT_PROBE_RAY_DATA 1 => R32G32B32A32_FLOAT
#ifndef RTXGI_DDGI_FORMAT_PROBE_RAY_DATA
#error Required define RTXGI_DDGI_FORMAT_PROBE_RAY_DATA is not defined for ProbeTraceRGS.hlsl!
#endif

// -------------------------------------------------------------------------------------------

#include "../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

#include "include/Descriptors.hlsl"
#include "include/Lighting.hlsl"
#include "include/RayTracing.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    float4 result = 0.f;

    uint2 DispatchIndex = DispatchRaysIndex().xy;
    int rayIndex = DispatchIndex.x;                    // index of the current probe ray
    int probeIndex = DispatchIndex.y;                  // index of current probe

    // Get the DDGIVolume's constants
    DDGIVolumeDescGPU DDGIVolume = UnpackDDGIVolumeDescGPU(DDGIVolumes[DDGI.volumeIndex]);

#if RTXGI_DDGI_PROBE_RELOCATION || RTXGI_DDGI_PROBE_CLASSIFICATION
    Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(DDGI.volumeIndex);
#endif

#if RTXGI_DDGI_PROBE_CLASSIFICATION
    #if RTXGI_DDGI_VOLUME_INFINITE_SCROLLING
        int storageProbeIndex = DDGIGetProbeIndexOffset(probeIndex, DDGIVolume.probeCounts, DDGIVolume.probeScrollOffsets);
    #else
        int storageProbeIndex = probeIndex;
    #endif

    int2 texelPosition = DDGIGetProbeTexelPosition(storageProbeIndex, DDGIVolume.probeCounts);
    float probeState = ProbeData.Load(int3(texelPosition, 0)).w;
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS)
    {
       // Do not shoot rays when the probe is inactive *unless* it is one of the "fixed" rays used by probe classification
       return;
    }
#endif

#if RTXGI_DDGI_PROBE_RELOCATION
    #if RTXGI_DDGI_VOLUME_INFINITE_SCROLLING
        float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeCounts, DDGIVolume.probeSpacing, DDGIVolume.probeScrollOffsets, ProbeData);
    #else
        float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeCounts, DDGIVolume.probeSpacing, ProbeData);
    #endif
#else
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeIndex, DDGIVolume.origin, DDGIVolume.probeCounts, DDGIVolume.probeSpacing);
#endif

    float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, DDGIVolume.probeNumRays, DDGIVolume.probeRayRotation);

    // Setup the probe ray
    RayDesc ray;
    ray.Origin = probeWorldPosition;
    ray.Direction = probeRayDirection;
    ray.TMin = 0.f;
    ray.TMax = DDGIVolume.probeMaxRayDistance;

    // Trace the Probe Ray
    PackedPayload packedPayload = (PackedPayload)0;

#if RTXGI_DDGI_PROBE_CLASSIFICATION
    // Pass the probe's state flag to hit shaders through the payload
    packedPayload.packed0.x = probeState;
#endif

    TraceRay(
        SceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        ray,
        packedPayload);

    // Get a reference to the ray data texture
    RWTexture2D<float4> RayData = GetDDGIVolumeRayDataUAV(DDGI.volumeIndex);

    // The ray missed. Set hit distance to a large value and exit early.
    if (packedPayload.hitT < 0.f)
    {
    #if (RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 1)
        RayData[DispatchIndex.xy] = float4(GetGlobalConst(app, skyRadiance), 1e27f);
    #else // RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 0
        RayData[DispatchIndex.xy] = float4(asfloat(RTXGIFloat3ToUint(GetGlobalConst(app, skyRadiance))), 1e27f, 0.f, 0.f);
    #endif
        return;
    }

    // Unpack the payload
    Payload payload = UnpackPayload(packedPayload);

    // Hit a surface backface.
    if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        // Make hit distance negative to mark a backface hit for blending, probe relocation, and probe classification.
        // Shorten the hit distance on a backface hit by 80% to decrease the influence of the probe during irradiance sampling.
    #if (RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 1)
        RayData[DispatchIndex.xy].w = -payload.hitT * 0.2f;
    #else // RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 0
        RayData[DispatchIndex.xy].g = -payload.hitT * 0.2f;
    #endif
        return;
    }

#if RTXGI_DDGI_PROBE_CLASSIFICATION
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE)
    {
        // Hit a front face, but the probe is inactive. This ray is only used for classification, so don't need to do lighting.
    #if (RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 1)
        RayData[DispatchIndex.xy].w = payload.hitT;
    #else // RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 0
        RayData[DispatchIndex.xy].g = payload.hitT;
    #endif
        return;
    }
#endif

    // Direct Lighting and Shadowing
    float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneBVH);

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, DDGIVolume);

    DDGIVolumeResources resources;
    resources.probeIrradiance = GetDDGIVolumeIrradianceSRV(DDGI.volumeIndex);
    resources.probeDistance = GetDDGIVolumeDistanceSRV(DDGI.volumeIndex);
#if RTXGI_DDGI_PROBE_RELOCATION || RTXGI_DDGI_PROBE_CLASSIFICATION
    resources.probeData = ProbeData;
#endif
    resources.bilinearSampler = GetBilinearWrapSampler();

    // Compute volume blending weight
    float volumeBlendWeight = DDGIGetVolumeBlendWeight(payload.worldPosition, DDGIVolume);

    // Avoid evaluating irradiance when the surface is outside the volume
    if (volumeBlendWeight > 0)
    {
        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            payload.worldPosition,
            surfaceBias,
            payload.normal,
            DDGIVolume,
            resources);

        // Attenuate irradiance by the blend weight
        irradiance *= volumeBlendWeight;
    }

    // Perfectly diffuse reflectors don't exist in the real world. Limit the BRDF
    // albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9f;

    // Compute final color
    result = float4(diffuse + ((min(payload.albedo, maxAlbedo) / PI) * irradiance), payload.hitT);

#if (RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 1)
    // Use R32G32B32A32_FLOAT format. Store color components and hit distance as 32-bit float values.
    RayData[DispatchIndex.xy] = result;
#else // RTXGI_DDGI_FORMAT_PROBE_RAY_DATA == 0
    // Use R32G32_FLOAT format (don't use R32G32_UINT since hit distance needs to be negative sometimes).
    // Pack color as R10G10B10 in R32 and store hit distance in G32.
    static const float c_threshold = 1.f / 255.f;
    if (RTXGIMaxComponent(result.rgb) <= c_threshold) result.rgb = float3(0.f, 0.f, 0.f);
    RayData[DispatchIndex.xy] = float4(asfloat(RTXGIFloat3ToUint(result.rgb)), payload.hitT, 0.f, 0.f);
#endif
}
