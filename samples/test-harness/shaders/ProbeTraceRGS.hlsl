/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
    float4 result = 0.f;

    uint2 DispatchIndex = DispatchRaysIndex().xy;
    int rayIndex = DispatchIndex.x;                    // index of ray within a probe
    int probeIndex = DispatchIndex.y;                  // index of current probe

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    int2 texelPosition = DDGIGetProbeTexelPosition(probeIndex, DDGIVolume.probeGridCounts);
    int  probeState = DDGIProbeStates[texelPosition];
    if (probeState == PROBE_STATE_INACTIVE)
    {
       return;  // if the probe is inactive, do not shoot rays
    }
#endif

#if RTXGI_DDGI_PROBE_RELOCATION
    float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing, DDGIProbeOffsets);
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
    PayloadData payload = (PayloadData)0;
    TraceRay(
        SceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        ray,
        payload);

    result = float4(payload.baseColor, payload.hitT);

    // Ray miss. Set hit distance to a large value and exit early.
    if (payload.hitT < 0.f)
    {
        result.w = 1e27f;
        DDGIProbeRTRadiance[DispatchIndex.xy] = result;
        return;
    }

    // Hit a surface backface. Set the radiance to black and exit early.
    if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        // Shorten the hit distance on a backface hit by 20%
        // Make distance negative to encode backface for the probe position preprocess.
        DDGIProbeRTRadiance[DispatchIndex.xy] = float4(0.f, 0.f, 0.f, -payload.hitT * 0.2f);
        return;
    }

    // Direct Lighting and Shadowing
    float3 diffuse = DirectDiffuseLighting(
        payload.baseColor,
        payload.worldPosition,
        payload.normal,
        NormalBias,
        ViewBias,
        SceneBVH);

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
#if RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE
    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, DDGIVolume);

    DDGIVolumeResources resources;
    resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
    resources.probeDistanceSRV = DDGIProbeDistanceSRV;
    resources.trilinearSampler = TrilinearSampler;
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
    
    // Compute final color
    result = float4(diffuse + ((payload.baseColor / PI) * irradiance), payload.hitT);

    DDGIProbeRTRadiance[DispatchIndex.xy] = result;
}
