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
    float       probeVariablePad0;
    float       probeIrradianceEncodingGamma;
    float       probeInverseIrradianceEncodingGamma;
    int         probeNumIrradianceTexels;
    int         probeNumDistanceTexels;
    float       normalBias;
    float       viewBias;
    float2      probeVariablePad1;
    float4x4    probeRayRotationTransform;      // 160B

#if !RTXGI_DDGI_PROBE_RELOCATION && !RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    float4      padding[6];                     // 160B + 96B = 256B
#elif !RTXGI_DDGI_PROBE_RELOCATION && RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    float       probeBackfaceThreshold;         // 164B
    float3      padding;                        // 176B
    float4      padding1[5];                    // 176B + 80B = 256B
#elif RTXGI_DDGI_PROBE_RELOCATION /* && (RTXGI_DDGI_PROBE_STATE_CLASSIFIER || !RTXGI_DDGI_PROBE_STATE_CLASSIFIER) */
    float       probeBackfaceThreshold;         // 164B
    float       probeMinFrontfaceDistance;      // 168B
    float2      padding;                        // 176B
    float4      padding1[5];                    // 176B + 80B = 256B
#endif

};

#endif /* RTXGI_DDGI_VOLUME_DESC_GPU_H */
