/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../include/Common.hlsl"
#include "../../include/Descriptors.hlsl"
#include "../../include/RayTracing.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"
#include "../../../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"


// ---[ Helpers ]---

float3 GetProbeData(
    int probeIndex,
    int3 probeCoords,
    float3 worldPosition,
    DDGIVolumeResourceIndices resourceIndices,
    DDGIVolumeDescGPU volume,
    out float3 sampleDirection)
{
    float3 color = float3(0.f, 0.f, 0.f);

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's world-space position
    float3 probePosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the octahedral coordinates for the direction
    sampleDirection = normalize(worldPosition - probePosition);
    float2 octantCoords = DDGIGetOctahedralCoordinates(sampleDirection);

    // Get the probe data type to visualize
    uint type = GetGlobalConst(ddgivis, probeType);
    if (type == RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE)
    {
        // Get the volume's irradiance texture array
        Texture2DArray<float4> ProbeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);

        // Get the texture array uv coordinates for the octant of the probe
        float3 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumIrradianceInteriorTexels, volume);

        // Sample the irradiance texture
        color = ProbeIrradiance.SampleLevel(GetBilinearWrapSampler(), uv, 0).rgb;

        // Decode the tone curve
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        color = pow(color, exponent);

        // Go back to linear irradiance
        color *= color;

        // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to normalize for the display.
        color *= 2.f;

        // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
        if (volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32)
        {
            color *= 1.0989f;
        }
    }
    else if (type == RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE)
    {
        // Get the volume's distance texture array
        Texture2DArray<float4> ProbeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);

        // Get the texture array uv coordinates for the octant of the probe
        float3 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumDistanceInteriorTexels, volume);

        // Sample the distance texture and reconstruct the depth
        float distance = 2.f * ProbeDistance.SampleLevel(GetBilinearWrapSampler(), uv, 0).r;

        // Normalize the distance for visualization
        float value = saturate(distance / GetGlobalConst(ddgivis, distanceDivisor));
        color = float3(value, value, value);
    }

    return color;
}

void WriteResult(
    uint2 LaunchIndex,
    float3 color,
    float hitT,
    RWTexture2D<float4> GBufferAOutput,
    RWTexture2D<float4> GBufferBOutput)
{
    // Convert from linear to sRGB
    color = LinearToSRGB(color);

    // Overwrite GBufferA's albedo and mark the pixel to not be lit or post processed
    GBufferAOutput[LaunchIndex] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);

    // Overwrite GBufferB's hit distance with the distance to the probe
    GBufferBOutput[LaunchIndex].w = hitT;
}

// ---[ Ray Generation Shaders ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RaytracingAccelerationStructure DDGIProbeVisTLAS = GetAccelerationStructure(DDGIPROBEVIS_TLAS_INDEX);

    // Setup the primary ray
    RayDesc ray;
    ray.Origin = GetCamera().position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the primary ray direction
    float  halfHeight = GetCamera().tanHalfFovY;
    float  halfWidth = (GetCamera().aspect * halfHeight);
    float3 lowerLeftCorner = GetCamera().position - (halfWidth * GetCamera().right) - (halfHeight * GetCamera().up) + GetCamera().forward;
    float3 horizontal = (2.f * halfWidth) * GetCamera().right;
    float3 vertical = (2.f * halfHeight) * GetCamera().up;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    // Trace
    ProbeVisualizationPayload payload = (ProbeVisualizationPayload)0;
    TraceRay(
        DDGIProbeVisTLAS,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0x01,
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

            // Get the DDGIVolume structured buffers
            StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
            StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

            // Get the DDGIVolume's bindless resource indices
            DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

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
            float3 color = GetProbeData(probeIndex, probeCoords, payload.worldPosition, resourceIndices, volume, sampleDirection);

            // Color the probe if classification is enabled
            if (volume.probeClassificationEnabled)
            {
                const float3 INACTIVE_COLOR = float3(1.f, 0.f, 0.f);      // Red
                const float3 ACTIVE_COLOR = float3(0.f, 1.f, 0.f);        // Green

                // Get the probe data texture array
                Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

                // Get the probe's location in the probe data texture
                uint3 probeStateTexCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

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
            WriteResult(LaunchIndex, color, payload.hitT, GBufferA, GBufferB);
        }
    }
}

[shader("raygeneration")]
void RayGenHideInactive()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    float3 color = float3(0.f, 1.f, 0.f);

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RaytracingAccelerationStructure DDGIProbeVisTLAS = GetAccelerationStructure(DDGIPROBEVIS_TLAS_INDEX);

    // Setup the primary ray
    RayDesc ray;
    ray.Origin = GetCamera().position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the primary ray direction
    float  halfHeight = GetCamera().tanHalfFovY;
    float  halfWidth = (GetCamera().aspect * halfHeight);
    float3 lowerLeftCorner = GetCamera().position - (halfWidth * GetCamera().right) - (halfHeight * GetCamera().up) + GetCamera().forward;
    float3 horizontal = (2.f * halfWidth) * GetCamera().right;
    float3 vertical = (2.f * halfHeight) * GetCamera().up;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    ProbeVisualizationPayload payload = (ProbeVisualizationPayload)0;
    while(payload.hitT >= 0.f)
    {
        // Trace
        TraceRay(
            DDGIProbeVisTLAS,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0x02,
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

                // Get the DDGIVolume structured buffers
                StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
                StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

                // Get the DDGIVolume's bindless resource indices
                DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

                // Load the DDGIVolume constants
                DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

                // Adjust for all volume probe instances existing in a single TLAS
                int probeIndex = (payload.instanceIndex - payload.instanceOffset);

                // Get the probe's grid coordinates
                float3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

                // Adjust probe index for scroll offsets
                probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

                // Get the probe data texture array
                Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

                // Early out: if the probe is inactive
                uint3 probeStateTexCoords = DDGIGetProbeTexelCoords(probeIndex, volume);
                float probeState = ProbeData[probeStateTexCoords].w;
                if(probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE) continue;

                // Get the probe's data to display
                float3 sampleDirection;
                float3 color = GetProbeData(probeIndex, probeCoords, payload.worldPosition, resourceIndices, volume, sampleDirection);

                // Write the result to the GBuffer
                WriteResult(LaunchIndex, color, payload.hitT, GBufferA, GBufferB);
            }
        }
    }
}
