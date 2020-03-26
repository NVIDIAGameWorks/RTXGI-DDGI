/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_PROBE_COMMON_HLSL
#define RTXGI_DDGI_PROBE_COMMON_HLSL

#include "../Common.hlsl"
#include "../../include/rtxgi/Defines.h"
#include "../../include/rtxgi/ddgi/DDGIVolumeDefines.h"
#include "../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

//------------------------------------------------------------------------
// Probe Indexing Helpers
//------------------------------------------------------------------------

/**
* Gets the number of probes on a horizontal plane in the active coordinate system.
*/
int DDGIGetProbesPerPlane(int3 probeGridCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return (probeGridCounts.x * probeGridCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return (probeGridCounts.x * probeGridCounts.y);
#endif
}

/**
* Get the index of the horizontal plane that the thread coordinates map to in the active coordinate system.
*/
int DDGIGetPlaneIndex(uint2 threadCoords, int3 probeGridCounts, int probeNumTexels)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return int(threadCoords.x / (probeGridCounts.x * probeNumTexels));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return int(threadCoords.x / (probeGridCounts.y * probeNumTexels));
#endif
}

/**
* Gets the index of a probe within a given horizontal plane in the active coordinate system.
*/
int DDGIGetProbeIndexInPlane(uint2 threadCoords, int planeIndex, int3 probeGridCounts, int probeNumTexels)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return int(threadCoords.x / probeNumTexels) - (planeIndex * probeGridCounts.x) + (probeGridCounts.x * int(threadCoords.y / probeNumTexels));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return int(threadCoords.x / probeNumTexels) - (planeIndex * probeGridCounts.y) + (probeGridCounts.y * int(threadCoords.y / probeNumTexels));
#endif
}

//------------------------------------------------------------------------
// Octahedral Paramerization
//------------------------------------------------------------------------

/**
* Compute the offset and state texel position for the given probeIndex.
*/
int2 DDGIGetProbeTexelPosition(int probeIndex, int3 probeGridCounts)
{
    // Compute the probe index for this thread
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return int2(probeIndex % (probeGridCounts.x * probeGridCounts.y), probeIndex / (probeGridCounts.x * probeGridCounts.y));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return int2(probeIndex % (probeGridCounts.y * probeGridCounts.z), probeIndex / (probeGridCounts.y * probeGridCounts.z));
#endif
}

