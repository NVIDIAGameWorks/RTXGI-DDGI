/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Descriptors.hlsl"
#include "../../include/Common.hlsl"
#include "../../include/RayTracing.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"


// ---[ Helpers ]---

float3 GetProbeData(uint volumeIndex, int probeIndex, int3 probeCoords, float3 worldPosition, DDGIVolumeDescGPU volume, out float3 sampleDirection)
{
    float3 color = float3(0.f, 0.f, 0.f);

    // Get the probe data texture
    Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);

    // Get the probe's world-space position
    float3 probePosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the octahedral coordinates for the direction
    sampleDirection = normalize(worldPosition - probePosition);
    float2 octantCoords = DDGIGetOctahedralCoordinates(sampleDirection);

    // Get the probe data type to visualize
    uint type = GetGlobalConst(ddgivis, probeType);
    if (type == RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE)
    {
        // Get the volume's irradiance texture
        Texture2D<float4> ProbeIrradiance = GetDDGIVolumeIrradianceSRV(volumeIndex);

        // Get the texture atlas uv coordinates for the octant of the probe
        float2 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumIrradianceTexels, volume);

        // Sample the irradiance texture
        color = ProbeIrradiance.SampleLevel(BilinearWrapSampler, uv, 0).rgb;

        // Decode the tone curve
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        color = pow(color, exponent);

        // Go back to linear irradiance
        color *= color;

        // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to normalize for the display.
        color *= 2.f;

        // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
        if (volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R10G10B10A2_FLOAT)
        {
            color *= 1.0989f;
        }
    }
    else if (type == RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE)
    {
        // Get the volume's distance texture
        Texture2D<float4> ProbeDistance = GetDDGIVolumeDistanceSRV(volumeIndex);

        // Get the texture atlas uv coordinates for the octant of the probe
        float2 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumDistanceTexels, volume);

        // Sample the distance texture and reconstruct the depth
        float distance = ProbeDistance.SampleLevel(BilinearWrapSampler, uv, 0).r;

        // Normalize the distance for visualization
        float value = saturate(distance / GetGlobalConst(ddgivis, distanceDivisor));
        color = float3(value, value, value);
    }

    return color;
}

void WriteResult(uint2 LaunchIndex, float3 color, float hitT)
{
    // Convert from linear to sRGB
    color = LinearToSRGB(color);

    // Overwrite GBufferA's albedo and mark the pixel to not be lit
    GBufferA[LaunchIndex] = float4(color, 0.f);

    // Overwrite GBufferB's hit distance with the distance to the probe
    GBufferB[LaunchIndex].w = hitT;
}

// ---[ Ray Generation Shaders ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // Setup the primary ray
    RayDesc ray;
    ray.Origin = Camera.position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the ray direction
    float  halfHeight = Camera.tanHalfFovY;
    float  halfWidth = (Camera.aspect * halfHeight);
    float3 lowerLeftCorner = Camera.position - (halfWidth * Camera.right) - (halfHeight * Camera.up) + Camera.forward;
    float3 horizontal = (2.f * halfWidth) * Camera.right;
    float3 vertical = (2.f * halfHeight) * Camera.up;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    // Trace
    ProbesPayload payload = (ProbesPayload)0;
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
        // If the GBuffer doesn't contain geometry or a visualization
        // probe is hit by a primary ray - and the probe is the
        // closest surface - overwrite GBufferA with probe information.
        float depth = GBufferB[LaunchIndex].w;
        if(depth < 0.f || payload.hitT < depth)
        {
            // Get the DDGIVolume index
            uint volumeIndex = payload.volumeIndex;

            // Load the DDGIVolume constants
            DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

            // Adjust for all volume probe instances existing in a single TLAS
            int probeIndex = (payload.instanceIndex - payload.instanceOffset);

            // Get the probe's grid coordinates
            float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

            // Adjust probe index for scroll offsets
            probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

            // Get the probe's data to display
            float3 sampleDirection;
            float3 color = GetProbeData(volumeIndex, probeIndex, probeCoords, payload.worldPosition, volume, sampleDirection);

            // Color the probe if classification is enabled
            if (volume.probeClassificationEnabled)
            {
                const float3 INACTIVE_COLOR = float3(1.f, 0.f, 0.f);      // Red
                const float3 ACTIVE_COLOR = float3(0.f, 1.f, 0.f);        // Green

                // Get the probe's location in the probe data texture
                Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);
                uint2 probeStateTexCoords = DDGIGetProbeDataTexelCoords(probeIndex, volume);

                // Get the probe's state
                float probeState = ProbeData[probeStateTexCoords].w;

                // Probe coloring
                if (abs(dot(ray.Direction, sampleDirection)) < 0.45f)
                {
                    if (probeState == RTXGI_DDGI_PROBE_STATE_ACTIVE)
                    {
                        color = ACTIVE_COLOR;
                    }
                    else if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE)
                    {
                        color = INACTIVE_COLOR;
                    }
                }
            }

            // Write the result to the GBuffer
            WriteResult(LaunchIndex, color, payload.hitT);
        }
    }
}

[shader("raygeneration")]
void RayGenHideInactive()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    float3 color = float3(0.f, 1.f, 0.f);

    // Setup the primary ray
    RayDesc ray;
    ray.Origin = Camera.position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the ray direction
    float  halfHeight = Camera.tanHalfFovY;
    float  halfWidth = (Camera.aspect * halfHeight);
    float3 lowerLeftCorner = Camera.position - (halfWidth * Camera.right) - (halfHeight * Camera.up) + Camera.forward;
    float3 horizontal = (2.f * halfWidth) * Camera.right;
    float3 vertical = (2.f * halfHeight) * Camera.up;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    ProbesPayload payload = (ProbesPayload)0;
    while(payload.hitT >= 0.f)
    {
        // Trace
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
            // Adjust the ray for the continuation
            ray.TMin = payload.hitT + 0.001f;

            // If the GBuffer doesn't contain geometry or a visualization
            // probe is hit by a primary ray - and the probe is the
            // closest surface - overwrite GBufferA with probe information.
            float depth = GBufferB[LaunchIndex].w;
            if (depth < 0.f || payload.hitT < depth)
            {
                // Get the DDGIVolume index
                uint volumeIndex = payload.volumeIndex;

                // Adjust for all volume probe instances existing in a single TLAS
                int probeIndex = (payload.instanceIndex - payload.instanceOffset);

                // Load the DDGIVolume constants
                DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

                // Get the probe's grid coordinates
                float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

                // Adjust probe index for scroll offsets
                probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

                // Get the probe's state
                Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);
                uint2 probeStateTexCoords = DDGIGetProbeDataTexelCoords(probeIndex, volume);
                float probeState = ProbeData[probeStateTexCoords].w;

                if(probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE) continue;

                // Get the probe's data to display
                float3 sampleDirection;
                float3 color = GetProbeData(volumeIndex, probeIndex, probeCoords, payload.worldPosition, volume, sampleDirection);

                // Write the result to the GBuffer
                WriteResult(LaunchIndex, color, payload.hitT);
            }
        }
    }
}
