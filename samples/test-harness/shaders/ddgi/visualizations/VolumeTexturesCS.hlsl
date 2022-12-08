/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// THGP_DIM_X must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_X 8
#ifndef THGP_DIM_X
    #error Required define THGP_DIM_X is not defined for VolumeTexturesCS.hlsl!
#endif

// THGP_DIM_Y must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_Y 4
#ifndef THGP_DIM_Y
    #error Required define THGP_DIM_Y is not defined for VolumeTexturesCS.hlsl!
#endif

// -------------------------------------------------------------------------------------------

#include "../../include/Common.hlsl"
#include "../../include/Descriptors.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"
#include "../../../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"

// ---[ Compute Shader ]---

[numthreads(THGP_DIM_X, THGP_DIM_Y, 1)]
void CS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the DDGIVolume index from root/push constants
    uint volumeIndex = GetDDGIVolumeIndex();

    // Get the DDGIVolume structured buffers
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
    StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    Texture2DArray<float4> RayData = GetTex2DArray(resourceIndices.rayDataSRVIndex);
    Texture2DArray<float4> ProbeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);
    Texture2DArray<float4> ProbeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);
    Texture2DArray<float4> ProbeVariability = GetTex2DArray(resourceIndices.probeVariabilitySRVIndex);
    Texture2DArray<float4> ProbeVariabilityAverage = GetTex2DArray(resourceIndices.probeVariabilityAverageSRVIndex);

    // Load and unpack the DDGIVolume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get probe dimensions
    float numIrradianceProbeTexels = (volume.probeNumIrradianceInteriorTexels + 2);
    float numDistanceProbeTexels = (volume.probeNumDistanceInteriorTexels + 2);

    // Get the probe counts (coordinate system specific)
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    uint2 numProbesPerSlice = uint2(volume.probeCounts.x, volume.probeCounts.z);
    uint  numSlices = volume.probeCounts.y;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    uint2 numProbesPerSlice = uint2(volume.probeCounts.y, volume.probeCounts.x);
    uint  numSlices = volume.probeCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    uint2 numProbesPerSlice = uint2(volume.probeCounts.x, volume.probeCounts.y);
    uint  numSlices = volume.probeCounts.z;
