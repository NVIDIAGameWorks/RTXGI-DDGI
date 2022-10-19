/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_IRRADIANCE_HLSL
#define RTXGI_DDGI_IRRADIANCE_HLSL

#include "include/ProbeCommon.hlsl"

struct DDGIVolumeResources
{
    Texture2DArray<float4> probeIrradiance;
    Texture2DArray<float4> probeDistance;
    Texture2DArray<float4> probeData;
    SamplerState bilinearSampler;
};

/**
 * Computes the surfaceBias parameter used by DDGIGetVolumeIrradiance().
 * The surfaceNormal and cameraDirection arguments are expected to be normalized.
 */
float3 DDGIGetSurfaceBias(float3 surfaceNormal, float3 cameraDirection, DDGIVolumeDescGPU volume)
{
    return (surfaceNormal * volume.probeNormalBias) + (-cameraDirection * volume.probeViewBias);
}

/**
 * Computes a weight value in the range [0, 1] for a world position and volume pair.
 * All positions inside the given volume recieve a weight of 1.
 * Positions outside the volume receive a weight in [0, 1] that
 * decreases as the position moves away from the volume.
 */ 
float DDGIGetVolumeBlendWeight(float3 worldPosition, DDGIVolumeDescGPU volume)
{
    // Get the volume's origin and extent
    float3 origin = volume.origin + (volume.probeScrollOffsets * volume.probeSpacing);
    float3 extent = (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;

    // Get the delta between the (rotated volume) and the world-space position
    float3 position = (worldPosition - origin);
    position = abs(RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation)));

    float3 delta = position - extent;
    if(all(delta < 0)) return 1.f;

    // Adjust the blend weight for each axis
    float volumeBlendWeight = 1.f;
    volumeBlendWeight *= (1.f - saturate(delta.x / volume.probeSpacing.x));
    volumeBlendWeight *= (1.f - saturate(delta.y / volume.probeSpacing.y));
    volumeBlendWeight *= (1.f - saturate(delta.z / volume.probeSpacing.z));

    return volumeBlendWeight;
}

/**
 * Computes irradiance for the given world-position using the given volume, surface bias, 
 * sampling direction, and volume resources.
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

    // Get the 3D grid coordinates of the probe nearest the biased world position (i.e. the "base" probe)
    int3   baseProbeCoords = DDGIGetBaseProbeGridCoords(biasedWorldPosition, volume);

    // Get the world-space position of the base probe (ignore relocation)
    float3 baseProbeWorldPosition = DDGIGetProbeWorldPosition(baseProbeCoords, volume);

    // Clamp the distance (in grid space) between the given point and the base probe's world position (on each axis) to [0, 1]
    float3 gridSpaceDistance = (biasedWorldPosition - baseProbeWorldPosition);
    if(!IsVolumeMovementScrolling(volume)) gridSpaceDistance = RTXGIQuaternionRotate(gridSpaceDistance, RTXGIQuaternionConjugate(volume.rotation));
    float3 alpha = clamp((gridSpaceDistance / volume.probeSpacing), float3(0.f, 0.f, 0.f), float3(1.f, 1.f, 1.f));

    // Iterate over the 8 closest probes and accumulate their contributions
    for(int probeIndex = 0; probeIndex < 8; probeIndex++)
    {
        // Compute the offset to the adjacent probe in grid coordinates by
        // sourcing the offsets from the bits of the loop index: x = bit 0, y = bit 1, z = bit 2
        int3 adjacentProbeOffset = int3(probeIndex, probeIndex >> 1, probeIndex >> 2) & int3(1, 1, 1);

        // Get the 3D grid coordinates of the adjacent probe by adding the offset to 
        // the base probe and clamping to the grid boundaries
        int3 adjacentProbeCoords = clamp(baseProbeCoords + adjacentProbeOffset, int3(0, 0, 0), volume.probeCounts - int3(1, 1, 1));

        // Get the adjacent probe's index, adjusting the adjacent probe index for scrolling offsets (if present)
        int adjacentProbeIndex = DDGIGetScrollingProbeIndex(adjacentProbeCoords, volume);

        // Early Out: don't allow inactive probes to contribute to irradiance
        int probeState = DDGILoadProbeState(adjacentProbeIndex, resources.probeData, volume);
        if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE) continue;

        // Get the adjacent probe's world position
        float3 adjacentProbeWorldPosition = DDGIGetProbeWorldPosition(adjacentProbeCoords, volume, resources.probeData);

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

        // Compute the octahedral coordinates of the adjacent probe
        float2 octantCoords = DDGIGetOctahedralCoordinates(-biasedPosToAdjProbe);

        // Get the texture array coordinates for the octant of the probe
        float3 probeTextureUV = DDGIGetProbeUV(adjacentProbeIndex, octantCoords, volume.probeNumDistanceInteriorTexels, volume);

        // Sample the probe's distance texture to get the mean distance to nearby surfaces
        float2 filteredDistance = 2.f * resources.probeDistance.SampleLevel(resources.bilinearSampler, probeTextureUV, 0).rg;

        // Find the variance of the mean distance
        float variance = abs((filteredDistance.x * filteredDistance.x) - filteredDistance.y);

        // Occlusion test
        float chebyshevWeight = 1.f;
        if(biasedPosToAdjProbeDist > filteredDistance.x) // occluded
        {
            // v must be greater than 0, which is guaranteed by the if condition above.
            float v = biasedPosToAdjProbeDist - filteredDistance.x;
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

        // Get the octahedral coordinates for the sample direction
        octantCoords = DDGIGetOctahedralCoordinates(direction);

        // Get the probe's texture coordinates
        probeTextureUV = DDGIGetProbeUV(adjacentProbeIndex, octantCoords, volume.probeNumIrradianceInteriorTexels, volume);

        // Sample the probe's irradiance
        float3 probeIrradiance = resources.probeIrradiance.SampleLevel(resources.bilinearSampler, probeTextureUV, 0).rgb;

        // Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        probeIrradiance = pow(probeIrradiance, exponent);

        // Accumulate the weighted irradiance
        irradiance += (weight * probeIrradiance);
        accumulatedWeights += weight;
    }

    if(accumulatedWeights == 0.f) return float3(0.f, 0.f, 0.f);

    irradiance *= (1.f / accumulatedWeights);   // Normalize by the accumulated weights
    irradiance *= irradiance;                   // Go back to linear irradiance
    irradiance *= RTXGI_2PI;                    // Multiply by the area of the integration domain (hemisphere) to complete the Monte Carlo Estimator equation

    // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
    if (volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32)
    {
        irradiance *= 1.0989f;
    }

    return irradiance;
}

#endif // RTXGI_DDGI_IRRADIANCE_HLSL
