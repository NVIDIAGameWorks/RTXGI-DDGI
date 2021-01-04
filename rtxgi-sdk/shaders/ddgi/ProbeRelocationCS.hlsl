/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
    // Compute the probe index for this thread
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID.xy, DDGIVolume.probeGridCounts);

    // Early out if the thread maps past the number of probes
    int numProbes = DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y * DDGIVolume.probeGridCounts.z;
    if (probeIndex >= numProbes)
    {
        return;
    }

#if RTXGI_DDGI_PROBE_SCROLL
    int storageProbeIndex = DDGIGetProbeIndexOffset(probeIndex, DDGIVolume.probeGridCounts, DDGIVolume.probeScrollOffsets);
#else
    int storageProbeIndex = probeIndex;
#endif
    uint2 offsetTexelPosition = DDGIGetProbeTexelPosition(storageProbeIndex, DDGIVolume.probeGridCounts);

    // Get the current world position offset
    float3 currentOffset = DDGIDecodeProbeOffset(offsetTexelPosition, DDGIVolume.probeGridSpacing, DDGIProbeOffsets);

    // Initialize
    int   closestBackfaceIndex = -1;
    int   closestFrontfaceIndex = -1;
    int   farthestFrontfaceIndex = -1;
    float closestBackfaceDistance = 1e27f;
    float closestFrontfaceDistance = 1e27f;
    float farthestFrontfaceDistance = 0.f;
    float backfaceCount = 0.f;

    // Get the number of ray samples to inspect
    int numRays = min(DDGIVolume.numRaysPerProbe, RTXGI_DDGI_NUM_FIXED_RAYS);

    // Iterate over the rays cast for this probe to find the number of backfaces and closest/farthest distances to the probe
    for (int rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        int2 rayTexCoord = int2(rayIndex, probeIndex);

        // Load the hit distance from the ray cast
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        float hitDistance = DDGIProbeRTRadianceUAV[rayTexCoord].a;
#else
        float hitDistance = DDGIProbeRTRadianceUAV[rayTexCoord].g;
#endif

        // Found a backface
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

        // Found a frontface
        if (hitDistance > 0.f)
        {
            if (hitDistance < closestFrontfaceDistance) 
            {
                closestFrontfaceDistance = hitDistance;
                closestFrontfaceIndex = rayIndex;
            }
            else if (hitDistance > farthestFrontfaceDistance) 
            {
                farthestFrontfaceDistance = hitDistance;
                farthestFrontfaceIndex = rayIndex;
            }
        }
    }

    float3 fullOffset = float3(1e27f, 1e27f, 1e27f);

    if (closestBackfaceIndex != -1 && (float(backfaceCount) / numRays) > DDGIVolume.probeBackfaceThreshold)
    {
        // If there is a close backface AND more than 25% of the hit geometry are backfacing, assume the probe is inside geometry
        float3 closestBackfaceDirection = closestBackfaceDistance * normalize(DDGIGetProbeRayDirection(closestBackfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform));
        fullOffset = currentOffset + closestBackfaceDirection * (ProbeDistanceScale + 1.f);
    }
    else if (closestFrontfaceDistance < DDGIVolume.probeMinFrontfaceDistance) 
    {
        // Don't move the probe if moving towards the farthest frontface will also bring us closer to the nearest frontface
        float3 closestFrontfaceDirection = normalize(DDGIGetProbeRayDirection(closestFrontfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform));
        float3 farthestFrontfaceDirection = normalize(DDGIGetProbeRayDirection(farthestFrontfaceIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform));

        if (dot(closestFrontfaceDirection, farthestFrontfaceDirection) <= 0.f)
        {
            // Ensures the probe never moves through the farthest frontface
            farthestFrontfaceDistance *= min(farthestFrontfaceDistance, 1.f);
            
            // ProbeDistanceScale decreases from 1.f - 0.f for backface movement
            // It can go to 0 to ensure some movement away from close surfaces
            fullOffset = currentOffset + farthestFrontfaceDirection * ProbeDistanceScale;
        }
    }
    else if (closestFrontfaceDistance > DDGIVolume.probeMinFrontfaceDistance + ProbeDistanceScale)
    {
        // Probe isn't near anything, try to move it back towards zero offset
        float moveBackMargin = min(closestFrontfaceDistance - DDGIVolume.probeMinFrontfaceDistance, length(currentOffset));
        float3 moveBackDirection = normalize(-currentOffset);
        fullOffset = currentOffset + (moveBackMargin * moveBackDirection);
    }

    // Absolute maximum distance probe could be moved is 0.5 * probeSpacing
    // Clamp to less than maximum distance to avoid degenerate cases
    if (all(abs(fullOffset) < 0.45f * DDGIVolume.probeGridSpacing))
    {
        currentOffset = fullOffset;
    }

    DDGIEncodeProbeOffset(offsetTexelPosition.xy, DDGIVolume.probeGridSpacing, currentOffset, DDGIProbeOffsets);
}
