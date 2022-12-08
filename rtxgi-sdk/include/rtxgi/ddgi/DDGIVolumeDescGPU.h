/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_VOLUME_DESC_GPU_H
#define RTXGI_DDGI_VOLUME_DESC_GPU_H

#ifndef HLSL
#include <rtxgi/Math.h>
#include <rtxgi/Types.h>
using namespace rtxgi;
#endif

/**
 * Describes the location (i.e. index) of DDGIVolume resources
 * on the D3D descriptor heap or in bindless resource arrays.
 */
struct DDGIVolumeResourceIndices
{
    uint     rayDataUAVIndex;                    // Index of the ray data UAV on the descriptor heap or in a RWTexture2D resource array
    uint     rayDataSRVIndex;                    // Index of the ray data SRV on the descriptor heap or in a Texture2D resource array
    uint     probeIrradianceUAVIndex;            // Index of the probe irradiance UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeIrradianceSRVIndex;            // Index of the probe irradiance SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 16B
    uint     probeDistanceUAVIndex;              // Index of the probe distance UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeDistanceSRVIndex;              // Index of the probe distance SRV on the descriptor heap or in a Texture2DArray resource array
    uint     probeDataUAVIndex;                  // Index of the probe data UAV on the descriptor heap or in a RWTexture2DArray resource array
    uint     probeDataSRVIndex;                  // Index of the probe data SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 32B
    uint     probeVariabilityUAVIndex;           // Index of the probe variability UAV on the descriptor heap or in a RWTexture2DArray resource Array
    uint     probeVariabilitySRVIndex;           // Index of the probe variability SRV on the descriptor heap or in a Texture2DArray resource array
    uint     probeVariabilityAverageUAVIndex;    // Index of the probe variability average UAV on the descriptor heap or in a RWTexture2DArray resource Array
    uint     probeVariabilityAverageSRVIndex;    // Index of the probe variability average SRV on the descriptor heap or in a Texture2DArray resource array
    //------------------------------------------------- 48B
};

/**
 * Describes the properties of a DDGIVolume, with values packed to compact formats.
 * This version of the struct uses 128B to store some values at full precision.
 */
struct DDGIVolumeDescGPUPacked
{
    float3   origin;
    float    probeHysteresis;
    //------------------------------------------------- 16B
    float4   rotation;
    //------------------------------------------------- 32B
    float4   probeRayRotation;
    //------------------------------------------------- 64B
    float    probeMaxRayDistance;
    float    probeNormalBias;
    float    probeViewBias;
    float    probeDistanceExponent;
    //------------------------------------------------- 80B
    float3   probeSpacing;
    uint     packed0;       // probeCounts.x (10), probeCounts.y (10), probeCounts.z (10), unused (2)
    //------------------------------------------------- 96B
    float    probeIrradianceEncodingGamma;
    float    probeIrradianceThreshold;
    float    probeBrightnessThreshold;
    uint     packed1;       // probeRandomRayBackfaceThreshold (16), probeFixedRayBackfaceThreshold (16)
    //------------------------------------------------- 112B
    float    probeMinFrontfaceDistance;
    uint     packed2;       // probeNumRays (16), probeNumIrradianceInteriorTexels (8), probeNumDistanceInteriorTexels (8)
    uint     packed3;       // probeScrollOffsets.x (15) sign bit (1), probeScrollOffsets.y (15) sign bit (1)
    uint     packed4;       // probeScrollOffsets.z (15) sign bit (1)
                            // movementType (1), rayDataFormat (1), irradianceFormat (1), probeRelocationEnabled (1), probeClassificationEnabled (1)
                            // probeScrollClear Y-Z plane (1), probeScrollClear X-Z plane (1), probeScrollClear X-Y plane (1)
                            // probeScrollDirection Y-Z plane (1), probeScrollDirection X-Z plane (1), probeScrollDirection X-Y plane (1)
                            // unused (5)
    //------------------------------------------------- 128B
};

/**
 * Describes the properties of a DDGIVolume.
 */
struct DDGIVolumeDescGPU
{
    float3   origin;                             // world-space location of the volume center

    float4   rotation;                           // rotation quaternion for the volume
    float4   probeRayRotation;                   // rotation quaternion for probe rays

    uint     movementType;                       // type of movement the volume allows. 0: default, 1: infinite scrolling

    float3   probeSpacing;                       // world-space distance between probes
    int3     probeCounts;                        // number of probes on each axis of the volume

    int      probeNumRays;                       // number of rays traced per probe
    int      probeNumIrradianceInteriorTexels;   // number of texels in one dimension of a probe's irradiance texture (does not include 1-texel border)
    int      probeNumDistanceInteriorTexels;     // number of texels in one dimension of a probe's distance texture (does not include 1-texel border)

