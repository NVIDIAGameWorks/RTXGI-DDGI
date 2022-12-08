/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "../../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"
#include "../../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    // Get the DDGIVolume's index (from root/push constants)
    uint volumeIndex = GetDDGIVolumeIndex();

    // Get the DDGIVolume structured buffers
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
    StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the DDGIVolume's constants from the structured buffer
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Compute the probe index for this thread
    int rayIndex = DispatchRaysIndex().x;                    // index of the ray to trace for this probe
    int probePlaneIndex = DispatchRaysIndex().y;             // index of this probe within the plane of probes
    int planeIndex = DispatchRaysIndex().z;                  // index of the plane this probe is part of
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);

    int probeIndex = (planeIndex * probesPerPlane) + probePlaneIndex;

    // Get the probe's grid coordinates
    float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

    // Adjust the probe index for the scroll offsets
    probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's state
    float probeState = DDGILoadProbeState(probeIndex, ProbeData, volume);

    // Early out: do not shoot rays when the probe is inactive *unless* it is one of the "fixed" rays used by probe classification
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS) return;

    // Get the probe's world position
    // Note: world positions are computed from probe coordinates *not* adjusted for infinite scrolling
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get a random normalized ray direction to use for a probe ray
    float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, volume);

    // Get the coordinates for the probe ray in the RayData texture array
    // Note: probe index is the scroll adjusted index (if scrolling is enabled)
    uint3 outputCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

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

    // Get the acceleration structure
    RaytracingAccelerationStructure SceneTLAS = GetAccelerationStructure(SCENE_TLAS_INDEX);

#if GFX_NVAPI
    if (GetPTShaderExecutionReordering())
    {
        NvHitObject hit;
        NvTraceRayHitObject(
            SceneTLAS,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            0,
            0,
            0,
            ray,
            packedPayload,
            hit);
        NvReorderThread(hit, 0, 0);
        NvInvokeHitObject(SceneTLAS, hit, packedPayload);
    }
    else
    {
        TraceRay(
            SceneTLAS,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            0,
            0,
            0,
            ray,
            packedPayload);
    }
#else
    // Trace the Probe Ray
    TraceRay(
        SceneTLAS,
        RAY_FLAG_NONE,
        0xFF,
        0,
        0,
        0,
        ray,
        packedPayload);
#endif

    // Get the ray data texture array
    RWTexture2DArray<float4> RayData = GetRWTex2DArray(resourceIndices.rayDataUAVIndex);

    // The ray missed. Store the miss radiance, set the hit distance to a large value, and exit early.
    if (packedPayload.hitT < 0.f)
    {
        // Store the ray miss
        DDGIStoreProbeRayMiss(RayData, outputCoords, volume, GetGlobalConst(app, skyRadiance));
        return;
    }

    // Unpack the payload
    Payload payload = UnpackPayload(packedPayload);

    // The ray hit a surface backface
    if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        // Store the ray backface hit
        DDGIStoreProbeRayBackfaceHit(RayData, outputCoords, volume, payload.hitT);
        return;
    }

    // Early out: a "fixed" ray hit a front facing surface. Fixed rays are not blended since their direction
    // is not random and they would bias the irradiance estimate. Don't perform lighting for these rays.
    if((volume.probeRelocationEnabled || volume.probeClassificationEnabled) && rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS)
    {
        // Store the ray front face hit distance (only)
        DDGIStoreProbeRayFrontfaceHit(RayData, outputCoords, volume, payload.hitT);
        return;
    }

    // Get the (dynamic) lights
    StructuredBuffer<Light> Lights = GetLights();

    // Direct Lighting and Shadowing
    float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneTLAS, Lights);

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, volume);

    // Get the volume resources needed for the irradiance query
    DDGIVolumeResources resources;
    resources.probeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);
    resources.probeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);
    resources.probeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);
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
    DDGIStoreProbeRayFrontfaceHit(RayData, outputCoords, volume, radiance, payload.hitT);
}
