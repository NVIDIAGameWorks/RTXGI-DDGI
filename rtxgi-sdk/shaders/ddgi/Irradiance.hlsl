/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_IRRADIANCE_HLSL
#define RTXGI_DDGI_IRRADIANCE_HLSL

#include "ProbeCommon.hlsl"

struct DDGIVolumeResources
{
    Texture2D<float4> probeIrradianceSRV;
    Texture2D<float4> probeDistanceSRV;
    SamplerState trilinearSampler;
#if RTXGI_DDGI_PROBE_RELOCATION
    RWTexture2D<float4> probeOffsets;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    RWTexture2D<uint> probeStates;
#endif
};

/**
* Computes the surfaceBias parameter used by DDGIGetVolumeIrradiance().
* The surfaceNormal and cameraDirection arguments are expected to be normalized.
*/
float3 DDGIGetSurfaceBias(float3 surfaceNormal, float3 cameraDirection, DDGIVolumeDescGPU DDGIVolume)
{
    return (surfaceNormal * DDGIVolume.normalBias) + (-cameraDirection * DDGIVolume.viewBias);
}

/**
* Computes a blending weight for the given volume for blending between multiple volumes. 
* Return value of 1.0 means full contribution from this volume while 0.0 means no contribution.
*/ 
float DDGIGetVolumeBlendWeight(float3 worldPosition, DDGIVolumeDescGPU volume)
{
    // Start fully weighted
    float volumeBlendWeight = 1.f;
    
    // Shift from [-n/2, n/2] to [0, n]
    float3 position = (worldPosition - volume.origin) + (volume.probeGridSpacing * (volume.probeGridCounts - 1)) * 0.5f;
    float3 probeCoords = (position / volume.probeGridSpacing);

    // Map numbers over the max to the range 0 to 1 for blending
    float3 overProbeMax = (volume.probeGridCounts - 1.f) - probeCoords;

    // Use the geometric mean across all axes for weight
    volumeBlendWeight *= clamp(probeCoords.x, 0.f, 1.f);
    volumeBlendWeight *= clamp(probeCoords.y, 0.f, 1.f);
    volumeBlendWeight *= clamp(probeCoords.z, 0.f, 1.f);
    volumeBlendWeight *= clamp(overProbeMax.x, 0.f, 1.f);
    volumeBlendWeight *= clamp(overProbeMax.y, 0.f, 1.f);
    volumeBlendWeight *= clamp(overProbeMax.z, 0.f, 1.f);

    return volumeBlendWeight;
}

