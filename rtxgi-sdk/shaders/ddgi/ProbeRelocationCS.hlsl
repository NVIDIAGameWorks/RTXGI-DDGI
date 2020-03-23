/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "ProbeCommon.hlsl"

cbuffer RootConstants : register(b0, space1)
{
    float   ProbeDistanceScale;
    uint3   ProbeRootConstantPadding;
}

ConstantBuffer<DDGIVolumeDescGPU> DDGIVolume    : register(b1, space1);

RWTexture2D<float4> DDGIProbeRTRadianceUAV      : register(u0, space1);
RWTexture2D<float4> DDGIProbeOffsets            : register(u3, space1);

[numthreads(8, 4, 1)]
void DDGIProbeRelocationCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    // Get the current world position offset
    float3 currentOffset = DDGIDecodeProbeOffset(DispatchThreadID.xy, DDGIVolume.probeGridSpacing, DDGIProbeOffsets);
    
    // Compute the probe index for this thread
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID.xy, DDGIVolume.probeGridCounts);

    // Initialize
    int   closestBackfaceIndex = -1;
    int   farthestFrontfaceIndex = -1;
    float farthestFrontfaceDistance = 0.f;
    float closestBackfaceDistance = 1e27f;
    float closestFrontfaceDistance = 1e27f;
    float backfaceCount = 0.f;

    // Loop over the probe rays to find the number of backfaces and closest/farthest distances to them
    for (int rayIndex = 0; rayIndex < DDGIVolume.numRaysPerProbe; rayIndex++) 
    {
        int2 rayTexCoord = int2(rayIndex, probeIndex);

        float hitDistance = DDGIProbeRTRadianceUAV[rayTexCoord].a;
        if (hitDistance < 0.f) 
        {
            backfaceCount++;

            // Negate the hit distance on a backface hit and scale back to the full distance
            hitDistance = hitDistance * -5.f;
            if (hitDistance < closestBackfaceDistance) 
            {
                // Make up for the shortening of backfaces
                closestBackfaceDistance = hitDistance;
                closestBackfaceIndex = rayIndex;
            }
        }

        if (hitDistance > 0.f) 
        {
            if (hitDistance < closestFrontfaceDistance) 
            {
                closestFrontfaceDistance = hitDistance;
            }
            else if (hitDistance > farthestFrontfaceDistance) 
            {
                farthestFrontfaceDistance = hitDistance;
                farthestFrontfaceIndex = rayIndex;
            }
        }
    }

    float3 fullOffset = float3(1e27f, 1e27f, 1e27f);

    if (closestBackfaceIndex != -1 && (float(backfaceCount) / DDGIVolume.numRaysPerProbe) > DDGIVolume.probeBackfaceThreshold) 
    {
        // If there's a close backface AND more than 25% of the hit geometry is backfaces, assume the probe is inside geometry
        float3 closestBackfaceDirection = closestBackfaceDistance * normalize(DDGIGetProbeRayDirection(closestBackfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform));
        fullOffset = currentOffset + closestBackfaceDirection * (ProbeDistanceScale + 1.f);
    }
    else if (closestFrontfaceDistance < DDGIVolume.probeMinFrontfaceDistance) 
    {
        // Ensure that we never move through the farthest frontface
        float3 farthestDirection = min(farthestFrontfaceDistance, 1.f) * normalize(DDGIGetProbeRayDirection(farthestFrontfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform));

        // The farthest frontface may also be the closest if the probe can only
        // see one surface. If this is the case, don't move the probe.
        if (dot(farthestDirection, DDGIGetProbeRayDirection(farthestFrontfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform)) <= 0.f)
        {
            // ProbeDistanceScale decreases from 1.f - 0.f for backface movement
            // It can go to 0 to ensure some movement away from close surfaces
            fullOffset = currentOffset + farthestDirection * ProbeDistanceScale;
        }
    }

    // Absolute maximum distance probe could be moved is 0.5 * probeSpacing
    // Clamp to less than maximum distance to avoid degenerate cases
    if (all(abs(fullOffset) < 0.45f * DDGIVolume.probeGridSpacing)) 
    {
        currentOffset = fullOffset;
    }

    DDGIEncodeProbeOffset(DispatchThreadID.xy, DDGIVolume.probeGridSpacing, currentOffset, DDGIProbeOffsets);
}