#endif

    // Output color
    float3 color = float3(0.f, 0.f, 0.f);

    // Irradiance
    float irradianceScale = GetGlobalConst(ddgivis, irradianceTextureScale);
    uint2 numTexelsPerSlice = numProbesPerSlice * numIrradianceProbeTexels;
    uint2 irradianceRect = uint2(numTexelsPerSlice.x * numSlices, numTexelsPerSlice.y) * irradianceScale;
    if(DispatchThreadID.x < irradianceRect.x && DispatchThreadID.y < irradianceRect.y)
    {
        // Compute the sampling coordinates
        uint2  numScaledTexelsPerSlice = numTexelsPerSlice * irradianceScale;
        float2 sliceUV = (float2(0.5f, 0.5f) + float2(DispatchThreadID.xy % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
        float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
        float3 coords = float3(sliceUV, sliceIndex);

        // Sample the irradiance texture array
        float3 result = ProbeIrradiance.SampleLevel(GetPointClampSampler(), coords, 0).rgb;

        // Decode the tone curve
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        color = pow(result, exponent);

        // Go back to linear irradiance
        color *= color;

        // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to normalize for the display.
        color *= 2.f;

        // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
        if (volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32)
        {
            color *= 1.0989f;
        }

        // Convert to sRGB before storing
        color = LinearToSRGB(color);

        // Overwrite GBufferA's albedo and mark the pixel to not be lit or post-processed
        // Note: because the visualized probe's irradiance marked to be ignored during post-processing, it will be visible during the furnace test!
        GBufferA[DispatchThreadID.xy] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);
        return;
    }

    // Distance
    float distanceScale = GetGlobalConst(ddgivis, distanceTextureScale);
    numTexelsPerSlice = numProbesPerSlice * numDistanceProbeTexels;
    uint2 distanceRect = uint2(numTexelsPerSlice.x * numSlices, numTexelsPerSlice.y) * distanceScale;
    float xmax = distanceRect.x;
    float ymin = irradianceRect.y + 5;
    float ymax = (ymin + distanceRect.y);
    if (DispatchThreadID.x < xmax && DispatchThreadID.y >= ymin && DispatchThreadID.y < ymax)
    {
        // Compute the sampling coordinates
        uint2  numScaledTexelsPerSlice = numTexelsPerSlice * distanceScale;
        float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
        float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
        float3 coords = float3(sliceUV, sliceIndex);

        // Sample the distance texture array
        color.r = 2.f * ProbeDistance.SampleLevel(GetPointClampSampler(), coords, 0).r;

        // Normalize for display
        color.r = saturate(color.r / GetGlobalConst(ddgivis, distanceDivisor));

        // Overwrite GBufferA's albedo and mark the pixel to not be lit or post-processed
        GBufferA[DispatchThreadID.xy] = float4(color.rrr, COMPOSITE_FLAG_IGNORE_PIXEL);
        return;
    }

    // Variability
    float variabilityScale = GetGlobalConst(ddgivis, probeVariabilityTextureScale);
    numTexelsPerSlice = numProbesPerSlice * volume.probeNumIrradianceInteriorTexels;
    uint2 variabilityRect = uint2(numTexelsPerSlice.x * numSlices, numTexelsPerSlice.y) * variabilityScale;
    xmax = variabilityRect.x;
    ymin += distanceRect.y + 5;
    ymax = (ymin + variabilityRect.y);
    if (DispatchThreadID.x < xmax.x && DispatchThreadID.y >= ymin && DispatchThreadID.y < ymax)
    {
        // Compute the sampling coordinates
        uint2  numScaledTexelsPerSlice = numTexelsPerSlice * variabilityScale;
        float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
        float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
        float3 coords = float3(sliceUV, sliceIndex);

        // Sample the variability texture
        float diff = ProbeVariability.SampleLevel(GetPointClampSampler(), coords, 0).r;

        // Sample the probe data texture
        bool active = true;
        if (volume.probeClassificationEnabled)
        {
            // Sample the probe data texture
            uint state = ProbeData.SampleLevel(GetPointClampSampler(), coords, 0).a;
            active = (state == RTXGI_DDGI_PROBE_STATE_ACTIVE);
        }

        // Disabled = blue, above threshold = green, below = red, nan = yellow
        if (!active) color = float3(0.f, 0.f, 1.f);
        else if (isnan(diff)) color = float3(1.f, 1.f, 0.f);
        else if (diff > GetGlobalConst(ddgivis, probeVariabilityTextureThreshold)) color = float3(0.f, 1.0, 0.f);
        else color = float3(1.f, 0.f, 0.f);

        // Overwrite GBufferA's albedo and mark the pixel to not be lit
        GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

        return;
    }

    // Variability average
    // 1/4 number of slices (rounded up) after reduction
    uint2 variabilityAvgRect = uint2(numTexelsPerSlice.x * ((numSlices + 3)/4), numTexelsPerSlice.y) * variabilityScale;
    xmax = variabilityAvgRect.x;
    ymin += variabilityRect.y + 5;
    ymax = (ymin + variabilityAvgRect.y);
    if (DispatchThreadID.x < xmax.x && DispatchThreadID.y >= ymin && DispatchThreadID.y < ymax)
    {
        // Compute the sampling coordinates
        uint2  numScaledTexelsPerSlice = numTexelsPerSlice * variabilityScale;
        float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
        float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
        float3 coords = float3(sliceUV, sliceIndex);

        // Sample the variability average texture
        float diff = ProbeVariabilityAverage.SampleLevel(GetPointClampSampler(), coords, 0).r;

        // Above threshold = green, below = red, nan = yellow
        if (isnan(diff)) color = float3(1.f, 1.f, 0.f);
        else if (diff > GetGlobalConst(ddgivis, probeVariabilityTextureThreshold)) color = float3(0.f, 1.f, 0.f);
        else color = float3(1.f, 0.f, 0.f);

        // Overwrite GBufferA's albedo and mark the pixel to not be lit
        GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

        return;
    }

    // Get the texture scale factor for probe data
    float probeDataScale = GetGlobalConst(ddgivis, probeDataTextureScale);

    // Relocation Offsets
    uint2 offsetRect = 0;
    ymin += variabilityAvgRect.y + 5;
    if (volume.probeRelocationEnabled)
    {
        offsetRect = uint2(numProbesPerSlice.x * numSlices, numProbesPerSlice.y) * probeDataScale;

        xmax = offsetRect.x;
        ymax = (ymin + offsetRect.y);
        if (DispatchThreadID.x < xmax && DispatchThreadID.y >= ymin && DispatchThreadID.y < ymax)
        {
            // Compute the sampling coordinates
            uint2  numScaledTexelsPerSlice = numProbesPerSlice * probeDataScale;
            float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
            float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
            float3 coords = float3(sliceUV, sliceIndex);

            // Sample the probe data texture array
            color = ProbeData.SampleLevel(GetPointClampSampler(), coords, 0).rgb;

            // Overwrite GBufferA's albedo and mark the pixel to not be lit or post-processed
            GBufferA[DispatchThreadID.xy] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);
            return;
        }
    }

    // Classification States
    if(volume.probeClassificationEnabled)
    {
        uint2 statesRect = uint2(numProbesPerSlice.x * numSlices, numProbesPerSlice.y) * probeDataScale;
        xmax = statesRect.x;

        if (volume.probeRelocationEnabled) ymin += offsetRect.y + 5;
        else ymin += 5;

        ymax = (ymin + statesRect.y);
        if (DispatchThreadID.x < xmax && DispatchThreadID.y >= ymin && DispatchThreadID.y < ymax)
        {
            // Compute the sampling coordinates
            uint2  numScaledTexelsPerSlice = numProbesPerSlice * probeDataScale;
            float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
            float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
            float3 coords = float3(sliceUV, sliceIndex);

            // Sample the probe data texture
            uint state = ProbeData.SampleLevel(GetPointClampSampler(), coords, 0).a;

            // Set probe state colors
            if(state == RTXGI_DDGI_PROBE_STATE_ACTIVE) color = float3(0.f, 1.f, 0.f);
            else color = float3(1.f, 0.f, 0.f);

            // Overwrite GBufferA's albedo and mark the pixel to not be lit or post-processed
            GBufferA[DispatchThreadID.xy] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);
            return;
        }
    }

    // Ray Data
    float rayDataScale = GetGlobalConst(ddgivis, rayDataTextureScale);
    numTexelsPerSlice = uint2(volume.probeNumRays, numProbesPerSlice.x * numProbesPerSlice.y);
    uint2 rayDataRect = uint2(numTexelsPerSlice.x * numSlices, numTexelsPerSlice.y) * rayDataScale;
    xmax = rayDataRect.x;
    ymin = ymax + 5;
    ymax = (ymin + rayDataRect.y);
    if (DispatchThreadID.x <= xmax && DispatchThreadID.y > ymin && DispatchThreadID.y <= ymax)
    {
        // Compute the sampling coordinates
        uint2  numScaledTexelsPerSlice = numTexelsPerSlice * rayDataScale;
        float2 sliceUV = (float2(0.5f, 0.5f) + float2(uint2(DispatchThreadID.x, DispatchThreadID.y - ymin) % numScaledTexelsPerSlice)) / float2(numScaledTexelsPerSlice);
        float  sliceIndex = float(DispatchThreadID.x / numScaledTexelsPerSlice.x);
        float3 coords = float3(sliceUV, sliceIndex);

        if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
        {
            color = RayData.SampleLevel(GetPointClampSampler(), coords, 0).rgb;
        }
        else if (volume.probeRayDataFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2)
        {
            color = RTXGIUintToFloat3(asuint(RayData.SampleLevel(GetPointClampSampler(), coords, 0).r));
        }

        // Overwrite GBufferA's albedo and mark the pixel to not be lit or post-processed
        GBufferA[DispatchThreadID.xy] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);
        return;
    }

}