    float    probeHysteresis;                    // weight of the previous irradiance and distance data store in probes
    float    probeMaxRayDistance;                // maximum world-space distance a probe ray can travel
    float    probeNormalBias;                    // offset along the surface normal, applied during lighting to avoid numerical instabilities when determining visibility
    float    probeViewBias;                      // offset along the camera view ray, applied during lighting to avoid numerical instabilities when determining visibility
    float    probeDistanceExponent;              // exponent used during visibility testing. High values react rapidly to depth discontinuities, but may cause banding
    float    probeIrradianceEncodingGamma;       // exponent that perceptually encodes irradiance for faster light-to-dark convergence

    float    probeIrradianceThreshold;           // threshold to identify when large lighting changes occur
    float    probeBrightnessThreshold;           // threshold that specifies the maximum allowed difference in brightness between the previous and current irradiance values
    float    probeRandomRayBackfaceThreshold;    // threshold that specifies the ratio of *random* rays traced for a probe that may hit back facing triangles before the probe is considered inside geometry (used in blending)

    // Probe Relocation, Probe Classification
    float    probeFixedRayBackfaceThreshold;     // threshold that specifies the ratio of *fixed* rays traced for a probe that may hit back facing triangles before the probe is considered inside geometry (used in relocation & classification)
    float    probeMinFrontfaceDistance;          // minimum world-space distance to a front facing triangle allowed before a probe is relocated

    // Infinite Scrolling Volumes
    int3     probeScrollOffsets;                 // grid-space offsets used for scrolling movement
    bool     probeScrollClear[3];                // whether probes of a plane need to be cleared due to scrolling movement
    bool     probeScrollDirections[3];           // direction of scrolling movement (0: negative, 1: positive)

    // Feature Options
    uint     probeRayDataFormat;                 // texture format of the ray data texture (EDDGIVolumeTextureFormat)
    uint     probeIrradianceFormat;              // texture format of the irradiance texture (EDDGIVolumeTextureFormat)
    bool     probeRelocationEnabled;             // whether probe relocation is enabled for this volume
    bool     probeClassificationEnabled;         // whether probe classification is enabled for this volume
    bool     probeVariabilityEnabled;            // whether probe variability is enabled for this volume
};

#ifndef HLSL // CPU only
static inline rtxgi::DDGIVolumeDescGPUPacked PackDDGIVolumeDescGPU(const rtxgi::DDGIVolumeDescGPU input)
{
    rtxgi::DDGIVolumeDescGPUPacked output = {};

    output.origin = input.origin;
    output.probeHysteresis = input.probeHysteresis;
    output.rotation = input.rotation;
    output.probeRayRotation = input.probeRayRotation;
    output.probeMaxRayDistance = input.probeMaxRayDistance;
    output.probeNormalBias = input.probeNormalBias;
    output.probeViewBias = input.probeViewBias;
    output.probeDistanceExponent = input.probeDistanceExponent;
    output.probeSpacing = input.probeSpacing;

    output.packed0  = (uint32_t)input.probeCounts.x;
    output.packed0 |= (uint32_t)input.probeCounts.y << 10;
    output.packed0 |= (uint32_t)input.probeCounts.z << 20;

    output.probeIrradianceEncodingGamma = input.probeIrradianceEncodingGamma;
    output.probeIrradianceThreshold = input.probeIrradianceThreshold;
    output.probeBrightnessThreshold = input.probeBrightnessThreshold;

    output.packed1  = (uint32_t)(input.probeRandomRayBackfaceThreshold * 65535);
    output.packed1 |= (uint32_t)(input.probeFixedRayBackfaceThreshold * 65535) << 16;

    output.probeMinFrontfaceDistance = input.probeMinFrontfaceDistance;

    output.packed2  = (uint32_t)input.probeNumRays;
    output.packed2 |= (uint32_t)input.probeNumIrradianceInteriorTexels << 16;
    output.packed2 |= (uint32_t)input.probeNumDistanceInteriorTexels << 24;

    // Probe Scroll Offsets
    output.packed3 = (output.packed3 & ~0x7FFF)     | abs(input.probeScrollOffsets.x);
    output.packed3 = (output.packed3 & ~0x8000)     | ((input.probeScrollOffsets.x < 0) << 15);
    output.packed3 = (output.packed3 & ~0x10000)    | abs(input.probeScrollOffsets.y) << 16;
    output.packed3 = (output.packed3 & ~0x80000000) | ((input.probeScrollOffsets.y < 0) << 31);
    output.packed4 = (output.packed4 & ~0x7FFF)     | abs(input.probeScrollOffsets.z);
    output.packed4 = (output.packed4 & ~0x8000)     | ((input.probeScrollOffsets.z < 0) << 15);

    // Feature Bits
    output.packed4 = (output.packed4 & ~0x10000)    | (input.movementType << 16);
    output.packed4 = (output.packed4 & ~0xE0000)    | (input.probeRayDataFormat << 17);
    output.packed4 = (output.packed4 & ~0x700000)   | (input.probeIrradianceFormat << 20);
    output.packed4 = (output.packed4 & ~0x800000)   | (input.probeRelocationEnabled << 23);
    output.packed4 = (output.packed4 & ~0x1000000)  | (input.probeClassificationEnabled << 24);
    output.packed4 = (output.packed4 & ~0x2000000)  | (input.probeVariabilityEnabled << 25);

    output.packed4 = (output.packed4 & ~0x4000000)  | (input.probeScrollClear[0] << 26);
    output.packed4 = (output.packed4 & ~0x8000000)  | (input.probeScrollClear[1] << 27);
    output.packed4 = (output.packed4 & ~0x10000000)  | (input.probeScrollClear[2] << 28);

    output.packed4 = (output.packed4 & ~0x20000000) | (input.probeScrollDirections[0] << 29);
    output.packed4 = (output.packed4 & ~0x40000000) | (input.probeScrollDirections[1] << 30);
    output.packed4 = (output.packed4 & ~0x80000000) | (input.probeScrollDirections[2] << 31);

    return output;
}
#endif // ifndef HLSL

