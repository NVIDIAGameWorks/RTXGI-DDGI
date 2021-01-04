/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
    ray.Origin = cameraPosition;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the primary ray direction
    float  halfHeight = cameraTanHalfFovY;
    float  halfWidth = (cameraAspect * halfHeight);
    float3 lowerLeftCorner = cameraPosition - (halfWidth * cameraRight) - (halfHeight * cameraUp) + cameraForward;
    float3 horizontal = (2.f * halfWidth) * cameraRight;
    float3 vertical = (2.f * halfHeight) * cameraUp;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    // Primary Ray Trace
    PackedPayload packedPayload = (PackedPayload)0;
    TraceRay(
        SceneBVH,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        1,
        0,
        ray,
        packedPayload);

    if (packedPayload.hitT > 0.f)
    {
        // If the scene's primary ray hit missed geometry or 
        // the probe vis primary ray hit a probe, and the probe is the
        // closest surface, overwrite GBufferA with probe vis data.
        float depth = GBufferB[LaunchIndex].w;
        if(depth < 0.f || packedPayload.hitT < depth)
        {
            DDGIVolumeDescGPU DDGIVolume = DDGIVolumes.volumes[volumeSelect];
            Texture2D<float4> DDGIProbeIrradianceSRV = GetDDGIProbeIrradianceSRV(volumeSelect);
            float3 result = 0.f;

            // Unpack the payload
            Payload payload = UnpackPayload(packedPayload);

#if RTXGI_DDGI_PROBE_RELOCATION
            RWTexture2D<float4> DDGIProbeOffsets = GetDDGIProbeOffsetsUAV(volumeSelect);
            float3 probePosition =
                DDGIGetProbeWorldPositionWithOffset(
                    payload.instanceIndex,
                    DDGIVolume.origin,
                    DDGIVolume.probeGridCounts,
                    DDGIVolume.probeGridSpacing,
#if RTXGI_DDGI_PROBE_SCROLL
                    DDGIVolume.probeScrollOffsets,
#endif
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
#if RTXGI_DDGI_PROBE_SCROLL
            float2 uv = DDGIGetProbeUV(payload.instanceIndex, coords, DDGIVolume.probeGridCounts, DDGIVolume.probeNumIrradianceTexels, DDGIVolume.probeScrollOffsets);
#else
            float2 uv = DDGIGetProbeUV(payload.instanceIndex, coords, DDGIVolume.probeGridCounts, DDGIVolume.probeNumIrradianceTexels);
#endif
            result = DDGIProbeIrradianceSRV.SampleLevel(BilinearSampler, uv, 0).rgb;

            // Decode the tone curve
            float3 exponent = DDGIVolume.probeIrradianceEncodingGamma * 0.5f;
            result = pow(result, exponent);

            // Go back to linear irradiance
            result *= result;

            // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to normalize for the display.
            result *= 2;

#if !RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
            result *= 1.0989f;                      // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
#endif

            // Filtered Distance
            /*float2 uv = DDGIGetProbeUV(payload.instanceIndex, coords, DDGIVolume.probeGridCounts, DDGIVolume.probeNumDistanceTexels);
            float  distance = 2.f * DDGIProbeDistanceSRV.SampleLevel(BilinearSampler, uv, 0).r;
            result = float3(distance, distance, distance) / 2.f;*/

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            const float3 INACTIVE_COLOR = float3(1.f, 0.f, 0.f);      // Red
            const float3 ACTIVE_COLOR = float3(0.f, 1.f, 0.f);        // Green
            RWTexture2D<uint> DDGIProbeStates = GetDDGIProbeStatesUAV(volumeSelect);
#if RTXGI_DDGI_PROBE_SCROLL
            int probeIndex = DDGIGetProbeIndexOffset(payload.instanceIndex, DDGIVolume.probeGridCounts, DDGIVolume.probeScrollOffsets);
#else
            int probeIndex = payload.instanceIndex;
#endif

            uint2 probeStateTexcoord = DDGIGetProbeTexelPosition(probeIndex, DDGIVolume.probeGridCounts);
            uint state = DDGIProbeStates.Load(probeStateTexcoord).r;

            // Border visualization for probe states
            if (abs(dot(ray.Direction, sampleDirection)) < 0.45f)
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

            // Convert to sRGB before storing
            result = LinearToSRGB(result);

            GBufferA[LaunchIndex] = float4(result, 0.f);
            GBufferB[LaunchIndex] = packedPayload.hitT;
        }
    }
}
