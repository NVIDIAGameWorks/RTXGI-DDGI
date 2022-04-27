/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_PROBE_INDEXING_HLSL
#define RTXGI_DDGI_PROBE_INDEXING_HLSL

#include "Common.hlsl"

//------------------------------------------------------------------------
// Probe Indexing Helpers
//------------------------------------------------------------------------

/**
 * Get the number of probes on a horizontal plane, in the active coordinate system.
 */
int DDGIGetProbesPerPlane(int3 probeCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return (probeCounts.x * probeCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return (probeCounts.x * probeCounts.y);
#endif
}

/**
 * Get the index of the horizontal plane that the texel coordinates map to, in the active coordinate system.
 * Texel coordinates do *not* include the octahedral texture's 1-texel border.
 */
int DDGIGetPlaneIndex(uint2 texCoords, int3 probeCounts, int probeNumTexels)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return int(texCoords.x / (probeCounts.x * probeNumTexels));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    return int(texCoords.x / (probeCounts.y * probeNumTexels));
#endif
}

/**
 * Get the index of the horizontal plane, in the active coordinate system.
 */
int DDGIGetPlaneIndex(int3 probeCoords)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return probeCoords.z;
#else
    return probeCoords.y;
#endif
}

/**
 * Get the index of a probe within a horizontal plane that the texel coordinates map to, in the active coordinate system.
 * Texel coordinates do *not* include the octahedral texture's 1-texel border.
 */
int DDGIGetProbeIndexInPlane(uint2 texCoords, int planeIndex, int3 probeCounts, int probeNumTexels)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return int(texCoords.x / probeNumTexels) - (planeIndex * probeCounts.x) + (probeCounts.x * int(texCoords.y / probeNumTexels));
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    return int(texCoords.x / probeNumTexels) - (planeIndex * probeCounts.y) + (probeCounts.y * int(texCoords.y / probeNumTexels));
#endif
}

/**
 * Get the index of a probe within a horizontal plane that the probe coordinates map to, in the active coordinate system.
 */
int DDGIGetProbeIndexInPlane(int3 probeCoords, int3 probeCounts)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    return probeCoords.x + (probeCounts.x * probeCoords.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    return probeCoords.y + (probeCounts.y * probeCoords.x);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    return probeCoords.x + (probeCounts.x * probeCoords.y);
#endif
}

//------------------------------------------------------------------------
// Probe Indices
//------------------------------------------------------------------------

/**
 * Computes the probe index from 3D grid coordinates.
 * The opposite of DDGIGetProbeCoords(probeIndex,...).
 */
int DDGIGetProbeIndex(int3 probeCoords, DDGIVolumeDescGPU volume)
{
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = DDGIGetPlaneIndex(probeCoords);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(probeCoords, volume.probeCounts);

    return (planeIndex * probesPerPlane) + probeIndexInPlane;
}

/**
 * Computes the probe index from 2D texture coordinates.
 */
int DDGIGetProbeIndex(uint2 texCoords, int probeNumTexels, DDGIVolumeDescGPU volume)
{
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = DDGIGetPlaneIndex(texCoords, volume.probeCounts, probeNumTexels);
    int probeIndexInPlane = DDGIGetProbeIndexInPlane(texCoords, planeIndex, volume.probeCounts, probeNumTexels);

    return (planeIndex * probesPerPlane) + probeIndexInPlane;
}

//------------------------------------------------------------------------
// Probe Grid Coordinates
//------------------------------------------------------------------------

/**
 * Computes the 3D grid-space coordinates for the probe at the given probe index in the range [0, numProbes-1].
 * The opposite of DDGIGetProbeIndex(probeCoords,...).
 */
int3 DDGIGetProbeCoords(int probeIndex, DDGIVolumeDescGPU volume)
{
    int3 probeCoords;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    probeCoords.x = probeIndex % volume.probeCounts.x;
    probeCoords.y = probeIndex / (volume.probeCounts.x * volume.probeCounts.z);
    probeCoords.z = (probeIndex / volume.probeCounts.x) % volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    probeCoords.x = (probeIndex / volume.probeCounts.y) % volume.probeCounts.x;
    probeCoords.y = probeIndex % volume.probeCounts.y;
    probeCoords.z = probeIndex / (volume.probeCounts.x * volume.probeCounts.y);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    probeCoords.x = probeIndex % volume.probeCounts.x;
    probeCoords.y = (probeIndex / volume.probeCounts.x) % volume.probeCounts.y;
    probeCoords.z = probeIndex / (volume.probeCounts.y * volume.probeCounts.x);
#endif

    return probeCoords;
}

/**
 * Computes the 3D grid-space coordinates of the "base" probe (i.e. floor of xyz) of the 8-probe
 * cube that surrounds the given world space position. The other seven probes of the cube
 * are offset by 0 or 1 in grid space along each axis.
 *
 * This function accounts for scroll offsets to adjust the volume's origin.
 */
int3 DDGIGetBaseProbeGridCoords(float3 worldPosition, DDGIVolumeDescGPU volume)
{
    // Get the vector from the volume origin to the surface point
    float3 position = worldPosition - (volume.origin + (volume.probeScrollOffsets * volume.probeSpacing));

    // Rotate the world position into the volume's space
    if(!IsVolumeMovementScrolling(volume)) position = RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation));

    // Shift from [-n/2, n/2] to [0, n] (grid space)
    position += (volume.probeSpacing * (volume.probeCounts - 1)) * 0.5f;

    // Quantize the position to grid space
    int3 probeCoords = int3(position / volume.probeSpacing);

    // Clamp to [0, probeCounts - 1]
    // Snaps positions outside of grid to the grid edge
    probeCoords = clamp(probeCoords, int3(0, 0, 0), (volume.probeCounts - int3(1, 1, 1)));

    return probeCoords;
}

