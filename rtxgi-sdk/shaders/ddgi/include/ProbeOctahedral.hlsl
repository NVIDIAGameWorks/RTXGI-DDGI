/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_PROBE_OCTAHEDRAL_HLSL
#define RTXGI_DDGI_PROBE_OCTAHEDRAL_HLSL

#include "Common.hlsl"

//------------------------------------------------------------------------
// Probe Octahedral Indexing
//------------------------------------------------------------------------

/**
 * Computes normalized octahedral coordinates for the given texel coordinates.
 * Maps the top left texel to (-1,-1).
 * Used by DDGIProbeBlendingCS() in ProbeBlending.hlsl.
 */
float2 DDGIGetNormalizedOctahedralCoordinates(int2 texCoords, int numTexels)
{
    // Map 2D texture coordinates to a normalized octahedral space
    float2 octahedralTexelCoord = float2(texCoords.x % numTexels, texCoords.y % numTexels);

    // Move to the center of a texel
    octahedralTexelCoord.xy += 0.5f;

    // Normalize
    octahedralTexelCoord.xy /= float(numTexels);

    // Shift to [-1, 1);
    octahedralTexelCoord *= 2.f;
    octahedralTexelCoord -= float2(1.f, 1.f);

    return octahedralTexelCoord;
}

/**
 * Computes the normalized octahedral direction that corresponds to the
 * given normalized coordinates on the [-1, 1] square.
 * The opposite of DDGIGetOctahedralCoordinates().
 * Used by DDGIProbeBlendingCS() in ProbeBlending.hlsl.
 */
float3 DDGIGetOctahedralDirection(float2 coords)
{
    float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));
    if (direction.z < 0.f)
    {
        direction.xy = (1.f - abs(direction.yx)) * RTXGISignNotZero(direction.xy);
    }
    return normalize(direction);
}

/**
 * Computes the octant coordinates in the normalized [-1, 1] square, for the given a unit direction vector.
 * The opposite of DDGIGetOctahedralDirection().
 * Used by GetDDGIVolumeIrradiance() in Irradiance.hlsl.
 */
float2 DDGIGetOctahedralCoordinates(float3 direction)
{
    float l1norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    float2 uv = direction.xy * (1.f / l1norm);
    if (direction.z < 0.f)
    {
        uv = (1.f - abs(uv.yx)) * RTXGISignNotZero(uv.xy);
    }
    return uv;
}

#endif // RTXGI_DDGI_PROBE_OCTAHEDRAL_HLSL
