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

ConstantBuffer<DDGIVolumeDescGPU> DDGIVolume    : register(b1, space1);

RWTexture2D<float4> DDGIProbeRTRadianceUAV      : register(u0, space1);
RWTexture2D<uint> DDGIProbeStates               : register(u4, space1);

[numthreads(8, 4, 1)]
void DDGIProbeStateClassifierCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    // Compute the probe index for this thread
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID.xy, DDGIVolume.probeGridCounts);

    // Early out if the thread maps past the number of probes
    int numTotalProbes = DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y * DDGIVolume.probeGridCounts.z;
    if (probeIndex >= numTotalProbes)
    {
        return;
    }

#if RTXGI_DDGI_PROBE_SCROLL
    int storageProbeIndex = DDGIGetProbeIndexOffset(probeIndex, DDGIVolume.probeGridCounts, DDGIVolume.probeScrollOffsets);
#else
    int storageProbeIndex = probeIndex;
#endif
    uint2 offsetTexelPosition = DDGIGetProbeTexelPosition(storageProbeIndex, DDGIVolume.probeGridCounts);

    // Compute the bounds used to check for surrounding geometry
    float3 geometryBounds = DDGIVolume.probeGridSpacing;

    // Conservatively increase the geometry search area
    geometryBounds *= 2.f;

#if RTXGI_DDGI_PROBE_RELOCATION
    // A conservative bound that assumes the maximum offset value for this probe
    geometryBounds *= 1.45f;
#endif

    float closestFrontfaceDistance = 1e27f;
    int   backfaceCount = 0;

    // Get the number of ray samples to inspect
    int numRays = min(DDGIVolume.numRaysPerProbe, RTXGI_DDGI_NUM_FIXED_RAYS);

    // Iterate over the rays cast for this probe
    for (int rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        int2 rayTexCoord = int2(rayIndex, probeIndex);

        // Load the hit distance from the ray cast
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        float hitDistance = DDGIProbeRTRadianceUAV[rayTexCoord].a;
#else
        float hitDistance = DDGIProbeRTRadianceUAV[rayTexCoord].g;
#endif

        // Don't include backface hit distances
        if (hitDistance < 0.f)
        {
            backfaceCount++;
            continue;
        }

        // Store the closest front face hit distance
        closestFrontfaceDistance = min(closestFrontfaceDistance, hitDistance);
    }

    // If this probe is near geometry, wake it up if the backface percentage is above probeBackfaceThreshold
    if (all(closestFrontfaceDistance <= geometryBounds) && (float(backfaceCount) / numRays < DDGIVolume.probeBackfaceThreshold))
    {
        DDGIProbeStates[offsetTexelPosition] = PROBE_STATE_ACTIVE;
        return;
    }

    DDGIProbeStates[offsetTexelPosition] = PROBE_STATE_INACTIVE;
}

[numthreads(8, 4, 1)]
void DDGIProbeStateActivateAllCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    DDGIProbeStates[DispatchThreadID.xy] = PROBE_STATE_ACTIVE;
}