/**
* Computes the normalized texture coordinates of the given probe, 
* using the probe index, octant coordinates, probe grid counts, 
* and the number of texels used by a probe.
*/
float2 DDGIGetProbeUV(uint probeIndex, float2 octantCoordinates, int3 probeGridCounts, int numTexels)
{
    int probesPerPlane = DDGIGetProbesPerPlane(probeGridCounts);
    int planeIndex = int(probeIndex / probesPerPlane);

    float probeInteriorTexels = float(numTexels);
    float probeTexels = (probeInteriorTexels + 2.f);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float x = float(probeIndex % probeGridCounts.x) + (planeIndex * probeGridCounts.x);
    float y = float((probeIndex / probeGridCounts.x) % probeGridCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    float x = float(probeIndex % probeGridCounts.y) + (planeIndex * probeGridCounts.y);
    float y = float((probeIndex / probeGridCounts.y) % probeGridCounts.x);
#endif

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float textureWidth = probeTexels * (probeGridCounts.x * probeGridCounts.y);
    float textureHeight = probeTexels * probeGridCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    float textureWidth = probeTexels * (probeGridCounts.y * probeGridCounts.z);
    float textureHeight = probeTexels * probeGridCounts.x;
#endif

    float2 uv = float2(x * probeTexels, y * probeTexels) + (probeTexels * 0.5f);
    uv += octantCoordinates.xy * (probeInteriorTexels * 0.5f);
    uv /= float2(textureWidth, textureHeight);
    return uv;
}

/**
* Computes normalized octahedral coordinates for the given thread/pixel coordinates.
* Maps the top left texel to (-1,-1).
*/
float2 DDGIGetNormalizedOctahedralCoordinates(int2 threadCoords, int numTexels)
{
    // Map thread coordinates to a normalized octahedral space
    float2 octahedralTexelCoord = float2(threadCoords.x % numTexels, threadCoords.y % numTexels);

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

//------------------------------------------------------------------------
// Probe Ray Direction
//------------------------------------------------------------------------

/*
* Computes a low discrepancy spherically distributed direction on the unit sphere,
* for the given index in a set of samples. Each direction is unique in 
* the set, but the set of directions is always the same.
*/
float3 DDGISphericalFibonacci(float index, float numSamples)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = 2.f * RTXGI_PI * frac(index * b);
    float cosTheta = 1.f - (2.f * index + 1.f) * (1.f / numSamples);
    float sinTheta = sqrt(saturate(1.f - (cosTheta * cosTheta)));

    return float3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

/**
* Computes a ray direction for the given ray index.
* Generate a spherically distributed normalized ray direction, then apply the given rotation transformation.
*/
float3 DDGIGetProbeRayDirection(int rayIndex, int numRaysPerProbe, matrix<float, 4, 4> rotationTransform)
{
    float3 direction = DDGISphericalFibonacci(rayIndex, numRaysPerProbe);
    return normalize(mul(float4(direction, 0.f), rotationTransform).xyz);
}

//------------------------------------------------------------------------
// Probe Indexing
//------------------------------------------------------------------------

/**
* Computes the probe index from 2D texture coordinates and probe counts.
*/
int DDGIGetProbeIndex(int2 texcoord, int3 probeGridCounts)
{
    // Compute the probe index for this thread
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return texcoord.x + (texcoord.y * (probeGridCounts.x * probeGridCounts.y));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return texcoord.x + (texcoord.y * (probeGridCounts.y * probeGridCounts.z));
#endif
}

/**
* Computes the probe index from 3D grid coordinates and probe counts.
* The opposite of DDGIGetProbeCoords().
*/
int DDGIGetProbeIndex(int3 probeCoords, int3 probeGridCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return probeCoords.x + (probeGridCounts.x * probeCoords.z) + (probeGridCounts.x * probeGridCounts.z) * probeCoords.y;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    return probeCoords.y + (probeGridCounts.y * probeCoords.x) + (probeGridCounts.y * probeGridCounts.x) * probeCoords.z;
#endif
}

/**
* Computes the probe index from the given thread coordinates of the irradiance and distance textures
* the number of probes in the volume, and the number of texels per probe.
*/
int DDGIGetProbeIndex(uint2 threadCoords, int3 probeGridCounts, int probeNumTexels)
{
    int probesPerPlane = DDGIGetProbesPerPlane(probeGridCounts);
    int planeIndex = DDGIGetPlaneIndex(threadCoords, probeGridCounts, probeNumTexels);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(threadCoords, planeIndex, probeGridCounts, probeNumTexels);

    return (planeIndex * probesPerPlane) + probeIndexInPlane;
}

/**
* Computes the 3D grid coordinates for the probe at the given probe index.
* The opposite of DDGIGetProbeIndex().
*/
int3 DDGIGetProbeCoords(int probeIndex, int3 probeGridCounts)
{
    int3 probeCoords;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    probeCoords.x = probeIndex % probeGridCounts.x;
    probeCoords.y = probeIndex / (probeGridCounts.x * probeGridCounts.z);
    probeCoords.z = (probeIndex / probeGridCounts.x) % probeGridCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    probeCoords.x = (probeIndex / probeGridCounts.y) % probeGridCounts.x;
    probeCoords.y = probeIndex % probeGridCounts.y;
    probeCoords.z = probeIndex / (probeGridCounts.x * probeGridCounts.y);
#endif

    return probeCoords;
}

/**
* Computes the 3D grid coordinates of the base probe (i.e. floor of xyz) of the 8-probe 
* cube that surrounds the given world space position. The other seven probes are offset 
* by 0 or 1 in grid space along each axis.
*/
int3 DDGIGetBaseProbeGridCoords(float3 worldPosition, float3 origin, int3 probeGridCounts, float3 probeGridSpacing)
{
    // Shift from [-n/2, n/2] to [0, n]
    float3 position = (worldPosition - origin) + (probeGridSpacing * (probeGridCounts - 1)) * 0.5f;

    int3 probeCoords = int3(position / probeGridSpacing);

    // Clamp to [0, probeGridCounts - 1]
    // Snaps positions outside of grid to the grid edge
    probeCoords = clamp(probeCoords, int3(0, 0, 0), (probeGridCounts - int3(1, 1, 1)));

    return probeCoords;
}

/**
* Computes the world space position of a probe at the given 3D grid coordinates.
*/
float3 DDGIGetProbeWorldPosition(int3 probeCoords, float3 origin, int3 probeGridCounts, float3 probeGridSpacing)
{
    // Multiply the grid coordinates by the grid spacing
    float3 probeGridWorldPosition = (probeCoords * probeGridSpacing);

    // Shift the grid by half of each axis extent to center the volume about its origin
    float3 probeGridShift = (probeGridSpacing * (probeGridCounts - 1)) * 0.5f;

    // Compute the probe's world position
    return (origin + probeGridWorldPosition - probeGridShift);
}

/*
* Computes the world space position of the probe at the given probe index (without the probe offsets).
*/
float3 DDGIGetProbeWorldPosition(int probeIndex, float3 origin, int3 probeGridCounts, float3 probeGridSpacing)
{
    float3 probeCoords = DDGIGetProbeCoords(probeIndex, probeGridCounts);
    return DDGIGetProbeWorldPosition(probeCoords, origin, probeGridCounts, probeGridSpacing);
}

//------------------------------------------------------------------------
// Probe Relocation
//------------------------------------------------------------------------

#if RTXGI_DDGI_PROBE_RELOCATION

/*
* Reads the probe offset and transforms it into a world space offset.
*/
float3 DDGIDecodeProbeOffset(int2 probeOffsetTexcoord, float3 probeGridSpacing, RWTexture2D<float4> probeOffsets)
{
    return probeOffsets[probeOffsetTexcoord].xyz * probeGridSpacing;
}

/*
* Normalizes the world space offset and writes it to the probe offset texture.
* Probe Position Preprocess effectively limits this range to [0.f, 0.45f).
*/
void DDGIEncodeProbeOffset(int2 probeOffsetTexcoord, float3 probeGridSpacing, float3 wsOffset, RWTexture2D<float4> probeOffsets)
{
    probeOffsets[probeOffsetTexcoord].xyz = wsOffset / probeGridSpacing;
}

/*
* Computes the world space position of a probe at the given probe index, including the probe's offset value.
*/
float3 DDGIGetProbeWorldPositionWithOffset(int probeIndex, float3 origin, int3 probeGridCounts, float3 probeGridSpacing, RWTexture2D<float4> probeOffsets)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    int textureWidth = (probeGridCounts.x * probeGridCounts.y);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    int textureWidth = (probeGridCounts.y * probeGridCounts.z);
#endif

    // Find the texture coords of the probe in the offsets texture
    int2 offsetTexcoords = int2(probeIndex % textureWidth, probeIndex / textureWidth);
    return DDGIDecodeProbeOffset(offsetTexcoords, probeGridSpacing, probeOffsets) + DDGIGetProbeWorldPosition(probeIndex, origin, probeGridCounts, probeGridSpacing);
}

/**
* Compute the world space position from the 3D grid coordinates, including the probe's offset value.
*/
float3 DDGIGetProbeWorldPositionWithOffset(int3 probeCoords, float3 origin, int3 probeGridCounts, float3 probeGridSpacing, RWTexture2D<float4> probeOffsets)
{
    int probeIndex = DDGIGetProbeIndex(probeCoords, probeGridCounts);
    return DDGIGetProbeWorldPositionWithOffset(probeIndex, origin, probeGridCounts, probeGridSpacing, probeOffsets);
}

#endif /* RTXGI_DDGI_PROBE_RELOCATION */

//------------------------------------------------------------------------
// Probe State Classification
//------------------------------------------------------------------------

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER

#define PROBE_STATE_ACTIVE      0   // probe shoots rays and may be sampled by a front facing surface or another probe (recursive irradiance)
#define PROBE_STATE_INACTIVE    1   // probe doesn't need to shoot rays, it isn't near a front facing surface or another active probe

#endif /* RTXGI_DDGI_PROBE_STATE_CLASSIFIER */

#endif /* RTXGI_DDGI_PROBE_COMMON_HLSL */
