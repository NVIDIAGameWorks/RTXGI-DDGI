/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

#include "include/Common.hlsl"
#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/LightingCommon.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    DDGIVolumeDescGPU DDGIVolume = DDGIVolumes.volumes[volumeSelect];
    float4 result = 0.f;

    uint2 DispatchIndex = DispatchRaysIndex().xy;
    int rayIndex = DispatchIndex.x;                    // index of ray within a probe
    int probeIndex = DispatchIndex.y;                  // index of current probe

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    RWTexture2D<uint> DDGIProbeStates = GetDDGIProbeStatesUAV(volumeSelect);
#if RTXGI_DDGI_PROBE_SCROLL
    int storageProbeIndex = DDGIGetProbeIndexOffset(probeIndex, DDGIVolume.probeGridCounts, DDGIVolume.probeScrollOffsets);
#else
    int storageProbeIndex = probeIndex;
#endif
    int2 texelPosition = DDGIGetProbeTexelPosition(storageProbeIndex, DDGIVolume.probeGridCounts);
    int  probeState = DDGIProbeStates[texelPosition];
    if (probeState == PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS)
    {
       // if the probe is inactive, do not shoot rays, unless it is one of the fixed rays that could potentially reactivate the probe
       return;
    }
#endif

#if RTXGI_DDGI_PROBE_RELOCATION
    RWTexture2D<float4> DDGIProbeOffsets = GetDDGIProbeOffsetsUAV(volumeSelect);
#if RTXGI_DDGI_PROBE_SCROLL
    float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing, DDGIVolume.probeScrollOffsets, DDGIProbeOffsets);
#else
    float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing, DDGIProbeOffsets);
#endif
#else
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing);
#endif

    float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform);

    // Setup the probe ray
    RayDesc ray;
    ray.Origin = probeWorldPosition;
    ray.Direction = probeRayDirection;
    ray.TMin = 0.f;
    ray.TMax = DDGIVolume.probeMaxRayDistance;

    // Probe Ray Trace
    PackedPayload packedPayload = (PackedPayload)0;
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    // Pass in the probeState flag as the first uint.
    // It is overwritten by at the end of TraceRay.
    packedPayload.albedoAndNormal.x = probeState;
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

    RWTexture2D<float4> DDGIProbeRTRadiance = GetDDGIProbeRTRadianceUAV(volumeSelect);

    // Ray miss. Set hit distance to a large value and exit early.
    if (packedPayload.hitT < 0.f)
    {
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        DDGIProbeRTRadiance[DispatchIndex.xy] = float4(SkyIntensity.xxx, 1e27f);
#else
        DDGIProbeRTRadiance[DispatchIndex.xy] = float4(asfloat(RTXGIFloat3ToUint(SkyIntensity.xxx)), 1e27f, 0.f, 0.f);
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
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        DDGIProbeRTRadiance[DispatchIndex.xy].w = -payload.hitT * 0.2f;
#else
        DDGIProbeRTRadiance[DispatchIndex.xy].g = -payload.hitT * 0.2f;
#endif
        return;
    }

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    if (probeState == PROBE_STATE_INACTIVE)
    {
        // Hit a frontface, but probe is inactive, so this ray will only be used for reclassification, don't need any lighting
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        DDGIProbeRTRadiance[DispatchIndex.xy].w = payload.hitT;
#else
        DDGIProbeRTRadiance[DispatchIndex.xy].g = payload.hitT;
#endif
        return;
    }
#endif

    // Direct Lighting and Shadowing
    float3 diffuse = DirectDiffuseLighting(payload, NormalBias, ViewBias, SceneBVH);

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
#if RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE
    Texture2D<float4> DDGIProbeIrradianceSRV = GetDDGIProbeIrradianceSRV(volumeSelect);
    Texture2D<float4> DDGIProbeDistanceSRV = GetDDGIProbeDistanceSRV(volumeSelect);

    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, DDGIVolume);

    DDGIVolumeResources resources;
    resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
    resources.probeDistanceSRV = DDGIProbeDistanceSRV;
    resources.bilinearSampler = BilinearSampler;
#if RTXGI_DDGI_PROBE_RELOCATION
    resources.probeOffsets = DDGIProbeOffsets;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    resources.probeStates = DDGIProbeStates;

#endif

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
#endif

    // Perfectly diffuse reflectors don't exist in the real world. Limit the BRDF
    // albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9f;

    // Compute final color
    result = float4(diffuse + ((min(payload.albedo, maxAlbedo) / PI) * irradiance), payload.hitT);

#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
    // Use R32G32B32A32_FLOAT format. Store color components and hit distance as 32-bit float values.
    DDGIProbeRTRadiance[DispatchIndex.xy] = result;
#else
    // Use R32G32_FLOAT format (don't use R32G32_UINT since hit distance needs to be negative sometimes).
    // Pack color as R10G10B10 in R32 and store hit distance in G32.
    static const float c_threshold = 1.f / 255.f;
    if (RTXGIMaxComponent(result.rgb) <= c_threshold) result.rgb = float3(0.f, 0.f, 0.f);
    DDGIProbeRTRadiance[DispatchIndex.xy] = float4(asfloat(RTXGIFloat3ToUint(result.rgb)), payload.hitT, 0.f, 0.f);
#endif
}
