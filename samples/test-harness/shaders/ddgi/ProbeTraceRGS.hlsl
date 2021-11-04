/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../include/Descriptors.hlsl"
#include "../include/Lighting.hlsl"
#include "../include/RayTracing.hlsl"

#include "../../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 DispatchIndex = DispatchRaysIndex().xy;
    int rayIndex = DispatchIndex.x;                    // index of the current probe ray
    int probeIndex = DispatchIndex.y;                  // index of current probe

    // Get the DDGIVolume's index
#if SPIRV
    uint volumeIndex = Global.ddgi_volumeIndex;
#else
    uint volumeIndex = DDGI.volumeIndex;
#endif

    // Get the DDGIVolume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get the probe's grid coordinates
    float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

    // Adjust the probe index for the scroll offsets
    probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

    // Get the probe data texture
    Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);

    // Get the probe's state
    float probeState = DDGILoadProbeState(probeIndex, ProbeData, volume);

    // Early out: do not shoot rays when the probe is inactive *unless* it is one of the "fixed" rays used by probe classification
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS) return;

    // Get the probe's world position
    // Note: world positions are computed from probe coordinates *not* adjusted for infinite scrolling
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get a random normalized ray direction to use for a probe ray
    float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, volume);

    // Texture output coordinates
    // Note: probe index is the scroll adjusted index (if scrolling is enabled)
    uint2 texCoords = uint2(rayIndex, probeIndex);

    // Setup the probe ray
    RayDesc ray;
    ray.Origin = probeWorldPosition;
    ray.Direction = probeRayDirection;
    ray.TMin = 0.f;
    ray.TMax = volume.probeMaxRayDistance;

    // Setup the ray payload
    PackedPayload packedPayload = (PackedPayload)0;

    // If classification is enabled, pass the probe's state to hit shaders through the payload
    if(volume.probeClassificationEnabled) packedPayload.packed0.x = probeState;

    // Trace the Probe Ray
    TraceRay(
        SceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        ray,
        packedPayload);

    // Get the ray data texture
    RWTexture2D<float4> RayData = GetDDGIVolumeRayDataUAV(volumeIndex);

    // The ray missed. Store the miss radiance, set the hit distance to a large value, and exit early.
    if (packedPayload.hitT < 0.f)
    {
        // Store the ray miss
        DDGIStoreProbeRayMiss(RayData, texCoords, volume, GetGlobalConst(app, skyRadiance));
        return;
    }

    // Unpack the payload
    Payload payload = UnpackPayload(packedPayload);

    // The ray hit a surface backface.
    if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        // Store the ray backface hit
        DDGIStoreProbeRayBackfaceHit(RayData, texCoords, volume, payload.hitT);
        return;
    }

    // Early out: a "fixed" ray hit a front facing surface. Fixed rays are not blended since their direction
    // is not random and they would bias the irradiance estimate. Don't perform lighting for these rays.
    if(volume.probeClassificationEnabled && rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS)
    {
        // Store the ray front face hit distance (only)
        DDGIStoreProbeRayFrontfaceHit(RayData, texCoords, volume, payload.hitT);
        return;
    }

    // Direct Lighting and Shadowing
    float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneBVH);

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, volume);

    // Setup the volume resources needed for the irradiance query
    DDGIVolumeResources resources;
    resources.probeIrradiance = GetDDGIVolumeIrradianceSRV(volumeIndex);
    resources.probeDistance = GetDDGIVolumeDistanceSRV(volumeIndex);
    resources.probeData = GetDDGIVolumeProbeDataSRV(volumeIndex);
    resources.bilinearSampler = GetBilinearWrapSampler();

    // Compute volume blending weight
    float volumeBlendWeight = DDGIGetVolumeBlendWeight(payload.worldPosition, volume);

    // Don't evaluate irradiance when the surface is outside the volume
    if (volumeBlendWeight > 0)
    {
        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            payload.worldPosition,
            surfaceBias,
            payload.normal,
            volume,
            resources);

        // Attenuate irradiance by the blend weight
        irradiance *= volumeBlendWeight;
    }

    // Perfectly diffuse reflectors don't exist in the real world.
    // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9f;

    // Store the final ray radiance and hit distance
    float3 radiance = diffuse + ((min(payload.albedo, float3(maxAlbedo, maxAlbedo, maxAlbedo)) / PI) * irradiance);
    DDGIStoreProbeRayFrontfaceHit(RayData, texCoords, volume, radiance, payload.hitT);
}
