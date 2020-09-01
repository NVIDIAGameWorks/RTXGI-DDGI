/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_VOLUME_DESC_GPU_H
#define RTXGI_DDGI_VOLUME_DESC_GPU_H

/**
 * DDGIVolumeDescGPU
 * The condensed DDGIVolume descriptor for use on the GPU.
 */
struct DDGIVolumeDescGPU
{
    float3      origin;
    int         numRaysPerProbe;
    float3      probeGridSpacing;
    float       probeMaxRayDistance;
    int3        probeGridCounts;
    float       probeDistanceExponent;
    float       probeHysteresis;
    float       probeChangeThreshold;
    float       probeBrightnessThreshold;
    float       probeIrradianceEncodingGamma;
    float       probeInverseIrradianceEncodingGamma;
    int         probeNumIrradianceTexels;
    int         probeNumDistanceTexels;
    float       normalBias;
    float       viewBias;
    float3      probeVariablePad0;
    float4x4    probeRayRotationTransform;      // 160B

#if RTXGI_DDGI_PROBE_SCROLL
    int         volumeMovementType;             // 0: default, 1: scrolling
    int3        probeScrollOffsets;             // 176B
#else
    float4      probeVariablePad1;              // 176B
#endif

#if !RTXGI_DDGI_PROBE_RELOCATION && !RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    float4      padding[5];                     // 176B + 80B = 256B
#elif !RTXGI_DDGI_PROBE_RELOCATION && RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    float       probeBackfaceThreshold;         // 180B
    float3      padding;                        // 192B
    float4      padding1[4];                    // 192B + 64B = 256B
#elif RTXGI_DDGI_PROBE_RELOCATION /* && (RTXGI_DDGI_PROBE_STATE_CLASSIFIER || !RTXGI_DDGI_PROBE_STATE_CLASSIFIER) */
    float       probeBackfaceThreshold;         // 180B
    float       probeMinFrontfaceDistance;      // 184B
    float2      padding;                        // 192B
    float4      padding1[4];                    // 192B + 64B = 256B
#endif

};

#endif /* RTXGI_DDGI_VOLUME_DESC_GPU_H */
