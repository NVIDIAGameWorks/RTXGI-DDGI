/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../include/rtxgi/ddgi/DDGIVolumeDefines.h"
#include "../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

cbuffer RootConstants : register(b0, space1)
{
    uint    ProbeNumIrradianceOrDistanceTexels;
    uint    ProbeUAVIndex;
    uint2   ProbeRootConstantPadding;
}

ConstantBuffer<DDGIVolumeDescGPU> DDGIVolume    : register(b1, space1);

// Probe irradiance or filtered distance
RWTexture2D<float4> DDGIProbeUAV[2]             : register(u1, space1);

[numthreads(8, 8, 1)]
void DDGIProbeBorderRowUpdateCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint probeSideLength = (ProbeNumIrradianceOrDistanceTexels + 2);
    uint probeSideLengthMinusOne = (probeSideLength - 1);

    // Map thread index to border row texel coordinates
    uint2 threadCoordinates = DispatchThreadID.xy;
    threadCoordinates.y *= probeSideLength;

    // Ignore the corner texels
    int mod = (DispatchThreadID.x % probeSideLength);
    if (mod == 0 || mod == probeSideLengthMinusOne)
    {
        return;
    }

    // Compute the interior texel coordinates to copy (top row)
    uint probeStart = uint(threadCoordinates.x / probeSideLength) * probeSideLength;
    uint offset = probeSideLengthMinusOne - (threadCoordinates.x % probeSideLength);

    uint2 copyCoordinates = uint2(probeStart + offset, (threadCoordinates.y + 1));

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING && RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
    threadCoordinates.y += probeSideLengthMinusOne;
    copyCoordinates = uint2(probeStart + offset, threadCoordinates.y - 1);
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
    return;
#endif

    // Top row
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];

    // Compute the interior texel coordinate to copy (bottom row)
    threadCoordinates.y += probeSideLengthMinusOne;
    copyCoordinates = uint2(probeStart + offset, threadCoordinates.y - 1);

    // Bottom row   
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];
}

[numthreads(8, 8, 1)]
void DDGIProbeBorderColumnUpdateCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint probeSideLength = (ProbeNumIrradianceOrDistanceTexels + 2);
    uint probeSideLengthMinusOne = (probeSideLength - 1);

    // Map thread index to border row texel coordinates
    uint2 threadCoordinates = DispatchThreadID.xy;
    threadCoordinates.x *= probeSideLength;

    uint2 copyCoordinates = uint2(0, 0);

    // Handle the corner texels
    int mod = (threadCoordinates.y % probeSideLength);
    if (mod == 0 || mod == probeSideLengthMinusOne)
    {
        // Left corner
        copyCoordinates.x = threadCoordinates.x + ProbeNumIrradianceOrDistanceTexels;
        copyCoordinates.y = threadCoordinates.y - sign(mod - 1) * ProbeNumIrradianceOrDistanceTexels;

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING && RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
        DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
#else
        DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];
#endif

        // Right corner
        threadCoordinates.x += probeSideLengthMinusOne;
        copyCoordinates.x = threadCoordinates.x - ProbeNumIrradianceOrDistanceTexels;

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING & RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
        DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
#else
        DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];
#endif
        return;
    }

    // Compute the interior texel coordinates to copy (left column)
    uint probeStart = uint(threadCoordinates.y / probeSideLength) * probeSideLength;
    uint offset = probeSideLengthMinusOne - (threadCoordinates.y % probeSideLength);

    copyCoordinates = uint2(threadCoordinates.x + 1, probeStart + offset);

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING && RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
    threadCoordinates.x += probeSideLengthMinusOne;
    copyCoordinates = uint2(threadCoordinates.x - 1, probeStart + offset);
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = float4(threadCoordinates, copyCoordinates);
    return;
#endif

    // Left column
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];

    // Compute the interior texel coordinate to copy (right column)
    threadCoordinates.x += probeSideLengthMinusOne;
    copyCoordinates = uint2(threadCoordinates.x - 1, probeStart + offset);

    // Right column
    DDGIProbeUAV[ProbeUAVIndex][threadCoordinates] = DDGIProbeUAV[ProbeUAVIndex][copyCoordinates];
}