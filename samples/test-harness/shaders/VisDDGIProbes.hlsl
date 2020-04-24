/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../../rtxgi-sdk/shaders/ddgi/ProbeCommon.hlsl"

#include "include/Common.hlsl"
#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    RayDesc ray;
    ray.Origin = cameraOrigin;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the primary ray direction
    float  halfHeight = cameraTanHalfFovY;
    float  halfWidth = (cameraAspect * halfHeight);
    float3 lowerLeftCorner = cameraOrigin - (halfWidth * cameraRight) - (halfHeight * cameraUp) + cameraForward;
    float3 horizontal = (2.f * halfWidth) * cameraRight;
    float3 vertical = (2.f * halfHeight) * cameraUp;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    // Primary Ray Trace
    PayloadData payload = (PayloadData)0;
    TraceRay(
        SceneBVH,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        1,
        0,
        ray,
        payload);

    if (payload.hitT > 0.f)
    {
        // If the scene's primary ray hit missed geometry or 
        // the probe vis primary ray hit a probe, and the probe is the
        // closest surface, overwrite RTGBufferA with probe vis data.
        float depth = RTGBufferB[LaunchIndex].w;
        if(depth < 0.f || payload.hitT < depth)
        {
            float3 result = 0.f;
#if RTXGI_DDGI_PROBE_RELOCATION
            float3 probePosition =
                DDGIGetProbeWorldPositionWithOffset(
                    payload.instanceIndex,
                    DDGIVolume.origin,
                    DDGIVolume.probeGridCounts,
                    DDGIVolume.probeGridSpacing,
                    DDGIProbeOffsets);
#else
            float3 probePosition =
                DDGIGetProbeWorldPosition(
                    payload.instanceIndex,
                    DDGIVolume.origin,
                    DDGIVolume.probeGridCounts,
                    DDGIVolume.probeGridSpacing);
#endif

            float3 sampleDirection = normalize(payload.worldPosition - probePosition);
            float2 coords = DDGIGetOctahedralCoordinates(sampleDirection);
            
            // Irradiance
            float2 uv = DDGIGetProbeUV(payload.instanceIndex, coords, DDGIVolume.probeGridCounts, DDGIVolume.probeNumIrradianceTexels);
            result = DDGIProbeIrradianceSRV.SampleLevel(TrilinearSampler, uv, 0).rgb;

            // Filtered Distance
            /*float2 uv = DDGIGetProbeUV(payload.instanceIndex, coords, DDGIVolume.probeGridCounts, DDGIVolume.probeNumDistanceTexels);
            float  distance = DDGIProbeDistanceSRV.SampleLevel(TrilinearSampler, uv, 0).r;
            result = float3(distance, distance, distance) / 2.f;*/

            // Decode the tone curve
            float3 exponent = DDGIVolume.probeIrradianceEncodingGamma * 0.5f;
            result = pow(result, exponent);

            // Go back to linear irradiance
            result *= result;

            // Factored out of the probes
            result *= (0.5f * RTXGI_PI);

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            const float3 INACTIVE_COLOR = float3(1.f, 0.f, 0.f);          // Red
            const float3 ACTIVE_COLOR = float3(0.f, 1.f, 0.f);            // Green

            int probeXY = DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y;
            int2 probeStateTexcoord = int2(payload.instanceIndex % probeXY, payload.instanceIndex / probeXY);
            uint state = DDGIProbeStates.Load(probeStateTexcoord).r;

            // Border visualization for probe states
            if (abs(dot(normalize(probePosition - cameraOrigin), sampleDirection)) < 0.45f)
            {
                if (state == PROBE_STATE_ACTIVE)
                {
                    result = ACTIVE_COLOR;
                }
                else if (state == PROBE_STATE_INACTIVE)
                {
                    result = INACTIVE_COLOR;
                }
            }
#endif

            // Apply tonemapping
            result = ACESFilm(result);

            // Gamma correct
            result = LinearToSRGB(result);

            RTGBufferA[LaunchIndex] = float4(result, 0.f);
        }
    }
}
