/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_PROBE_DATA_COMMON_HLSL
#define RTXGI_DDGI_PROBE_DATA_COMMON_HLSL

#include "Common.hlsl"

//------------------------------------------------------------------------
// Probe Data Texture Write Helpers
//------------------------------------------------------------------------

/**
 * Normalizes the world-space offset and writes it to the probe data texture.
 * Probe Relocation limits this range to [0.f, 0.45f).
 */
void DDGIStoreProbeDataOffset(RWTexture2DArray<float4> probeData, uint3 coords, float3 wsOffset, DDGIVolumeDescGPU volume)
{
    probeData[coords].xyz = wsOffset / volume.probeSpacing;
}

//------------------------------------------------------------------------
// Probe Data Texture Read Helpers
//------------------------------------------------------------------------

/**
 * Reads the probe's position offset (from a Texture2DArray) and converts it to a world-space offset.
 */
float3 DDGILoadProbeDataOffset(Texture2DArray<float4> probeData, uint3 coords, DDGIVolumeDescGPU volume)
{
    return probeData.Load(int4(coords, 0)).xyz * volume.probeSpacing;
}

/**
 * Reads the probe's position offset (from a RWTexture2DArray) and converts it to a world-space offset.
 */
float3 DDGILoadProbeDataOffset(RWTexture2DArray<float4> probeData, uint3 coords, DDGIVolumeDescGPU volume)
{
    return probeData[coords].xyz * volume.probeSpacing;
}

#endif // RTXGI_DDGI_PROBE_DATA_COMMON_HLSL