//------------------------------------------------------------------------
// Texture Coordinates
//------------------------------------------------------------------------

/**
 * Computes the 2D texture coordinates of the probe at the given probe index.
 *
 * When infinite scrolling is enbled, probeIndex is expected to be the scroll adjusted probe index.
 * Obtain the adjusted index with DDGIGetScrollingProbeIndex().
 */
uint2 DDGIGetProbeTexelCoords(int probeIndex, DDGIVolumeDescGPU volume)
{
    // Find the probe's plane index
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);
    int planeIndex = int(probeIndex / probesPerPlane);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    int gridSpaceX = (probeIndex % volume.probeCounts.x);
    int gridSpaceY = (probeIndex / volume.probeCounts.x);

    int x = gridSpaceX + (planeIndex * volume.probeCounts.x);
    int y = gridSpaceY % volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    int gridSpaceX = (probeIndex % volume.probeCounts.y);
    int gridSpaceY = (probeIndex / volume.probeCounts.y);

    int x = gridSpaceX + (planeIndex * volume.probeCounts.y);
    int y = gridSpaceY % volume.probeCounts.x;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    int gridSpaceX = (probeIndex % volume.probeCounts.x);
    int gridSpaceY = (probeIndex / volume.probeCounts.x);

    int x = gridSpaceX + (planeIndex * volume.probeCounts.x);
    int y = gridSpaceY % volume.probeCounts.y;
#endif

    return uint2(x, y);
}

/**
 * Computes the normalized texture UVs for the Probe Irradiance and Probe Distance textures
 * (used in blending) given the probe index and 2D normalized octant coordinates [-1, 1].
 * 
 * When infinite scrolling is enbled, probeIndex is expected to be the scroll adjusted probe index.
 * Obtain the adjusted index with DDGIGetScrollingProbeIndex().
 */
float2 DDGIGetProbeUV(int probeIndex, float2 octantCoordinates, int numTexels, DDGIVolumeDescGPU volume)
{
    // Get the probe's texel coordinates, assuming one texel per probe
    uint2 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Adjust for the number of interior and border texels
    float probeInteriorTexels = float(numTexels);
    float probeTexels = (probeInteriorTexels + 2.f);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float textureWidth = probeTexels * (volume.probeCounts.x * volume.probeCounts.y);
    float textureHeight = probeTexels * volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    float textureWidth = probeTexels * (volume.probeCounts.y * volume.probeCounts.z);
    float textureHeight = probeTexels * volume.probeCounts.x;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    float textureWidth = probeTexels * (volume.probeCounts.x * volume.probeCounts.z);
    float textureHeight = probeTexels * volume.probeCounts.y;
#endif

    float2 uv = float2(coords.x * probeTexels, coords.y * probeTexels) + (probeTexels * 0.5f);
    uv += octantCoordinates.xy * (probeInteriorTexels * 0.5f);
    uv /= float2(textureWidth, textureHeight);
    return uv;
}

//------------------------------------------------------------------------
// Probe Classification
//------------------------------------------------------------------------

/**
 * Loads and returns the probe's classification state (from a RWTexture2D).
 */
float DDGILoadProbeState(int probeIndex, RWTexture2D<float4> probeData, DDGIVolumeDescGPU volume)
{
    float state = RTXGI_DDGI_PROBE_STATE_ACTIVE;
    if (volume.probeClassificationEnabled)
    {
        // Get the probe's texel coordinates in the Probe Data texture
        int2 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Get the probe's classification state
        state = probeData[probeDataCoords].w;
    }

    return state;
}

/**
 * Loads and returns the probe's classification state (from a Texture2D).
 */
float DDGILoadProbeState(int probeIndex, Texture2D<float4> probeData, DDGIVolumeDescGPU volume)
{
    float state = RTXGI_DDGI_PROBE_STATE_ACTIVE;
    if (volume.probeClassificationEnabled)
    {
        // Get the probe's texel coordinates in the Probe Data texture
        int2 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

        // Get the probe's classification state
        state = probeData.Load(int3(probeDataCoords, 0)).w;
    }

    return state;
}

//------------------------------------------------------------------------
// Infinite Scrolling
//------------------------------------------------------------------------

/**
 * Adjusts the probe index for when infinite scrolling is enabled.
 * This can run when scrolling is disabled since zero offsets result
 * in the same probe index.
 */
int DDGIGetScrollingProbeIndex(int3 probeCoords, DDGIVolumeDescGPU volume)
{
    return DDGIGetProbeIndex(((probeCoords + volume.probeScrollOffsets + volume.probeCounts) % volume.probeCounts), volume);
}

/**
 * Clears probe irradiance and distance data for a plane of probes that have been scrolled to new positions.
 */
bool DDGIClearScrolledPlane(RWTexture2D<float4> output, uint2 outputCoords, int3 probeCoords, int planeIndex, DDGIVolumeDescGPU volume)
{
    if (volume.probeScrollClear[planeIndex])
    {
        int offset = volume.probeScrollOffsets[planeIndex];
        int probeCount = volume.probeCounts[planeIndex];
        int direction = volume.probeScrollDirections[planeIndex];

        int coord = 0;
        if(direction) coord = (probeCount + (offset - 1)) % probeCount; // scrolling in positive direction
        else coord = (probeCount + (offset % probeCount)) % probeCount; // scrolling in negative direction

        if (probeCoords[planeIndex] == coord)
        {
            output[outputCoords] = float4(0.f, 0.f, 0.f, 0.f);
            return true;
        }
    }
    return false;
}

#endif // RTXGI_DDGI_PROBE_INDEXING_HLSL