/**
* Samples irradiance from the given volume's probes using information about the surface, sampling direction, and volume.
*/
float3 DDGIGetVolumeIrradiance( 
    float3 worldPosition,
    float3 surfaceBias,
    float3 direction,
    DDGIVolumeDescGPU volume,
    DDGIVolumeResources resources)
{
    float3 irradiance = float3(0.f, 0.f, 0.f);
    float  accumulatedWeights = 0.f;

    // Bias the world space position
    float3 biasedWorldPosition = (worldPosition + surfaceBias);
    
    // Get the 3D grid coordinates of the base probe (near the biased world position)
    int3   baseProbeCoords = DDGIGetBaseProbeGridCoords(biasedWorldPosition, volume.origin, volume.probeGridCounts, volume.probeGridSpacing);

    // Get the world space position of the base probe
    float3 baseProbeWorldPosition = DDGIGetProbeWorldPosition(baseProbeCoords, volume.origin, volume.probeGridCounts, volume.probeGridSpacing);

    // Clamp the distance between the given point and the base probe's world position (on each axis) to [0, 1]
    float3 alpha = clamp(((biasedWorldPosition - baseProbeWorldPosition) / volume.probeGridSpacing), float3(0.f, 0.f, 0.f), float3(1.f, 1.f, 1.f));

    // Iterate over the 8 closest probes and accumulate their contributions
    for(int probeIndex = 0; probeIndex < 8; probeIndex++)
    {
        // Compute the offset to the adjacent probe in grid coordinates by
        // sourcing the offsets from the bits of the loop index: x = bit 0, y = bit 1, z = bit 2
        int3 adjacentProbeOffset = int3(probeIndex, probeIndex >> 1, probeIndex >> 2) & int3(1, 1, 1);

        // Get the 3D grid coordinates of the adjacent probe by adding the offset to the base probe
        // Clamp to the grid boundaries
        int3 adjacentProbeCoords = clamp(baseProbeCoords + adjacentProbeOffset, int3(0, 0, 0), volume.probeGridCounts - int3(1, 1, 1));

        // Get the adjacent probe's world position
#if RTXGI_DDGI_PROBE_RELOCATION
        float3 adjacentProbeWorldPosition = DDGIGetProbeWorldPositionWithOffset(adjacentProbeCoords, volume.origin, volume.probeGridCounts, volume.probeGridSpacing, resources.probeOffsets);
#else
        float3 adjacentProbeWorldPosition = DDGIGetProbeWorldPosition(adjacentProbeCoords, volume.origin, volume.probeGridCounts, volume.probeGridSpacing);
#endif

        // Get the adjacent probe's index (used for texture lookups)
        int adjacentProbeIndex = DDGIGetProbeIndex(adjacentProbeCoords, volume.probeGridCounts);

        // Compute the distance and direction from the (biased and non-biased) shading point and the adjacent probe
        float3 worldPosToAdjProbe = normalize(adjacentProbeWorldPosition - worldPosition);
        float3 biasedPosToAdjProbe = normalize(adjacentProbeWorldPosition - biasedWorldPosition);
        float  biasedPosToAdjProbeDist = length(adjacentProbeWorldPosition - biasedWorldPosition);

        // Compute trilinear weights based on the distance to each adjacent probe
        // to smoothly transition between probes. adjacentProbeOffset is binary, so we're
        // using a 1-alpha when adjacentProbeOffset = 0 and alpha when adjacentProbeOffset = 1.
        float3 trilinear = max(0.001f, lerp(1.f - alpha, alpha, adjacentProbeOffset));
        float  trilinearWeight = (trilinear.x * trilinear.y * trilinear.z);
        float  weight = 1.f;

        // A naive soft backface weight would ignore a probe when
        // it is behind the surface. That's good for walls, but for
        // small details inside of a room, the normals on the details
        // might rule out all of the probes that have mutual visibility 
        // to the point. We instead use a "wrap shading" test. The small
        // offset at the end reduces the "going to zero" impact.
        float wrapShading = (dot(worldPosToAdjProbe, direction) + 1.f) * 0.5f;
        weight *= (wrapShading * wrapShading) + 0.2f;

        // Compute the texture coordinates of this adjacent probe and sample the probe's filtered distance
        float2 octantCoords = DDGIGetOctahedralCoordinates(-biasedPosToAdjProbe);
        float2 probeTextureCoords = DDGIGetProbeUV(adjacentProbeIndex, octantCoords, volume.probeGridCounts, volume.probeNumDistanceTexels);
        float2 filteredDistance = resources.probeDistanceSRV.SampleLevel(resources.trilinearSampler, probeTextureCoords, 0).rg;

        float meanDistanceToSurface = filteredDistance.x;
        float variance = abs((filteredDistance.x * filteredDistance.x) - filteredDistance.y);

        float chebyshevWeight = 1.f;
        if(biasedPosToAdjProbeDist > meanDistanceToSurface) // In "shadow"
        {
            // v must be greater than 0, which is guaranteed by the if condition above.
            float v = biasedPosToAdjProbeDist - meanDistanceToSurface;
            chebyshevWeight = variance / (variance + (v * v));
        
            // Increase the contrast in the weight
            chebyshevWeight = max((chebyshevWeight * chebyshevWeight * chebyshevWeight), 0.f);
        }

        // Avoid visibility weights ever going all the way to zero because
        // when *no* probe has visibility we need a fallback value
        weight *= max(0.05f, chebyshevWeight);

        // Avoid a weight of zero
        weight = max(0.000001f, weight);

        // A small amount of light is visible due to logarithmic perception, so
        // crush tiny weights but keep the curve continuous
        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
        {
            weight *= (weight * weight) * (1.f / (crushThreshold * crushThreshold));
        }

        // Apply the trilinear weights
        weight *= trilinearWeight;

        // Sample the probe irradiance
        octantCoords = DDGIGetOctahedralCoordinates(direction);
        probeTextureCoords = DDGIGetProbeUV(adjacentProbeIndex, octantCoords, volume.probeGridCounts, volume.probeNumIrradianceTexels);
        float3 probeIrradiance = resources.probeIrradianceSRV.SampleLevel(resources.trilinearSampler, probeTextureCoords, 0).rgb;

        // Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending for the trilinear
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        probeIrradiance = pow(probeIrradiance, exponent);

        // Accumulate the weighted irradiance
        irradiance += (weight * probeIrradiance);
        accumulatedWeights += weight;
    }

    irradiance *= (1.f / accumulatedWeights);   // Normalize by the accumulated weights
    irradiance *= irradiance;                   // Go back to linear irradiance
    irradiance *= (0.5f * RTXGI_PI);            // Factored out of the probes

    return irradiance;
}

#endif /* RTXGI_DDGI_IRRADIANCE_HLSL */