#ifndef HLSL // CPU
static inline rtxgi::DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(const rtxgi::DDGIVolumeDescGPUPacked input)
{
    rtxgi::DDGIVolumeDescGPU output;
#else // GPU
DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(DDGIVolumeDescGPUPacked input)
{
    DDGIVolumeDescGPU output = (DDGIVolumeDescGPU)0;
#endif // ifndef HLSL
    output.origin = input.origin;
    output.probeHysteresis = input.probeHysteresis;
    output.rotation = input.rotation;
    output.probeRayRotation = input.probeRayRotation;
    output.probeMaxRayDistance = input.probeMaxRayDistance;
    output.probeNormalBias = input.probeNormalBias;
    output.probeViewBias = input.probeViewBias;
    output.probeDistanceExponent = input.probeDistanceExponent;
    output.probeSpacing = input.probeSpacing;

    // Probe Counts
    output.probeCounts.x = input.packed0 & 0x000003FF;
    output.probeCounts.y = (input.packed0 >> 10) & 0x000003FF;
    output.probeCounts.z = (input.packed0 >> 20) & 0x000003FF;

    // Thresholds
    output.probeIrradianceEncodingGamma = input.probeIrradianceEncodingGamma;
    output.probeIrradianceThreshold = input.probeIrradianceThreshold;
    output.probeBrightnessThreshold = input.probeBrightnessThreshold;

    output.probeRandomRayBackfaceThreshold = (float)(input.packed1 & 0x0000FFFF) / 65535.f;
    output.probeFixedRayBackfaceThreshold = (float)((input.packed1 >> 16) & 0x0000FFFF) / 65535.f;

    output.probeMinFrontfaceDistance = input.probeMinFrontfaceDistance;

    output.probeNumRays = input.packed2 & 0x0000FFFF;
    output.probeNumIrradianceInteriorTexels = (input.packed2 >> 16) & 0x000000FF;
    output.probeNumDistanceInteriorTexels = (input.packed2 >> 24) & 0x000000FF;

    // Probe Scroll Offsets
    output.probeScrollOffsets.x = input.packed3 & 0x00007FFF;
    if ((input.packed3 >> 15) & 0x00000001) output.probeScrollOffsets.x *= -1;
    output.probeScrollOffsets.y = (input.packed3 >> 16) & 0x00007FFF;
    if ((input.packed3 >> 31) & 0x00000001) output.probeScrollOffsets.y *= -1;
    output.probeScrollOffsets.z = (input.packed4) & 0x00007FFF;
    if ((input.packed4 >> 15) & 0x00000001) output.probeScrollOffsets.z *= -1;

    // Feature Bits
    output.movementType = (input.packed4 >> 16) & 0x00000001;
    output.probeRayDataFormat = (uint)((input.packed4 >> 17) & 0x00000007);
    output.probeIrradianceFormat = (uint)((input.packed4 >> 20) & 0x00000007);
    output.probeRelocationEnabled = (bool)((input.packed4 >> 23) & 0x00000001);
    output.probeClassificationEnabled = (bool)((input.packed4 >> 24) & 0x00000001);
    output.probeVariabilityEnabled = (bool)((input.packed4 >> 25) & 0x00000001);
    output.probeScrollClear[0] = (bool)((input.packed4 >> 26) & 0x00000001);
    output.probeScrollClear[1] = (bool)((input.packed4 >> 27) & 0x00000001);
    output.probeScrollClear[2] = (bool)((input.packed4 >> 28) & 0x00000001);
    output.probeScrollDirections[0] = (bool)((input.packed4 >> 29) & 0x00000001);
    output.probeScrollDirections[1] = (bool)((input.packed4 >> 30) & 0x00000001);
    output.probeScrollDirections[2] = (bool)((input.packed4 >> 31) & 0x00000001);

    return output;
}

#endif // RTXGI_DDGI_VOLUME_DESC_GPU_H
